extends SceneTree

# Opt-in: the HUD extras on the user's own game folder. Never part of CI.
# Prints counts and element names only; level identity is an index.
#   --game-folder=DIR --cache=DIR [--level=N] [--ticks=N]
# The experience thresholds below are authored synthetic numbers, not game data:
# this checks the real bar_green art and the real HUD font, not the table.
var args := {}
var failures := 0

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	run.call_deferred()

func check(ok: bool, what: String) -> bool:
	if not ok:
		failures += 1
		printerr("FAILED: " + what)
	return ok

func run() -> void:
	if not args.has("game-folder") or not args.has("cache"):
		printerr("--game-folder and --cache are required")
		quit(2)
		return
	root.content_scale_size = root.size
	var scene: Control = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = str(args.cache).path_join("level-library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = str(args.cache)
	check(scene.open_game_folder(str(args["game-folder"])), "game folder opens")
	var counts := {"green_loaded": scene.hud_extras_counts.get("loaded", 0), "green_refused": scene.hud_extras_counts.get("refused", 0)}
	check(int(scene.hud_extras_counts.get("loaded", 0)) == 11 and int(scene.hud_extras_counts.get("refused", 0)) == 0, "all eleven bar_green frames decode (100x6, 3 rows)")
	check(scene.hud_font_loaded and scene.play_hud.extras != null, "HUD font and native extras service present")
	var index := int(args.get("level", "0"))
	check(scene.real.load_level(index).get("ok", false), "level %d loads" % index)
	scene.actions.play.pressed.emit()
	check(scene.play_mode, "play starts")
	for t in range(int(args.get("ticks", "60"))): scene.advance_step()
	var hud: Control = scene.play_hud
	var base: Dictionary = scene.raw_observation.duplicate(true)
	# Experience bar over the real art: half fill, every animation frame.
	var frames := {}
	var xp_ok := true
	for tick in range(0, 44, 4):
		var value := base.duplicate()
		value.merge({"ready_ticks": tick, "acknowledged_level": 3, "experience": 1500, "experience_base": 1000, "experience_next": 2000}, true)
		hud.observe_play(value, true)
		var found := false
		for record in hud.records:
			if hud._element(record) == "experience":
				found = true
				frames[record.resource] = true
				xp_ok = xp_ok and int(record.clip.right) == 27 + 49 and hud.textures.has(record.resource)
		xp_ok = xp_ok and found and "experience" in hud.drawn
	check(xp_ok and frames.size() == 11, "experience bar drawn with a real texture on all 11 animation frames, half fill (right = 76)")
	counts["xp_frames"] = frames.size()
	# Pending rectangles without a health gate, in the real play observation.
	var value := base.duplicate()
	value.merge({"hp": 0.0, "pending_health": 8.0, "dead": false}, true)
	hud.observe_play(value, true)
	check("pending_health" in hud.drawn, "pending health rectangle drawn at health 0 with the dead bit clear")
	# Gold counter with the real HUD font: text glyphs appear while visible.
	var visible_ticks := 0
	var text_ticks := 0
	var credited_at := -1
	for tick in range(0, 140):
		var gold_value := base.duplicate()
		gold_value.merge({"ready_ticks": tick, "gold": 500, "gold_pickups": [{"slot": 0, "amount": 250, "x": 320, "y": 200}] if tick == 0 else []}, true)
		hud.observe_play(gold_value, true)
		var state: Dictionary = hud.extras.gold_state()
		if int(state.alpha[0]) > 0: visible_ticks += 1
		if "gold" in hud.drawn and not hud.text_glyphs.is_empty(): text_ticks += 1
		if credited_at < 0 and int(state.gold[0]) != 500: credited_at = tick
	counts["gold_visible_frames"] = visible_ticks
	counts["gold_text_frames"] = text_ticks
	check(credited_at == 43 and visible_ticks == 133, "gold credited at tick 43, counter visible 133 frames (got %d, %d)" % [credited_at, visible_ticks])
	check(text_ticks >= 100, "counter text drawn with the real font while visible")
	check(str(hud.refusals.get("gold_coin", "")).length() > 0, "coin refused with its reason")
	check(not str(hud.refusals.get("minimap", "")).is_empty(), "minimap still refused with its reason")
	counts["drawn"] = hud.drawn
	print(JSON.stringify(counts, "  ", true))
	scene.return_to_selection()
	scene.real.clear_cache()
	quit(1 if failures else 0)
