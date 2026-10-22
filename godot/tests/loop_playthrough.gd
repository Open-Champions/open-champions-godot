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

# Compare two rendered viewport captures with the dialog view shown and hidden.
# The box bounds come from docs/DIALOG.md; a nonzero count proves that its
# pixels, rather than only native draw calls or a decoded texture, reached play.
func dialog_box_pixels() -> int:
	RenderingServer.force_draw()
	var shown: Image = scene.viewport.get_texture().get_image()
	scene.npc_dialog.hide()
	await process_frame
	RenderingServer.force_draw()
	var hidden: Image = scene.viewport.get_texture().get_image()
	scene.npc_dialog.show()
	await process_frame
	if shown == null or hidden == null or shown.is_empty() or hidden.is_empty(): return 0
	var size: Vector2 = scene.npc_dialog.size
	var factor := minf(size.x / 640.0, size.y / 448.0)
	var origin := Vector2((size.x - 640.0 * factor) * 0.5, (size.y - 448.0 * factor) * 0.5)
	var box := Rect2i(Vector2i(origin + Vector2(35, 280) * factor), Vector2i(Vector2(560, 156) * factor))
	var bounds := box.intersection(Rect2i(Vector2i.ZERO, shown.get_size()))
	var changed := 0
	for y in range(bounds.position.y, bounds.end.y, 3):
		for x in range(bounds.position.x, bounds.end.x, 3):
			if shown.get_pixel(x, y) != hidden.get_pixel(x, y): changed += 1
	return changed

# The level screenshot is paired with an actor-hidden viewport capture. The
# count checks that the inspected mesh actually contributed visible pixels.
func player_actor_pixels() -> int:
	RenderingServer.force_draw()
	var shown: Image = scene.viewport.get_texture().get_image()
	scene.actor_visual.hide()
	await process_frame
	RenderingServer.force_draw()
	var hidden: Image = scene.viewport.get_texture().get_image()
	scene.actor_visual.show()
	await process_frame
	if shown == null or hidden == null or shown.is_empty() or hidden.is_empty(): return 0
	var changed := 0
	for y in range(0, shown.get_height(), 3):
		for x in range(0, shown.get_width(), 3):
			if shown.get_pixel(x, y) != hidden.get_pixel(x, y): changed += 1
	return changed

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
	var class_mesh: bool = scene.actor_visual.mesh != null and scene.actor_visual.mesh != scene._default_mesh \
		and not (scene.actor_visual.mesh is CapsuleMesh) and not (scene.actor_visual.mesh is SphereMesh) \
		and not scene.asset_label.text.begins_with("Practice figure")
	var actor_pixels := await player_actor_pixels() if DisplayServer.get_name() != "headless" else 0
	mark("player_visual", "not_applicable_headless" if DisplayServer.get_name() == "headless" else \
		("class_mesh" if class_mesh and actor_pixels > 0 else "refused"),
		{"class_mesh": class_mesh, "mesh_type": scene.actor_visual.mesh.get_class(), "actor_pixels": actor_pixels,
		"practice_label": scene.asset_label.text.begins_with("Practice figure")})
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
		# Opening the dialog starts a camera swoop. Its draw task returns no
		# operations until that move has arrived; sample only a drawable frame.
		for i in range(4000):
			advance()
			await process_frame
			if not scene.controller.dialog_status().get("camera_moving", false) and \
					 not scene.npc_dialog.frame.get("sprites", []).is_empty() and \
					 scene.npc_dialog.frame.get("glyphs", []).size() > 40: break
		var frame: Dictionary = scene.npc_dialog.frame
		for why in frame.get("refusals", []): refusal("dialog_draw", why)
		await capture("04-dialog-attempt")
		var box_pixels := await dialog_box_pixels() if DisplayServer.get_name() != "headless" else 0
		var visual_result := "not_applicable_headless" if DisplayServer.get_name() == "headless" else \
			("drawn" if frame.get("open", false) and not frame.get("sprites", []).is_empty() and \
			 not frame.get("glyphs", []).is_empty() and box_pixels > 0 else "refused")
		mark("dialog_visual", visual_result,
			{"box_sprites": frame.get("sprites", []).size(),
			"glyphs": frame.get("glyphs", []).size(),
			"box_pixels": box_pixels,
			"camera_moving": scene.controller.dialog_status().get("camera_moving", false),
			"frame_open": frame.get("open", false),
			"paused": scene.paused, "menu_visible": scene.menu.visible,
			"font_ready": scene.npc_dialog.font_texture != null,
			"box_ready": scene.npc_dialog.box_texture != null})
		# The later prompt is absent during the opening text reveal. Capture it
		# before sending a choice so its text and cursor can be inspected too.
		var prompt_drawn := false
		for i in range(4000):
			advance()
			await process_frame
			if int(scene.npc_dialog.frame.get("cursor_glyph_quads", 0)) > 0:
				prompt_drawn = true
				break
		mark("dialog_prompt", "drawn" if prompt_drawn else "unavailable",
			{"cursor_glyph_quads": int(scene.npc_dialog.frame.get("cursor_glyph_quads", 0))})
		if prompt_drawn: await capture("04-dialog-prompt")
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
	var transition_before: int = scene.real.current
	var pending_levels := int(scene.controller.dialog_status().get("counts", {}).get("pending_levels", 0))
	if pending_levels > 0 and fe.screen() == "playing": advance(0, 180)
	mark("level_change", "changed" if scene.real.current != transition_before else "refused",
		{"script_enabled": scene.controller.get_play_capabilities().get("level_script_enabled", false),
		"pending_script_levels": pending_levels, "exit_target_exposed": false})
	await capture("10-level-change-attempt")
	save_json()
	var incomplete := false
	for row in rows:
		if str(row.result) in ["refused", "blocked_by_walk_and_talk"]:
			incomplete = true
	quit(1 if incomplete else 0)
