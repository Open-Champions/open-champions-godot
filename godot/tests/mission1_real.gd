extends SceneTree
# Opt-in counts-only acceptance on the owner's read-only disc. Candidate levels
# are discovered from the script header census, never a committed identifier.
# Explicit entry probes below do not establish physical interaction/reachability.
var args := {}
var failures := 0
var checks := 0
var results: Array = []
var scene: Control

func check(ok: bool, what: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAILED: " + what)

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var pair := arg.substr(2).split("=", true, 1)
			args[pair[0]] = pair[1]
	run.call_deferred()

func run() -> void:
	if not args.has("game-folder") or not args.has("out"):
		quit(2)
		return
	var out := str(args.out)
	DirAccess.make_dir_recursive_absolute(out)
	scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.real.cache_dir = out.path_join("cache")
	scene.components.library.history_path = out.path_join("library.json")
	check(scene.open_game_folder(str(args["game-folder"])), "read-only game folder opens")
	var candidates: Array[int] = []
	for i in range(scene.real.levels.size()):
		var level: Dictionary = scene.real.levels[i]
		var image: PackedByteArray = scene.real.read_entry(level.gob, level.stem + ".lmp", "script.scr")
		if image.size() < 0xa0: continue
		var publics := image.decode_u16(0x80)
		var natives := image.decode_u16(0x88)
		if (publics == 47 and natives == 21) or (publics == 36 and natives == 20): candidates.append(i)
	check(candidates.size() == 2, "two documented destination script censuses")
	for index in candidates:
		scene.level_script.set_pressed_no_signal(true)
		check(scene.load_real_level(index), "destination terrain loads")
		var initial: Dictionary = scene.controller.script_mission1_status()
		check(initial.get("ok", false) and initial.get("objects_validated", false), "addressed script and object links validate")
		if not initial.get("ok", false):
			var selected: Dictionary = scene.real.levels[index]
			var script: PackedByteArray = scene.real.read_entry(selected.gob, selected.stem + ".lmp", "script.scr")
			var objects: PackedByteArray = scene.real.read_entry(selected.gob, selected.stem + ".lmp", "objects.ob")
			var diagnostic: Dictionary = scene.controller.enable_level_script(script, objects)
			printerr("Candidate refusal: ", str(diagnostic.get("error", "unspecified")))
			continue
		var branch := int(initial.branch)
		check(initial.records == (283 if branch == 0 else 270), "record census")
		check(initial.factory_selected == (267 if branch == 0 else 254), "factory selection census")
		check(initial.hostile_placements == (69 if branch == 0 else 85), "placed hostile census")
		check(not initial.continuous_execution and not initial.objective_reachability and not initial.transitions, "Unknowns stay refused")
		var commands: Dictionary = scene.controller.script_command_counts()
		check(int(commands.faults) == 0, "no VM fault at bounded load")
		var row := {"branch": branch, "initial": initial, "actions": []}
		if branch == 0:
			check(initial.stage1_callbacks == 13 and initial.stage2_callbacks == 2 and initial.catapult_callbacks == 2, "addressed objective callback census")
			check(initial.predicates.size() == 4, "four stage predicates")
			for bridge in range(3):
				var request: Dictionary = scene.controller.script_mission1_action("bridge", bridge)
				check(request.get("refused", false), "bridge callee remains refused")
				row.actions.append(request)
		else:
			check(initial.switch_pairs == 8 and initial.predicates.size() == 8, "eight paired switches")
			var keys := [0x1d38, 0x1e08, 0x1ed8, 0x1fa8, 0x2078, 0x2148, 0x2218, 0x22e8]
			var actions := [0x1cac, 0x1d7c, 0x1e4c, 0x1f1c, 0x1fec, 0x20bc, 0x218c, 0x225c]
			for i in range(8):
				var key: Dictionary = scene.controller.script_mission1_action("entry", keys[i])
				check(key.get("ok", false) and int(key.value) == 1, "switch key evaluates addressed predicate")
				var action: Dictionary = scene.controller.script_mission1_action("entry", actions[i])
				check(action.get("refused", false) and not action.get("fault", true), "switch stops at untraced effect")
				var state: Dictionary = scene.controller.script_mission1_status()
				check(state.local_count == i + 1 and not state.predicates[i], "switch scalar effects survive refusal")
				row.actions.append(action)
			var gate: Dictionary = scene.controller.script_mission1_action("count_gate")
			check(gate.get("ok", false), "eight-switch addressed defer gate")
			var queued: Dictionary = scene.controller.script_mission1_status()
			check(queued.save_inhibit and int(queued.deferred_queued) > 0, "save inhibit and deferred door work recorded")
			var door: Dictionary = scene.controller.script_mission1_action("entry", 0xa3c)
			check(door.get("refused", false) and not door.get("fault", true), "door stops at cutscene")
			var after: Dictionary = scene.controller.script_mission1_status()
			check(not after.save_inhibit and not after.door_persistent, "no completion flag invented after refused cutscene")
			row["door"] = door
			# Campaign qvars survive a fresh host; unknown effect bodies cannot
			# invent a reconstructed local switch count from those persistent bits.
			check(scene.attach_level_script(true), "campaign revisit host attaches")
			var restored: Dictionary = scene.controller.script_mission1_status()
			check(restored.predicates.count(false) == 8, "all eight persistent keys restored")
			check(restored.local_count == initial.local_count, "untraced restoration effect does not fabricate a count")
			row["revisited"] = restored
		row["final"] = scene.controller.script_mission1_status()
		if DisplayServer.get_name() != "headless":
			scene.start_play()
			for tick in range(8):
				scene.advance_step()
				await process_frame
			# Same static-view capture path as the integration journey: force
			# a real draw instead of awaiting a signal that may never recur.
			RenderingServer.force_draw()
			var image := root.get_texture().get_image()
			check(image != null and not image.is_empty(), "rendered destination image available")
			if image != null and not image.is_empty():
				check(image.save_png(out.path_join("destination-%d.png" % branch)) == OK, "rendered capture saved")
		results.append(row)
	var summary := {"checks": checks, "failures": failures, "levels": results}
	var file := FileAccess.open(out.path_join("result.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify(summary, "\t") + "\n")
	print("Mission 1 levels ", results.size(), " checks ", checks, " failures ", failures)
	scene.queue_free()
	await process_frame
	quit(1 if failures else 0)
