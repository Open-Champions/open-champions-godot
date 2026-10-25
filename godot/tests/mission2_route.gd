extends SceneTree

# Private acceptance only. A synthetic offer controls the hub mission bytes;
# the menu table, destination archive, scripts, Player and World are real.
# Other three levels are direct diagnostics, never an asserted story route.
var args := {}
var scene
var checks := 0
var failures := 0
var result := {"synthetic_hub_offer": true, "playable_route_verified": false, "levels": []}
const LEVELS := ["innovat1", "inmine1", "inmine2", "innovat2"]

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var parts := arg.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	run.call_deferred()

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAILED: " + label)

func index_of(stem: String) -> int:
	for i in range(scene.real.levels.size()):
		if str(scene.real.levels[i].stem) == stem: return i
	return -1

func capture(name: String) -> void:
	if DisplayServer.get_name() == "headless": return
	for i in range(3): await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and image.save_png(str(args.out).path_join(name + ".png")) == OK, "rendered capture")

func menu_fixture(name: String) -> bool:
	var image := FileAccess.get_file_as_bytes(str(args["fixture-dir"]).path_join(name + ".scr"))
	return scene.controller.enable_level_script(image, PackedByteArray(), false, {}, -1, "hub").get("ok", false)

func run() -> void:
	if not args.has_all(["game-folder", "out", "fixture-dir"]):
		quit(2)
		return
	root.size = Vector2i(1120, 780)
	root.content_scale_size = root.size
	scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = str(args.out).path_join("library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = str(args.out).path_join("cache")
	check(scene.open_game_folder(str(args["game-folder"])), "selected folder opens")
	scene.level_script.set_pressed_no_signal(true)
	# The same normal character-creation flow used by loop_playthrough.gd.
	var fe = scene.frontend
	fe.options_path = str(args.out).path_join("options.img")
	if FileAccess.file_exists(fe.options_path): DirAccess.remove_absolute(fe.options_path)
	fe.boot()
	if fe.screen() == "boot-card": fe.tap(0x40)
	for i in range(239): fe.step(0)
	if fe.screen() == "intro-movie":
		for i in range(30): fe.step(0)
		fe.tap(0x40)
	if fe.screen() == "main":
		for i in range(4): fe.tap(0x40)
		if fe.screen() == "difficulty": fe.tap(0x40)
		if fe.screen() == "char-class":
			for i in range(11): fe.tap(0x4000)
			fe.tap(0x40)
		if fe.screen() == "char-appearance": fe.tap(0x40)
		if fe.screen() == "char-ability":
			for i in range(20): fe.tap(0x2000)
			fe.tap(0x40)
		if fe.screen() == "char-name":
			fe.type_name("Probe")
			fe.tap(0x4000)
			fe.tap(0x40)
	check(scene.real.current == index_of("hub") and scene.play_mode, "character enters real hub")
	var before: Dictionary = scene.controller.get_observation()
	check(before.get("player_ready", false), "normal player created")
	check(menu_fixture("locked"), "controlled completed-mission-1 state loads")
	var menu_before: Dictionary = scene.controller.script_mission_menu_state()
	check(not scene.controller.script_mission_menu_tick(0x1040, -1).get("ok", false), "unresearched button chord refuses")
	check(scene.controller.script_mission_menu_state() == menu_before, "refused chord leaves menu unchanged")
	scene._refresh_mission_menu()
	check(scene.mission_story_buttons[1].disabled, "locked mission-2 button is disabled")
	await capture("00-menu-locked")
	check(not scene.mission_menu_select_story(2), "mission 1 completion does not select locked mission 2")
	var inventory_image := FileAccess.get_file_as_bytes(str(args["fixture-dir"]).path_join("inventory.campaign"))
	check(scene.controller.load_campaign_bytes(inventory_image).get("ok", false), "authored inventory campaign loads")
	for i in range(17): scene.mission_menu_frame()
	scene._refresh_mission_menu()
	check(scene.mission_story_buttons[1].disabled, "unsupported inventory disables mission-2 entry")
	check(not scene.mission_menu_select_story(2), "unsupported inventory cannot queue a story selection")
	var inventory_before: Dictionary = scene.controller.script_mission_menu_state()
	check(not scene.controller.script_mission_menu_tick(0x40, -1).get("ok", false), "native confirm refuses unknown inventory migration")
	check(scene.controller.script_mission_menu_state() == inventory_before, "inventory refusal leaves menu unchanged")
	await capture("00b-inventory-refused")
	check(menu_fixture("offered"), "controlled mission-2 offer loads")
	for i in range(17): scene.mission_menu_frame()
	check(scene.controller.script_mission_menu_state().get("row", -1) == 1, "menu selects second row")
	scene._refresh_mission_menu()
	await capture("01-menu")
	var campaign_before: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	scene.mission_story_buttons[1].pressed.emit()
	check(scene.mission_menu_target == 1, "available second story button submits its selection")
	var launched := false
	for i in range(40):
		var tick: Dictionary = scene.mission_menu_frame()
		if tick.get("launched", false):
			launched = true
			break
	check(launched and scene.controller.script_level_request().get("name", "") == LEVELS[0], "real table requests documented mission-2 entry")
	check(scene._apply_level_change() and scene.real.current == index_of(LEVELS[0]), "menu request loads first story archive")
	check(scene.last_campaign_carried and scene.last_campaign_image_restored, "campaign image restored before native act-change handling")
	var after: Dictionary = scene.controller.get_observation()
	check(after.get("player_class", -1) == before.get("player_class", -2) and after.get("experience", -1) == before.get("experience", -2) and after.get("gold", -1) == before.get("gold", -2), "player identity, experience and gold carried")
	var campaign_after: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	check(int(campaign_after.get("act", -1)) != int(campaign_before.get("act", -1)) and int(campaign_after.get("act_resets", 0)) > 0, "documented mission-2 entry changes act and runs native reset")
	check(int(campaign_before.get("quest_lines", 0)) > 0 and int(campaign_after.get("quest_lines", -1)) == 0, "act reset clears the authored old-act quest")
	check(campaign_after.get("mission_complete_bits", -1) == campaign_before.get("mission_complete_bits", -2) and scene.controller.script_mission_menu_state().get("second_story_available", false), "act reset retains completion and mission-2 availability")
	result["cross_act_reset"] = int(campaign_after.get("act_resets", 0)) > 0
	result["menu_loaded_entry"] = launched and scene.real.current == index_of(LEVELS[0])
	scene._observe(after)
	await capture("02-entry")
	for slot in range(LEVELS.size()):
		if slot > 0:
			check(scene.load_real_level(index_of(LEVELS[slot])), "direct diagnostic level loads")
			scene.start_play()
		var counts: Dictionary = scene.controller.script_command_counts()
		check(counts.get("requires_complete_commands", false), "mission-2 command boundary enabled")
		check(int(counts.get("faults", -1)) == 0, "no VM fault")
		check(scene.controller.get_observation().get("fight_player_ready", false), "real Player bound to level World")
		var ticks_before := int(scene.controller.get_observation().get("ready_ticks", 0))
		var world_ticks_before := int(scene.controller.get_observation().get("fight_counts", {}).get("ticks", -1))
		for tick in range(8):
			var advanced: Dictionary = scene.advance_step()
			check(advanced.get("ok", false), "real level ready tick runs")
		var ticks_after := int(scene.controller.get_observation().get("ready_ticks", 0))
		var world_after: Dictionary = scene.controller.get_observation()
		var world_ticks_after := int(world_after.get("fight_counts", {}).get("ticks", -1))
		check(ticks_after > ticks_before, "real Player/World ready count advances")
		check(not world_after.get("fight_failed", true) and world_ticks_after > world_ticks_before,
			"real Player/World runs without a runtime failure")
		var records := int(counts.get("objects_registered", 0)) + int(counts.get("objects_not_created", 0))
		var row := {"slot": slot, "direct_diagnostic": slot > 0, "records": records,
			"built": counts.get("objects_built", 0), "unbuilt": counts.get("objects_unbuilt", 0),
			"not_created": counts.get("objects_not_created", 0), "refusal_stops": counts.get("refusal_stops", 0),
			"refused": counts.get("refused", {}), "script_stopped": counts.get("script_stopped", false),
			"quest_lines": counts.get("campaign", {}).get("quest_lines", 0),
			"ready_ticks": ticks_after - ticks_before,
			"world_ticks": world_ticks_after - world_ticks_before,
			"player_bound": scene.controller.get_observation().get("fight_player_ready", false)}
		if row.script_stopped:
			check(not scene.controller.script_level_request().get("queued", false), "stopped script cannot leave")
		for target in scene.controller.script_exit_targets():
			check(not target.get("script_action", false) and not scene.controller.script_run_exit(int(target.index)).get("ok", false), "unproved exit remains refused")
		result.levels.push_back(row)
		await capture("level-%d" % slot)
	result["checks"] = checks
	result["failures"] = failures
	var file := FileAccess.open(str(args.out).path_join("result.json"), FileAccess.WRITE)
	if file != null: file.store_string(JSON.stringify(result, "  ", true) + "\n")
	print("MISSION2_RESULT " + JSON.stringify(result))
	scene.free()
	scene = null
	await process_frame
	quit(0 if failures == 0 else 1)
