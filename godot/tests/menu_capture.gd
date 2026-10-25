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
	if inventory_frame.get("sprites", []).size() != 22 or inventory_frame.get("selected_texts", []).size() != 1 or inventory_frame.get("frames", []).size() != 1:
		printerr("bounded inventory draw was not submitted")
		quit(1)
		return
	for index in [20, 21]:
		var cursor: Dictionary = inventory_frame.sprites[index]
		if str(cursor.resource) != "mousecursor.tex" or int(cursor.layer) != 7 or int(cursor.tint) != (0x80808080 if index == 20 else 0x34000000):
			printerr("documented inventory cursor submission was absent")
			quit(1)
			return
		if scene.game_menus.inventory_art.load_art(str(cursor.lump), str(cursor.resource), true).is_empty():
			printerr("documented inventory cursor resource did not load")
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
	var unverified_definition: int = int(adopted.get("unverified_definition", 0))
	if unverified_definition == 0:
		printerr("no bounded-looking runtime item definition was found for the refusal probe")
		quit(1)
		return
	for page in [1, 2]:
		var page_record := original_record.duplicate()
		var base: int = page * 0x1208
		page_record.encode_u32(0x1330 + base, 1)
		for byte in range(0x48):
			page_record[0x130 + base + byte] = original_record[0x130 + byte]
		page_record.encode_u32(0x130 + base, unverified_definition)
		if not scene.ps2_import.replace_record(page_record).get("ok", false) or not scene.game_menus.adopt_imported().get("ok", false):
			printerr("page %d counted item could not be loaded" % page)
			quit(1)
			return
		scene.game_menus.service.set_page(page)
		scene.game_menus._refresh_status()
		var page_frame: Dictionary = scene.game_menus.service.frame("inventory")
		var refused_grid := false
		var refused_cell := false
		for gap in page_frame.get("gaps", []):
			if str(gap.id) in ["inventory-page-grid", "inventory-grid-footprint"]:
				refused_grid = true
				refused_cell = str(gap.id) == "inventory-grid-footprint" and int(gap.get("column", -1)) == 0 and int(gap.get("row", -1)) == 0
		if not page_frame.get("ok", false) or not refused_grid or not refused_cell or not page_frame.get("icons", []).is_empty() or not page_frame.get("frames", []).is_empty() or page_frame.get("rects", []).size() != 65:
			printerr("page %d unverified runtime footprint was drawn or not labelled in its cell" % page)
			quit(1)
			return
		if not scene.game_menus.status.text.contains("ITEM FOOTPRINT REFUSED"):
			printerr("page %d runtime footprint was not labelled refused" % page)
			quit(1)
			return
		scene.game_menus.service.set_cursor(200, 100)
		scene.game_menus.queue_redraw()
		for _frame in range(4):
			await process_frame
			await RenderingServer.frame_post_draw
		var page_image: Image = scene.viewport.get_texture().get_image()
		if page_image == null or page_image.save_png(capture_dir.path_join("inventory-page-%d.png" % page)) != OK:
			printerr("page %d item capture failed" % page)
			quit(1)
			return
	var tall_record := original_record.duplicate()
	tall_record.encode_u32(0x1330 + 0x1208, 1)
	for byte in range(0x48):
		tall_record[0x130 + 0x1208 + byte] = original_record[0x130 + byte]
	if not scene.ps2_import.replace_record(tall_record).get("ok", false) or not scene.game_menus.adopt_imported().get("ok", false):
		printerr("tall-footprint probe record could not be loaded")
		quit(1)
		return
	scene.game_menus.service.set_page(1)
	scene.game_menus._refresh_status()
	var tall_frame: Dictionary = scene.game_menus.service.frame("inventory")
	var tall_refused := false
	for gap in tall_frame.get("gaps", []):
		if str(gap.id) == "inventory-grid-footprint":
			tall_refused = true
	if not tall_refused or not tall_frame.get("icons", []).is_empty() or not scene.game_menus.status.text.contains("ITEM FOOTPRINT REFUSED"):
		printerr("tall-footprint draw was not labelled refused")
		quit(1)
		return
	scene.game_menus.queue_redraw()
	for _frame in range(4):
		await process_frame
		await RenderingServer.frame_post_draw
	var tall_image: Image = scene.viewport.get_texture().get_image()
	if tall_image == null or tall_image.save_png(capture_dir.path_join("inventory-page-1-tall-refused.png")) != OK:
		printerr("tall-footprint refusal capture failed")
		quit(1)
		return
	var drag_record := original_record.duplicate()
	for byte in range(4):
		drag_record[0x43f0 + equipment_slot * 4 + byte] = 0
	drag_record.encode_u64(0x130 + 8, drag_record.decode_u64(0x130 + 8) | (1 << 35))
	if not scene.ps2_import.replace_record(drag_record).get("ok", false) or not scene.game_menus.adopt_imported().get("ok", false):
		printerr("authored drag record could not be loaded")
		quit(1)
		return
	scene.game_menus.service.set_page(0)
	scene.game_menus.service.set_cursor(17, 100)
	var drag_action: Dictionary = scene.game_menus.service.press(2, int(scene.game_menus.service.state().equip_mask))
	var held_frame: Dictionary = scene.game_menus.service.frame("inventory")
	if not drag_action.get("ok", false) or str(drag_action.get("action", "")) != "drag" or held_frame.get("held_icons", []).size() != 2:
		printerr("held-item picture was not submitted on drag")
		quit(1)
		return
	var held: Dictionary = held_frame.held_icons[0]
	var shadow: Dictionary = held_frame.held_icons[1]
	if int(held.x) != 119 or int(held.y) != 122 or int(held.layer) != 7 or int(held.tint) != 0x80808080 or int(shadow.x) != 119 or int(shadow.y) != 125 or int(shadow.layer) != 7 or int(shadow.tint) != 0x34000000 or str(shadow.picture) != str(held.picture) or not scene.game_menus._icon(str(held.picture)).has("texture"):
		printerr("held-item picture placement or loading refused")
		quit(1)
		return
	scene.game_menus.queue_redraw()
	for _frame in range(4):
		await process_frame
		await RenderingServer.frame_post_draw
	var drag_image: Image = scene.viewport.get_texture().get_image()
	if drag_image == null or drag_image.save_png(capture_dir.path_join("inventory-drag.png")) != OK:
		printerr("held-item picture capture failed")
		quit(1)
		return
	if not scene.ps2_import.replace_record(original_record).get("ok", false) or not scene.game_menus.adopt_imported().get("ok", false):
		printerr("baseline imported record could not be restored")
		quit(1)
		return
	var saved_index: PackedByteArray = scene.game_menus.icon_index
	scene.game_menus.icon_index = PackedByteArray()
	scene.game_menus.icon_cache.clear()
	var failed_icon: Dictionary = scene.game_menus._icon(str(live_frame.icons[0].picture))
	if not failed_icon.has("refused"):
		printerr("missing picture index did not produce an explicit refusal")
		quit(1)
		return
	scene.game_menus.queue_redraw()
	for _frame in range(4):
		await process_frame
		await RenderingServer.frame_post_draw
	var refused_image: Image = scene.viewport.get_texture().get_image()
	if refused_image == null or refused_image.get_data() == actual_icons.get_data() or refused_image.save_png(capture_dir.path_join("inventory-icon-refused.png")) != OK:
		printerr("icon refusal did not change the rendered image")
		quit(1)
		return
	scene.game_menus.icon_index = saved_index
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
	scene.ps2_import.replace_record(original_record)
	print("captures ", saved + 7)
	quit(0 if saved == 8 else 1)
