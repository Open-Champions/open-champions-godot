extends SceneTree

# Opt-in, counts-only acceptance smoke against the owner's read-only game
# folder. No game bytes, names, levels or images are written to the project.
const CROSS := 0x40
const DOWN := 0x4000
const UP := 0x1000
const RIGHT := 0x2000
var args := {}

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var parts := arg.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	call_deferred("run")

func run() -> void:
	var report := {"folder_open": 0, "level_entered": 0, "player_ready": 0,
		"level_text": 0, "experience": 0, "mana_bar": 0, "gold": 0,
		"max_mana_known": 0, "gold_record_known": 0}
	if not args.has("disc") or not args.has("cache"):
		print(JSON.stringify(report))
		quit(2)
		return
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.real.cache_dir = str(args["cache"])
	report.folder_open = int(scene.open_game_folder(str(args["disc"])))
	if report.folder_open == 0 or scene.real.levels.is_empty():
		print(JSON.stringify(report))
		quit(1)
		return
	var frontend = scene.frontend
	var options := "user://player-state-smoke-%d.img" % OS.get_process_id()
	frontend.options_path = options
	frontend.boot()
	frontend.tap(CROSS)
	for i in range(239): frontend.step(0)
	frontend.tap(CROSS)
	for i in range(30): frontend.step(0)
	frontend.tap(CROSS)
	for i in range(4): frontend.tap(CROSS)
	frontend.tap(DOWN)
	frontend.tap(CROSS)
	frontend.tap(UP)
	frontend.tap(CROSS)
	frontend.tap(CROSS)
	frontend.tap(CROSS)
	for i in range(20): frontend.tap(RIGHT)
	frontend.tap(CROSS)
	frontend.type_name("Smoke")
	frontend.tap(DOWN)
	frontend.tap(CROSS)
	var value: Dictionary = scene.controller.get_observation()
	report.level_entered = int(scene.session_started and scene.real.current >= 0)
	report.player_ready = int(value.get("player_ready", false))
	report.max_mana_known = int(value.has("max_mana_raw"))
	report.gold_record_known = int(value.has("gold"))
	for key in ["level_text", "experience", "mana_bar", "gold"]:
		report[key] = int(key in scene.play_hud.drawn)
	print("PLAYER_STATE_REAL_SMOKE " + JSON.stringify(report))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(options))
	scene.queue_free()
	var complete := true
	for key in ["folder_open", "level_entered", "player_ready", "level_text", "experience", "mana_bar", "gold", "max_mana_known", "gold_record_known"]:
		complete = complete and int(report[key]) == 1
	quit(0 if complete else 1)
