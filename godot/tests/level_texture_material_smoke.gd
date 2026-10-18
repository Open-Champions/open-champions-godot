extends SceneTree

const Factory = preload("res://scenes/level_texture_material.gd")
var checks := 0

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		push_error(message)
		quit(1)

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
	var pixel := viewport.get_texture().get_image().get_pixel(32, 32)
	check(absf(pixel.r - 0.5) < 0.2 and absf(pixel.g - 0.5) < 0.2, "GS reverse-alpha 64/128 mixes green and red")
	bytes[3] = 80
	image = Image.create_from_data(1, 1, false, Image.FORMAT_RGBA8, bytes)
	texture = ImageTexture.create_from_image(image)
	foreground.material_override = Factory.create(texture, {"alpha1": 1, "test1": 0x582f5, "blend_enabled": true, "sort_layer": 4})
	check(foreground.material_override.render_priority == 4, "translucent batch carries layer four priority")
	for i in 4: await process_frame
	pixel = viewport.get_texture().get_image().get_pixel(32, 32)
	check(pixel.r > 0.8 and pixel.g < 0.2, "GS alpha 80/128 is cut out before drawing")
	viewport.queue_free()
	print("level texture material: ", checks, " checks passed")
	quit()
