extends SceneTree

# The front end on the user's game folder: the folder's first font row, the
# class start stats from the user's executable, the hub level after character
# creation, then the pause menu over that level. Writes PNGs only to the given
# capture directory (keep it under work/private/) and prints counts only.
const CROSS := 0x40
const CIRCLE := 0x20
const DOWN := 0x4000
const LEFT := 0x8000
const RIGHT := 0x2000
const START := 0x800
const TRIANGLE := 0x10
var folder := ""
var capture_dir := ""
var cache := ""
var scene
var fe
var saved := 0

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="):
			folder = argument.trim_prefix("--game-folder=")
		elif argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
		elif argument.begins_with("--cache="):
			cache = argument.trim_prefix("--cache=")
	call_deferred("run")

func shot(label: String) -> void:
	for _frame in range(3):
		await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image != null and image.save_png(capture_dir.path_join(label + ".png")) == OK:
		saved += 1
	else:
		printerr("capture failed ", label)

func run() -> void:
	if folder.is_empty() or capture_dir.is_empty() or cache.is_empty():
		printerr("game folder, capture dir and cache are required")
		quit(2)
		return
	if DisplayServer.get_name() == "headless":
		printerr("headless display cannot capture")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	root.size = Vector2i(1280, 960)
	root.content_scale_size = root.size
	scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	await process_frame
	scene.real.cache_dir = cache
	if scene.components.has("library"):
		scene.components.library.history_path = cache.path_join("level-library.json")
	if not scene.open_game_folder(folder):
		printerr("game folder refused")
		quit(1)
		return
	var font_ok: bool = scene.load_real_font(0)
	fe = scene.frontend
	fe.options_path = cache.path_join("frontend-options.img")
	if FileAccess.file_exists(fe.options_path):
		DirAccess.remove_absolute(fe.options_path)
	if scene.components.has("art"):
		fe.sync_font(scene.components.art)
	fe.boot()
	print("levels ", scene.real.levels.size(), " font_loaded ", font_ok, " game_font ", fe.font_texture != null, " start_stats_refused ", fe.log_lines.any(func(l): return str(l).begins_with("class start stats refused")))
	await shot("01-boot-card")
	fe.tap(CROSS)
	await shot("02-soe-logo")
	for i in range(240):
		fe.step(0)
	await shot("03-intro-movie")
	for i in range(31):
		fe.step(0)
	fe.tap(CROSS)
	await shot("04-main")
	fe.tap(DOWN)
	fe.tap(DOWN)
	fe.tap(CROSS)
	for i in range(30):
		fe.step(RIGHT)
	await shot("05-options")
	fe.tap(TRIANGLE)
	await shot("06-options-load")
	fe.tap(CIRCLE)
	fe.tap(CIRCLE)
	fe.tap(DOWN)
	fe.tap(CROSS)
	await shot("07-credits")
	fe.tap(CIRCLE)
	fe.tap(LEFT)
	for i in range(3):
		fe.tap(0x1000)
	fe.tap(DOWN)
	fe.tap(CROSS)
	await shot("08-load-game")
	fe.tap(CIRCLE)
	fe.tap(0x1000)
	fe.tap(CROSS)
	await shot("09-game-type")
	fe.tap(CROSS)
	await shot("10-players")
	fe.tap(CROSS)
	await shot("11-mode")
	fe.tap(CROSS)
	await shot("12-difficulty")
	fe.tap(CROSS)
	await shot("13-char-class")
	fe.tap(CROSS)
	await shot("14-char-appearance")
	fe.tap(CROSS)
	for i in range(20):
		fe.tap(RIGHT)
	await shot("15-char-ability")
	fe.tap(CROSS)
	fe.type_name("Capture")
	await shot("16-char-name")
	fe.tap(DOWN)
	fe.tap(CROSS)
	print("start ", fe.start_mode, " session ", scene.session_started)
	for _tick in range(30):
		scene.advance_step()
	await shot("17-playing")
	fe.tap(START)
	await shot("18-pause")
	for i in range(3):
		fe.tap(DOWN)
	fe.tap(CROSS)
	await shot("19-pause-options")
	fe.tap(CIRCLE)
	fe.tap(DOWN)
	fe.tap(CROSS)
	await shot("20-exit-confirm")
	fe.tap(CIRCLE)
	fe.tap(CIRCLE)
	fe.open_save_screen()
	await shot("21-save-game")
	print("captures ", saved)
	quit(0 if saved == 21 else 1)
