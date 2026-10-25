extends SceneTree
# Exported GPU test artifacts from this application's own viewport. This is
# independent of desktop capture, file-picker automation and original assets.
var output := ""
var failures := 0
var checks := 0
func check(value: bool, why: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(why)
func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="):
			output = arg.trim_prefix("--capture-dir=")
	call_deferred("run")
func capture(name: String) -> void:
	await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and not image.is_empty(), "rendered viewport available")
	if image != null:
		check(image.save_png(output.path_join(name)) == OK, "viewport artifact saved")
func authoritative(o: Dictionary) -> Array:
	return [o.position_raw, o.hp_raw, o.mana_raw, o.pending_health_raw, o.pending_mana_raw, o.frame, o.ready_ticks, o.submissions]
func run() -> void:
	if output.is_empty() or DisplayServer.get_name() == "headless":
		push_error("A graphical renderer and explicit --capture-dir are required.")
		quit(1)
		return
	check(DirAccess.make_dir_recursive_absolute(output) == OK, "capture directory")
	var scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await capture("00-disabled.png")
	var enabled := false
	for button in scene.find_children("*", "Button", true, false):
		if button.text == "Recovery test (opt in)":
			button.pressed.emit()
			enabled = true
	check(enabled and scene.raw_observation.hp == 50, "opt-in button starts explicit recovery setup")
	await capture("01-enabled.png")
	for k in range(5): scene.advance_step()
	check(scene.raw_observation.hp > 54 and scene.raw_observation.mana > 22, "visible native recovery")
	await capture("02-recovered.png")
	scene.set_paused(true)
	var before := authoritative(scene.raw_observation)
	scene.advance_step()
	await capture("03-paused.png")
	check(authoritative(scene.raw_observation) == before and scene.paused, "pause keeps recovery fixed")
	scene.reset_session()
	check(scene.raw_observation.hp == 50 and scene.raw_observation.pending_health == 10, "reset restores pending setup")
	await capture("04-reset.png")
	scene.held_commands = 17
	scene.advance_step()
	scene.save_format.select(1)
	var save_path := output.path_join("visual-continuation.ocnsave")
	check(scene.save_session_file(save_path), "snapshot saved through scene")
	scene.held_commands = 0
	for k in range(2): scene.advance_step()
	var uninterrupted := authoritative(scene.raw_observation)
	check(scene.load_session_file(save_path), "saved recovery published through scene")
	check(scene.raw_observation.hp == 50 and scene.raw_observation.pending_health == 10, "delayed saved state displayed")
	await capture("05-loaded-before-ready.png")
	for k in range(2): scene.advance_step()
	check(authoritative(scene.raw_observation) == uninterrupted, "visible exact save/load continuation")
	await capture("06-load-continued.png")
	print("SESSION_RECOVERY_VISUAL_PASS checks=%d failures=%d renderer=%s" % [checks, failures, DisplayServer.get_name()])
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	quit(0 if failures == 0 else 1)
