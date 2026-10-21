extends Control

# Host layout around the native front-end flow (ChampionsFrontendService,
# docs/FRONTEND.md). Every screen change, timing, slider step and commit is
# native. This script maps host keys to pad bits, performs the flow's requests
# (file choosers, the play session, audio) and draws a HOST layout: the
# research does not document these screens' positions, frames, colours or
# sounds, so nothing drawn here is presented as the original picture. The one
# documented style used is the pause text colour 0x80464646.

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
const KEY_HELP := "Host keys: arrows = d-pad · Enter/Space/X = cross · Backspace/C/Esc = circle · T/Tab = triangle · P = start"
const PAUSE_TEXT := 0x80464646 # pauseMenu_task text_begin colour (Observed)
const HOST_TEXT := 0x80707070  # host colour: front-end colours are not documented
const MUSIC_FRONT := "mx_maintheme_l.vag"

var scene: Control
var service: RefCounted
var layout_service: Object
var dynamic_service: Object
var view := {}
var booted := false
var key_bits := {}
var accumulator := 0.0
var options_path := "user://frontend/options.img"
var queued_paths := {} # request kind -> path, used by tests instead of a dialog
var log_lines: Array[String] = []
var host_error := ""
var name_buffer := ""
var font_bytes := PackedByteArray()
var font_texture: ImageTexture
var start_mode := "" # how the last start_play request was served

func _ready() -> void:
	mouse_filter = MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(PRESET_FULL_RECT)
	visible = false
	if ClassDB.class_exists("ChampionsFrontendService"):
		service = ClassDB.instantiate("ChampionsFrontendService")
	if ClassDB.class_exists("ChampionsUiLayoutService"):
		layout_service = ClassDB.instantiate("ChampionsUiLayoutService")
	if ClassDB.class_exists("ChampionsUiDynamicLayoutService"):
		dynamic_service = ClassDB.instantiate("ChampionsUiDynamicLayoutService")
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--frontend-options="):
			options_path = argument.trim_prefix("--frontend-options=")

func configure(owner: Control) -> void:
	scene = owner

func available() -> bool:
	return service != null

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
	if not active():
		# In play only start (P) reaches the flow; everything else is the game's.
		if key != KEY_P or scene == null or not scene.session_started or scene.stopped:
			return false
		if not booted or screen() != "playing":
			if not attach().get("ok", false):
				return false
		_set_key(key, event.pressed)
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
			"front_end_music": _front_end_music()
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

func _front_end_music() -> void:
	if scene != null and scene.audio != null:
		scene.audio.reset(false)

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
	if character.get("ready", false):
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
	var real = scene.get("real")
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
		var adopted: Dictionary = scene.game_menus.service.adopt(character)
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

func sync_font(art) -> void:
	if font_texture != null or art == null:
		return
	var hud = art.get("hud_view")
	if hud == null or hud.font_bytes.is_empty() or hud.font_texture == null:
		return
	font_bytes = hud.font_bytes
	font_texture = hud.font_texture
	queue_redraw()

func _refresh() -> void:
	visible = active()
	if scene != null and scene.has_method("_show_menu") and scene.is_inside_tree():
		scene._show_menu(scene.flow)
	queue_redraw()

func _word(tint: int) -> Color:
	var channel := func(shift: int) -> float:
		return clampf(float((tint >> shift) & 255) / 128.0, 0.0, 1.0)
	return Color(channel.call(0), channel.call(8), channel.call(16), 1.0)

func _game_text(x: int, y: int, text: String, colour: int) -> bool:
	if font_texture == null or font_bytes.is_empty() or layout_service == null or text.is_empty():
		return false
	var named: Dictionary = layout_service.hud_text_glyphs(font_bytes, text.to_utf8_buffer(), x, y, colour)
	if not named.get("ok", false):
		return false
	for glyph in named.glyphs:
		var sw := int(glyph.source_width)
		var sh := int(glyph.source_height)
		if sw <= 0 or sh <= 0:
			continue
		draw_texture_rect_region(font_texture, Rect2(float(glyph.x_half) * 0.5, float(glyph.y), sw, sh),
			Rect2(int(glyph.source_x), int(glyph.source_y), sw, sh), _word(int(glyph.tint)), false)
	return true

func _host_text(pos: Vector2, text: String, size_px: int, colour: Color, width := -1.0, centred := false) -> void:
	var font := ThemeDB.fallback_font
	var align := HORIZONTAL_ALIGNMENT_CENTER if centred else HORIZONTAL_ALIGNMENT_LEFT
	if width > 0.0:
		draw_multiline_string(font, pos, text, align, width, size_px, -1, colour)
	else:
		draw_string(font, pos, text, align, -1, size_px, colour)

func _draw() -> void:
	if not active():
		return
	var name := screen()
	var pause_family: bool = name in ["pause", "exit-confirm"] or (name in ["options", "options-load", "load-game"] and scene != null and scene.session_started and not view.get("in_frontend", true))
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.02, 0.02, 0.03, 0.8 if pause_family else 1.0), true)
	var scale := minf(size.x / 640.0, size.y / 480.0)
	if scale <= 0.0:
		return
	var offset := Vector2((size.x - 640.0 * scale) * 0.5, (size.y - 480.0 * scale) * 0.5)
	draw_set_transform(offset, 0.0, Vector2(scale, scale * 480.0 / 448.0))
	var muted := Color(0.62, 0.66, 0.72)
	var title := "%s" % name
	if int(view.get("state", -1)) >= 0:
		title += " · state %d" % int(view.state)
	_host_text(Vector2(16, 22), "HOST LAYOUT — " + title, 13, Color(0.95, 0.78, 0.35))
	match name:
		"soe-logo":
			_host_text(Vector2(0, 200), "Publisher logo (picture not drawn) · frame %d of 240 · any button skips the logo and the film" % int(view.get("frames", 0)), 13, muted, 640, true)
		"intro-movie":
			_host_text(Vector2(0, 200), "Intro film (MPEG-2, not decoded) · a button after frame 30 ends it · frame %d" % int(view.get("frames", 0)), 13, muted, 640, true)
		"credits":
			_host_text(Vector2(0, 200), "Credits: credits_run was not read. Refused. Circle returns.", 14, muted, 640, true)
		"playing":
			pass
	var entries: Array = view.get("entries", [])
	var cursor := int(view.get("cursor", 0))
	# Host placement: one centred column; two columns for long lists; the exit
	# prompt side by side because its documented moves are left and right.
	var per_column := entries.size() if entries.size() <= 8 else int(ceil(entries.size() / 2.0))
	var bottom := 120
	var y := 120
	for index in range(entries.size()):
		var entry: Dictionary = entries[index]
		var selected := index == cursor
		var enabled := bool(entry.enabled)
		var cx := 320
		var half := 170
		if name == "exit-confirm":
			cx = 240 + index * 160
			y = 200
			half = 70
		elif entries.size() > 8:
			cx = 190 + (index / per_column) * 260
			y = 120 + (index % per_column) * 28
			half = 120
		if selected:
			draw_rect(Rect2(cx - half, y - 13, half * 2, 24), Color(0.95, 0.78, 0.35, 0.18), true)
			_host_text(Vector2(cx - half - 16, y + 4), ">", 15, Color(0.95, 0.78, 0.35))
		var label := str(entry.label)
		var colour := PAUSE_TEXT if pause_family else HOST_TEXT
		if not enabled:
			colour = 0x80303030
		if not _game_text(cx, y - 12, label, colour):
			var tint := Color(0.92, 0.92, 0.92) if enabled else Color(0.45, 0.45, 0.45)
			if pause_family and enabled:
				tint = _word(PAUSE_TEXT)
			_host_text(Vector2(cx - half, y), label, 15, tint, half * 2, true)
		var step := 28
		if int(entry.type) == 4:
			var value := float(_bits_to_float(int(entry.value)))
			draw_rect(Rect2(270, y + 14, 100, 6), Color(0.2, 0.2, 0.24), true)
			draw_rect(Rect2(270, y + 14, 100.0 * clampf(value, 0.0, 1.0), 6), Color(0.95, 0.78, 0.35), true)
			_host_text(Vector2(378, y + 21), "%.4f (0x%08x)" % [value, int(entry.value)], 10, muted)
			step = 40
		if not enabled and not str(entry.refusal).is_empty():
			_host_text(Vector2(500, y), "refused: " + str(entry.refusal), 10, Color(0.85, 0.45, 0.4), 136)
		bottom = maxi(bottom, y + step - 12)
		if name != "exit-confirm" and entries.size() <= 8:
			y += step
	var character: Dictionary = view.get("character", {})
	if name.begins_with("char-"):
		var stats: Array = character.get("stats", [0, 0, 0, 0])
		_host_text(Vector2(16, 60), "Model %d · class %d · points left %d · stats %s%s" % [int(character.get("model", 0)), int(character.get("class_index", 0)), int(view.get("ability_remaining", 0)), str(stats), "" if character.get("base_known", false) else " (start table not loaded: spent points only)"], 12, muted, 608)
		if name == "char-name":
			_host_text(Vector2(16, 80), "Type the name on the host keyboard; Backspace deletes. Then Done.", 12, muted, 608)
	if name == "options" or name == "options-load":
		_host_text(Vector2(16, 60), "music live 0x%08x · image 0x%08x · options file %s" % [int(view.get("music_live", 0)), int(view.get("music_image", 0)), "present" if view.get("options_file", false) else "absent"], 12, muted, 608)
	var notes_y := maxi(300, bottom + 20)
	var error := host_error if not host_error.is_empty() else str(view.get("error", ""))
	if not error.is_empty():
		_host_text(Vector2(16, notes_y), "Refused: " + error, 12, Color(0.95, 0.5, 0.45), 608)
		notes_y += 18
	for note in view.get("notes", []):
		var line := "· " + str(note)
		var height := int(ThemeDB.fallback_font.get_multiline_string_size(line, HORIZONTAL_ALIGNMENT_LEFT, 608, 10).y)
		if notes_y + height > 434:
			break
		_host_text(Vector2(16, notes_y), line, 10, muted, 608)
		notes_y += height + 2
	_host_text(Vector2(16, 444), KEY_HELP, 10, Color(0.5, 0.55, 0.6))

func _bits_to_float(bits: int) -> float:
	var buffer := PackedByteArray()
	buffer.resize(4)
	buffer.encode_u32(0, bits)
	return buffer.decode_float(0)
