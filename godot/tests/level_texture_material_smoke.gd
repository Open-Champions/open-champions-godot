extends SceneTree

const Factory = preload("res://scenes/level_texture_material.gd")
const Terrain = preload("res://scenes/level_texture_terrain.gd")
const LevelDrawCamera = preload("res://scenes/level_draw_camera.gd")
var checks := 0
var failed := 0

# Every failed check is reported; the exit status is set once at the end.
func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failed += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(64, 64)
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
	var bytes := PackedByteArray([0, 255, 0, 64])
	var image := Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, bytes)
	var texture := ImageTexture.create_from_image(image)
	foreground.material_override = Factory.create(texture, {"alpha1": 1, "test1": 0x587f4, "blend_enabled": true})
	check(foreground.material_override.render_priority == 0, "ordinary batch has base sort layer")
	for i in 4: await process_frame
	await RenderingServer.frame_post_draw
	var pixel := viewport.get_texture().get_image().get_pixel(32, 32)
	check(absf(pixel.r - 0.5) < 0.2 and absf(pixel.g - 0.5) < 0.2, "GS reverse-alpha 64/128 mixes green and red")
	bytes[3] = 80
	image = Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, bytes)
	texture = ImageTexture.create_from_image(image)
	foreground.material_override = Factory.create(texture, {"alpha1": 1, "test1": 0x582f5, "blend_enabled": true, "sort_layer": 4})
	check(foreground.material_override.render_priority == 4, "translucent batch carries layer four priority")
	for i in 4: await process_frame
	await RenderingServer.frame_post_draw
	pixel = viewport.get_texture().get_image().get_pixel(32, 32)
	check(pixel.r > 0.8 and pixel.g < 0.2, "GS alpha 80/128 is cut out before drawing")
	bytes[3] = 0
	image = Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, bytes)
	texture = ImageTexture.create_from_image(image)
	foreground.material_override = Factory.create(texture, {"alpha1": 2, "test1": 0x587f4, "blend_enabled": true})
	for i in 4: await process_frame
	await RenderingServer.frame_post_draw
	pixel = viewport.get_texture().get_image().get_pixel(32, 32)
	check(pixel.g > 0.8 and pixel.r < 0.2, "alpha 2 with alpha 0 replaces the destination with the source")
	bytes[3] = 128
	image = Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, bytes)
	texture = ImageTexture.create_from_image(image)
	foreground.material_override = Factory.create(texture, {"alpha1": 2, "test1": 0x587f4, "blend_enabled": true})
	for i in 4: await process_frame
	await RenderingServer.frame_post_draw
	pixel = viewport.get_texture().get_image().get_pixel(32, 32)
	check(pixel.r < 0.05 and pixel.g < 0.05, "alpha 2 with alpha 128 writes black and does not keep the destination")
	var automap := Factory.automap_material(texture)
	check(automap.render_priority == 5 and automap.shader.code.contains("filter_nearest") and automap.shader.code.contains("depth_test_disabled"), "automap tiles are layer 5, point sampled and not depth tested")
	# An environment chunk without its context-2 first pass is not drawn with
	# the second-pass equation 9 over the framebuffer (level-draw.md 1/3): the
	# base pass stays opaque until an environment image exists.
	bytes = PackedByteArray([0, 255, 0, 64])
	image = Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, bytes)
	texture = ImageTexture.create_from_image(image)
	foreground.material_override = Factory.create(texture, {"alpha1": 9, "test1": 0x587f4, "blend_enabled": true, "environment": true})
	for i in 4: await process_frame
	await RenderingServer.frame_post_draw
	pixel = viewport.get_texture().get_image().get_pixel(32, 32)
	check(pixel.g > 0.8 and pixel.r < 0.05, "environment chunk without its first pass draws its base pass opaque")
	viewport.queue_free()
	camera_rows()
	await constant_light()
	await environment_pass()
	if failed:
		print("level texture material: ", failed, " of ", checks, " checks failed")
		quit(1)
		return
	print("level texture material: ", checks, " checks passed")
	quit()

func near(a: float, b: float) -> bool:
	return absf(a - b) < 1e-6

# g_cameraMatrix rows are the camera-space images of the world x, y, z axes
# (render-pipeline.md section 3 at research-textures 391cf6b). A camera looking
# along native +x has right (0,-1,0), down (0,0,-1) and forward (1,0,0), so the
# memory is (0,0,1, -1,0,0, 0,-1,0). Expected rows from the pinned
# ref_leveldraw.py (sha256 936cd0ee...3bca): env_rows(axis_xy(m), 0, 0) =
# ((0,0,-6,0), (0,-6,192,192)).
func camera_rows() -> void:
	var native = ClassDB.instantiate("ChampionsLevelTexturePreview")
	var holder := Node3D.new()
	root.add_child(holder)
	var camera := Camera3D.new()
	holder.add_child(camera)
	camera.global_transform = Transform3D(Basis(Vector3(0, 0, 1), Vector3(0, 1, 0), Vector3(-1, 0, 0)), Vector3.ZERO)
	var drawer := Node3D.new()
	drawer.set_script(LevelDrawCamera)
	var rows: Dictionary = native.environment_rows(drawer._camera_components(camera), 0, 0, -1)
	var row4: PackedFloat64Array = rows.get("row4", PackedFloat64Array())
	var row5: PackedFloat64Array = rows.get("row5", PackedFloat64Array())
	check(rows.get("ok", false) and row4.size() == 4 and row5.size() == 4, "environment rows accept the camera components")
	check(near(row4[0], 0) and near(row4[1], 0) and near(row4[2], -6) and near(row4[3], 0), "row 4 is 6 x (m00, m01, m10, m11) of the camera matrix rows")
	check(near(row5[0], 0) and near(row5[1], -6) and near(row5[2], 192) and near(row5[3], 192), "row 5 is (6 m20, 6 m21, 192, 192)")
	drawer.free()
	holder.queue_free()
	native.free()

func quad_arrays(z: float, half: float) -> Array:
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(-half, -half, z), Vector3(half, -half, z), Vector3(half, half, z), Vector3(-half, -half, z), Vector3(half, half, z), Vector3(-half, half, z)])
	arrays[Mesh.ARRAY_TEX_UV] = PackedVector2Array([Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 1), Vector2(1, 0), Vector2(0, 0)])
	return arrays

func render_view(perspective: bool) -> SubViewport:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(64, 64)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var camera := Camera3D.new()
	if perspective:
		camera.fov = 50.0
	else:
		camera.projection = Camera3D.PROJECTION_ORTHOGONAL
		camera.size = 2.0
		camera.position = Vector3(0, 0, 3)
	camera.current = true
	viewport.add_child(camera)
	return viewport

# A flat constant-light chunk (flag 0x200, constant 0) at base level 0x80 is
# texel x 128 / 128: the texture is unchanged (render-pipeline.md section 9
# item 3). Without a documented base level the constant colour is refused and
# the chunk keeps its base texture only.
func constant_light() -> void:
	var viewport := render_view(false)
	var texel := PackedByteArray([0x60, 0xa0, 0x30, 0])
	var source := {"width": 1, "height": 1, "rgba": texel}
	var positions := PackedVector3Array()
	for point in quad_arrays(0.1, 1.0)[Mesh.ARRAY_VERTEX]: positions.push_back(Vector3(point.x, -point.z, point.y))
	var surface := {"image": 0, "positions": positions, "uv": quad_arrays(0.1, 1.0)[Mesh.ARRAY_TEX_UV], "alpha1": 1, "test1": 0x587f4, "blend_enabled": true, "sort_layer": 4, "dynamic_light": false, "light_constant": 0, "environment": false}
	var terrain: Node3D = Terrain.build({"images": [source], "surfaces": [surface]}, true)
	check(terrain != null, "constant-light terrain builds")
	viewport.add_child(terrain)
	var chunk: MeshInstance3D = terrain.get_child(0)
	var unlit_colours = chunk.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
	check(unlit_colours == null or unlit_colours.is_empty(), "no constant colour is stored before the level's base level is known")
	var methods: Array = []
	for method in (Terrain as Script).get_script_method_list(): methods.append(str(method.name))
	var native = ClassDB.instantiate("ChampionsLevelTexturePreview")
	var builder: Object = load("res://scenes/level_texture_terrain.gd")
	if "set_constant_light" in methods:
		check(not builder.call("set_constant_light", chunk, 0x30, native), "a base level other than 0x80, 0x40 or 0x20 is refused")
		check(builder.call("set_constant_light", chunk, 0x40, native), "base level 0x40 is accepted")
		var stored: PackedColorArray = chunk.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
		check(stored.size() == 6 and stored[0].r8 == 0x40, "flat constant 0 at base 0x40 stores channel 0x40")
		check(builder.call("set_constant_light", chunk, 0x80, native), "base level 0x80 is accepted")
	native.free()
	for i in 4: await process_frame
	var pixel := viewport.get_texture().get_image().get_pixel(32, 32)
	check(absf(pixel.r8 - 0x60) <= 2 and absf(pixel.g8 - 0xa0) <= 2 and absf(pixel.b8 - 0x30) <= 2, "constant 0 at base 0x80 leaves the texel unchanged (%d, %d, %d)" % [pixel.r8, pixel.g8, pixel.b8])
	viewport.queue_free()

# The environment first pass is modulated by the vertex colour, and its screen
# term is (sx, sy) in pixels of the 1280 x 448 buffer from its centre with y
# down: sy = F * 0.4666667 * cam.y / cam.z (render-pipeline.md sections 3, 4.6
# and 9 item 6). Fov 50 gives F = 1024. A fragment 16 rows above the centre of
# this 64-row view is at sy = -111, so v = 192 - 7.8 wraps to texel row 56; one
# 16 rows below is at row 72. The image is black above row 64 and white below.
func environment_pass() -> void:
	var viewport := render_view(true)
	var env_bytes := PackedByteArray()
	env_bytes.resize(128 * 128 * 4)
	for y in 128:
		for x in 128:
			var value := 255 if y >= 64 else 0
			var at := (y * 128 + x) * 4
			env_bytes[at] = value
			env_bytes[at + 1] = value
			env_bytes[at + 2] = value
			env_bytes[at + 3] = 255
	var env_texture := ImageTexture.create_from_image(Image.create_from_data(128, 128, false, Image.FORMAT_RGBA8, env_bytes))
	var base := ImageTexture.create_from_image(Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, PackedByteArray([0, 0, 0, 128])))
	var arrays := quad_arrays(-2.0, 2.0)
	var colours := PackedColorArray()
	colours.resize(6)
	colours.fill(Color(0.5, 0.5, 0.5, 1.0))
	arrays[Mesh.ARRAY_COLOR] = colours
	var custom := PackedFloat32Array()
	custom.resize(24)
	arrays[Mesh.ARRAY_CUSTOM0] = custom
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, Mesh.ARRAY_CUSTOM_RGBA_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT)
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var material := Factory.create(base, {"alpha1": 9, "test1": 0x587f4, "blend_enabled": true, "environment": true, "env_ready": true}, true)
	material.set_shader_parameter("env_texture", env_texture)
	material.set_shader_parameter("use_env", true)
	material.set_shader_parameter("env_row4", Vector4(0, 0, 0, 0))
	material.set_shader_parameter("env_row5", Vector4(0, 0, 192, 192))
	instance.material_override = material
	viewport.add_child(instance)
	for i in 4: await process_frame
	var picture := viewport.get_texture().get_image()
	var above := picture.get_pixel(32, 16)
	var below := picture.get_pixel(32, 48)
	check(above.r < 0.1, "screen y points down: above the centre samples environment row 56 (%.3f)" % above.r)
	check(below.r > 0.4 and below.r < 0.85, "below the centre samples row 72, modulated by the vertex colour (%.3f)" % below.r)
	viewport.queue_free()
