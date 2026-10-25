extends SceneTree
var checks := 0
var failures := 0
var scene
func check(value: bool, why: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(why)
func state(o: Dictionary) -> Array:
	return [o.position_raw, o.hp_raw, o.mana_raw, o.pending_health_raw, o.pending_mana_raw, o.facing_raw, o.angular_velocity_raw, o.frame, o.ready_ticks, o.paused, o.stopped]
func _initialize() -> void:
	call_deferred("run")
func snap_ok() -> bool:
	var p: Dictionary = scene.controller.get_presentation()
	return p.previous_facing == p.current_facing and p.previous_position == p.current_position and p.alpha == 0
func capture(label: String) -> void:
	if "--facing-visual" in OS.get_cmdline_user_args():
		await process_frame
		await RenderingServer.frame_post_draw
		var directory := ProjectSettings.globalize_path("user://")
		for argument in OS.get_cmdline_user_args():
			if argument.begins_with("--facing-captures="):
				directory = argument.trim_prefix("--facing-captures=")
		check(get_root().get_texture().get_image().save_png(directory.path_join(label + ".png")) == OK, "rendered capture saved")
func run() -> void:
	scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	check(not scene.raw_observation.facing_enabled, "facing default disabled")
	var found := false
	for button in scene.find_children("*", "Button", true, false):
		if button.text == "Facing test (opt in)":
			button.pressed.emit()
			found = true
	check(found and scene.raw_observation.facing_enabled, "labeled facing opt in")
	check(scene.raw_observation.turn_rate_raw == 0x3f800000 and snap_ok(), "explicit demonstration setup and interpolation reset")
	await capture("facing-default")
	# Existing synthetic optional-model route makes yaw visible; no new art.
	var model = load("res://tests/model_fixture.gd")
	var archive = load("res://tests/texture_fixture.gd")
	var model_file := FileAccess.open("user://session_facing_model.LMP", FileAccess.WRITE)
	var names: Array[String] = ["facing.vif"]
	var payloads: Array[PackedByteArray] = [model.model()]
	model_file.store_buffer(archive.lmp(names, payloads))
	model_file = null
	var selected: Dictionary = scene.select_asset_archive("user://session_facing_model.LMP")
	check(selected.get("ok", false) and scene.apply_asset_model(0), "existing optional synthetic model route")
	# Exercise the existing shortest-path presentation across 0/65535.
	scene.reset_session()
	scene.controller.advance_time(16667, 8)
	scene.controller.advance_time(16667, 8)
	scene._observe(scene.controller.advance_time(8332, 0))
	check(scene.raw_observation.facing_raw > 0xf000 and absf(wrapf(scene.actor.rotation.y, -PI, PI)) < 0.2, "near-wrap native interpolation takes short path")
	await capture("facing-interpolated-wrap")
	scene.reset_session()
	scene.held_commands = 4
	scene.advance_step()
	scene.release_controls()
	scene.advance_step()
	check(scene.raw_observation.facing_raw == 0, "focus/dialog release drains delayed direction")
	scene.reset_session()
	scene.held_commands = 8
	scene.advance_step()
	check(scene.raw_observation.facing_raw == 0, "delayed input no turn")
	for k in range(4): scene.advance_step()
	check(scene.raw_observation.facing_raw > 0xc000, "negative Y turns near wrap")
	check(scene.raw_observation.angular_velocity_raw != 0, "native spring retained")
	await capture("facing-wrap")
	var angle: int = scene.raw_observation.facing_raw
	scene.held_commands = 4
	for k in range(5): scene.advance_step()
	check(scene.raw_observation.facing_raw != angle, "opposite direction reversal")
	check("Facing test" in scene.health_label.text, "read-only diagnostic visible")
	await capture("facing-reversal")
	for k in range(30): scene.advance_step()
	check(scene.raw_observation.facing_raw < 0x8000, "reversed direction continues toward positive Y")
	await capture("facing-positive-y")
	scene.set_paused(true)
	var before := state(scene.raw_observation)
	for k in range(3): scene.advance_step()
	check(state(scene.raw_observation) == before and snap_ok(), "pause freezes facing and snaps")
	await capture("facing-paused")
	for direction in [[1, 0, "positive-x"], [4, 0x4000, "positive-y"], [2, 0x8000, "negative-x"], [8, 0xc000, "negative-y"]]:
		scene.reset_session()
		scene.held_commands = direction[0]
		for k in range(40): scene.advance_step()
		var difference: int = (int(scene.raw_observation.facing_raw) - int(direction[1]) + 32768) % 65536 - 32768
		check(absi(difference) < 5000, "exported native cardinal facing " + direction[2])
		await capture("facing-" + direction[2])
	for recovery in [false, true]:
		scene.set_recovery_test(recovery)
		check(scene.raw_observation.facing_enabled and snap_ok(), "recovery toggle retains facing")
		for direct in [false, true]:
			scene.reset_session()
			scene.held_commands = 5
			for k in range(4): scene.advance_step()
			scene.held_commands = 10
			scene.advance_step()
			scene.set_paused(true)
			var path := "user://session_facing_%s.ocnsave" % str(direct)
			var result: Dictionary = scene.controller.save_snapshot_file(path) if direct else scene.controller.save_file(path)
			check(result.get("ok", false), "facing save succeeds")
			var clone = ClassDB.instantiate("ChampionsSessionController")
			var prepared: Dictionary = clone.prepare_save_file(path)
			check(prepared.get("ok", false), "facing load prepares")
			if prepared.get("ok", false):
				clone.commit_terrain_file(prepared.token)
				check(state(clone.get_observation()) == state(scene.raw_observation), "loaded facing state")
				clone.set_paused(false)
				scene.controller.set_paused(false)
				for command in [10, 8, 0, 5, 0]:
					check(state(clone.advance(command)) == state(scene.controller.advance(command)), "native continuation equal")
			check(scene.load_session_file(path) and snap_ok(), "scene load resets facing endpoints")
			await capture("facing-loaded")
			var file := FileAccess.open(path, FileAccess.READ)
			var bad := file.get_buffer(file.get_length())
			file = null
			bad[bad.size() - 1] ^= 1
			file = FileAccess.open("user://session_facing_bad.ocnsave", FileAccess.WRITE)
			file.store_buffer(bad)
			file = null
			before = state(scene.raw_observation)
			check(not scene.load_session_file("user://session_facing_bad.ocnsave") and state(scene.raw_observation) == before, "corrupt save leaves scene intact")
			clone = null
	var fixture = load("res://tests/session_sandbox_cases.gd").new()
	var file := FileAccess.open("user://session_facing_terrain.WORLD", FileAccess.WRITE)
	file.store_buffer(fixture.flat_world())
	file = null
	fixture = null
	check(scene.load_terrain_file("user://session_facing_terrain.WORLD") and scene.raw_observation.facing_enabled and snap_ok(), "terrain retains facing and snaps")
	scene.reset_session()
	check(scene.raw_observation.facing_raw == 0 and scene.raw_observation.angular_velocity_raw == 0 and snap_ok(), "reset setup")
	await capture("facing-reset")
	scene.set_facing_test(false)
	check(not scene.raw_observation.facing_enabled and scene.raw_observation.recovery_enabled, "facing opt out preserves recovery")
	print("SESSION_FACING_SMOKE_PASS checks=%d failures=%d" % [checks, failures])
	scene.queue_free()
	await process_frame
	quit(0 if failures == 0 else 1)
