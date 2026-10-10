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
	"Return to Arms · archive metadata compared with private game files. Assets and gameplay are not decoded.",
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
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for edge in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + edge, 32)
	add_child(margin)
	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 18)
	margin.add_child(layout)
	var header := HBoxContainer.new()
	layout.add_child(header)
	var title_group := VBoxContainer.new()
	title_group.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title_group.add_theme_constant_override("separation", 5)
	header.add_child(title_group)
	title_group.add_child(_label("OPEN CHAMPIONS", 14, GOLD))
	title_group.add_child(_label("Archive browser", 32))
	title_group.add_child(_label("Explore the files behind the world.", 16, MUTED))
	var badge := _label("METADATA ONLY", 13, GOLD)
	badge.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	header.add_child(badge)
	var profile_panel := PanelContainer.new()
	profile_panel.add_theme_stylebox_override("panel", _box(PANEL))
	layout.add_child(profile_panel)
	var profile_layout := VBoxContainer.new()
	profile_layout.add_theme_constant_override("separation", 9)
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
	results.add_theme_constant_override("separation", 10)
	results_panel.add_child(results)
	archive_name = _label("No archive selected", 20)
	results.add_child(archive_name)
	archive_path = _label("GOB and LMP · up to 64 MiB · read only", 13, MUTED)
	archive_path.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	results.add_child(archive_path)
	status = _label("Ready to inspect your own game files", 14, MUTED)
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	results.add_child(status)
	table = Tree.new()
	table.size_flags_vertical = Control.SIZE_EXPAND_FILL
	table.columns = 5
	table.hide_root = true
	table.column_titles_visible = true
	table.select_mode = Tree.SELECT_ROW
	table.add_theme_stylebox_override("panel", _box(BACKGROUND, BORDER, 6))
	var headings := ["Entry", "Container", "Type", "Offset (bytes)", "Size (bytes)"]
	for column in range(headings.size()):
		table.set_column_title(column, headings[column])
		table.set_column_expand(column, column < 2)
		table.set_column_custom_minimum_width(column, 72 if column == 2 else 125)
	table.visible = false
	results.add_child(table)
	empty_state = VBoxContainer.new()
	empty_state.size_flags_vertical = Control.SIZE_EXPAND_FILL
	empty_state.alignment = BoxContainer.ALIGNMENT_CENTER
	empty_state.add_theme_constant_override("separation", 12)
	results.add_child(empty_state)
	empty_title = _label("Start with an archive", 24)
	empty_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	empty_state.add_child(empty_title)
	empty_description = _label("Open a GOB or LMP from your own game files.\nEntry names, sizes and nested containers will appear here.", 16, MUTED)
	empty_description.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	empty_description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	empty_state.add_child(empty_description)
	var footer := _label("Files stay on your device. Nothing is extracted, modified or uploaded.  •  No assets or gameplay are decoded.", 13, MUTED)
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

func _profile_changed(index: int) -> void:
	_clear()
	evidence.text = PROFILE_NOTES[index]
	if inspector == null:
		_show_error("The archive reader is unavailable. Build the pinned native extension, then restart the browser.")

func _clear() -> void:
	rows.clear()
	current_path = ""
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
	rows.clear()
	search.clear()
	search.editable = false
	status.text = message
	status.add_theme_color_override("font_color", RED)
	_show_empty("Archive could not be read", "Choose another file to try again.\nNo partial results were kept.")

func open_archive(path: String) -> void:
	if inspector == null:
		return
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
	search.editable = not rows.is_empty()
	status.add_theme_color_override("font_color", MUTED)
	status.text = "%d entries · %s · %s" % [rows.size(), String.humanize_size(result["file_size"]), GAME_NAMES[game_choice.selected]]
	if rows.is_empty():
		_show_empty("This archive is empty", "The archive is valid and has no entries.")
	else:
		_render_rows()

func _render_rows() -> void:
	if rows.is_empty():
		return
	table.clear()
	var root_item := table.create_item()
	var query := search.text.strip_edges().to_lower()
	var visible_count := 0
	for row in rows:
		if not query.is_empty() and query not in str(row["name"]).to_lower() and query not in str(row["parent"]).to_lower():
			continue
		var item := table.create_item(root_item)
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
