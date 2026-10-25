extends SceneTree
var checks := 0
var failures := 0
var scene
func check(value: bool, why: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(why)
func _initialize() -> void:
	call_deferred("run")
func snap_ok() -> bool:
	var p: Dictionary = scene.controller.get_presentation()
	return p.previous_facing == p.current_facing and p.previous_position == p.current_position and p.alpha == 0
func values() -> Array:
	var o: Dictionary = scene.controller.get_observation()
	return [o.position_raw, o.hp_raw, o.frame, o.ready_ticks, o.animation_track, o.animation_time_raw, o.animation_event, o.animation_joints_raw, o.paused]
func capture(label: String) -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--cache-captures="):
			await process_frame
			await RenderingServer.frame_post_draw
			check(get_root().get_texture().get_image().save_png(argument.trim_prefix("--cache-captures=").path_join(label + ".png")) == OK, "capture saved")
func press(text: String) -> void:
	var found := false
	for button in scene.find_children("*", "Button", true, false):
		if button.text == text:
			button.pressed.emit()
			found = true
	check(found, "diagnostic control " + text)
func run() -> void:
	scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	check(not scene.raw_observation.animation_cache_enabled, "disabled default")
	press("Animation-cache diagnostic (opt in)")
	check(scene.raw_observation.animation_cache_enabled and snap_ok(), "explicit policy and snap")
	press("Clip start")
	check(scene.raw_observation.animation_track == 0, "queued command does not decode")
	for k in range(3): scene.advance_step()
	check(scene.raw_observation.animation_track > 0, "ready command selects track")
	var old: Array = scene.raw_observation.animation_joints_raw.duplicate(true)
	for k in range(3): scene.advance_step()
	check(old != scene.raw_observation.animation_joints_raw, "joint observations change")
	check(scene.animation_cache_label.visible and "no model pose" in scene.animation_cache_label.text, "diagnostic label")
	await capture("cache-changing")
	var stable := values()
	for k in range(20):
		scene.controller.get_observation()
		scene._apply_presentation()
	check(stable == values(), "render and getters immutable")
	scene.set_paused(true)
	stable = values()
	for k in range(3): scene.advance_step()
	check(stable == values() and snap_ok(), "pause freezes decoding")
	press("Clip restart")
	for direct in [false, true]:
		var path := "user://control_anim_cache_test.ocns"
		var saved: Dictionary = scene.controller.save_snapshot_file(path) if direct else scene.controller.save_file(path)
		check(saved.get("ok", false), "diagnostic save")
		var prepared: Dictionary = scene.controller.prepare_save_file(path)
		check(prepared.get("ok", false), "diagnostic load preparation")
		if prepared.get("ok", false):
			var committed: Dictionary = scene.controller.commit_terrain_file(prepared.token)
			scene._observe(committed)
			check(committed.get("ok", false) and values() == stable and snap_ok(), "load current paused state and snap")
	await capture("cache-paused-loaded")
	scene.set_paused(false)
	for k in range(2): scene.advance_step()
	check(scene.raw_observation.animation_time_raw < stable[5], "pending restart consumed after load")
	press("Clip stop")
	for k in range(2): scene.advance_step()
	check(scene.raw_observation.animation_track == 0, "stop consumes on ready tick")
	await capture("cache-stopped")
	var file := FileAccess.open("user://control_anim_cache_invalid.ocns", FileAccess.WRITE)
	file.store_buffer(PackedByteArray([1,2,3]))
	file = null
	stable = values()
	check(not scene.controller.prepare_save_file("user://control_anim_cache_invalid.ocns").get("ok", true), "invalid load refuses")
	check(values() == stable, "invalid load retains owner")
	press("Reset")
	check(scene.raw_observation.animation_track == 0 and snap_ok(), "reset policy retained and snap")
	press("Clip start")
	for k in range(3): scene.advance_step()
	check(scene.raw_observation.animation_track > 0, "recovery after invalid load/reset")
	scene.set_facing_test(true)
	check(scene.raw_observation.animation_cache_enabled and scene.raw_observation.facing_enabled and snap_ok(), "facing replacement retains cache")
	scene.set_recovery_test(true)
	check(scene.raw_observation.animation_cache_enabled and scene.raw_observation.recovery_enabled and snap_ok(), "recovery replacement retains cache")
	await capture("cache-combined")
	press("Cache off / restart")
	check(not scene.raw_observation.animation_cache_enabled and snap_ok(), "off resets presentation")
	scene.queue_free()
	await process_frame
	print("control_anim_cache smoke: %d checks, %d failures" % [checks, failures])
	quit(1 if failures else 0)
