extends SceneTree

# One real level, then each documented menu. Prints counts only.
var folder := ""
var capture_dir := ""
var cache := ""

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="):
			folder = argument.trim_prefix("--game-folder=")
		elif argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
		elif argument.begins_with("--cache="):
			cache = argument.trim_prefix("--cache=")
	call_deferred("run")

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
	root.size = Vector2i(1400, 1000)
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
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
	var loaded := -1
	var tried := 0
	for index in range(mini(8, scene.real.levels.size())):
		tried += 1
		var result: Dictionary = scene.real.load_level(index)
		if result.get("ok", false):
			loaded = index
			break
	print("fonts ", scene.real.fonts.size(), " font_loaded ", font_ok, " levels ", scene.real.levels.size(), " tried ", tried, " loaded_index ", loaded)
	if loaded < 0:
		printerr("no level loaded")
		quit(1)
		return
	scene.actions.play.pressed.emit()
	await process_frame
	if not scene.play_mode:
		printerr("play did not start")
		quit(1)
		return
	for _tick in range(20):
		scene.advance_step()
	scene.viewport.size = Vector2i(1280, 720)
	if scene.game_menus != null and scene.components.has("art"):
		scene.game_menus.sync_font(scene.components.art)
	var saved := 0
	for which in ["inventory", "shop", "shop-bag", "skills", "stats", "level-up", "save", "pause"]:
		scene.game_menus.open_screen(which)
		for _frame in range(4):
			await process_frame
		await RenderingServer.frame_post_draw
		var image: Image = scene.viewport.get_texture().get_image()
		var path := capture_dir.path_join(which + ".png")
		if image != null and image.save_png(path) == OK:
			saved += 1
		else:
			printerr("capture failed ", which)
	print("captures ", saved)
	quit(0 if saved == 8 else 1)
