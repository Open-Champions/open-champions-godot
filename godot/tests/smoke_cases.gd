extends RefCounted

var failures := 0
var checks := 0
var texture_checks_complete := false
var plain_four_checks_complete := false
var eight_bit_checks_complete := false
var model_checks_complete := false
var textured_model_checks_complete := false
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
