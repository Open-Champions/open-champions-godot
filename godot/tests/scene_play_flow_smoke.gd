extends SceneTree
var scene
var checks := 0
var failures := 0
func _initialize() -> void:
	call_deferred("run")
func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(message)
func key(code: int, pressed := true) -> InputEventKey:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = pressed
	return event
func run() -> void:
	scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	check(scene.controller != null and scene.paused and scene.raw_observation.ready_ticks == 0, "entry is paused before any native tick")
	check(scene.actions.play.disabled and "Install" in scene.capability_label.text, "absent play runtime has actionable disabled action")
	check(root.gui_get_focus_owner() == scene.actions.terrain, "entry has keyboard focus")
	Input.parse_input_event(key(KEY_TAB))
	await process_frame
	check(root.gui_get_focus_owner() != null and root.gui_get_focus_owner() != scene.actions.terrain, "Tab navigates menu")
	Input.parse_input_event(key(KEY_TAB, false))
	scene.actions.diagnostic.grab_focus()
	Input.parse_input_event(key(KEY_ENTER))
	Input.parse_input_event(key(KEY_ENTER, false))
	await process_frame
	check(scene.session_started and not scene.paused, "keyboard activates diagnostic start")
	scene.handle_input_event(key(KEY_D))
	scene.handle_input_event(key(KEY_H))
	scene.advance_step()
	scene.handle_input_event(key(KEY_ESCAPE))
	check(scene.paused and scene.held_commands == 0 and not scene.pending_damage, "pause releases direction and damage edge")
	var before: Dictionary = scene.raw_observation.duplicate(true)
	for i in range(3): scene.advance_step()
	check(scene.raw_observation == before, "pause holds native authority")
	scene.resume_session()
	scene.advance_step()
	check(scene.raw_observation.hp == 100 and scene.raw_observation.position.x == 0, "resume drains delayed input")
	var controller = scene.controller
	var ticks: int = scene.raw_observation.ready_ticks
	scene.return_to_selection()
	scene.resume_session()
	check(scene.controller == controller and scene.raw_observation.ready_ticks == ticks, "menu never recreates controller")
	scene.handle_input_event(key(KEY_D))
	scene._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
	check(scene.paused and scene.held_commands == 0, "focus loss pauses and releases")
	scene.open_dialog(scene.load_dialog)
	check(scene.load_dialog.visible and scene.paused, "load dialog is modal paused")
	scene.load_dialog.get_cancel_button().pressed.emit()
	scene.load_dialog.hide()
	scene._dialog_cancelled()
	check(scene.paused and "cancelled" in scene.status_label.text, "cancel retains pause and explains resume")
	scene.reset_session()
	check(scene.paused and scene.raw_observation.ready_ticks == 0, "reset snaps fresh state and stays in menu")
	controller = null
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	print("SCENE_PLAY_FLOW_PASS checks=%d failures=%d" % [checks, failures])
	quit(0 if failures == 0 else 1)
