extends RefCounted

var failures := 0
var checks := 0
var texture_checks_complete := false
var plain_four_checks_complete := false
var eight_bit_checks_complete := false
var model_checks_complete := false
var textured_model_checks_complete := false
var variant_checks_complete := false
var animation_checks_complete := false
var skinning_checks_complete := false
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
	model_cases(inspector, browser)
	check(model_checks_complete, "model smoke cases reached their final check")
	textured_model_cases(inspector, browser)
	check(textured_model_checks_complete, "textured model cases reached their final check")
	variant_cases(inspector, browser)
	check(variant_checks_complete, "variant cases reached their final check")
	animation_cases(inspector, browser)
	check(animation_checks_complete, "animation cases reached their final check")
	skinning_cases(inspector)
	check(skinning_checks_complete, "skinning bridge cases reached their final check")
	load("res://tests/skinning_viewer_cases.gd").new().run(self, inspector, browser)
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
	var player_texture: Dictionary = inspector.read_player_resource(path, 3, digest, "same.tex")
	check(player_texture.get("ok", false) and player_texture.get("bytes", PackedByteArray()) == other,
		"player texture copy retains the validated duplicate row")
	var direct := write_file("texture-direct.LMP", child)
	var direct_listing: Dictionary = inspector.inspect_archive(direct)
	check(inspector.inspect_texture(direct, 1, "rta", direct_listing.get("fingerprint", "")).get("ok", false), "direct LMP texture")
	texture_rejected(inspector, path, 4, "rta", digest, "mismatched 8-bit flags and 32-bit packet return no pixels")
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
	check(browser.preview_image.texture == null and browser.preview_rgba.is_empty() and not browser.preview_info.text.is_empty(), "unsupported UI texture clears image and explains limitation")
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
	check(not inspector.read_player_resource(path, 3, digest, "same.tex").get("ok", false),
		"changed texture archive refuses a saved player row")
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
	plain_four_cases(inspector, browser, fixture)
	check(plain_four_checks_complete, "plain 4-bit cases reached their final check")
	eight_bit_cases(inspector, browser, fixture)
	check(eight_bit_checks_complete, "8-bit cases reached their final check")
	texture_checks_complete = true

func plain_four_cases(inspector: RefCounted, browser: Control, fixture: Script) -> void:
	var texture: PackedByteArray = fixture.plain_four_texture()
	var names: Array[String] = ["plain-four.tex"]
	var payloads: Array[PackedByteArray] = [texture]
	var child: PackedByteArray = fixture.lmp(names, payloads)
	var path := write_file("plain-four.GOB", gob(child))
	var listing: Dictionary = inspector.inspect_archive(path)
	var digest: String = listing.get("fingerprint", "")
	var decoded: Dictionary = inspector.inspect_texture(path, 1, "rta", digest)
	check(decoded.get("ok", false) and decoded.get("width", 0) == 4 and decoded.get("height", 0) == 4, "plain 4-bit nested texture dimensions")
	var pixels: PackedByteArray = decoded.get("rgba", PackedByteArray())
	var expected := PackedByteArray()
	for i in range(16):
		expected.append_array(PackedByteArray([i * 13 + 1, 255 - i * 11, i * 7, [0, 127, 255, 255][i % 4]]))
	check(pixels == expected, "plain 4-bit palette lookup, low nibble first, stored rows and scaled alpha")
	var direct := write_file("plain-four.LMP", child)
	var direct_listing: Dictionary = inspector.inspect_archive(direct)
	check(inspector.inspect_texture(direct, 0, "rta", direct_listing.get("fingerprint", "")).get("rgba", PackedByteArray()) == expected, "plain 4-bit direct LMP matches nested texture")
	texture_rejected(inspector, path, 1, "norrath", digest, "plain 4-bit Norrath profile rejected")
	browser.open_archive(path)
	select_row(browser, 1)
	browser._preview_selected()
	check(browser.preview_rgba == expected and browser.preview_image.texture != null and not browser.preview_alpha.button_pressed, "plain 4-bit UI previews RGB by default")
	browser.preview_alpha.button_pressed = true
	var alpha: PackedByteArray = browser._preview_pixels()
	check(alpha.size() == 64 and alpha[4] == 127 and alpha[5] == 127 and alpha[6] == 127 and alpha[7] == 255, "plain 4-bit alpha channel view is opaque grayscale")
	# A valid replacement with a changed palette must not reuse a stale selection.
	var replacement := texture.duplicate()
	replacement[224] ^= 1
	var replacements: Array[PackedByteArray] = [replacement]
	write_file("plain-four.GOB", gob(fixture.lmp(names, replacements)))
	browser._preview_selected()
	check(browser.preview_rgba.is_empty() and browser.preview_image.texture == null and "changed" in browser.preview_info.text, "changed 4-bit palette clears stale preview")
	browser.open_archive(path)
	select_row(browser, 1)
	browser._preview_selected()
	check(browser.preview_rgba.size() == 64 and browser.preview_rgba[0] == (expected[0] ^ 1), "reopened 4-bit archive resolves replacement palette")
	browser._clear()
	plain_four_checks_complete = true

func eight_bit_cases(inspector: RefCounted, browser: Control, fixture: Script) -> void:
	# Two uniform uploads isolate CLUT permutation from the GS address mapper.
	# Index8 selects stored palette16; index16 selects stored palette8.
	var index_eight: PackedByteArray = fixture.eight_bit_texture(8)
	var index_sixteen: PackedByteArray = fixture.eight_bit_texture(16)
	var anchored := index_eight.duplicate()
	anchored[1344] = 0
	var names: Array[String] = ["index-eight.tex", "index-sixteen.etex", "origin.tex"]
	var payloads: Array[PackedByteArray] = [index_eight, index_sixteen, anchored]
	var child: PackedByteArray = fixture.lmp(names, payloads)
	var path := write_file("eight-bit.GOB", gob(child))
	var listing: Dictionary = inspector.inspect_archive(path)
	var digest: String = listing.get("fingerprint", "")
	var eight: Dictionary = inspector.inspect_texture(path, 1, "rta", digest)
	var sixteen: Dictionary = inspector.inspect_texture(path, 2, "rta", digest)
	var expected_eight := PackedByteArray()
	var expected_sixteen := PackedByteArray()
	for _pixel in range(256):
		expected_eight.append_array(PackedByteArray([16, 239, 48, 255]))
		expected_sixteen.append_array(PackedByteArray([8, 247, 24, 127]))
	check(eight.get("ok", false) and eight.get("width", 0) == 16 and eight.get("height", 0) == 16, "8-bit nested texture dimensions")
	check(eight.get("rgba", PackedByteArray()) == expected_eight, "8-bit index8 selects stored palette16 for every pixel")
	check(sixteen.get("rgba", PackedByteArray()) == expected_sixteen, "8-bit index16 selects stored palette8 and scales alpha")
	var expected_origin := expected_eight.duplicate()
	expected_origin[0] = 0
	expected_origin[1] = 255
	expected_origin[2] = 0
	check(inspector.inspect_texture(path, 3, "rta", digest).get("rgba", PackedByteArray()) == expected_origin, "8-bit authored first upload byte maps to top-left sample")
	var direct := write_file("eight-bit.LMP", child)
	var direct_listing: Dictionary = inspector.inspect_archive(direct)
	check(inspector.inspect_texture(direct, 1, "rta", direct_listing.get("fingerprint", "")).get("rgba", PackedByteArray()) == expected_sixteen, "8-bit direct LMP matches nested texture")
	texture_rejected(inspector, path, 1, "norrath", digest, "8-bit Norrath profile remains rejected")
	browser.open_archive(path)
	select_row(browser, 2)
	browser._preview_selected()
	check(browser.preview_rgba == expected_sixteen and browser.preview_image.texture != null and not browser.preview_alpha.button_pressed, "8-bit UI defaults to RGB")
	var rgb: PackedByteArray = browser._preview_pixels()
	check(rgb.size() == 1024 and rgb[0] == 8 and rgb[3] == 255, "8-bit RGB view ignores stored alpha")
	browser.preview_alpha.button_pressed = true
	var alpha: PackedByteArray = browser._preview_pixels()
	check(alpha.size() == 1024 and alpha[0] == 127 and alpha[1] == 127 and alpha[2] == 127 and alpha[3] == 255, "8-bit alpha channel is opaque grayscale")
	# The palette is in the same authored first upload as the other fixtures.
	var replacement := index_sixteen.duplicate()
	replacement[224 + 8 * 4] = 77
	var replacements: Array[PackedByteArray] = [index_eight, replacement, anchored]
	write_file("eight-bit.GOB", gob(fixture.lmp(names, replacements)))
	browser._preview_selected()
	check(browser.preview_rgba.is_empty() and browser.preview_image.texture == null and "changed" in browser.preview_info.text, "changed 8-bit palette clears stale preview")
	browser.open_archive(path)
	select_row(browser, 2)
	browser._preview_selected()
	check(browser.preview_rgba.size() == 1024 and browser.preview_rgba[0] == 77, "reopened 8-bit archive resolves replaced palette")
	var invalid := index_eight.duplicate()
	invalid.encode_u16(6, 0xffff)
	var invalid_names: Array[String] = ["invalid-eight.tex"]
	var invalid_payloads: Array[PackedByteArray] = [invalid]
	var invalid_path := write_file("invalid-eight.LMP", fixture.lmp(invalid_names, invalid_payloads))
	var invalid_listing: Dictionary = inspector.inspect_archive(invalid_path)
	texture_rejected(inspector, invalid_path, 0, "rta", invalid_listing.get("fingerprint", ""), "truncated 8-bit packet returns no pixels")
	browser.open_archive(invalid_path)
	select_row(browser, 0)
	browser._preview_selected()
	check(browser.preview_rgba.is_empty() and browser.preview_image.texture == null and not browser.preview_info.text.is_empty(), "8-bit error clears previous UI texture")
	var four_as_32: PackedByteArray = fixture.plain_four_texture()
	four_as_32.encode_u64(304, 1 << 48)
	var world := index_eight.duplicate()
	world.encode_u16(8, 0x0104)
	var rejected_names: Array[String] = ["four-as32.tex", "world.tex"]
	var rejected_payloads: Array[PackedByteArray] = [four_as_32, world]
	var rejected_path := write_file("excluded-layouts.LMP", fixture.lmp(rejected_names, rejected_payloads))
	var rejected_listing: Dictionary = inspector.inspect_archive(rejected_path)
	texture_rejected(inspector, rejected_path, 0, "rta", rejected_listing.get("fingerprint", ""), "4-bit-as-32-bit remains unsupported")
	texture_rejected(inspector, rejected_path, 1, "rta", rejected_listing.get("fingerprint", ""), "8-bit world storage remains unsupported")
	browser._clear()
	eight_bit_checks_complete = true

func model_rejected(inspector: RefCounted, path: String, index: int, profile: String, digest: String, label: String) -> void:
	var result: Dictionary = inspector.inspect_model(path, index, profile, digest)
	check(not result.get("ok", true) and result.get("meshes", []).is_empty() and not str(result.get("error", "")).is_empty(), label)

func model_cases(inspector: RefCounted, browser: Control) -> void:
	var fixture = load("res://tests/model_fixture.gd")
	var archive_fixture = load("res://tests/texture_fixture.gd")
	var data: PackedByteArray = fixture.model()
	var names: Array[String] = ["note", "triangle.vif", "unsupported.vif"]
	var payloads: Array[PackedByteArray] = [PackedByteArray([0]), data, fixture.model(0x44)]
	var child: PackedByteArray = archive_fixture.lmp(names, payloads)
	var path := write_file("models.GOB", gob(child))
	var listing: Dictionary = inspector.inspect_archive(path)
	var digest: String = listing.get("fingerprint", "")
	var model: Dictionary = inspector.inspect_model(path, 2, "rta", digest)
	check(model.get("ok", false) and model.get("meshes", []).size() == 2 and model.get("triangle_count", 0) == 2, "native model returns two authored triangles")
	if model.get("meshes", []).size() == 2:
		var mesh: Dictionary = model["meshes"][0]
		check(mesh["positions"] == PackedVector3Array([Vector3.ZERO, Vector3(2, 0, 0), Vector3(0, 1, 2)]), "model positions retain rest-pose units")
		check(mesh["uv_texels"] == PackedVector2Array([Vector2.ZERO, Vector2(4, 0), Vector2(0, 2)]), "model UVs retain per-corner texel coordinates")
		check(mesh["normals"][0].is_equal_approx(Vector3(0, -114.0 / 127, 57.0 / 127)), "model source normals preserved")
		check(mesh["visible_by_default"] and not model["meshes"][1]["visible_by_default"] and model["part_mask"] == "1", "hidden mask controls visibility while part mask does not")
	var direct := write_file("models.LMP", child)
	var direct_listing: Dictionary = inspector.inspect_archive(direct)
	check(inspector.inspect_model(direct, 1, "rta", direct_listing.get("fingerprint", "")).get("triangle_count", 0) == 2, "direct model archive matches nested result")
	model_rejected(inspector, path, 2, "norrath", digest, "Norrath model remains unverified")
	model_rejected(inspector, path, -1, "rta", digest, "negative model row rejected")
	model_rejected(inspector, path, 1, "rta", digest, "non-model entry rejected")
	model_rejected(inspector, path, 3, "rta", digest, "unsupported model program returns no partial meshes")
	var malformed: PackedByteArray = archive_fixture.two_lmps(child, PackedByteArray([1, 0, 0]))
	var bad_path := write_file("model-later-malformed.GOB", malformed)
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(malformed)
	model_rejected(inspector, bad_path, 2, "rta", hash.finish().hex_encode(), "model preview validates later nested containers")
	browser.open_archive(path)
	browser.search.text = "triangle"
	browser.search.text_changed.emit(browser.search.text)
	select_row(browser, 2)
	check(not browser.model_button.disabled and browser.preview_button.disabled, "model selection enables only model preview")
	browser._preview_model(false)
	var viewer = browser.model_viewer
	check(viewer.mesh_count == 1 and viewer.triangle_count == 1, "model viewer defaults to visible mesh subset")
	check(viewer.model_bounds == AABB(Vector3(0, 0, -1), Vector3(2, 2, 1)), "viewer frames actual visible geometry with Z-up to Y-up transform")
	if viewer.model_root.get_child_count() == 1:
		var instance: MeshInstance3D = viewer.model_root.get_child(0)
		check(instance.mesh.get_surface_count() == 1 and instance.mesh.surface_get_array_len(0) == 3 and instance.material_override.cull_mode == BaseMaterial3D.CULL_DISABLED, "viewer builds one two-sided triangle surface")
		var arrays: Array = instance.mesh.surface_get_arrays(0)
		var expected_normal := Vector3(0, 57, 114).normalized()
		# ArrayMesh packs normals; the measured Godot round-trip error is 2.25e-5.
		# This bound tolerates quantization while still detecting axis/sign errors.
		check(arrays[Mesh.ARRAY_NORMAL][0].distance_to(expected_normal) < 0.0001, "viewer rotates normals with positions")
	var initial_camera: Vector3 = viewer.camera.position
	var initial_distance: float = viewer.distance
	viewer.orbit(0.3, 0.2)
	check(not viewer.camera.position.is_equal_approx(initial_camera), "model orbit changes camera")
	viewer.zoom(0.8)
	check(viewer.distance < initial_distance, "model zoom changes distance")
	viewer.frame_model()
	check(viewer.camera.position.is_equal_approx(initial_camera), "frame resets orbit and framing")
	var press := InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = Vector2(30, 40)
	viewer.input_surface.gui_input.emit(press)
	var motion := InputEventMouseMotion.new()
	motion.position = Vector2(42, 45)
	motion.relative = Vector2.ZERO
	motion.button_mask = 0
	viewer.input_surface.gui_input.emit(motion)
	check(not viewer.camera.position.is_equal_approx(initial_camera), "viewport drag uses absolute positions when synthesized relative and button mask are zero")
	var after_first_motion: Vector3 = viewer.camera.position
	viewer.input_surface.gui_input.emit(motion)
	check(viewer.camera.position.is_equal_approx(after_first_motion), "unchanged pointer position does not repeat orbit delta")
	motion.position = Vector2(47, 48)
	viewer.input_surface.gui_input.emit(motion)
	check(not viewer.camera.position.is_equal_approx(after_first_motion), "subsequent pointer position continues orbit from previous position")
	var after_drag: Vector3 = viewer.camera.position
	var release := InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	viewer.window_input.emit(release)
	viewer.input_surface.gui_input.emit(motion)
	check(viewer.camera.position.is_equal_approx(after_drag) and not viewer.dragging, "release outside viewport stops orbit capture")
	viewer.input_surface.gui_input.emit(press)
	viewer.focus_exited.emit()
	viewer.input_surface.gui_input.emit(motion)
	check(viewer.camera.position.is_equal_approx(after_drag), "window focus loss stops orbit capture")
	var wheel := InputEventMouseButton.new()
	wheel.button_index = MOUSE_BUTTON_WHEEL_UP
	wheel.pressed = true
	var before_wheel: float = viewer.distance
	viewer.input_surface.gui_input.emit(wheel)
	check(viewer.distance < before_wheel, "viewport wheel route still zooms")
	viewer.frame_model()
	browser.search.text = "unsupported"
	browser.search.text_changed.emit(browser.search.text)
	check(viewer.mesh_count == 0 and not viewer.visible, "filter clears model viewer")
	select_row(browser, 3)
	browser._preview_model(false)
	check(viewer.mesh_count == 0 and not browser.preview_info.text.is_empty(), "unsupported model clears geometry and explains failure")
	browser.open_archive(path)
	select_row(browser, 2)
	browser._preview_model(false)
	var changed := data.duplicate()
	changed.encode_u64(8, 3)
	var replacements: Array[PackedByteArray] = [PackedByteArray([0]), changed, fixture.model(0x44)]
	write_file("models.GOB", gob(archive_fixture.lmp(names, replacements)))
	browser._preview_model(false)
	check(viewer.mesh_count == 0 and "changed" in browser.preview_info.text, "stale model file clears old geometry")
	browser.open_archive(path)
	select_row(browser, 2)
	browser._preview_model(false)
	check(viewer.mesh_count == 0 and "No triangles" in browser.preview_info.text, "all-hidden model reports no default visible triangles")
	browser.open_archive(direct)
	select_row(browser, 1)
	browser._preview_model(false)
	browser.game_choice.select(1)
	browser._profile_changed(1)
	check(viewer.mesh_count == 0 and not viewer.visible, "profile change clears model viewer")
	browser.game_choice.select(0)
	browser._profile_changed(0)
	browser._clear()
	model_checks_complete = true

func inspect_model_file(inspector: RefCounted, path: String, index: int) -> Dictionary:
	var listing: Dictionary = inspector.inspect_archive(path)
	return inspector.inspect_model(path, index, "rta", listing.get("fingerprint", ""))

func textured_model_cases(inspector: RefCounted, browser: Control) -> void:
	var model_fixture = load("res://tests/model_fixture.gd")
	var texture_fixture = load("res://tests/texture_fixture.gd")
	var model: PackedByteArray = model_fixture.model(0x42, 2, true)
	var texture: PackedByteArray = texture_fixture.rgba_texture()
	var other := texture.duplicate()
	other[224] = 222
	var names: Array[String] = ["body.vif", "body.tex"]
	var payloads: Array[PackedByteArray] = [model, texture]
	var other_payloads: Array[PackedByteArray] = [model, other]
	var first: PackedByteArray = texture_fixture.lmp(names, payloads)
	var second: PackedByteArray = texture_fixture.lmp(names, other_payloads)
	var nested: PackedByteArray = texture_fixture.two_lmps(first, second)
	# Identical display parent names still denote distinct actual LMP containers.
	for i in range(32):
		nested[40 + i] = nested[i]
	var path := write_file("paired-models.GOB", nested)
	var decoded := inspect_model_file(inspector, path, 1)
	var alternate := inspect_model_file(inspector, path, 4)
	check(decoded.get("ok", false) and decoded.get("texture", {}).get("ok", false), "model finds its same-LMP companion")
	check(decoded["texture"]["rgba"][0] == 1 and alternate["texture"]["rgba"][0] == 222, "same named LMPs and stems resolve by actual container identity")
	var viewer = browser.model_viewer
	check(viewer.show_model(decoded, false) and viewer.texture_colors.button_pressed and not viewer.texture_colors.disabled, "valid model texture colors default on")
	var pixels: PackedByteArray = viewer.texture_pixels
	check(pixels.size() == 64 and pixels[0] == 1 and pixels[1] == 255 and pixels[12] == 40 and pixels[48] == 157, "asymmetric companion RGB retains top-left stored row order")
	var all_opaque := true
	for i in range(3, pixels.size(), 4):
		all_opaque = all_opaque and pixels[i] == 255
	check(all_opaque and decoded["texture"]["rgba"][3] == 0, "display ignores alpha without mutating decoded source")
	var mesh_instance: MeshInstance3D = viewer.model_root.get_child(0)
	var arrays: Array = mesh_instance.mesh.surface_get_arrays(0)
	check(arrays[Mesh.ARRAY_TEX_UV] == PackedVector2Array([Vector2(0.125, 0.125), Vector2(0.875, 0.125), Vector2(0.125, 0.875)]), "per-corner texel UVs divide once by width and height without a V flip")
	check(viewer.model_material.albedo_color == Color.WHITE and viewer.model_material.albedo_texture == viewer.model_texture and viewer.model_material.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED, "texture colors use untinted unlit material")
	check(viewer.model_material.texture_filter == BaseMaterial3D.TEXTURE_FILTER_NEAREST and not viewer.model_material.texture_repeat and viewer.model_material.transparency == BaseMaterial3D.TRANSPARENCY_DISABLED, "inspection material uses nearest clamp and opaque RGB")
	viewer.orbit(0.2, 0.1)
	var orbit_position: Vector3 = viewer.camera.position
	viewer.texture_colors.button_pressed = false
	check(viewer.model_material.albedo_texture == null and viewer.mesh_count == 1 and viewer.camera.position == orbit_position, "texture toggle keeps geometry and orbit while showing neutral material")
	viewer.texture_colors.button_pressed = true
	check(viewer.model_material.albedo_texture != null and viewer.camera.position == orbit_position, "texture toggle restores colors without reframing")
	var model_names: Array[String] = ["body.vif"]
	var model_payloads: Array[PackedByteArray] = [model]
	var tex_names: Array[String] = ["body.tex"]
	var tex_payloads: Array[PackedByteArray] = [texture]
	var separated: PackedByteArray = texture_fixture.two_lmps(texture_fixture.lmp(model_names, model_payloads), texture_fixture.lmp(tex_names, tex_payloads))
	var missing := inspect_model_file(inspector, write_file("separated-model.GOB", separated), 1)
	check(missing.get("ok", false) and missing["texture"]["status"] == "missing", "matching basename in another LMP is not borrowed")
	check(viewer.show_model(missing, false) and viewer.texture_colors.disabled and viewer.model_texture == null and viewer.model_material.albedo_texture == null and not viewer.texture_note.text.is_empty(), "missing companion clears previous texture and explains neutral fallback")
	var duplicate_names: Array[String] = ["body.vif", "body.tex", "BODY.TEX"]
	var duplicate_payloads: Array[PackedByteArray] = [model, texture, other]
	var ambiguous := inspect_model_file(inspector, write_file("duplicate-companions.LMP", texture_fixture.lmp(duplicate_names, duplicate_payloads)), 0)
	check(ambiguous.get("ok", false) and ambiguous["texture"]["status"] == "ambiguous" and ambiguous["texture"]["rgba"].is_empty(), "case-only duplicate companions are ambiguous")
	check(viewer.show_model(ambiguous, false) and viewer.texture_colors.disabled and viewer.model_material.albedo_texture == null, "ambiguous companion preserves neutral geometry")
	var prefix_names: Array[String] = ["folder/body.vif", "body.tex"]
	var mismatch := inspect_model_file(inspector, write_file("path-mismatch.LMP", texture_fixture.lmp(prefix_names, payloads)), 0)
	check(mismatch.get("ok", false) and mismatch["texture"]["status"] == "missing", "companion lookup preserves the full entry path")
	var unsupported := texture.duplicate()
	unsupported.encode_u16(8, 0x0101)
	var unsupported_payloads: Array[PackedByteArray] = [model, unsupported]
	var unavailable := inspect_model_file(inspector, write_file("unsupported-companion.LMP", texture_fixture.lmp(names, unsupported_payloads)), 0)
	check(unavailable.get("ok", false) and unavailable["texture"]["status"] == "unavailable" and unavailable["texture"]["rgba"].is_empty(), "unsupported companion retains model geometry without pixels")
	check(viewer.show_model(unavailable, false) and viewer.texture_colors.disabled and viewer.mesh_count == 1 and not viewer.texture_note.text.is_empty(), "unsupported companion explains neutral fallback")
	var rectangular := texture.duplicate()
	rectangular.encode_u16(2, 2)
	var rectangular_payloads: Array[PackedByteArray] = [model, rectangular]
	var rectangular_model := inspect_model_file(inspector, write_file("rectangular-companion.LMP", texture_fixture.lmp(names, rectangular_payloads)), 0)
	viewer.show_model(rectangular_model, false)
	var rectangular_mesh: MeshInstance3D = viewer.model_root.get_child(0)
	var rectangular_arrays: Array = rectangular_mesh.mesh.surface_get_arrays(0)
	check(rectangular_arrays[Mesh.ARRAY_TEX_UV] == PackedVector2Array([Vector2(0.125, 0.25), Vector2(0.875, 0.25), Vector2(0.125, 1.75)]), "rectangular companion UVs use width for U and height for V; outside values remain for clamp sampling")
	var hidden_payloads: Array[PackedByteArray] = [model_fixture.model(0x42, 3, true), texture]
	var hidden := inspect_model_file(inspector, write_file("hidden-textured-model.LMP", texture_fixture.lmp(names, hidden_payloads)), 0)
	check(not viewer.show_model(hidden, false) and viewer.mesh_count == 0 and viewer.model_texture == null and viewer.texture_pixels.is_empty() and viewer.texture_colors.disabled, "all-hidden model failure also clears prepared companion texture")
	browser.open_archive(path)
	select_row(browser, 1)
	browser._preview_model(false)
	check(browser.model_viewer.texture_colors.button_pressed, "browser model preview enables valid companion colors")
	var replacement := nested.duplicate()
	replacement[replacement.size() - 1] ^= 1
	write_file("paired-models.GOB", replacement)
	browser._preview_model(false)
	check(viewer.model_texture == null and viewer.texture_pixels.is_empty() and viewer.texture_colors.disabled and viewer.mesh_count == 0 and "changed" in browser.preview_info.text, "stale archive clears both model and texture")
	check(decoded["texture"]["rgba"][0] == 1 and decoded["texture"]["rgba"][3] == 0, "returned companion pixels remain owned after later inspections")
	browser.open_archive(path)
	var listing: Dictionary = inspector.inspect_archive(path)
	model_rejected(inspector, path, 1, "norrath", listing.get("fingerprint", ""), "textured model still rejects Norrath")
	browser.game_choice.select(1)
	browser._profile_changed(1)
	check(viewer.model_texture == null and viewer.texture_colors.disabled, "profile switch clears companion state")
	browser.game_choice.select(0)
	browser._profile_changed(0)
	browser._clear()
	textured_model_checks_complete = true

func variant_cases(inspector: RefCounted, browser: Control) -> void:
	var model_fixture = load("res://tests/model_fixture.gd")
	var texture_fixture = load("res://tests/texture_fixture.gd")
	var names: Array[String] = ["variants.vif", "variants.tex", "plain.vif"]
	var payloads: Array[PackedByteArray] = [model_fixture.variant_model(), texture_fixture.rgba_texture(), model_fixture.model()]
	var path := write_file("variants.LMP", texture_fixture.lmp(names, payloads))
	var decoded := inspect_model_file(inspector, path, 0)
	check(decoded.get("has_variants", false) and decoded["meshes"][0]["variant_visibility"].size() == 32, "variant adapter exposes 32 byte flags per mesh")
	check(decoded["meshes"][0]["variant_visibility"][0] == 0 and decoded["meshes"][1]["variant_visibility"][0] == 1, "variant bit0 swaps default visibility")
	check(decoded["meshes"][0]["variant_visibility"][1] == 0 and decoded["meshes"][1]["variant_visibility"][1] == 0, "hide wins conflicting show and hide in one variant")
	check(decoded["meshes"][0]["variant_visibility"][31] == 1 and decoded["meshes"][1]["variant_visibility"][31] == 1, "variant bit31 is preserved")
	var viewer = browser.model_viewer
	check(viewer.show_model(decoded, false) and not viewer.variant_choice.disabled and viewer.variant_choice.item_count == 33 and viewer.variant_choice.selected == 0, "variant selector starts at Default with 32 explicit single-bit choices")
	check(viewer.cached_meshes.size() == 2 and viewer.model_root.get_child_count() == 2 and viewer.mesh_count == 1, "variant model caches both surfaces once and shows default subset")
	var first_surface: int = viewer.model_root.get_child(0).mesh.get_instance_id()
	var second_surface: int = viewer.model_root.get_child(1).mesh.get_instance_id()
	viewer.orbit(0.2, 0.1)
	var original_yaw: float = viewer.yaw
	var original_pitch: float = viewer.pitch
	var original_zoom: float = viewer.distance / viewer.radius
	viewer.variant_choice.select(1)
	viewer.variant_choice.item_selected.emit(1)
	check(viewer.mesh_count == 1 and viewer.triangle_count == 1 and viewer.model_bounds == AABB(Vector3(100, 0, -1), Vector3(2, 2, 1)), "variant selection updates visible counts and actual bounds")
	check(viewer.yaw == original_yaw and viewer.pitch == original_pitch and is_equal_approx(viewer.distance / viewer.radius, original_zoom), "variant switch preserves orbit and relative zoom")
	viewer.texture_colors.button_pressed = false
	viewer.orbit(0.1, 0)
	check(viewer.variant_choice.selected == 1 and viewer.model_material.albedo_texture == null and viewer.mesh_count == 1, "texture toggle and orbit preserve variant selection")
	viewer.select_variant(2)
	check(viewer.mesh_count == 0 and viewer.triangle_count == 0 and not viewer.variant_choice.disabled and viewer.cached_meshes.size() == 2 and "no meshes visible" in viewer.info.text, "all-hidden variant retains selector and cached geometry")
	viewer.frame_model()
	viewer.select_variant(0)
	check(viewer.mesh_count == 1 and viewer.variant_choice.selected == 0 and not viewer.texture_colors.button_pressed, "Default recovers from all-hidden variant without resetting texture toggle")
	viewer.select_variant(32)
	check(viewer.mesh_count == 2 and viewer.triangle_count == 2 and viewer.model_bounds.size.x == 102, "variant bit31 reveals both cached meshes")
	check(viewer.model_root.get_child(0).mesh.get_instance_id() == first_surface and viewer.model_root.get_child(1).mesh.get_instance_id() == second_surface, "variant switches reuse existing GPU surfaces")
	# Mutating a returned result must not change already cached visibility choices.
	decoded["meshes"][0]["variant_visibility"][31] = 0
	viewer.select_variant(0)
	viewer.select_variant(32)
	check(viewer.mesh_count == 2, "viewer owns copied variant visibility data")
	var hidden_names: Array[String] = ["hidden.vif"]
	var hidden_payloads: Array[PackedByteArray] = [model_fixture.variant_model(3)]
	var hidden := inspect_model_file(inspector, write_file("variant-default-hidden.LMP", texture_fixture.lmp(hidden_names, hidden_payloads)), 0)
	viewer.yaw = 2.8
	viewer.pitch = -1.2
	viewer.distance = viewer.radius * 39.0
	check(viewer.show_model(hidden, false) and viewer.mesh_count == 0 and not viewer.variant_choice.disabled and not viewer.has_framed_geometry, "all-hidden Default can still inspect available variants")
	viewer.select_variant(32)
	check(viewer.mesh_count == 2 and viewer.has_framed_geometry, "visible variant recovers from initially hidden Default")
	var first_visible_transform: Transform3D = viewer.camera.transform
	viewer.frame_model()
	check(viewer.camera.transform.is_equal_approx(first_visible_transform) and is_equal_approx(viewer.yaw, 0.55) and is_equal_approx(viewer.pitch, 0.3), "initially hidden model frames first visible geometry independently of previous camera")
	var absent := inspect_model_file(inspector, path, 2)
	check(not absent.get("has_variants", true) and viewer.show_model(absent, false) and viewer.variant_choice.disabled and viewer.variant_choice.selected == 0, "model without variants disables and resets selector")
	browser.open_archive(path)
	select_row(browser, 0)
	browser._preview_model(false)
	viewer.select_variant(32)
	# Selector works from owned data even if the archive disappears after decoding.
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	viewer.select_variant(1)
	check(viewer.mesh_count == 1 and viewer.variant_choice.selected == 1, "variant switching does not reread the archive")
	browser._preview_model(false)
	check(viewer.cached_meshes.is_empty() and viewer.variant_choice.disabled and viewer.variant_choice.selected == 0, "failed archive reload clears variant state")
	write_file("variants.LMP", texture_fixture.lmp(names, payloads))
	browser.open_archive(path)
	select_row(browser, 0)
	browser._preview_model(false)
	viewer.select_variant(32)
	browser.game_choice.select(1)
	browser._profile_changed(1)
	check(viewer.cached_meshes.is_empty() and viewer.variant_choice.disabled and viewer.variant_choice.selected == 0, "profile switch clears variant choices and cache")
	browser.game_choice.select(0)
	browser._profile_changed(0)
	browser._clear()
	variant_checks_complete = true

func animation_rejected(inspector: RefCounted, path: String, index: int, profile: String, digest: String, tick: float, label: String) -> void:
	var result: Dictionary = inspector.sample_animation(path, index, profile, digest, tick)
	var empty := true
	for field in ["parents", "origins", "valid", "drives_joint"]:
		empty = empty and result.get(field, []).is_empty()
	check(not result.get("ok", true) and empty and not str(result.get("error", "")).is_empty()
		and result.get("name", "not empty") == "" and result.get("duration_ticks", -1) == 0
		and result.get("tick", -1) == 0, label)

func animation_cases(inspector: RefCounted, browser: Control) -> void:
	var fixture = load("res://tests/animation_fixture.gd")
	var archive_fixture = load("res://tests/texture_fixture.gd")
	var empty_wire: PackedByteArray = fixture.empty()
	check(empty_wire.slice(empty_wire.size() - 4) == PackedByteArray([0,255,0,0]), "authored animation terminal has independent literal LE16 wire encoding")
	var names: Array[String] = ["note", "motion.anm", "motion.anm", "broken.anm", "zero.anm", "mask.anm", "invalid.anm", "large.anm"]
	var payloads: Array[PackedByteArray] = [PackedByteArray([0]), fixture.hierarchy(), fixture.invalid_ancestor(), PackedByteArray([1,0,0]), fixture.empty(), fixture.mask63(), fixture.all_invalid(), fixture.large_coordinate()]
	var child: PackedByteArray = archive_fixture.lmp(names, payloads)
	var path := write_file("animations.GOB", gob(child))
	var listing: Dictionary = inspector.inspect_archive(path)
	var digest: String = listing.get("fingerprint", "")
	var result: Dictionary = inspector.sample_animation(path, 2, "rta", digest, 1.5)
	check(result.get("ok", false) and result.get("duration_ticks", -1) == 8 and result.get("tick", -1) == 1.5, "animation bridge accepts explicit fractional tick")
	check(result.get("parents", PackedInt32Array()) == PackedInt32Array([-1,0,-1]), "animation hierarchy retains child and independent root")
	check(result.get("origins", PackedVector3Array()) == PackedVector3Array([Vector3(2.5,2,3), Vector3(2.5,4,4), Vector3(-2,1,-1)]), "animation sampled origins independently anchor parent rotation and fractional translation")
	check(result.get("valid", PackedByteArray()) == PackedByteArray([1,1,1]) and result.get("drives_joint", PackedByteArray()) == PackedByteArray([0,1,1]), "animation masked parent remains numerically valid and composes into child")
	var initial: Dictionary = inspector.sample_animation(path, 2, "rta", digest, 0)
	var advanced: Dictionary = inspector.sample_animation(path, 2, "rta", digest, 5)
	check(advanced.get("origins", PackedVector3Array()) == PackedVector3Array([Vector3(4,2,3), Vector3(4,4,4), Vector3(-2,1,-1)]), "animation duplicate-time velocity records settle in stored order")
	var repeated: Dictionary = inspector.sample_animation(path, 2, "rta", digest, 0)
	var saved_origins: PackedVector3Array = result.get("origins", PackedVector3Array())
	check(initial.get("origins") == repeated.get("origins") and saved_origins.size() == 3 and saved_origins[0] == Vector3(2.5,2,3), "animation backward repeat is stateless and prior result owns origins")
	var duplicate: Dictionary = inspector.sample_animation(path, 3, "rta", digest, 0)
	check(duplicate.get("ok", false) and duplicate.get("valid", PackedByteArray()) == PackedByteArray([0,0,0,1])
		and duplicate.get("origins", PackedVector3Array()) == PackedVector3Array([Vector3.ZERO,Vector3.ZERO,Vector3.ZERO,Vector3(3,-2,1)]), "duplicate animation name uses exact row; invalid ancestor propagates while independent root survives")
	var empty: Dictionary = inspector.sample_animation(path, 5, "rta", digest, 0)
	check(empty.get("ok", false) and empty.get("origins", PackedVector3Array()).is_empty() and empty.get("duration_ticks", -1) == 0, "zero-joint zero-duration animation is valid empty data")
	var masked: Dictionary = inspector.sample_animation(path, 6, "rta", digest, 0)
	var mask_flags: PackedByteArray = masked.get("drives_joint", PackedByteArray())
	check(masked.get("ok", false) and mask_flags.size() == 64 and mask_flags.count(0) == 1 and mask_flags[63] == 0, "animation mask bit63 survives bridge without signed integer loss")
	var direct := write_file("animations.LMP", child)
	var direct_listing: Dictionary = inspector.inspect_archive(direct)
	check(inspector.sample_animation(direct, 1, "rta", direct_listing.get("fingerprint", ""), 1.5).get("origins") == result.get("origins"), "direct LMP and nested animation sample agree")
	animation_rejected(inspector, path, 2, "norrath", digest, 0, "animation rejects Norrath and clears all result fields")
	animation_rejected(inspector, path, 1, "rta", digest, 0, "animation rejects non-anm entry")
	animation_rejected(inspector, path, -1, "rta", digest, 0, "animation rejects negative row")
	animation_rejected(inspector, path, 99, "rta", digest, 0, "animation rejects missing row")
	animation_rejected(inspector, path, 4, "rta", digest, 0, "malformed animation produces no partial joints")
	animation_rejected(inspector, path, 2, "rta", "", 0, "animation requires archive fingerprint")
	for tick in [-1.0, 8.01, INF, NAN]:
		animation_rejected(inspector, path, 2, "rta", digest, tick, "animation rejects out-of-range or nonfinite tick")
	var malformed: PackedByteArray = archive_fixture.two_lmps(child, PackedByteArray([1,0,0]))
	var malformed_path := write_file("animation-later-malformed.GOB", malformed)
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(malformed)
	animation_rejected(inspector, malformed_path, 2, "rta", hash.finish().hex_encode(), 0, "animation preview validates later malformed container even with matching hash")

	browser.open_archive(path)
	browser.search.text = "motion"
	browser.search.text_changed.emit(browser.search.text)
	select_row(browser, 2)
	var viewer = browser.animation_viewer
	check(browser.animation_row_index == 2 and not browser.animation_button.disabled and browser.model_button.disabled and browser.preview_button.disabled, "filtered animation selection keeps original row and enables only animation inspection")
	check(viewer.total_joint_count == 3 and viewer.joint_count == 3 and viewer.link_count == 1 and viewer.masked_count == 1, "animation selection automatically samples tick0 with valid joint and link counts")
	browser._sample_animation(1.5, false, true)
	check(viewer.points == PackedVector3Array([Vector3(2.5,3,-2),Vector3(2.5,4,-4),Vector3(-2,-1,-1)]), "animation viewer maps Z-up origins to Godot Y-up exactly once")
	viewer.orbit(0.3, 0.2)
	viewer.zoom(0.8)
	var camera_before: Transform3D = viewer.camera.transform
	var distance_before: float = viewer.distance
	var origin_before: Vector3 = viewer.display_origin
	browser._sample_animation(5, false, true)
	check(viewer.camera.transform.is_equal_approx(camera_before) and is_equal_approx(viewer.distance, distance_before) and viewer.display_origin == origin_before, "manual animation sample preserves camera, zoom and display origin")
	browser._sample_animation(0, false, true)
	check(viewer.points.size() == 3 and viewer.points[0] == Vector3(1,3,-2), "viewer supports backward sampling")
	viewer.frame_animation()
	check(not viewer.camera.transform.is_equal_approx(camera_before), "explicit animation Frame reframes sampled joints")
	var owned_input: Dictionary = initial.duplicate(true)
	check(viewer.show_animation(owned_input, false, false), "animation viewer accepts owned bridge result")
	owned_input["origins"][0] = Vector3(900,900,900)
	owned_input["parents"][1] = -1
	owned_input["valid"][0] = 0
	owned_input["drives_joint"][0] = 1
	check(viewer.points[0] == Vector3(1,3,-2) and viewer.parents[1] == 0 and viewer.valid[0] == 1 and viewer.drives_joint[0] == 0, "animation viewer owns copied points, hierarchy and flags")
	viewer.tick_input.get_line_edit().text = "1.5"
	viewer.sample_button.pressed.emit()
	check(viewer.sampled_tick == 1.5 and viewer.points[0] == Vector3(2.5,3,-2), "Sample button applies typed fractional tick and reaches native bridge")
	var before_drag: Transform3D = viewer.camera.transform
	var press := InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = Vector2(30,40)
	viewer.input_surface.gui_input.emit(press)
	var motion := InputEventMouseMotion.new()
	motion.position = Vector2(42,45)
	motion.relative = Vector2.ZERO
	motion.button_mask = 0
	viewer.input_surface.gui_input.emit(motion)
	check(not viewer.camera.transform.is_equal_approx(before_drag), "animation pointer drag uses positions despite zero relative and button mask")
	var after_drag: Transform3D = viewer.camera.transform
	var release := InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	viewer.window_input.emit(release)
	motion.position = Vector2(50,60)
	viewer.input_surface.gui_input.emit(motion)
	check(not viewer.dragging and viewer.camera.transform.is_equal_approx(after_drag), "animation release outside viewport ends drag")
	viewer.input_surface.gui_input.emit(press)
	viewer.focus_exited.emit()
	viewer.input_surface.gui_input.emit(motion)
	check(not viewer.dragging and viewer.camera.transform.is_equal_approx(after_drag), "animation focus loss ends drag")
	var wheel := InputEventMouseButton.new()
	wheel.button_index = MOUSE_BUTTON_WHEEL_UP
	wheel.pressed = true
	var before_wheel: float = viewer.distance
	viewer.input_surface.gui_input.emit(wheel)
	check(viewer.distance < before_wheel and not viewer.input_surface.mouse_force_pass_scroll_events, "animation wheel zoom is handled without page scroll propagation")
	var large: Dictionary = inspector.sample_animation(path, 8, "rta", digest, 4294967295.0)
	var large_expected := Vector3(549739036672.0,549739036672.0,549739036672.0)
	check(large.get("ok", false) and large.get("origins", PackedVector3Array()) == PackedVector3Array([large_expected]), "authored maximum-duration velocity has independently calculated single-precision origin")
	check(viewer.show_animation(large, false, false) and viewer.points == PackedVector3Array([Vector3(large_expected.x,large_expected.z,-large_expected.y)])
		and viewer.camera.position.is_finite() and not viewer.camera.position.is_equal_approx(viewer.center), "large animation origin remains owned while rebased camera stays finite and nondegenerate")
	check(viewer.joint_root.get_child_count() == 1 and viewer.joint_root.get_child(0).position.is_zero_approx(), "large-coordinate joint marker is rendered near local origin")
	var large_origin: Vector3 = viewer.display_origin
	var large_camera: Transform3D = viewer.camera.transform
	var large_initial: Dictionary = inspector.sample_animation(path, 8, "rta", digest, 0)
	check(viewer.show_animation(large_initial, false, true) and viewer.display_origin == large_origin and viewer.camera.transform.is_equal_approx(large_camera), "large-coordinate backward sample preserves camera and render origin until Frame")
	viewer.frame_animation()
	check(viewer.display_origin.is_zero_approx() and viewer.joint_root.get_child(0).position.is_zero_approx()
		and not viewer.camera.position.is_equal_approx(viewer.center), "Frame rebases updated animation geometry and camera together")
	select_row(browser, 3)
	check(viewer.total_joint_count == 4 and viewer.joint_count == 1 and viewer.link_count == 0 and viewer.invalid_count == 3, "invalid ancestor markers and links are omitted; independent root remains visible")
	browser.search.text = "zero"
	browser.search.text_changed.emit(browser.search.text)
	check(viewer.total_joint_count == 0 and not viewer.visible, "filter clears animation geometry and closes viewer")
	select_row(browser, 5)
	check(viewer.joint_count == 0 and viewer.link_count == 0, "empty animation does not fabricate origin geometry")
	browser.search.text = "invalid"
	browser.search.text_changed.emit(browser.search.text)
	select_row(browser, 7)
	check(viewer.total_joint_count == 1 and viewer.joint_count == 0 and viewer.link_count == 0 and viewer.invalid_count == 1, "all-invalid animation displays no replacement identity joint")
	browser.open_archive(path)
	select_row(browser, 2)
	var replacements: Array[PackedByteArray] = payloads.duplicate()
	replacements[1] = fixture.invalid_ancestor()
	write_file("animations.GOB", gob(archive_fixture.lmp(names, replacements)))
	browser._sample_animation(1, false, true)
	check(viewer.total_joint_count == 0 and not viewer.visible and "changed" in browser.preview_info.text, "same-path archive replacement clears old animation on sample")
	animation_rejected(inspector, path, 2, "rta", digest, 0, "bridge rejects stale animation archive hash")
	write_file("animations.GOB", gob(child))
	browser.open_archive(path)
	select_row(browser, 2)
	browser._sample_animation(-1, false, true)
	check(viewer.total_joint_count == 0 and not viewer.visible, "failed manual animation sample clears old geometry")
	browser.open_archive(path)
	select_row(browser, 2)
	browser.game_choice.select(1)
	browser._profile_changed(1)
	check(viewer.total_joint_count == 0 and not viewer.visible and browser.animation_button.disabled, "profile change clears animation context")
	browser.game_choice.select(0)
	browser._profile_changed(0)
	browser._clear()
	animation_checks_complete = true

func skin_positions(result: Dictionary, mesh_slot := 0) -> PackedVector3Array:
	var meshes: Array = result.get("meshes", [])
	if mesh_slot < 0 or mesh_slot >= meshes.size() or typeof(meshes[mesh_slot].get("positions")) != TYPE_PACKED_VECTOR3_ARRAY:
		return PackedVector3Array()
	return meshes[mesh_slot]["positions"]

func skin_positions_equal(actual: PackedVector3Array, expected: PackedVector3Array) -> bool:
	if actual.size() != expected.size():
		return false
	for index in range(actual.size()):
		if not actual[index].is_finite() or not actual[index].is_equal_approx(expected[index]):
			return false
	return true

func skinning_rejected(inspector: RefCounted, path: String, model_row: int, animation_row: int, profile: String, fingerprint: String, tick: float, label: String) -> void:
	var result: Dictionary = inspector.sample_skinned_model(path, model_row, animation_row, profile, fingerprint, tick)
	check(not result.get("ok", true) and not str(result.get("error", "")).is_empty()
		and result.get("meshes", [1]).is_empty() and result.get("model_name", "x") == ""
		and result.get("animation_name", "x") == "" and result.get("tick", -1) == 0
		and result.get("duration_ticks", -1) == 0 and result.get("triangle_count", -1) == 0
		and result.get("pairing_verified", true) == false, label)

func skinning_cases(inspector: RefCounted) -> void:
	var fixture = load("res://tests/skinning_fixture.gd")
	var models = load("res://tests/model_fixture.gd")
	var animations = load("res://tests/animation_fixture.gd")
	var archives = load("res://tests/texture_fixture.gd")
	var attachment: PackedByteArray = models.model()
	attachment[0x13] = 0
	var above_mask: PackedByteArray = fixture.animation()
	above_mask[0x4f] = 128 # Bit63 is above this four-joint clip's joint count.
	var empty_mask: PackedByteArray = animations.empty()
	empty_mask[0x48] = 1
	var broken: PackedByteArray = fixture.animation()
	broken.resize(broken.size() - 4)
	var names: Array[String] = ["note", "shape.vif", "shape.vif", "motion.anm", "motion.anm", "attachment.vif", "badref.vif", "mask.anm", "invalid.anm", "broken.anm", "zero-invalid.vif", "joint63.vif", "full63.anm", "above-mask.anm", "empty-mask.anm"]
	var payloads: Array[PackedByteArray] = [PackedByteArray([0]), models.model(), fixture.model(), fixture.animation(), fixture.animation(512), attachment, fixture.model("rigid63"), animations.mask63(), fixture.invalid_ancestor(), broken, fixture.model("zero_invalid"), fixture.model("rigid63"), fixture.full63(), above_mask, empty_mask]
	var child: PackedByteArray = archives.lmp(names, payloads)
	var path := write_file("skinning.GOB", gob(child))
	var listing: Dictionary = inspector.inspect_archive(path)
	var digest: String = listing.get("fingerprint", "")
	var player_body: Dictionary = inspector.read_player_resource(path, 2, digest, "shape.vif")
	var player_clip: Dictionary = inspector.read_player_resource(path, 4, digest, "motion.anm")
	check(player_body.get("ok", false), "player body is copied from a validated row: " + str(player_body.get("error", "")))
	check(player_clip.get("ok", false), "player clip is copied from a validated row: " + str(player_clip.get("error", "")))
	check(player_body.get("bytes", PackedByteArray()) == payloads[1]
		and player_clip.get("bytes", PackedByteArray()) == payloads[3],
		"selected player resources retain the fixture bytes")
	check(not inspector.read_player_resource(path, 4, digest, "other.anm").get("ok", false),
		"player clip row must retain its selected name")
	var rigid: Dictionary = inspector.sample_skinned_model(path, 2, 4, "rta", digest, 1.5)
	check(rigid.get("ok", false) and rigid.get("duration_ticks", -1) == 8 and rigid.get("tick", -1) == 1.5
		and rigid.get("triangle_count", -1) == 2 and rigid.get("model_name", "") == "shape.vif"
		and rigid.get("animation_name", "") == "motion.anm" and rigid.get("pairing_verified", true) == false,
		"explicit model/animation rows sample fractionally without claiming semantic rig compatibility")
	var rigid_meshes: Array = rigid.get("meshes", [])
	check(rigid_meshes.size() == 2, "skinning preserves both original meshes including default-hidden mesh")
	if rigid_meshes.size() != 2:
		return
	check(rigid_meshes[0].get("index", -1) == 0 and rigid_meshes[1].get("index", -1) == 1
		and rigid_meshes[0].get("visible_by_default", false) and not rigid_meshes[1].get("visible_by_default", true),
		"skinning retains mesh order and visibility independently of parent part mask")
	check(skin_positions_equal(skin_positions(rigid), PackedVector3Array([Vector3(2.5,2,3),Vector3(2.5,4,3),Vector3(1.5,2,5)])),
		"rigid sampled corners independently anchor rotation, translation and native Z-up order")
	check(skin_positions_equal(skin_positions(rigid, 1), PackedVector3Array([Vector3(2.5,102,3),Vector3(2.5,104,3),Vector3(1.5,102,5)])),
		"default-hidden mesh keeps distinct translated source corners")
	var posed_normal := Vector3(114.0 / 127.0, 0, 57.0 / 127.0)
	var rigid_normals: PackedVector3Array = rigid_meshes[0].get("normals", PackedVector3Array())
	check(rigid_meshes[0].keys().size() == 4 and rigid_normals.size() == 3
		and rigid_normals[0].is_equal_approx(posed_normal)
		and rigid_normals[1].is_equal_approx(posed_normal)
		and rigid_normals[2].is_equal_approx(posed_normal)
		and not rigid_meshes[0].has("uv_texels") and not rigid.has("texture"),
		"skinned sample returns the joint rotation of the stored normal and still omits UV and material data")
	var weighted: Dictionary = inspector.sample_skinned_model(path, 3, 4, "rta", digest, 1.5)
	var expected := PackedVector3Array([Vector3(-31.0/255,892.0/255,574.0/255),Vector3(735.0/255,690.0/255,555.0/255),Vector3(45.0/255,210.0/255,65.0/255)])
	check(weighted.get("ok", false) and weighted.get("triangle_count", -1) == 1 and skin_positions_equal(skin_positions(weighted), expected),
		"duplicate model name resolves exact row; two/three/four byte-weight blends match independent rotated positions")
	var initial: Dictionary = inspector.sample_skinned_model(path, 3, 4, "rta", digest, 0)
	var advanced: Dictionary = inspector.sample_skinned_model(path, 3, 4, "rta", digest, 5)
	check(skin_positions_equal(skin_positions(advanced), PackedVector3Array([Vector3(65.0/255,892.0/255,574.0/255),Vector3(885.0/255,690.0/255,555.0/255),Vector3(45.0/255,210.0/255,65.0/255)])),
		"skinned fractional motion honors duplicate-time velocity replacement and zero-weight contribution")
	var repeated: Dictionary = inspector.sample_skinned_model(path, 3, 4, "rta", digest, 0)
	check(skin_positions(initial) == skin_positions(repeated) and skin_positions_equal(skin_positions(weighted), expected),
		"backward repeated skinning is stateless and previous result owns its arrays")
	weighted["meshes"][0]["positions"][0] = Vector3(999,999,999)
	var after_mutation: Dictionary = inspector.sample_skinned_model(path, 3, 4, "rta", digest, 1.5)
	check(skin_positions_equal(skin_positions(after_mutation), expected) and skin_positions(initial) == skin_positions(repeated),
		"mutating one returned position array cannot alter another result or future sample")
	var duplicate_animation: Dictionary = inspector.sample_skinned_model(path, 2, 5, "rta", digest, 0)
	check(duplicate_animation.get("ok", false) and skin_positions_equal(skin_positions(duplicate_animation), PackedVector3Array([Vector3(8,2,3),Vector3(8,4,3),Vector3(7,2,5)]))
		and duplicate_animation.get("pairing_verified", true) == false, "same-count duplicate animation resolves exact row but never certifies a matching rig")
	var high_joint: Dictionary = inspector.sample_skinned_model(path, 12, 13, "rta", digest, 0)
	check(high_joint.get("ok", false) and skin_positions_equal(skin_positions(high_joint), PackedVector3Array([Vector3(63,0,0),Vector3(65,0,0),Vector3(63,1,2)])),
		"joint63 influences survive the bridge and select the final transform")
	var direct_path := write_file("skinning.LMP", child)
	var direct_listing: Dictionary = inspector.inspect_archive(direct_path)
	var direct: Dictionary = inspector.sample_skinned_model(direct_path, 2, 3, "rta", direct_listing.get("fingerprint", ""), 1.5)
	check(skin_positions_equal(skin_positions(direct), expected), "direct LMP and nested GOB explicitly selected rows agree")
	# Explicit pair selection may cross containers. No filename search or inferred pairing.
	var model_names: Array[String] = ["different.vif"]
	var animation_names: Array[String] = ["unrelated.anm"]
	var model_payloads: Array[PackedByteArray] = [fixture.model()]
	var animation_payloads: Array[PackedByteArray] = [fixture.animation()]
	var split_path := write_file("skinning-split.GOB", archives.two_lmps(archives.lmp(model_names,model_payloads), archives.lmp(animation_names,animation_payloads)))
	var split_listing: Dictionary = inspector.inspect_archive(split_path)
	var split: Dictionary = inspector.sample_skinned_model(split_path, 1, 3, "rta", split_listing.get("fingerprint", ""), 1.5)
	check(split.get("ok", false) and split.get("pairing_verified", true) == false and skin_positions_equal(skin_positions(split), expected),
		"explicit cross-container rows are accepted without matching names or compatibility claim")

	skinning_rejected(inspector,path,2,4,"norrath",digest,0,"skinning rejects Norrath atomically")
	skinning_rejected(inspector,path,2,4,"rta","",0,"skinning requires a current archive fingerprint")
	for pair in [[-1,4],[99,4],[2,-1],[2,99],[1,4],[2,1],[4,2],[2,2]]:
		skinning_rejected(inspector,path,pair[0],pair[1],"rta",digest,0,"invalid row or extension rejects both outputs atomically")
	for tick in [-1.0,8.01,INF,NAN]:
		skinning_rejected(inspector,path,2,4,"rta",digest,tick,"out-of-range or nonfinite skinning time clears output")
	skinning_rejected(inspector,path,6,4,"rta",digest,0,"attachment model needs parent assembly and is rejected")
	skinning_rejected(inspector,path,7,4,"rta",digest,0,"influence outside selected animation joint count is rejected")
	skinning_rejected(inspector,path,2,8,"rta",digest,0,"partial clip is rejected even when its masked joint is unused")
	skinning_rejected(inspector,path,2,14,"rta",digest,0,"raw mask bit above animation joint count remains an unsupported partial clip")
	skinning_rejected(inspector,path,2,15,"rta",digest,0,"zero-joint clip with nonzero raw mask is rejected")
	skinning_rejected(inspector,path,2,9,"rta",digest,0,"invalid ancestor transform cannot skin referenced corners")
	skinning_rejected(inspector,path,11,9,"rta",digest,0,"invalid zero-weight referenced joint is rejected before publication")
	skinning_rejected(inspector,path,2,10,"rta",digest,0,"malformed animation stream leaves no partial skinned mesh")
	var bad_model_names: Array[String] = ["broken.vif", "good.anm"]
	var bad_model_payloads: Array[PackedByteArray] = [PackedByteArray([0,1,2]),fixture.animation()]
	var bad_model_path := write_file("skinning-bad-model.LMP", archives.lmp(bad_model_names,bad_model_payloads))
	var bad_model_listing: Dictionary = inspector.inspect_archive(bad_model_path)
	skinning_rejected(inspector,bad_model_path,0,1,"rta",bad_model_listing.get("fingerprint", ""),0,"malformed model leaves no partial sampled output")
	var malformed: PackedByteArray = archives.two_lmps(child,PackedByteArray([1,0,0]))
	var malformed_path := write_file("skinning-later-malformed.GOB",malformed)
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(malformed)
	skinning_rejected(inspector,malformed_path,2,4,"rta",hash.finish().hex_encode(),0,"full archive walk rejects malformed later LMP despite matching fingerprint and earlier pair")
	var replacements: Array[PackedByteArray] = payloads.duplicate()
	replacements[3] = fixture.animation(512)
	write_file("skinning.GOB",gob(archives.lmp(names,replacements)))
	skinning_rejected(inspector,path,2,4,"rta",digest,0,"changing only selected animation at same path invalidates shared archive fingerprint")
	check(not inspector.read_player_resource(path, 4, digest, "motion.anm").get("ok", false),
		"changed clip archive refuses a saved player row and fingerprint")
	replacements = payloads.duplicate()
	replacements[1] = fixture.model()
	write_file("skinning.GOB",gob(archives.lmp(names,replacements)))
	skinning_rejected(inspector,path,2,4,"rta",digest,0,"changing only selected model at same path invalidates shared archive fingerprint")
	check(not inspector.read_player_resource(path, 2, digest, "shape.vif").get("ok", false),
		"changed body archive refuses a saved player row and fingerprint")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	skinning_rejected(inspector,path,2,4,"rta",digest,0,"missing archive yields fully cleared skinning failure")
	check(skin_positions_equal(skin_positions(rigid), PackedVector3Array([Vector3(2.5,2,3),Vector3(2.5,4,3),Vector3(1.5,2,5)])),
		"earlier result remains owned after source archive changes and removal")
	skinning_checks_complete = true
