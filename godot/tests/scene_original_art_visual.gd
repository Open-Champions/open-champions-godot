extends SceneTree

# Run from the actual macOS export with a graphical renderer and --capture-dir.
# Every texture is synthetic test data; controls are driven by their public UI
# methods, not by a desktop click or retail input.
const PANEL = preload("res://ui/original_art/original_art_panel.tscn")
const FIXTURE = preload("res://tests/texture_fixture.gd")
const TEST = preload("res://tests/scene_original_art_smoke.gd")
var output := ""
var failures := 0
var checks := 0

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("Original art visual: " + label)

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="):
			output = arg.trim_prefix("--capture-dir=")
	call_deferred("run")

func write(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_buffer(bytes)
	file.close()

func capture(label: String) -> void:
	await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and not image.is_empty(), "rendered viewport for " + label)
	if image != null:
		check(image.save_png(output.path_join(label + ".png")) == OK, "saved capture " + label)

func choose(panel: Control, id: String) -> void:
	for i in range(panel.identity_choice.item_count):
		if panel.identity_choice.get_item_text(i) == id:
			panel.identity_choice.select(i)
			panel._refresh_rows()
			break
	check(panel.row_list.item_count == 1, "one explicit synthetic row for " + id)
	if panel.row_list.item_count == 1:
		panel.row_list.select(0)
		check(panel.load_selection(), "load " + id)

func run() -> void:
	if output.is_empty() or DisplayServer.get_name() == "headless":
		push_error("A graphical exported app and --capture-dir are required.")
		quit(1)
		return
	check(DirAccess.make_dir_recursive_absolute(output) == OK, "capture directory")
	check(ClassDB.class_exists("ChampionsArchiveInspector"), "local release extension loaded")
	if not ClassDB.class_exists("ChampionsArchiveInspector"):
		quit(1)
		return
	var panel = PANEL.instantiate()
	root.add_child(panel)
	panel.configure(ClassDB.instantiate("ChampionsArchiveInspector"))
	await capture("00-empty")
	panel.show_picker()
	await capture("01-picker-empty")
	panel.picker.hide()
	var hud := output.path_join("HUD.LMP")
	write(hud, FIXTURE.lmp(["bar_red10.tex", "bar_blue10.tex", "hud_statusbar00.tex"], [TEST.fixture(100, 8, 9), TEST.fixture(100, 8, 13), TEST.fixture(160, 46, 17)]))
	panel.open_source(hud)
	panel.show_picker()
	choose(panel, "bar_red")
	choose(panel, "bar_blue")
	choose(panel, "statusbar")
	await capture("02-picker-loaded")
	panel.picker.hide()
	var interface := output.path_join("INTERFAC.LMP")
	write(interface, FIXTURE.lmp(["button.tex", "tiletop.tex", "tilecenter.tex"], [TEST.fixture(83, 38, 3), TEST.fixture(32, 32, 5), TEST.fixture(320, 224, 7)]))
	panel.open_source(interface)
	choose(panel, "button")
	choose(panel, "frame_top")
	choose(panel, "frame_center")
	panel.update_observation({"hp":25, "max_hp":100, "mana":10, "max_mana":20})
	await capture("03-loaded-1x")
	panel.scale_choice.select(1)
	panel.scale_choice.item_selected.emit(1)
	check(panel.integer_scale == 2, "2x selector interaction")
	await capture("04-loaded-2x")
	panel.clear_art()
	await capture("05-unloaded")
	print("SCENE_ORIGINAL_ART_VISUAL_PASS checks=%d failures=%d renderer=%s viewport=%s" % [checks, failures, DisplayServer.get_name(), str(root.size)])
	quit(0 if failures == 0 else 1)
