extends "res://tests/playable_wave_smoke.gd"

# Real production shell/resources; explicit UI handlers submit deterministic
# fixed steps. The default is ten logical minutes, not ten wall-clock minutes.
var target_ticks := 36000
var expected_art_count := 2
var peak_records := 0
var warm_capacity := PackedInt64Array()
var started_usec := 0

func play_state(value: Dictionary) -> Dictionary:
	value = value.duplicate(true)
	# Allocation capacity is measured separately; it is not game authority and
	# a freshly decoded continuation may allocate a different bounded capacity.
	value.erase("history_retained_capacity")
	return value

func step_play() -> void:
	var tick: int = scene.raw_observation.ready_ticks
	scene.held_commands = 1 if (tick / 32) % 2 == 0 else 2
	scene.pending_damage = tick % 2 == 0
	scene.advance_step()
	var o: Dictionary = scene.raw_observation
	var count := 0
	for n in o.history_retained_counts: count += n
	peak_records = maxi(peak_records, count)
	check(not scene.stopped and not o.failed and o.session_mode == "play" and count <= 96 * o.history_window, "bounded live play tick %d" % o.ready_ticks)
	if o.ready_ticks == 256: warm_capacity = o.history_retained_capacity.duplicate()
	if o.ready_ticks > 256:
		check(o.history_retained_capacity == warm_capacity, "capacity stable after warm-up at %d" % o.ready_ticks)

func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--ready-ticks="):
			target_ticks = maxi(600, int(arg.trim_prefix("--ready-ticks=")))
	directory = "user://playable-wave-smoke-play-%d" % OS.get_process_id()
	root.content_scale_size = root.size
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(directory))
	if not output.is_empty(): DirAccess.make_dir_recursive_absolute(output)
	var first := directory.path_join("first.WORLD")
	var second := directory.path_join("second.WORLD")
	var model := directory.path_join("cosmetic.lmp")
	var hud := directory.path_join("HUD.LMP")
	var world := WorldFixture.world(SandboxCases.new().flat_world())
	write(first, world)
	world.encode_u32(0xac, 40)
	write(second, world)
	write(model, Archives.lmp(["shape.vif", "motion.anm"], [SkinFixture.model(), SkinFixture.animation()]))
	write(hud, Archives.lmp(["bar_red10.tex", "hud_statusbar00.tex"], [ArtFixture.fixture(100, 8, 9), ArtFixture.fixture(160, 46, 17)]))
	scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	check(scene.controller != null and not scene.component_factory.is_valid() and scene.components.size() == 3, "real native shell and all production components")
	for id in scene.COMPONENTS:
		check(scene.components[id].scene_file_path == scene.COMPONENTS[id].path and not scene.actions[id].disabled, "production resource " + id)
	scene.components.library.history_path = directory.path_join("history.json")
	scene.components.library._entries.clear()
	check(scene.paused and not scene.actions.play.disabled and scene.raw_observation.session_mode == "diagnostic", "entry offers explicit play without starting authority")
	await capture("01-entry")
	scene.actions.library.pressed.emit()
	scene.components.library._file_dialog.file_selected.emit(first)
	await capture("02-library")
	press(scene.components.library, "Load / retry")
	var vertex: Vector3 = scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0]
	choose_level(second)
	check(scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX][0] != vertex, "second native WORLD mesh")
	choose_level(first)
	var ground = scene.terrain_root
	scene.actions.play.pressed.emit()
	check(scene.play_mode and scene.mode_known and not scene.paused and scene.raw_observation.session_mode == "play" and scene.terrain_root == ground, "visible start action selects native play preserving WORLD owner")
	check(scene.save_methods == ["save_file"] and scene.save_format.get_item_text(0) == "Continuation (current state)" and not scene.actions.save.disabled, "advertised continuation is the only play save choice")
	check(scene.debug_toggle.disabled and not scene.actions.diagnostic.visible, "unsupported diagnostic controls disabled in play")
	for i in range(4): step_play()
	check(scene.raw_observation.ready_ticks == 3 and scene.raw_observation.position.x > 0, "play moves in selected WORLD")
	await capture("03-play-loaded")
	choose_pair(model)
	scene.components.animation.play_button.pressed.emit()
	await capture("04-animation-picker")
	press(scene.components.animation, "Hide picker (keep preview)")
	scene.resume_session()
	var initial := corners()
	for i in range(4): step_play()
	check(scene.components.animation.sample_tick == 4 and corners() != initial, "real cosmetic animation advances with play ready ticks")
	await capture("05-animated-play")
	scene.actions.art.pressed.emit()
	var before: Dictionary = scene.controller.get_observation()
	var art = scene.components.art
	art.file_dialog.file_selected.emit(hud)
	choose_art("bar_red")
	choose_art("statusbar")
	check(scene.controller.get_observation() == before and art.loaded.bar_red.image.get_data().slice(0, 4) == ArtFixture.expected_pixel(0, 0, 9), "native art pixels without simulation mutation")
	await capture("06-art-picker")
	art.picker.hide()
	scene.scene_scroll.ensure_control_visible(art)
	await capture("07-art-panel")
	await extra_panels()
	scene.scene_scroll.scroll_vertical = 0
	scene.resume_session()
	started_usec = Time.get_ticks_usec()
	while scene.raw_observation.ready_ticks < target_ticks and not scene.stopped:
		step_play()
		if scene.raw_observation.ready_ticks == 600:
			await capture("08-past-512")
			scene.resume_session()
		if scene.raw_observation.ready_ticks % 128 == 0:
			await process_frame
			# Scripted focus changes never turn the deterministic soak into a stall.
			scene.resume_session()
		if scene.raw_observation.ready_ticks % 4000 == 0:
			print("PLAYABLE_SOAK_PROGRESS ticks=", scene.raw_observation.ready_ticks, " peak_records=", peak_records)
	check(scene.raw_observation.ready_ticks == target_ticks and not scene.stopped and scene.raw_observation.history_retired_through_ready >= 512 and not scene.raw_observation.history_complete, "long play survives windows without reset and declares missing history")
	check(scene.terrain_root == ground and scene.components.animation.active and art.loaded.size() == expected_art_count, "WORLD, preview and art owners survive long play")
	await capture("09-long-play")
	scene.handle_input_event(SandboxCases.new().key(KEY_D))
	scene.handle_input_event(SandboxCases.new().key(KEY_H))
	scene._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
	check(scene.paused and scene.held_commands == 0 and not scene.pending_damage, "focus release pauses long play")
	before = scene.controller.get_observation()
	var pose := corners()
	for i in range(3): scene.advance_step()
	check(scene.controller.get_observation() == before and corners() == pose, "paused native state and cosmetic freeze")
	await capture("10-paused")
	await continuation_cases()
	write(directory.path_join("bad.WORLD"), PackedByteArray([1, 2, 3]))
	write(directory.path_join("water.WORLD"), WorldFixture.world(SandboxCases.new().flat_world(), 0x80))
	write(directory.path_join("moving.WORLD"), WorldFixture.world(SandboxCases.new().flat_world(), 0x20))
	await refusal_cases(first, directory.path_join("bad.WORLD"), directory.path_join("water.WORLD"), directory.path_join("moving.WORLD"), model, hud)
	check(scene.play_mode and scene.raw_observation.session_mode == "play", "level replacement keeps explicit play mode")
	scene.actions.play.pressed.emit()
	scene.reset_session()
	check(scene.paused and scene.play_mode and scene.raw_observation.ready_ticks == 0 and scene.raw_observation.terrain_mode == "static_world", "explicit reset preserves selected WORLD and play policy")
	var owner = scene.controller
	scene.return_to_selection()
	scene.resume_session()
	check(scene.controller == owner and not scene.paused, "return and re-enter preserve live owner")
	scene.set_paused(true)
	await capture("15-reset-reentered")
	var summary := {"target_ticks": target_ticks, "peak_records": peak_records, "warm_capacity": Array(warm_capacity), "checks": checks, "failures": failures, "renderer": DisplayServer.get_name(), "elapsed_seconds": (Time.get_ticks_usec() - started_usec) / 1000000.0}
	if not output.is_empty():
		var evidence := FileAccess.open(output.path_join("acceptance.json"), FileAccess.WRITE)
		evidence.store_string(JSON.stringify(summary, "\t") + "\n")
	print("PLAYABLE_PLAY_RESULT ", JSON.stringify(summary))
	owner = null
	ground = null
	art = null
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	quit(0 if failures == 0 else 1)

func continuation_cases() -> void:
	var path := directory.path_join("combined.ocplay")
	scene.actions.save.pressed.emit()
	await capture("11-save-dialog")
	scene.save_dialog.file_selected.emit(path)
	scene.save_dialog.hide()
	check(FileAccess.file_exists(path), "production Save writes advertised continuation")
	var file := FileAccess.open(path, FileAccess.READ)
	var bytes := file.get_buffer(file.get_length())
	file = null
	check(bytes.slice(0, 8) == PackedByteArray([79, 67, 80, 76, 65, 89, 0, 0]) and bytes.size() < 32 * 1024 * 1024, "independent bounded OCPLAY envelope")
	print("PLAYABLE_CONTINUATION bytes=", bytes.size(), " ticks=", scene.raw_observation.ready_ticks)
	var saved: Dictionary = play_state(scene.controller.get_observation())
	var future: Array[Dictionary] = []
	# Advance the original owner first: this is an uninterrupted baseline,
	# not two copies loaded from the same newly serialized representation.
	scene.resume_session()
	for i in range(257):
		scene.held_commands = 1 if (i / 32) % 2 == 0 else 2
		scene.pending_damage = i % 3 == 0
		scene.advance_step()
		future.append(play_state(scene.controller.get_observation()))
	scene.set_paused(true)
	check(future.back().ready_ticks == saved.ready_ticks + 257 and future.back().position_raw != saved.position_raw, "uninterrupted future crosses two retention windows")
	scene.actions.load.pressed.emit()
	scene.load_dialog.file_selected.emit(path)
	scene.load_dialog.hide()
	check(scene.paused and scene.play_mode and scene.mode_known and same_state(play_state(scene.controller.get_observation()), saved), "continuation restores every authoritative observation")
	check(scene.raw_observation.terrain_name == "Restored collision ground", "continuation restores collision state without claiming original mesh association")
	check(not scene.components.animation.active and scene.actor_visual.visible and scene.components.art.loaded.size() == expected_art_count, "load clears cosmetic preview and preserves read-only art")
	check(scene.save_methods == ["save_file"] and not scene.actions.save.disabled, "load retains continuation save dispatch")
	await capture("12-loaded-continuation")
	scene.resume_session()
	for i in range(257):
		scene.held_commands = 1 if (i / 32) % 2 == 0 else 2
		scene.pending_damage = i % 3 == 0
		scene.advance_step()
		check(same_state(play_state(scene.controller.get_observation()), future[i]), "identical uninterrupted future at offset %d" % i)
	check(not scene.stopped and scene.raw_observation.ready_ticks > target_ticks, "loaded play continues beyond soak endpoint")
	await capture("13-continued")
	scene.set_paused(true)
	var before: Dictionary = scene.controller.get_observation()
	bytes[8] = 255
	write(directory.path_join("unknown.ocplay"), bytes)
	check(not scene.load_session_file(directory.path_join("unknown.ocplay")) and scene.controller.get_observation() == before, "unknown continuation version refuses atomically")
	var alias_path := directory.path_join("bridge-alias.ocplay")
	check(scene.controller.save_snapshot_file(alias_path).get("ok", false) and scene.controller.get_observation() == before, "bridge save alias preserves authority")
	file = FileAccess.open(alias_path, FileAccess.READ)
	check(file != null and file.get_buffer(8) == bytes.slice(0, 8), "bridge alias writes explicit OCPLAY magic, never a legacy complete snapshot")
	file = null
	check(not scene.controller.set_recovery_test(true).get("ok", true) and scene.controller.get_observation() == before, "unsupported play diagnostic refuses atomically")
	await capture("14-continuation-refused")

func extra_panels() -> void:
	pass
