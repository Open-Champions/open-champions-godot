extends Control

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
var host: PanelContainer
var status: Label
var opener: Button

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

func _host_button(box: VBoxContainer, label: String, tip: String, action: Callable) -> void:
	var button := Button.new()
	button.text = label
	button.tooltip_text = tip
	button.pressed.connect(action)
	box.add_child(button)

func sync_font(art) -> void:
	if font_texture != null or art == null:
		return
	var view = art.get("hud_view")
	if view == null or view.font_bytes.is_empty() or view.font_texture == null:
		return
	font_bytes = view.font_bytes
	font_texture = view.font_texture
	queue_redraw()

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
		_refresh_status()
		return true
	var file := FileAccess.open(companion, FileAccess.READ)
	if file == null:
		return false
	var loaded: Dictionary = service.load_bytes(file.get_buffer(file.get_length()))
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
	_show(service.press(mode, edge))

func _show(result: Dictionary) -> void:
	_refresh_status()
	if not result.get("ok", false):
		status.text = str(result.get("error", "refused")) + "\n" + status.text
	queue_redraw()

func _refresh_status() -> void:
	if status == null or service == null:
		return
	var row: Dictionary = service.state()
	var place: Dictionary = service.layout()
	if not row.get("ok", false):
		status.text = str(row.get("error", "menu state refused"))
		return
	var font_note := "Game font loaded. Which HUD font the original menu uses was not documented." if font_texture != null else "Game font is not loaded yet."
	status.text = "Host panel. J equips, U drops, K buys or sells, using the selected mask set. These are not original keyboard bindings.\n%s gold %s  level %s  skill %s  stat %s  ack %s\nShop choice y %s and %s. Centre x was not executed.\n%s\n%s" % [
		str(row.scheme), str(row.gold), str(row.level), str(row.skill_points), str(row.stat_points), str(row.acknowledged),
		str(place.get("shop_buy_y", "")), str(place.get("shop_sell_y", "")), str(place.get("scheme_note", "")), font_note]

func _gui_input(event: InputEvent) -> void:
	if not open or service == null:
		return
	if event is InputEventMouseMotion or (event is InputEventMouseButton and event.pressed):
		var logical := _logical(event.position)
		service.set_cursor(int(logical.x), int(logical.y))
		queue_redraw()

func _logical(pos: Vector2) -> Vector2:
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
	draw_set_transform(Vector2.ZERO, 0.0, Vector2(factor, factor * 480.0 / 448.0))
	var drawn: Dictionary = service.frame(screen)
	if not drawn.get("ok", false):
		return
	for rect in drawn.rects:
		var height := int(rect.y1) - int(rect.y0)
		draw_rect(Rect2(int(rect.x0), int(rect.y0), int(rect.x1) - int(rect.x0), height), _word(int(rect.colour)), height <= 2)
	for text in drawn.texts:
		_glyphs(int(text.x), int(text.y), str(text.text), int(text.colour), bool(text.centred))

func _word(tint: int) -> Color:
	var channel := func(shift: int) -> float:
		return clampf(float((tint >> shift) & 255) / 128.0, 0.0, 1.0)
	return Color(channel.call(0), channel.call(8), channel.call(16), channel.call(24))

func _glyphs(x: int, y: int, text: String, colour: int, centred: bool) -> void:
	if font_texture == null or font_bytes.is_empty() or text.is_empty() or layout_service == null:
		return
	var glyphs: Array = []
	if centred:
		var named: Dictionary = layout_service.hud_text_glyphs(font_bytes, text.to_utf8_buffer(), x, y, colour)
		if named.get("ok", false):
			glyphs = named.glyphs
	elif dynamic_service != null:
		var units := PackedInt32Array()
		for index in text.length():
			units.append(text.unicode_at(index))
		var laid: Dictionary = dynamic_service.text_layout(font_bytes, units, x, y, units.size(), true, 0, 0, 448, 17, false, 0, 100000, colour, -1, -1)
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
		draw_texture_rect_region(font_texture, Rect2(float(glyph.x_half) * 0.5, float(glyph.y), sw, sh), Rect2(sx, sy, sw, sh), tint, false)
