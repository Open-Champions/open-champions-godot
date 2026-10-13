extends SceneTree
var failures := 0
func check(value: bool, why: String):
	if not value:
		failures += 1
		push_error(why)
func _initialize():
	call_deferred("run")
func run():
	var scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	var c = scene.controller
	check(c != null, "real bridge loaded")
	c.advance_time(16667, 1)
	c.advance_time(16667, 1)
	var poses: Dictionary = c.get_presentation()
	check(poses.previous_position == Vector3.ZERO and poses.current_position.x > 0.99, "two native ready poses exposed")
	c.advance_time(8333, 1)
	poses = c.get_presentation()
	check(abs(poses.alpha - 0.50002) < 0.001, "fractional accumulator alpha")
	scene._apply_presentation()
	check(scene.actor.position.x > 0.49 and scene.actor.position.x < 0.51, "actor interpolates without feedback")
	check(c.get_observation().position.x > 0.99, "interpolation leaves authoritative position intact")
	c.set_paused(true)
	var before: Dictionary = c.get_observation()
	c.advance_time(2000000, 31)
	check(c.get_observation() == before, "paused clock never advances Session")
	poses = c.get_presentation()
	check(poses.previous_position == poses.current_position and poses.alpha == 0, "pause collapses pair")
	c.set_paused(false)
	c.advance_time(8333, 0)
	check(c.get_observation().submissions == before.submissions, "resume has no catch-up debt")
	c.snap_presentation()
	check(c.get_presentation().alpha == 0, "explicit teleport snap discards blend")
	c.reset_session()
	poses = c.get_presentation()
	check(poses.previous_position == Vector3.ZERO and poses.current_position == Vector3.ZERO and poses.alpha == 0, "reset snaps both poses")
	c.advance_time(1000000, 0)
	check(c.get_observation().submissions == 60 and c.get_observation().ready_ticks == 59, "60 fixed submissions and pipeline startup")
	# Both native-owned save formats must reset interpolation on publication.
	for direct in [false, true]:
		c.reset_session()
		c.advance_time(33334, 1)
		var checkpoint: Dictionary = c.get_observation()
		var path := "user://frame_rate_load_smoke.ocnsave"
		var saved: Dictionary = c.save_snapshot_file(path) if direct else c.save_file(path)
		check(saved.get("ok", false), "native Controller writes checkpoint")
		c.advance_time(58333, 1)
		var moved: Dictionary = c.get_observation()
		check(moved.position.x > checkpoint.position.x and c.get_presentation().alpha > 0.49, "pre-load state has moved and retains blend debt")
		var prepared: Dictionary = c.prepare_save_file(path)
		check(prepared.get("ok", false) and c.get_observation() == moved, "load preparation preserves live authority")
		if prepared.get("ok", false):
			check(c.commit_terrain_file(prepared.token).get("ok", false), "Controller publishes native load")
		poses = c.get_presentation()
		check(poses.previous_position == checkpoint.position and poses.current_position == checkpoint.position and poses.alpha == 0, "load snaps both positions without pre-load smearing")
		check(poses.previous_facing == poses.current_facing and c.get_observation().facing_raw == checkpoint.facing_raw, "load preserves facing and resets its blend")
		c.advance_time(8333, 0)
		check(c.get_observation().submissions == checkpoint.submissions, "load discards pre-load tick debt")
		var live: Dictionary = c.get_observation()
		var blend: Dictionary = c.get_presentation()
		check(not c.prepare_save_file("user://missing-frame-rate-save.ocnsave").get("ok", false) and c.get_observation() == live and c.get_presentation() == blend, "failed load preserves authority and presentation")
		scene._last_clock_usec = 1
		scene.held_commands = 1
		scene.pending_damage = true
		check(scene.load_session_file(path), "scene asks Controller to restore checkpoint")
		check(scene._last_clock_usec == 0 and scene.held_commands == 0 and not scene.pending_damage, "scene discards elapsed and input debt after load")
		check(scene.actor.position == Vector3(checkpoint.position.x, checkpoint.position.z, -checkpoint.position.y), "scene displays restored pose immediately")
		DirAccess.remove_absolute(path)
	# Shortest-angle interpolation must cross the wrap, not rotate a full turn.
	check(abs(lerp_angle(TAU - 0.1, 0.1, 0.5) - TAU) < 0.001, "facing shortest wrap")
	var settings = root.get_node("FrameRate")
	var old := [settings.cap, settings.vsync, settings.readout]
	for cap in [0, 30, 60, 120, 144, 240, -1]:
		settings.set_preferences(cap, 0, true, false)
		check(Engine.max_fps == cap if cap >= 0 else Engine.max_fps >= 0, "cap applied")
	check(settings.meter.visible, "FPS toggle visible")
	check(not settings.settings_window.visible, "settings closed by default")
	# Use an isolated user-config file, leaving the player's settings intact.
	var old_path: String = settings.config_path
	settings.config_path = "user://frame_rate_smoke.cfg"
	settings.set_preferences(144, 1, true)
	var reloaded = load("res://ui/frame_rate.gd").new()
	reloaded.config_path = settings.config_path
	reloaded._load_preferences()
	check(reloaded.cap == 144 and reloaded.vsync == 1 and reloaded.readout, "preferences persist and reload from user config")
	reloaded.free()
	DirAccess.remove_absolute(settings.config_path)
	settings.config_path = old_path
	settings.set_preferences(old[0], old[1], old[2], false)
	scene.queue_free()
	await process_frame
	await process_frame
	print("Frame rate smoke: %d failures" % failures)
	quit(0 if failures == 0 else 1)
