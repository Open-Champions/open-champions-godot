extends Control

const RealText = preload("res://real_data/real_text.gd")
const FrontendPainter = preload("res://ui/frontend_view.gd")
const RTA_EXE_SHA := "292f6f48e3609c0026def2307d76c222cd5b389303d53b89b0e107d7b1dcd675"

# Host shell around the documented menu contract. Canvas positions come from
# ChampionsMenuService. Buttons in the side panel are host controls: the start
# bit, the stat-spend bit and the skill-confirm bit were not executed.
var service: Object
var layout_service: Object
var dynamic_service: Object
var open := false
var screen := "inventory"
var mode := 2
var font_bytes := PackedByteArray()
var font_texture: ImageTexture
var small_font_bytes := PackedByteArray()
var small_font_texture: ImageTexture
var scene: Control
var host: PanelContainer
var status: Label
var opener: Button
var inventory_art: RefCounted
var live_imported := false
var icon_index := PackedByteArray()
var icon_cache := {}
var icon_index_path := ""
var inventory_art_path := ""

func _ready() -> void:
	mouse_filter = MOUSE_FILTER_IGNORE
	set_anchors_preset(PRESET_FULL_RECT)
	if ClassDB.class_exists("ChampionsMenuService"):
		service = ClassDB.instantiate("ChampionsMenuService")
	if ClassDB.class_exists("ChampionsUiLayoutService"):
		layout_service = ClassDB.instantiate("ChampionsUiLayoutService")
	if ClassDB.class_exists("ChampionsUiDynamicLayoutService"):
		dynamic_service = ClassDB.instantiate("ChampionsUiDynamicLayoutService")
	opener = Button.new()
	opener.text = "Menus"
	opener.tooltip_text = "Host open. The original start button bit was not in the menu contract."
	opener.position = Vector2(8, 8)
	opener.pressed.connect(func(): open_screen("inventory"))
	add_child(opener)
	host = PanelContainer.new()
	host.anchor_left = 1.0
	host.anchor_right = 1.0
	host.anchor_bottom = 1.0
	host.offset_left = -300
	host.offset_top = 8
	host.offset_right = -8
	host.offset_bottom = -8
	host.visible = false
	host.mouse_filter = MOUSE_FILTER_STOP
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(270, 0)
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	host.add_child(scroll)
	var box := VBoxContainer.new()
	box.custom_minimum_size = Vector2(260, 0)
	scroll.add_child(box)
	add_child(host)
	status = Label.new()
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	status.custom_minimum_size = Vector2(250, 0)
	box.add_child(status)
	_host_button(box, "Inventory", "Bag page 0. J equips, U drops. Host keys for the cross-confirm masks.", func(): open_screen("inventory"))
	for page in range(6):
		var selected_page := page
		_host_button(box, "Inventory page %d" % page, "Pages 0-2 are bags; pages 3-5 dispatch the stat sheet.", func(): _open_inventory_page(selected_page))
	_host_button(box, "Shop stock", "Mode 0. K buys. Choice centre x was not executed.", func(): open_screen("shop"))
	_host_button(box, "Shop bag", "Mode 1. K sells, or opens the confirm box when the cell is equipped.", func(): open_screen("shop-bag"))
	_host_button(box, "Skills", "Displayed rank cost. The confirm bit was not executed.", func(): open_screen("skills"))
	_host_button(box, "Stats", "Pages 3-5 draw this sheet. A press here does not spend.", func(): open_screen("stats"))
	_host_button(box, "Level up", "Acknowledges the pending level and shows page 5.", func(): open_screen("level-up"))
	_host_button(box, "Save index", "Equipment named by the executed bag*1000+index conversion.", func(): open_screen("save"))
	_host_button(box, "Pause", "Font, layer and colour only. Frame positions were not executed.", func(): open_screen("pause"))
	_host_button(box, "Cross / circle", "Cross-confirm is inferred. Circle-confirm is the other executed mask set.", _toggle_scheme)
	_host_button(box, "Spend stat 0", "Handler only. The pad bit and cursor value were not executed.", func(): _call("spend_stat", [0]))
	_host_button(box, "Spend skill point", "Handler spends one point and ignores the displayed cost. The confirm bit was not executed.", func(): _call("spend_skill", [0, 1]))
	_host_button(box, "Grant level", "Documented skill and stat point bands. The experience table was not in this contract.", _grant)
	_host_button(box, "Close", "Host close. The original close control was not executed.", close_menu)
	_refresh_status()

func configure(play_flow: Control) -> void:
	scene = play_flow

func adopt_imported() -> Dictionary:
	if service == null or scene == null or scene.get("ps2_import") == null or scene.get("real") == null:
		return {"ok": false, "error": "No imported player or selected disc."}
	var imported = scene.get("ps2_import")
	var real = scene.get("real")
	if not imported.has_character() or not real.files.has("slus_209.73"):
		return {"ok": false, "error": "Imported player and selected RTA executable are required."}
	var path: String = real.files["slus_209.73"]
	if FileAccess.get_sha256(path).to_lower() != RTA_EXE_SHA:
		return {"ok": false, "error": "Selected executable does not match the researched US v2.00 revision."}
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null or file.get_length() > 8 * 1024 * 1024:
		return {"ok": false, "error": "Selected executable is unavailable or outside the bounded reader."}
	sync_font(null)
	var result: Dictionary = service.adopt_record(imported.record_bytes(), file.get_buffer(file.get_length()), icon_index)
	live_imported = result.get("ok", false)
	_refresh_status()
	queue_redraw()
	return result

func _host_button(box: VBoxContainer, label: String, tip: String, action: Callable) -> void:
	var button := Button.new()
	button.text = label
	button.tooltip_text = tip
	button.pressed.connect(action)
	box.add_child(button)

func sync_font(art) -> void:
	var changed := false
	if font_texture == null and art != null:
		var view = art.get("hud_view")
		if view != null and not view.font_bytes.is_empty() and view.font_texture != null:
			font_bytes = view.font_bytes
			font_texture = view.font_texture
			changed = true
	if small_font_texture == null and scene != null and layout_service != null:
		var real = scene.get("real")
		if real != null and not real.files.is_empty():
			var raw: PackedByteArray = RealText.fonts(real).get("gamefontsmall.fnt", PackedByteArray())
			if not raw.is_empty():
				var decoded: Dictionary = layout_service.decode_hud_font(raw, true)
				if decoded.get("ok", false):
					var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
					small_font_bytes = raw
					small_font_texture = ImageTexture.create_from_image(image)
					changed = true
	if changed:
		_refresh_status()
		queue_redraw()
	if scene != null and scene.get("real") != null:
		var real = scene.get("real")
		var art_path := str(real.files.get("inventry.lmp", ""))
		if art_path != inventory_art_path and scene.get("inspector") != null:
			inventory_art = FrontendPainter.new()
			inventory_art.configure(real, scene.get("inspector"))
			inventory_art_path = art_path
			queue_redraw()
		var index_path := str(real.files.get("piccache.hdr", ""))
		if index_path != icon_index_path:
			icon_index = PackedByteArray()
			icon_cache.clear()
			icon_index_path = index_path
		if icon_index.is_empty() and not index_path.is_empty():
			var file := FileAccess.open(index_path, FileAccess.READ)
			if file != null and file.get_length() > 0 and file.get_length() <= 8 * 1024 * 1024:
				icon_index = file.get_buffer(file.get_length())

func _icon(picture: String) -> Dictionary:
	if icon_cache.has(picture):
		return icon_cache[picture]
	if scene == null or scene.get("real") == null or scene.get("inspector") == null or icon_index.is_empty():
		return {"refused": "Picture cache index or runtime reader is unavailable."}
	var real = scene.get("real")
	if not real.files.has("piccache.dat"):
		return {"refused": "Picture cache data is unavailable."}
	var inspector = scene.get("inspector")
	var found: Dictionary = inspector.lookup_inventory_icon(icon_index, picture, "rta")
	if not found.get("ok", false):
		return {"refused": "Picture cache lookup failed."}
	if not found.get("found", false):
		return {"miss": true}
	var offset: int = int(found.first_sector) * 2048
	var length: int = int(found.sectors) * 2048
	var file := FileAccess.open(real.files["piccache.dat"], FileAccess.READ)
	if file == null or length <= 0 or length > 8 * 1024 * 1024 or offset < 0 or offset + length > file.get_length():
		return {"refused": "Picture cache payload is unavailable or outside its bounds."}
	file.seek(offset)
	var payload := file.get_buffer(length)
	if payload.size() != length:
		return {"refused": "Picture cache payload is truncated."}
	var decoded: Dictionary = inspector.decode_inventory_icon(payload, "rta", true)
	if not decoded.get("ok", false) or int(decoded.width) != int(found.width) or int(decoded.height) != int(found.height):
		return {"refused": "Picture cache payload failed decoding or dimension checks."}
	var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
	icon_cache[picture] = {"texture": ImageTexture.create_from_image(image), "width": int(decoded.width), "height": int(decoded.height)}
	return icon_cache[picture]

func open_screen(which: String) -> void:
	if service == null:
		return
	open = true
	mouse_filter = MOUSE_FILTER_STOP
	opener.visible = false
	host.visible = true
	screen = which
	mode = 2
	if which == "shop":
		mode = 0
		service.set_page(0)
	elif which == "shop-bag":
		mode = 1
		screen = "inventory"
		service.set_page(0)
	elif which == "stats":
		service.set_page(3)
	elif which == "level-up":
		service.open_level_up()
	elif which != "pause":
		service.set_page(0)
	_refresh_status()
	queue_redraw()

func close_menu() -> void:
	open = false
	mouse_filter = MOUSE_FILTER_IGNORE
	opener.visible = true
	host.visible = false
	queue_redraw()

func _open_inventory_page(page: int) -> void:
	open_screen("inventory")
	_call("set_page", [page])

func handle_host_key(event: InputEvent) -> bool:
	if service == null or not event is InputEventKey or not event.pressed or event.echo:
		return false
	var key_event: InputEventKey = event as InputEventKey
	var key: int = key_event.physical_keycode if key_event.physical_keycode != 0 else key_event.keycode
	if not open:
		if key == KEY_M:
			open_screen("inventory")
			return true
		return false
	if key == KEY_ESCAPE or key == KEY_M:
		close_menu()
		return true
	var row: Dictionary = service.state()
	if not row.get("ok", false):
		return true
	if key == KEY_J:
		_press(int(row.equip_mask))
		return true
	if key == KEY_U:
		_press(int(row.drop_mask))
		return true
	if key == KEY_K:
		_press(int(row.shop_mask))
		return true
	return false

func save_companion(path: String) -> bool:
	if service == null:
		return false
	var saved: Dictionary = service.save_bytes()
	if not saved.get("ok", false):
		status.text = str(saved.get("error", "menu save refused"))
		return false
	var file := FileAccess.open(path + ".menus", FileAccess.WRITE)
	if file == null:
		return false
	file.store_buffer(saved.bytes)
	return true

func load_companion(path: String) -> bool:
	if service == null:
		return false
	var companion := path + ".menus"
	if not FileAccess.file_exists(companion):
		service.reset()
		live_imported = false
		_refresh_status()
		return true
	var file := FileAccess.open(companion, FileAccess.READ)
	if file == null:
		return false
	var loaded: Dictionary = service.load_bytes(file.get_buffer(file.get_length()))
	live_imported = false
	_refresh_status()
	return loaded.get("ok", false)

func _toggle_scheme() -> void:
	if service == null:
		return
	var row: Dictionary = service.state()
	var next: String = "circle" if str(row.get("scheme", "cross")) == "cross" else "cross"
	_show(service.set_scheme(next))

func _grant() -> void:
	_call("grant_level", [])

func _call(method: String, args: Array) -> void:
	if service == null:
		return
	_show(service.callv(method, args))

func _press(edge: int) -> void:
	if service == null:
		return
	var result: Dictionary = service.press(mode, edge)
	if result.get("ok", false) and live_imported and scene != null and scene.get("ps2_import") != null:
		var synced: Dictionary = scene.get("ps2_import").replace_record(service.record_bytes())
		if not synced.get("ok", false):
			result = synced
	_show(result)

func _show(result: Dictionary) -> void:
	_refresh_status()
	if not result.get("ok", false):
		status.text = str(result.get("error", "refused")) + "\n" + status.text
	queue_redraw()

func _refresh_status() -> void:
	if status == null or service == null:
		return
	var row: Dictionary = service.state()
	if not row.get("ok", false):
		status.text = str(row.get("error", "menu state refused"))
		return
	var source := "Imported player" if row.get("live_record", false) else "Authored fixture"
	var font_note := "" if small_font_texture != null else "\nGame font unavailable; item text hidden."
	var drop_note := "\nDrop unavailable for imported play." if row.get("live_record", false) else ""
	var detail_note := "\nItem detail fields unavailable." if screen == "inventory" else ""
	var page_note := ""
	if screen == "inventory" and int(row.page) in [1, 2]:
		var page_frame: Dictionary = service.frame("inventory")
		for gap in page_frame.get("gaps", []):
			if str(gap.id) == "inventory-page-grid":
				page_note = "\nPAGE ITEMS REFUSED: display grid unavailable."
			elif str(gap.id) == "inventory-grid-footprint":
				page_note = "\nITEM FOOTPRINT REFUSED: later placement unknown."
	status.text = "%s · Page %d\nGold %d · Level %d\nJ equip/unequip · U drop\n%s controls are host keys.%s%s%s%s" % [
		source, int(row.page), int(row.gold), int(row.level), str(row.scheme).capitalize(), drop_note, detail_note, font_note, page_note]

func _gui_input(event: InputEvent) -> void:
	if not open or service == null:
		return
	if event is InputEventMouseMotion or (event is InputEventMouseButton and event.pressed):
		var logical := _logical(event.position)
		service.set_cursor(int(logical.x) - (80 if screen == "inventory" else 0), int(logical.y))
		queue_redraw()

func _logical(pos: Vector2) -> Vector2:
	if screen == "inventory":
		var inventory_scale := minf(1.0, minf(size.x / 640.0, size.y / 480.0))
		return pos / inventory_scale if inventory_scale > 0.0 else Vector2.ZERO
	var factor := minf(1.0, size.x / 640.0)
	if factor <= 0.0:
		return Vector2.ZERO
	return Vector2(pos.x / factor, pos.y / (factor * 480.0 / 448.0))

func _draw() -> void:
	if not open or service == null:
		return
	draw_rect(Rect2(Vector2.ZERO, size), Color(0, 0, 0, 0.45), true)
	var factor := minf(1.0, size.x / 640.0)
	if factor <= 0.0:
		return
	if screen == "inventory":
		# The inventory's executed 480-line box and scrollbar submissions need
		# the whole 640x480 canvas in the in-game panel.
		var inventory_scale := minf(factor, size.y / 480.0)
		draw_set_transform(Vector2.ZERO, 0.0, Vector2(inventory_scale, inventory_scale))
	else:
		draw_set_transform(Vector2.ZERO, 0.0, Vector2(factor, factor * 480.0 / 448.0))
	var drawn: Dictionary = service.frame(screen)
	if not drawn.get("ok", false):
		return
	if screen == "inventory":
		# Original queue sorts by layer, with later submissions before earlier
		# ones in a layer. This bounded art sequence is the zero-state fixture.
		for sprite in drawn.get("sprites", []):
			if int(sprite.layer) >= 7:
				continue
			if inventory_art == null:
				continue
			var art: Dictionary = inventory_art.load_art(str(sprite.lump), str(sprite.resource), true)
			if art.is_empty():
				continue
			draw_texture(art.texture, Vector2(int(sprite.x), int(sprite.y)), _word(int(sprite.tint)))
		for frame in drawn.get("frames", []):
			if layout_service == null or inventory_art == null:
				continue
			var pieces: Dictionary = layout_service.menu_frame(int(frame.x), int(frame.y), int(frame.width), int(frame.height), int(frame.style), false, false)
			if not pieces.get("ok", false):
				continue
			for piece in pieces.draws:
				var art: Dictionary = inventory_art.load_art("interfac.lmp", str(piece.resource), true)
				if art.is_empty():
					continue
				var full := Rect2(int(piece.x), int(piece.y), int(art.width), int(art.height))
				var shown := full
				if bool(piece.clipped):
					var clip: Dictionary = piece.clip
					shown = full.intersection(Rect2(int(clip.left), int(clip.top), int(clip.right) - int(clip.left), int(clip.bottom) - int(clip.top)))
				if shown.has_area():
					draw_texture_rect_region(art.texture, shown, Rect2(shown.position - full.position, shown.size), _word(int(frame.tint)), false)
		for rect in drawn.get("rects", []):
			if int(rect.layer) in [4, 5]:
				draw_rect(Rect2(int(rect.x0), int(rect.y0), int(rect.x1) - int(rect.x0), int(rect.y1) - int(rect.y0)), _word(int(rect.colour)), true)
		for icon in drawn.get("icons", []):
			var art: Dictionary = _icon(str(icon.picture))
			if art.get("miss", false) and inventory_art != null:
				art = inventory_art.load_art("equipmnt.lmp", "pic_dagger1.tex", true)
			var row_y: float = 119.0 + float(icon.row) * 34.0
			if art.has("refused") or art.is_empty():
				var mark := Rect2(97 + int(icon.column) * 32, row_y, 96, 32)
				draw_rect(mark, Color(0.35, 0.0, 0.0, 0.95), true)
				draw_rect(mark, Color(1.0, 0.2, 0.2), false, 2.0)
				draw_string(ThemeDB.fallback_font, mark.position + Vector2(2, 20), "ICON REFUSED", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color.WHITE)
				continue
			if scene.get("inspector") == null:
				continue
			var position: Dictionary = scene.get("inspector").inventory_icon_grid_placement(int(art.width), int(art.height), 80, int(icon.column), row_y, "rta")
			if not position.get("ok", false):
				continue
			var full := Rect2(int(position.x), int(position.y), int(art.width), int(art.height))
			var shown := full.intersection(Rect2(0, 102, 640, 346))
			if shown.has_area():
				draw_texture_rect_region(art.texture, shown, Rect2(shown.position - full.position, shown.size), _word(int(icon.tint)), false)
		for row_text in drawn.get("texts", []):
			_glyphs(int(row_text.x), int(row_text.y), str(row_text.text), int(row_text.colour), bool(row_text.centred), true)
		for selected in drawn.get("selected_texts", []):
			_glyphs(int(selected.x), int(selected.y), str(selected.text), int(selected.colour), true, true)
		for gap in drawn.get("gaps", []):
			if str(gap.id) == "inventory-page-grid":
				draw_string(ThemeDB.fallback_font, Vector2(98, 160), "PAGE ITEMS REFUSED", HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Color.WHITE)
			elif str(gap.id) == "inventory-grid-footprint":
				var column := int(gap.get("column", -1))
				var row := int(gap.get("row", -1))
				if column >= 0 and row >= 0:
					var cell := Rect2(97 + column * 32, 119 + row * 34, 32, 32)
					draw_rect(cell, Color(0.4, 0.0, 0.0, 0.9), true)
					draw_string(ThemeDB.fallback_font, cell.position + Vector2(2, 20), "REF", HORIZONTAL_ALIGNMENT_LEFT, 30, 10, Color.WHITE)
		var held_icons: Array = drawn.get("held_icons", [])
		# The UI queue paints later layer-7 submissions first.
		for index in range(held_icons.size() - 1, -1, -1):
			var held: Dictionary = held_icons[index]
			var held_art: Dictionary = _icon(str(held.picture))
			if held_art.get("miss", false) and inventory_art != null:
				held_art = inventory_art.load_art("equipmnt.lmp", "pic_dagger1.tex", true)
			if held_art.has("texture"):
				# The plain-sprite callee adds one to y.
				draw_texture(held_art.texture, Vector2(int(held.x), int(held.y) + 1), _word(int(held.tint)))
			elif index == 0:
				draw_string(ThemeDB.fallback_font, Vector2(int(held.x), int(held.y) + 20), "DRAG REFUSED", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color.WHITE)
		for gap in drawn.get("gaps", []):
			if str(gap.id) == "inventory-drag-picture":
				draw_string(ThemeDB.fallback_font, Vector2(98, 180), "DRAG REFUSED", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color.WHITE)
		for sprite in drawn.get("sprites", []):
			if int(sprite.layer) != 7 or inventory_art == null:
				continue
			var cursor_art: Dictionary = inventory_art.load_art(str(sprite.lump), str(sprite.resource), true)
			if not cursor_art.is_empty():
				draw_texture(cursor_art.texture, Vector2(int(sprite.x), int(sprite.y)), _word(int(sprite.tint)))
	if screen in ["stats", "level-up", "skills"] or (screen == "inventory" and int(service.state().get("page", 0)) >= 3):
		for text in drawn.texts:
			_glyphs(int(text.x), int(text.y), str(text.text), int(text.colour), bool(text.centred), true)

func _word(tint: int) -> Color:
	var channel := func(shift: int) -> float:
		return clampf(float((tint >> shift) & 255) / 128.0, 0.0, 1.0)
	return Color(channel.call(0), channel.call(8), channel.call(16), channel.call(24))

func _glyphs(x: int, y: int, text: String, colour: int, centred: bool, small: bool) -> void:
	var raw := small_font_bytes if small else font_bytes
	var texture := small_font_texture if small else font_texture
	if texture == null or raw.is_empty() or text.is_empty() or layout_service == null:
		return
	var glyphs: Array = []
	if centred:
		var named: Dictionary = layout_service.hud_text_glyphs(raw, text.to_utf8_buffer(), x, y, colour)
		if named.get("ok", false):
			glyphs = named.glyphs
	elif dynamic_service != null:
		var units := PackedInt32Array()
		for index in text.length():
			units.append(text.unicode_at(index))
		var laid: Dictionary = dynamic_service.text_layout(raw, units, x, y, units.size(), true, 0, 0, 448, 17, false, 0, 100000, colour, -1, -1)
		if laid.get("ok", false):
			glyphs = laid.glyphs
	for glyph in glyphs:
		var sw := int(glyph.source_width)
		var sh := int(glyph.source_height)
		var sx := int(glyph.source_x)
		var sy := int(glyph.source_y)
		if sw <= 0 or sh <= 0:
			continue
		var tint := _word(int(glyph.tint))
		draw_texture_rect_region(texture, Rect2(float(glyph.x_half) * 0.5, float(glyph.y), sw, sh), Rect2(sx, sy, sw, sh), tint, false)
