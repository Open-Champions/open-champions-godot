extends Control

# The front end over the native flow (ChampionsFrontendService, docs/FRONTEND.md).
# Every screen change, timing, slider step and commit is native, and so is the
# layout: Flow::layout() returns the draw calls research-menus-layout dc458d5
# documents (positions, frames, sprites, fonts, layers, colours). This script
# maps host keys to pad bits, performs the flow's requests (file choosers, the
# play session, music, menu sounds) and paints the calls through ui/frontend_view.gd
# with the user's own fonts and textures. What the research lists as Unknown is
# drawn as a labelled host layout ("HOST LAYOUT" on screen).

const FRAME_SECONDS := 1.0 / 60.0
# Host keyboard -> PS2 pad bits (bits from the research; the key choice is host).
const PAD_KEYS := {
	KEY_UP: 0x1000, KEY_DOWN: 0x4000, KEY_LEFT: 0x8000, KEY_RIGHT: 0x2000,
	KEY_ENTER: 0x40, KEY_KP_ENTER: 0x40, KEY_SPACE: 0x40, KEY_X: 0x40,
	KEY_BACKSPACE: 0x20, KEY_C: 0x20, KEY_ESCAPE: 0x20,
	KEY_T: 0x10, KEY_TAB: 0x10, KEY_P: 0x800,
}
# On the name page only these keys stay pad buttons; the rest type.
const NAME_PAD_KEYS := [KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_KP_ENTER, KEY_ESCAPE, KEY_TAB]
const KEY_HELP := "Host keys: arrows = d-pad · Enter/Space/X = cross · Backspace/C/Esc = circle · T/Tab = triangle · P = start · F5 = Save screen in play"
const FrontendView = preload("res://ui/frontend_view.gd")

var scene: Control
var service: RefCounted
var view_painter: RefCounted # ui/frontend_view.gd
var scene_view := {}          # the native scene of the current screen
var last_scene := {}
var show_notes := false       # F1: the native notes over the screen
var sounds := {}              # menu clip resource -> requests seen
var sounds_voiced := 0        # requests that played (none: the clips are not voiced)
var view := {}
var booted := false
var key_bits := {}
var accumulator := 0.0
var options_path := "user://frontend/options.img"
var queued_paths := {} # request kind -> path, used by tests instead of a dialog
var log_lines: Array[String] = []
var host_error := ""
var name_buffer := ""
var start_mode := "" # how the last start_play request was served

func _ready() -> void:
	mouse_filter = MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(PRESET_FULL_RECT)
	visible = false
	if ClassDB.class_exists("ChampionsFrontendService"):
		service = ClassDB.instantiate("ChampionsFrontendService")
	view_painter = FrontendView.new()
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--frontend-options="):
			options_path = argument.trim_prefix("--frontend-options=")

func configure(owner: Control) -> void:
	scene = owner

func available() -> bool:
	return service != null

# dialog_isOpen: offline the start button cannot open the pause menu and its recall and exit
# entries are not selectable while a conversation is open (docs/spec/dialog-gaps.md 3).
var _dialog_open := false
func set_dialog_open(open: bool) -> void:
	if service == null or not booted or open == _dialog_open: return
	_dialog_open = open
	service.set_dialog_open(open)

# Overlay drawn and taking keys: every flow screen except play itself.
func active() -> bool:
	return booted and str(view.get("screen", "")) != "playing"

func screen() -> String:
	return str(view.get("screen", ""))

# Power-on: the stored options image (or none, a blank card) into the flow.
func boot() -> Dictionary:
	if service == null:
		return {"ok": false, "error": "ChampionsFrontendService is not installed"}
	var bytes := PackedByteArray()
	if FileAccess.file_exists(options_path):
		bytes = FileAccess.get_file_as_bytes(options_path)
	view = service.boot(bytes)
	if not view.get("ok", false):
		host_error = str(view.get("error", "boot refused"))
		return view
	booted = true
	host_error = ""
	key_bits.clear()
	if scene != null and scene.session_started:
		scene.set_paused(true)
	_load_start_stats()
	_handle(view.get("requests", []))
	_refresh()
	return view

# A play session started from the host menu: join at play so start pauses.
func attach() -> Dictionary:
	if not booted:
		var booted_view := boot()
		if not booted_view.get("ok", false):
			return booted_view
	view = service.attach_play()
	_handle(view.get("requests", []))
	if scene != null and scene.session_started and not scene.stopped:
		scene.set_paused(false)
	_refresh()
	return view

# One displayed frame of pad 0.
func step(buttons: int) -> Dictionary:
	if not booted:
		return {"ok": false, "error": "front end not booted"}
	var result: Dictionary = service.frame(buttons, 128, 128)
	if result.get("ok", false):
		if str(result.get("screen", "")) != screen():
			host_error = ""
		view = result
		_handle(result.get("requests", []))
	else:
		host_error = str(result.get("error", "frame refused"))
	_refresh()
	return view

func tap(buttons: int) -> Dictionary:
	step(buttons)
	return step(0)

func held_bits() -> int:
	var bits := 0
	for value in key_bits.values():
		bits |= int(value)
	return bits

func _process(delta: float) -> void:
	if not booted:
		return
	if scene != null and scene.get("automatic_steps") == false:
		return
	accumulator = minf(accumulator + delta, FRAME_SECONDS * 8.0)
	while accumulator >= FRAME_SECONDS:
		accumulator -= FRAME_SECONDS
		step(held_bits())

# Returns true when the key belongs to the front end.
func handle_key(event: InputEvent) -> bool:
	if not event is InputEventKey or event.echo:
		return false
	var key: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
	# Host route from the researched five-entry pause menu to the documented
	# save screen. F5 is a client shortcut, not an original pause entry.
	if key == KEY_F5 and screen() in ["playing", "pause"] and scene != null and scene.session_started and not scene.stopped:
		if event.pressed: open_save_screen()
		return true
	if not active():
		# In play only start (P) reaches the flow; everything else is the game's.
		if key != KEY_P or scene == null or not scene.session_started or scene.stopped:
			return false
		if not booted or screen() != "playing":
			if not attach().get("ok", false):
				return false
		_set_key(key, event.pressed)
		return true
	if key == KEY_F1:
		if event.pressed:
			show_notes = not show_notes
			queue_redraw()
		return true
	if screen() == "char-name" and not key in NAME_PAD_KEYS:
		# Name page: the host keyboard types (the grid's characters are not documented).
		if event.pressed:
			if key == KEY_BACKSPACE:
				if not name_buffer.is_empty():
					name_buffer = name_buffer.substr(0, name_buffer.length() - 1)
					_send_name()
			elif event.unicode >= 32 and event.unicode < 127:
				name_buffer += char(event.unicode)
				_send_name()
		return true
	if PAD_KEYS.has(key):
		_set_key(key, event.pressed)
	return true

func _set_key(key: int, pressed: bool) -> void:
	if pressed:
		key_bits[key] = PAD_KEYS.get(key, 0)
	else:
		key_bits.erase(key)

func type_name(text: String) -> Dictionary:
	name_buffer = text
	return _send_name()

func _send_name() -> Dictionary:
	var result: Dictionary = service.set_name(name_buffer)
	if result.get("ok", false):
		view = result
		host_error = ""
	else:
		host_error = str(result.get("error", "name refused"))
		name_buffer = name_buffer.substr(0, maxi(0, name_buffer.length() - 1))
	_refresh()
	return result

func open_save_screen() -> Dictionary:
	if not booted or screen() != "playing":
		var joined := attach()
		if not joined.get("ok", false):
			return joined
	var result: Dictionary = service.request_save()
	if result.get("ok", false):
		view = result
		_handle(result.get("requests", []))
	else:
		host_error = str(result.get("error", "save screen refused"))
	_refresh()
	return result

func queue_path(kind: String, path: String) -> void:
	queued_paths[kind] = path

func _log(line: String) -> void:
	log_lines.append(line)
	if log_lines.size() > 400:
		log_lines.remove_at(0)

func _handle(requests: Array) -> void:
	for request in requests:
		var kind := str(request.kind)
		_log("%s %s %d" % [kind, str(request.arg), int(request.value)])
		match kind:
			"pause": _pause(int(request.value) == 1)
			"front_end_music": _front_end_music(str(request.arg), int(request.value))
			"credits_music": _credits_music(str(request.arg), int(request.value))
			"menu_sound": _menu_sound(str(request.arg))
			"apply_music_volume": _apply_volume(int(request.value))
			"preview_music": _preview(str(request.arg), int(request.value))
			"write_options": _write_options()
			"load_options": _load_options()
			"load_session": _choose("load_session", func(path): return _load_session(path))
			"save_session": _choose("save_session", func(path): return _save_session(path))
			"import_ps2": _choose("import_ps2", func(path): return _import_ps2(path))
			"start_play": _start_play(str(request.arg))
			"recall": _recall()
			"exit_session": _exit_session()

func _pause(on: bool) -> void:
	if scene == null or not scene.session_started:
		return
	scene.set_paused(on)

# The flow already applied the volume gate (music_playGlobal 0x2110e0) and kept the
# original flags; the audio side gates again on its own volume word.
func _front_end_music(_name: String, _flags: int) -> void:
	if scene != null and scene.audio != null:
		scene.audio.reset(false)

# End-credits music: the same gated music_playGlobal, flags 3.
func _credits_music(name: String, flags: int) -> void:
	_preview(name, flags)

# FX.LMP menu clips (move, select, back). The flow requests them at the documented
# triggers. They are not voiced: the clips' variation draws and the group volume
# word 0x41bac8 are not in the research (docs/FRONTEND.md).
func _menu_sound(resource: String) -> void:
	sounds[resource] = int(sounds.get(resource, 0)) + 1

func _apply_volume(bits: int) -> void:
	# The flow's live g_musicVolume word goes to the mixer bit for bit.
	if scene != null and scene.audio != null and not scene.audio.load_options_file_music(bits):
		_log("music volume refused: " + str(scene.audio.bridge.get_last_error()))

func _preview(name: String, flags: int) -> void:
	if scene == null or scene.audio == null or scene.audio.bridge == null:
		return
	if not scene.audio.bridge.has_method("music_play_global"):
		return
	if not scene.audio.bridge.music_play_global(name, flags):
		_log("preview %s not queued: %s" % [name, str(scene.audio.bridge.get_last_error())])

func _write_options() -> void:
	DirAccess.make_dir_recursive_absolute(options_path.get_base_dir())
	var file := FileAccess.open(options_path, FileAccess.WRITE)
	if file == null:
		host_error = "options file cannot be written: " + options_path
		return
	file.store_buffer(service.options_image())
	file.close()

func _load_options() -> void:
	if not FileAccess.file_exists(options_path):
		view = service.options_loaded(false, PackedByteArray())
	else:
		view = service.options_loaded(true, FileAccess.get_file_as_bytes(options_path))
	_handle(view.get("requests", []))

func _choose(kind: String, action: Callable) -> void:
	if queued_paths.has(kind):
		var path: String = queued_paths[kind]
		queued_paths.erase(kind)
		action.call(path)
		return
	var dialog: FileDialog = null
	if scene != null:
		match kind:
			"load_session": dialog = scene.load_dialog
			"save_session": dialog = scene.save_dialog
			"import_ps2": dialog = scene.get("ps2_dialog")
	if dialog == null:
		_report_refusal(kind, "no file chooser is available for " + kind)
		return
	var once := func(path: String): action.call(path)
	dialog.file_selected.connect(once, CONNECT_ONE_SHOT)
	dialog.canceled.connect(func():
		if dialog.file_selected.is_connected(once): dialog.file_selected.disconnect(once)
		_report_refusal(kind, "cancelled"), CONNECT_ONE_SHOT)
	dialog.popup_centered_ratio(0.8)

func _report_refusal(kind: String, reason: String) -> void:
	match kind:
		"load_session": view = service.load_result(false, reason)
		"save_session": view = service.save_result(false, reason)
		"import_ps2": view = service.import_result({"ok": false, "error": reason})
	_refresh()

func _load_session(path: String) -> bool:
	var ok: bool = scene != null and scene.load_session_file(path)
	view = service.load_result(ok, "" if ok else str(scene.status_label.text))
	_handle(view.get("requests", []))
	_refresh()
	return ok

func _save_session(path: String) -> bool:
	var ok: bool = scene != null and scene.save_session_file(path)
	view = service.save_result(ok, "" if ok else str(scene.status_label.text))
	_handle(view.get("requests", []))
	_refresh()
	return ok

func _import_ps2(path: String) -> Dictionary:
	var result: Dictionary = {"ok": false, "error": "PS2 import is not installed"}
	if scene != null and scene.has_method("import_ps2_save_file") and scene.get("ps2_import") != null:
		result = scene.import_ps2_save_file(path)
	view = service.import_result(result)
	_handle(view.get("requests", []))
	_refresh()
	return result

# playerSel_finish queues the hub; a slot import asks for its saved level.
func _start_play(level_name: String) -> void:
	if scene == null:
		return
	var character: Dictionary = view.get("character", {})
	var real = scene.get("real")
	if character.get("ready", false) and real != null and real.files.has("slus_209.73"):
		if not character.get("imported", false):
			var accepted: Dictionary = scene.controller.set_player_character(character)
			if not accepted.get("ok", false):
				host_error = "Player record refused: " + str(accepted.get("error", "unknown"))
				_refresh()
				return
		elif not scene.controller.get_observation().get("player_ready", false):
			host_error = "Imported player record was not accepted by the session."
			_refresh()
			return
	var loaded := false
	if real != null and not real.levels.is_empty():
		for index in range(real.levels.size()):
			if str(real.levels[index].stem).to_lower() == level_name.to_lower():
				loaded = scene.load_real_level(index)
				break
		start_mode = "level %s %s" % [level_name, "loaded" if loaded else "not found in the game folder"]
	else:
		start_mode = "no game folder: built-in play session; level %s not loaded" % level_name
	scene.start_play()
	if scene.session_started and scene.game_menus != null and scene.game_menus.service != null and character.get("ready", false):
		var imported: bool = character.get("imported", false)
		var has_executable: bool = real != null and real.files.has("slus_209.73")
		# A saved character can still enter the built-in session without a selected
		# disc. Its scalar HUD/menu state is available; live item actions remain refused.
		var adopted: Dictionary = scene.game_menus.adopt_imported() if imported and has_executable else scene.game_menus.service.adopt(character)
		if not adopted.get("ok", false):
			_log("menu ledger refused the character: " + str(adopted.get("error", "")))
	if scene.session_started:
		scene.set_paused(false)

func _recall() -> void:
	# hudmsg_sendRecall reaches recall_execute through game message 3. The play
	# session has no gate-scroll recall path, so the host refuses it.
	host_error = "Use Gate Scroll refused: the play session has no recall path (message 3 is not wired into the session)."

func _exit_session() -> void:
	if scene != null and scene.session_started:
		scene.return_to_selection()
	view = service.return_to_frontend()
	_handle(view.get("requests", []))

func _load_start_stats() -> void:
	var real = scene.get("real") if scene != null else null
	if real == null or not real.files.has("slus_209.73"):
		return
	var result: Dictionary = service.load_class_start_stats(real.files["slus_209.73"])
	if result.get("ok", false):
		view = result
	else:
		_log("class start stats refused: " + str(result.get("error", "")))

# The user's fonts and textures are read once the play scene has a game folder.
func sync_font(_art = null) -> void:
	_ensure_view()

func _ensure_view() -> void:
	if view_painter == null or scene == null:
		return
	var real = scene.get("real")
	if real == null or real.files.is_empty():
		return
	if view_painter.real == real and view_painter.has_fonts():
		return
	if view_painter.real == real and view_painter.fonts.size() == 0 and view_painter.get_meta("tried", false):
		return
	view_painter.set_meta("tried", true)
	view_painter.configure(real, scene.get("inspector"))
	last_scene = {}
	queue_redraw()

func fonts_loaded() -> bool:
	return view_painter != null and view_painter.has_fonts()

# The native draw calls for the current screen.
func _scene() -> Dictionary:
	if service == null or not booted:
		return {}
	var result: Dictionary = service.layout()
	return result if result.get("ok", false) else {}

func _refresh() -> void:
	visible = active()
	if scene != null and scene.has_method("_show_menu") and scene.is_inside_tree():
		scene._show_menu(scene.flow)
	_ensure_view()
	scene_view = _scene()
	queue_redraw()

func _is_pause_family() -> bool:
	var name := screen()
	return name in ["pause", "exit-confirm"] or (name in ["options", "options-load", "load-game"] and scene != null and scene.session_started and not view.get("in_frontend", true))

func _host_text(pos: Vector2, text: String, size_px: int, colour: Color, width := -1.0) -> void:
	var font := ThemeDB.fallback_font
	if width > 0.0:
		draw_multiline_string(font, pos, text, HORIZONTAL_ALIGNMENT_LEFT, width, size_px, -1, colour)
	else:
		draw_string(font, pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, size_px, colour)

func _draw() -> void:
	if not active():
		return
	var pause_family := _is_pause_family()
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.02, 0.02, 0.03, 0.8 if pause_family else 1.0), true)
	var factor := minf(size.x / 640.0, size.y / 480.0)
	if factor <= 0.0:
		return
	var offset := Vector2((size.x - 640.0 * factor) * 0.5, (size.y - 480.0 * factor) * 0.5)
	draw_set_transform(offset, 0.0, Vector2(factor, factor * 480.0 / 448.0))
	if scene_view.is_empty():
		scene_view = _scene()
	if scene_view != last_scene:
		view_painter.build(scene_view)
		last_scene = scene_view.duplicate(true)
	view_painter.paint(self)
	if screen() in ["char-class", "char-appearance"]:
		_draw_body_asset_preview()
	# Host labels: what the research leaves Unknown on this screen.
	var gold := Color(0.95, 0.78, 0.35)
	if bool(scene_view.get("host_layout", false)):
		_host_text(Vector2(6, 12), "HOST LAYOUT — " + str(scene_view.get("host_label", "")), 10, gold)
	var error := host_error if not host_error.is_empty() else str(view.get("error", ""))
	var notes_y := 412
	if not error.is_empty():
		_host_text(Vector2(6, notes_y), "Refused: " + error, 11, Color(0.95, 0.5, 0.45), 628)
	if show_notes:
		var y := 40
		draw_rect(Rect2(0, 28, 640, 400), Color(0, 0, 0, 0.82), true)
		_host_text(Vector2(8, y), "%s · state %d · cursor %d · music live 0x%08x image 0x%08x" % [screen(), int(view.get("state", -1)), int(view.get("cursor", 0)), int(view.get("music_live", 0)), int(view.get("music_image", 0))], 10, gold)
		y += 16
		for note in view.get("notes", []):
			var height := int(ThemeDB.fallback_font.get_multiline_string_size("· " + str(note), HORIZONTAL_ALIGNMENT_LEFT, 624, 9).y)
			if y + height > 424:
				break
			_host_text(Vector2(8, y), "· " + str(note), 9, Color(0.72, 0.76, 0.82), 624)
			y += height + 2
	_host_text(Vector2(6, 445), KEY_HELP + " · F1 = notes", 9, Color(0.45, 0.5, 0.55))

func _draw_body_asset_preview() -> void:
	# Constructor resource pairing is observed, but the menu's camera and
	# reachable appearance values are Unknown. Show only the disc body texture.
	var code := str(view.get("preview_body_code", ""))
	if code.is_empty() or view_painter == null:
		return
	var asset: Dictionary = view_painter.load_body_texture(code)
	var box := Rect2(390, 105, 180, 180)
	draw_rect(box, Color(0.08, 0.09, 0.1, 1.0), true)
	if not asset.is_empty():
		draw_texture_rect(asset.texture, box, false)
	else:
		_host_text(Vector2(396, 180), "Disc body texture unavailable", 12, Color(0.95, 0.5, 0.45), 170)
	_host_text(Vector2(390, 303), "DISC RGB TEXTURE · model %d" % int(view.get("preview_model", 0)), 10, Color(0.95, 0.78, 0.35))
	_host_text(Vector2(390, 319), "Camera and variants Unknown", 10, Color(0.72, 0.76, 0.82))

func _bits_to_float(bits: int) -> float:
	var buffer := PackedByteArray()
	buffer.resize(4)
	buffer.encode_u32(0, bits)
	return buffer.decode_float(0)
