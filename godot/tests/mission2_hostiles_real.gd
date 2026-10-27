extends SceneTree

# Opt-in, read-only acceptance using the owner's selected RTA folder.
# Identities/counts below are public research-mission2 ffff9a7 census inputs.
# Prints only counts; never writes decoded game content. Not a CI fixture.
# --game-folder=DIR --cache=DIR
const LEVELS := ["innovat1", "inmine1", "inmine2", "innovat2"]
const CLASSES := ["clockworkcutter", "clockworkhammer", "Droid", "IronGuardian"]
const EXPECTED := [[23,29,0,0], [23,23,30,0], [20,14,0,0], [0,0,0,1]]
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
		printerr("mission2: " + what)
	return ok

func run() -> void:
	if not args.has("game-folder") or not args.has("cache"):
		quit(2)
		return
	var scene: Control = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = str(args.cache).path_join("level-library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = str(args.cache)
	if not check(scene.open_game_folder(str(args["game-folder"])), "folder refused"):
		quit(1)
		return
	var total := 0
	for number in range(LEVELS.size()):
		scene.return_to_selection()
		for i in range(3): await process_frame
		var index := -1
		for i in range(scene.real.levels.size()):
			if str(scene.real.levels[i].stem) == LEVELS[number]: index = i
		if not check(index >= 0, "level candidate missing"): continue
		var loaded: Dictionary = scene.real.load_level(index)
		if not check(loaded.get("ok", false), "level load refused"): continue
		scene.level_script.set_pressed_no_signal(true)
		if not check(scene.attach_level_script(), "script host refused"): continue
		scene.actions.play.pressed.emit()
		if not check(scene.play_mode, "play refused"): continue
		for i in range(60):
			var result: Dictionary = scene.advance_step()
			check(result.get("ok", false), "native tick refused")
			if scene.controller.text_hud().get("help_open", false): scene.controller.help_frame(true)
		var observed: Dictionary = scene.controller.get_observation()
		check(observed.get("fight_started", false), "fight did not start")
		check(not observed.get("fight_failed", false), "fight runtime failed")
		var counts := [0,0,0,0]
		for actor in observed.get("fight_enemies", []):
			var kind := CLASSES.find(str(actor.kind))
			if kind >= 0:
				counts[kind] += 1
				check(int(actor.hp_raw) != 0, "base HP was not constructed")
		check(counts == EXPECTED[number], "hostile census mismatch")
		check(scene.fight_enemies.instances.is_empty(), "mission2 actor used a boar model")
		for count in counts: total += int(count)
		print("MISSION2_LEVEL %d counts=%s" % [number, str(counts)])
	print("MISSION2_REAL total=%d failures=%d" % [total, failures])
	scene.queue_free()
	await process_frame
	quit(0 if failures == 0 else 1)
