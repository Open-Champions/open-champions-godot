extends SceneTree

# Combined production-resource acceptance, using only authored fixture bytes.
# UI signals and shell input drive the actual native controller and decoders.
const WorldFixture = preload("res://tests/world_geometry_fixture.gd")
const SandboxCases = preload("res://tests/session_sandbox_cases.gd")
const SkinFixture = preload("res://tests/skinning_fixture.gd")
const Archives = preload("res://tests/texture_fixture.gd")
const ArtFixture = preload("res://tests/scene_original_art_smoke.gd")
var scene
var checks := 0
var failures := 0
var output := ""
# One fixture directory per process. user:// is shared by every checkout of
# this project and by the exported app (one project name), so a smoke running
# elsewhere at the same time could rewrite cosmetic.lmp between this run's
# write and its read. That was the intermittent "stale clip" failure: the
# picker opened a file already holding the 512 clip, so the rewrite changed
# nothing and the archive fingerprint still matched.
var directory := "user://playable-wave-smoke-%d" % OS.get_process_id()

func _finalize() -> void:
	remove_tree(ProjectSettings.globalize_path(directory))

func remove_tree(path: String) -> void:
	var dir := DirAccess.open(path)
	if dir == null: return
	for name in dir.get_directories(): remove_tree(path.path_join(name))
	for name in dir.get_files(): dir.remove(name)
	DirAccess.remove_absolute(path)

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="):
			output = arg.trim_prefix("--capture-dir=")
	call_deferred("run")

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("Playable wave: " + label)

func same_state(actual: Dictionary, expected: Dictionary) -> bool:
	# Established native-save boundary: mesh/source labels are presentation;
	# indexed snapshots may defer historical-record validation. Compare every
	# authoritative field, including exact raw32 values and counters.
	actual = actual.duplicate(true)
	expected = expected.duplicate(true)
	for field in ["terrain_name", "terrain_note", "terrain_mode", "save_history_pending"]:
		actual.erase(field)
		expected.erase(field)
	for field in expected:
		if actual.get(field) != expected[field]:
			print("STATE_DIFF ", field, " expected=", expected[field], " actual=", actual.get(field))
	return actual == expected

func write(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, "write synthetic fixture " + path.get_file())
	if file != null:
		file.store_buffer(bytes)
		file.close()

func capture(label: String) -> void:
	# Allow nested container minimum sizes to settle after synchronous fixtures.
	for i in range(4): await process_frame
	if output.is_empty(): return
	check(DisplayServer.get_name() != "headless", "captures require graphical execution")
	if DisplayServer.get_name() == "headless": return
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and not image.is_empty() and image.save_png(output.path_join(label + ".png")) == OK, "capture " + label)

func press(parent: Node, caption: String) -> void:
	for button in parent.find_children("*", "Button", true, false):
		if button.text == caption:
			button.pressed.emit()
			return
	check(false, "button exists " + caption)

func choose_level(path: String) -> void:
	scene.actions.library.pressed.emit()
	scene.components.library._file_dialog.file_selected.emit(path)
	press(scene.components.library, "Load / retry")

func choose_pair(path: String) -> void:
	scene.actions.animation.pressed.emit()
	var preview = scene.components.animation
	preview.file_dialog.file_selected.emit(path)
	preview.model_choice.select(1)
	preview.animation_choice.select(1)
	preview.pair_button.pressed.emit()
	check(preview.active and not scene.actor_visual.visible, "real animation selection owns cosmetic mesh")

func corners() -> PackedVector3Array:
	return scene.components.animation.mesh_instance.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]

func choose_art(id: String) -> void:
	var panel = scene.components.art
	for i in range(panel.identity_choice.item_count):
		if panel.identity_choice.get_item_text(i) == id:
			panel.identity_choice.select(i)
			panel.identity_choice.item_selected.emit(i)
	check(panel.row_list.item_count == 1, "one explicit synthetic art row " + id)
	if panel.row_list.item_count == 1:
		panel.row_list.select(0)
		panel.load_button.pressed.emit()
	check(panel.loaded.has(id), "real native art loaded " + id)

func run() -> void:
	root.content_scale_size = root.size
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(directory))
	if not output.is_empty(): DirAccess.make_dir_recursive_absolute(output)
	check(ClassDB.class_exists("ChampionsSessionController"), "real locally built extension loaded")
	if not ClassDB.class_exists("ChampionsSessionController"):
		quit(1)
		return
	var first := directory.path_join("first.WORLD")
	var second := directory.path_join("second.WORLD")
	var bad := directory.path_join("bad.WORLD")
	var water := directory.path_join("water.WORLD")
	var moving := directory.path_join("moving.WORLD")
	var model := directory.path_join("cosmetic.lmp")
	var hud := directory.path_join("HUD.LMP")
	var world := WorldFixture.world(SandboxCases.new().flat_world())
	write(first, world)
	world.encode_u32(0xac, 40)
	write(second, world)
	write(bad, PackedByteArray([1, 2, 3]))
	write(water, WorldFixture.world(SandboxCases.new().flat_world(), 0x80))
	write(moving, WorldFixture.world(SandboxCases.new().flat_world(), 0x20))
	write(model, Archives.lmp(["shape.vif", "motion.anm"], [SkinFixture.model(), SkinFixture.animation()]))
	write(hud, Archives.lmp(["bar_red10.tex", "hud_statusbar00.tex"], [ArtFixture.fixture(100, 8, 9), ArtFixture.fixture(160, 46, 17)]))
	scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	# No component_factory: production resolves all three frozen resource paths.
	root.add_child(scene)
	await process_frame
	check(not scene.component_factory.is_valid() and scene.components.size() == 3, "all real production components attached without mocks")
	for id in scene.COMPONENTS:
		check(scene.components.has(id) and scene.components[id].scene_file_path == scene.COMPONENTS[id].path and not scene.actions[id].disabled, "real component resource enabled " + id)
	if scene.components.size() != 3:
		quit(1)
		return
	# Isolate path history without replacing the component or callback.
	scene.components.library.history_path = directory.path_join("history.json")
	scene.components.library._entries.clear()
	check(scene.paused and scene.raw_observation.ready_ticks == 0 and not scene.actions.play.disabled, "entry offers integrated play capability")
	await capture("01-entry")
	scene.actions.library.pressed.emit()
	check(scene.paused and scene.components.library.visible and not scene.menu.visible, "library opens as sole modal")
	scene.components.library._file_dialog.file_selected.emit(first)
	await capture("02-library")
	press(scene.components.library, "Load / retry")
	check(scene.raw_observation.terrain_mode == "static_world" and scene.paused, "library loads native WORLD")
	var vertex: Vector3 = scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0]
	choose_level(second)
	check(scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0] != vertex, "second WORLD changes actual mesh")
	choose_level(first)
	check(scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0] == vertex, "return to first WORLD restores geometry")
	scene.actions.diagnostic.pressed.emit()
	scene.handle_input_event(SandboxCases.new().key(KEY_D))
	for i in range(4): scene.advance_step()
	check(scene.raw_observation.ready_ticks == 3 and scene.raw_observation.position.x > 0, "native movement in selected WORLD")
	scene.set_paused(true)
	await capture("03-level-loaded")
	choose_pair(model)
	check(scene.paused and not scene.components.animation.play_button.disabled, "visible paused picker permits explicit playback intent")
	scene.components.animation.play_button.pressed.emit()
	check(scene.components.animation.playing and scene.components.animation.sample_tick == 0, "play waits for native session resume")
	await capture("04-animation-picker")
	press(scene.components.animation, "Hide picker (keep preview)")
	scene.resume_session()
	scene.components.animation.observe(scene.raw_observation)
	var initial := corners()
	await capture("05-animation-zero")
	# Focus may change during capture; explicitly resume this scripted interaction.
	scene.resume_session()
	for i in range(4): scene.advance_step()
	var preview = scene.components.animation
	var native_sample: Dictionary = scene.inspector.sample_skinned_model(model, preview.model_row, preview.animation_row, "rta", preview.archive_fingerprint, preview.sample_tick)
	var expected := PackedVector3Array()
	for point in native_sample.meshes[0].positions: expected.append(Vector3(point.x, point.z, -point.y))
	check(preview.sample_tick == 4 and corners() != initial and corners() == expected, "combined ready ticks animate exact native bridge geometry")
	await capture("06-animation-four")
	scene.actions.art.pressed.emit()
	check(scene.paused and scene.held_commands == 0, "art picker pauses/releases movement")
	var before: Dictionary = scene.controller.get_observation()
	var art = scene.components.art
	art.file_dialog.file_selected.emit(hud)
	choose_art("bar_red")
	choose_art("statusbar")
	check(scene.controller.get_observation() == before, "art decode is read-only native state")
	check(art.loaded.bar_red.image.get_data().slice(0, 4) == ArtFixture.expected_pixel(0, 0, 9), "native-shaped art has independently expected pixels")
	await capture("07-art-picker")
	if DisplayServer.get_name() != "headless":
		check(art.picker.position.y + art.picker.size.y <= root.size.y and art.picker.position.x + art.picker.size.x <= root.size.x, "art picker and Close button fit the exported window")
	art.picker.hide()
	scene.scene_scroll.ensure_control_visible(art)
	await capture("08-art-loaded")
	art.scale_choice.select(1)
	art.scale_choice.item_selected.emit(1)
	check(art.integer_scale == 2, "integer art enlargement")
	await capture("09-art-2x")
	scene.scene_scroll.scroll_vertical = 0
	scene.resume_session()
	scene.handle_input_event(SandboxCases.new().key(KEY_D))
	scene.handle_input_event(SandboxCases.new().key(KEY_H))
	scene.actions.pause.pressed.emit()
	check(scene.paused and scene.held_commands == 0 and not scene.pending_damage, "combined pause releases controls and damage edge")
	before = scene.controller.get_observation()
	var pose := corners()
	for i in range(3): scene.advance_step()
	check(scene.controller.get_observation() == before and corners() == pose, "pause freezes native state and cosmetic pose")
	await capture("10-paused")
	await save_cases()
	await refusal_cases(first, bad, water, moving, model, hud)
	# Diagnostic mode remains explicitly bounded after the runtime wave.
	scene.reset_session()
	scene.resume_session()
	for i in range(514): scene.advance_step()
	check(scene.stopped and scene.raw_observation.ready_ticks == 512, "diagnostic 512-tick cap without auto-reset")
	await capture("12-expected-cap")
	if DisplayServer.get_name() != "headless":
		var label: String = scene.save_format.get_item_text(scene.save_format.selected)
		var width: float = scene.save_format.get_theme_font("font").get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, scene.save_format.get_theme_font_size("font_size")).x
		check(not scene.save_format.text.is_empty() and scene.save_format.size.x >= width, "save format remains legible after hidden-menu reload")
	scene.reset_session()
	check(scene.paused and not scene.stopped and scene.raw_observation.ready_ticks == 0, "explicit reset recovers same level")
	var owner = scene.controller
	scene.return_to_selection()
	scene.resume_session()
	check(scene.controller == owner and not scene.paused, "return and re-enter retain owner")
	scene.handle_input_event(SandboxCases.new().key(KEY_D))
	scene._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
	check(scene.paused and scene.held_commands == 0, "focus loss releases native input")
	var old_art = weakref(art)
	var old_preview = weakref(preview)
	owner = null
	art = null
	preview = null
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	check(old_art.get_ref() == null and old_preview.get_ref() == null, "component nodes released on scene exit")
	print("PLAYABLE_WAVE_%s checks=%d failures=%d renderer=%s mode=diagnostic cap=512" % ["PASS" if failures == 0 else "FAIL", checks, failures, DisplayServer.get_name()])
	quit(0 if failures == 0 else 1)

func save_cases() -> void:
	var replay := directory.path_join("combined.ocnsave")
	var snapshot := directory.path_join("combined.snapshot")
	scene.save_format.select(0)
	scene.save_dialog.file_selected.emit(replay)
	check(FileAccess.file_exists(replay), "save dialog writes native replay")
	scene.save_format.select(1)
	scene.save_dialog.file_selected.emit(snapshot)
	check(FileAccess.file_exists(snapshot), "save dialog writes native snapshot")
	var saved: Dictionary = scene.controller.get_observation()
	scene.resume_session()
	scene.handle_input_event(SandboxCases.new().key(KEY_D))
	for i in range(5): scene.advance_step()
	scene.set_paused(true)
	var uninterrupted: Dictionary = scene.controller.get_observation()
	check(uninterrupted.position != saved.position, "state changes after save")
	for path in [replay, snapshot]:
		scene.load_dialog.file_selected.emit(path)
		check(scene.paused and same_state(scene.controller.get_observation(), saved), "load restores exact authoritative native observation " + path.get_file())
		check(scene.raw_observation.terrain_name == "Restored collision ground", "legacy save restores collision presentation, not WORLD mesh")
		check(not scene.components.animation.active and scene.actor_visual.visible, "load clears cosmetic ownership")
		check(scene.components.art.loaded.size() == 2, "load preserves read-only selected art")
		scene.resume_session()
		scene.handle_input_event(SandboxCases.new().key(KEY_D))
		for i in range(5): scene.advance_step()
		scene.set_paused(true)
		check(same_state(scene.controller.get_observation(), uninterrupted), "identical future native continuation " + path.get_file())
	# Equal-counter load must still clear the selected preview.
	choose_pair(directory.path_join("cosmetic.lmp"))
	press(scene.components.animation, "Hide picker (keep preview)")
	check(scene.save_session_file(snapshot) and scene.load_session_file(snapshot), "equal-counter native save/load")
	check(not scene.components.animation.active and scene.actor_visual.visible, "equal-counter load clears cosmetic preview")
	await capture("11-loaded-save")

func refusal_cases(first: String, bad: String, water: String, moving: String, model: String, hud: String) -> void:
	choose_pair(model)
	press(scene.components.animation, "Hide picker (keep preview)")
	var before: Dictionary = scene.controller.get_observation()
	var ground: Mesh = scene.terrain_root.get_child(0).mesh
	# A moving-piece chunk decodes as rest-pose ground since real-data round 2:
	# prepare and cancel it without replacing the scene.
	var moving_prepared: Dictionary = scene.controller.prepare_terrain_file(moving)
	check(moving_prepared.get("ok", false) and scene.controller.cancel_terrain_file(moving_prepared.token) and scene.controller.get_observation() == before, "moving-piece WORLD prepares without replacing the scene")
	for path in [bad, water, directory.path_join("missing.WORLD")]:
		choose_level(path)
		check(scene.controller.get_observation() == before and scene.terrain_root.get_child(0).mesh == ground and scene.components.animation.active, "WORLD refusal preserves state/ground/cosmetic " + path.get_file())
	await capture("11b-world-refusal")
	press(scene.components.library, "Close")
	var art = scene.components.art
	var pixels: PackedByteArray = art.loaded.bar_red.image.get_data()
	scene.actions.art.pressed.emit()
	art.file_dialog.file_selected.emit(directory.path_join("missing.LMP"))
	check(art.loaded.bar_red.image.get_data() == pixels and scene.controller.get_observation() == before, "art refusal preserves pixels and native state")
	await capture("11c-art-refusal")
	art.picker.hide()
	write(model, Archives.lmp(["shape.vif", "motion.anm"], [SkinFixture.model(), SkinFixture.animation(512)]))
	scene.actions.animation.pressed.emit()
	scene.components.animation.restart_button.pressed.emit()
	check(not scene.components.animation.active and scene.actor_visual.visible and not scene.components.animation.last_error.is_empty(), "stale clip clears cosmetic owner and restores static mesh")
	check(scene.controller.get_observation() == before, "failed clip never changes native state")
	await capture("11d-clip-refusal")
	press(scene.components.animation, "Hide picker (keep preview)")
	write(directory.path_join("bad.save"), PackedByteArray([1, 2, 3]))
	check(not scene.load_session_file(directory.path_join("bad.save")), "corrupt save refuses")
	check(scene.controller.get_observation() == before and scene.terrain_root.get_child(0).mesh == ground, "corrupt save preserves current state and ground")
	check(not scene.save_session_file(directory.path_join("absent/subdir/save")), "unwritable destination refuses")
	check(same_state(scene.controller.get_observation(), before), "failed save preserves native state")
	await capture("11e-save-refusal")
	choose_level(first)
	check(scene.raw_observation.ready_ticks == 0 and not scene.components.animation.active, "successful level replacement resets and clears cosmetic owner")
	art.open_source(hud)
