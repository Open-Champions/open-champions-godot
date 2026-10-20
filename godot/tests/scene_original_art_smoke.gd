extends SceneTree

const PANEL = preload("res://ui/original_art/original_art_panel.tscn")
const FIXTURE = preload("res://tests/texture_fixture.gd")
const DIRECTORY := "user://scene-original-art-smoke"
var checks := 0
var failures := 0

func check(ok: bool, description: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("Original art: " + description)

func write(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_buffer(bytes)
	file.close()

static func fixture(width: int, height: int, seed: int = 0) -> PackedByteArray:
	# Independently authored plain RTA PSMCT32 transfer; each row and both crop
	# edges differ. This is synthetic data, not a retail image.
	var pixel_count := width * height
	var image_qwords := ((pixel_count * 4) + 15) >> 4
	var data := PackedByteArray()
	data.resize(224 + image_qwords * 16)
	data.encode_u16(0, width)
	data.encode_u16(2, height)
	data.encode_u16(4, 1)
	data.encode_u16(6, 6 + image_qwords)
	data.encode_u16(8, 0x60)
	var exponent_w := ceili(log(float(width)) / log(2.0))
	var exponent_h := ceili(log(float(height)) / log(2.0))
	var tbw := maxi(1, (1 << exponent_w) >> 6)
	data.encode_u32(12, tbw | (exponent_w << 12) | (exponent_h << 16))
	data.encode_u32(16, 128)
	data.encode_u64(128, 4 | (1 << 60))
	data.encode_u64(136, 0x0e)
	data.encode_u64(144, tbw << 48)
	data.encode_u64(152, 0x50)
	data.encode_u64(168, 0x51)
	data.encode_u64(176, width | (height << 32))
	data.encode_u64(184, 0x52)
	data.encode_u64(200, 0x53)
	data.encode_u64(208, image_qwords | 0x8000 | (2 << 58))
	for y in range(height):
		for x in range(width):
			var index := (y * width + x) * 4 + 224
			data[index] = (seed + x * 3 + y * 17) & 255
			data[index + 1] = (x * 7 + y * 5) & 255
			data[index + 2] = (x ^ y) & 255
			data[index + 3] = [0, 32, 64, 128][(x + y) % 4]
	return data

static func expected_pixel(x: int, stored_y: int, seed: int) -> PackedByteArray:
	return PackedByteArray([(seed + x * 3 + stored_y * 17) & 255, (x * 7 + stored_y * 5) & 255, (x ^ stored_y) & 255, [0, 63, 127, 255][(x + stored_y) % 4]])

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	root.content_scale_size = Vector2i(960, 720)
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(DIRECTORY))
	var panel = PANEL.instantiate()
	root.add_child(panel)
	await process_frame
	check(ClassDB.class_exists("ChampionsArchiveInspector"), "real extension is loaded")
	if not ClassDB.class_exists("ChampionsArchiveInspector"):
		quit(1)
		return
	panel.configure(ClassDB.instantiate("ChampionsArchiveInspector"))
	var ready_events: Array[bool] = []
	panel.art_ready.connect(func(ready: bool): ready_events.append(ready))
	check("not loaded" in panel.state_label.text.to_lower(), "empty state has no substitute")
	var good := fixture(100, 8, 9)
	var duplicate := fixture(100, 8, 77)
	var bad := duplicate.slice(0, 400)
	var source := DIRECTORY + "/HUD.LMP"
	write(source, FIXTURE.lmp(["bar_red10.tex", "bar_red10.tex", "bar_red10.tex"], [good, duplicate, bad]))
	panel.open_source(source)
	panel.identity_choice.select(panel.identity_choice.get_item_index(1)) # bar_red in pinned table order
	panel._refresh_rows()
	check(panel.row_list.item_count == 3 and panel.row_list.get_selected_items().is_empty(), "duplicate names require explicit row")
	check(not panel.load_selection(), "no automatic first duplicate")
	panel.row_list.select(0)
	var first_loaded: bool = panel.load_selection()
	check(first_loaded, "selected first row decodes through native inspector")
	if not first_loaded:
		quit(1)
		return
	var image: Image = panel.loaded["bar_red"]["image"]
	check(image.get_size() == Vector2i(100, 8), "documented woven bar dimensions")
	var rgba := image.get_data()
	for out_y in range(8):
		var stored_y := (out_y >> 1) + (4 if out_y % 2 == 1 else 0)
		for x in [0, 1, 98, 99]:
			var offset: int = (out_y * 100 + x) * 4
			check(rgba.slice(offset, offset + 4) == expected_pixel(x, stored_y, 9), "exact row/crop-edge/alpha pixel %d,%d" % [x, out_y])
	check(panel.loaded.size() == 1 and ready_events == [true], "first load is one atomic publish")
	panel.row_list.select(2)
	check(not panel.load_selection() and panel.loaded["bar_red"]["image"] == image, "malformed duplicate cannot partially replace art")
	panel.row_list.select(1)
	check(panel.load_selection(), "explicit second duplicate can replace art")
	check(panel.loaded["bar_red"]["image"].get_data().slice(0, 4) == expected_pixel(0, 0, 77), "row identity determines displayed pixels")
	var before: Image = panel.loaded["bar_red"]["image"]
	write(source, FIXTURE.lmp(["bar_red10.tex"], [good]))
	check(not panel.load_selection() and panel.loaded["bar_red"]["image"] == before, "stale fingerprint refuses replacement")
	var wrong := DIRECTORY + "/INTERFAC.LMP"
	write(wrong, FIXTURE.lmp(["button.tex", "tiletop.tex"], [fixture(88, 38), fixture(32, 32)]))
	panel.open_source(wrong)
	panel.identity_choice.select(panel.identity_choice.get_item_index(2)) # button
	panel._refresh_rows()
	check(panel.row_list.item_count == 0, "wrong catalogue page dimensions refused")
	write(wrong, FIXTURE.lmp(["button.tex", "tiletop.tex"], [fixture(83, 38, 3), fixture(32, 32, 5)]))
	panel.open_source(wrong)
	check(panel.row_list.item_count == 1, "correct button page is selectable")
	panel.row_list.select(0)
	check(panel.load_selection(), "button shaped and cropped through native operation")
	var button_image: Image = panel.loaded["button"]["image"]
	check(button_image.get_size() == Vector2i(83, 38), "button crop is the used page")
	check(button_image.get_data().slice((37 * 83 + 82) * 4, (37 * 83 + 82) * 4 + 4) == expected_pixel(82, 37, 3), "button last crop edge is exact")
	var wrong_format := fixture(83, 38)
	wrong_format.encode_u32(12, wrong_format.decode_u32(12) | (0x21 << 6))
	write(wrong, FIXTURE.lmp(["button.tex"], [wrong_format]))
	panel.open_source(wrong)
	panel.row_list.select(0)
	check(not panel.load_selection() and panel.loaded["button"]["image"] == button_image, "unsupported format refuses replacement")
	write(wrong, FIXTURE.lmp(["button.tex", "tiletop.tex"], [fixture(83, 38, 3), fixture(32, 32, 5)]))
	panel.open_source(wrong)
	panel.identity_choice.select(panel.identity_choice.get_item_index(9)) # frame_top
	panel._refresh_rows()
	panel.row_list.select(0)
	check(panel.load_selection(), "whole frame tile as exact image")
	check(panel.loaded["frame_top"]["image"].get_size() == Vector2i(32, 32), "frame tile intact")
	var status_source := DIRECTORY + "/HUD.LMP"
	write(status_source, FIXTURE.lmp(["hud_statusbar00.tex"], [fixture(160, 46, 17)]))
	panel.open_source(status_source)
	panel.identity_choice.select(panel.identity_choice.get_item_index(12)) # statusbar
	panel._refresh_rows()
	panel.row_list.select(0)
	var status_loaded: bool = panel.load_selection()
	check(status_loaded, "whole status image through native shape")
	if not status_loaded:
		quit(1)
		return
	var status_image: Image = panel.loaded["statusbar"]["image"]
	check(status_image.get_size() == Vector2i(160, 46), "documented status crop after weaving")
	check(status_image.get_data().slice((45 * 160 + 159) * 4, (45 * 160 + 159) * 4 + 4) == expected_pixel(159, 45, 17), "status last crop-edge pixel and alpha")
	panel.set_integer_scale(2)
	await process_frame
	var widget = panel.find_child("Image_frame_top", true, false)
	check(widget != null and widget.custom_minimum_size == Vector2(64, 64) and widget.texture_filter == CanvasItem.TEXTURE_FILTER_NEAREST, "2x nearest integer scale")
	panel.set_integer_scale(1)
	await process_frame
	widget = panel.find_child("Image_frame_top", true, false)
	check(widget != null and widget.custom_minimum_size == Vector2(32, 32), "1x exact size")
	panel.update_observation({"hp":0, "max_hp":100})
	var red_widget = panel.widgets["bar_red"]
	check("0" in panel.health_label.text and panel.mana_label.text == "Mana: unsupported" and panel._ratio("hp", "max_hp") == 0.0, "health zero and no mana")
	panel.update_observation({"hp":100, "max_hp":100, "mana":0, "max_mana":20})
	check(panel._ratio("hp", "max_hp") == 1.0 and panel._ratio("mana", "max_mana") == 0.0, "full health and empty mana")
	panel.update_observation({"hp":25, "max_hp":100, "mana":10, "max_mana":20})
	check(panel._ratio("hp", "max_hp") == 0.25 and panel._ratio("mana", "max_mana") == 0.5, "intermediate diagnostic ratios")
	check(panel.widgets["bar_red"] == red_widget and red_widget.fill_ratio == 0.25, "observation updates existing standalone bar without rebuilding")
	panel.update_observation({"hp":"bad", "max_hp":100, "mana":20})
	check(panel.health_label.text == "Health: unsupported" and panel.mana_label.text == "Mana: unsupported", "unsupported observation refused")
	panel.clear_art()
	check(panel.loaded.is_empty() and panel.image_flow.get_child_count() == 0 and ready_events.back() == false, "unload clears images and signals")
	panel.open_source(wrong)
	panel.identity_choice.select(panel.identity_choice.get_item_index(2))
	panel._refresh_rows()
	panel.row_list.select(0)
	check(panel.load_selection(), "error recovery after unload")
	print("SCENE ORIGINAL ART: %d checks, %d failures" % [checks, failures])
	quit(0 if failures == 0 else 1)
