extends SceneTree

# Synthetic slot images only (ChampionsPs2SaveImport.synthetic_slot builds one
# with native build_slot). No save from a card is read or written.
var failures := 0

func check(condition: bool, label: String) -> void:
	if not condition:
		failures += 1
		push_error("FAIL " + label)
	else:
		print("PASS " + label)

func _initialize() -> void:
	call_deferred("_run")

func _write(path: String, data: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_buffer(data)
	file.close()

func _run() -> void:
	check(ClassDB.class_exists("ChampionsPs2SaveImport"), "import class registered")
	if failures > 0:
		quit(1)
		return
	var size := int(ClassDB.class_call_static("ChampionsPs2SaveImport", "file_size"))
	var image: PackedByteArray = ClassDB.class_call_static("ChampionsPs2SaveImport", "synthetic_slot", "Aria", 777)
	check(size == 0x4bc78 and image.size() == size, "synthetic slot is one 0x4bc78-byte image")
	var importer = ClassDB.instantiate("ChampionsPs2SaveImport")
	var result: Dictionary = importer.import_bytes(image, 0)
	check(result.get("ok", false), "synthetic slot imports: %s" % result.get("error", ""))
	check(result.name == "Aria" and result.gold == 777 and result.level == 4 and result.experience == 5000, "character fields")
	check(result.base_stats == [46, 37, 24, 28] and result.bag_counts == [1, 0, 0] and result.skills == [[7, 2]], "stats, bags and skills")
	check(result.quests == [["opened", 7], ["door", -2]] and importer.quest_value("door") == -2, "quest table in native state")
	check(importer.record_word(0x130) == 0x3f7710 and importer.record_word(0x43d8) == 0, "definition pointer restored, object pointer cleared")
	check(importer.record_word(0x43f0) == 0x4d8af0 + 0x130, "equipment index restored to the live item address")
	var player_record: PackedByteArray = importer.record_bytes()
	check(player_record.size() == 0x4c30, "import exposes one in-memory player record")
	var changed := player_record.duplicate()
	for byte in range(4):
		changed[0x43f0 + byte] = 0
	check(importer.replace_record(changed).ok and importer.record_word(0x43f0) == 0, "menu writeback clears the imported equipment pointer")
	check(importer.replace_record(player_record).ok and importer.record_word(0x43f0) == 0x4d8af0 + 0x130, "menu writeback restores the imported equipment pointer")
	check(result.level_name == "crypt" and result.difficulty == 2 and result.play_time == 12345, "campaign header")
	check(result.refusals.size() == 1 and str(result.refusals[0]).begins_with("camera:"), "camera listed as not applied")
	var bad := image.duplicate()
	bad[0x68] = 0
	var refused: Dictionary = importer.import_bytes(bad, 0)
	check(not refused.get("ok", true) and "record size" in str(refused.error), "bad record-size field refused")
	check(importer.has_character() and importer.quest_value("opened") == 7, "refusal keeps the previous import")
	check(not importer.import_bytes(image.slice(0, size - 1), 0).ok, "short image refused")
	check(not importer.import_bytes(image, 1).ok, "missing character index refused")
	var path := "user://ps2_save_import_smoke_slot0"
	_write(path, image)
	var tiny := "user://ps2_save_import_smoke_tiny"
	_write(tiny, PackedByteArray([1, 2, 3]))
	check(importer.import_file(path, 0).ok, "import from a file")
	check(not importer.import_file(tiny, 0).ok, "wrong-sized file refused before reading")
	check(not importer.import_file("user://ps2_save_import_missing", 0).ok, "missing file refused")
	# The play scene option.
	var scene = load("res://scenes/play_flow.tscn").instantiate()
	root.add_child(scene)
	await process_frame
	await process_frame
	var action: Button = scene.actions.get("ps2_import")
	check(action != null and action.text == "Import character from PS2 save…" and not action.disabled, "play scene offers the PS2 import")
	var shown: Dictionary = scene.import_ps2_save_file(ProjectSettings.globalize_path(path))
	check(shown.get("ok", false) and "Aria" in scene.ps2_status.text and "777 gold" in scene.ps2_status.text, "play scene shows the imported character")
	var denied: Dictionary = scene.import_ps2_save_file(ProjectSettings.globalize_path(tiny))
	check(not denied.ok and "PS2 import refused" in scene.status_label.text and "previous import kept" in scene.ps2_status.text, "play scene shows a refusal")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(tiny))
	scene.queue_free()
	await process_frame
	print("ps2 save import smoke: %d failure(s)" % failures)
	quit(0 if failures == 0 else 1)
