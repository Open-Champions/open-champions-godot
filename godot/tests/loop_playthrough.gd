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

func inside_trigger(point: Vector3, target: Dictionary) -> bool:
	var lo: Vector3 = target.get("min", Vector3.ZERO)
	var hi: Vector3 = target.get("max", Vector3.ZERO)
	return point.x >= lo.x and point.x <= hi.x and point.y >= lo.y and point.y <= hi.y

func trigger_distance(point: Vector3, target: Dictionary) -> float:
	var lo: Vector3 = target.get("min", Vector3.ZERO)
	var hi: Vector3 = target.get("max", Vector3.ZERO)
	return maxf(maxf(lo.x - point.x, point.x - hi.x), 0.0) + \
		maxf(maxf(lo.y - point.y, point.y - hi.y), 0.0)

func walk_into_trigger(target: Dictionary, max_ticks: int) -> Dictionary:
	var began: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
	if inside_trigger(began, target): return {"entered": false, "ticks": 0}
	var lo: Vector3 = target.get("min", Vector3.ZERO)
	var hi: Vector3 = target.get("max", Vector3.ZERO)
	var aim := Vector3((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, 0)
	var moved := false
	var entered := false
	var ticks := 0
	var still := 0
	var without_progress := 0
	var best_distance := trigger_distance(began, target)
	while ticks < max_ticks and not scene.controller.script_level_request().get("queued", false):
		var before: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		var command := 0
		if before.x < lo.x or before.x > hi.x:
			command = 1 if before.x < aim.x else 2
		elif before.y < lo.y or before.y > hi.y:
			command = 4 if before.y < aim.y else 8
		advance(command)
		ticks += 1
		var after: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		if after.distance_to(before) > 0.001: moved = true; still = 0
		else: still += 1
		if moved and not inside_trigger(before, target) and inside_trigger(after, target): entered = true
		var distance := trigger_distance(after, target)
		if distance < best_distance - 0.25:
			best_distance = distance
			without_progress = 0
		else: without_progress += 1
		if still >= 20 or without_progress >= 600 or scene.stopped: break
		if command == 0 and ticks % 15 == 0 and not scene.controller.script_level_request().get("queued", false): break
	return {"entered": entered, "ticks": ticks, "navigation_stalled": without_progress >= 600}

func approach_use(target: Dictionary, max_ticks: int) -> Dictionary:
	var origin: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
	var goal: Vector3 = target.get("position", Vector3.ZERO)
	var best := origin.distance_to(goal)
	var stale := 0
	var ticks := 0
	while ticks < max_ticks and not scene.stopped:
		var at: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		if absf(at.x - goal.x) <= 48.0 and absf(at.y - goal.y) <= 48.0 and goal.z >= at.z and goal.z <= at.z + 60.0:
			game_key(KEY_E)
			advance(0, 2)
			return {"pressed": true, "ticks": ticks}
		var command := 0
		if absf(at.x - goal.x) > 24.0: command = 1 if at.x < goal.x else 2
		elif absf(at.y - goal.y) > 24.0: command = 4 if at.y < goal.y else 8
		if command == 0: break
		advance(command)
		ticks += 1
		var distance: float = scene.controller.get_observation().get("position", Vector3.ZERO).distance_to(goal)
		if distance < best - 0.25: best = distance; stale = 0
		else: stale += 1
		if stale >= 600: break
	return {"pressed": false, "ticks": ticks, "navigation_stalled": stale >= 600}

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
	# Step 6: walk the native player into a real Trigger's inclusive x/y box.
	var transition_before: int = scene.real.current
	var exit_targets: Array = scene.controller.script_exit_targets()
	var pending: Dictionary = scene.controller.script_level_request()
	var walk_attempts := 0
	var walked_into := false
	var queued_by_walk := false
	var entered_without_change := 0
	var navigation_stalls := 0
	var sources_examined := 1
	var candidate_indices: Array[int] = []
	for index in range(scene.real.levels.size()):
		var level: Dictionary = scene.real.levels[index]
		var objects: PackedByteArray = scene.real.read_entry(level.gob, level.stem + ".lmp", "objects.ob")
		if objects.is_empty(): continue
		var inventory: Dictionary = scene.controller.inspect_exit_records(objects)
		if inventory.get("ok", false) and int(inventory.get("triggers", 0)) >= 4:
			candidate_indices.append(index)
	var sources: Array[int] = [-1]
	sources.append_array(candidate_indices)
	for index in sources:
		if index >= 0:
			if index == transition_before: continue
			if not scene.load_real_level(index, true): continue
			scene.start_play()
			help_close()
			if scene.paused and scene.session_started: scene.resume_session()
			sources_examined += 1
			exit_targets = scene.controller.script_exit_targets()
		pending = scene.controller.script_level_request()
		if pending.get("queued", false): continue
		var walk_targets: Array = exit_targets.filter(func(t): return t.get("walk_in", false))
		if walk_targets.size() < 4: continue
		var origin: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		walk_targets.sort_custom(func(a, b): return a.position.distance_squared_to(origin) < b.position.distance_squared_to(origin))
		for target in walk_targets:
			walk_attempts += 1
			var dispatches_before := int(scene.controller.script_trigger_status().get("queued_dispatches", 0))
			var walk: Dictionary = walk_into_trigger(target, 45000)
			if walk.get("navigation_stalled", false): navigation_stalls += 1
			walked_into = bool(walk.get("entered", false))
			pending = scene.controller.script_level_request()
			var dispatch: Dictionary = scene.controller.script_trigger_status()
			queued_by_walk = walked_into and bool(pending.get("queued", false)) and \
				int(dispatch.get("queued_dispatches", 0)) > dispatches_before and \
				int(dispatch.get("queued_record", -1)) == int(target.get("index", -2)) and \
				str(dispatch.get("queued_destination", "")) == str(pending.get("name", ""))
			if walked_into and not pending.get("queued", false): entered_without_change += 1
			if pending.get("queued", false): break
		if queued_by_walk:
			transition_before = scene.real.current
			break
	var record_before: Dictionary = scene.controller.get_observation()
	var trigger_status: Dictionary = scene.controller.script_trigger_status()
	var quests_before: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var transition_ok: bool = queued_by_walk and bool(pending.get("queued", false)) and bool(scene._apply_level_change())
	var record_after: Dictionary = scene.controller.get_observation()
	var start: Dictionary = scene.controller.get_terrain().get("start", {})
	var arrived_position: Vector3 = record_after.get("position", Vector3.ZERO)
	var marker_position: Vector3 = start.get("position", Vector3.ZERO)
	var arrival_at_marker: bool = transition_ok and start.has("position") and \
		arrived_position.distance_to(marker_position) < 0.01
	var quests_after: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var record_carried: bool = transition_ok and scene.last_campaign_carried and scene.last_campaign_image_restored \
		and str(record_after.get("name", "")) == str(record_before.get("name", "")) \
		and int(record_after.get("gold", -1)) == int(record_before.get("gold", -2)) \
		and int(record_after.get("experience", -1)) == int(record_before.get("experience", -2)) \
		and int(record_after.get("level", -1)) == int(record_before.get("level", -2))
	mark("level_change", "changed" if record_carried and arrival_at_marker and scene.real.current != transition_before else "refused",
		{"level_index_before": transition_before, "level_index_after": scene.real.current,
		"exit_targets": exit_targets.size(), "walk_attempts": walk_attempts, "walked_into": walked_into,
		"queued_by_walk": queued_by_walk,
		"entered_without_change": entered_without_change,
		"navigation_stalls": navigation_stalls,
		"trigger_samples": int(trigger_status.get("samples", 0)),
		"trigger_dispatches": int(trigger_status.get("dispatches", 0)),
		"trigger_queued_dispatches": int(trigger_status.get("queued_dispatches", 0)),
		"trigger_missing": int(trigger_status.get("missing", 0)),
		"arrival_at_marker": arrival_at_marker,
		"arrival_pre_search": bool(start.get("pre_search", false)),
		"arrival_rank": int(start.get("rank", 0)),
		"sources_examined": sources_examined,
		"pending_script_level": pending.get("queued", false), "campaign_carried": scene.last_campaign_carried,
		"campaign_image_restored": scene.last_campaign_image_restored,
		"record_carried": record_carried, "quest_vars_before": int(quests_before.get("vars", 0)),
		"quest_vars_after": int(quests_after.get("vars", 0))})
	await capture("10-level-change")
	# Normal E/use on an owner-supplied level record; the selected action is
	# never invoked by the diagnostic menu.
	var push_sources: Array[int] = []
	var portal_sources: Array[int] = []
	for index in range(scene.real.levels.size()):
		var level: Dictionary = scene.real.levels[index]
		var objects: PackedByteArray = scene.real.read_entry(level.gob, level.stem + ".lmp", "objects.ob")
		if objects.is_empty(): continue
		var inventory: Dictionary = scene.controller.inspect_exit_records(objects)
		if not inventory.get("ok", false): continue
		if int(inventory.get("pushes", 0)) > 0: push_sources.append(index)
		if int(inventory.get("portals", 0)) > 0: portal_sources.append(index)
	var physical_push := false
	var push_attempts := 0
	for index in push_sources:
		if push_attempts >= 6 or physical_push: break
		if index != scene.real.current:
			if not scene.load_real_level(index, true): continue
			scene.start_play()
			help_close()
			if scene.paused and scene.session_started: scene.resume_session()
		var targets: Array = scene.controller.script_exit_targets().filter(func(t): return t.get("physical_use", false))
		var origin: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		targets.sort_custom(func(a, b): return a.position.distance_squared_to(origin) < b.position.distance_squared_to(origin))
		for target in targets:
			push_attempts += 1
			var count_before := int(scene.controller.script_use_status().get("uses", 0))
			var approach: Dictionary = approach_use(target, 2500)
			var use: Dictionary = scene.controller.script_use_status()
			physical_push = bool(approach.get("pressed", false)) and int(use.get("uses", 0)) > count_before and \
				int(use.get("record", -1)) == int(target.get("index", -2))
			if physical_push or push_attempts >= 6: break
	mark("push_trigger_use", "used" if physical_push else "refused",
		{"candidate_levels": push_sources.size(), "attempts": push_attempts, "physical_use": physical_push,
		"use_refusal": scene.controller.script_use_status().get("refusal", "")})
	await capture("11-push-use")
	# Discover two real gate levels, then request travel from the second to a
	# discovered destination. The host menu exposes explicit destination choice;
	# the research did not dynamically compare its full pad input machine.
	var gate_discoveries := 0
	var gate_pressed := 0
	var gate_menu := false
	var portal_arrived := false
	var portal_rank := -1
	var portal_at_spot := false
	var gate_request_at_181 := false
	var gate_advance_calls := 0
	var gate_ready_before := -1
	var gate_ready_after := -1
	for index in portal_sources:
		if gate_discoveries >= 2: break
		if not scene.load_real_level(index, true): continue
		scene.start_play()
		help_close()
		if scene.paused and scene.session_started: scene.resume_session()
		var targets: Array = scene.controller.script_exit_targets().filter(func(t): return t.get("kind", "") in ["Portal", "PortalHurt"])
		var origin: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		targets.sort_custom(func(a, b): return a.position.distance_squared_to(origin) < b.position.distance_squared_to(origin))
		for target in targets:
			var approach: Dictionary = approach_use(target, 2500)
			if not approach.get("pressed", false): continue
			gate_pressed += 1
			var gate: Dictionary = scene.controller.script_gate_status()
			if str(gate.get("prompt", "")) == "JumpGateVirgin" or gate.get("menu_open", false) or int(gate.get("travel_ticks", 0)) > 0:
				gate_discoveries += 1
				gate_menu = bool(gate.get("menu_open", false))
				break
	if gate_menu:
		var gate: Dictionary = scene.controller.script_gate_status()
		var choices: Array = gate.get("destinations", [])
		if not choices.is_empty() and scene.controller.script_gate_confirm(str(choices[0])).get("ok", false):
			gate_ready_before = int(scene.controller.get_observation().get("ready_ticks", -1))
			while gate_advance_calls < 360 and not scene.controller.script_gate_status().get("request_written", false):
				advance()
				gate_advance_calls += 1
			gate_ready_after = int(scene.controller.get_observation().get("ready_ticks", -1))
			var pending_gate: Dictionary = scene.controller.script_level_request()
			var travel: Dictionary = scene.controller.script_gate_status()
			gate_request_at_181 = pending_gate.get("queued", false) and \
				int(travel.get("travel_ticks", -1)) == 181 and travel.get("request_written", false) and \
				int(pending_gate.get("orientation", -1)) == 255
			if gate_request_at_181 and scene._apply_level_change():
				if not scene.session_started: scene.start_play()
				if scene.session_started and not scene.stopped: scene.set_paused(false)
				var arrival: Dictionary = scene.controller.get_terrain().get("start", {})
				portal_rank = int(arrival.get("rank", -1))
				portal_at_spot = scene.controller.get_observation().get("position", Vector3.ZERO).distance_to(
					arrival.get("position", Vector3.ZERO)) < 0.01
				portal_arrived = bool(arrival.get("portal_free_spot", false)) and portal_rank == 3 and portal_at_spot
	mark("portal_arrival", "arrived" if portal_arrived else "refused",
		{"candidate_levels": portal_sources.size(), "gate_discoveries": gate_discoveries,
		"physical_gate_presses": gate_pressed, "menu_opened": gate_menu,
		"request_at_181": gate_request_at_181, "gate_advance_calls": gate_advance_calls,
		"gate_ready_ticks": gate_ready_after - gate_ready_before,
		"portal_free_spot": portal_arrived, "rank": portal_rank,
		"position_matches_start": portal_at_spot,
		"object_grid_verified": false, "constructor_seed_verified": false})
	await capture("12-portal-arrival")
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
	save_json()
	var incomplete := false
	for row in rows:
		if str(row.result) in ["refused", "blocked_by_walk_and_talk"]:
			incomplete = true
	quit(1 if incomplete else 0)
