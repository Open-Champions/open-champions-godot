extends SceneTree

var checks := 0
var failures := 0

func _initialize() -> void:
	call_deferred("run")

func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(message)

# A complete skills_screen tree row and pad; the binding defaults none of them.
func skill_row(extra: Dictionary) -> Dictionary:
	var row := {"kind": 0, "need": 1, "ident": 1, "pre_kind": 0, "pre": -1, "page": 5, "resource": true, "in_table": true, "cursor": "inside", "edge": 0x40, "held": 0, "panel30": 0}
	row.merge(extra, true)
	return row

func key(code: int, pressed := true) -> InputEventKey:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = pressed
	return event

func run() -> void:
	var scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	scene.component_factory = func(_id): return null
	root.add_child(scene)
	await process_frame
	var menus = scene.game_menus
	var service = menus.service if menus != null else null
	check(service != null, "menu service is registered")
	if service == null:
		print("MENU_SMOKE checks=%d failures=%d" % [checks, failures])
		quit(1)
		return
	var state: Dictionary = service.state()
	check(state.get("ok", false) and int(state.gold) == 500 and int(state.bag_count) == 2, "seeded gold and bag")
	check(service.press(2, 0x40).action == "equip" and int(service.state().equipment_index) == 0, "cross equips blade")
	check(int(service.record_word(0x43f0).value) == 0x4d8af0 + 0x130, "equip writes player record")
	check(service.press(2, 0x40).action == "unequip" and int(service.state().equipment_index) == -1, "cross unequips blade")
	check(int(service.record_word(0x43f0).value) == 0, "unequip clears player record")
	service.reset()
	check(service.press(0, 0x80).action == "buy" and int(service.state().gold) == 441, "shop buy charges 59")
	service.reset()
	check(service.press(1, 0x80).action == "sell" and int(service.state().gold) == 501, "shop sell pays 1")
	service.reset()
	service.press(2, 0x40)
	check(service.press(1, 0x80).action == "confirm" and int(service.state().gold) == 500, "equipped sell does not sell")
	service.reset()
	check(service.press(3, 0).action == "workshop" and int(service.state().bag_count) == 2, "workshop does not change the bag")
	service.set_page(3)
	check(service.press(2, 0x40).action == "none" and int(service.state().stat_points) == 3, "stats page does not spend")
	var skill: Dictionary = service.spend_skill(0, 1)
	check(not skill.get("ok", false) and str(skill.get("error", "")).contains("not the skills_screen sender") and int(service.state().skill_points) == 4, "bare spend is not the skill screen")
	var off_page: Dictionary = service.skill_screen(skill_row({"page": 3}))
	check(not off_page.get("ok", false) and str(off_page.get("error", "")).contains("page 5") and int(service.state().skill_points) == 4, "skill screen needs the level-up page")
	service.set_page(5)
	var no_rules: Dictionary = service.skill_screen(skill_row({}))
	check(not no_rules.get("ok", false) and str(no_rules.get("error", "")).contains("level-up rules") and int(service.state().skill_points) == 4, "skill screen recalc refuses without the executable rules")
	for missing in ["kind", "need", "ident", "pre_kind", "pre", "page", "resource", "in_table", "cursor", "edge", "held", "panel30"]:
		var partial := skill_row({})
		partial.erase(missing)
		var refused: Dictionary = service.skill_screen(partial)
		check(not refused.get("ok", false) and str(refused.get("error", "")).contains("needs " + missing) and int(service.state().skill_points) == 4, "skill row without " + missing + " refuses")
	check(not service.skill_screen(skill_row({"gate": 0})).get("ok", false), "the latch word is not a caller field")
	check(not service.state().local_menu_known, "local menu state starts unknown")
	service.set_local_menu_open(false)
	service.reset()
	check(service.state().local_menu_known, "a stated menu state survives a new ledger")
	var thresholds := PackedInt32Array()
	for index in range(81):
		thresholds.append(index * 100)
	var formulas := PackedInt32Array()
	for class_index in range(8):
		for word in [0, 0x3f800000, 0x3f800000, 0x40000000, 0x40400000, 0]:
			formulas.append(word)
	check(service.configure_levelup(thresholds, formulas).get("ok", false), "authored XP rules load")
	var before_invalid: PackedByteArray = service.record_bytes()
	var invalid_adopt: Dictionary = service.adopt({"level": 81, "stats": [20, 20, 20, 20]})
	check(not invalid_adopt.get("ok", false), "level-81 adoption refuses")
	check(service.record_bytes() == before_invalid and service.state().get("ok", false), "refused adoption preserves record and usable state")
	var before_invalid_save: PackedByteArray = service.save_bytes().bytes
	var invalid_save := before_invalid_save.duplicate()
	invalid_save[24] = 81
	check(not service.load_bytes(invalid_save).get("ok", false), "level-81 restoration refuses")
	check(service.save_bytes().bytes == before_invalid_save and service.state().get("ok", false), "refused restoration preserves ledger and usable state")
	var xp: Dictionary = service.give_xp(500)
	check(xp.get("ok", false) and int(xp.levels_gained) == 1 and int(xp.level) == 6 and int(xp.experience) == 500, "exact XP threshold")
	check(int(xp.skill_points) == 5 and int(xp.stat_points) == 6 and int(xp.max_hp) == 0x424c0000 and int(xp.max_mana) == 0x41880000, "level resource and point awards")
	check(int(xp.acknowledged) == 4 and int(xp.hud_xp_base) == 300 and int(xp.hud_next_xp) == 400, "pending level HUD interval follows acknowledged level")
	check(int(xp.level_message_calls) == 1 and xp.level_message_stored and int(xp.level_message_frames) == 360 and int(xp.level_message_force) == 0 and int(xp.level_message_undecided) == 0, "level step asks for the 360-frame force-0 message")
	var opened: Dictionary = service.open_level_up()
	check(int(opened.acknowledged) == 6 and int(opened.page) == 5, "opening inventory for level-up acknowledges")
	check(int(opened.hud_xp_base) == 500 and int(opened.hud_next_xp) == 600, "acknowledged level advances HUD interval")
	check(service.press(2, 0x40).action == "none", "level-up page does not spend as an inventory cell")
	check(not service.press_stat(2, 3, 0x40, 0, 2999).sent, "stat timer gate")
	var stat: Dictionary = service.press_stat(2, 3, 0x40, 0, 3000)
	check(stat.get("ok", false) and stat.get("sent", false) and int(stat.base_stats[3]) == 14 and int(stat.stat_points) == 5, "stat row sends and spends")
	var sent: Dictionary = service.skill_screen(skill_row({}))
	check(sent.get("ok", false) and sent.get("sent", false) and int(sent.send_kind) == 0 and int(sent.send_value) == 1 and int(sent.skill_points) == 4, "passive row sends the mask and spends one point")
	var outside: Dictionary = service.skill_screen(skill_row({"cursor": "outside"}))
	check(outside.get("ok", false) and not outside.get("sent", false) and int(service.state().skill_points) == 4, "a cursor outside the tree does not send")
	var latched: Dictionary = service.skill_screen(skill_row({"held": 2}))
	check(latched.get("ok", false) and not latched.get("sent", false) and int(latched.latch) == 2, "held bit 2 on page 5 latches")
	var kept: Dictionary = service.skill_screen(skill_row({"panel30": 1}))
	check(kept.get("ok", false) and not kept.get("sent", false) and int(kept.latch) == 2 and int(service.state().skill_points) == 4, "the ledger keeps the latch word")
	check(int(service.skill_screen(skill_row({"cursor": "outside", "panel30": 1})).latch) == 2, "an exit that would not change the latch leaves it")
	var edge: Dictionary = service.skill_screen(skill_row({"kind": 1, "ident": 95, "cursor": "edge", "virtual_float": 0}))
	check(not edge.get("ok", false) and str(edge.get("error", "")).contains("cursor-box"), "cursor-box edges stay refused")
	var kill: Dictionary = service.give_kill_xp(0x42000000, 0x41f00000)
	check(kill.get("ok", false) and int(kill.award) > 0 and int(kill.experience) == 500 + int(kill.award), "authored solo kill award reaches record")
	check(not service.spend_skill(0, 1).get("ok", false), "bare skill spend stays refused")
	var stats_snapshot: PackedByteArray = service.save_bytes().bytes
	var missing_half := stats_snapshot.slice(0, stats_snapshot.size() - 4)
	var oversized_half := stats_snapshot.duplicate()
	oversized_half.encode_u32(oversized_half.size() - 4, 256)
	for malformed in [missing_half, oversized_half]:
		check(service.load_bytes(stats_snapshot).get("ok", false), "restore stat fixture before exhaustion")
		var points := int(service.state().stat_points)
		for point in range(points):
			check(service.press_stat(2, 3, 0x40, 0, 3000).sent, "exhaust current stat points")
		service.set_page(2)
		var exhausted_record: PackedByteArray = service.record_bytes()
		var exhausted_save: PackedByteArray = service.save_bytes().bytes
		check(int(service.state().stat_points) == 0 and ((int(service.record_word(0xec).value) >> 16) & 0xff) == 1, "exhaustion switches sender half")
		check(not service.load_bytes(malformed).get("ok", false), "invalid version-4 tail refuses")
		check(service.record_bytes() == exhausted_record and service.save_bytes().bytes == exhausted_save, "tail refusal preserves full record and ledger metadata")
		var retained: Dictionary = service.state()
		check(retained.get("ok", false) and int(retained.page) == 2 and not retained.live_record, "tail refusal preserves page and ownership with readable state")
		check(service.give_xp(0).get("ok", false) and service.save_bytes().bytes == exhausted_save, "tail refusal preserves effective-state validity and XP operation")
		check(service.load_bytes(stats_snapshot).get("ok", false) and service.press_stat(2, 3, 0x40, 0, 3000).sent and int(service.state().stat_points) == points - 1, "valid restore and stat spend work after tail refusal")
	var imported_snapshot := stats_snapshot.duplicate()
	imported_snapshot.encode_u32(imported_snapshot.size() - 68, 0)
	imported_snapshot.encode_u32(imported_snapshot.size() - 64, 1)
	imported_snapshot.encode_u32(imported_snapshot.size() - 60, 0)
	check(service.load_bytes(imported_snapshot).get("ok", false) and service.state().live_record, "authored companion restores imported relationship")
	for malformed in [missing_half, oversized_half]:
		service.set_page(2)
		var imported_record: PackedByteArray = service.record_bytes()
		var imported_save: PackedByteArray = service.save_bytes().bytes
		check(not service.load_bytes(malformed).get("ok", false), "invalid tail refuses over imported relationship")
		check(service.record_bytes() == imported_record and service.save_bytes().bytes == imported_save and service.state().get("ok", false) and service.state().live_record and int(service.state().page) == 2, "tail refusal retains imported relationship and readable metadata")
		check(not service.give_xp(0).get("ok", false) and service.save_bytes().bytes == imported_save, "imported effective-state refusal remains intact")
		check(service.open_level_up().get("ok", false) and int(service.state().page) == 5 and service.state().live_record, "supported imported menu operation works after refusal")
	var no_hud_level: Dictionary = service.adopt({"level": 1, "acknowledged": 0, "stats": [10, 10, 10, 10]})
	check(no_hud_level.get("ok", false) and no_hud_level.get("xp_ready", false) and int(no_hud_level.hud_next_xp) == 0, "unacknowledged imported record keeps XP rules without inventing a HUD interval")
	service.reset()
	service.set_scheme("circle")
	check(int(service.state().shop_mask) == 0 and service.press(0, 0x80).action == "none" and int(service.state().gold) == 500, "circle scheme has no shop button")
	check(service.press(2, 0x20).action == "equip", "circle equip bit")
	service.reset()
	var drawn: Dictionary = service.frame("inventory")
	check(drawn.get("ok", false) and int(drawn.texts[0].x) == 17 and int(drawn.texts[0].y) == 89 and str(drawn.texts[0].text) == "blade", "item name sits on the hit cell")
	check(drawn.sprites.size() == 22 and drawn.selected_texts.size() == 1 and drawn.icons.is_empty(), "bounded box and cursor art with authored item text; no invented icon")
	check(service.frame("pause").texts.is_empty(), "pause draws no invented frame")
	check(str(service.frame("stats").texts[0].text) == "5", "level uses the executed header position")
	var ids := {}
	for gap in service.gaps().gaps:
		ids[str(gap.id)] = true
	check(not ids.has("skill-spend-sender"), "executed skill sender is no longer a refusal")
	for id in ["item-info-frame", "inventory-underbar", "inventory-localized-fields", "inventory-icon-pulse", "inventory-live-drop", "skill-confirm-input", "skill-virtual-bodies", "skill-cursor-edges", "pause-frames", "memory-card-frames"]:
		check(ids.has(id), "refusal " + id)
	service.press(2, 0x40)
	var dir := "user://menu-smoke-%d" % OS.get_process_id()
	DirAccess.make_dir_recursive_absolute(dir)
	var path := dir.path_join("companion")
	check(menus.save_companion(path), "companion save")
	service.reset()
	check(int(service.state().equipment_index) == -1, "reset clears equipment")
	check(menus.load_companion(path) and int(service.state().equipment_index) == 0 and int(service.state().gold) == 500, "companion load")
	check(menus.load_companion(dir.path_join("missing")) and int(service.state().gold) == 500 and int(service.state().equipment_index) == -1, "missing companion reseeds")
	scene.start_diagnostic()
	check(scene.session_started and not scene.paused, "diagnostic is running")
	scene.actions.inventory.pressed.emit()
	check(menus.open and menus.screen == "inventory", "in-game inventory action opens equipment screen")
	menus.close_menu()
	check(scene.handle_input_event(key(KEY_M)) and menus.open, "host M opens the menu")
	check(scene.handle_input_event(key(KEY_J)) and int(service.state().equipment_index) == 0, "host J uses the equip mask")
	check(scene.handle_input_event(key(KEY_ESCAPE)) and not menus.open and not scene.paused, "escape closes the menu and leaves the session running")
	check(scene.handle_input_event(key(KEY_ESCAPE)) and scene.paused, "escape still pauses when the menu is closed")
	# A combat fixture has its own active player. An imported menu ledger may
	# coexist with it, but must not replace its observed HP and mana.
	scene.play_mode = true
	scene.session_started = true
	menus.live_imported = true
	scene.raw_observation = {"combat_enabled": true, "hp": 25.0, "max_hp": 100.0, "mana": 40.0, "max_mana": 100.0, "ready_ticks": 7}
	scene.refresh_menu_hud()
	var health_right := -1
	var mana_right := -1
	var has_xp := false
	for record in scene.play_hud.records:
		if str(record.get("resource", "")).begins_with("bar_red"):
			health_right = int(record.clip.right)
		elif str(record.get("resource", "")).begins_with("bar_blue"):
			mana_right = int(record.clip.right)
		elif str(record.get("resource", "")).begins_with("bar_green"):
			has_xp = true
	check(health_right == 51 and mana_right == 66 and not has_xp, "combat HUD keeps active player resources and refuses unrelated ledger XP")
	scene.queue_free()
	await process_frame
	print("MENU_SMOKE checks=%d failures=%d" % [checks, failures])
	quit(0 if failures == 0 else 1)
