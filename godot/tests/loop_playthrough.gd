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
const MISSION_IMAGES := ["misswar.tex", "missinnovation.tex", "misspit.tex", "misswater.tex",
	"misstorment.tex", "missdisease.tex", "missvalor.tex", "missfire.tex",
	"missstorms.tex", "missfeydark.tex", "missnightmare.tex", "missfear.tex"]

var args := {}
var scene: Control
var fe: Control
var rows: Array[Dictionary] = []
var refusal_counts := {}
var seen := {}
var out_dir := ""
var screenshots := 0
var audit_menu_checks := true

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
	if stage.begins_with("mission_menu_") and result == "refused": audit_menu_checks = false
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
		if label.begins_with("10-mission-menu"): audit_menu_checks = false
		refusal(label, "rendered viewport has no image")
		return
	if image.save_png(out_dir.path_join(label + ".png")) == OK:
		screenshots += 1
	else:
		if label.begins_with("10-mission-menu"): audit_menu_checks = false
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
	var dialog_choice_completed := false
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
		dialog_choice_completed = closed and int(dialog_counts.get("choices_taken", 0)) > 0
		mark("dialog_choice_close", "closed" if dialog_choice_completed else "refused",
			{"closed": closed, "choices_taken": int(dialog_counts.get("choices_taken", 0)),
			"pending_levels": int(dialog_counts.get("pending_levels", 0))})
	else:
		mark("dialog_choice_close", "blocked_by_walk_and_talk")
	# The opening choice and this hub PushTrigger are checked separately in
	# research-story-route. Exercise their native integration in this session.
	var transition_before: int = scene.real.current
	var at_hub: bool = transition_before >= 0 and str(scene.real.levels[transition_before].stem) == "hub"
	var menu_before: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_1_unlock", "available" if at_hub and menu_before.get("first_story_available", false) else "refused",
		{"at_hub": at_hub, "available": menu_before.get("first_story_available", false)})
	var hub_trigger_found := false
	for target in scene.controller.script_exit_targets():
		if int(target.index) == 11 and str(target.kind) == "PushTrigger" and target.get("script_action", false):
			hub_trigger_found = true
			break
	var menu_was_closed: bool = menu_before.get("ok", false) and not menu_before.get("open", false)
	var trigger_result: Dictionary = scene.controller.script_run_exit(11) if at_hub and hub_trigger_found and dialog_choice_completed and menu_was_closed and not scene.controller.dialog_status().get("open", false) else {"ok": false}
	var menu_after: Dictionary = scene.controller.script_mission_menu_state()
	var menu_open: bool = bool(menu_after.get("ok", false) and menu_after.get("open", false))
	var trigger_opened_menu: bool = hub_trigger_found and menu_was_closed and trigger_result.get("ok", false) and menu_open
	scene._refresh_mission_menu()
	var opening_before: Dictionary = scene.controller.script_mission_menu_state()
	var early_key := game_key(KEY_ENTER)
	var opening_after: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_menu_opening_gate", "checked" if early_key and int(opening_after.get("state", -1)) == int(opening_before.get("state", -2)) and not opening_after.get("pending", "") else "refused")
	await capture("10-mission-menu-opening-gate")
	mark("hub_trigger_mission_menu", "opened" if trigger_opened_menu else "refused",
		{"hub_trigger_found": hub_trigger_found, "dialog_choice_completed": dialog_choice_completed,
		"menu_was_closed": menu_was_closed, "trigger_action_ok": trigger_result.get("ok", false), "menu_open": menu_open,
		"host_panel_visible": scene.mission_menu_panel.visible})
	for i in range(18): await process_frame
	var mission_art = scene.mission_menu_panel
	var table_state: Dictionary = scene.controller.script_mission_menu_state()
	var table_rows: Array = table_state.get("rows", [])
	var matched_rows := 0
	if table_rows.size() == 12:
		for i in range(12):
			var row: Dictionary = table_rows[i]
			if str(row.get("image", "")).to_lower() == MISSION_IMAGES[i] and int(row.get("x", -1)) == (140 if i % 2 == 0 else 70) and int(row.get("y", -1)) == 40 + 200 * i:
				matched_rows += 1
	mark("mission_menu_table", "checked" if matched_rows == 12 else "refused", {"matched_rows": matched_rows})
	mark("mission_menu_assets", "checked" if mission_art.assets_valid and mission_art.art.size() == 32 else "refused",
		{"textures": mission_art.art.size(), "small_font": mission_art.font_texture != null})
	await capture("10-mission-menu")
	var unsupported := false
	for name in ["cosmiclines.tex", "starfield.tex", "starsparkle.tex", "mainglow.tex", "amuletglow.tex", "bonusglow.tex"]:
		if mission_art.submitted_sprites.has(name): unsupported = true
	mark("mission_menu_unsupported_art", "checked" if not unsupported and mission_art.submitted_text_keys == ["AcceptBack"] else "refused")
	var missing_row: Dictionary = table_state.duplicate(true)
	missing_row.rows[0].available = false
	mission_art.present(missing_row)
	for i in range(2): await process_frame
	await capture("10-mission-menu-row-zero-missing-bit")
	mark("mission_menu_row_zero_draw", "checked" if missing_row.rows[0].draw_present and not missing_row.rows[0].available and int(mission_art.submitted_tints.get("misswar.tex", 0)) == 0x7f808080 else "refused")
	mission_art.present(table_state)
	var prior_art_valid: bool = mission_art.assets_valid
	var config_refused: bool = not mission_art.configure(scene.real, scene.controller, scene.inspector, PackedByteArray())
	scene._refresh_mission_menu()
	await capture("10-mission-menu-art-refused")
	var refused_before: Dictionary = scene.controller.script_mission_menu_state()
	var refused_input := game_key(KEY_ENTER)
	var refused_after: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_menu_art_gate", "checked" if prior_art_valid and config_refused and not mission_art.visible and refused_input and refused_before == refused_after else "refused",
		{"input_handled": refused_input})
	mission_art.configure(scene.real, scene.controller, scene.inspector, scene.real.hud_font_bytes())
	scene._refresh_mission_menu()
	for first_active in [false, true]:
		for pause_open in [false, true]:
			var prompt_state: Dictionary = table_state.duplicate(true)
			prompt_state.network = true
			prompt_state.prompt_first_active = first_active
			prompt_state.prompt_paused = pause_open
			mission_art.present(prompt_state)
			for i in range(2): await process_frame
			await capture("10-mission-menu-network-%d-%d" % [int(first_active), int(pause_open)])
			var expected_key := "AcceptBack" if first_active else ("" if pause_open else "clientMissionSelect")
			mark("mission_menu_network_prompt", "checked" if mission_art.submitted_text_keys == ([] if expected_key.is_empty() else [expected_key]) else "refused",
				{"first_active": first_active, "paused": pause_open})
	var unknown_prompt: Dictionary = table_state.duplicate(true)
	unknown_prompt.network = true
	mission_art.present(unknown_prompt)
	for i in range(2): await process_frame
	await capture("10-mission-menu-network-unknown")
	mark("mission_menu_network_unknown", "checked" if mission_art.submitted_text_keys.is_empty() else "refused")
	mission_art.present(table_state)
	if DisplayServer.get_name() != "headless":
		var menu_view = scene.mission_menu_panel
		var actual: Dictionary = scene.controller.script_mission_menu_state()
		var base_image := Image.load_from_file(out_dir.path_join("10-mission-menu.png"))
		var altered: Dictionary = actual.duplicate(true)
		var altered_rows: Array = altered.get("rows", [])
		if altered_rows.size() == 12:
			var second_row: Dictionary = altered_rows[1]
			second_row.available = true
			second_row.draw_present = true
			menu_view.present(altered)
			await capture("10-mission-menu-second-available")
		menu_view.present(actual)
		var second := Image.load_from_file(out_dir.path_join("10-mission-menu-second-available.png"))
		var differences := 0
		if base_image != null and second != null and base_image.get_size() == second.get_size():
			# The documented second node is (70,240), 411x156 on a 640x448 canvas.
			var scale := minf(float(base_image.get_width()) / 640.0, float(base_image.get_height()) / 448.0)
			var left := int((base_image.get_width() - 640.0 * scale) * 0.5)
			var top := int((base_image.get_height() - 448.0 * scale) * 0.5)
			for y in range(250, 385, 8):
				for x in range(80, 465, 8):
					var point := Vector2i(left + int(x * scale), top + int(y * scale))
					if base_image.get_pixelv(point) != second.get_pixelv(point): differences += 1
		mark("mission_menu_render", "checked" if menu_view.assets_valid and menu_view.art.size() == 32 and differences > 0 else "refused",
			{"textures": menu_view.art.size(), "locked_available_changed_samples": differences})
	if args.has("menu-cancel-only"):
		var cancel_handled := game_key(KEY_ESCAPE)
		for i in range(9): await process_frame
		var cancelled: Dictionary = scene.controller.script_mission_menu_state()
		var cancel_ok: bool = cancel_handled and int(cancelled.get("state", -1)) == 4 and int(cancelled.get("counter", 1)) <= 0 and not cancelled.get("open", true) and not scene.mission_menu_panel.visible
		mark("mission_menu_cancel", "checked" if cancel_ok else "refused",
			{"handled": cancel_handled, "closed": not cancelled.get("open", true), "counter": cancelled.get("counter", 1)})
		save_json()
		quit(0 if cancel_ok else 1)
		return
	var locked_move_handled := game_key(KEY_DOWN)
	var locked_move_state: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_menu_locked_navigation", "checked" if locked_move_handled and int(locked_move_state.get("row", -1)) == 0 and int(locked_move_state.get("column", -1)) == 0 else "refused",
		{"handled": locked_move_handled, "row": locked_move_state.get("row", -1), "column": locked_move_state.get("column", -1)})
	var selected := false
	if trigger_opened_menu:
		var ready: Dictionary = scene.controller.script_mission_menu_state()
		if int(ready.get("state", -1)) == 2 and ready.get("first_story_available", false):
			var confirm_handled := game_key(KEY_ENTER)
			var confirm: Dictionary = scene.controller.script_mission_menu_state()
			for i in range(4): await process_frame
			var waiting: Dictionary = scene.controller.script_mission_menu_state()
			await capture("10-mission-menu-fade-gate")
			mark("mission_menu_fade_gate", "checked" if confirm_handled and int(confirm.get("state", -1)) == 5 and int(waiting.get("state", -1)) == 6 and not waiting.get("pending", "") else "refused")
			# Controlled test fade; the live client refuses until its source is ported.
			var after_fade: Dictionary = scene.controller.script_mission_menu_tick(0, 128)
			if after_fade.get("ok", false): after_fade = scene.controller.script_mission_menu_tick(0, 128)
			if after_fade.get("ok", false): after_fade = scene.controller.script_mission_menu_tick(0, 128)
			selected = after_fade.get("launched", false) and str(after_fade.get("pending", "")) == "blood1"
	var pending: Dictionary = scene.controller.script_level_request()
	mark("mission_1_story_request", "blood1" if selected and str(pending.get("name", "")) == "blood1" else "refused",
		{"queued": pending.get("queued", false), "orientation": pending.get("orientation", -1)})
	var record_before: Dictionary = scene.controller.get_observation()
	var quests_before: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var transition_ok: bool = selected and bool(scene._apply_level_change())
	var record_after: Dictionary = scene.controller.get_observation()
	var quests_after: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var mission1_still_available: bool = bool(scene.controller.script_mission_menu_state().get("first_story_available", false))
	var record_carried: bool = transition_ok and scene.last_campaign_carried and scene.last_campaign_image_restored \
		and str(record_after.get("name", "")) == str(record_before.get("name", "")) \
		and int(record_after.get("player_class", -1)) == int(record_before.get("player_class", -2)) \
		and int(record_after.get("gold", -1)) == int(record_before.get("gold", -2)) \
		and int(record_after.get("experience", -1)) == int(record_before.get("experience", -2)) \
		and int(record_after.get("level", -1)) == int(record_before.get("level", -2)) \
		and int(record_after.get("record_max_hp_raw", -1)) == int(record_before.get("record_max_hp_raw", -2)) \
		and int(record_after.get("record_max_mana_raw", -1)) == int(record_before.get("record_max_mana_raw", -2))
	var at_blood1: bool = scene.real.current >= 0 and str(scene.real.levels[scene.real.current].stem) == "blood1"
	mark("blood1_arrival", "arrived" if record_carried and at_blood1 and mission1_still_available else "refused",
		{"level_index_before": transition_before, "level_index_after": scene.real.current,
		"pending_script_level": pending.get("queued", false), "campaign_carried": scene.last_campaign_carried,
		"campaign_image_restored": scene.last_campaign_image_restored,
		"record_carried": record_carried, "quest_vars_before": int(quests_before.get("vars", 0)),
		"quest_vars_after": int(quests_after.get("vars", 0)),
		"mission1_still_available": mission1_still_available,
		"mission_available_before": int(quests_before.get("mission_available_bits", 0)),
		"mission_available_after": int(quests_after.get("mission_available_bits", 0))})
	await capture("10-level-change")
	if args.has("route-only"):
		save_json()
		quit(0 if at_hub and dialog_choice_completed and trigger_opened_menu and selected and record_carried and at_blood1 and mission1_still_available and audit_menu_checks else 1)
		return
	# Combat remains a separate diagnostic until blood1 hostile constructors
	# yield live enemies. Direct selection of valor is not a campaign route.
	var valor_index := -1
	for index in range(scene.real.levels.size()):
		if str(scene.real.levels[index].stem) == "valor": valor_index = index
	var valor_loaded: bool = valor_index >= 0 and scene.load_real_level(valor_index)
	if valor_loaded: scene.start_play()
	mark("valor_fight_setup", "loaded" if valor_loaded else "refused")
	var before: Dictionary = scene.controller.get_observation()
	var level_before: int = scene.real.current
	var hp_before := float(before.get("hp", -1.0))
	var gold_before := int(before.get("gold", -1))
	var actors_before: Array = before.get("combat_actors", [])
	var attack_handled := game_key(KEY_J)
	advance(0, 120)
	var after: Dictionary = scene.controller.get_observation()
	var actors_after: Array = after.get("combat_actors", [])
	mark("valor_fight", "combat_observed" if valor_loaded and after.get("combat_enabled", false) else "refused",
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
	if fe.screen() == "pause":
		for entry in fe.view.get("entries", []):
			if str(entry.get("id", "")) == "save": pause_save_entry = true
	mark("pause_save", "available" if pause_save_entry else "refused")
	if fe.screen() == "pause": tap(0x20)
	var save_path := out_dir.path_join("playthrough.ocplay")
	for suffix in ["", ".player", ".menus"]:
		if FileAccess.file_exists(save_path + suffix): DirAccess.remove_absolute(save_path + suffix)
	fe.queue_path("save_session", save_path)
	var save_request: Dictionary = fe.open_save_screen()
	if save_request.get("ok", false) and fe.screen() == "save-game": tap(CROSS)
	var saved := FileAccess.file_exists(save_path)
	mark("host_save_fallback", "saved" if saved else "refused", {"screen": fe.screen()})
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
	var carried: bool = quit_ok and load_attempted and fe.screen() == "playing" and scene.real.current == level_before \
		and int(restored.get("gold", -1)) == gold_before \
		and is_equal_approx(float(restored.get("hp", -1.0)), hp_before)
	mark("load_and_carry", "restored" if carried else "refused",
		{"screen": fe.screen(), "load_attempted": load_attempted,
		"level_index_before": level_before,
		"level_index_after": scene.real.current, "gold_before": gold_before,
		"gold_after": int(restored.get("gold", -1)), "hp_before": hp_before,
		"hp_after": float(restored.get("hp", -1.0))})
	await capture("09-loaded")
	save_json()
	var incomplete := false
	for row in rows:
		if str(row.result) in ["refused", "blocked_by_walk_and_talk"]:
			incomplete = true
	quit(1 if incomplete else 0)
