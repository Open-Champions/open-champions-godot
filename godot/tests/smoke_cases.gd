extends RefCounted

var failures := 0
var checks := 0
var texture_checks_complete := false
const DIRECTORY := "user://archive-browser-smoke"
var files: Array[String] = []

func check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error("Archive browser smoke: " + label)

func put32(data: PackedByteArray, offset: int, value: int) -> void:
	data.encode_u32(offset, value)

func lump() -> PackedByteArray:
	var data := PackedByteArray()
	data.resize(44)
	put32(data, 0, 2)
	put32(data, 4, 28)
	put32(data, 8, 41)
	put32(data, 12, 3)
	put32(data, 16, 34)
	put32(data, 20, 44)
	put32(data, 24, 0)
	for i in range(5):
		data[28 + i] = "alpha".to_ascii_buffer()[i]
		data[34 + i] = "empty".to_ascii_buffer()[i]
	data[41] = 0x11
	data[42] = 0x22
	data[43] = 0x33
	return data

func gob(child: PackedByteArray) -> PackedByteArray:
	var data := PackedByteArray()
	data.resize(80)
	var name := "example.LmP".to_ascii_buffer()
	for i in range(name.size()):
		data[i] = name[i]
	put32(data, 32, 80)
	put32(data, 36, child.size())
	data.append_array(child)
	return data

func write_file(name: String, data: PackedByteArray) -> String:
	var path := DIRECTORY.path_join(name)
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		check(false, "synthetic fixture can be written")
		return ""
	file.store_buffer(data)
	file.close()
	files.append(path)
	return path

func rejected(inspector: RefCounted, path: String, label: String) -> void:
	var result: Dictionary = inspector.inspect_archive(path)
	check(not result.get("ok", true) and result.get("rows", []).is_empty() and not str(result.get("error", "")).is_empty(), label)

func run(inspector: RefCounted, browser: Control) -> int:
	if inspector == null:
		check(false, "native extension loaded")
		return failures
	check(true, "native extension loaded")
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(DIRECTORY))
	var nested := write_file("nested.GOB", gob(lump()))
	var unicode_path := write_file("Chämpions 测试.LMP", lump())
	var empty := write_file("empty.LMP", PackedByteArray([0, 0, 0, 0]))
	var invalid := write_file("invalid.LMP", PackedByteArray([1, 0, 0]))
	var invalid_nested := write_file("invalid-nested.GOB", gob(PackedByteArray([1, 0, 0])))
	var result: Dictionary = inspector.inspect_archive(nested)
	check(result.get("ok", false) and result.get("rows", []).size() == 3, "nested archive has three rows")
	if result.get("rows", []).size() == 3:
		check(result["rows"][1]["name"] == "alpha" and result["rows"][1]["parent"] == "example.LmP" and result["rows"][1]["offset"] == 121, "nested name, parent and absolute offset")
	check(inspector.inspect_archive(unicode_path).get("ok", false), "Unicode file paths")
	check(inspector.inspect_archive(empty).get("ok", false), "empty archive")
	rejected(inspector, invalid, "truncated table gives no partial rows")
	rejected(inspector, invalid_nested, "nested failure gives no partial rows")
	rejected(inspector, DIRECTORY.path_join("missing.LMP"), "missing file")
	rejected(inspector, DIRECTORY.path_join("unsupported.txt"), "unsupported extension")
	var extreme := lump()
	put32(extreme, 8, 0xffffffff)
	rejected(inspector, write_file("offset.LMP", extreme), "extreme payload offset")
	var many := PackedByteArray([0, 0, 0, 0])
	put32(many, 0, 20001)
	rejected(inspector, write_file("count.LMP", many), "entry count limit before allocation")
	var long_name := PackedByteArray()
	long_name.resize(1042)
	long_name.fill(65)
	put32(long_name, 0, 1)
	put32(long_name, 4, 16)
	put32(long_name, 8, 1042)
	put32(long_name, 12, 0)
	long_name[1041] = 0
	rejected(inspector, write_file("name.LMP", long_name), "long name allocation limit")
	var sparse_path := DIRECTORY.path_join("large.LMP")
	var sparse := FileAccess.open(sparse_path, FileAccess.WRITE)
	if sparse != null:
		sparse.seek(64 * 1024 * 1024)
		sparse.store_8(0)
		sparse.close()
		files.append(sparse_path)
		rejected(inspector, sparse_path, "file size cap before reading")
	else:
		check(false, "size-cap fixture can be created")
	for i in range(10):
		inspector.inspect_archive(unicode_path)
	if result.get("rows", []).size() == 3:
		check(result["rows"][1]["name"] == "alpha", "result strings survive repeat loads and buffer destruction")
	check(browser.game_choice.item_count == 2, "both game choices available")
	browser.open_archive(nested)
	check(browser.rows.size() == 3 and browser.table.visible, "UI shows actual native metadata")
	browser.search.text = "alpha"
	browser.search.text_changed.emit(browser.search.text)
	check(browser.table.get_root().get_child_count() == 1, "filter narrows visible rows")
	browser.search.text = "nothing matches this"
	browser.search.text_changed.emit(browser.search.text)
	check(browser.empty_state.visible and not browser.table.visible, "no-match filter state")
	browser.open_archive(invalid)
	check(browser.rows.is_empty() and browser.empty_state.visible, "error clears previous results")
	browser.open_archive(empty)
	check(browser.empty_title.text == "This archive is empty", "valid empty archive state")
	browser.game_choice.select(1)
	browser._profile_changed(1)
	check("unverified" in browser.evidence.text and browser.rows.is_empty(), "Norrath status is explicit; profile change clears results")
	browser.open_archive(unicode_path)
	check(browser.rows.size() == 2, "same authored fixture readable under Norrath label, without compatibility claim")
	browser._clear()
	check(browser.rows.is_empty() and browser.current_path.is_empty() and browser.search.text.is_empty(), "clear state")
	texture_cases(inspector, browser)
	check(texture_checks_complete, "texture smoke cases reached their final check")
	for path in files:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(DIRECTORY))
	print("Archive browser checks: ", checks, "; failures: ", failures)
	return failures

func texture_rejected(inspector: RefCounted, path: String, index: int, profile: String, digest: String, label: String) -> void:
	var result: Dictionary = inspector.inspect_texture(path, index, profile, digest)
	check(not result.get("ok", true) and result.get("rgba", PackedByteArray()).is_empty() and not str(result.get("error", "")).is_empty(), label)

func select_row(browser: Control, index: int) -> void:
	var item: TreeItem = browser.table.get_root().get_first_child()
	while item != null:
		if item.get_metadata(0) == index:
			item.select(0)
			browser._selected_entry_changed()
			return
		item = item.get_next()
	check(false, "requested original row is visible")

func texture_cases(inspector: RefCounted, browser: Control) -> void:
	var fixture = load("res://tests/texture_fixture.gd")
	var texture: PackedByteArray = fixture.rgba_texture()
	var palette := texture.duplicate()
	palette.encode_u16(8, 4)
	var other := texture.duplicate()
	other[224] = 222
	var names: Array[String] = ["note", "same.tex", "same.tex", "palette.etex"]
	var payloads: Array[PackedByteArray] = [PackedByteArray([1]), texture, other, palette]
	var child: PackedByteArray = fixture.lmp(names, payloads)
	var path := write_file("textures.GOB", gob(child))
	var listing: Dictionary = inspector.inspect_archive(path)
	var digest: String = listing.get("fingerprint", "")
	var result: Dictionary = inspector.inspect_texture(path, 2, "rta", digest)
	check(result.get("ok", false) and result.get("width", 0) == 4 and result.get("height", 0) == 4, "nested texture dimensions")
	var pixels: PackedByteArray = result.get("rgba", PackedByteArray())
	check(pixels.size() == 64 and pixels[0] == 1 and pixels[16] == 53 and pixels[60] == 196, "asymmetric texture stored row order")
	check(pixels.size() == 64 and pixels[3] == 0 and pixels[7] == 127 and pixels[11] == 255 and pixels[15] == 255, "stored alpha scales and clamps")
	var duplicate: Dictionary = inspector.inspect_texture(path, 3, "rta", digest)
	check(duplicate.get("ok", false) and duplicate["rgba"][0] == 222, "duplicate texture names resolve by original row index")
	var direct := write_file("texture-direct.LMP", child)
	var direct_listing: Dictionary = inspector.inspect_archive(direct)
	check(inspector.inspect_texture(direct, 1, "rta", direct_listing.get("fingerprint", "")).get("ok", false), "direct LMP texture")
	texture_rejected(inspector, path, 4, "rta", digest, "unsupported palette returns no pixels")
	texture_rejected(inspector, path, 2, "norrath", digest, "Norrath texture contract remains unverified")
	texture_rejected(inspector, path, -1, "rta", digest, "negative texture index rejected")
	texture_rejected(inspector, path, 900, "rta", digest, "absent texture index rejected")
	texture_rejected(inspector, path, 1, "rta", digest, "actual non-texture name rejected")
	texture_rejected(inspector, path, 2, "rta", "", "missing archive fingerprint rejected")
	var malformed: PackedByteArray = fixture.two_lmps(child, PackedByteArray([1, 0, 0]))
	var broken := write_file("later-malformed.GOB", malformed)
	# A caller-supplied matching digest must not bypass later-container validation.
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(malformed)
	texture_rejected(inspector, broken, 2, "rta", hash.finish().hex_encode(), "malformed later nested container rejects earlier texture")
	browser.game_choice.select(0)
	browser._profile_changed(0)
	browser.open_archive(path)
	browser.search.text = "same.tex"
	browser.search.text_changed.emit(browser.search.text)
	select_row(browser, 3)
	browser._preview_selected()
	check(browser.preview_rgba.size() == 64 and browser.preview_rgba[0] == 222 and browser.preview_row_index == 3, "filtered UI uses original row index")
	check(browser.preview_image.texture != null and not browser.preview_alpha.button_pressed, "preview defaults to RGB")
	# Headless Dummy renderer cannot read back textures; verify the source pixels
	# through the same pixel conversion helper used by actual rendering instead.
	check(browser._preview_pixels()[3] == 255, "RGB preview ignores alpha")
	browser.preview_alpha.button_pressed = true
	var alpha_pixels: PackedByteArray = browser._preview_pixels()
	check(alpha_pixels[0] == 0 and alpha_pixels[1] == 0 and alpha_pixels[2] == 0 and alpha_pixels[3] == 255 and alpha_pixels[4] == 127 and alpha_pixels[5] == 127 and alpha_pixels[6] == 127 and alpha_pixels[7] == 255, "alpha channel is scaled opaque grayscale")
	browser.search.text = "palette"
	browser.search.text_changed.emit(browser.search.text)
	check(browser.preview_image.texture == null and browser.preview_rgba.is_empty(), "filter change clears previous preview")
	select_row(browser, 4)
	browser._preview_selected()
	check(browser.preview_image.texture == null and browser.preview_rgba.is_empty() and "32-bit" in browser.preview_info.text, "unsupported UI texture clears image and explains limitation")
	browser.search.text = "same"
	browser.search.text_changed.emit(browser.search.text)
	select_row(browser, 2)
	browser._preview_selected()
	select_row(browser, 3)
	check(browser.preview_image.texture == null, "selection change clears preview")
	browser._preview_selected()
	# Modify a valid archive at the same path after listing; old identity is stale.
	var changed := gob(child)
	changed[changed.size() - 1] ^= 1
	write_file("textures.GOB", changed)
	texture_rejected(inspector, path, 2, "rta", digest, "changed archive fingerprint rejected")
	browser._preview_selected()
	check(browser.preview_image.texture == null and browser.preview_rgba.is_empty() and "changed" in browser.preview_info.text, "stale file failure clears previously displayed texture")
	browser.open_archive(direct)
	select_row(browser, 1)
	browser._preview_selected()
	browser.game_choice.select(1)
	browser._profile_changed(1)
	check(browser.preview_image.texture == null and browser.preview_rgba.is_empty(), "profile change clears texture")
	browser.game_choice.select(0)
	browser._profile_changed(0)
	browser.open_archive(direct)
	select_row(browser, 1)
	browser._preview_selected()
	browser.open_archive(write_file("new-empty.LMP", PackedByteArray([0, 0, 0, 0])))
	check(browser.preview_image.texture == null and browser.preview_rgba.is_empty(), "new file clears texture")
	browser._clear()
	texture_checks_complete = true
