extends SceneTree

const Creature = preload("res://real_data/real_creature.gd")
const Renderer = preload("res://fx/native_fx_renderer.gd")
var checks := 0

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		push_error(message)
		quit(1)

func _initialize() -> void:
	call_deferred("run")

func _pixel(viewport: SubViewport) -> Color:
	for i in 4:
		await process_frame
	return viewport.get_texture().get_image().get_pixel(32, 32)

func _creature(equation: int, alpha_byte: int, blend: int, env: ImageTexture = null, env_blend := -1, fix2 := 0) -> Creature:
	var creature := Creature.new()
	var bytes := PackedByteArray([0, 255, 0, alpha_byte])
	creature.texture = ImageTexture.create_from_image(Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, bytes))
	creature.equation = equation
	creature.alpha_fix = 32
	creature.fix2 = fix2
	creature.environment = env
	creature.two_pass = env != null
	creature.meshes = [{"visible_by_default": true, "uv": PackedVector2Array([Vector2.ZERO, Vector2.ZERO, Vector2.ZERO]), "blend": blend, "environment": env != null, "environment_blend": env_blend}]
	return creature

func run() -> void:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(64, 64)
	viewport.transparent_bg = false
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 2.0
	camera.position = Vector3(0, 0, 3)
	camera.current = true
	viewport.add_child(camera)
	var background := MeshInstance3D.new()
	var quad := QuadMesh.new()
	quad.size = Vector2(2, 2)
	background.mesh = quad
	var red := StandardMaterial3D.new()
	red.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	red.albedo_color = Color.RED
	red.cull_mode = BaseMaterial3D.CULL_DISABLED
	background.material_override = red
	viewport.add_child(background)
	var foreground := MeshInstance3D.new()
	foreground.mesh = quad
	foreground.position.z = 0.1
	viewport.add_child(foreground)
	# Decoder byte 127 is raw GS alpha 64 (64 * 255 / 128). As is about one half.
	var additive := _creature(0x48, 127, 1)
	var additive_material := additive._material(1, -1, false)
	check(additive_material.shader.code.contains("blend_add") and additive_material.shader.code.contains("ALPHA = as_w"), "equation 0x48 adds source times alpha")
	foreground.material_override = additive_material
	var pixel := await _pixel(viewport)
	check(pixel.r > 0.7 and pixel.g > 0.3 and pixel.g < 0.8 and pixel.b < 0.2, "equation 0x48 adds half green onto red")
	var premul := _creature(9, 127, 1)
	foreground.material_override = premul._material(1, -1, false)
	pixel = await _pixel(viewport)
	check(pixel.r > 0.3 and pixel.r < 0.8 and pixel.g > 0.7 and pixel.b < 0.2, "equation 9 keeps the source and scales the destination by alpha")
	var cut := _creature(9, 255, 1)
	foreground.material_override = cut._material(1, -1, false)
	pixel = await _pixel(viewport)
	check(pixel.r > 0.8 and pixel.g < 0.2, "decoded alpha byte 255 fails TEST_1 and is not drawn")
	var gated := _creature(0, 127, 1)
	foreground.material_override = gated._material(-1, -1, false)
	pixel = await _pixel(viewport)
	check(pixel.g > 0.8 and pixel.r < 0.2, "an undocumented register replaces with the source")
	var replaced := _creature(9, 127, 0)
	foreground.material_override = replaced._material(0, -1, false)
	pixel = await _pixel(viewport)
	check(replaced._material(0, -1, false).shader.code.contains("depth_draw_opaque") and pixel.g > 0.8 and pixel.r < 0.2, "blend off replaces with the source")
	var env_image := Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, PackedByteArray([0, 0, 255, 255]))
	var env := ImageTexture.create_from_image(env_image)
	var mapped := _creature(9, 127, 1, env, 1, 128)
	var mapped_material := mapped._material(1, 1, true)
	check(mapped_material.shader.code.contains("src + pass1 * as_w") and not mapped_material.shader.code.contains("hint_screen_texture"), "fix 128 composites the environment without reading the framebuffer")
	var code := mapped_material.shader.code
	var vertex_at := code.find("void vertex()")
	var fragment_at := code.find("void fragment()")
	check(vertex_at >= 0 and fragment_at > vertex_at and code.find("MODELVIEW_NORMAL_MATRIX * NORMAL") > vertex_at and code.find("MODELVIEW_NORMAL_MATRIX * NORMAL") < fragment_at
		and code.contains("inversesqrt(len2) * 0.99") and code.contains("vec2(n.x, -n.y)") and code.contains("varying vec2 env_uv")
		and code.find("NORMAL", fragment_at) < 0 and code.find("192.0", fragment_at) < 0,
		"the environment UV is computed per vertex from the view normal x 0.99 and only sampled per pixel")
	var unflagged := _creature(9, 127, 1, env, 1, 128)
	unflagged.two_pass = false
	check(not unflagged._material(1, 1, true).shader.code.contains("env_texture"), "no environment pass without header flag 0x10")
	var heavy := _creature(9, 127, 1, env, 1, 129)
	check(not heavy._material(1, 1, true).shader.code.contains("env_texture"), "header +0x20 above 0x80 draws no environment pass")
	# A quad whose left and right vertex normals lean -0.6 / +0.6 in view x,
	# over a horizontal gradient (column i = red 2i). With TEST_1 failing
	# (alpha byte 255) the pixel is the environment sample alone. The UV is
	# interpolated per vertex: at pixel column 8 the view x of c is
	# 0.99 * lerp(-0.6, 0.6, t); a per-pixel renormalised normal would give
	# about 3 texels less. This winding is a Godot back face, where the
	# fragment NORMAL is flipped; the vertex-stage UV is not.
	var ramp := Image.create(128, 128, false, Image.FORMAT_RGBA8)
	for y in 128:
		for x in 128: ramp.set_pixel(x, y, Color8(2 * x, 0, 0, 255))
	var ramp_env := ImageTexture.create_from_image(ramp)
	var leaning := ArrayMesh.new()
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(-1, -1, 0), Vector3(1, -1, 0), Vector3(1, 1, 0), Vector3(-1, -1, 0), Vector3(1, 1, 0), Vector3(-1, 1, 0)])
	var left := Vector3(-0.6, 0, 0.8)
	var right := Vector3(0.6, 0, 0.8)
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([left, right, right, left, right, left])
	arrays[Mesh.ARRAY_TEX_UV] = PackedVector2Array([Vector2.ZERO, Vector2.ZERO, Vector2.ZERO, Vector2.ZERO, Vector2.ZERO, Vector2.ZERO])
	leaning.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	foreground.mesh = leaning
	var sampled := _creature(9, 255, 1, ramp_env, 1, 128)
	foreground.material_override = sampled._material(1, 1, true)
	for i in 4: await process_frame
	var ramp_red := viewport.get_texture().get_image().get_pixel(8, 32).r
	var t := (8.5 / 32.0) / 2.0
	var nx := lerpf(-0.6, 0.6, t)
	var per_vertex := fposmod(192.0 + 64.0 * 0.99 * nx, 128.0)
	var per_pixel := fposmod(192.0 + 64.0 * nx / sqrt(nx * nx + 0.64), 128.0)
	var expect := 2.0 * (per_vertex - 0.5) / 255.0
	check(absf(ramp_red - expect) < 0.01 and absf(ramp_red - 2.0 * (per_pixel - 0.5) / 255.0) > 0.015,
		"the environment UV interpolates per vertex (red %f, expected %f)" % [ramp_red, expect])
	foreground.mesh = quad
	var fixed := _creature(0x64, 127, 1)
	check(fixed._material(1, -1, false).shader.code.contains("ALPHA = fix_w;"), "0x64 uses F = FIX / 128 without a clamp")
	foreground.material_override = mapped_material
	pixel = await _pixel(viewport)
	check(pixel.r < 0.2 and pixel.g > 0.7 and pixel.b > 0.3 and pixel.b < 0.8, "equation 9 over a solid environment adds half of that environment")
	var partial := _creature(0x64, 127, 1, env, 1, 64)
	var partial_material := partial._material(1, 1, true)
	check(partial_material.shader.code.contains("hint_screen_texture") and partial_material.shader.code.contains("src * fix_w + pass1 * (1.0 - fix_w)"), "a partial environment weight reads the framebuffer and uses the 0x64 weight")
	# One synthetic model with an environment pass on only its first mesh.
	# Its two material states force real_creature.apply() to assign per-surface
	# ShaderMaterials instead of an instance override.
	var mixed := _creature(9, 255, 1, env, 1, 128)
	mixed.meshes.append({"visible_by_default": true, "uv": PackedVector2Array([Vector2.ZERO, Vector2.ZERO, Vector2.ZERO]), "blend": 1, "environment": false, "environment_blend": -1})
	var mixed_arrays: Array = []
	mixed_arrays.resize(Mesh.ARRAY_MAX)
	mixed_arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3.ZERO, Vector3.RIGHT, Vector3.UP])
	mixed_arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([Vector3.BACK, Vector3.BACK, Vector3.BACK])
	mixed_arrays[Mesh.ARRAY_COLOR] = PackedColorArray([Color(0.5, 1.0, 1.0), Color.WHITE, Color.WHITE])
	var mixed_mesh := ArrayMesh.new()
	for surface in 2: mixed_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, mixed_arrays)
	var mixed_instance := MeshInstance3D.new()
	mixed_instance.mesh = mixed_mesh
	root.add_child(mixed_instance)
	check(mixed.apply(mixed_instance) and mixed_instance.material_override == null, "mixed-pass model uses per-surface materials")
	var source_mesh: ArrayMesh = mixed_instance.mesh
	var env_material: ShaderMaterial = source_mesh.surface_get_material(0)
	var base_material: ShaderMaterial = source_mesh.surface_get_material(1)
	check(env_material != base_material and env_material.shader.code.contains("texture(env_texture, env_uv).rgb * COLOR.rgb") and not base_material.shader.code.contains("env_texture"), "only the tagged surface draws vertex-colour-modulated environment")
	var renderer := Renderer.new()
	root.add_child(renderer)
	check(renderer.bridge.set_level_light(Vector3.UP, Color.WHITE, Color.WHITE, 128, 1.0, 0).ok, "mixed-pass lighting setup")
	check(renderer.apply_vertex_lighting(mixed_instance, true).ok, "mixed-pass model accepts FX lighting")
	var lit_mesh: ArrayMesh = mixed_instance.mesh
	check(lit_mesh.surface_get_material(0) is ShaderMaterial and lit_mesh.surface_get_material(1) is ShaderMaterial and lit_mesh.surface_get_material(0) != env_material and lit_mesh.surface_get_material(1) != base_material, "both surface shaders receive lit copies")
	var lit_colors: PackedColorArray = lit_mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
	check(lit_colors[1].r > 0.0, "FX lighting produces nonzero red reference")
	check(lit_colors.size() == 3 and lit_colors[0].r <= lit_colors[1].r * 0.5 + 0.001, "FX lighting multiplies existing vertex colour")
	check(renderer.apply_vertex_lighting(mixed_instance, true).ok and mixed_instance.mesh.surface_get_material(0) == lit_mesh.surface_get_material(0), "relighting reuses per-surface shader copy")
	mixed_instance.queue_free()
	renderer.queue_free()
	viewport.queue_free()
	print("creature blend: ", checks, " checks passed")
	quit()
