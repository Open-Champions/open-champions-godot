extends SceneTree

const Creature = preload("res://real_data/real_creature.gd")
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
	check(mapped_material.shader.code.contains("192.0 + 64.0") and mapped_material.shader.code.contains("src + pass1 * as_w") and not mapped_material.shader.code.contains("hint_screen_texture"), "fix 128 composites the environment without reading the framebuffer")
	foreground.material_override = mapped_material
	pixel = await _pixel(viewport)
	check(pixel.r < 0.2 and pixel.g > 0.7 and pixel.b > 0.3 and pixel.b < 0.8, "equation 9 over a solid environment adds half of that environment")
	var partial := _creature(0x64, 127, 1, env, 1, 64)
	var partial_material := partial._material(1, 1, true)
	check(partial_material.shader.code.contains("hint_screen_texture") and partial_material.shader.code.contains("src * fix_w + pass1 * (1.0 - fix_w)"), "a partial environment weight reads the framebuffer and uses the 0x64 weight")
	viewport.queue_free()
	print("creature blend: ", checks, " checks passed")
	quit()
