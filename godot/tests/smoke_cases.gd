extends RefCounted

var failures := 0
var checks := 0
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
	for path in files:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(DIRECTORY))
	print("Archive browser checks: ", checks, "; failures: ", failures)
	return failures
