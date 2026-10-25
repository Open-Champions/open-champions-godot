extends Control

signal level_loaded(path: String)

const HISTORY_PATH := "user://level-library.json"
const MAX_HISTORY := 128
const MAX_DIRECTORY_ENTRIES := 512
const MAX_SCAN_MATCHES := 128

var _load_level: Callable
var _release_controls: Callable
var history_path := HISTORY_PATH
var _entries: Array[Dictionary] = []
var _selected_path := ""
var _list: ItemList
var _path_label: Label
var _status: Label
var _file_dialog: FileDialog
var _directory_dialog: FileDialog

func _ready() -> void:
	_build_ui()
	_read_history()
	_refresh()

func configure(load_level: Callable, release_controls: Callable) -> void:
	_load_level = load_level
	_release_controls = release_controls

func show_library() -> void:
	_enter()
	visible = true
	_status.text = "Choose a local WORLD candidate. Only a successful native load starts a fresh scene."

func _enter() -> void:
	if _release_controls.is_valid():
		_release_controls.call()

func _build_ui() -> void:
	var shade := ColorRect.new()
	shade.color = Color(0.04, 0.07, 0.12, 1.0)
	shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(shade)
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	margin.add_theme_constant_override("margin_left", 18)
	margin.add_theme_constant_override("margin_right", 18)
	margin.add_theme_constant_override("margin_top", 14)
	margin.add_theme_constant_override("margin_bottom", 14)
	add_child(margin)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 8)
	margin.add_child(column)
	var heading := Label.new()
	heading.text = "LEVEL LIBRARY  ·  LOCAL WORLD FILES"
	heading.add_theme_color_override("font_color", Color(0.91, 0.95, 1.0))
	heading.add_theme_font_size_override("font_size", 20)
	column.add_child(heading)
	var actions := HFlowContainer.new()
	actions.add_theme_constant_override("h_separation", 8)
	column.add_child(actions)
	_button(actions, "Add WORLD file", _open_file_dialog)
	_button(actions, "Scan one folder", _open_directory_dialog)
	_button(actions, "Load / retry", _load_selected)
	_button(actions, "Forget", _forget_selected)
	_button(actions, "Close", hide)
	_list = ItemList.new()
	_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_list.custom_minimum_size.y = 100
	_list.item_selected.connect(_on_selected)
	_list.item_activated.connect(_on_activated)
	column.add_child(_list)
	_path_label = Label.new()
	_path_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_path_label.clip_text = false
	_path_label.custom_minimum_size.y = 38
	column.add_child(_path_label)
	_status = Label.new()
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.custom_minimum_size.y = 42
	column.add_child(_status)
	_file_dialog = FileDialog.new()
	_file_dialog.title = "Add local WORLD file"
	_file_dialog.access = FileDialog.ACCESS_FILESYSTEM
	_file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	_file_dialog.filters = PackedStringArray(["*.world, *.WORLD ; WORLD file"])
	_file_dialog.file_selected.connect(_on_file_selected)
	_file_dialog.canceled.connect(func(): _status.text = "Selection canceled. The current scene was retained.")
	add_child(_file_dialog)
	_directory_dialog = FileDialog.new()
	_directory_dialog.title = "Scan one selected folder"
	_directory_dialog.access = FileDialog.ACCESS_FILESYSTEM
	_directory_dialog.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	_directory_dialog.dir_selected.connect(_on_directory_selected)
	_directory_dialog.canceled.connect(func(): _status.text = "Folder selection canceled. The current scene was retained.")
	add_child(_directory_dialog)

func _button(parent: Node, caption: String, action: Callable) -> void:
	var button := Button.new()
	button.text = caption
	button.custom_minimum_size.y = 36
	button.pressed.connect(action)
	parent.add_child(button)

func _open_file_dialog() -> void:
	_enter()
	_file_dialog.popup_centered_ratio(0.85)

func _open_directory_dialog() -> void:
	_enter()
	_directory_dialog.popup_centered_ratio(0.85)

func _on_file_selected(path: String) -> void:
	_add_candidate(path)
	_status.text = "Candidate added. Select Load / retry to check its current bytes."

func _on_directory_selected(path: String) -> void:
	var dir := DirAccess.open(path)
	if dir == null:
		_status.text = "Folder unavailable: " + path
		return
	var names: Array[String] = []
	dir.list_dir_begin()
	var visited := 0
	while visited < MAX_DIRECTORY_ENTRIES:
		var name := dir.get_next()
		if name.is_empty():
			break
		visited += 1
		if dir.current_is_dir() or dir.is_link(name) or name.get_extension().to_lower() != "world":
			continue
		names.append(name)
	dir.list_dir_end()
	names.sort()
	var added := 0
	for name in names.slice(0, MAX_SCAN_MATCHES):
		if _add_candidate(path.path_join(name), false):
			added += 1
	_status.text = "Added %d candidates from one folder. Scanned at most %d entries; load each to verify." % [added, MAX_DIRECTORY_ENTRIES]

func _add_candidate(path: String, evict_if_full := true) -> bool:
	if path.is_empty() or path.get_extension().to_lower() != "world":
		return false
	var absolute := ProjectSettings.globalize_path(path).simplify_path()
	for entry in _entries:
		if entry.path == absolute:
			_selected_path = absolute
			_refresh()
			return false
	if _entries.size() >= MAX_HISTORY:
		if not evict_if_full:
			return false
		var oldest := 0
		for i in _entries.size():
			if int(_entries[i].recent) < int(_entries[oldest].recent):
				oldest = i
		_entries.remove_at(oldest)
	_entries.append({"path": absolute, "name": absolute.get_file(), "state": "candidate", "recent": 0})
	_selected_path = absolute
	_save_history()
	_refresh()
	return true

func _on_selected(index: int) -> void:
	_selected_path = str(_entries[index].path)
	_path_label.text = _selected_path

func _on_activated(index: int) -> void:
	_on_selected(index)
	_load_selected()

func _load_selected() -> void:
	if _selected_path.is_empty():
		_status.text = "Select a WORLD candidate first."
		return
	_enter()
	var index := _find(_selected_path)
	if index < 0:
		return
	if not FileAccess.file_exists(_selected_path):
		_entries[index].state = "missing"
		_status.text = "Missing file. Restore it at this path or forget the candidate: " + _selected_path
		_refresh()
		return
	if not _load_level.is_valid():
		_status.text = "The scene load callback is unavailable."
		return
	if bool(_load_level.call(_selected_path)):
		record_loaded(_selected_path)
	else:
		_entries[index].state = "refused"
		var detail := _native_error()
		_status.text = "Native loader refused this file; scene retained. %s  %s" % [detail, _selected_path]
		_refresh()

# The game-folder streamed path has already prepared and committed its native
# session when it reaches here. Preserve the library history and signal once.
func record_loaded(path: String) -> void:
	var index := _find(path)
	if index < 0: return
	_entries[index].state = "recent candidate"
	_entries[index].recent = _next_recent()
	_status.text = "Loaded: " + path
	_save_history()
	_refresh()
	level_loaded.emit(path)
	hide()

func _native_error() -> String:
	var source: Object = _load_level.get_object()
	if source != null:
		var label = source.get("status_label")
		if label is Label:
			return label.text
	return ""

func _next_recent() -> int:
	var latest := 0
	for entry in _entries:
		latest = maxi(latest, int(entry.get("recent", 0)))
	return latest + 1

func _forget_selected() -> void:
	var index := _find(_selected_path)
	if index < 0:
		return
	_entries.remove_at(index)
	_selected_path = ""
	_save_history()
	_refresh()
	_status.text = "Selection forgotten. The local file was not changed."

func _find(path: String) -> int:
	for i in _entries.size():
		if _entries[i].path == path:
			return i
	return -1

func _refresh() -> void:
	_list.clear()
	_entries.sort_custom(func(a: Dictionary, b: Dictionary):
		if int(a.recent) != int(b.recent):
			return int(a.recent) > int(b.recent)
		return str(a.path) < str(b.path))
	for entry in _entries:
		var state := str(entry.state)
		if not FileAccess.file_exists(str(entry.path)):
			state = "missing"
		var index := _list.item_count
		_list.add_item("%s  ·  %s  [%s]" % [entry.name, str(entry.path).get_base_dir().get_file(), state])
		_list.set_item_tooltip(index, str(entry.path))
		if entry.path == _selected_path:
			_list.select(index)
	_path_label.text = _selected_path if not _selected_path.is_empty() else "Select a row to see its full path."

func _read_history() -> void:
	if not FileAccess.file_exists(history_path):
		return
	var data = JSON.parse_string(FileAccess.get_file_as_string(history_path))
	if not data is Array:
		return
	for item in data.slice(0, MAX_HISTORY):
		if not item is Dictionary or not item.get("path") is String:
			continue
		var path: String = item.path
		if path.is_absolute_path() and path.get_extension().to_lower() == "world" and _find(path) < 0:
			_entries.append({"path": path, "name": path.get_file(), "state": "candidate", "recent": maxi(0, int(item.get("recent", 0)))})

func _save_history() -> void:
	var saved: Array[Dictionary] = []
	for entry in _entries:
		saved.append({"path": entry.path, "name": entry.name, "recent": entry.recent})
	var file := FileAccess.open(history_path, FileAccess.WRITE)
	if file != null:
		file.store_string(JSON.stringify(saved))
		file.close()
