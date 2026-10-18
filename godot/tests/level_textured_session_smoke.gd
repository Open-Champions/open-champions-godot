extends SceneTree

const TerrainFixture = preload("res://tests/session_sandbox_cases.gd")
const GeometryFixture = preload("res://tests/world_geometry_fixture.gd")
const TexturedSession = preload("res://scenes/level_textured_session.tscn")
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
	for i in range(6): world.encode_u16(packet_start + 0x90 + 2 * i, [0, 0, 640, 0, 0, 320][i])
	var world_path := "user://level_textured_session_synthetic.WORLD"
	var world_file := FileAccess.open(world_path, FileAccess.WRITE)
	check(world_file != null, "WORLD fixture writable")
	world_file.store_buffer(world)
	world_file.close()
	var tex_path := "res://tests/fixtures/level_texture_synthetic.bin"
	var scene = TexturedSession.instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	check(scene.controller != null, "inherited native Session controller is live")
	check(scene.load_textured_terrain(world_path, tex_path), "matching WORLD/TEX pair loads into Session")
	check(scene.terrain_root.get_child_count() == 1, "Session ground has a textured surface")
	var mesh: MeshInstance3D = scene.terrain_root.get_child(0)
	var arrays: Array = mesh.mesh.surface_get_arrays(0)
	check(arrays[Mesh.ARRAY_VERTEX].size() == 3 and arrays[Mesh.ARRAY_TEX_UV].size() == 3, "Session mesh retains actual geometry and UVs")
	check(arrays[Mesh.ARRAY_VERTEX][0] == Vector3(1, -1, -1), "native Z-up vertex uses inherited Session display axes")
	var material: ShaderMaterial = mesh.material_override
	var texture: Texture2D = material.get_shader_parameter("base_texture")
	check(texture != null and material.shader.code.contains("repeat_disable") and material.shader.code.contains("filter_linear"), "Session material samples decoded texture with clamp and bilinear")
	check(texture.get_width() == 40 and texture.get_height() == 20, "Session texture dimensions")
	var old: Node3D = scene.terrain_root
	check(not scene.load_textured_terrain("user://missing.WORLD", tex_path) and scene.terrain_root == old, "failed pair keeps owned terrain")
	scene.advance_step()
	check(scene.terrain_root == old and scene.raw_observation.get("ready_ticks", -1) >= 0, "ready Session step retains textured ground")
	if OS.get_cmdline_user_args().has("--capture") and DisplayServer.get_name() != "headless":
		scene.set_process(false)
		scene.actor.hide() # inspect the authored ground without avatar occlusion
		scene.camera.position = Vector3(14, 17, 13)
		scene.camera.look_at(Vector3(1, 1, -5), Vector3.UP)
		for i in range(4): await process_frame
		var screenshot: Image = scene.viewport.get_texture().get_image()
		check(screenshot != null, "Session viewport capture")
		var capture_path := "user://level_textured_session_synthetic.png"
		check(screenshot.save_png(capture_path) == OK, "Session screenshot saved")
		print("textured Session screenshot: ", ProjectSettings.globalize_path(capture_path))
	scene.queue_free()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(world_path))
	print("level textured Session: ", checks, " checks passed")
	quit()
