extends SceneTree

const Present = preload("res://real_data/loading_present.gd")
const Host = preload("res://scenes/real_data_play.gd")
signal synthetic_post_draw
var checks := 0
var failures := 0

func expect(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	# A stale signal, and subsequent process frames without drawing, must
	# never turn into a successful present or an unbounded wait.
	synthetic_post_draw.emit()
	var start := Time.get_ticks_msec()
	expect(not await Present.wait(self, synthetic_post_draw, 0.05), "missing draw refuses")
	expect(Time.get_ticks_msec() - start < 2000, "missing draw is bounded")
	expect(synthetic_post_draw.get_connections().is_empty(), "timeout disconnects observer")
	create_timer(0.02).timeout.connect(func(): synthetic_post_draw.emit())
	expect(await Present.wait(self, synthetic_post_draw, 1.0), "new draw accepted")
	expect(synthetic_post_draw.get_connections().is_empty(), "success disconnects observer")
	var host := Host.new()
	host.controller = ClassDB.instantiate("ChampionsSessionController")
	for attached in [false, true]:
		host.last_level_script_attached = attached
		var state: Dictionary = host._loading_prerequisite_state()
		expect(not state.ok and state.script_attached == attached, "attachment observed without readiness promotion")
		for field in ["deferred_complete", "equipment_pending", "peers_ready", "stores_complete"]:
			expect(not state.has(field) and field in state.unavailable, "unknown prerequisite remains unknown: " + field)
	host.free()
	if DisplayServer.get_name() != "headless":
		# Reproduce a live SceneTree with no renderer post-draw events.
		RenderingServer.render_loop_enabled = false
		expect(not await Present.wait(self, RenderingServer.frame_post_draw, 0.05),
			"disabled rendering refuses instead of waiting for a signal forever")
		RenderingServer.render_loop_enabled = true
		# force_draw performs a real render, as in the journey capture helper.
		create_timer(0.02).timeout.connect(func(): RenderingServer.force_draw())
		expect(await Present.wait(self, RenderingServer.frame_post_draw), "actual renderer draw observed")
	print("LOADING_PRESENT_CASES %d/%d" % [checks - failures, checks])
	quit(1 if failures else 0)
