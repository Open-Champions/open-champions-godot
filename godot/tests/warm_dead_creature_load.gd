extends SceneTree

# Opt-in regression with a caller-supplied legacy version-2 native save whose
# Creature record has nonpositive HP. No private save bytes belong in the repo.
# --game-folder=ABS --save=ABS --out=ABS (ignored cache). Prints counts only.
var args := {}

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var parts := arg.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	call_deferred("run")

func word(bytes: PackedByteArray, at: int) -> int:
	if at < 0 or at + 4 > bytes.size(): return -1
	return int(bytes[at]) | int(bytes[at + 1]) << 8 | int(bytes[at + 2]) << 16 | int(bytes[at + 3]) << 24

func dead_records(bytes: PackedByteArray) -> int:
	# This reads only the authored native OCLS v1 directory. Field offset +4
	# is its raw current-HP word, not the original writer's HP fraction.
	if bytes.size() < 16 or bytes.slice(0, 8) != PackedByteArray([79, 67, 76, 83, 1, 0, 0, 0]): return -1
	var slots := word(bytes, 8)
	if slots < 1 or slots > 10: return -1
	var at := 12
	var dead := 0
	for slot in range(slots):
		var key_size := word(bytes, at)
		if key_size < 1 or key_size > 19 or at + 4 + key_size > bytes.size(): return -1
		at += 4 + key_size
		var count := word(bytes, at)
		if count < 0 or count > 65536: return -1
		at += 4
		for record in range(count):
			at += 4 # ordinal
			var class_size := word(bytes, at)
			if class_size < 1 or class_size > 255 or at + 4 + class_size + 16 > bytes.size(): return -1
			at += 4 + class_size + 12 + 4 # class, position, angle
			var fields_size := word(bytes, at)
			at += 4
			if fields_size < 16 or at + fields_size > bytes.size(): return -1
			if bytes.decode_float(at + 4) <= 0.0: dead += 1
			at += fields_size
	return dead

func run() -> void:
	var report := {"opened": false, "legacy_dead_records": 0,
		"warm_host_enabled": false, "missing_slot_refused": false,
		"native_dead_refused": false, "old_host_retained": false,
		"scene_load_refused": false, "candidate_failure_reported": false,
		"no_restored_claim": false}
	if not args.has("game-folder") or not args.has("save") or not args.has("out") or \
			not str(args["game-folder"]).is_absolute_path() or not str(args["save"]).is_absolute_path() or \
			not str(args["out"]).is_absolute_path():
		print("WARM_DEAD_CREATURE " + JSON.stringify(report))
		quit(2)
		return
	var save_path: String = str(args["save"])
	var manifest = JSON.parse_string(FileAccess.get_file_as_string(save_path + ".level"))
	var state := FileAccess.get_file_as_bytes(save_path + ".worldstate")
	if manifest is Dictionary and int(manifest.get("version", -1)) == 2 and manifest.get("scripted", false):
		report.legacy_dead_records = dead_records(state)
	if report.legacy_dead_records < 1:
		print("WARM_DEAD_CREATURE " + JSON.stringify(report))
		quit(1)
		return
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	DirAccess.make_dir_recursive_absolute(str(args["out"]))
	scene.real.cache_dir = str(args["out"]).path_join("cache")
	report.opened = scene.open_game_folder(str(args["game-folder"]))
	if report.opened:
		var index := int(manifest.get("level_index", -1))
		if index >= 0 and index < scene.real.levels.size():
			scene.level_script.set_pressed_no_signal(true)
			var warm_loaded: bool = scene.load_real_level(index, false)
			var stem: String = str(scene.real.levels[index].stem)
			report.warm_host_enabled = warm_loaded and scene.controller.get_play_capabilities().get("level_script_enabled", false)
			if report.warm_host_enabled:
				report.missing_slot_refused = not scene.controller.load_level_object_directory(state, "not-in-save").get("ok", true)
				var directory: Dictionary = scene.controller.load_level_object_directory(state, stem)
				var level: Dictionary = scene.real.levels[index]
				var script: PackedByteArray = scene.real.read_entry(level.gob, level.stem + ".lmp", "script.scr")
				var objects: PackedByteArray = scene.real.read_entry(level.gob, level.stem + ".lmp", "objects.ob")
				var rejected: Dictionary = scene.controller.enable_level_script(script, objects, false, {}, -1, stem) if directory.get("ok", false) else {}
				report.native_dead_refused = not rejected.get("ok", true) and "dead built-Creature" in str(rejected.get("error", ""))
				report.old_host_retained = scene.controller.get_play_capabilities().get("level_script_enabled", false)
				var loaded: bool = scene.load_session_file(save_path)
				report.scene_load_refused = not loaded
				report.candidate_failure_reported = scene.status_label.text == \
						"Level refused: level object construction or restoration failed."
				report.no_restored_claim = "restored" not in scene.status_label.text.to_lower() and \
						"loaded level, player" not in scene.status_label.text.to_lower()
	print("WARM_DEAD_CREATURE " + JSON.stringify(report))
	var passed: bool = report.opened and report.legacy_dead_records > 0 and report.warm_host_enabled and \
		report.missing_slot_refused and report.native_dead_refused and report.old_host_retained and \
		report.scene_load_refused and report.candidate_failure_reported and \
		report.no_restored_claim
	scene.queue_free()
	quit(0 if passed else 1)
