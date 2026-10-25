extends SceneTree

# Opt-in private resource check with a native-authored synthetic save character.
# The folder is read only; no game bytes or identifiers are emitted.
var failures := 0

func check(ok: bool, label: String) -> void:
	if not ok:
		failures += 1
		push_error("FAIL " + label)
	else:
		print("PASS " + label)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var folder := ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="):
			folder = argument.trim_prefix("--game-folder=")
	if folder.is_empty():
		quit(2)
		return
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	root.add_child(scene)
	await process_frame
	await process_frame
	scene.components.art.hud_view.visible = false # The HUD is outside this actor-state probe.
	check(scene.open_game_folder(folder), "read-only game folder opened")
	var importer = ClassDB.instantiate("ChampionsPs2SaveImport")
	var image: PackedByteArray = ClassDB.class_call_static("ChampionsPs2SaveImport", "synthetic_slot", "Aria", 777)
	var character: Dictionary = importer.import_bytes(image, 0)
	check(character.get("ok", false), "authored character decoded")
	# The generic import fixture does not promise a class/model pair. Supply
	# the traced pair for this record-lifetime test (player-model-resources.md).
	var classes := [0, 0, 3, 3, 1, 1, 4, 4, 2, 2, 5, 6]
	character.class_index = classes[int(character.model_index)]
	character.acknowledged = character.level
	scene._show_ps2_import(character)
	var first: Dictionary = scene.controller.get_observation()
	check(scene.imported_appearance.size() == 4 and scene._imported_record_matches(first), "appearance bound to accepted record")
	var same: Dictionary = scene.controller.set_player_character(character)
	scene._observe(same)
	check(same.get("ok", false) and int(same.get("player_record_revision", -1)) > int(first.get("player_record_revision", -1)), "same-character replacement changes record revision")
	check(scene.imported_appearance.is_empty(), "replacement drops stale appearance")
	scene._show_ps2_import(character)
	check(scene.imported_appearance.size() == 4, "new import binds appearance again")
	scene.reset_session()
	check(scene.imported_appearance.is_empty(), "session reset drops appearance")
	scene._show_ps2_import(character)
	check(scene.imported_appearance.size() == 4, "appearance can be rebound after reset")
	check(scene.open_game_folder(folder) and scene.imported_appearance.is_empty(), "game folder switch drops appearance")
	scene.queue_free()
	print("PLAYER_APPEARANCE_LIFETIME failures=%d" % failures)
	quit(0 if failures == 0 else 1)
