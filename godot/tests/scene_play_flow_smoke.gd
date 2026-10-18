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
func capture(label: String) -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--scene-play-flow-captures="):
			await process_frame
			await process_frame
			await RenderingServer.frame_post_draw
			check(root.get_texture().get_image().save_png(argument.trim_prefix("--scene-play-flow-captures=").path_join(label + ".png")) == OK, "export capture " + label)
func run() -> void:
	scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	check(scene.controller != null and scene.paused and scene.raw_observation.ready_ticks == 0, "entry is paused before any native tick")
	check(scene.actions.play.disabled and "Install" in scene.capability_label.text, "absent play runtime has actionable disabled action")
	await capture("01-entry-wide")
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
	await capture("02-running")
	scene.handle_input_event(key(KEY_D))
	scene.handle_input_event(key(KEY_H))
	scene.advance_step()
	scene.handle_input_event(key(KEY_ESCAPE))
	check(scene.paused and scene.held_commands == 0 and not scene.pending_damage, "pause releases direction and damage edge")
	await capture("03-paused")
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
	for id in ["library", "animation", "art"]:
		check(scene.actions[id].disabled and "Install component" in scene.actions[id].tooltip_text, "missing " + id + " is honest")
	before = scene.controller.get_observation()
	var ground = scene.terrain_root
	check(not scene.load_session_file("user://scene-play-flow-missing.ocnsave"), "missing save refused")
	check(scene.controller.get_observation() == before and scene.terrain_root == ground, "failed load preserves native and displayed terrain")
	check(not scene.save_session_file("user://scene-play-flow-absent-dir/save.ocnsave"), "write failure surfaced")
	check(scene.controller.get_observation() == before, "save failure preserves state")
	var file := FileAccess.open("user://scene-play-flow-bad.ocnsave", FileAccess.WRITE)
	file.store_buffer(PackedByteArray([1, 2, 3]))
	file = null
	check(not scene.load_session_file("user://scene-play-flow-bad.ocnsave"), "malformed save refused")
	await capture("04-failed-load")
	root.size = Vector2i(800, 600)
	root.content_scale_size = root.size
	await capture("05-entry-narrow")
	scene.actions.archive.grab_focus()
	await process_frame
	check(scene.actions.archive.get_global_rect().end.x <= root.size.x, "narrow menu action fits horizontally")
	scene.resume_session()
	scene._last_clock_usec = 1
	scene.set_paused(true)
	scene.automatic_steps = true
	scene._process(60.0)
	scene.resume_session()
	ticks = scene.raw_observation.ready_ticks
	scene._process(60.0)
	check(scene.raw_observation.ready_ticks == ticks, "resume never catches up paused clock debt")
	scene.automatic_steps = false
	for i in range(514): scene.advance_step()
	check(scene.stopped and scene.raw_observation.ready_ticks == 512 and "limit" in scene.status_label.text, "baseline diagnostic cap remains explicit")
	await capture("06-limit-narrow")
	scene.reset_session()
	check(not scene.stopped and scene.raw_observation.ready_ticks == 0 and scene.paused, "explicit reset recovers stopped diagnostic")
	controller = null
	ground = null
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	await component_cases()
	print("SCENE_PLAY_FLOW_PASS checks=%d failures=%d" % [checks, failures])
	quit(0 if failures == 0 else 1)

func component_cases() -> void:
	scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	scene.component_factory = func(_id): return load("res://tests/scene_play_flow_component_fixture.gd").new()
	root.add_child(scene)
	await process_frame
	check(scene.components.size() == 3, "isolated injectable adapter accepts frozen signatures")
	var animation = scene.components.animation
	var art = scene.components.art
	var library = scene.components.library
	check(animation.configured == [scene.inspector, scene.actor], "animation gets shared inspector and actor only")
	check(art.configured[0] == scene.inspector, "art gets shared inspector")
	scene.start_diagnostic()
	animation.preview_changed.emit(true)
	check(not scene.actor_visual.visible, "accepted cosmetic preview hides static actor")
	var clears: int = animation.clears
	scene.set_paused(true)
	scene.resume_session()
	check(animation.clears == clears and not animation.suspended, "menu suspend/resume does not clear selection")
	scene.open_component("animation")
	check(scene.paused and animation.suspended, "picker pauses and suspends")
	scene.handle_input_event(key(KEY_ESCAPE))
	check(scene.paused, "Escape cannot resume behind component picker")
	animation.hide()
	scene.resume_session()
	check(not scene.paused, "explicit resume after picker close")
	scene.advance_step()
	check(animation.observations[0] == scene.raw_observation and art.observations[0] == scene.raw_observation, "raw native observations reach both consumers")
	scene.set_paused(true)
	var path := "user://scene-play-flow-good.ocnsave"
	check(scene.save_session_file(path), "real native save via shell")
	check(not scene.load_session_file("user://scene-play-flow-missing.ocnsave") and animation.clears == clears, "failed load retains cosmetic selection")
	check(scene.load_session_file(path) and animation.clears == clears + 1 and scene.actor_visual.visible, "successful equal-counter load clears preview and restores static")
	var pose: Dictionary = scene.controller.get_presentation()
	check(pose.alpha == 0 and pose.previous_position == pose.current_position, "native load snaps presentation")
	animation.preview_changed.emit(true)
	scene.reset_session()
	check(animation.clears == clears + 2 and scene.actor_visual.visible, "reset clears preview")
	var fixture = load("res://tests/session_sandbox_cases.gd").new()
	var terrain_path := "user://scene-play-flow-ground.WORLD"
	var file := FileAccess.open(terrain_path, FileAccess.WRITE)
	file.store_buffer(fixture.flat_world())
	file = null
	fixture = null
	library.configured[1].call()
	check(scene.paused, "library release callback pauses")
	check(library.configured[0].call(terrain_path), "library uses real atomic terrain callback")
	check(animation.clears == clears + 3 and not scene.session_started, "successful level load clears cosmetic and returns selection")
	animation.preview_changed.emit(true)
	animation.preview_changed.emit(false)
	check(scene.actor_visual.visible, "component error restores static actor")
	art.art_ready.emit(true)
	check("loaded" in scene.art_state.text and scene.health_label.visible, "art never replaces diagnostic health")
	var lifetime: WeakRef = weakref(scene.controller)
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	check(lifetime.get_ref() == null and not is_instance_valid(animation) and not is_instance_valid(art) and not is_instance_valid(library), "exit releases native and component owners")
	# Invalid optional components refuse without production placeholders.
	scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	scene.component_factory = func(_id): return Control.new()
	root.add_child(scene)
	check(scene.components.is_empty() and "contract mismatch" in scene.actions.library.tooltip_text, "incomplete component rejected")
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
