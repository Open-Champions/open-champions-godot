extends Control

const BACKGROUND := Color("0b1112")
const PANEL := Color("132023")
const BORDER := Color("2b4144")
const INK := Color("eef0e9")
const MUTED := Color("a7bbba")
const GOLD := Color("dec384")
const RED := Color("ffb1a8")
const GAME_NAMES := ["Champions: Return to Arms", "Champions of Norrath"]
const PROFILE_NOTES := [
	"Return to Arms · bounded 32-bit, plain 4-bit and 8-bit texture previews. Supported rest-pose models can also be inspected. Gameplay remains unavailable.",
	"Norrath · compatibility is unverified. This selection labels your work; it does not detect the game or validate its formats."
]

var inspector: RefCounted
var game_choice: OptionButton
var evidence: Label
var open_button: Button
var clear_button: Button
var search: LineEdit
var archive_name: Label
var archive_path: Label
var status: Label
var table: Tree
var empty_state: VBoxContainer
var empty_title: Label
var empty_description: Label
var file_dialog: FileDialog
var rows: Array = []
var current_path := ""
var current_fingerprint := ""
var preview_button: Button
var model_button: Button
var model_viewer
var preview_image: TextureRect
var preview_info: Label
var preview_alpha: CheckButton
var preview_rgba := PackedByteArray()
var preview_width := 0
var preview_height := 0
var preview_row_index := -1

func _ready() -> void:
	_build_interface()
	if ClassDB.class_exists("ChampionsArchiveInspector"):
		inspector = ClassDB.instantiate("ChampionsArchiveInspector")
	else:
		open_button.disabled = true
		_show_error("The archive reader is unavailable. Build the pinned native extension, then restart the browser.")
	if "--smoke-test" in OS.get_cmdline_user_args():
		call_deferred("_export_smoke_test")

func _box(color: Color, border: Color = BORDER, radius: int = 10) -> StyleBoxFlat:
	var box := StyleBoxFlat.new()
	box.bg_color = color
	box.border_color = border
	box.set_border_width_all(1)
	box.set_corner_radius_all(radius)
	box.content_margin_left = 16
	box.content_margin_right = 16
	box.content_margin_top = 12
	box.content_margin_bottom = 12
	return box

func _label(text: String, size: int = 16, color: Color = INK) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", size)
	label.add_theme_color_override("font_color", color)
	return label

func _button(text: String, primary := false) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size.y = 44
	button.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	button.add_theme_stylebox_override("normal", _box(GOLD if primary else PANEL))
	button.add_theme_stylebox_override("hover", _box(Color("ebd49e") if primary else Color("203336")))
	button.add_theme_stylebox_override("pressed", _box(Color("c8a86a") if primary else BORDER))
	button.add_theme_stylebox_override("focus", _box(Color(0, 0, 0, 0), GOLD))
	button.add_theme_color_override("font_color", BACKGROUND if primary else INK)
	button.add_theme_color_override("font_hover_color", BACKGROUND if primary else INK)
	button.add_theme_color_override("font_pressed_color", BACKGROUND if primary else INK)
	return button

func _build_interface() -> void:
	var theme_resource := Theme.new()
	theme_resource.default_font_size = 16
	theme = theme_resource
	var background := ColorRect.new()
	background.color = BACKGROUND
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)
	# The page may exceed a short window (or grow when an error wraps). Keep
	# every control reachable instead of letting container minimums clip it.
	var page_scroll := ScrollContainer.new()
	page_scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	page_scroll.follow_focus = true
	add_child(page_scroll)
	var margin := MarginContainer.new()
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	margin.size_flags_vertical = Control.SIZE_EXPAND_FILL
	for edge in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + edge, 20)
	page_scroll.add_child(margin)
	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 12)
	margin.add_child(layout)
	var header := HBoxContainer.new()
	layout.add_child(header)
	var title_group := VBoxContainer.new()
	title_group.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title_group.add_theme_constant_override("separation", 5)
	header.add_child(title_group)
	title_group.add_child(_label("OPEN CHAMPIONS", 14, GOLD))
	title_group.add_child(_label("Archive browser", 28))
	var badge := _label("ARCHIVES + ASSET PREVIEW", 13, GOLD)
	badge.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	header.add_child(badge)
	var profile_panel := PanelContainer.new()
	profile_panel.add_theme_stylebox_override("panel", _box(PANEL))
	layout.add_child(profile_panel)
	var profile_layout := VBoxContainer.new()
	profile_layout.add_theme_constant_override("separation", 6)
	profile_panel.add_child(profile_layout)
	var profile_row := HBoxContainer.new()
	profile_row.add_theme_constant_override("separation", 16)
	profile_layout.add_child(profile_row)
	profile_row.add_child(_label("GAME", 12, MUTED))
	game_choice = OptionButton.new()
	game_choice.custom_minimum_size = Vector2(320, 36)
	for title in GAME_NAMES:
		game_choice.add_item(title)
	game_choice.item_selected.connect(_profile_changed)
	profile_row.add_child(game_choice)
	var profile_hint := _label("Choose the game your archive belongs to", 14, MUTED)
	profile_hint.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	profile_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	profile_row.add_child(profile_hint)
	evidence = _label(PROFILE_NOTES[0], 14, MUTED)
	evidence.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	profile_layout.add_child(evidence)
	var toolbar := HBoxContainer.new()
	toolbar.add_theme_constant_override("separation", 10)
	layout.add_child(toolbar)
	open_button = _button("Open archive…", true)
	open_button.pressed.connect(func(): file_dialog.popup_centered_ratio(0.75))
	toolbar.add_child(open_button)
	clear_button = _button("Clear")
	clear_button.disabled = true
	clear_button.pressed.connect(_clear)
	toolbar.add_child(clear_button)
	search = LineEdit.new()
	search.placeholder_text = "Filter entry names or containers"
	search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	search.add_theme_stylebox_override("normal", _box(PANEL))
	search.add_theme_stylebox_override("focus", _box(PANEL, GOLD))
	search.text_changed.connect(func(_text): _render_rows())
	search.editable = false
	toolbar.add_child(search)
	var results_panel := PanelContainer.new()
	results_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	results_panel.add_theme_stylebox_override("panel", _box(PANEL))
	layout.add_child(results_panel)
	var results := VBoxContainer.new()
	results.add_theme_constant_override("separation", 6)
	results_panel.add_child(results)
	archive_name = _label("No archive selected", 18)
	archive_name.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	results.add_child(archive_name)
	archive_path = _label("GOB and LMP · up to 64 MiB · read only", 13, MUTED)
	archive_path.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	results.add_child(archive_path)
	status = _label("Ready to inspect your own game files", 14, MUTED)
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	results.add_child(status)
	var content_row := HBoxContainer.new()
	content_row.size_flags_vertical = Control.SIZE_EXPAND_FILL
	content_row.add_theme_constant_override("separation", 16)
	results.add_child(content_row)
	var table_area := VBoxContainer.new()
	table_area.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	table_area.size_flags_vertical = Control.SIZE_EXPAND_FILL
	content_row.add_child(table_area)
	table = Tree.new()
	table.size_flags_vertical = Control.SIZE_EXPAND_FILL
	table.columns = 5
	table.hide_root = true
	table.column_titles_visible = true
	table.select_mode = Tree.SELECT_ROW
	table.item_selected.connect(_selected_entry_changed)
	table.add_theme_stylebox_override("panel", _box(BACKGROUND, BORDER, 6))
	var headings := ["Entry", "Container", "Type", "Offset (bytes)", "Size (bytes)"]
	for column in range(headings.size()):
		table.set_column_title(column, headings[column])
		table.set_column_expand(column, column < 2)
		table.set_column_custom_minimum_width(column, 72 if column == 2 else 125)
	table.visible = false
	table_area.add_child(table)
	empty_state = VBoxContainer.new()
	empty_state.size_flags_vertical = Control.SIZE_EXPAND_FILL
	empty_state.alignment = BoxContainer.ALIGNMENT_CENTER
	empty_state.add_theme_constant_override("separation", 12)
	table_area.add_child(empty_state)
	empty_title = _label("Start with an archive", 24)
	empty_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	empty_state.add_child(empty_title)
	empty_description = _label("Open a GOB or LMP from your own game files.\nEntry names, sizes and nested containers will appear here.", 16, MUTED)
	empty_description.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	empty_description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	empty_state.add_child(empty_description)
	var preview_panel := PanelContainer.new()
	preview_panel.custom_minimum_size.x = 280
	preview_panel.add_theme_stylebox_override("panel", _box(BACKGROUND, BORDER, 6))
	content_row.add_child(preview_panel)
	var preview_layout := VBoxContainer.new()
	preview_layout.add_theme_constant_override("separation", 6)
	preview_panel.add_child(preview_layout)
	preview_layout.add_child(_label("Asset preview", 18))
	var preview_scope := _label("RTA · 32-bit / plain 4-bit / 8-bit", 13, MUTED)
	preview_layout.add_child(preview_scope)
	preview_button = _button("Preview selected texture")
	preview_button.custom_minimum_size.y = 36
	preview_button.disabled = true
	preview_button.pressed.connect(_preview_selected)
	preview_layout.add_child(preview_button)
	model_button = _button("Preview selected model")
	model_button.custom_minimum_size.y = 36
	model_button.disabled = true
	model_button.pressed.connect(_preview_model)
	preview_layout.add_child(model_button)
	preview_image = TextureRect.new()
	preview_image.custom_minimum_size = Vector2(220, 128)
	preview_image.size_flags_vertical = Control.SIZE_EXPAND_FILL
	preview_image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	preview_image.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	preview_image.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	preview_layout.add_child(preview_image)
	preview_alpha = CheckButton.new()
	preview_alpha.text = "Show alpha channel"
	preview_alpha.disabled = true
	preview_alpha.toggled.connect(func(_enabled): _render_preview())
	preview_layout.add_child(preview_alpha)
	preview_info = _label("Select .tex/.etex for textures or .vif for rest-pose models.", 13, MUTED)
	preview_info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	preview_layout.add_child(preview_info)
	var footer := _label("Files stay on your device. Nothing is saved, modified or uploaded.  •  Asset previews are not a playable game.", 13, MUTED)
	footer.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var footer_row := HBoxContainer.new()
	footer_row.add_theme_constant_override("separation", 18)
	footer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	footer_row.add_child(footer)
	var licenses_button := _button("Licenses")
	licenses_button.pressed.connect(_show_licenses)
	footer_row.add_child(licenses_button)
	layout.add_child(footer_row)
	file_dialog = FileDialog.new()
	file_dialog.title = "Open a Champions archive"
	file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	file_dialog.access = FileDialog.ACCESS_FILESYSTEM
	file_dialog.use_native_dialog = true
	file_dialog.filters = PackedStringArray(["*.gob,*.GOB ; GOB archives", "*.lmp,*.LMP ; LMP archives"])
	file_dialog.file_selected.connect(open_archive)
	add_child(file_dialog)
	model_viewer = load("res://ui/model_viewer.tscn").instantiate()
	add_child(model_viewer)

func _profile_changed(index: int) -> void:
	_clear()
	evidence.text = PROFILE_NOTES[index]
	if inspector == null:
		_show_error("The archive reader is unavailable. Build the pinned native extension, then restart the browser.")

func _clear() -> void:
	_clear_preview()
	rows.clear()
	current_path = ""
	current_fingerprint = ""
	search.clear()
	search.editable = false
	clear_button.disabled = true
	archive_name.text = "No archive selected"
	archive_path.text = "GOB and LMP · up to 64 MiB · read only"
	archive_path.tooltip_text = ""
	status.text = "Ready to inspect your own game files"
	status.add_theme_color_override("font_color", MUTED)
	_show_empty("Start with an archive", "Open a GOB or LMP from your own game files.\nEntry names, sizes and nested containers will appear here.")

func _show_empty(title: String, detail: String) -> void:
	table.clear()
	table.hide()
	empty_state.show()
	empty_title.text = title
	empty_description.text = detail

func _show_error(message: String) -> void:
	_clear_preview()
	current_fingerprint = ""
	rows.clear()
	search.clear()
	search.editable = false
	status.text = message
	status.add_theme_color_override("font_color", RED)
	_show_empty("Archive could not be read", "Choose another file to try again.\nNo partial results were kept.")

func open_archive(path: String) -> void:
	if inspector == null:
		return
	_clear_preview()
	current_fingerprint = ""
	current_path = path
	archive_name.text = path.get_file()
	archive_path.text = path
	archive_path.tooltip_text = path
	clear_button.disabled = false
	search.clear()
	var result: Dictionary = inspector.inspect_archive(path)
	if not result.get("ok", false):
		_show_error(str(result.get("error", "The archive could not be read.")))
		return
	rows = result["rows"]
	current_fingerprint = result["fingerprint"]
	search.editable = not rows.is_empty()
	status.add_theme_color_override("font_color", MUTED)
	status.text = "%d entries · %s · %s" % [rows.size(), String.humanize_size(result["file_size"]), GAME_NAMES[game_choice.selected]]
	if rows.is_empty():
		_show_empty("This archive is empty", "The archive is valid and has no entries.")
	else:
		_render_rows()

func _render_rows() -> void:
	_clear_preview()
	if rows.is_empty():
		return
	table.clear()
	var root_item := table.create_item()
	var query := search.text.strip_edges().to_lower()
	var visible_count := 0
	for row_index in range(rows.size()):
		var row: Dictionary = rows[row_index]
		if not query.is_empty() and query not in str(row["name"]).to_lower() and query not in str(row["parent"]).to_lower():
			continue
		var item := table.create_item(root_item)
		item.set_metadata(0, row_index)
		item.set_text(0, row["name"])
		item.set_tooltip_text(0, row["name"])
		item.set_text(1, row["parent"] if row["parent"] != "" else "—")
		item.set_tooltip_text(1, row["parent"])
		item.set_text(2, row["kind"])
		item.set_text(3, str(row["offset"]))
		item.set_text(4, str(row["size"]))
		item.set_text_alignment(3, HORIZONTAL_ALIGNMENT_RIGHT)
		item.set_text_alignment(4, HORIZONTAL_ALIGNMENT_RIGHT)
		visible_count += 1
	if visible_count == 0:
		_show_empty("No matching entries", "Try a different name or clear the filter.")
	else:
		empty_state.hide()
		table.show()

func _clear_preview() -> void:
	if model_viewer != null:
		model_viewer.clear_model()
	model_button.disabled = true
	preview_row_index = -1
	preview_rgba = PackedByteArray()
	preview_width = 0
	preview_height = 0
	preview_image.texture = null
	preview_alpha.set_pressed_no_signal(false)
	preview_alpha.disabled = true
	preview_button.disabled = true
	preview_info.text = "Select .tex/.etex for textures or .vif for rest-pose models."
	preview_info.add_theme_color_override("font_color", MUTED)

func _selected_entry_changed() -> void:
	_clear_preview()
	var selected := table.get_selected()
	if selected == null:
		return
	var row_index: int = selected.get_metadata(0)
	if row_index < 0 or row_index >= rows.size():
		return
	preview_row_index = row_index
	if game_choice.selected != 0:
		preview_info.text = "Norrath asset compatibility is unverified. Preview is available only for Return to Arms."
		return
	var entry_name := str(rows[row_index]["name"]).to_lower()
	if entry_name.ends_with(".vif"):
		model_button.disabled = false
		preview_info.text = "Rest-pose inspection with a neutral material. Supported static layouts only; no animation or gameplay."
	if entry_name.ends_with(".tex") or entry_name.ends_with(".etex"):
		preview_button.disabled = false
		preview_info.text = "Ready to inspect this entry. World cells and 4-bit textures uploaded as 32-bit data remain unsupported."

func _preview_selected() -> void:
	var row_index := preview_row_index
	_clear_preview()
	if inspector == null or row_index < 0:
		return
	preview_row_index = row_index
	var profile := "rta" if game_choice.selected == 0 else "norrath"
	var result: Dictionary = inspector.inspect_texture(current_path, row_index, profile, current_fingerprint)
	if not result.get("ok", false):
		preview_info.text = str(result.get("error", "This texture is unsupported."))
		preview_info.add_theme_color_override("font_color", RED)
		return
	preview_rgba = result["rgba"]
	preview_width = result["width"]
	preview_height = result["height"]
	preview_alpha.disabled = false
	preview_button.disabled = false
	_render_preview()

func _preview_model(open_window := true) -> void:
	var row_index := preview_row_index
	_clear_preview()
	if inspector == null or row_index < 0:
		return
	preview_row_index = row_index
	var profile := "rta" if game_choice.selected == 0 else "norrath"
	var result: Dictionary = inspector.inspect_model(current_path, row_index, profile, current_fingerprint)
	if not model_viewer.show_model(result, open_window):
		preview_info.text = model_viewer.last_error
		preview_info.add_theme_color_override("font_color", RED)
		return
	model_button.disabled = false
	preview_info.text = "%d visible meshes · %d triangles. Rest-pose model viewer ready." % [model_viewer.mesh_count, model_viewer.triangle_count]

func _preview_pixels() -> PackedByteArray:
	var display_rgba := preview_rgba.duplicate()
	for offset in range(0, display_rgba.size(), 4):
		if preview_alpha.button_pressed:
			var alpha := display_rgba[offset + 3]
			display_rgba[offset] = alpha
			display_rgba[offset + 1] = alpha
			display_rgba[offset + 2] = alpha
		display_rgba[offset + 3] = 255
	return display_rgba

func _render_preview() -> void:
	if preview_rgba.is_empty():
		return
	var image := Image.create_from_data(preview_width, preview_height, false, Image.FORMAT_RGBA8, _preview_pixels())
	if image == null or image.is_empty():
		_clear_preview()
		preview_info.text = "The decoded texture could not be displayed."
		return
	preview_image.texture = ImageTexture.create_from_image(image)
	preview_info.text = "%d × %d · %s\nStored rows · alpha 0–128 → 0–255.\nGame blending is not reproduced." % [preview_width, preview_height, "alpha channel (scaled)" if preview_alpha.button_pressed else "RGB (alpha ignored)"]

func _export_smoke_test() -> void:
	var smoke = load("res://tests/smoke_cases.gd").new()
	var failures: int = smoke.run(inspector, self)
	print("Open Champions browser smoke: ", "PASS" if failures == 0 else "FAIL")
	get_tree().quit(0 if failures == 0 else 1)

func _show_licenses() -> void:
	var dialog := AcceptDialog.new()
	dialog.title = "Open Champions · licenses and notices"
	var content := VBoxContainer.new()
	content.custom_minimum_size = Vector2(700, 420)
	content.add_theme_constant_override("separation", 10)
	dialog.add_child(content)
	var selector := OptionButton.new()
	var labels := ["Open Champions · MIT", "godot-cpp · MIT", "Godot Engine · MIT", "Godot third-party notices"]
	var paths := ["open-champions", "godot-cpp", "godot", "godot-third-party"]
	for label in labels:
		selector.add_item(label)
	content.add_child(selector)
	var notice := TextEdit.new()
	notice.editable = false
	notice.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	notice.custom_minimum_size = Vector2(680, 360)
	notice.size_flags_vertical = Control.SIZE_EXPAND_FILL
	notice.add_theme_font_size_override("font_size", 13)
	content.add_child(notice)
	var load_notice := func(index: int):
		var path: String = "res://licenses/" + str(paths[index]) + ".txt"
		notice.text = FileAccess.get_file_as_string(path) if FileAccess.file_exists(path) else "License notice is missing from this build."
	selector.item_selected.connect(load_notice)
	load_notice.call(0)
	add_child(dialog)
	dialog.confirmed.connect(dialog.queue_free)
	dialog.canceled.connect(dialog.queue_free)
	dialog.popup_centered()
