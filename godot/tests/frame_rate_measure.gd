extends SceneTree
# Counts actual process callbacks and native ready ticks over monotonic time.
# Run <=7s to stay within the existing 512-ready-tick authored scene budget.
var scene
var start := 0
var last := 0
var frames := 0
var ready_start := 0
var submissions_start := 0
var simulation_clock_start := 0
var samples: Array[float] = []
var measuring := false
var requested_cap := 0
func _initialize():
	call_deferred("run")
func run():
	requested_cap = int(OS.get_cmdline_user_args()[0]) if not OS.get_cmdline_user_args().is_empty() else 0
	var settings = root.get_node_or_null("FrameRate")
	if settings != null:
		settings.set_preferences(requested_cap, 0, true, false)
	else:
		Engine.max_fps = requested_cap
		if DisplayServer.get_name() != "headless":
			DisplayServer.window_set_vsync_mode(0)
	scene = load("res://scenes/session_sandbox.tscn").instantiate()
	root.add_child(scene)
	await create_timer(0.5).timeout
	# Hold an authored command throughout both baseline and changed workload.
	scene.held_commands = 1
	ready_start = scene.raw_observation.ready_ticks
	submissions_start = scene.raw_observation.submissions
	simulation_clock_start = scene._last_clock_usec if "FrameRate" in root.get_children().map(func(node): return node.name) else 0
	start = Time.get_ticks_usec()
	last = start
	measuring = true
func _process(_delta):
	if not measuring:
		return false
	var now := Time.get_ticks_usec()
	samples.append((now - last) / 1000.0)
	last = now
	frames += 1
	var elapsed := (now - start) / 1e6
	if elapsed >= 4.0:
		measuring = false
		samples.sort()
		var ticks: int = scene.raw_observation.ready_ticks - ready_start
		# Pair counters with the monotonic timestamp of the completed scene update.
		# SceneTree callbacks precede node processing; mixing their timestamps can
		# miscount endpoints by a whole render interval under catch-up/load.
		var simulation_elapsed: float = (scene._last_clock_usec - simulation_clock_start) / 1e6 if simulation_clock_start > 0 else elapsed
		print("FRAME_MEASURE cap=%d display=%s renderer=%s seconds=%.6f frames=%d fps=%.3f mean_ms=%.3f p95_ms=%.3f ready_ticks=%d simulation_seconds=%.6f ticks_per_s=%.3f submissions=%d stopped=%s" % [requested_cap, DisplayServer.get_name(), RenderingServer.get_current_rendering_method(), elapsed, frames, frames / elapsed, elapsed * 1000 / frames, samples[int(samples.size() * 0.95)], ticks, simulation_elapsed, ticks / simulation_elapsed, scene.raw_observation.submissions - submissions_start, scene.raw_observation.stopped])
		scene.queue_free()
		finish.call_deferred()
	return false
func finish():
	await process_frame
	await process_frame
	quit()
