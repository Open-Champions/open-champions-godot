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
	for _frame in range(10):
		await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image != null and label == "13a-char-class-model-11" and bright_pixels(image, Rect2i(780, 225, 360, 360), 4) < 50:
		printerr("selected disc body texture preview did not render")
		return
	if image != null and label in ["08-load-game", "08a-load-card-synthetic", "21-save-game", "22-save-card-synthetic", "23-save-empty-card"]:
		var frame_colour := image.get_pixel(500, 350)
		if maxf(frame_colour.r, maxf(frame_colour.g, frame_colour.b)) < 0.08 or bright_pixels(image, Rect2i(300, 730, 680, 140), 4) < 100:
			printerr("rendered card frame or host file action absent: ", label)
			return
		if label in ["08a-load-card-synthetic", "22-save-card-synthetic"] and bright_pixels(image, Rect2i(140, 290, 960, 175), 4) < 50:
			printerr("rendered card rows absent: ", label)
			return
		if label == "23-save-empty-card":
			for row in range(5):
				if bright_pixels(image, Rect2i(148, 286 + 34 * row, 38, 29), 1) < 10:
					printerr("rendered empty save row number absent: ", row + 1)
					return
	if image != null and image.save_png(capture_dir.path_join(label + ".png")) == OK:
		saved += 1
	else:
		printerr("capture failed ", label)

func bright_pixels(image: Image, area: Rect2i, step: int) -> int:
	var count := 0
	for y in range(area.position.y, area.end.y, step):
		for x in range(area.position.x, area.end.x, step):
			var colour := image.get_pixel(x, y)
			if maxf(colour.r, maxf(colour.g, colour.b)) > 0.4:
				count += 1
	return count

func bind_synthetic_card_rows() -> bool:
	# Authored metadata and headers: five indexed dates/clocks and one preview.
	var metadata := PackedByteArray()
	var headers := PackedByteArray()
	metadata.resize(8 * 16)
	headers.resize(8 * 0x70)
	var hours := [0x00, 0x11, 0x12, 0x13, 0x23]
	var minutes := [0x05, 0x10, 0x00, 0x25, 0x59]
	var expected := ["12:05am", "11:10am", "12:00pm", "1:25pm", "11:59pm"]
	for row in range(5):
		metadata[16 * (row + 2) + 3] = hours[row]
		metadata[16 * (row + 2) + 2] = minutes[row]
		metadata[16 * (row + 2) + 6] = 0x01
		metadata[16 * (row + 2) + 5] = 0x09
		metadata[16 * (row + 2) + 7] = 0x26
	headers[2 * 0x70 + 0x64] = 59
	headers[2 * 0x70 + 0x6a] = 1
	headers[2 * 0x70 + 0x6c] = 1
	var result: Dictionary = fe.service.set_card_cache(metadata, headers, 8, 2)
	if not result.get("ok", false):
		return false
	result = fe.service.set_card_preview(2, true)
	if not result.get("ok", false):
		return false
	fe._refresh()
	var clock_row := 0
	var number_row := 0
	var date_row := 0
	var preview_fields := 0
	for draw in fe.scene_view.get("draws", []):
		match str(draw.get("role", "")):
			"supplied card clock":
				if clock_row >= 5 or int(draw.x) != 487 or int(draw.y) != 128 + 17 * clock_row or str(draw.font) != "small" or str(draw.text) != expected[clock_row]:
					return false
				clock_row += 1
			"supplied card number":
				if number_row >= 5 or int(draw.x) != 77 or int(draw.y) != 128 + 17 * number_row or str(draw.text) != "%d." % (number_row + 3):
					return false
				number_row += 1
			"supplied card date":
				if date_row >= 5 or int(draw.x) != 365 or int(draw.y) != 128 + 17 * date_row or str(draw.text) != "1/9/2026":
					return false
				date_row += 1
			"preview player count (header field)", "preview difficulty index (header field)", "debug play seconds (minute display unverified)":
				preview_fields += 1
	return clock_row == 5 and number_row == 5 and date_row == 5 and preview_fields == 3

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
	print("levels ", scene.real.levels.size(), " font_loaded ", font_ok, " game_font ", fe.fonts_loaded(), " start_stats_refused ", fe.log_lines.any(func(l): return str(l).begins_with("class start stats refused")))
	if fe.log_lines.any(func(l): return str(l).begins_with("class start stats refused")):
		printerr("audited class start table refused from the selected disc")
		quit(1)
		return
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
	if not fe.fonts_loaded() or int(fe.view_painter.drawn.frame_piece) != 40 or int(fe.view_painter.drawn.text_fallback) != 0:
		printerr("load screen did not paint the documented tile frame and game font")
		quit(1)
		return
	if not bind_synthetic_card_rows():
		printerr("load screen refused authored indexed card rows")
		quit(1)
		return
	await shot("08a-load-card-synthetic")
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
	fe.tap(DOWN) # audited carousel position 1: model 11, class 6
	await shot("13a-char-class-model-11")
	if fe.view_painter.load_body_texture("im").is_empty():
		printerr("disc body texture preview did not decode")
		quit(1)
		return
	fe.tap(CROSS)
	await shot("14-char-appearance")
	if int(fe.view.character.model) != 11 or fe.view.character.base != [26, 37, 24, 28]:
		printerr("model 11 did not receive the audited class 6 starting row")
		quit(1)
		return
	fe.tap(CROSS)
	for i in range(20):
		fe.tap(RIGHT)
	await shot("15-char-ability")
	fe.tap(CROSS)
	fe.type_name("Capture")
	await shot("16-char-name")
	fe.tap(DOWN)
	fe.tap(CROSS)
	if fe.screen() != "playing" or fe.view.character.name != "Capture" or fe.view.character.stats != [46, 37, 24, 28] or bool(fe.view.character.appearance_known) or bool(fe.view.character.skill_points_known):
		printerr("class 6 created record does not match the audited known fields and Unknown labels")
		quit(1)
		return
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
	if not fe.fonts_loaded() or int(fe.view_painter.drawn.frame_piece) != 40 or int(fe.view_painter.drawn.text_fallback) != 0:
		printerr("save screen did not paint the documented tile frame and game font")
		quit(1)
		return
	if not bind_synthetic_card_rows():
		printerr("save screen refused authored indexed card rows")
		quit(1)
		return
	await shot("22-save-card-synthetic")
	var empty_result: Dictionary = fe.service.set_card_cache(PackedByteArray(), PackedByteArray(), 0, 0)
	if not empty_result.get("ok", false):
		printerr("empty save card cache refused")
		quit(1)
		return
	fe._refresh()
	var free_rows := 0
	var number_rows := 0
	for draw in fe.scene_view.get("draws", []):
		if str(draw.get("role", "")) == "supplied card number":
			if number_rows >= 5 or int(draw.x) != 77 or int(draw.y) != 128 + 17 * number_rows or str(draw.font) != "small" or str(draw.text) != "%d." % (number_rows + 1):
				printerr("empty save row number, anchor or font changed")
				quit(1)
				return
			number_rows += 1
		if str(draw.get("role", "")) == "save-only free row (localized wording unknown)":
			if int(draw.x) != 100 or int(draw.y) != 128 or str(draw.font) != "small":
				printerr("save free row anchor or font changed")
				quit(1)
				return
			free_rows += 1
	if free_rows != 1 or number_rows != 5:
		printerr("empty save did not expose five numbers and one free row")
		quit(1)
		return
	await shot("23-save-empty-card")
	print("captures ", saved)
	quit(0 if saved == 25 else 1)
