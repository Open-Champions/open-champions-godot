extends SceneTree

# Drives the front-end flow through every screen on the play scene: boot card
# check, logo, film, main menu, game type, players, mode, difficulty, character
# class / appearance / ability / name, options (slider, commit, load), credits,
# load (native session and PS2 import), save, pause, pause options, exit prompt.
# Files go to a per-process user:// directory (shared app_userdata, wave 17).

const UP := 0x1000
const DOWN := 0x4000
const LEFT := 0x8000
const RIGHT := 0x2000
const CROSS := 0x40
const CIRCLE := 0x20
const TRIANGLE := 0x10
const START := 0x800

var scene
var fe
var checks := 0
var failures := 0
var dir := ""
var visited := {}

func _initialize() -> void:
	call_deferred("run")

func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(message)

func key(code: int, pressed := true, unicode := 0) -> InputEventKey:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = pressed
	event.unicode = unicode
	return event

func tap(bits: int) -> Dictionary:
	var v: Dictionary = fe.tap(bits)
	visited[str(v.get("screen", ""))] = true
	return v

func frames(bits: int, count: int) -> Dictionary:
	var v: Dictionary = {}
	for i in range(count):
		v = fe.step(bits)
	visited[str(v.get("screen", ""))] = true
	return v

func kinds() -> Array:
	var out := []
	for line in fe.log_lines:
		out.append(str(line).split(" ")[0])
	return out

func to_char_class() -> void:
	for i in range(5):
		tap(CROSS)

func finish_character(name: String) -> void:
	tap(CROSS) # class shown first
	tap(CROSS) # default appearance
	for i in range(20):
		tap(RIGHT)
	tap(CROSS)
	fe.type_name(name)
	tap(DOWN)
	tap(CROSS)

func run() -> void:
	root.size = Vector2i(1280, 720)
	root.content_scale_size = root.size
	dir = "user://frontend-smoke-%d" % OS.get_process_id()
	DirAccess.make_dir_recursive_absolute(dir)
	scene = load("res://scenes/play_flow.tscn").instantiate()
	scene.automatic_steps = false
	scene.component_factory = func(_id): return null
	root.add_child(scene)
	await process_frame
	fe = scene.frontend
	check(fe != null and fe.available(), "front end attached with ChampionsFrontendService")
	check(scene.actions.has("frontend") and not scene.actions.frontend.disabled, "title action offered")
	fe.options_path = dir.path_join("options.img")

	# Blank card: create, then the logo runs its 240 frames into the film.
	var v: Dictionary = fe.boot()
	check(v.get("ok", false) and fe.screen() == "boot-card", "boot at the card check")
	check(fe.active() and fe.visible, "overlay covers the scene")
	check(not scene.menu.visible, "host menu hidden behind the front end")
	check(v.entries[0].id == "create", "blank card asks to create")
	tap(CROSS)
	check(FileAccess.file_exists(fe.options_path), "options image written")
	check(FileAccess.get_file_as_bytes(fe.options_path).size() == ClassDB.class_call_static("ChampionsFrontendService", "options_size"), "760-byte image")
	check(fe.screen() == "soe-logo", "logo")
	v = frames(0, 239)
	check(fe.screen() == "intro-movie", "film after the logo's 240 frames")
	tap(CROSS)
	check(fe.screen() == "intro-movie", "press inside 30 frames ignored")
	frames(0, 30)
	tap(CROSS)
	check(fe.screen() == "main" and int(fe.view.state) == 0, "main menu, state 0")
	check("front_end_music" in kinds() and "apply_music_volume" in kinds(), "front-end music and volume applied")
	check(fe.view.entries.size() == 4 and fe.view.entries[0].label == "New Game", "main menu entries")
	var notes: Array = fe.view.notes
	check(notes.any(func(n): return str(n).find("not in the research") >= 0), "layout gap named on screen")

	# Second boot with the file: logo press skips the film.
	v = fe.boot()
	check(v.entries[0].id == "continue" and v.options_file, "file read at boot")
	tap(CROSS)
	tap(CROSS)
	check(fe.screen() == "main" and fe.view.skip_splash, "logo press skips the film")

	# Options: slider, commit, load.
	tap(DOWN)
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "options" and int(fe.view.cursor) == 1, "options on music")
	var before := int(fe.view.music_live)
	fe.log_lines.clear()
	frames(RIGHT, 12)
	check(int(fe.view.music_live) > before, "held right raises music")
	check("preview_music" in kinds(), "slider previews the theme")
	frames(0, 1)
	var live := int(fe.view.music_live)
	tap(CIRCLE)
	check(fe.screen() == "main", "circle leaves options")
	var image := FileAccess.get_file_as_bytes(fe.options_path)
	check(image.decode_u32(8) == live, "committed music in the image file")
	check(scene.audio.music_volume_bits == live, "mixer volume is the flow's word")
	tap(CROSS)
	tap(TRIANGLE)
	check(fe.screen() == "options-load", "triangle: load options")
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "options" and int(fe.view.music_live) == live, "options loaded from the file")
	tap(CIRCLE)

	# Credits are refused.
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "credits", "credits")
	tap(CIRCLE)
	check(fe.screen() == "main", "credits back")

	# Load Saved Game screen and back.
	for i in range(3):
		tap(UP)
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "load-game", "load screen from the title")
	tap(CIRCLE)
	check(fe.screen() == "main", "load back")

	# New game route and character creation.
	tap(UP)
	var states := [int(fe.view.state)]
	for i in range(4):
		tap(CROSS)
		states.append(int(fe.view.state))
	check(states == [0, 1, 2, 4, 3], "front-end state order 0 1 2 4 3")
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "difficulty", "higher difficulty refused")
	tap(UP)
	tap(CROSS)
	check(fe.screen() == "char-class" and fe.view.entries.size() == 12, "class page with twelve models")
	tap(CROSS)
	check(fe.screen() == "char-appearance", "appearance page")
	tap(CROSS)
	check(fe.screen() == "char-ability", "ability page")
	tap(CROSS)
	check(fe.screen() == "char-ability" and str(fe.view.error) != "", "unspent points refused")
	for i in range(20):
		tap(RIGHT)
	check(int(fe.view.ability_remaining) == 0, "20 points spent")
	tap(CROSS)
	check(fe.screen() == "char-name", "name page")
	for c in "Smoke":
		scene.handle_input_event(key(KEY_A, true, c.unicode_at(0)))
		scene.handle_input_event(key(KEY_A, false))
	check(fe.view.character.name == "Smoke", "name typed on the host keyboard")
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "playing" and scene.session_started and not scene.paused, "character done: play starts")
	check(not fe.visible, "overlay hidden in play")
	check(fe.start_mode.begins_with("no game folder"), "no folder: built-in session, labelled")
	var ledger: Dictionary = scene.game_menus.service.state()
	check(int(ledger.gold) == 500 and int(ledger.level) == 1 and int(ledger.acknowledged) == 1, "in-game menus adopt the new character")

	# Pause menu: start opens it; gate scroll refused; options; exit prompt.
	tap(START)
	check(fe.screen() == "pause" and scene.paused and not scene.menu.visible, "start pauses with the game menu, host menu hidden")
	check(fe.view.entries.size() == 5, "five pause entries offline")
	tap(DOWN)
	tap(CROSS)
	check(fe.host_error.begins_with("Use Gate Scroll refused"), "gate scroll refused and labelled")
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "load-game", "load from pause")
	tap(CIRCLE)
	check(fe.screen() == "pause" and int(fe.view.cursor) == 2, "back on load")
	tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "options", "pause options")
	fe.log_lines.clear()
	frames(LEFT, 8)
	check(fe.log_lines.any(func(l): return str(l).begins_with("preview_music mx_main_l.vag")), "pause slider previews mx_main_l.vag")
	tap(CIRCLE)
	check(fe.screen() == "pause" and int(fe.view.cursor) == 3, "pause options back")
	tap(CIRCLE)
	check(fe.screen() == "playing" and not scene.paused, "circle closes the pause menu")

	# Save screen from play (host stands in for a savepoint), then load it back.
	var save_path := dir.path_join("frontend.ocplay")
	fe.queue_path("save_session", save_path)
	v = fe.open_save_screen()
	visited[fe.screen()] = true
	check(fe.screen() == "save-game" and scene.paused, "save screen pauses")
	tap(CROSS)
	check(FileAccess.file_exists(save_path), "session saved through the save screen")
	check(fe.screen() == "saved" and int(fe.view.save_latch) == 1, "saved box, latch 1")
	tap(CROSS)
	check(fe.screen() == "playing" and int(fe.view.save_latch) == 0 and not scene.paused, "box closed, latch 0")
	tap(START)
	tap(DOWN)
	tap(DOWN)
	fe.queue_path("load_session", save_path)
	tap(CROSS)
	tap(CROSS)
	check(fe.screen() == "playing" and scene.session_started and not scene.paused, "pause -> load restores the session")

	# Exit prompt: opens on no; left then cross leaves to the front end.
	tap(START)
	for i in range(4):
		tap(DOWN)
	tap(CROSS)
	check(fe.screen() == "exit-confirm" and int(fe.view.cursor) == 1, "exit prompt on no")
	tap(CROSS)
	check(fe.screen() == "pause", "no returns")
	tap(CROSS)
	tap(LEFT)
	tap(CROSS)
	check(fe.screen() == "main" and fe.visible, "yes: back at the title")

	# PS2 import from Load Saved Game and from the class page.
	if ClassDB.class_exists("ChampionsPs2SaveImport"):
		var slot: PackedByteArray = ClassDB.class_call_static("ChampionsPs2SaveImport", "synthetic_slot", "Aria", 777)
		var slot_path := dir.path_join("BASLUS-20973_slot0")
		var out := FileAccess.open(slot_path, FileAccess.WRITE)
		out.store_buffer(slot)
		out.close()
		tap(DOWN)
		tap(CROSS)
		tap(DOWN)
		fe.queue_path("import_ps2", slot_path)
		tap(CROSS)
		check(fe.screen() == "playing" and fe.view.character.imported, "slot import from the load screen plays")
		check(int(scene.game_menus.service.state().gold) == 777, "imported gold in the in-game menus")
		fe.boot()
		tap(CROSS)
		tap(CROSS)
		to_char_class()
		fe.queue_path("import_ps2", slot_path)
		tap(TRIANGLE)
		check(fe.screen() == "playing" and fe.view.character.name == "Aria", "class-page triangle imports")
		fe.queue_path("import_ps2", dir.path_join("missing"))
		fe.boot()
		tap(CROSS)
		tap(CROSS)
		to_char_class()
		tap(TRIANGLE)
		check(fe.screen() == "char-class" and str(fe.view.error) != "", "refused import stays on the page")

	# Host-started play joins at start.
	fe.booted = false
	scene.start_play()
	scene.handle_input_event(key(KEY_P))
	fe.step(fe.held_bits())
	scene.handle_input_event(key(KEY_P, false))
	fe.step(fe.held_bits())
	check(fe.screen() == "pause", "P pauses a host-started session")

	for name in ["boot-card", "soe-logo", "intro-movie", "main", "game-type", "players", "mode", "difficulty", "options", "options-load", "credits", "load-game", "save-game", "saved", "char-class", "char-appearance", "char-ability", "char-name", "playing", "pause", "exit-confirm"]:
		check(visited.has(name) or name in ["boot-card", "soe-logo", "intro-movie"], "visited " + name)
	_cleanup()
	print("FRONTEND_SMOKE checks=%d failures=%d screens=%d" % [checks, failures, visited.size()])
	quit(1 if failures else 0)

func _cleanup() -> void:
	var d := DirAccess.open(dir)
	if d != null:
		for file in d.get_files():
			d.remove(file)
	DirAccess.remove_absolute(dir)
