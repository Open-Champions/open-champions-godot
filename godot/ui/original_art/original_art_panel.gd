extends Control

signal art_ready(ready: bool)

const CATALOGUE = preload("res://ui/original_art/catalogue.gd")
const EXACT_IMAGE = preload("res://ui/original_art/exact_image.gd")
const MAX_ROWS := 20000
var catalogue := CATALOGUE.hud_entries()
var hud_view
var hud_status: Label
var inventory_panel

var inspector: RefCounted
var source_path := ""
var fingerprint := ""
var candidates: Array = []
var loaded: Dictionary = {} # id -> {image, source, fingerprint, row_index}
var cards: Dictionary = {}
var widgets: Dictionary = {}
var integer_scale := 1
var observation: Dictionary = {}

var state_label: Label
var health_label: Label
var mana_label: Label
var image_flow: HFlowContainer
var picker: AcceptDialog
var source_button: Button
var file_dialog: FileDialog
var font_dialog: FileDialog
var font_status: Label
var font_choice: AcceptDialog
var font_rows: ItemList
var font_path := ""
var font_fingerprint := ""
var font_candidates: Array = []
var identity_choice: OptionButton
var row_list: ItemList
var picker_status: Label
var load_button: Button
var scale_choice: OptionButton

func _ready() -> void:
	_build()
	update_observation({})
	_refresh()

func configure(value: RefCounted) -> void:
	inspector = value
	if inventory_panel != null: inventory_panel.configure(value)
	if state_label != null:
		_refresh()

func show_picker() -> void:
	picker.popup_centered(Vector2i(760, 520))
	_refresh_rows()

func clear_art() -> void:
	loaded.clear()
	if hud_view != null: hud_view.set_art(loaded, catalogue)
	_refresh()
	art_ready.emit(false)

func set_integer_scale(value: int) -> void:
	integer_scale = clampi(value, 1, 2)
	_refresh()

func open_source(path: String) -> void:
	source_path = ""
	fingerprint = ""
	candidates.clear()
	_refresh_rows()
	if inspector == null:
		picker_status.text = "Native inspector unavailable. Build the local extension."
		return
	var suffix := path.get_extension().to_lower()
	if suffix not in ["gob", "lmp", "tex"]:
		picker_status.text = "Choose a GOB, LMP or TEX file."
		return
	var listing: Dictionary = inspector.inspect_ui_candidates(path, "rta")
	if not listing.get("ok", false):
		picker_status.text = "Source refused: " + str(listing.get("error", "Unknown error."))
		return
	if not listing.get("fingerprint", "") is String or str(listing["fingerprint"]).length() != 64 or not listing.get("rows", null) is Array or listing["rows"].size() > MAX_ROWS:
		picker_status.text = "Source metadata refused."
		return
	source_path = path
	fingerprint = listing["fingerprint"]
	candidates = listing["rows"]
	_refresh_rows()

func load_selection() -> bool:
	if inspector == null or source_path.is_empty() or identity_choice.selected < 0 or row_list.get_selected_items().is_empty():
		picker_status.text = "Choose a source, documented art identity and exact row."
		return false
	var id := identity_choice.get_item_text(identity_choice.selected)
	var displayed_index := row_list.get_selected_items()[0]
	var matching := _matching_rows(id)
	if displayed_index < 0 or displayed_index >= matching.size():
		picker_status.text = "Select an exact row."
		return false
	var row: Dictionary = matching[displayed_index]
	if not CATALOGUE.matches_source(id, source_path, row):
		picker_status.text = "Documented source metadata does not match."
		return false
	if int(row.get("psm", -1)) not in [0, 0x13, 0x14]:
		picker_status.text = "Texture format is unsupported."
		return false
	var decoded: Dictionary = inspector.inspect_texture(source_path, int(row["row_index"]), "rta", fingerprint)
	if not decoded.get("ok", false):
		picker_status.text = "Decode refused: " + str(decoded.get("error", "Unknown error."))
		return false
	var meta: Dictionary = catalogue[id]
	if int(decoded.get("width", -1)) != int(meta["page"][0]) or int(decoded.get("height", -1)) != int(meta["page"][1]) or str(decoded.get("name", "")).to_lower() != str(meta["entry"]).to_lower():
		picker_status.text = "Decoded identity or dimensions differ from the pinned catalogue."
		return false
	var shaped: Dictionary = inspector.shape_ui_art(int(decoded["width"]), int(decoded["height"]), decoded["rgba"], int(meta["rows"]), meta["rect"])
	if not shaped.get("ok", false):
		picker_status.text = "Native shape refused: " + str(shaped.get("error", "Unknown error."))
		return false
	if int(shaped.get("width", -1)) != int(meta["rect"][2]) or int(shaped.get("height", -1)) != int(meta["rect"][3]):
		picker_status.text = "Native shape dimensions differ from the catalogue crop."
		return false
	var pixels: PackedByteArray = shaped.get("rgba", PackedByteArray())
	if pixels.size() != int(shaped["width"]) * int(shaped["height"]) * 4:
		picker_status.text = "Native shape pixel length is invalid."
		return false
	var image := Image.create_from_data(int(shaped["width"]), int(shaped["height"]), false, Image.FORMAT_RGBA8, pixels)
	if image == null:
		picker_status.text = "Could not create the image."
		return false
	# Publish only after every identity, decode, shape and pixel check succeeds.
	loaded[id] = {"image":image, "source":source_path, "fingerprint":fingerprint, "row_index":int(row["row_index"])}
	hud_view.set_art(loaded, catalogue)
	_refresh()
	art_ready.emit(true)
	picker_status.text = "%s loaded from row %d. Metadata matching does not prove retail identity." % [id, int(row["row_index"])]
	return true

# The documented identity `id` from `path` without the picker: the same checks
# as load_selection, and only when exactly one row matches; duplicates are
# never chosen automatically.
func load_documented(id: String, path: String) -> bool:
	if not catalogue.has(id) or path.is_empty(): return false
	if source_path != path: open_source(path)
	if source_path != path: return false
	for index in range(identity_choice.item_count):
		if identity_choice.get_item_text(index) == id: identity_choice.select(index)
	if identity_choice.selected < 0 or identity_choice.get_item_text(identity_choice.selected) != id: return false
	_refresh_rows()
	if row_list.item_count != 1: return false
	row_list.select(0)
	return load_selection()

func update_observation(value: Dictionary) -> void:
	observation = value.duplicate(true)
	if health_label == null:
		return
	var hp := _number(value.get("hp"))
	var max_hp := _number(value.get("max_hp"))
	if hp < 0.0 or max_hp <= 0.0:
		health_label.text = "Health: unsupported"
	else:
		health_label.text = "Health: %s / %s" % [str(snappedf(hp, 0.01)), str(snappedf(max_hp, 0.01))]
	var mana := _number(value.get("mana"))
	var max_mana := _number(value.get("max_mana"))
	mana_label.text = "Mana: unsupported" if mana < 0.0 or max_mana <= 0.0 else "Mana: %s / %s" % [str(snappedf(mana, 0.01)), str(snappedf(max_mana, 0.01))]
	_refresh()

func _number(value: Variant) -> float:
	if typeof(value) not in [TYPE_INT, TYPE_FLOAT]:
		return -1.0
	var result := float(value)
	return result if is_finite(result) else -1.0

func _matching_rows(id: String) -> Array:
	var matching: Array = []
	for row in candidates:
		if row is Dictionary and CATALOGUE.matches_source(id, source_path, row):
			matching.append(row)
	return matching

func _refresh_rows() -> void:
	if row_list == null:
		return
	row_list.clear()
	var id := identity_choice.get_item_text(identity_choice.selected) if identity_choice.selected >= 0 else ""
	for row in _matching_rows(id):
		var parent_label := str(row.get("parent", ""))
		if parent_label.is_empty():
			parent_label = source_path.get_file()
		row_list.add_item("row %d · %s / %s · %dx%d · PSM %d" % [int(row["row_index"]), parent_label, str(row["name"]), int(row["width"]), int(row["height"]), int(row["psm"])])
	load_button.disabled = row_list.item_count == 0
	picker_status.text = "%d matching rows. Select one explicitly; duplicates are never chosen automatically. Loose TEX archive provenance is user asserted." % row_list.item_count

func _refresh() -> void:
	if state_label == null:
		return
	state_label.text = "Original art not loaded" if loaded.is_empty() else "%d documented image(s) loaded · full game-frame parity unverified" % loaded.size()
	_refresh_hud()
	for id in cards.keys():
		if not loaded.has(id):
			var old_card: Control = cards[id]
			image_flow.remove_child(old_card)
			old_card.queue_free()
			cards.erase(id)
			widgets.erase(id)
	for id in loaded.keys():
		var record: Dictionary = loaded[id]
		if not cards.has(id):
			var card := VBoxContainer.new()
			image_flow.add_child(card)
			var caption := Label.new()
			caption.text = str(id) + (" · authored diagnostic clip" if id in ["bar_red", "bar_blue"] else " · whole image")
			card.add_child(caption)
			var widget = EXACT_IMAGE.new()
			widget.name = "Image_" + str(id)
			card.add_child(widget)
			cards[id] = card
			widgets[id] = widget
		var widget = widgets[id]
		if widget.source_image != record["image"]:
			widget.set_art(record["image"], integer_scale)
		elif widget.integer_scale != integer_scale:
			widget.set_integer_scale(integer_scale)
		if id == "bar_red":
			widget.set_fill_ratio(_ratio("hp", "max_hp"))
		elif id == "bar_blue":
			widget.set_fill_ratio(_ratio("mana", "max_mana"))

func _ratio(value_key: String, maximum_key: String) -> float:
	var amount := _number(observation.get(value_key))
	var maximum := _number(observation.get(maximum_key))
	return clampf(amount / maximum, 0.0, 1.0) if amount >= 0.0 and maximum > 0.0 else 0.0

func _build() -> void:
	custom_minimum_size.y = 580
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["margin_left", "margin_top", "margin_right", "margin_bottom"]:
		margin.add_theme_constant_override(side, 12)
	add_child(margin)
	var layout := VBoxContainer.new()
	margin.add_child(layout)
	var heading := Label.new()
	heading.text = "Return to Arms · measured original UI images"
	layout.add_child(heading)
	state_label = Label.new()
	layout.add_child(state_label)
	var actions := HBoxContainer.new()
	layout.add_child(actions)
	var choose := Button.new()
	choose.text = "Choose local art…"
	choose.pressed.connect(show_picker)
	actions.add_child(choose)
	var unload := Button.new()
	unload.text = "Unload art"
	unload.pressed.connect(clear_art)
	actions.add_child(unload)
	scale_choice = OptionButton.new()
	scale_choice.tooltip_text = "Gallery image scale; the positioned HUD keeps its documented logical coordinates."
	scale_choice.add_item("1x", 1)
	scale_choice.add_item("2x", 2)
	scale_choice.item_selected.connect(func(index: int): set_integer_scale(scale_choice.get_item_id(index)))
	actions.add_child(scale_choice)
	var diagnostic := Label.new()
	diagnostic.text = "Authored diagnostic values from read-only Session observation"
	layout.add_child(diagnostic)
	health_label = Label.new()
	layout.add_child(health_label)
	mana_label = Label.new()
	layout.add_child(mana_label)
	var hud_caption := Label.new()
	hud_caption.text = "Native HUD positions · local slot 0 · top 112 logical rows · 4:3 mapping"
	hud_caption.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(hud_caption)
	hud_view = load("res://ui/original_art/hud_view.gd").new()
	layout.add_child(hud_view)
	hud_status = Label.new()
	hud_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(hud_status)
	var load_font := Button.new()
	load_font.text = "Load HUD font…"
	load_font.pressed.connect(func(): font_dialog.popup_centered_ratio(0.8))
	layout.add_child(load_font)
	font_status = Label.new()
	font_status.text = "Header +2 is not a line height."
	font_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(font_status)
	var inventory_button := Button.new()
	inventory_button.text = "Inventory picture grid…"
	inventory_button.pressed.connect(show_inventory)
	layout.add_child(inventory_button)
	inventory_panel = load("res://ui/inventory_icons/inventory_panel.gd").new()
	add_child(inventory_panel)
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	layout.add_child(scroll)
	image_flow = HFlowContainer.new()
	image_flow.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	image_flow.add_theme_constant_override("h_separation", 16)
	image_flow.add_theme_constant_override("v_separation", 16)
	scroll.add_child(image_flow)
	picker = AcceptDialog.new()
	picker.title = "Choose measured original art"
	picker.ok_button_text = "Close"
	add_child(picker)
	var picker_layout := VBoxContainer.new()
	picker.add_child(picker_layout)
	source_button = Button.new()
	source_button.text = "Open GOB / LMP / TEX…"
	source_button.pressed.connect(func(): file_dialog.popup_centered_ratio(0.8))
	picker_layout.add_child(source_button)
	identity_choice = OptionButton.new()
	for id in catalogue.keys():
		identity_choice.add_item(id)
	identity_choice.item_selected.connect(func(_index: int): _refresh_rows())
	picker_layout.add_child(identity_choice)
	row_list = ItemList.new()
	row_list.custom_minimum_size = Vector2(680, 280)
	picker_layout.add_child(row_list)
	load_button = Button.new()
	load_button.text = "Load selected row"
	load_button.pressed.connect(load_selection)
	picker_layout.add_child(load_button)
	var load_hud := Button.new()
	load_hud.text = "Load matching HUD animation set"
	load_hud.pressed.connect(load_hud_set)
	picker_layout.add_child(load_hud)
	picker_status = Label.new()
	# Give wrapping the same initial width as the row list. Before the first
	# container layout, a zero-width label can enlarge the dialog by thousands
	# of pixels and leave its Close button below the exported window.
	picker_status.custom_minimum_size.x = 680
	picker_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	picker_layout.add_child(picker_status)
	file_dialog = FileDialog.new()
	file_dialog.access = FileDialog.ACCESS_FILESYSTEM
	file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	file_dialog.add_filter("*.gob,*.GOB,*.lmp,*.LMP,*.tex,*.TEX", "RTA archive or texture")
	file_dialog.file_selected.connect(open_source)
	add_child(file_dialog)
	font_dialog = FileDialog.new()
	font_dialog.access = FileDialog.ACCESS_FILESYSTEM
	font_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	font_dialog.add_filter("*.fnt,*.FNT,*.lmp,*.LMP,*.gob,*.GOB", "RTA font or archive")
	font_dialog.file_selected.connect(open_font)
	add_child(font_dialog)
	font_choice = AcceptDialog.new()
	font_choice.title = "Choose one font row"
	font_choice.ok_button_text = "Load font row"
	add_child(font_choice)
	font_rows = ItemList.new()
	font_rows.custom_minimum_size = Vector2(360, 160)
	font_choice.add_child(font_rows)
	font_choice.confirmed.connect(load_selected_font)

func _refresh_hud() -> void:
	if hud_view == null or hud_status == null: return
	hud_view.observe(observation)
	hud_status.text = hud_view.error if not hud_view.error.is_empty() else ("HUD resources ready for this tick." if hud_view.missing.is_empty() else "Select missing HUD art: " + ", ".join(hud_view.missing))
	if not hud_view.valid_bar(observation, "mana", "max_mana"):
		hud_status.text += " Mana observation unavailable; mana bar omitted."

func show_inventory() -> void:
	inventory_panel.popup_centered(Vector2i(720, 540))

func load_hud_set() -> bool:
	# Explicit bulk request; no guessed row or duplicate-name choice. Restore
	# the previous selection if any decode in this synchronous batch refuses.
	var ids: Array[String] = []
	for id in catalogue:
		if id.begins_with("bar_red") or id.begins_with("bar_blue") or id == "statusbar":
			var rows := _matching_rows(id)
			if rows.size() != 1:
				picker_status.text = "HUD set needs exactly one row for %s; current art retained." % catalogue[id].entry
				return false
			ids.append(id)
	var before := loaded.duplicate()
	for id in ids:
		for index in range(identity_choice.item_count):
			if identity_choice.get_item_text(index) == id: identity_choice.select(index)
		_refresh_rows()
		row_list.select(0)
		if not load_selection():
			loaded = before
			hud_view.set_art(loaded, catalogue)
			_refresh()
			art_ready.emit(not loaded.is_empty())
			return false
	picker_status.text = "Loaded all 23 explicit HUD frame/bar resources. Game-frame parity remains unverified."
	return true

func open_font(path: String) -> void:
	if font_status == null:
		return
	font_status.text = "Header +2 is not a line height."
	if inspector == null or hud_view == null:
		font_status.text = "Native inspector unavailable. Header +2 is not a line height."
		return
	var suffix := path.get_extension().to_lower()
	if suffix == "fnt":
		var file := FileAccess.open(path, FileAccess.READ)
		if file == null:
			font_status.text = "HUD font could not be read. Header +2 is not a line height."
			return
		_apply_font(file.get_buffer(file.get_length()))
		return
	if suffix not in ["gob", "lmp"]:
		font_status.text = "Choose an FNT, LMP or GOB. Header +2 is not a line height."
		return
	var listing: Dictionary = inspector.inspect_ui_candidates(path, "rta")
	if not listing.get("ok", false):
		font_status.text = "HUD font archive refused. Header +2 is not a line height."
		return
	font_candidates = []
	for row in listing.get("rows", []):
		if row is Dictionary and row.get("kind", "") == "font":
			font_candidates.append(row)
	font_path = path
	font_fingerprint = str(listing.get("fingerprint", ""))
	if font_candidates.size() != 1:
		font_rows.clear()
		for row in font_candidates:
			font_rows.add_item("font row %d" % int(row.row_index))
		font_status.text = "%d font rows. Select one explicit row. Header +2 is not a line height." % font_candidates.size()
		font_choice.popup_centered(Vector2i(420, 280))
		return
	_load_font_row(int(font_candidates[0].row_index))

func load_selected_font() -> void:
	if font_rows.get_selected_items().is_empty():
		font_status.text = "Select one font row. Header +2 is not a line height."
		return
	_load_font_row(int(font_candidates[font_rows.get_selected_items()[0]].row_index))
	font_choice.hide()

func _load_font_row(row_index: int) -> void:
	var read: Dictionary = inspector.read_fnt_entry(font_path, row_index, "rta", font_fingerprint)
	if not read.get("ok", false):
		font_status.text = "HUD font row refused. Header +2 is not a line height."
		return
	_apply_font(read.bytes)

func _apply_font(bytes: PackedByteArray) -> void:
	if hud_view.set_font(bytes):
		hud_view.observe(observation)
		font_status.text = "HUD font page decoded. Header +2 is not a line height."
	else:
		font_status.text = "HUD font page refused. Previous font kept. Header +2 is not a line height."
