extends SceneTree

# One real level, then each documented menu. Prints counts only.
var folder := ""
var capture_dir := ""
var cache := ""

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="):
			folder = argument.trim_prefix("--game-folder=")
		elif argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
		elif argument.begins_with("--cache="):
			cache = argument.trim_prefix("--cache=")
	call_deferred("run")

func run() -> void:
	if folder.is_empty() or capture_dir.is_empty() or cache.is_empty():
		printerr("game folder, capture dir and cache are required")
		quit(2)
		return
	if DisplayServer.get_name() == "headless":
		printerr("headless display cannot capture")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	root.size = Vector2i(1400, 1000)
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	await process_frame
	scene.real.cache_dir = cache
	if scene.components.has("library"):
		scene.components.library.history_path = cache.path_join("level-library.json")
	if not scene.open_game_folder(folder):
		printerr("game folder refused")
		quit(1)
		return
	var font_ok: bool = scene.load_real_font(0)
	var loaded := -1
	var tried := 0
	for index in range(mini(8, scene.real.levels.size())):
		tried += 1
		var result: Dictionary = scene.real.load_level(index)
		if result.get("ok", false):
			loaded = index
			break
	print("fonts ", scene.real.fonts.size(), " font_loaded ", font_ok, " levels ", scene.real.levels.size(), " tried ", tried, " loaded_index ", loaded)
	if loaded < 0:
		printerr("no level loaded")
		quit(1)
		return
	scene.actions.play.pressed.emit()
	await process_frame
	if not scene.play_mode:
		printerr("play did not start")
		quit(1)
		return
	for _tick in range(20):
		scene.advance_step()
	if scene.game_menus != null and scene.components.has("art"):
		scene.game_menus.sync_font(scene.components.art)
	if scene.game_menus == null or scene.game_menus.small_font_texture == null:
		printerr("documented small menu font did not load")
		quit(1)
		return
	if scene.game_menus.inventory_art == null or scene.game_menus.inventory_art.load_art("inventry.lmp", "inventrybox3.tex", true).is_empty():
		printerr("documented inventory box art did not load")
		quit(1)
		return
	var inventory_frame: Dictionary = scene.game_menus.service.frame("inventory")
	if inventory_frame.get("sprites", []).size() != 20 or inventory_frame.get("selected_texts", []).size() != 1 or inventory_frame.get("frames", []).size() != 1:
		printerr("bounded inventory draw was not submitted")
		quit(1)
		return
	var shop_frame: Dictionary = scene.game_menus.service.frame("shop")
	if shop_frame.get("sprites", []).size() != 1:
		printerr("shop submitted a texture without a pinned tint")
		quit(1)
		return
	var queue: Array = scene.game_menus._shop_queue(shop_frame)
	# Research: buttontop is submitted before the layer-0 text batch. The
	# two text draws are Buy then Sell; 2D queue traversal reverses them.
	if queue.size() != 3 or str(queue[0].kind) != "text" or int(queue[0].index) != 1 or str(queue[1].kind) != "text" or int(queue[1].index) != 0 or str(queue[2].kind) != "sprite" or int(queue[2].index) != 0:
		printerr("shop packet queue differs from the research order")
		quit(1)
		return
	var layered: Array = scene.game_menus._shop_queue({"draw_order": [
		{"kind": "sprite", "index": 0, "layer": 1, "submission": 3},
		{"kind": "sprite", "index": 1, "layer": 0, "submission": 0},
		{"kind": "text", "index": 0, "layer": 0, "submission": 1},
		{"kind": "text", "index": 1, "layer": 0, "submission": 2},
	]})
	if layered.map(func(packet: Dictionary) -> String: return "%s:%d" % [packet.kind, int(packet.index)]) != ["text:1", "text:0", "sprite:1", "sprite:0"]:
		printerr("2D queue did not use ascending layer and reverse submission")
		quit(1)
		return
	var shop_art_count := 0
	for sprite in shop_frame.sprites:
		var art: Dictionary = scene.game_menus._shop_art(str(sprite.resource))
		if not art.is_empty():
			shop_art_count += 1
	if shop_art_count != 1:
		printerr("checked shop art could not be read from selected disc")
		quit(1)
		return
	for sprite in inventory_frame.sprites:
		if str(sprite.resource).contains("underbar"):
			printerr("unverified underbar submission was drawn")
			quit(1)
			return
			quit(1)
			return
	var saved := 0
	for which in ["inventory", "shop", "shop-bag", "skills", "stats", "level-up", "save", "pause"]:
		scene.game_menus.open_screen(which)
		for _frame in range(4):
			await process_frame
		await RenderingServer.frame_post_draw
		var image: Image = scene.viewport.get_texture().get_image()
		var path := capture_dir.path_join(which + ".png")
		if image != null and image.save_png(path) == OK:
			saved += 1
		else:
			printerr("capture failed ", which)
	var first := Image.new()
	if first.load(capture_dir.path_join("inventory.png")) != OK:
		printerr("inventory capture could not be read")
		quit(1)
		return
	var shop_capture := Image.new()
	if shop_capture.load(capture_dir.path_join("shop.png")) != OK or shop_capture.get_data() == first.get_data():
		printerr("rendered shop did not differ from inventory")
		quit(1)
		return
	scene.game_menus.close_menu()
	scene.game_menus.configure(null)
	var missing_pause: Dictionary = scene.game_menus.open_vendor_shop()
	if missing_pause.get("ok", false) or scene.game_menus.vendor_open:
		printerr("shop opened without an authoritative pause lifecycle")
		quit(1)
		return
	scene.game_menus.configure(scene)
	scene.npc_dialog.emit_signal("shop_requested", 0)
	if not scene.game_menus.vendor_open or not scene.game_menus.open or scene.game_menus.live_imported or not scene.paused:
		printerr("hub shop without a player record did not open in refusal state")
		quit(1)
		return
	scene.game_menus.close_menu()
	scene.game_menus.sync_font(scene.components.art)
	scene.game_menus.open_screen("inventory")
	scene.game_menus.service.set_cursor(17, 123)
	scene.game_menus.queue_redraw()
	for _frame in range(4):
		await process_frame
		await RenderingServer.frame_post_draw
	var hover: Image = scene.viewport.get_texture().get_image()
	if hover == null or hover.get_data() == first.get_data() or hover.save_png(capture_dir.path_join("inventory-hover.png")) != OK:
		printerr("inventory hover did not change the rendered image")
		quit(1)
		return
	# Use an authored synthetic PS2 slot through the same import bridge as play.
	# Its item resolves through the selected disc's tables and picture cache.
	var slot: PackedByteArray = ClassDB.class_call_static("ChampionsPs2SaveImport", "synthetic_slot", "Aria", 777)
	var imported: Dictionary = scene.ps2_import.import_bytes(slot, 0)
	var adopted: Dictionary = scene.game_menus.adopt_imported() if imported.get("ok", false) else imported
	if not adopted.get("ok", false):
		printerr("runtime item table or record refused: ", adopted.get("error", ""))
		quit(1)
		return
	var live_frame: Dictionary = scene.game_menus.service.frame("inventory")
	if not live_frame.get("ok", false) or live_frame.get("icons", []).size() != 1:
		printerr("runtime picture association refused: ", live_frame.get("error", ""), " icons=", live_frame.get("icons", []).size())
		quit(1)
		return
	# Rehouse the authored item in its executable-defined equipment slot.
	# The generic save fixture originally puts it in slot 0 regardless of type.
	var equipment_slot := int(live_frame.icons[0].slot)
	if equipment_slot < 0 or equipment_slot >= 17:
		printerr("authored icon has no bounded equipment slot")
		quit(1)
		return
	var coherent: PackedByteArray = scene.ps2_import.record_bytes()
	var pointer := coherent.slice(0x43f0, 0x43f4)
	for byte in range(4):
		coherent[0x43f0 + byte] = 0
		coherent[0x43f0 + equipment_slot * 4 + byte] = pointer[byte]
		if int(live_frame.icons[0].category) == 0:
			coherent[0x4434 + byte] = pointer[byte]
	if not scene.ps2_import.replace_record(coherent).get("ok", false) or not scene.game_menus.adopt_imported().get("ok", false):
		printerr("authored equipment slot could not be loaded")
		quit(1)
		return
	scene.game_menus.queue_redraw()
	for _frame in range(4):
		await process_frame
		await RenderingServer.frame_post_draw
	var actual_icons: Image = scene.viewport.get_texture().get_image()
	if actual_icons == null or actual_icons.save_png(capture_dir.path_join("inventory-icons.png")) != OK:
		printerr("runtime icon capture failed")
		quit(1)
		return
	var original_record: PackedByteArray = scene.ps2_import.record_bytes()
	if int(scene.ps2_import.record_word(0x43f0 + equipment_slot * 4)) == 0:
		printerr("authored imported equipment pointer was empty")
		quit(1)
		return
	scene.game_menus.service.set_cursor(17, 100)
	scene.game_menus._press(int(scene.game_menus.service.state().equip_mask))
	if int(scene.ps2_import.record_word(0x43f0 + equipment_slot * 4)) != 0:
		printerr("unequip did not change the imported player record")
		quit(1)
		return
	scene.automatic_steps = true
	scene.held_commands = 1
	scene.pending_damage = true
	scene.npc_dialog.emit_signal("shop_requested", 0)
	if not scene.game_menus.vendor_open or not scene.game_menus.service.state().get("live_record", false) or not scene.paused or scene.held_commands != 0 or scene.pending_damage:
		printerr("hub shop event did not open the imported player's shop")
		quit(1)
		return
	var paused_ticks: int = int(scene.raw_observation.ready_ticks)
	var paused_submissions: int = int(scene.raw_observation.submissions)
	var paused_state: Dictionary = scene.raw_observation.duplicate(true)
	for _frame in range(8):
		await process_frame
	if scene.raw_observation != paused_state or int(scene.raw_observation.ready_ticks) != paused_ticks or int(scene.raw_observation.submissions) != paused_submissions:
		printerr("automatic session advanced while shop held offline pause")
		quit(1)
		return
	var live_shop: Dictionary = scene.game_menus.service.frame("shop")
	if live_shop.get("sprites", []).size() != 1 or int(scene.game_menus.service.state().gold) != 777:
		printerr("live shop did not retain the imported player")
		quit(1)
		return
	for _frame in range(4):
		await process_frame
		await RenderingServer.frame_post_draw
	var live_shop_image: Image = scene.viewport.get_texture().get_image()
	if live_shop_image == null or live_shop_image.get_height() < shop_capture.get_height() or live_shop_image.get_data() == shop_capture.get_data() or live_shop_image.save_png(capture_dir.path_join("shop-live.png")) != OK:
		printerr("live shop capture did not show imported state")
		quit(1)
		return
	var stock_refused: Dictionary = scene.game_menus.service.press(0, int(scene.game_menus.service.state().shop_mask))
	if stock_refused.get("ok", false) or int(scene.game_menus.service.state().gold) != 777:
		printerr("unknown stock purchase was not refused")
		quit(1)
		return
	var before_sale: PackedByteArray = scene.ps2_import.record_bytes()
	var before_count: int = int(before_sale[0x134]) | (int(before_sale[0x135]) << 8)
	scene.game_menus.open_screen("shop-bag")
	scene.game_menus.service.set_cursor(17, 100)
	scene.game_menus._press(int(scene.game_menus.service.state().shop_mask))
	var after_sale: PackedByteArray = scene.ps2_import.record_bytes()
	var after_count: int = int(after_sale[0x134]) | (int(after_sale[0x135]) << 8)
	if after_sale == before_sale or after_count >= before_count or int(scene.ps2_import.record_word(0xe4)) < 777:
		printerr("shop sale did not sync: count ", before_count, " -> ", after_count,
			" gold ", 777, " -> ", int(scene.ps2_import.record_word(0xe4)),
			" action state ", scene.game_menus.status.text.left(160))
		quit(1)
		return
	scene.game_menus.close_menu()
	if scene.game_menus.vendor_open or scene.game_menus.open or scene.game_menus.inventory_art != null or scene.paused:
		printerr("shop close did not hide and unload resources: vendor=", scene.game_menus.vendor_open,
			" open=", scene.game_menus.open, " art=", scene.game_menus.inventory_art != null,
			" paused=", scene.paused, " native=", scene.raw_observation.get("paused", null))
		quit(1)
		return
	for _frame in range(30):
		if int(scene.raw_observation.submissions) > paused_submissions:
			break
		await process_frame
	if int(scene.raw_observation.submissions) <= paused_submissions:
		printerr("automatic session did not resume after shop close")
		quit(1)
		return
	scene.automatic_steps = false
	scene.ps2_import.replace_record(original_record)
	print("captures ", saved + 3)
	quit(0 if saved == 8 else 1)
