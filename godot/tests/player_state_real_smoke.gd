extends SceneTree

# Opt-in, counts-only acceptance smoke against the owner's read-only game
# folder. No game bytes, names, levels or images are written to the project.
const CROSS := 0x40
const DOWN := 0x4000
const UP := 0x1000
const RIGHT := 0x2000
var args := {}
var run_dir := ""

func finish(report: Dictionary, code: int) -> void:
	print("PLAYER_STATE_REAL_SMOKE " + JSON.stringify(report))
	if not run_dir.is_empty():
		var listing := DirAccess.open(run_dir)
		if listing != null:
			listing.list_dir_begin()
			var entry := listing.get_next()
			while not entry.is_empty():
				if entry != "." and entry != ".." and not listing.current_is_dir():
					DirAccess.remove_absolute(ProjectSettings.globalize_path(run_dir.path_join(entry)))
				entry = listing.get_next()
			listing.list_dir_end()
		DirAccess.remove_absolute(ProjectSettings.globalize_path(run_dir))
	quit(code)

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var parts := arg.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	call_deferred("run")

func run() -> void:
	var report := {"folder_open": 0, "level_entered": 0, "player_ready": 0,
		"level_text": 0, "experience": 0, "mana_bar": 0, "gold": 0,
		"max_mana_known": 0, "gold_record_known": 0, "native_roundtrip": 0,
		"save_ok": 0, "companion_exists": 0, "load_ok": 0,
		"plain_save_ok": 0, "plain_load_ok": 0,
		"load_player_stage_failed": 0, "load_native_stage_failed": 0,
		"direct_companion_read": 0, "carry_two_levels": 0,
		"carry_loads": 0, "carry_gold_steps": 0,
		"unknown_current_refused": 0}
	if not args.has("disc") or not args.has("cache"):
		finish(report, 2)
		return
	run_dir = "user://player-state-smoke-%d" % OS.get_process_id()
	if DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(run_dir)) != OK:
		finish(report, 2)
		return
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.real.cache_dir = str(args["cache"])
	report.folder_open = int(scene.open_game_folder(str(args["disc"])))
	if report.folder_open == 0 or scene.real.levels.is_empty():
		scene.queue_free()
		finish(report, 1)
		return
	var frontend = scene.frontend
	var options := run_dir.path_join("options.img")
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
	report.max_mana_known = int(value.has("record_max_mana_raw"))
	report.gold_record_known = int(value.has("gold"))
	for key in ["level_text", "experience", "mana_bar", "gold"]:
		report[key] = int(key in scene.play_hud.drawn)
	# Authored script fixture: start tries checkPlayerAlive(0) then grants 123
	# gold. Three loads cross two carried boundaries; unknown HP must refuse.
	var gold_script := FileAccess.get_file_as_bytes("res://tests/fixtures/player_gold.scr")
	var initial_gold := int(value.get("gold", -1))
	var carried_ok := initial_gold >= 0 and not gold_script.is_empty()
	for stage in range(3):
		var enabled: Dictionary = scene.controller.enable_level_script(gold_script, PackedByteArray(), true)
		var live: Dictionary = scene.controller.get_observation()
		report.carry_loads += int(enabled.get("ok", false) and enabled.get("campaign_carried", false))
		report.carry_gold_steps += int(int(live.get("gold", -1)) == initial_gold + 123 * (stage + 1))
		carried_ok = carried_ok and enabled.get("ok", false) and (stage == 0 or enabled.get("campaign_carried", false)) and int(live.get("gold", -1)) == initial_gold + 123 * (stage + 1)
		report.unknown_current_refused += int(scene.controller.script_command_counts().get("unknown_player_current_refusals", 0))
	report.carry_two_levels = int(carried_ok and report.carry_loads == 2 and report.unknown_current_refused == 3)
	value = scene.controller.get_observation()
	var save_path := run_dir.path_join("session.ocplay")
	scene.save_format.select(0)
	report.save_ok = int(scene.save_session_file(save_path))
	report.companion_exists = int(FileAccess.file_exists(save_path + ".player"))
	if report.save_ok and report.companion_exists:
		report.load_ok = int(scene.load_session_file(save_path))
		if report.load_ok:
			var restored: Dictionary = scene.controller.get_observation()
			report.load_ok = int(restored.get("player_ready", false))
		else:
			report.load_player_stage_failed = int(scene.status_label.text.begins_with("Native scene loaded, but player record refused"))
			report.load_native_stage_failed = int(scene.status_label.text.begins_with("Load refused"))
			report.direct_companion_read = int(scene.controller.load_player_companion(save_path).get("ok", false))
		var plain = load("res://scenes/play_flow.tscn").instantiate()
		plain.automatic_steps = false
		root.add_child(plain)
		await process_frame
		var tables: Dictionary = plain.controller.load_arena_tables(str(scene.real.files["slus_209.73"]))
		var player_loaded: Dictionary = plain.controller.load_player_companion(save_path) if tables.get("ok", false) else {}
		if player_loaded.get("ok", false):
			plain.start_play()
			var plain_path := run_dir.path_join("plain.ocplay")
			plain.save_format.select(0)
			report.plain_save_ok = int(plain.save_session_file(plain_path))
			if report.plain_save_ok:
				report.plain_load_ok = int(plain.load_session_file(plain_path))
				if report.plain_load_ok:
					var plain_restored: Dictionary = plain.controller.get_observation()
					report.native_roundtrip = int(plain_restored.get("player_ready", false) and plain_restored.get("gold", -1) == value.get("gold", -2) and plain_restored.get("record_max_mana_raw", -1) == value.get("record_max_mana_raw", -2))
		plain.queue_free()
	scene.queue_free()
	var complete := true
	for key in ["folder_open", "level_entered", "player_ready", "level_text", "experience", "mana_bar", "gold", "max_mana_known", "gold_record_known", "native_roundtrip", "carry_two_levels"]:
		complete = complete and int(report[key]) == 1
	finish(report, 0 if complete else 1)
