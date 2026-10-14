extends RefCounted

const FIXTURE = preload("res://tests/texture_fixture.gd")
const DIRECTORY := "user://ui-art-gallery-smoke"
var failures := 0
var checks := 0

func check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error("UI art gallery: " + label)

func write(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_buffer(bytes)
	file.close()

func run(browser, visual := false) -> int:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(DIRECTORY))
	var gallery = browser.ui_gallery
	var gallery_button: Button
	var licenses_button: Button
	for button in browser.find_children("*", "Button", true, false):
		if button.text == "UI art gallery":
			gallery_button = button
		elif button.text == "Licenses":
			licenses_button = button
	check(gallery_button != null and licenses_button != null, "merged footer retains gallery and licenses controls")
	if gallery_button != null and licenses_button != null:
		check(gallery_button.get_parent() == licenses_button.get_parent() and gallery_button.get_parent() is HFlowContainer, "footer controls share a wrapping row")
		await browser.get_tree().process_frame
		var scroll: ScrollContainer = browser.find_children("*", "ScrollContainer", true, false)[0]
		scroll.ensure_control_visible(gallery_button)
		await browser.get_tree().process_frame
		check(scroll.get_global_rect().encloses(gallery_button.get_global_rect()) and scroll.get_global_rect().encloses(licenses_button.get_global_rect()), "both footer controls are reachable without clipping")
		gallery_button.pressed.emit()
		check(gallery.visible, "footer entry point opens gallery")
		gallery.hide()
	gallery.storage_path = DIRECTORY + "/measurements.json"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(gallery.storage_path))
	check(gallery.load_marks(), "fresh local measurements")
	var texture := FIXTURE.rgba_texture()
	texture.encode_u16(8, 0x60)
	var rejected := texture.duplicate()
	rejected.encode_u32(16, 0xfffffff0)
	var names: Array[String] = []
	var payloads: Array[PackedByteArray] = []
	for i in range(23):
		names.append("synthetic_%02d.tex" % i)
		payloads.append(texture if i != 22 else rejected)
	names.append("not_ui.tex")
	payloads.append(FIXTURE.rgba_texture())
	var lump := FIXTURE.lmp(names, payloads)
	var source := DIRECTORY + "/gallery.lmp"
	write(source, lump)
	var metadata: Dictionary = gallery.inspector.inspect_ui_candidates(source, "rta")
	check(metadata.get("ok", false) and metadata["rows"].size() == 23, "flag candidates include refused payload, exclude nonflag")
	check(not gallery.inspector.inspect_ui_candidates(source, "norrath").get("ok", false), "RTA profile boundary")
	gallery.open_source(source)
	check(gallery.candidates.size() == 23 and gallery.listing.item_count == 20, "first page is bounded")
	gallery.next.pressed.emit()
	check(gallery.page == 1 and gallery.listing.item_count == 3 and gallery.next.disabled, "next page")
	gallery.listing.item_selected.emit(2)
	check(gallery.original.texture == null and gallery.save_button.disabled and "refused" in gallery.status.text, "refused decode clears display and cannot mark")
	gallery.previous.pressed.emit()
	gallery.listing.item_selected.emit(0)
	check(gallery.selected["name"] == "synthetic_00.tex" and gallery.selected["format"] == "PSMCT32", "name and format metadata")
	check(gallery.original.custom_minimum_size == Vector2(4, 4) and gallery.enlarged.custom_minimum_size == Vector2(12, 12), "1x and exact integer-scaled sizes")
	check(gallery.original.texture_filter == CanvasItem.TEXTURE_FILTER_NEAREST and gallery.enlarged.texture_filter == CanvasItem.TEXTURE_FILTER_NEAREST, "nearest sampling")
	var displayed: PackedByteArray = gallery.original.texture.get_image().get_data()
	check(displayed.slice(0, 3) == texture.slice(224, 227) and displayed[3] == 255 and displayed.slice(16, 19) == texture.slice(240, 243), "exact RGB and stored row order; alpha inspection only")
	gallery.scale_choice.value = 5
	check(gallery.enlarged.custom_minimum_size == Vector2(20, 20), "scale control changes only integer enlargement")
	check(gallery.fields[2].value == 0 and gallery.fields[3].value == 0, "no invented default frame rectangle")
	gallery.frame_name.text = "synthetic frame"
	var values := [1, 1, 3, 3, 1, 1, 1, 1]
	for i in range(8):
		gallery.fields[i].value = values[i]
	gallery.save_button.pressed.emit()
	check(gallery.marks.size() == 1 and FileAccess.file_exists(gallery.storage_path), "mark persisted in user folder")
	if not FileAccess.file_exists(gallery.storage_path):
		return failures + 1
	var saved_text := FileAccess.get_file_as_string(gallery.storage_path)
	var document = JSON.parse_string(saved_text)
	check(document["coordinate_space"] == "original_stored_pixels" and document["frames"][0]["rect"].map(func(v): return int(v)) == [1, 1, 3, 3] and document["frames"][0]["borders"].map(func(v): return int(v)) == [1, 1, 1, 1], "rectangle and ordered nine-slice borders roundtrip")
	check(document["frames"][0]["entry"]["fingerprint"] == metadata["fingerprint"] and document["frames"][0]["entry"]["row_index"] == 0, "source fingerprint and explicit row provenance")
	gallery.fields[2].value = 4
	gallery.save_selection()
	check(FileAccess.get_file_as_string(gallery.storage_path) == saved_text, "out-of-bounds rectangle cannot overwrite evidence")
	gallery.fields[2].value = 3
	gallery.fields[4].value = 3
	gallery.save_selection()
	check(FileAccess.get_file_as_string(gallery.storage_path) == saved_text, "overlapping borders refused")
	gallery.fields[4].value = 1
	gallery.frame_name.text = "second region"
	gallery.save_selection()
	check(gallery.marks.size() == 2, "multiple named regions per texture")
	gallery.saved_labels.item_selected.emit(1)
	check(gallery.frame_name.text == "synthetic frame" and gallery.fields[0].value == 1, "saved-label picker restores original pixel measurements")
	gallery.frame_name.text = "synthetic frame"
	gallery.fields[0].value = 0
	gallery.restore_selection()
	check(gallery.fields[0].value == 1, "restore saved measurements")
	gallery.fields[0].value = 0
	gallery.save_selection()
	check(gallery.marks.size() == 2 and gallery.marks[0]["rect"][0] == 0, "same label updates rather than duplicates")
	check(gallery.load_marks() and gallery.marks.size() == 2, "reload disk evidence")
	gallery.marked_only.button_pressed = true
	check(gallery.filtered.size() == 1 and gallery.decoded.is_empty(), "marked filter and stale selection clearing")
	gallery.marked_only.button_pressed = false
	gallery.search.text = "synthetic_21"
	gallery.search.text_changed.emit(gallery.search.text)
	check(gallery.filtered.size() == 1 and gallery.page == 0, "entry-name filtering resets page")
	gallery.search.text = "4x4 PSMCT32"
	gallery.search.text_changed.emit(gallery.search.text)
	check(gallery.filtered.size() == 23, "dimension and format filtering")
	gallery.search.clear()
	gallery.search.text_changed.emit("")
	var gob_path := DIRECTORY + "/nested.gob"
	write(gob_path, FIXTURE.two_lmps(FIXTURE.lmp(["duplicate.tex"], [texture]), FIXTURE.lmp(["duplicate.tex"], [texture])))
	gallery.open_source(gob_path)
	check(gallery.candidates.size() == 2 and gallery.candidates[0]["row_index"] != gallery.candidates[1]["row_index"], "nested duplicate names preserve row identity")
	gallery.select_item(1)
	check(gallery.decoded.get("ok", false) and gallery.selected["parent"] == "second.LMP", "nested decode uses explicit parent and row")
	var loose_path := DIRECTORY + "/loose.tex"
	write(loose_path, texture)
	gallery.open_source(loose_path)
	check(gallery.candidates.size() == 1 and gallery.candidates[0]["row_index"] == 0, "loose texture candidates")
	gallery.select_item(0)
	check(gallery.decoded.get("ok", false), "loose texture runtime decoding")
	gallery.frame_name.text = "stale frame"
	for i in range(8):
		gallery.fields[i].value = values[i]
	var before_stale := FileAccess.get_file_as_string(gallery.storage_path)
	write(loose_path, rejected)
	gallery.save_selection()
	check(FileAccess.get_file_as_string(gallery.storage_path) == before_stale and gallery.original.texture == null and "stale" in gallery.status.text, "save revalidates fingerprint and preserves existing evidence")
	gallery.select_item(0)
	check(gallery.original.texture == null and "changed" in gallery.status.text, "changed input fingerprint refuses stale decode")
	write(loose_path, texture)
	gallery.open_folder(DIRECTORY)
	while gallery.scanning:
		await browser.get_tree().process_frame
	check(gallery.candidates.size() == 26, "folder discovers archives, nested entries and loose textures")
	gallery.open_source(DIRECTORY + "/absent.lmp")
	check(gallery.candidates.is_empty() and gallery.original.texture == null, "source error clears stale pixels")
	write(gallery.storage_path, "{broken".to_utf8_buffer())
	check(not gallery.load_marks() and gallery.marks.is_empty(), "corrupt JSON blocks writes")
	check(not gallery.write_marks([]) and FileAccess.get_file_as_string(gallery.storage_path) == "{broken", "corrupt evidence file preserved")
	var bad = document["frames"][0].duplicate(true)
	bad["rect"][0] = 0.5
	check(not gallery.valid_measurement(bad), "fractional measurements refused")
	bad["rect"][0] = -1
	check(not gallery.valid_measurement(bad), "negative measurements refused")
	gallery.storage_path = "res://forbidden.json"
	check(not gallery.load_marks() and not gallery.write_marks([]), "repository storage forbidden")
	gallery.storage_path = DIRECTORY + "/measurements.json"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(gallery.storage_path))
	gallery.load_marks()
	gallery.open_source(source)
	gallery.select_item(0)
	gallery.scale_choice.value = 8
	gallery.popup_centered()
	await browser.get_tree().process_frame
	check(gallery.visible and gallery.original.size == Vector2(4, 4) and gallery.enlarged.size == Vector2(32, 32), "real controls keep exact texture sizes")
	gallery.frame_name.text = "focused input"
	for i in range(8):
		gallery.fields[i].value = [0, 0, 2, 2, 1, 1, 1, 1][i]
	gallery.fields[0].get_line_edit().grab_focus()
	gallery.fields[0].get_line_edit().text = "1"
	gallery.save_button.pressed.emit()
	check(gallery.marks.size() == 1 and gallery.marks[0]["rect"] == [1, 0, 2, 2], "pending typed numeric input applies before saving")
	check(gallery.storage_label.size.y > 0 and gallery.storage_path in gallery.storage_label.text, "local file path remains visible")
	print("UI art gallery smoke: ", "PASS" if failures == 0 else "FAIL", " · ", checks, " checks")
	if visual:
		var artwork := FIXTURE.rgba_texture()
		artwork.resize(224 + 64 * 32 * 4)
		artwork.encode_u16(0, 64)
		artwork.encode_u16(2, 32)
		artwork.encode_u16(6, 518)
		artwork.encode_u16(8, 0x60)
		artwork.encode_u32(12, 1 | (6 << 12) | (5 << 16))
		artwork.encode_u64(176, 64 | (32 << 32))
		artwork.encode_u64(208, 512 | 0x8000 | (2 << 58))
		for y in range(32):
			for x in range(64):
				var offset := 224 + (y * 64 + x) * 4
				artwork[offset] = x * 4
				artwork[offset + 1] = y * 8
				artwork[offset + 2] = 220 if (x / 4 + y / 4) % 2 == 0 else 30
				artwork[offset + 3] = 128
		write(loose_path, artwork)
		gallery.open_source(loose_path)
		gallery.select_item(0)
		gallery.scale_choice.value = 4
		gallery.frame_name.text = "Synthetic sample only"
		for i in range(8):
			gallery.fields[i].value = [0, 0, 64, 32, 4, 4, 4, 4][i]
	else:
		gallery.hide()
		for path in [source, gob_path, loose_path, gallery.storage_path, gallery.storage_path + ".tmp"]:
			DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	var font_cases = load("res://tests/ui_font_shape_cases.gd").new()
	failures += await font_cases.run(gallery, visual)
	return failures
