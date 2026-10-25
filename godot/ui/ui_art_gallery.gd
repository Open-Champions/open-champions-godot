extends Window

# Inspection UI, not original game UI. Every rectangle is entered by the player.
const PAGE_SIZE := 20
const MAX_SOURCES := 4096
const MAX_CANDIDATES := 20000
const MARKS_PATH := "user://ui_frame_measurements.json"
var inspector: RefCounted
var candidates: Array = []
var filtered: Array = []
var page := 0
var selected: Dictionary = {}
var decoded: Dictionary = {}
var marks: Array = []
var storage_path := MARKS_PATH
var storage_error := ""
var source_generation := 0
var scanning := false
var search: LineEdit
var marked_only: CheckButton
var listing: ItemList
var previous: Button
var next: Button
var page_info: Label
var info: Label
var status: Label
var storage_label: Label
var original: TextureRect
var enlarged: TextureRect
var scale_choice: SpinBox
var saved_labels: OptionButton
var frame_name: LineEdit
var fields: Array[SpinBox] = []
var save_button: Button
var font_metadata: TextEdit
var shaped_mode: CheckButton
var field_rows: SpinBox
var crop_enabled: CheckButton
var crop_fields: Array[SpinBox] = []
var file_picker: FileDialog
var folder_picker: FileDialog

func _ready() -> void:
	title = "UI art gallery · Return to Arms"
	size = Vector2i(1050, 720)
	min_size = Vector2i(640, 480)
	close_requested.connect(hide)
	if ClassDB.class_exists("ChampionsArchiveInspector"):
		inspector = ClassDB.instantiate("ChampionsArchiveInspector")
	_build()
	load_marks()

func _label(text: String, parent: Node) -> Label:
	var label := Label.new()
	label.text = text
	parent.add_child(label)
	return label

func _button(text: String, parent: Node, action: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.pressed.connect(action)
	parent.add_child(button)
	return button

func _build() -> void:
	var scroll := ScrollContainer.new()
	scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(scroll)
	var layout := VBoxContainer.new()
	layout.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	layout.add_theme_constant_override("separation", 10)
	scroll.add_child(layout)
	_label("Texture candidates and explicit FNT entries · flag 0x0040 does not prove frame identity", layout)
	_label("Stored view: RGB with alpha ignored · explicit shape view preserves RGBA bytes", layout)
	var toolbar := HBoxContainer.new()
	layout.add_child(toolbar)
	_button("Open GOB / LMP / TEX / FNT…", toolbar, func(): file_picker.popup_centered_ratio(0.8))
	_button("Open data folder…", toolbar, func(): folder_picker.popup_centered_ratio(0.8))
	search = LineEdit.new()
	search.placeholder_text = "Filter source, entry, dimensions or format"
	search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	search.text_changed.connect(func(_text): apply_filter())
	toolbar.add_child(search)
	marked_only = CheckButton.new()
	marked_only.text = "Marked only"
	marked_only.toggled.connect(func(_enabled): apply_filter())
	toolbar.add_child(marked_only)
	listing = ItemList.new()
	listing.custom_minimum_size.y = 160
	listing.item_selected.connect(select_item)
	layout.add_child(listing)
	var paging := HBoxContainer.new()
	layout.add_child(paging)
	previous = _button("Previous", paging, func(): set_page(page - 1))
	page_info = _label("No candidates", paging)
	next = _button("Next", paging, func(): set_page(page + 1))
	info = _label("Open your own RTA data. Files remain read-only.", layout)
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var scale_row := HBoxContainer.new()
	layout.add_child(scale_row)
	_label("Integer enlargement", scale_row)
	scale_choice = SpinBox.new()
	scale_choice.min_value = 1
	scale_choice.max_value = 8
	scale_choice.step = 1
	scale_choice.value = 3
	scale_choice.value_changed.connect(func(_value): render_texture())
	scale_row.add_child(scale_choice)
	font_metadata = TextEdit.new()
	font_metadata.editable = false
	font_metadata.custom_minimum_size.y = 115
	font_metadata.visible = false
	layout.add_child(font_metadata)
	var shape_row := HFlowContainer.new()
	layout.add_child(shape_row)
	shaped_mode = CheckButton.new()
	shaped_mode.text = "Explicit shape preview"
	shaped_mode.toggled.connect(func(_v): render_texture())
	shape_row.add_child(shaped_mode)
	_label("Field rows (0 = no weave)", shape_row)
	field_rows = SpinBox.new()
	field_rows.max_value = 1024
	field_rows.value_changed.connect(func(_v): render_texture())
	shape_row.add_child(field_rows)
	var crop_row := HFlowContainer.new()
	layout.add_child(crop_row)
	crop_enabled = CheckButton.new()
	crop_enabled.text = "Explicit crop"
	crop_enabled.toggled.connect(func(_v): render_texture())
	crop_row.add_child(crop_enabled)
	for caption in ["X", "Y", "W", "H"]:
		var crop_group := HBoxContainer.new()
		crop_row.add_child(crop_group)
		_label(caption, crop_group)
		var value := SpinBox.new()
		value.max_value = 2048
		value.custom_minimum_size.x = 70
		value.value_changed.connect(func(_v): render_texture())
		crop_fields.append(value)
		crop_group.add_child(value)
	var pictures := HBoxContainer.new()
	layout.add_child(pictures)
	original = _picture("Original · 1×", pictures)
	enlarged = _picture("Nearest · integer scale", pictures)
	_label("Measure in original stored pixels. Rectangle: x, y, width, height. Borders: left, top, right, bottom.", layout)
	var marking := HBoxContainer.new()
	layout.add_child(marking)
	frame_name = LineEdit.new()
	frame_name.placeholder_text = "Your frame label"
	frame_name.max_length = 128
	frame_name.custom_minimum_size.x = 180
	marking.add_child(frame_name)
	saved_labels = OptionButton.new()
	saved_labels.fit_to_longest_item = false
	saved_labels.clip_text = true
	saved_labels.custom_minimum_size.x = 140
	saved_labels.add_item("Saved labels")
	saved_labels.item_selected.connect(func(index):
		if index > 0:
			frame_name.text = saved_labels.get_item_text(index)
			restore_selection())
	marking.add_child(saved_labels)
	for label in ["X", "Y", "W", "H", "L", "T", "R", "B"]:
		var group := VBoxContainer.new()
		marking.add_child(group)
		_label(label, group)
		var field := SpinBox.new()
		field.min_value = 0
		field.max_value = 1024
		field.step = 1
		field.custom_minimum_size.x = 70
		fields.append(field)
		group.add_child(field)
	var actions := HBoxContainer.new()
	layout.add_child(actions)
	save_button = _button("Mark frame / save measurements", actions, save_selection)
	save_button.disabled = true
	_button("Load label's saved measurements", actions, restore_selection)
	storage_label = _label("Local JSON: " + storage_path, layout)
	storage_label.tooltip_text = ProjectSettings.globalize_path(storage_path)
	status = _label("Measurements are user observations, not verified original layout facts.", layout)
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	file_picker = FileDialog.new()
	file_picker.access = FileDialog.ACCESS_FILESYSTEM
	file_picker.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	file_picker.use_native_dialog = true
	file_picker.filters = PackedStringArray(["*.gob,*.lmp,*.tex,*.etex,*.fnt ; RTA data"])
	file_picker.file_selected.connect(open_source)
	add_child(file_picker)
	folder_picker = FileDialog.new()
	folder_picker.access = FileDialog.ACCESS_FILESYSTEM
	folder_picker.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	folder_picker.use_native_dialog = true
	folder_picker.dir_selected.connect(open_folder)
	add_child(folder_picker)

func _picture(caption: String, parent: Node) -> TextureRect:
	var column := VBoxContainer.new()
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(column)
	_label(caption, column)
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(280, 200)
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_child(scroll)
	var texture := TextureRect.new()
	texture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	texture.stretch_mode = TextureRect.STRETCH_SCALE
	texture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	texture.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	texture.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	scroll.add_child(texture)
	return texture

func format_name(psm: int) -> String:
	return {0: "PSMCT32", 19: "PSMT8", 20: "PSMT4"}.get(psm, "PSM %d" % psm)

func clear_sources() -> void:
	source_generation += 1
	scanning = false
	candidates.clear()
	clear_selection()
	apply_filter()

func clear_selection() -> void:
	selected = {}
	decoded = {}
	font_metadata.text = ""
	font_metadata.visible = false
	shaped_mode.set_pressed_no_signal(false)
	crop_enabled.set_pressed_no_signal(false)
	field_rows.set_value_no_signal(0)
	for value in crop_fields:
		value.set_value_no_signal(0)
	original.texture = null
	enlarged.texture = null
	original.custom_minimum_size = Vector2.ZERO
	enlarged.custom_minimum_size = Vector2.ZERO
	save_button.disabled = true
	frame_name.clear()
	saved_labels.clear()
	saved_labels.add_item("Saved labels")
	for field in fields:
		field.value = 0

func open_source(path: String) -> void:
	clear_sources()
	if inspector == null:
		status.text = "Build the native extension before opening game data."
		return
	var error := append_source(path)
	apply_filter()
	status.text = error if not error.is_empty() else "%d texture candidates / explicit FNT entries; select one to inspect." % candidates.size()

func append_source(path: String) -> String:
	var result: Dictionary = inspector.inspect_ui_candidates(path, "rta")
	if not result.get("ok", false):
		return str(result.get("error", "Could not read source."))
	var incoming: Array = result["rows"]
	if candidates.size() + incoming.size() > MAX_CANDIDATES:
		return "Candidate limit reached (20,000). Open a smaller data folder."
	for row in incoming:
		var candidate: Dictionary = row.duplicate()
		candidate["source"] = path
		candidate["fingerprint"] = result["fingerprint"]
		candidate["format"] = "FNT metadata" if candidate.get("kind") == "font" else format_name(candidate["psm"])
		candidates.append(candidate)
	return ""

# Bounded recursive traversal, yielding between files to keep the tool responsive.
# Symlink directories are skipped; game data is never written or extracted.
func open_folder(path: String) -> void:
	clear_sources()
	if inspector == null:
		status.text = "Build the native extension before opening game data."
		return
	var generation := source_generation
	scanning = true
	var pending: Array[String] = [path]
	var visited := 0
	var files := 0
	var refused := 0
	var directory_entries := 0
	while not pending.is_empty():
		if generation != source_generation:
			return
		var directory_path: String = pending.pop_back()
		var directory := DirAccess.open(directory_path)
		visited += 1
		if directory == null:
			refused += 1
			continue
		if visited > MAX_SOURCES:
			status.text = "Directory limit reached. Open a smaller folder."
			scanning = false
			apply_filter()
			return
		directory.list_dir_begin()
		var name := directory.get_next()
		while not name.is_empty():
			directory_entries += 1
			if directory_entries > 65536 or pending.size() >= MAX_SOURCES:
				directory.list_dir_end()
				status.text = "Directory-entry limit reached. Open a smaller folder."
				scanning = false
				apply_filter()
				return
			if generation != source_generation:
				directory.list_dir_end()
				return
			var child := directory_path.path_join(name)
			if directory.current_is_dir():
				if not directory.is_link(name):
					pending.append(child)
			elif name.get_extension().to_lower() in ["gob", "lmp", "tex", "etex", "fnt"]:
				files += 1
				if files > MAX_SOURCES or candidates.size() >= MAX_CANDIDATES:
					directory.list_dir_end()
					status.text = "Source/candidate limit reached. Open a smaller folder."
					scanning = false
					apply_filter()
					return
				if not append_source(child).is_empty():
					refused += 1
				status.text = "Scanning · %d sources · %d candidates · %d refused sources" % [files, candidates.size(), refused]
				await get_tree().process_frame
			name = directory.get_next()
		directory.list_dir_end()
	if generation != source_generation:
		return
	scanning = false
	apply_filter()
	status.text = "%d sources · %d candidates · %d refused sources. Decode occurs on selection." % [files, candidates.size(), refused]

func identity(row: Dictionary) -> String:
	return "%s|%s|%d" % [row.get("source", ""), row.get("fingerprint", ""), row.get("row_index", -1)]

func is_marked(row: Dictionary) -> bool:
	for mark in marks:
		if identity(mark["entry"]) == identity(row):
			return true
	return false

func apply_filter() -> void:
	clear_selection()
	filtered.clear()
	var query := search.text.strip_edges().to_lower()
	for row in candidates:
		var text := "%s %s %s %dx%d %s" % [row["source"], row["parent"], row["name"], row["width"], row["height"], row["format"]]
		if (query.is_empty() or query in text.to_lower()) and (not marked_only.button_pressed or is_marked(row)):
			filtered.append(row)
	page = 0
	render_page()

func set_page(value: int) -> void:
	page = clampi(value, 0, maxi(0, ceili(float(filtered.size()) / PAGE_SIZE) - 1))
	clear_selection()
	render_page()

func render_page() -> void:
	listing.clear()
	for i in range(page * PAGE_SIZE, mini((page + 1) * PAGE_SIZE, filtered.size())):
		var row: Dictionary = filtered[i]
		var dimensions := "unparsed page" if row.get("kind") == "font" else "%d×%d" % [row["width"], row["height"]]
		var text := "%s%s / %s · %s · %s · %s" % ["[marked] " if is_marked(row) else "", row["parent"], row["name"], dimensions, row["format"], str(row["source"]).get_file()]
		listing.add_item(text)
		listing.set_item_tooltip(listing.item_count - 1, str(row["source"]) + "\n" + text)
	previous.disabled = page == 0
	next.disabled = (page + 1) * PAGE_SIZE >= filtered.size()
	page_info.text = "%d candidates · page %d / %d" % [filtered.size(), page + 1, maxi(1, ceili(float(filtered.size()) / PAGE_SIZE))]

func select_item(index: int) -> void:
	clear_selection()
	var absolute := page * PAGE_SIZE + index
	if absolute < 0 or absolute >= filtered.size():
		return
	selected = filtered[absolute]
	var dimensions := "unparsed page" if selected.get("kind") == "font" else "%d×%d" % [selected["width"], selected["height"]]
	info.text = "%s / %s · %s · %s\n%s · row %d" % [selected["parent"], selected["name"], dimensions, selected["format"], selected["source"], selected["row_index"]]
	if selected.get("kind") == "font":
		var font: Dictionary = inspector.inspect_ui_font(selected["source"], selected["row_index"], "rta", selected["fingerprint"])
		if not font.get("ok", false):
			status.text = "Font refused: " + str(font.get("error", "Invalid FNT."))
			return
		font_metadata.visible = true
		font_metadata.text = "Raw ordered glyphs: code,u0,u1,v0,v1,signed y,advance\n" + str(font["glyphs"]) + "\nRaw kerning: left,right,signed amount\n" + str(font["kerns"])
		info.text += "\n%d glyphs · %d kerning records · offsets %d / %d / %d · no text layout inferred" % [font["glyphs"].size(), font["kerns"].size(), font["glyph_offset"], font["kerning_offset"], font["texture_offset"]]
		decoded = font["page"]
		if decoded.get("ok", false):
			info.text += "\nEmbedded stored page: %d×%d" % [decoded["width"], decoded["height"]]
	else:
		decoded = inspector.inspect_texture(selected["source"], selected["row_index"], "rta", selected["fingerprint"])
	if not decoded.get("ok", false):
		status.text = "Decode refused: " + str(decoded.get("error", "Unsupported texture."))
		decoded = {}
		return
	for mark in marks:
		if identity(mark["entry"]) == identity(selected):
			saved_labels.add_item(mark["label"])
	render_texture()
	save_button.disabled = not storage_error.is_empty() or selected.get("kind") == "font" or shaped_mode.button_pressed
	if not storage_error.is_empty():
		status.text = storage_error
	elif selected.get("kind") == "font":
		status.text = "Raw font data and embedded page only. No text layout or original baseline inferred."
	else:
		status.text = "Enter a label, rectangle and borders in original stored pixels, then save."

func render_texture() -> void:
	if decoded.is_empty():
		return
	var view := decoded
	if shaped_mode.button_pressed:
		var crop: Array = []
		if crop_enabled.button_pressed:
			for value in crop_fields:
				crop.append(int(value.value))
		view = inspector.shape_ui_art(decoded["width"], decoded["height"], decoded["rgba"], int(field_rows.value), crop)
		if not view.get("ok", false):
			original.texture = null
			enlarged.texture = null
			save_button.disabled = true
			status.text = "Shape refused: " + str(view.get("error", "Invalid shape."))
			return
	var pixels: PackedByteArray = view["rgba"].duplicate()
	if not shaped_mode.button_pressed:
		for offset in range(3, pixels.size(), 4):
			pixels[offset] = 255 # Historical stored RGB inspection semantics.
	var image := Image.create_from_data(view["width"], view["height"], false, Image.FORMAT_RGBA8, pixels)
	save_button.disabled = shaped_mode.button_pressed or selected.get("kind") == "font" or not storage_error.is_empty()
	status.text = "Explicit shape: exact RGBA; measurements disabled. No original layout inferred." if shaped_mode.button_pressed else "Stored RGB inspection; measurements remain in stored pixels."
	# UPSCALER INTEGRATION: reuse a landed native implementation here, behind an
	# explicit opt-in inspection mode; measurements always stay in stored pixels.
	# This branch uses only integer nearest enlargement, without altering art.
	var texture := ImageTexture.create_from_image(image)
	original.texture = texture
	enlarged.texture = texture
	original.custom_minimum_size = Vector2(view["width"], view["height"])
	enlarged.custom_minimum_size = original.custom_minimum_size * int(scale_choice.value)

static func valid_measurement(mark: Dictionary) -> bool:
	if not mark.get("label") is String or str(mark["label"]).strip_edges().is_empty() or str(mark["label"]).length() > 128:
		return false
	if not mark.get("entry") is Dictionary or not mark.get("rect") is Array or not mark.get("borders") is Array:
		return false
	var entry: Dictionary = mark["entry"]
	for key in ["source", "fingerprint", "name", "parent", "format"]:
		if not entry.get(key) is String:
			return false
	if str(entry["source"]).is_empty() or str(entry["fingerprint"]).length() != 64 or not str(entry["fingerprint"]).is_valid_hex_number(false):
		return false
	if mark.get("evidence") != "user_measured_unverified" or mark.get("row_order") != "stored_no_weaving":
		return false
	for key in ["width", "height", "row_index", "flags", "psm"]:
		if not _integer(entry.get(key)):
			return false
	var width := int(entry["width"])
	var height := int(entry["height"])
	if width < 1 or width > 1024 or height < 1 or height > 1024 or int(entry["row_index"]) < 0 or int(entry["row_index"]) >= MAX_CANDIDATES or int(entry["psm"]) < 0 or int(entry["psm"]) > 63 or not (int(entry["flags"]) & 0x40):
		return false
	var rect: Array = mark["rect"]
	var borders: Array = mark["borders"]
	if rect.size() != 4 or borders.size() != 4:
		return false
	for value in rect + borders:
		if not _integer(value) or value < 0 or value > 1024:
			return false
	return rect[2] > 0 and rect[3] > 0 and rect[0] + rect[2] <= width and rect[1] + rect[3] <= height and borders[0] + borders[2] <= rect[2] and borders[1] + borders[3] <= rect[3]

static func _integer(value: Variant) -> bool:
	return (value is int or value is float) and is_finite(float(value)) and float(value) == floor(float(value))

func load_marks() -> bool:
	storage_label.text = "Local JSON: " + storage_path
	storage_label.tooltip_text = ProjectSettings.globalize_path(storage_path)
	marks.clear()
	storage_error = ""
	if not storage_path.begins_with("user://"):
		storage_error = "Measurements must be stored in user://."
	elif FileAccess.file_exists(storage_path):
		var file := FileAccess.open(storage_path, FileAccess.READ)
		if file == null or file.get_length() > 8 * 1024 * 1024:
			storage_error = "Measurements file cannot be read or exceeds 8 MiB; it was preserved."
		else:
			var parser := JSON.new()
			var parse_error := parser.parse(file.get_as_text())
			var document = parser.data if parse_error == OK else null
			if not document is Dictionary or document.get("schema_version") != 1 or document.get("game") != "rta" or document.get("coordinate_space") != "original_stored_pixels" or not document.get("frames") is Array:
				storage_error = "Invalid measurements JSON; existing file preserved."
			else:
				for mark in document["frames"]:
					if not mark is Dictionary or not valid_measurement(mark):
						storage_error = "Invalid frame measurements; existing file preserved."
						break
				if storage_error.is_empty():
					marks = document["frames"]
	if not storage_error.is_empty():
		status.text = storage_error
	return storage_error.is_empty()

func save_selection() -> void:
	if decoded.is_empty() or selected.is_empty() or not storage_error.is_empty() or shaped_mode.button_pressed or selected.get("kind") == "font":
		return
	# Revalidate at save time too: decoding earlier is not proof of current bytes.
	var current: Dictionary = inspector.inspect_texture(selected["source"], selected["row_index"], "rta", selected["fingerprint"])
	if not current.get("ok", false):
		clear_selection()
		status.text = "Cannot save stale measurements: " + str(current.get("error", "Source changed."))
		return
	var values: Array = []
	for field in fields:
		if field.get_line_edit().has_focus():
			field.apply()
		values.append(int(field.value))
	var mark := {"label": frame_name.text.strip_edges(), "entry": selected.duplicate(), "rect": values.slice(0, 4), "borders": values.slice(4, 8), "evidence": "user_measured_unverified", "row_order": "stored_no_weaving"}
	if not valid_measurement(mark):
		status.text = "Enter a frame label and an in-bounds nonempty rectangle. Opposite borders must fit inside it."
		return
	var updated := marks.duplicate(true)
	var replaced := false
	for i in range(updated.size()):
		if identity(updated[i]["entry"]) == identity(selected) and updated[i]["label"] == mark["label"]:
			updated[i] = mark
			replaced = true
			break
	if not replaced:
		updated.append(mark)
	if not write_marks(updated):
		return
	marks = updated
	saved_labels.clear()
	saved_labels.add_item("Saved labels")
	for saved in marks:
		if identity(saved["entry"]) == identity(selected):
			saved_labels.add_item(saved["label"])
	render_page()
	status.text = "Saved local measurements. These remain unverified until compared with the original UI."

func write_marks(updated: Array) -> bool:
	if not storage_path.begins_with("user://") or not storage_error.is_empty():
		return false
	var document := {"schema_version": 1, "game": "rta", "coordinate_space": "original_stored_pixels", "frames": updated}
	var text := JSON.stringify(document, "\t")
	if text.to_utf8_buffer().size() > 8 * 1024 * 1024:
		status.text = "Measurements exceed the 8 MiB limit. Existing file preserved."
		return false
	var file := FileAccess.open(storage_path + ".tmp", FileAccess.WRITE)
	if file == null:
		status.text = "Cannot write measurements in the user's data folder."
		return false
	file.store_string(text)
	file.flush()
	var error := file.get_error()
	file.close()
	if error != OK or DirAccess.rename_absolute(ProjectSettings.globalize_path(storage_path + ".tmp"), ProjectSettings.globalize_path(storage_path)) != OK:
		status.text = "Could not replace measurements JSON. Existing file preserved."
		return false
	return true

func restore_selection() -> void:
	if selected.is_empty():
		return
	for mark in marks:
		if identity(mark["entry"]) == identity(selected) and mark["label"] == frame_name.text.strip_edges():
			var values: Array = mark["rect"] + mark["borders"]
			for i in range(8):
				fields[i].value = values[i]
			status.text = "Loaded this label's measurements in original stored pixels."
			return
	status.text = "No saved measurements for this entry and label."
