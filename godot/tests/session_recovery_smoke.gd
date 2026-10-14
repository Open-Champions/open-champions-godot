extends SceneTree
var failures := 0
var checks := 0
func check(value: bool, why: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(why)
func _initialize() -> void:
	call_deferred("run")
func authoritative(o: Dictionary) -> Array:
	return [o.position_raw, o.hp_raw, o.mana_raw, o.pending_health_raw, o.pending_mana_raw, o.frame, o.submissions, o.ready_ticks, o.damage_events, o.paused, o.stopped, o.failed]
func run() -> void:
	var scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	check(not scene.raw_observation.recovery_enabled, "default recovery disabled")
	var buttons: Array = scene.find_children("*", "Button", true, false)
	var enabled := false
	for button in buttons:
		if button.text == "Recovery test (opt in)":
			button.pressed.emit()
			enabled = true
	check(enabled, "labeled opt-in control exists and dispatches")
	check(scene.raw_observation.recovery_enabled and scene.raw_observation.hp == 50 and scene.raw_observation.mana == 20, "authored below-cap setup")
	check(scene.raw_observation.pending_health == 10 and scene.raw_observation.pending_mana == 6, "explicit pending amounts displayed")
	check("Mana" in scene.health_label.text and "Pending" in scene.health_label.text, "read-only resource diagnostic visible")
	scene.advance_step()
	check(scene.raw_observation.hp == 50 and scene.raw_observation.ready_ticks == 0, "input wait does not recover")
	scene.advance_step()
	check(scene.raw_observation.hp > 51 and scene.raw_observation.mana > 20.5 and scene.raw_observation.pending_health == 9, "both original recovery paths visible")
	scene.set_paused(true)
	var before := authoritative(scene.raw_observation)
	for k in range(4): scene.advance_step()
	check(authoritative(scene.raw_observation) == before, "paused rendering does not recover")
	for direct in [false, true]:
		scene.reset_session()
		scene.held_commands = 17
		scene.advance_step()
		var path := "user://session_recovery_%s.ocnsave" % str(direct)
		var saved: Dictionary = scene.controller.save_snapshot_file(path) if direct else scene.controller.save_file(path)
		check(saved.get("ok", false), "recovery checkpoint saved")
		var clone = ClassDB.instantiate("ChampionsSessionController")
		var prepared: Dictionary = clone.prepare_save_file(path)
		check(prepared.get("ok", false), "recovery checkpoint prepared")
		if prepared.get("ok", false):
			clone.commit_terrain_file(prepared.token)
			for command in [17, 0, 16, 0]:
				check(authoritative(scene.controller.advance(command)) == authoritative(clone.advance(command)), "delayed input continuation matches")
		check(scene.load_session_file(path), "scene publishes saved recovery state")
		var poses: Dictionary = scene.controller.get_presentation()
		check(poses.previous_position == poses.current_position and poses.alpha == 0, "load resets interpolation")
		check(scene.raw_observation.recovery_enabled and scene.raw_observation.pending_health == 10, "load retains policy and pending amount")
		var file := FileAccess.open(path, FileAccess.READ)
		var corrupt := file.get_buffer(file.get_length())
		file = null
		corrupt[8] = 99
		var bad_path := "user://session_recovery_invalid.ocnsave"
		file = FileAccess.open(bad_path, FileAccess.WRITE)
		file.store_buffer(corrupt)
		file = null
		before = authoritative(scene.raw_observation)
		check(not scene.load_session_file(bad_path), "newer invalid save refused")
		check(authoritative(scene.raw_observation) == before, "invalid load preserves current scene")
		if direct:
			corrupt[8] = 4
			corrupt[corrupt.size() - 1] ^= 1
			file = FileAccess.open(bad_path, FileAccess.WRITE)
			file.store_buffer(corrupt)
			file = null
			check(not scene.load_session_file(bad_path), "corrupt v4 journal refused before publication")
			check(authoritative(scene.raw_observation) == before, "corrupt v4 journal preserves current scene")
		clone = null
	var reference: Array = []
	for fps in [30, 60, 144, 240]:
		scene.reset_session()
		for frame in range(1, fps + 1):
			var now: int = frame * 1000000 / fps
			var previous: int = (frame - 1) * 1000000 / fps
			scene.controller.advance_time(now - previous, 0)
		var actual := authoritative(scene.controller.get_observation())
		if reference.is_empty(): reference = actual
		else: check(actual == reference, "render rate preserves native recovery")
	scene.reset_session()
	check(scene.raw_observation.hp == 50 and scene.raw_observation.pending_mana == 6, "reset restores explicit setup")
	var terrain_fixture = load("res://tests/session_sandbox_cases.gd").new()
	var terrain_path := "user://session_recovery_terrain.WORLD"
	var terrain_file := FileAccess.open(terrain_path, FileAccess.WRITE)
	terrain_file.store_buffer(terrain_fixture.flat_world())
	terrain_file = null
	terrain_fixture = null
	check(scene.load_terrain_file(terrain_path), "terrain selection works with recovery enabled")
	check(scene.raw_observation.recovery_enabled and scene.raw_observation.ready_tick_limit == 256 and scene.raw_observation.pending_health == 10, "terrain replacement retains explicit recovery policy/setup")
	scene.advance_step()
	scene.advance_step()
	check(scene.raw_observation.hp > 51, "recovery executes on selected terrain")
	scene.set_recovery_test(false)
	check(not scene.raw_observation.recovery_enabled and scene.raw_observation.hp == 100, "opt out restores legacy health setup")
	print("SESSION_RECOVERY_SMOKE_PASS checks=%d failures=%d" % [checks, failures])
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	quit(0 if failures == 0 else 1)
