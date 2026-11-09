extends SceneTree

# Opt-in check, against the user's own game folder, that the live save-buffer
# header follows the last card image loaded OR saved (research-save-screen
# @c8895e2 docs/save-screen.md:229, 258-279): load A, save B, and a death
# reload would read B. A refused save leaves the header unknown, so the death
# reload refuses. A card save in a level outside every act leaves the mark:
# save_build writes nothing there (research-saves@8d43286
# docs/save-format.md:55-58). The level script is off, so no Creature state
# is saved.
# --game-folder=ABS --out=ABS (ignored scratch) [--level=N]. Prints booleans
# and a level index only.
var args := {}

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var parts := arg.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	call_deferred("run")

func marked(scene, path: String, stem: String) -> bool:
	var status: Dictionary = scene.controller.death_reload_status()
	return scene._live_checkpoint_path == path and status.get("live_checkpoint", false) \
		and str(status.get("checkpoint_key", "")) == stem \
		and str(status.get("live_checkpoint_invalid", "x")).is_empty()

func act_of(scene, index: int) -> int:
	var found: Dictionary = scene.controller.campaign_act_of(str(scene.real.levels[index].stem))
	return int(found.get("act", -2)) if found.get("ok", false) else -2

func run() -> void:
	var report := {"opened": false, "level_index": -1, "saved_a": false, "a_marked": false,
		"loaded_a": false, "a_marked_after_load": false, "saved_b": false, "b_marked": false,
		"refused_save": false, "invalidated": false, "invalid_reason": false,
		"saved_c": false, "c_marked": false, "outside_level_loaded": false,
		"saved_outside": false, "outside_kept_mark": false, "no_reload_without_request": false}
	if not args.has("game-folder") or not args.has("out") or \
			not str(args["game-folder"]).is_absolute_path() or not str(args["out"]).is_absolute_path():
		print("DEATH_CHECKPOINT_MARK " + JSON.stringify(report))
		quit(2)
		return
	var out := str(args["out"])
	DirAccess.make_dir_recursive_absolute(out)
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.real.cache_dir = out.path_join("cache")
	report.opened = scene.open_game_folder(str(args["game-folder"]))
	if report.opened:
		# A synthetic authored character (as player_appearance_lifetime.gd), so
		# the save has a player record companion.
		var importer = ClassDB.instantiate("ChampionsPs2SaveImport")
		var image: PackedByteArray = ClassDB.class_call_static("ChampionsPs2SaveImport", "synthetic_slot", "Aria", 777)
		var character: Dictionary = importer.import_bytes(image, 0)
		var classes := [0, 0, 3, 3, 1, 1, 4, 4, 2, 2, 5, 6]
		character.class_index = classes[int(character.get("model_index", 0))]
		character.acknowledged = character.get("level", 1)
		report["character"] = character.get("ok", false) and \
			scene.controller.set_player_character(character).get("ok", false)
		scene.level_script.set_pressed_no_signal(false)
		# A, B and C are saved in a level an act row prefixes.
		var first := int(args.get("level", "0"))
		for index in range(first, scene.real.levels.size()):
			if act_of(scene, index) >= 0 and scene.load_real_level(index, false):
				report.level_index = index
				break
	if report.level_index >= 0:
		var a := out.path_join("mark-a.ocplay")
		var b := out.path_join("mark-b.ocplay")
		var c := out.path_join("mark-c.ocplay")
		var d := out.path_join("mark-d.ocplay")
		var stem := str(scene.real.levels[report.level_index].stem)
		for base in [a, b, c, d]:
			for suffix in ["", ".player", ".menus", ".level", ".campaign", ".worldstate", ".fight"]:
				if FileAccess.file_exists(base + suffix): DirAccess.remove_absolute(base + suffix)
		report.saved_a = scene.save_session_file(a)
		if not report.saved_a: print("DEATH_CHECKPOINT_MARK save_a status: " + str(scene.status_label.text).left(160))
		report.a_marked = report.saved_a and marked(scene, a, stem)
		report.loaded_a = report.saved_a and scene.load_session_file(a)
		report.a_marked_after_load = report.loaded_a and marked(scene, a, stem)
		report.saved_b = report.loaded_a and scene.save_session_file(b)
		report.b_marked = report.saved_b and marked(scene, b, stem)
		# A destination whose directory does not exist: the save is refused.
		report.refused_save = report.b_marked and not scene.save_session_file(out.path_join("absent/subdir/mark"))
		var status: Dictionary = scene.controller.death_reload_status()
		report.invalidated = report.refused_save and scene._live_checkpoint_path.is_empty() \
			and not status.get("live_checkpoint", true)
		report.invalid_reason = report.invalidated and not str(status.get("live_checkpoint_invalid", "")).is_empty()
		report.saved_c = report.invalidated and scene.save_session_file(c)
		report.c_marked = report.saved_c and marked(scene, c, stem)
		# A level no act row prefixes: its card save succeeds, but the mark
		# (path and key) stays C.
		if report.c_marked:
			for index in range(scene.real.levels.size()):
				if index != report.level_index and act_of(scene, index) == -1 and scene.load_real_level(index, false):
					report.outside_level_loaded = true
					break
		report.saved_outside = report.outside_level_loaded and scene.save_session_file(d)
		report.outside_kept_mark = report.saved_outside and marked(scene, c, stem)
		# No death task request: the scene's reload does nothing.
		report.no_reload_without_request = not scene.run_death_reload() and marked(scene, c, stem)
	print("DEATH_CHECKPOINT_MARK " + JSON.stringify(report))
	var passed := true
	for key in report:
		if key != "level_index" and not report[key]: passed = false
	scene.queue_free()
	quit(0 if passed else 1)
