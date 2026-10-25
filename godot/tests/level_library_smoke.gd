extends SceneTree

const LibraryScene = preload("res://scenes/level_library.tscn")
const SandboxScene = preload("res://scenes/session_sandbox.tscn")
const WorldGeometryFixture = preload("res://tests/world_geometry_fixture.gd")
const SandboxCases = preload("res://tests/session_sandbox_cases.gd")

var checks := 0
var failures := 0
var loaded: Array[String] = []
var directory := "user://level-library-smoke"

func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error("Level library: " + message)

func write_file(path: String, data: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, "write authored fixture " + path)
	if file != null:
		file.store_buffer(data)
		file.close()

func make_world(offset: int) -> PackedByteArray:
	var data: PackedByteArray = WorldGeometryFixture.world(SandboxCases.new().flat_world())
	data.encode_u32(0xac, offset)
	return data

func press(library: Control, caption: String) -> void:
	for node in library.find_children("*", "Button", true, false):
		if node.text == caption:
			node.pressed.emit()
			return
	check(false, "button exists: " + caption)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var actual_dir := ProjectSettings.globalize_path(directory)
	DirAccess.make_dir_recursive_absolute(actual_dir.path_join("a"))
	DirAccess.make_dir_recursive_absolute(actual_dir.path_join("b"))
	var history := directory.path_join("history.json")
	if FileAccess.file_exists(history):
		DirAccess.remove_absolute(ProjectSettings.globalize_path(history))
	var first := directory.path_join("a/shared.WORLD")
	var second := directory.path_join("b/shared.WORLD")
	var malformed := directory.path_join("broken.WORLD")
	var water := directory.path_join("water.WORLD")
	var moving := directory.path_join("moving.WORLD")
	var missing := directory.path_join("missing.WORLD")
	write_file(first, make_world(16))
	write_file(second, make_world(40))
	write_file(malformed, PackedByteArray([1, 2, 3]))
	write_file(water, WorldGeometryFixture.world(SandboxCases.new().flat_world(), 0x80))
	write_file(moving, WorldGeometryFixture.world(SandboxCases.new().flat_world(), 0x20))
	var scene = SandboxScene.instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	var library = LibraryScene.instantiate()
	library.history_path = history
	root.add_child(library)
	library.configure(scene.load_terrain_file, scene.set_paused.bind(true))
	library.level_loaded.connect(func(path: String): loaded.append(path))
	await process_frame
	check(scene.controller != null, "real local native extension installed")
	library.show_library()
	check(scene.paused and scene.held_commands == 0 and library.visible, "opening releases input and pauses native scene")
	library._file_dialog.file_selected.emit(first)
	press(library, "Load / retry")
	check(loaded.size() == 1 and loaded[0] == ProjectSettings.globalize_path(first), "first selected WORLD loaded via shell callback")
	check(scene.raw_observation.terrain_mode == "static_world" and scene.raw_observation.submissions == 0, "first real terrain starts fresh native state")
	var first_vertex: Vector3 = scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0]
	scene.set_paused(false)
	scene.handle_input_event(SandboxCases.new().key(KEY_D))
	scene.handle_input_event(SandboxCases.new().key(KEY_H))
	scene.advance_step()
	scene.advance_step()
	check(scene.raw_observation.position_raw[0] != 0 and scene.raw_observation.hp_raw != 0x42c80000, "actual native movement and health changed before switch")
	library.show_library()
	var before: Dictionary = scene.controller.get_observation()
	var old_mesh: Mesh = scene.terrain_root.get_child(0).mesh
	var old_appearance: Mesh = scene.actor_visual.mesh
	# A moving-piece chunk now decodes as geometry; prepare and cancel it
	# without disturbing the loaded scene.
	var moving_prepared: Dictionary = scene.controller.prepare_terrain_file(moving)
	check(moving_prepared.get("ok", false) and scene.controller.cancel_terrain_file(moving_prepared.token) and scene.controller.get_observation() == before, "moving-piece chunk prepares without replacing the scene")
	library._on_file_selected(moving)
	for candidate in [malformed, water, missing]:
		library._on_file_selected(candidate)
		library._load_selected()
		check(scene.controller.get_observation() == before and scene.terrain_root.get_child(0).mesh == old_mesh and scene.actor_visual.mesh == old_appearance, "refusal/missing retains native state, ground and appearance: " + candidate)
	check(library._entries.size() == 5 and "missing" in library._list.get_item_text(library._find(ProjectSettings.globalize_path(missing))), "missing recent remains actionable")
	library._on_file_selected(second)
	check(library._find(ProjectSettings.globalize_path(first)) >= 0 and library._find(ProjectSettings.globalize_path(second)) >= 0, "duplicate basenames keep full-path identity")
	library._load_selected()
	var second_vertex: Vector3 = scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0]
	check(second_vertex != first_vertex and scene.raw_observation.position_raw[0] == 0 and scene.raw_observation.hp_raw == 0x42c80000 and scene.held_commands == 0, "second accepted WORLD changes geometry and clears native position/health/held input")
	library.show_library()
	library._selected_path = ProjectSettings.globalize_path(first)
	library._load_selected()
	check(scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0] == first_vertex, "recent selection returns to first real geometry")
	library.show_library()
	var retained: Dictionary = scene.controller.get_observation()
	var retained_mesh: Mesh = scene.terrain_root.get_child(0).mesh
	write_file(second, PackedByteArray([1, 2, 3, 4]))
	library._selected_path = ProjectSettings.globalize_path(second)
	library._load_selected()
	check(scene.controller.get_observation() == retained and scene.terrain_root.get_child(0).mesh == retained_mesh and scene.actor_visual.mesh == old_appearance, "changed file is revalidated and refused atomically")
	scene.set_paused(false)
	press(library, "Add WORLD file")
	check(scene.paused and scene.held_commands == 0, "opening file picker pauses and releases input")
	library._file_dialog.canceled.emit()
	library._file_dialog.hide()
	check(scene.controller.get_observation() == retained and scene.terrain_root.get_child(0).mesh == retained_mesh and scene.actor_visual.mesh == old_appearance and "canceled" in library._status.text, "cancel preserves native and visual state")
	library._selected_path = ProjectSettings.globalize_path(missing)
	press(library, "Forget")
	check(library._find(ProjectSettings.globalize_path(missing)) == -1 and not FileAccess.file_exists(missing), "forget removes history only")
	var persisted: String = FileAccess.get_file_as_string(history)
	check(first.get_file() in persisted and second.get_file() in persisted and not ("missing.WORLD" in persisted), "history stores selected paths only")
	library.queue_free()
	await process_frame
	var reopened = LibraryScene.instantiate()
	reopened.history_path = history
	root.add_child(reopened)
	await process_frame
	check(reopened._entries.size() == 5 and reopened._find(ProjectSettings.globalize_path(first)) >= 0 and reopened._find(ProjectSettings.globalize_path(second)) >= 0, "isolated user history reopens as candidates")
	reopened.configure(scene.load_terrain_file, scene.set_paused.bind(true))
	reopened._selected_path = ProjectSettings.globalize_path(second)
	reopened._load_selected()
	check("refused" in reopened._status.text and ProjectSettings.globalize_path(second) in reopened._status.text, "native refusal detail and long local path are visible")
	for size in [Vector2i(960, 720), Vector2i(1280, 720)]:
		root.content_scale_size = size
		root.size = size
		reopened.show_library()
		reopened._load_selected()
		await process_frame
		check(reopened._list.size.y >= 100 and reopened._path_label.size.x > 0 and reopened._status.size.x > 0, "panel layout remains usable at " + str(size))
		var capture_dir := OS.get_environment("OPEN_CHAMPIONS_CAPTURE_DIR")
		if not DisplayServer.get_name() == "headless" and not capture_dir.is_empty():
			await RenderingServer.frame_post_draw
			DirAccess.make_dir_recursive_absolute(capture_dir)
			var capture := capture_dir.path_join("panel-%dx%d.png" % [size.x, size.y])
			var image := root.get_texture().get_image()
			check(image != null and image.get_size() == size and image.save_png(capture) == OK, "exported viewport capture at " + str(size))
	var scan_dir := directory.path_join("scan")
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(scan_dir.path_join("nested")))
	for i in 130:
		write_file(scan_dir.path_join("candidate-%03d.WORLD" % i), PackedByteArray([1]))
	write_file(scan_dir.path_join("nested/hidden.WORLD"), PackedByteArray([1]))
	reopened._directory_dialog.dir_selected.emit(ProjectSettings.globalize_path(scan_dir))
	check(reopened._entries.size() == 128 and reopened._find(ProjectSettings.globalize_path(first)) >= 0 and reopened._find(ProjectSettings.globalize_path(scan_dir.path_join("candidate-122.WORLD"))) >= 0, "one-folder scan fills available slots and preserves recents")
	check(reopened._find(ProjectSettings.globalize_path(scan_dir.path_join("candidate-123.WORLD"))) == -1 and reopened._find(ProjectSettings.globalize_path(scan_dir.path_join("nested/hidden.WORLD"))) == -1, "scan omits overflow and nested child")
	var explicit := directory.path_join("explicit.WORLD")
	write_file(explicit, PackedByteArray([1]))
	reopened._file_dialog.file_selected.emit(ProjectSettings.globalize_path(explicit))
	check(reopened._entries.size() == 128 and reopened._find(ProjectSettings.globalize_path(explicit)) >= 0 and reopened._find(ProjectSettings.globalize_path(first)) >= 0, "explicit Add makes room without evicting a recent load")
	check(loaded.size() == 3, "only three successful native loads emitted success")
	print("LEVEL_LIBRARY_SMOKE: %d checks, %d failures" % [checks, failures])
	if failures == 0:
		print("LEVEL_LIBRARY_SMOKE_PASS")
	reopened.queue_free()
	scene.queue_free()
	await process_frame
	await process_frame
	quit(0 if failures == 0 else 1)
