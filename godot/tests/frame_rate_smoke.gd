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
