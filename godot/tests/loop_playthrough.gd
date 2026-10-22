extends SceneTree

# Private, opt-in journey probe. The same script runs with --headless or a
# rendered display. Only counts and generic refusal text leave the game folder.
# --game-folder=ABS --out=ABS [--rendered]
const CROSS := 0x40
const DOWN := 0x4000
const UP := 0x1000
const RIGHT := 0x2000
const LEFT := 0x8000
const START := 0x800
const RealWorld = preload("res://real_data/real_world.gd")

var args := {}
var scene: Control
var fe: Control
var rows: Array[Dictionary] = []
var refusal_counts := {}
var seen := {}
var out_dir := ""
var screenshots := 0

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
		elif argument.begins_with("--"):
			args[argument.substr(2)] = true
	call_deferred("run")

func safe(value: Variant) -> String:
	var s := str(value)
	var path_re := RegEx.create_from_string("[^ ]*[/\\\\][^ ]*")
	s = path_re.sub(s, "<path>", true)
	var file_re := RegEx.create_from_string("[^ ]*\\.[A-Za-z0-9]{2,}[^ ]*")
	return file_re.sub(s, "<file>", true).left(180)

func refusal(stage: String, reason: Variant) -> void:
	var key := stage + ": " + safe(reason)
	refusal_counts[key] = int(refusal_counts.get(key, 0)) + 1
	print("PLAYTHROUGH_REFUSAL " + key)

func collect(stage: String) -> void:
	if scene == null: return
	var probes := {"scene": scene.status_label.text if scene.status_label != null else "",
		"frontend": fe.host_error if fe != null else ""}
	if scene.get("npc_dialog") != null:
		var d: Dictionary = scene.controller.dialog_status()
		if d.get("stalled", false): probes["dialog"] = d.get("stall_reason", "")
	if scene.get("fx") != null:
		for why in scene.fx.refusals: probes["fx " + safe(why)] = why
	if scene.get("audio") != null and "bank_errors" in scene.audio:
		for why in scene.audio.bank_errors: probes["audio " + safe(why)] = why
	for source in probes:
		var value := safe(probes[source])
		if value.is_empty() or not ("refus" in value.to_lower() or "unsupported" in value.to_lower() or source == "dialog"):
			continue
		var key := str(source) + " " + value
		if seen.get(key, false): continue
		seen[key] = true
		refusal(stage, key)

func mark(stage: String, result: String, extra := {}) -> void:
	collect(stage)
	var row := {"step": stage, "result": result,
		"fps": Engine.get_frames_per_second(),
		"memory_bytes": int(Performance.get_monitor(Performance.MEMORY_STATIC)),
		"rendered": DisplayServer.get_name() != "headless"}
	for key in extra: row[key] = extra[key]
	rows.append(row)
	print("PLAYTHROUGH_STEP " + JSON.stringify(row))

func capture(label: String) -> void:
	if DisplayServer.get_name() == "headless": return
	# A static view may not emit another frame_post_draw; the viewport keeps
	# its last completed texture. Force this state to draw before reading it.
	for i in range(2): await process_frame
	RenderingServer.force_draw()
	var image := root.get_texture().get_image()
	if image == null or image.is_empty():
		refusal(label, "rendered viewport has no image")
		return
	if image.save_png(out_dir.path_join(label + ".png")) == OK:
		screenshots += 1
	else:
		refusal(label, "screenshot write failed")

func tap(bits: int) -> void:
	fe.tap(bits)
	collect("frontend")

func advance(command := 0, count := 1) -> void:
	for i in range(count):
		scene.held_commands = command
		var r: Dictionary = scene.advance_step()
		if not r.get("ok", false): refusal("tick", r.get("error", "native step refused"))
		collect("tick")
		scene.held_commands = 0

func help_close() -> void:
	for i in range(200):
		if not scene.controller.text_hud().get("help_open", false): return
		if scene.controller.help_frame(true).get("closed", false): return
	refusal("dialog", "help box did not close in 200 frames")

func game_key(code: int) -> bool:
	var event := InputEventKey.new()
	event.keycode = code
	event.physical_keycode = code
	event.pressed = true
	var handled: bool = scene.handle_input_event(event)
	event.pressed = false
	scene.handle_input_event(event)
	return handled

func save_json() -> void:
	var summary := {"steps": rows, "refusals": refusal_counts,
		"screenshots": screenshots, "renderer": DisplayServer.get_name()}
	var file := FileAccess.open(out_dir.path_join("result.json"), FileAccess.WRITE)
	if file != null: file.store_string(JSON.stringify(summary, "  ", true) + "\n")
	print("PLAYTHROUGH_RESULT " + JSON.stringify(summary))

func run() -> void:
	if not args.has("game-folder") or not args.has("out"):
		printerr("--game-folder and --out are required")
		quit(2)
		return
	out_dir = str(args["out"])
	if not out_dir.is_absolute_path() or not str(args["game-folder"]).is_absolute_path():
		printerr("paths must be absolute")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(out_dir)
	root.size = Vector2i(1120, 780)
	root.content_scale_size = root.size
	scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = out_dir.path_join("library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = out_dir.path_join("cache")
	fe = scene.frontend
	fe.options_path = out_dir.path_join("options.img")
	# A repeat run must exercise the blank memory-card branch again.
	if FileAccess.file_exists(fe.options_path): DirAccess.remove_absolute(fe.options_path)
	var opened: bool = scene.open_game_folder(str(args["game-folder"]))
	mark("game_folder", "opened" if opened else "refused", {"level_candidates": scene.real.levels.size()})
	if not opened:
		save_json()
		quit(1)
		return
	scene.level_script.set_pressed_no_signal(true)
	var boot: Dictionary = fe.boot()
	mark("boot_card", fe.screen() if boot.get("ok", false) else "refused")
	await capture("01-boot")
	if fe.screen() == "boot-card": tap(CROSS)
	mark("logo", fe.screen())
	for i in range(239): fe.step(0)
	mark("intro", fe.screen())
	if fe.screen() == "intro-movie":
		for i in range(30): fe.step(0)
		tap(CROSS)
	mark("main_menu", fe.screen())
	await capture("02-main")
	if fe.screen() == "main":
		for i in range(4): tap(CROSS)
		if fe.screen() == "difficulty": tap(CROSS)
		mark("character_class", fe.screen())
		if fe.screen() == "char-class": tap(CROSS)
		if fe.screen() == "char-appearance": tap(CROSS)
		if fe.screen() == "char-ability":
			for i in range(20): tap(RIGHT)
			tap(CROSS)
		if fe.screen() == "char-name":
			fe.type_name("Probe")
			tap(DOWN)
			tap(CROSS)
	mark("character_created", fe.screen(), {"player_ready": scene.controller.get_observation().get("player_ready", false)})
	mark("first_level", "entered" if scene.real.current >= 0 and scene.play_mode else "refused",
		{"level_index": scene.real.current, "session_started": scene.session_started,
		"script_enabled": scene.controller.get_play_capabilities().get("level_script_enabled", false)})
	await capture("03-level")
	if scene.real.current < 0 and not scene.real.levels.is_empty():
		var direct: bool = scene.load_real_level(0)
		mark("direct_level_diagnostic", "loaded" if direct else "refused", {"level_index": scene.real.current})
		if direct and not scene.play_mode: scene.start_play()
	if scene.real.current >= 0 and not scene.controller.get_play_capabilities().get("level_script_enabled", false):
		mark("level_script", "enabled" if scene.attach_level_script() else "refused")
	else:
		mark("level_script", "enabled" if scene.controller.get_play_capabilities().get("level_script_enabled", false) else "unavailable")
	help_close()
	if scene.paused and scene.session_started: scene.resume_session()
	var dialog: Dictionary = scene.controller.dialog_status()
	var talkers := int(dialog.get("talkers", 0))
	var walk_start: Vector3 = scene.controller.get_presentation().get("current_position", Vector3.ZERO)
	var opened_dialog := false
	var physical_attempts := 0
	if dialog.get("ok", false) and talkers > 0 and scene.play_mode:
		# No target coordinates are published by the client. Sweep the four
		# movement directions and use the ordinary E key at each stop.
		for direction in [1, 2, 4, 8]:
			for part in range(4):
				advance(direction, 24)
				game_key(KEY_E)
				advance(0, 2)
				help_close()
				physical_attempts += 1
				if scene.controller.dialog_status().get("open", false):
					opened_dialog = true
					break
			if opened_dialog: break
	var walk_end: Vector3 = scene.controller.get_presentation().get("current_position", Vector3.ZERO)
	mark("walk_and_talk", "dialog_open" if opened_dialog else "refused",
		{"talkers": talkers, "physical_use_attempts": physical_attempts,
		"walk_displacement": walk_start.distance_to(walk_end),
		"nearest_target_known": false, "paused": scene.paused,
		"menu_visible": scene.menu.visible})
	if opened_dialog:
		for i in range(3): await process_frame
		var frame: Dictionary = scene.npc_dialog.frame
		for why in frame.get("refusals", []): refusal("dialog_draw", why)
		var visual_result := "not_applicable_headless" if DisplayServer.get_name() == "headless" else \
			("drawn" if frame.get("open", false) and
			(not frame.get("sprites", []).is_empty() or not frame.get("glyphs", []).is_empty()) else "refused")
		mark("dialog_visual", visual_result,
			{"box_sprites": frame.get("sprites", []).size(),
			"glyphs": frame.get("glyphs", []).size(),
			"frame_open": frame.get("open", false),
			"paused": scene.paused, "menu_visible": scene.menu.visible,
			"font_ready": scene.npc_dialog.font_texture != null,
			"box_ready": scene.npc_dialog.box_texture != null})
	await capture("04-dialog-attempt")
	if opened_dialog:
		game_key(KEY_DOWN)
		var closed := false
		for i in range(1200):
			await process_frame
			help_close()
			if scene.paused and not scene.stopped: scene.resume_session()
			if i % 8 == 0: game_key(KEY_ENTER)
			advance()
			if not scene.controller.dialog_status().get("open", false):
				closed = true
				break
		var dialog_counts: Dictionary = scene.controller.dialog_status().get("counts", {})
		for why in dialog_counts.get("refused_reasons", {}):
			refusal("dialog", "%s count=%d" % [safe(why), int(dialog_counts.refused_reasons[why])])
		mark("dialog_choice_close", "closed" if closed and int(dialog_counts.get("choices_taken", 0)) > 0 else "refused",
			{"closed": closed, "choices_taken": int(dialog_counts.get("choices_taken", 0)),
			"pending_levels": int(dialog_counts.get("pending_levels", 0))})
	else:
		mark("dialog_choice_close", "blocked_by_walk_and_talk")
	var before: Dictionary = scene.controller.get_observation()
	var level_before: int = scene.real.current
	var hp_before := float(before.get("hp", -1.0))
	var gold_before := int(before.get("gold", -1))
	var actors_before: Array = before.get("combat_actors", [])
	var attack_handled := game_key(KEY_J)
	advance(0, 120)
	var after: Dictionary = scene.controller.get_observation()
	var saved_position: Vector3 = scene.controller.get_presentation().get("current_position", Vector3.ZERO)
	var saved_hp := float(after.get("hp", -1.0))
	var saved_gold := int(after.get("gold", -1))
	var saved_campaign: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var actors_after: Array = after.get("combat_actors", [])
	mark("enemy_fight", "combat_observed" if after.get("combat_enabled", false) else "refused",
		{"attack_input_handled": attack_handled, "actors_before": actors_before.size(),
		"actors_after": actors_after.size(), "hp_before": hp_before,
		"hp_after": float(after.get("hp", -1.0)), "gold_before": gold_before,
		"gold_after": int(after.get("gold", -1)), "drops_observed": 0})
	await capture("05-fight-attempt")
	help_close()
	if scene.controller.dialog_status().get("open", false):
		refusal("pause", "dialog remained open before pause")
		if scene.controller.dialog_status().get("stalled", false): scene.leave_stalled_dialog()
	if fe.screen() == "playing": tap(START)
	mark("pause", fe.screen())
	await capture("06-pause")
	var pause_save_entry := false
	var pause_entries: int = fe.view.get("entries", []).size()
	if fe.screen() == "pause":
		for entry in fe.view.get("entries", []):
			if str(entry.get("id", "")) == "save": pause_save_entry = true
	# Research gives the original pause exactly five entries, with no Save.
	# The host's F5 route opens the documented in-game save screen.
	var native_pause_intact: bool = fe.screen() == "pause" and pause_entries == 5 and not pause_save_entry
	var save_path := out_dir.path_join("playthrough.ocplay")
	for suffix in ["", ".player", ".menus", ".level", ".campaign"]:
		if FileAccess.file_exists(save_path + suffix): DirAccess.remove_absolute(save_path + suffix)
	fe.queue_path("save_session", save_path)
	var route_handled := game_key(KEY_F5)
	mark("pause_save", "routed" if native_pause_intact and route_handled and fe.screen() == "save-game" else "refused",
		{"original_pause_entries": pause_entries})
	if fe.screen() == "save-game": tap(CROSS)
	var saved := FileAccess.file_exists(save_path) and FileAccess.file_exists(save_path + ".level") and FileAccess.file_exists(save_path + ".campaign")
	mark("in_game_save", "saved" if saved else "refused", {"screen": fe.screen()})
	await capture("07-save")
	if fe.screen() == "saved": tap(CROSS)
	if fe.screen() == "playing": tap(START)
	if fe.screen() == "pause":
		for i in range(4): tap(DOWN)
		tap(CROSS)
		if fe.screen() == "exit-confirm":
			tap(LEFT)
			tap(CROSS)
	mark("quit_to_main", fe.screen())
	await capture("08-main-after-quit")
	var quit_ok: bool = fe.screen() == "main"
	var load_attempted := false
	if quit_ok and saved:
		tap(DOWN)
		fe.queue_path("load_session", save_path)
		tap(CROSS)
		if fe.screen() == "load-game":
			load_attempted = true
			tap(CROSS)
	var restored: Dictionary = scene.controller.get_observation()
	var restored_campaign: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var quests_match := true
	for key in ["quest_lines", "mission_available_bits", "mission_complete_bits", "vars", "vars_nonzero"]:
		quests_match = quests_match and saved_campaign.get(key, -1) == restored_campaign.get(key, -2)
	var carried: bool = quit_ok and load_attempted and fe.screen() == "playing" and scene.real.current == level_before \
		and scene.terrain_root.has_meta("level_texture_pair") \
		and int(restored.get("gold", -1)) == saved_gold \
		and is_equal_approx(float(restored.get("hp", -1.0)), saved_hp) \
		and scene.controller.get_presentation().get("current_position", Vector3.ZERO).distance_to(saved_position) < 0.01 \
		and quests_match
	mark("load_and_carry", "restored" if carried else "refused",
		{"screen": fe.screen(), "load_attempted": load_attempted,
		"level_index_before": level_before,
		"level_index_after": scene.real.current, "gold_before": saved_gold,
		"gold_after": int(restored.get("gold", -1)), "hp_before": saved_hp,
		"hp_after": float(restored.get("hp", -1.0)), "quests_match": quests_match})
	await capture("09-loaded")
	var transition_before: int = scene.real.current
	var pending_levels := int(scene.controller.dialog_status().get("counts", {}).get("pending_levels", 0))
	if pending_levels > 0 and fe.screen() == "playing": advance(0, 180)
	mark("level_change", "changed" if scene.real.current != transition_before else "refused",
		{"script_enabled": scene.controller.get_play_capabilities().get("level_script_enabled", false),
		"pending_script_levels": pending_levels, "exit_target_exposed": false})
	await capture("10-level-change-attempt")
	# A new scene has no in-memory WORLD/TEX save association. The load must
	# recover the level from the selected folder and its saved identity. Remove
	# the first scene before creating another process-lifetime audio mixer.
	scene.queue_free()
	await process_frame
	scene = null
	fe = null
	var cold = load("res://scenes/real_data_play.tscn").instantiate()
	cold.automatic_steps = false
	root.add_child(cold)
	await process_frame
	cold.components.library.history_path = out_dir.path_join("cold-library.json")
	cold.components.library._entries.clear()
	cold.frontend.options_path = out_dir.path_join("cold-options.img")
	cold.real.cache_dir = out_dir.path_join("cold-cache")
	var cold_opened: bool = cold.open_game_folder(str(args["game-folder"]))
	var cold_loaded: bool = cold_opened and cold.level_textures.saves.is_empty() and cold.load_session_file(save_path)
	var cold_state: Dictionary = cold.controller.get_observation()
	var cold_campaign: Dictionary = cold.controller.script_command_counts().get("campaign", {})
	var cold_match: bool = cold_loaded and cold.real.current == level_before and cold.terrain_root.has_meta("level_texture_pair") and \
		int(cold_state.get("gold", -1)) == saved_gold and is_equal_approx(float(cold_state.get("hp", -1.0)), saved_hp) and \
		cold.controller.get_presentation().get("current_position", Vector3.ZERO).distance_to(saved_position) < 0.01
	for key in ["quest_lines", "mission_available_bits", "mission_complete_bits", "vars", "vars_nonzero"]:
		cold_match = cold_match and cold_campaign.get(key, -1) == saved_campaign.get(key, -2)
	mark("cold_load", "restored" if cold_match else "refused", {"opened": cold_opened, "loaded": cold_loaded})
	# A displayed level with different collision must not be reused for this save.
	var other_index := 0 if level_before != 0 else 1
	var other_loaded: bool = cold.load_real_level(other_index)
	var mismatch: Dictionary = cold.controller.prepare_save_file(save_path, true) if other_loaded else {}
	var mismatch_refused: bool = other_loaded and not mismatch.get("ok", false) and \
		"collision differs" in str(mismatch.get("error", ""))
	mark("mismatched_display", "refused_as_expected" if mismatch_refused else "refused",
		{"different_level_loaded": other_loaded})
	cold.queue_free()
	await process_frame
	save_json()
	var incomplete := false
	for row in rows:
		if str(row.result) in ["refused", "blocked_by_walk_and_talk"]:
			incomplete = true
	quit(1 if incomplete else 0)
