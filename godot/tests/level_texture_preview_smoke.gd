extends SceneTree

const TerrainFixture = preload("res://tests/session_sandbox_cases.gd")
const GeometryFixture = preload("res://tests/world_geometry_fixture.gd")
const Preview = preload("res://scenes/level_texture_preview.tscn")
var checks := 0

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		push_error(message)
		quit(1)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var world := GeometryFixture.world(TerrainFixture.new().flat_world())
	world.encode_u32(0xa4, 0x40)
	world.encode_u16(0xb8, 5049)
	var packet_start := world.decode_u32(0x84)
	for i in range(6):
		world.encode_u16(packet_start + 0x90 + 2 * i, [0, 0, 640, 0, 0, 320][i])
	var world_path := "user://level_texture_synthetic.WORLD"
	var world_file := FileAccess.open(world_path, FileAccess.WRITE)
	check(world_file != null, "WORLD fixture writable")
	world_file.store_buffer(world)
	world_file.close()
	var tex_path := "res://tests/fixtures/level_texture_synthetic.bin"
	var scene = Preview.instantiate()
	root.add_child(scene)
	check(scene.load_level(world_path, tex_path), "synthetic WORLD/TEX pair loads")
	check(scene.terrain_root.get_child_count() == 1, "one textured surface")
	var instance: MeshInstance3D = scene.terrain_root.get_child(0)
	var arrays: Array = instance.mesh.surface_get_arrays(0)
	check(arrays[Mesh.ARRAY_VERTEX].size() == 3 and arrays[Mesh.ARRAY_TEX_UV].size() == 3, "geometry and UVs reach ArrayMesh")
	var material: ShaderMaterial = instance.material_override
	var texture: Texture2D = material.get_shader_parameter("base_texture")
	check(texture != null and material.shader.code.contains("repeat_disable") and material.shader.code.contains("filter_linear"), "real decoded texture uses clamp and bilinear sampling")
	var image := texture.get_image()
	check(image.get_width() == 40 and image.get_height() == 20, "decoded texture dimensions")
	check(roundi(image.get_pixel(1, 0).r * 255.0) == 3, "exact synthetic RGB reaches material")
	var old: Node3D = scene.terrain_root
	check(not scene.load_level("user://missing.WORLD", tex_path) and scene.terrain_root == old, "failed replacement preserves old ground")
	if OS.get_cmdline_user_args().has("--capture") and DisplayServer.get_name() != "headless":
		for i in range(4): await process_frame
		var screenshot := root.get_texture().get_image()
		check(screenshot != null, "rendered viewport capture")
		var capture_path := "user://level_texture_synthetic_preview.png"
		check(screenshot.save_png(capture_path) == OK, "synthetic screenshot saved")
		print("level texture screenshot: ", ProjectSettings.globalize_path(capture_path))
	scene.queue_free()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(world_path))
	print("level texture preview: ", checks, " checks passed")
	quit()
