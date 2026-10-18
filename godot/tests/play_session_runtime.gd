extends SceneTree
var checks := 0
var failures := 0
var scene
func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error(why)
func _initialize() -> void:
	call_deferred("run")
func values(o: Dictionary) -> Array:
	return [o.position_raw, o.hp_raw, o.frame, o.submissions, o.ready_ticks, o.damage_events, o.paused, o.stopped, o.history_retired_through_ready, o.history_retained_counts, o.history_retired_counts]
func snap_ok() -> bool:
	var p: Dictionary = scene.controller.get_presentation()
	return p.previous_position == p.current_position and p.alpha == 0
func capture(label: String) -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--play-captures="):
			await process_frame
			await RenderingServer.frame_post_draw
			check(root.get_texture().get_image().save_png(arg.trim_prefix("--play-captures=").path_join(label + ".png")) == OK, "capture " + label)
func key(code: Key, down: bool) -> void:
	var e := InputEventKey.new()
	e.physical_keycode = code
	e.pressed = down
	scene.handle_input_event(e)
func run() -> void:
	scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	check(scene.raw_observation.session_mode == "diagnostic", "default remains diagnostic")
	var caps: Dictionary = scene.controller.get_play_capabilities()
	check(caps.available and caps.history_policy == "windowed" and "play-continuation-v1" in caps.save_formats, "play capability")
	var old_visual = scene.actor_visual
	var old_terrain = scene.terrain_root
	scene._observe(scene.controller.start_play_session())
	check(scene.raw_observation.session_mode == "play" and scene.actor_visual == old_visual and scene.terrain_root == old_terrain and snap_ok(), "atomic start preserves visual owners and snaps")
	for i in range(601):
		# Bounded back/forth movement stays on the authored practice ground.
		scene.held_commands = 1 if (i / 32) % 2 == 0 else 2
		scene.advance_step()
	check(scene.raw_observation.ready_ticks == 600 and not scene.stopped, "actual extension beyond 512")
	check(not scene.raw_observation.history_complete and scene.raw_observation.history_retired_through_ready == 512, "explicit unavailable prefix")
	await capture("play-running-600")
	key(KEY_D, true)
	for i in range(8): scene.advance_step()
	key(KEY_D, false)
	key(KEY_H, true)
	scene.advance_step()
	key(KEY_H, false)
	scene.advance_step()
	check(scene.raw_observation.hp_raw == 0x42c40000, "health rising edge")
	key(KEY_ESCAPE, true)
	check(scene.paused and snap_ok(), "pause interaction")
	var paused := values(scene.raw_observation)
	scene._observe(scene.controller.advance_time(2000000, 31))
	check(values(scene.raw_observation) == paused, "paused time does not submit")
	await capture("play-paused")
	var path := "user://play_session_runtime.ocplay"
	check(scene.save_session_file(path), "continuation save via scene handler")
	var before := values(scene.raw_observation)
	var candidate: Dictionary = scene.controller.prepare_save_file(path)
	check(candidate.get("ok", false) and values(scene.controller.get_observation()) == before, "load preparation atomic")
	if candidate.get("ok", false): scene._observe(scene.controller.commit_terrain_file(candidate.token))
	check(values(scene.raw_observation) == before and snap_ok(), "load committed unchanged state")
	check(scene.load_session_file(path) and values(scene.raw_observation) == before and snap_ok(), "scene load publishes continuation")
	var clone = ClassDB.instantiate("ChampionsSessionController")
	candidate = clone.prepare_save_file(path)
	check(candidate.get("ok", false), "clone load prepares")
	if candidate.get("ok", false): clone.commit_terrain_file(candidate.token)
	clone.set_paused(false)
	scene.set_paused(false)
	for command in [1, 17, 0, 2, 16, 0]:
		var a: Dictionary = scene.controller.advance(command)
		var b: Dictionary = clone.advance(command)
		check(values(a) == values(b), "same future after load")
		scene._observe(a)
	await capture("play-loaded-moving")
	var file := FileAccess.open(path, FileAccess.READ)
	var bad := file.get_buffer(file.get_length())
	file = null
	bad[8] = 255
	file = FileAccess.open("user://play_session_runtime_bad.ocplay", FileAccess.WRITE)
	file.store_buffer(bad)
	file = null
	before = values(scene.controller.get_observation())
	check(not scene.load_session_file("user://play_session_runtime_bad.ocplay") and values(scene.controller.get_observation()) == before and not scene.status_label.text.is_empty(), "unknown save atomic refusal and visible error")
	await capture("play-load-error")
	check(not scene.controller.set_recovery_test(true).get("ok", true) and values(scene.controller.get_observation()) == before, "unsupported diagnostics atomic refusal")
	var fixture = load("res://tests/session_sandbox_cases.gd").new()
	file = FileAccess.open("user://play_session_runtime.WORLD", FileAccess.WRITE)
	file.store_buffer(fixture.flat_world())
	file = null
	fixture = null
	check(scene.load_terrain_file("user://play_session_runtime.WORLD"), "terrain replacement")
	check(scene.raw_observation.session_mode == "play", "terrain retains selected play mode")
	var terrain_name: String = scene.raw_observation.terrain_name
	scene.reset_session()
	check(scene.raw_observation.session_mode == "play" and scene.raw_observation.ready_ticks == 0 and scene.raw_observation.terrain_name == terrain_name and snap_ok(), "explicit reset retains mode and terrain")
	await capture("play-reset-selected-terrain")
	clone = null
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	print("PLAY_SESSION_RUNTIME: %d checks, %d failures" % [checks, failures])
	quit(0 if failures == 0 else 1)
