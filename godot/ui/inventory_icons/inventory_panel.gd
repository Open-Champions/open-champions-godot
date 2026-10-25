extends AcceptDialog

const MAX_INDEX_BYTES := 8 * 1024 * 1024
const MAX_PAYLOAD_BYTES := 8 * 1024 * 1024
var inspector: RefCounted
var index_path := ""
var data_path := ""
var index_bytes := PackedByteArray()
var entries: Array = []
var occupied: Dictionary = {}
var grid
var status_label: Label
var paths_label: Label
var name_edit: LineEdit
var index_dialog: FileDialog
var data_dialog: FileDialog

func _ready() -> void:
	title = "Inventory picture grid"
	ok_button_text = "Close"
	var body := VBoxContainer.new()
	add_child(body)
	var heading := Label.new()
	heading.text = "Explicit cache pictures · authored 8×8 grid · no item ownership/equipment"
	heading.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	heading.custom_minimum_size.x = 620
	body.add_child(heading)
	var buttons := HBoxContainer.new()
	body.add_child(buttons)
	for kind in ["HDR", "DAT"]:
		var button := Button.new()
		button.text = "Choose " + kind + "…"
		buttons.add_child(button)
		var dialog := FileDialog.new()
		dialog.access = FileDialog.ACCESS_FILESYSTEM
		dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
		dialog.add_filter("*." + kind.to_lower() + ",*." + kind, "Picture cache " + kind)
		add_child(dialog)
		button.pressed.connect(func(): dialog.popup_centered_ratio(0.8))
		if kind == "HDR":
			index_dialog = dialog
			dialog.file_selected.connect(select_index)
		else:
			data_dialog = dialog
			dialog.file_selected.connect(select_data)
	var clear := Button.new()
	clear.text = "Clear grid"
	clear.pressed.connect(clear_grid)
	buttons.add_child(clear)
	paths_label = Label.new()
	paths_label.custom_minimum_size.x = 620
	paths_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.add_child(paths_label)
	var row := HBoxContainer.new()
	body.add_child(row)
	name_edit = LineEdit.new()
	name_edit.placeholder_text = "Exact picture name, e.g. pic_sword11.tex"
	name_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(name_edit)
	var load_button := Button.new()
	load_button.text = "Add picture"
	load_button.pressed.connect(func(): add_picture(name_edit.text.strip_edges()))
	row.add_child(load_button)
	name_edit.text_submitted.connect(func(text: String): add_picture(text.strip_edges()))
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size.y = 220
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_child(scroll)
	grid = load("res://ui/inventory_icons/inventory_grid.gd").new()
	scroll.add_child(grid)
	status_label = Label.new()
	status_label.custom_minimum_size.x = 620
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.add_child(status_label)
	_refresh_paths()
	status_label.text = "Choose both cache files and an explicit picture name. No placeholder art is fabricated."

func configure(value: RefCounted) -> void:
	inspector = value

func _refresh_paths() -> void:
	paths_label.text = "HDR: %s · DAT: %s" % [index_path.get_file() if not index_path.is_empty() else "not selected", data_path.get_file() if not data_path.is_empty() else "not selected"]

func refuse(message: String) -> bool:
	status_label.text = message + " Current grid retained."
	return false

func select_index(path: String) -> bool:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null or file.get_length() < 4 or file.get_length() > MAX_INDEX_BYTES:
		return refuse("HDR unavailable or outside the 8 MiB inspection limit.")
	var bytes := file.get_buffer(file.get_length())
	file = null
	if inspector == null: return refuse("Native inventory inspector unavailable.")
	# Native lookup parses the complete index even for this absent probe name.
	var parsed: Dictionary = inspector.lookup_inventory_icon(bytes, "__index_validation__.tex", "rta")
	if not parsed.get("ok", false): return refuse("HDR refused: " + str(parsed.get("error", "Invalid index.")))
	index_bytes = bytes
	index_path = path
	_refresh_paths()
	status_label.text = "HDR selected. Existing pictures keep their recorded source."
	return true

func select_data(path: String) -> bool:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null or file.get_length() < 2048: return refuse("DAT unavailable or shorter than one sector.")
	file = null
	data_path = path
	_refresh_paths()
	status_label.text = "DAT selected; only the requested bounded sector span will be read."
	return true

func clear_grid() -> void:
	entries.clear()
	occupied.clear()
	grid.entries = []
	grid.queue_redraw()
	status_label.text = "Grid cleared. Cache file selections retained."

func add_picture(picture: String) -> bool:
	if inspector == null or index_bytes.is_empty() or data_path.is_empty() or picture.is_empty():
		return refuse("Select HDR, DAT and an exact picture name first.")
	if picture.length() > 256 or entries.size() >= 64: return refuse("Picture name or grid capacity limit reached.")
	var entry: Dictionary = inspector.lookup_inventory_icon(index_bytes, picture, "rta")
	if not entry.get("ok", false) or not entry.get("found", false): return refuse("Picture lookup refused or name missing: " + picture)
	var offset: int = entry.first_sector * 2048
	var length: int = entry.sectors * 2048
	var file := FileAccess.open(data_path, FileAccess.READ)
	if file == null or length <= 0 or length > MAX_PAYLOAD_BYTES or offset < 0 or offset > file.get_length() or length > file.get_length() - offset:
		return refuse("DAT sector span refused.")
	file.seek(offset)
	var payload := file.get_buffer(length)
	file = null
	if payload.size() != length: return refuse("DAT changed or could not supply the complete sector span.")
	var decoded: Dictionary = inspector.decode_inventory_icon(payload, "rta", true)
	if not decoded.get("ok", false): return refuse("Picture decode refused: " + str(decoded.get("error", "Invalid packet.")))
	if decoded.width != entry.width or decoded.height != entry.height: return refuse("HDR and decoded dimensions differ.")
	var placement: Dictionary = inspector.inventory_icon_grid_placement(entry.width, entry.height, 0, 0, 30.0, "rta")
	if not placement.get("ok", false) or placement.columns > 8 or placement.rows > 8: return refuse("Picture footprint exceeds this inspection grid.")
	var chosen := Vector2i(-1, -1)
	for row in range(9 - int(placement.rows)):
		for column in range(9 - int(placement.columns)):
			var free := true
			for y in range(placement.rows):
				for x in range(placement.columns):
					if occupied.has(Vector2i(column + x, row + y)): free = false
			if free:
				chosen = Vector2i(column, row)
				break
		if chosen.x >= 0: break
	if chosen.x < 0: return refuse("No free footprint in the inspection grid.")
	placement = inspector.inventory_icon_grid_placement(entry.width, entry.height, 0, chosen.x, 30.0 + chosen.y * 34, "rta")
	if not placement.get("ok", false): return refuse("Native grid placement refused.")
	var image := Image.create_from_data(decoded.width, decoded.height, false, Image.FORMAT_RGBA8, decoded.rgba)
	if image == null: return refuse("Image allocation refused.")
	var texture := ImageTexture.create_from_image(image)
	for y in range(placement.rows):
		for x in range(placement.columns): occupied[chosen + Vector2i(x, y)] = true
	entries.append({"name":picture, "image":image, "texture":texture, "placement":placement, "cell":chosen, "source_hdr":index_path, "source_dat":data_path, "flags":decoded.flags})
	grid.entries = entries
	grid.queue_redraw()
	status_label.text = "%s · %d×%d · native grid placement (%d, %d) · %d picture(s)" % [picture, decoded.width, decoded.height, placement.x, placement.y, entries.size()]
	return true
