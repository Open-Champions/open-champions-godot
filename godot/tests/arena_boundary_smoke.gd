extends SceneTree

# Table-loading boundary only: no start_play_session, live match, or mode-B
# Session wiring. All bytes and names are authored here, never read from a game.
# Layout: native/tests/level_flow_tests.cpp::arena_executable_case; the strings
# are placed before the tables so a truncated table cannot fail on a string.
# Run from this worktree with an already-built extension:
# "$GODOT" --headless --path godot --script res://tests/arena_boundary_smoke.gd
# Each run owns a fresh user:// subdirectory; cleanup never walks other files.

const SEGMENT_ADDRESS := 0x503000
const SEGMENT_OFFSET := 0x100
const SEGMENT_SIZE := 0x900
const NAMES_A_TABLE := 0x5037b8
const NAMES_B_TABLE := 0x503798
const ELF_ERROR := "The executable is not an ELF32 image."
const ADDRESS_ERROR := "Arena table address is not file-backed."
const COUNT_ERROR := "Arena name tables are not the documented 13 and 7 entries."

var checks := 0
var failures := 0
var directory := ""
var owned_files: Array[String] = []


func check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error("ARENA_BOUNDARY_SMOKE: " + label)


func _initialize() -> void:
	call_deferred("run")


func file_offset(address: int) -> int:
	return SEGMENT_OFFSET + address - SEGMENT_ADDRESS


func put_names(bytes: PackedByteArray, table: int, count: int, start: int, prefix: String) -> void:
	for i in range(count):
		var address := start + 16 * i
		bytes.encode_u32(file_offset(table) + 4 * i, address)
		var name_bytes := ("%s%d" % [prefix, i]).to_ascii_buffer()
		for j in range(name_bytes.size()):
			bytes[file_offset(address) + j] = name_bytes[j]
		bytes[file_offset(address) + name_bytes.size()] = 0
	bytes.encode_u32(file_offset(table) + 4 * count, 0)


func synthetic_elf(count_a: int = 13, count_b: int = 7) -> PackedByteArray:
	var bytes := PackedByteArray()
	bytes.resize(SEGMENT_OFFSET + SEGMENT_SIZE)
	bytes.fill(0)
	bytes[0] = 0x7f
	bytes[1] = 0x45 # E
	bytes[2] = 0x4c # L
	bytes[3] = 0x46 # F
	bytes[4] = 1 # ELF32
	bytes[5] = 1 # Little endian
	bytes[6] = 1 # ELF version
	bytes.encode_u32(0x1c, 0x34) # Program-header offset
	bytes.encode_u16(0x28, 0x34) # ELF header size
	bytes.encode_u16(0x2a, 0x20) # Program-header entry size
	bytes.encode_u16(0x2c, 1) # One segment
	bytes.encode_u32(0x34, 1) # PT_LOAD
	bytes.encode_u32(0x38, SEGMENT_OFFSET)
	bytes.encode_u32(0x3c, SEGMENT_ADDRESS)
	bytes.encode_u32(0x44, SEGMENT_SIZE)
	bytes.encode_u32(0x48, SEGMENT_SIZE)
	put_names(bytes, NAMES_A_TABLE, count_a, 0x503100, "A")
	put_names(bytes, NAMES_B_TABLE, count_b, 0x503200, "B")
	return bytes


func write_fixture(name: String, bytes: PackedByteArray) -> String:
	var path := directory.path_join(name)
	check(not FileAccess.file_exists(path), name + ": fixture must be new")
	if FileAccess.file_exists(path):
		return ""
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, name + ": open authored fixture")
	if file == null:
		return ""
	owned_files.append(path)
	file.store_buffer(bytes)
	file.flush()
	var written := file.get_error() == OK and file.get_length() == bytes.size()
	file.close()
	check(written, name + ": write complete fixture")
	return path if written else ""


func unchanged(controller: Object, before: Dictionary, capabilities: Dictionary, label: String) -> void:
	var after: Dictionary = controller.get_observation()
	check(after == before, label + ": diagnostic observation unchanged")
	check(after.get("session_mode", "") == "diagnostic", label + ": no play session fabricated")
	check(controller.get_play_capabilities() == capabilities, label + ": capabilities unchanged")


func load_case(controller: Object, path: String, error: String, before: Dictionary, capabilities: Dictionary) -> void:
	var label := path.get_file()
	var result: Dictionary = controller.load_arena_tables(path)
	if error.is_empty():
		check(result.get("ok", false) == true, label + ": synthetic ELF accepted: " + str(result))
		check(result.get("names_a", -1) == 13 and result.get("names_b", -1) == 7,
			label + ": validated table counts are 13/7")
	else:
		check(result.get("ok", true) == false, label + ": refused")
		check(result.get("error", "") == error, label + ": expected error: " + str(result))
		check(not result.has("names_a") and not result.has("names_b"), label + ": no success counts on refusal")
	unchanged(controller, before, capabilities, label)


func fixture_case(controller: Object, name: String, bytes: PackedByteArray, error: String, before: Dictionary, capabilities: Dictionary) -> void:
	var path := write_fixture(name, bytes)
	if not path.is_empty():
		load_case(controller, path, error, before, capabilities)


func finish() -> void:
	for path in owned_files:
		check(DirAccess.remove_absolute(ProjectSettings.globalize_path(path)) == OK,
			"remove owned fixture " + path.get_file())
	if not directory.is_empty():
		# Nonrecursive removal refuses if any unowned file is present.
		check(DirAccess.remove_absolute(ProjectSettings.globalize_path(directory)) == OK,
			"remove owned empty fixture directory")
	print("ARENA_BOUNDARY_SMOKE_%s checks=%d failures=%d" % [
		"PASS" if failures == 0 else "FAIL", checks, failures])
	quit(0 if failures == 0 else 1)


func run() -> void:
	print("ARENA_BOUNDARY_SMOKE_BEGIN")
	for method in ["load_arena_tables", "get_observation", "get_play_capabilities"]:
		check(ClassDB.class_has_method("ChampionsSessionController", method), "bound method " + method)
	if failures != 0:
		finish()
		return
	var controller = ClassDB.instantiate("ChampionsSessionController")
	check(controller != null, "session controller exists")
	if controller == null:
		finish()
		return
	var before: Dictionary = controller.get_observation().duplicate(true)
	var capabilities: Dictionary = controller.get_play_capabilities().duplicate(true)
	check(before.get("ok", false) == true, "initial diagnostic observation is healthy")
	check(before.get("session_mode", "") == "diagnostic", "controller starts outside play")
	if failures != 0:
		controller = null
		finish()
		return
	var candidate := "user://arena-boundary-smoke-%d-%d" % [OS.get_process_id(), Time.get_ticks_usec()]
	var absolute := ProjectSettings.globalize_path(candidate)
	check(not DirAccess.dir_exists_absolute(absolute), "fixture directory is fresh")
	if failures == 0:
		check(DirAccess.make_dir_absolute(absolute) == OK, "create isolated fixture directory")
	if failures != 0:
		controller = null
		finish()
		return
	directory = candidate

	fixture_case(controller, "valid.elf", synthetic_elf(), "", before, capabilities)
	load_case(controller, directory.path_join("absent.elf"), "The executable could not be opened.", before, capabilities)
	fixture_case(controller, "empty.elf", PackedByteArray(), ELF_ERROR, before, capabilities)
	fixture_case(controller, "short-header.elf", synthetic_elf().slice(0, 0x33), ELF_ERROR, before, capabilities)
	var truncated := synthetic_elf()
	truncated.resize(truncated.size() - 1) # Declared PT_LOAD extends past EOF.
	fixture_case(controller, "short-segment.elf", truncated, ELF_ERROR, before, capabilities)
	truncated = synthetic_elf()
	# Keep the ELF segment valid but omit the mode-A table's terminating word.
	var table_end := file_offset(NAMES_A_TABLE) + 13 * 4
	truncated.resize(table_end)
	truncated.encode_u32(0x44, table_end - SEGMENT_OFFSET)
	truncated.encode_u32(0x48, table_end - SEGMENT_OFFSET)
	fixture_case(controller, "short-table.elf", truncated, ADDRESS_ERROR, before, capabilities)
	var malformed := synthetic_elf()
	malformed[0] = 0
	fixture_case(controller, "bad-magic.elf", malformed, ELF_ERROR, before, capabilities)
	fixture_case(controller, "count-a-12.elf", synthetic_elf(12, 7), COUNT_ERROR, before, capabilities)
	fixture_case(controller, "count-a-14.elf", synthetic_elf(14, 7), COUNT_ERROR, before, capabilities)
	fixture_case(controller, "count-b-6.elf", synthetic_elf(13, 6), COUNT_ERROR, before, capabilities)
	# A refusal clears the internal name cache; a fresh valid load still works.
	fixture_case(controller, "valid-after-refusals.elf", synthetic_elf(), "", before, capabilities)
	controller = null
	finish()
