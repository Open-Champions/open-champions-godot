extends SceneTree

# Opt-in smoke: the level script's text HUD on real levels of the user's own
# game folder (docs/STRINGS.md). Prints counts and level indices only, never a
# key or a text. Captures stay in --capture-dir (outside the project).
#   --game-folder=DIR --cache=DIR [--capture-dir=DIR] [--from=I] [--to=J]
#   [--ticks=N]   ready ticks to wait on each level for a text (default 600)
var args := {}
var scene: Control
var counts := {}
var failures := 0

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
		elif argument.begins_with("--"):
			args[argument.substr(2)] = true
	run.call_deferred()

func add(key: String, n := 1) -> void:
	counts[key] = int(counts.get(key, 0)) + n

func check(ok: bool, what: String) -> bool:
	if ok: add("checks.passed")
	else:
		failures += 1
		add("checks.failed")
		printerr("FAILED: " + what)
	return ok

func frames(n: int) -> void:
	for i in range(n): await process_frame

func capture(label: String) -> void:
	await frames(4)
	if not args.has("capture-dir") or DisplayServer.get_name() == "headless": return
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	if image != null and image.save_png(str(args["capture-dir"]).path_join(label + ".png")) == OK: add("captures")

func run() -> void:
	var packed: PackedScene = load("res://scenes/real_data_play.tscn")
	scene = packed.instantiate()
	root.add_child(scene)
	await frames(2)
	if not check(scene.controller != null, "the session controller loads"): return finish()
	scene.real.configure(scene, str(args.get("cache", "user://real-data-cache")))
	scene.level_script.set_pressed_no_signal(true)
	if not check(scene.open_game_folder(str(args.get("game-folder", ""))), "the game folder opens"): return finish()
	check(scene.text_sources.get("ok", false), "GTEXT.LMP tables install")
	add("text.tables", int(scene.text_sources.get("tables", 0)))
	check(scene.text_sources.get("help_chime", false), "the help chime resource is found")
	check(scene.text_hud.fonts.has("gamefontmed.fnt"), "gamefontmed decodes")
	var ticks := int(args.get("ticks", "600"))
	var from := int(args.get("from", "0"))
	var to := mini(int(args.get("to", str(scene.real.levels.size()))), scene.real.levels.size())
	var seen_message := false
	var seen_help := false
	var seen_bust := false
	for index in range(from, to):
		if seen_message and seen_help and seen_bust: break
		scene.return_to_selection()
		await frames(3)
		if not scene.load_real_level(index):
			add("levels.refused")
			continue
		add("levels.loaded")
		scene.actions.play.pressed.emit()
		if not scene.play_mode:
			add("levels.play_refused")
			continue
		var hud := {}
		for t in range(ticks):
			hud = scene.controller.text_hud()
			if hud.get("help_open", false) or int(hud.get("message_frames", 0)) > 0 or int(hud.get("bust_shown", 0)) > 0: break
			scene.advance_step()
		hud = scene.controller.text_hud()
		var message := int(hud.get("message_frames", 0)) > 0 and not seen_message
		var help: bool = hud.get("help_open", false) and not seen_help
		var bust := int(hud.get("bust_shown", 0)) > 0 and int(hud.get("bust_max", 0)) == -1 and not seen_bust
		if not (message or help or bust): continue
		print("text_level_index ", index, " message ", message, " help ", help, " bust ", bust)
		# Let the overlay run its displayed frames, then look at it.
		var drawn_before: Dictionary = scene.text_hud.drawn.duplicate()
		await frames(12)
		if message:
			seen_message = true
			check(int(scene.text_hud.drawn.message) > int(drawn_before.message), "a real setInfo message is drawn")
			await capture("text-message-%d" % index)
		if bust:
			seen_bust = true
			check(int(scene.text_hud.drawn.bust) > int(drawn_before.bust), "a real bust panel is drawn")
			await capture("text-bust-%d" % index)
		if help:
			seen_help = true
			check(int(scene.text_hud.drawn.help) > int(drawn_before.help), "a real help box is drawn")
			var th: Dictionary = scene.controller.text_hud()
			print("help_debug paused_by_help ", th.get("help_paused"), " session_paused ", scene.raw_observation.get("paused"), " delay ", th.get("help_close_delay"), " pulses ", th.get("pulse_starts"))
			check(th.get("help_paused", false), "an open help box pauses the session")
			check(not scene.controller.dialog_use().get("ok", true), "help owns use input while open")
			check(not scene.controller.dialog_press(0x40).get("ok", true), "help owns confirm input while open")
			var before := int(scene.controller.advance(0).get("ready_ticks", -1))
			for t in range(10): scene.advance_step()
			check(scene.raw_observation.get("paused", false), "the session observation is paused")
			check(int(scene.raw_observation.get("ready_ticks", 0)) == before, "no ready tick runs while the box is up")
			await capture("text-help-%d" % index)
			# Hold confirm: the box closes once its 90-frame delay has run out.
			scene.text_hud.confirm_override = 1
			var waited := 0
			# Displayed frames may come faster than 60 Hz; the overlay steps
			# help_task at 60 Hz, so wait by time (90 frames = 1.5 s).
			var deadline := Time.get_ticks_msec() + 6000
			while scene.controller.text_hud().get("help_open", false) and Time.get_ticks_msec() < deadline:
				await process_frame
				waited += 1
			scene.text_hud.confirm_override = -1
			th = scene.controller.text_hud()
			print("help_debug after waited ", waited, " open ", th.get("help_open"), " delay ", th.get("help_close_delay"), " pulses ", th.get("pulse_starts"), " paused_by_help ", th.get("help_paused"))
			add("help.frames_to_close", waited)
			check(not scene.controller.text_hud().get("help_open", false), "confirm closes the help box")
			check(not scene.controller.text_hud().get("help_paused", false), "closing releases the pause")
			check(int(th.get("menu_input_lockout", 0)) == 60, "close records the researched menu input lockout")
			for t in range(5): scene.advance_step()
			check(int(scene.raw_observation.get("ready_ticks", 0)) > before, "ready ticks run again after the confirm")
	# setInfo runs from triggers later in a level; a standing player may not
	# reach one within --ticks, so it is reported, not required.
	add("text.message_seen" if seen_message else "text.message_not_reached")
	check(seen_help or seen_message, "a real level showed a help box or a setInfo message")
	finish()

func finish() -> void:
	if scene != null:
		scene.return_to_selection()
		scene.real.clear_cache()
	print(JSON.stringify({"counts": counts, "failures": failures}, "  ", true))
	quit(0 if failures == 0 else 1)
