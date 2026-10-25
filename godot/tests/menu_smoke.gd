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
	var spent: Dictionary = service.spend_stat(0)
	check(spent.get("ok", false) and int(spent.base_stats[0]) == 11 and int(spent.stat_points) == 2, "stat handler")
	var skill: Dictionary = service.spend_skill(0, 1)
	check(skill.get("ok", false) and int(skill.skill_points) == 3 and int(skill.passive) == 3 and int(skill.rank_cost) == 10, "skill handler ignores displayed cost")
	service.reset()
	var granted: Dictionary = service.grant_level()
	check(int(granted.level) == 6 and int(granted.skill_points) == 5 and int(granted.stat_points) == 6, "level-up point bands")
	var opened: Dictionary = service.open_level_up()
	check(int(opened.acknowledged) == 6 and int(opened.page) == 5, "opening inventory for level-up acknowledges")
	check(service.press(2, 0x40).action == "none", "level-up page does not spend")
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
	for id in ["item-info-frame", "inventory-underbar", "inventory-localized-fields", "inventory-icon-pulse", "inventory-live-drop", "stat-spend-input", "skill-confirm-input", "pause-frames", "memory-card-frames", "shop-choice-centre-x", "exp-threshold-table", "mana-refill-offset"]:
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
	scene.queue_free()
	await process_frame
	print("MENU_SMOKE checks=%d failures=%d" % [checks, failures])
	quit(0 if failures == 0 else 1)
