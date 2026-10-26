extends SceneTree

var checks := 0
var failures := 0

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

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	scene.component_factory = func(_id): return null
	root.add_child(scene)
	await process_frame
	if scene.controller == null:
		check(false, "native controller loaded (run the editor import first)")
		scene.queue_free()
		await process_frame
		quit(1)
		return
	scene.session_started = true # Isolated host input test; no campaign record is synthesized.
	scene.resume_session()
	scene.raw_observation["fight_player_ready"] = true
	for binding in [[KEY_J, 16], [KEY_K, 32], [KEY_U, 64], [KEY_L, 128], [KEY_1, 256], [KEY_2, 512]]:
		check(scene._fight_key(key(binding[0])), "campaign key accepted")
		check(scene.held_commands == binding[1], "campaign command mapped")
		var repeat := key(binding[0])
		repeat.echo = true
		check(not scene._fight_key(repeat), "OS key repeat cannot restart combo")
		check(scene._fight_key(key(binding[0], false)) and scene.held_commands == 0, "release clears command")
	scene._fight_key(key(KEY_K))
	scene._fight_key(key(KEY_U))
	check(scene.held_commands == 96, "guard and swap coexist")
	scene._fight_key(key(KEY_K, false))
	check(scene.held_commands == 64, "releasing guard retains the independent swap key")
	scene.set_paused(true)
	check(scene.held_commands == 0, "pause clears all combat commands")
	scene.raw_observation["fight_player_ready"] = true
	check(not scene._fight_key(key(KEY_L)), "paused heavy is not queued")
	scene.resume_session()
	scene.raw_observation["fight_player_ready"] = true
	scene.terrain_dialog.show()
	check(not scene._fight_key(key(KEY_K)), "open picker gates combat presses")
	scene.terrain_dialog.hide()
	scene._fight_key(key(KEY_K))
	scene.release_controls()
	check(scene.held_commands == 0, "focus/menu release clears all combat commands")
	scene.raw_observation["fight_player_ready"] = false
	check(not scene._fight_key(key(KEY_K)), "campaign keys require a real player")
	check(not scene.controller.advance(32).get("ok", false), "ordinary controller retains its 0..31 command domain")
	check(not scene.controller.advance_time(0, 128).get("ok", false), "clock controller also refuses campaign-only bits without a Player")
	scene.queue_free()
	await process_frame
	print("CAMPAIGN_MOVES checks=", checks, " failures=", failures)
	quit(0 if failures == 0 else 1)
