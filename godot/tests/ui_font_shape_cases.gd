extends RefCounted
const FIXTURE = preload("res://tests/texture_fixture.gd")
const DIRECTORY := "user://ui-font-shape-smoke"
var failures := 0
var checks := 0
func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error("Font/shape: " + label)
func write(path: String, bytes: PackedByteArray) -> void:
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_buffer(bytes)
	f.close()
static func font(page: PackedByteArray) -> PackedByteArray:
	var d := PackedByteArray()
	d.resize(0x214 + 3 * 16 + 18)
	d.encode_u16(0, 3)
	d.encode_u32(8, 0x214)
	d.encode_u32(12, 0x244)
	d.encode_u32(16, d.size())
	for i in range(3):
		var o := 0x214 + i * 16
		d.encode_u16(o, 65) # Deliberate duplicates, no character-map interpretation.
		d.encode_u16(o + 2, i)
		d.encode_u16(o + 4, 65535)
		d.encode_u16(o + 6, 1)
		d.encode_u16(o + 8, 2)
		d.encode_s16(o + 10, [-32768,32767,-1][i])
		d.encode_u16(o + 12, 65535)
		d.encode_u16(o + 14, 65535) # Ignored first-link word.
	d.encode_u16(0x244, 0)
	d.encode_u16(0x246, 2)
	d.encode_s16(0x248, -32768)
	d.encode_u16(0x24a, 65535)
	d.encode_u16(0x250, 1) # Ignored after terminator.
	d.append_array(page)
	return d
func run(gallery, visual := false) -> int:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(DIRECTORY))
	var texture_source := DIRECTORY + "/synthetic.tex"
	var source := DIRECTORY + "/synthetic.fnt"
	var bad_source := DIRECTORY + "/malformed.fnt"
	var archive := DIRECTORY + "/synthetic.gob"
	var unsupported := DIRECTORY + "/unsupported.fnt"
	var page := FIXTURE.rgba_texture()
	var flagged_page := page.duplicate()
	flagged_page.encode_u16(8,0x60)
	write(texture_source,flagged_page)
	gallery.open_source(texture_source)
	gallery.listing.item_selected.emit(0)
	var mark_count: int = gallery.marks.size()
	gallery.field_rows.value = 2
	gallery.shaped_mode.button_pressed = true
	check(gallery.save_button.disabled, "texture shaped mode disables stored-coordinate marking")
	gallery.save_selection()
	check(gallery.marks.size() == mark_count, "shaped preview cannot mutate measurement JSON")
	gallery.shaped_mode.button_pressed = false
	check(not gallery.save_button.disabled and gallery.decoded["height"] == 4, "stored texture measurement mode recovers unchanged")
	write(source, font(page))
	write(bad_source, PackedByteArray([1,2,3]))
	write(unsupported, font(PackedByteArray([0,1,2])))
	var lump := FIXTURE.lmp(["synthetic.fnt", "broken.fnt"], [font(page), PackedByteArray([0])])
	write(archive, FIXTURE.two_lmps(lump, lump))
	gallery.open_source(source)
	check(gallery.candidates.size() == 1 and gallery.candidates[0]["kind"] == "font", "explicit loose FNT discoverable without flag")
	var hash: String = gallery.candidates[0]["fingerprint"]
	check(not gallery.inspector.inspect_ui_font(source, 1, "rta", hash).get("ok", false), "loose invalid row refused")
	check(not gallery.inspector.inspect_ui_font(source, 0, "norrath", hash).get("ok", false), "font RTA boundary")
	gallery.listing.item_selected.emit(0)
	check(gallery.font_metadata.visible and "-32768" in gallery.font_metadata.text and "32767" in gallery.font_metadata.text, "raw signed metrics displayed")
	var parsed: Dictionary = gallery.inspector.inspect_ui_font(source, 0, "rta", hash)
	check(parsed["glyphs"].size() == 3 and parsed["glyphs"][0][0] == parsed["glyphs"][2][0] and parsed["kerns"] == [[0,2,-32768]], "ordered duplicates and terminator retained")
	check(gallery.original.texture != null and gallery.save_button.disabled and "3 glyphs" in gallery.info.text, "metadata and embedded stored page; no font measurements/layout")
	var rgba: PackedByteArray = gallery.decoded["rgba"]
	gallery.field_rows.value = 2
	gallery.shaped_mode.button_pressed = true
	var image: Image = gallery.original.texture.get_image()
	var expected := PackedByteArray()
	for row in [0,2,1,3]:
		expected.append_array(rgba.slice(row * 16, (row + 1) * 16))
	check(image.get_data() == expected, "explicit woven pixel bytes including alpha")
	gallery.field_rows.value = 4
	check(gallery.original.texture.get_height() == 8 and gallery.original.texture.get_image().get_data().slice(112,128) == rgba.slice(48,64), "min(rows+k,height-1) selection")
	for i in range(4):
		gallery.crop_fields[i].value = [1,1,2,2][i]
	gallery.crop_enabled.button_pressed = true
	check(gallery.original.texture.get_width() == 2 and gallery.original.texture.get_height() == 2, "explicit crop dimensions")
	gallery.crop_fields[0].value = 1024
	check(gallery.original.texture == null and "Shape refused" in gallery.status.text, "invalid crop clears preview")
	gallery.shaped_mode.button_pressed = false
	check(gallery.original.texture != null and gallery.original.texture.get_width() == 4, "stored toggle recovers after bad shape")
	gallery.crop_enabled.button_pressed = false
	gallery.field_rows.value = 0
	gallery.shaped_mode.button_pressed = true
	check(gallery.original.texture.get_image().get_data() == rgba, "no weave preserves all RGBA bytes")
	check(not gallery.inspector.shape_ui_art(0,4,rgba,0,[]).get("ok", false) and not gallery.inspector.shape_ui_art(4,4,rgba,-1,[]).get("ok", false), "binding safety domain")
	check(not gallery.inspector.shape_ui_art(4,4,rgba,0,[0.5,0,1,1]).get("ok", false), "fractional crop refused")
	gallery.open_source(unsupported)
	gallery.listing.item_selected.emit(0)
	check(gallery.font_metadata.visible and gallery.original.texture == null and "Decode refused" in gallery.status.text, "unsupported texture retains parsed metadata")
	gallery.open_source(bad_source)
	gallery.listing.item_selected.emit(0)
	check(not gallery.font_metadata.visible and gallery.original.texture == null and "Font refused" in gallery.status.text, "malformed font clears old metadata and image")
	gallery.open_source(archive)
	check(gallery.candidates.size() == 4 and gallery.candidates[0]["row_index"] != gallery.candidates[2]["row_index"], "nested GOB/LMP selection uses flattened rows, duplicate identities separate")
	gallery.listing.item_selected.emit(2)
	check(gallery.font_metadata.visible and gallery.original.texture != null, "nested FNT preview")
	write(archive, FIXTURE.two_lmps(FIXTURE.lmp(["changed.fnt"], [font(page)]), lump))
	gallery.listing.item_selected.emit(2)
	check(gallery.original.texture == null and not gallery.font_metadata.visible and "changed" in gallery.status.text, "stale font archive fingerprint refused")
	write(source, font(page))
	gallery.open_source(source)
	gallery.listing.item_selected.emit(0)
	check(gallery.original.texture != null, "valid font selection recovers after errors")
	if visual:
		# Larger authored pixel grid, no original artwork or replacement fonts.
		page.resize(224 + 64 * 32 * 4)
		page.encode_u16(0,64)
		page.encode_u16(2,32)
		page.encode_u16(6,518)
		page.encode_u32(12,1 | (6 << 12) | (5 << 16))
		page.encode_u64(176,64 | (32 << 32))
		page.encode_u64(208,512 | 0x8000 | (2 << 58))
		for y in range(32):
			for x in range(64):
				var o := 224 + (y * 64 + x) * 4
				page[o] = x * 4
				page[o+1] = y * 8
				page[o+2] = 220 if (x / 4 + y / 4) % 2 == 0 else 30
				page[o+3] = 128
		write(source,font(page))
		gallery.open_source(source)
		gallery.listing.item_selected.emit(0)
		gallery.scale_choice.value = 4
		gallery.popup_centered()
		await render_capture(gallery, "stored")
		gallery.field_rows.value = 15
		gallery.shaped_mode.button_pressed = true
		await render_capture(gallery, "woven")
		for i in range(4):
			gallery.crop_fields[i].value = [2,1,32,20][i]
		gallery.crop_enabled.button_pressed = true
		await render_capture(gallery, "crop")
		gallery.crop_fields[0].value = 1024
		await render_capture(gallery, "refusal")
		gallery.shaped_mode.button_pressed = false
		await render_capture(gallery, "recovered")
	else:
		for path in [texture_source,source,bad_source,archive,unsupported]:
			DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("UI_FONT_SHAPE_PASS" if failures == 0 else "UI_FONT_SHAPE_FAIL", " checks=", checks, " failures=", failures)
	return failures

func render_capture(gallery, label: String) -> void:
	var scroll: ScrollContainer = gallery.get_child(0)
	scroll.scroll_vertical = 0
	for frame in range(3):
		await gallery.get_tree().process_frame
	await RenderingServer.frame_post_draw
	var image: Image = gallery.get_texture().get_image()
	check(image != null and not image.is_empty(), "exported gallery viewport " + label)
	if image != null and not image.is_empty():
		check(image.save_png(DIRECTORY + "/" + label + ".png") == OK, "synthetic rendered capture " + label)

	scroll.ensure_control_visible(gallery.status)
	for frame in range(3):
		await gallery.get_tree().process_frame
	await RenderingServer.frame_post_draw
	image = gallery.get_texture().get_image()
	check(image.save_png(DIRECTORY + "/" + label + "-preview.png") == OK, "scrolled preview/status capture " + label)
	var viewport_rect := scroll.get_global_rect()
	var status_rect: Rect2 = gallery.status.get_global_rect()
	check(status_rect.position.y >= viewport_rect.position.y - 1 and status_rect.end.y <= viewport_rect.end.y + 1, "status text vertically reachable through gallery scroll " + label)
