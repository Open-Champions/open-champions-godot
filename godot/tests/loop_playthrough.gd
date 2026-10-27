extends SceneTree

# Swing cycles of the fight step (6 ticks of J held, 24 released). The fight
# runs until the target's hit points reach zero through the real damage path
# or this bound (about 90,000 ticks) ends it; nothing is forced.
const SWING_CYCLES := 3000
# Ticks run after the target dies so the death, XP award and loot roll settle.
const SETTLE_TICKS := 480 # Authored probe bound, including flight/settle ticks.

# Private, opt-in journey probe. The same script runs with --headless or a
# rendered display. Only counts and generic refusal text leave the game folder.
# --game-folder=ABS --out=ABS [--rendered]
const CROSS := 0x40
const CIRCLE := 0x20
const DOWN := 0x4000
const UP := 0x1000
const RIGHT := 0x2000
const LEFT := 0x8000
const START := 0x800
const RealWorld = preload("res://real_data/real_world.gd")
const MISSION_IMAGES := ["misswar.tex", "missinnovation.tex", "misspit.tex", "misswater.tex",
	"misstorment.tex", "missdisease.tex", "missvalor.tex", "missfire.tex",
	"missstorms.tex", "missfeydark.tex", "missnightmare.tex", "missfear.tex"]
const LoadingDisplay = preload("res://real_data/loading_display.gd")

var args := {}
var scene: Control
var fe: Control
var rows: Array[Dictionary] = []
var refusal_counts := {}
var seen := {}
var out_dir := ""
var screenshots := 0
var audit_menu_checks := true

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
		elif argument.begins_with("--"):
			args[argument.substr(2)] = true
	call_deferred("run")

func safe(value: Variant) -> String:
	var s := str(value)
	var path_re := RegEx.create_from_string("[^ ]*[/\\\\][^ ]*")
	s = path_re.sub(s, "<path>", true)
	var file_re := RegEx.create_from_string("[^ ]*\\.[A-Za-z0-9]{2,}[^ ]*")
	return file_re.sub(s, "<file>", true).left(180)

func refusal(stage: String, reason: Variant) -> void:
	var key := stage + ": " + safe(reason)
	refusal_counts[key] = int(refusal_counts.get(key, 0)) + 1
	print("PLAYTHROUGH_REFUSAL " + key)

func collect(stage: String) -> void:
	if scene == null: return
	var probes := {"scene": scene.status_label.text if scene.status_label != null else "",
		"frontend": fe.host_error if fe != null else ""}
	if scene.get("npc_dialog") != null:
		var d: Dictionary = scene.controller.dialog_status()
		if d.get("stalled", false): probes["dialog"] = d.get("stall_reason", "")
	if scene.get("fx") != null:
		for why in scene.fx.refusals: probes["fx " + safe(why)] = why
	if scene.get("audio") != null and "bank_errors" in scene.audio:
		for why in scene.audio.bank_errors: probes["audio " + safe(why)] = why
	for source in probes:
		var value := safe(probes[source])
		if value.is_empty() or not ("refus" in value.to_lower() or "unsupported" in value.to_lower() or source == "dialog"):
			continue
		var key := str(source) + " " + value
		if seen.get(key, false): continue
		seen[key] = true
		refusal(stage, key)

func mark(stage: String, result: String, extra := {}) -> void:
	if stage.begins_with("mission_menu_") and result == "refused": audit_menu_checks = false
	collect(stage)
	var row := {"step": stage, "result": result,
		"fps": Engine.get_frames_per_second(),
		"memory_bytes": int(Performance.get_monitor(Performance.MEMORY_STATIC)),
		"rendered": DisplayServer.get_name() != "headless"}
	for key in extra: row[key] = extra[key]
	rows.append(row)
	print("PLAYTHROUGH_STEP " + JSON.stringify(row))

func pickup_save_refused(controller, path: String) -> bool:
	var intact := true
	for method in ["save_file", "save_snapshot_file", "save_player_companion"]:
		var target := path + "." + str(method)
		var destination := target + ".player" if method == "save_player_companion" else target
		if FileAccess.file_exists(destination): DirAccess.remove_absolute(destination)
		var absent: Dictionary = controller.call(method, target)
		intact = intact and not absent.get("ok", false) and "inventory/equipment" in str(absent.get("error", "")) and not FileAccess.file_exists(destination)
		var sentinel := "authored save refusal sentinel".to_utf8_buffer()
		var file := FileAccess.open(destination, FileAccess.WRITE)
		if file == null: return false
		file.store_buffer(sentinel)
		file.close()
		var existing: Dictionary = controller.call(method, target)
		intact = intact and not existing.get("ok", false) and FileAccess.get_file_as_bytes(destination) == sentinel
		intact = intact and not FileAccess.file_exists(target + ".ocnsave.tmp") and not FileAccess.file_exists(target + ".player.tmp")
		DirAccess.remove_absolute(destination)
	return intact

func capture(label: String) -> void:
	if DisplayServer.get_name() == "headless": return
	# A static view may not emit another frame_post_draw; the viewport keeps
	# its last completed texture. Force this state to draw before reading it.
	for i in range(2): await process_frame
	RenderingServer.force_draw()
	var image := root.get_texture().get_image()
	if image == null or image.is_empty():
		if label.begins_with("10-mission-menu"): audit_menu_checks = false
		refusal(label, "rendered viewport has no image")
		return
	if image.save_png(out_dir.path_join(label + ".png")) == OK:
		screenshots += 1
	else:
		if label.begins_with("10-mission-menu"): audit_menu_checks = false
		refusal(label, "screenshot write failed")

func capture_player_view(label: String) -> void:
	if DisplayServer.get_name() == "headless": return
	for i in range(2): await process_frame
	RenderingServer.force_draw()
	var image: Image = scene.viewport.get_texture().get_image()
	if image == null or image.is_empty() or image.save_png(out_dir.path_join(label + ".png")) != OK:
		refusal(label, "player viewport capture failed")
	else:
		screenshots += 1

# Compare two rendered viewport captures with the dialog view shown and hidden.
# The box bounds come from docs/DIALOG.md; a nonzero count proves that its
# pixels, rather than only native draw calls or a decoded texture, reached play.
func dialog_box_pixels() -> int:
	RenderingServer.force_draw()
	var shown: Image = scene.viewport.get_texture().get_image()
	scene.npc_dialog.hide()
	await process_frame
	RenderingServer.force_draw()
	var hidden: Image = scene.viewport.get_texture().get_image()
	scene.npc_dialog.show()
	await process_frame
	if shown == null or hidden == null or shown.is_empty() or hidden.is_empty(): return 0
	var size: Vector2 = scene.npc_dialog.size
	var factor := minf(size.x / 640.0, size.y / 448.0)
	var origin := Vector2((size.x - 640.0 * factor) * 0.5, (size.y - 448.0 * factor) * 0.5)
	var box := Rect2i(Vector2i(origin + Vector2(35, 280) * factor), Vector2i(Vector2(560, 156) * factor))
	var bounds := box.intersection(Rect2i(Vector2i.ZERO, shown.get_size()))
	var changed := 0
	for y in range(bounds.position.y, bounds.end.y, 3):
		for x in range(bounds.position.x, bounds.end.x, 3):
			if shown.get_pixel(x, y) != hidden.get_pixel(x, y): changed += 1
	return changed

# The level screenshot is paired with an actor-hidden viewport capture. The
# count checks that the inspected mesh actually contributed visible pixels.
func player_actor_pixels() -> int:
	RenderingServer.force_draw()
	var shown: Image = scene.viewport.get_texture().get_image()
	scene.actor_visual.hide()
	await process_frame
	RenderingServer.force_draw()
	var hidden: Image = scene.viewport.get_texture().get_image()
	scene.actor_visual.show()
	await process_frame
	if shown == null or hidden == null or shown.is_empty() or hidden.is_empty(): return 0
	var changed := 0
	for y in range(0, shown.get_height(), 3):
		for x in range(0, shown.get_width(), 3):
			if shown.get_pixel(x, y) != hidden.get_pixel(x, y): changed += 1
	return changed

# Explicit diagnostic samples from the selected model's animation lump. The
# level session does not yet expose weapon class, so row 0 is a stated test
# input, never an inferred equipped class or automatic action transition.
func player_pose_pixels() -> Dictionary:
	if scene.real_player == null or not scene.real_player.body_drawn:
		return {"ok": false, "error": "class body unavailable"}
	var idle: Dictionary = scene.real_player.draw_selected_pose(0, 0, 1.0)
	if not idle.get("ok", false): return idle
	var idle_mesh: Mesh = scene.actor_visual.mesh
	scene.asset_label.text = "Explicit class-0 idle clip sample · diagnostic pose"
	await capture("03-player-idle")
	await capture_player_view("03-player-idle-view")
	var idle_image: Image
	if DisplayServer.get_name() != "headless":
		RenderingServer.force_draw()
		idle_image = scene.viewport.get_texture().get_image()
	var walk: Dictionary = scene.real_player.draw_selected_pose(0, 1, 1.0)
	if not walk.get("ok", false):
		scene.real_player.restore_rest_pose()
		scene.asset_label.text = scene.real_player.status
		return walk
	scene.asset_label.text = "Explicit class-0 walk clip sample · diagnostic pose"
	await capture("03-player-walk")
	await capture_player_view("03-player-walk-view")
	var walk_mesh: Mesh = scene.actor_visual.mesh
	var walk_image: Image
	if DisplayServer.get_name() != "headless":
		RenderingServer.force_draw()
		walk_image = scene.viewport.get_texture().get_image()
	var pose_corners := 0
	if idle_mesh.get_surface_count() == walk_mesh.get_surface_count():
		for surface in range(idle_mesh.get_surface_count()):
			var first: PackedVector3Array = idle_mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]
			var second: PackedVector3Array = walk_mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]
			if first.size() != second.size(): continue
			for index in range(first.size()):
				if first[index] != second[index]: pose_corners += 1
	var changed := 0
	if idle_image != null and walk_image != null and not idle_image.is_empty() \
			and idle_image.get_size() == walk_image.get_size():
		for y in range(0, idle_image.get_height(), 3):
			for x in range(0, idle_image.get_width(), 3):
				if idle_image.get_pixel(x, y) != walk_image.get_pixel(x, y): changed += 1
	scene.real_player.restore_rest_pose()
	scene.asset_label.text = scene.real_player.status
	return {"ok": pose_corners > 0 and (changed > 0 or DisplayServer.get_name() == "headless"),
		"idle_slot": int(idle.slot), "walk_slot": int(walk.slot),
		"pose_corners": pose_corners, "pose_pixels": changed, "sample_tick": 1.0}

func tap(bits: int) -> void:
	fe.tap(bits)
	collect("frontend")

func advance(command := 0, count := 1) -> void:
	for i in range(count):
		scene.held_commands = command
		var r: Dictionary = scene.advance_step()
		if not r.get("ok", false): refusal("tick", r.get("error", "native step refused"))
		collect("tick")
		scene.held_commands = 0

func inside_trigger(point: Vector3, target: Dictionary) -> bool:
	var lo: Vector3 = target.get("min", Vector3.ZERO)
	var hi: Vector3 = target.get("max", Vector3.ZERO)
	return point.x >= lo.x and point.x <= hi.x and point.y >= lo.y and point.y <= hi.y

func trigger_distance(point: Vector3, target: Dictionary) -> float:
	var lo: Vector3 = target.get("min", Vector3.ZERO)
	var hi: Vector3 = target.get("max", Vector3.ZERO)
	return maxf(maxf(lo.x - point.x, point.x - hi.x), 0.0) + \
		maxf(maxf(lo.y - point.y, point.y - hi.y), 0.0)

func walk_into_trigger(target: Dictionary, max_ticks: int) -> Dictionary:
	var began: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
	if inside_trigger(began, target): return {"entered": false, "ticks": 0}
	var lo: Vector3 = target.get("min", Vector3.ZERO)
	var hi: Vector3 = target.get("max", Vector3.ZERO)
	var moved := false
	var entered := false
	var ticks := 0
	var still := 0
	var without_progress := 0
	var best_distance := trigger_distance(began, target)
	while ticks < max_ticks and not scene.controller.script_level_request().get("queued", false):
		var before: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		# Aim for the nearest point in the documented inclusive rectangle.
		# The authored record's origin can be at a far corner of a wide Trigger.
		var dx := clampf(before.x, lo.x, hi.x) - before.x
		var dy := clampf(before.y, lo.y, hi.y) - before.y
		var command := 0
		if absf(dx) >= absf(dy) and not is_zero_approx(dx): command = 1 if dx > 0 else 2
		elif not is_zero_approx(dy): command = 4 if dy > 0 else 8
		elif not is_zero_approx(dx): command = 1 if dx > 0 else 2
		advance(command)
		ticks += 1
		var after: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		if Vector2(after.x - before.x, after.y - before.y).length() > 0.001: moved = true; still = 0
		else: still += 1
		if moved and not inside_trigger(before, target) and inside_trigger(after, target): entered = true
		var distance := trigger_distance(after, target)
		if distance < best_distance - 0.25:
			best_distance = distance
			without_progress = 0
		else: without_progress += 1
		if still >= 20 or without_progress >= 120 or scene.stopped: break
		if command == 0 and ticks % 15 == 0 and not scene.controller.script_level_request().get("queued", false): break
	return {"entered": entered, "ticks": ticks, "navigation_stalled": still >= 20 or without_progress >= 120}

func approach_use(target: Dictionary, max_ticks: int) -> Dictionary:
	var origin: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
	var goal: Vector3 = target.get("position", Vector3.ZERO)
	var best := origin.distance_to(goal)
	var stale := 0
	var still := 0
	var ticks := 0
	while ticks < max_ticks and not scene.stopped:
		var at: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		if absf(at.x - goal.x) <= 48.0 and absf(at.y - goal.y) <= 48.0 and goal.z >= at.z and goal.z <= at.z + 60.0:
			game_key(KEY_E)
			advance(0, 2)
			return {"pressed": true, "ticks": ticks}
		var command := 0
		if absf(at.x - goal.x) > 24.0: command = 1 if at.x < goal.x else 2
		elif absf(at.y - goal.y) > 24.0: command = 4 if at.y < goal.y else 8
		if command == 0: break
		advance(command)
		ticks += 1
		var after: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		if Vector2(after.x - at.x, after.y - at.y).length() <= 0.001: still += 1
		else: still = 0
		var distance: float = after.distance_to(goal)
		if distance < best - 0.25: best = distance; stale = 0
		else: stale += 1
		if stale >= 600 or still >= 20: break
	return {"pressed": false, "ticks": ticks, "navigation_stalled": stale >= 600 or still >= 20}

func help_close() -> void:
	for i in range(200):
		if not scene.controller.text_hud().get("help_open", false): return
		if scene.controller.help_frame(true).get("closed", false): return
	refusal("dialog", "help box did not close in 200 frames")

func move_key(code: int, pressed: bool) -> bool:
	var event := InputEventKey.new()
	event.keycode = code
	event.physical_keycode = code
	event.pressed = pressed
	return scene.handle_input_event(event)

# Exercise authored inputs through the campaign bridge, never the Full fixture
# helpers or equipment writes. The unarmed stand-in must refuse guard/heavy.
func campaign_moves() -> bool:
	var checks := {}
	var initial: Dictionary = scene.controller.get_observation()
	if not initial.get("fight_player_ready", false):
		mark("combat_moves", "refused", {"real_player": false})
		return false
	scene.release_controls()
	advance(0, 30)
	var edges := int(scene.raw_observation.fight_counts.attack_edges)
	checks.attack_key = move_key(KEY_J, true)
	for i in range(32): scene.advance_step()
	checks.held_once = int(scene.raw_observation.fight_counts.attack_edges) == edges + 1 \
		and int(scene.raw_observation.fight_player_action_state) == 1 \
		and int(scene.raw_observation.fight_player_combo_step) == 0
	var first_animation_starts := int(scene.raw_observation.fight_counts.player_animation_starts)
	move_key(KEY_J, false)
	scene.advance_step()
	checks.combo_key = move_key(KEY_J, true)
	for i in range(8): scene.advance_step()
	checks.combo = int(scene.raw_observation.fight_player_combo_step) == 1
	checks.combo_animation = int(scene.raw_observation.fight_counts.player_animation_starts) > first_animation_starts
	await capture("04-combat-combo")
	move_key(KEY_J, false)
	advance(0, 100)
	checks.guard_key = move_key(KEY_K, true)
	for i in range(3): scene.advance_step()
	checks.unarmed_guard_refused = "guard: unavailable" in str(scene.raw_observation.get("fight_move_refusal", "")) \
		and not bool(scene.raw_observation.fight_player_blocking) \
		and int(scene.raw_observation.fight_player_action_state) != 8
	checks.guard_move_key = move_key(KEY_U, true)
	scene.advance_step()
	checks.guard_move_refused = "guard_move: unavailable" in str(scene.raw_observation.get("fight_move_refusal", "")) \
		and int(scene.raw_observation.fight_player_action_state) != 11
	move_key(KEY_K, false)
	move_key(KEY_U, false)
	scene.advance_step()
	var mana: float = scene.raw_observation.fight_player_mana
	edges = int(scene.raw_observation.fight_counts.attack_edges)
	checks.heavy_key = move_key(KEY_L, true)
	scene.advance_step()
	checks.heavy_refused = "heavy_attack: refused_unknown" in str(scene.raw_observation.get("fight_move_refusal", "")) \
		and int(scene.raw_observation.fight_player_attack_kind) != 2 \
		and int(scene.raw_observation.fight_counts.attack_edges) == edges
	# Ordinary ready ticks can regenerate mana; a refusal cannot spend it.
	checks.heavy_mana_not_spent = float(scene.raw_observation.fight_player_mana) >= mana
	await capture("04-combat-refusal")
	move_key(KEY_L, false)
	scene.advance_step()
	for binding in [[KEY_1, "ability_slot_1"], [KEY_2, "ability_slot_2"]]:
		checks[binding[1] + "_key"] = move_key(binding[0], true)
		scene.advance_step()
		checks[binding[1] + "_refused"] = (binding[1] + ": refused_unknown") in str(scene.raw_observation.get("fight_move_refusal", "")) \
			and int(scene.raw_observation.fight_counts.attack_edges) == edges
		checks[binding[1] + "_mana_not_spent"] = float(scene.raw_observation.fight_player_mana) >= mana
		move_key(binding[0], false)
		scene.advance_step()
	# Pause with attack still held. Resuming and pressing again must create an
	# edge in the native mapper as well as clearing the host's command word.
	move_key(KEY_J, true)
	scene.advance_step()
	edges = int(scene.raw_observation.fight_counts.attack_edges)
	scene.set_paused(true)
	checks.pause_releases = scene.held_commands == 0
	scene.resume_session()
	move_key(KEY_J, true)
	scene.advance_step()
	checks.resume_edge = int(scene.raw_observation.fight_counts.attack_edges) == edges + 1
	move_key(KEY_J, false)
	advance(0, 100)
	var counts: Dictionary = scene.raw_observation.fight_counts
	checks.animation_hooks = int(counts.player_animation_starts) > 0 and int(counts.player_animation_advances) > 0
	checks.world_running = not bool(scene.raw_observation.get("fight_failed", true))
	var passed := 0
	for value in checks.values(): passed += int(value)
	mark("combat_moves", "passed" if passed == checks.size() else "refused", {
		"checks": checks.size(), "passed": passed, "observed": checks,
		"animation_starts": counts.player_animation_starts,
		"animation_advances": counts.player_animation_advances})
	return passed == checks.size()

func game_key(code: int, unicode := 0) -> bool:
	var event := InputEventKey.new()
	event.keycode = code
	event.physical_keycode = code
	event.unicode = unicode
	event.pressed = true
	var handled: bool = scene.handle_input_event(event)
	event.pressed = false
	scene.handle_input_event(event)
	return handled

func same_player_record(before: Dictionary, after: Dictionary) -> bool:
	if not before.get("player_ready", false) or not after.get("player_ready", false): return false
	for key in ["name", "player_class", "gold", "experience", "level", "acknowledged_level",
		"record_max_hp_raw", "record_max_mana_raw"]:
		if not before.has(key) or not after.has(key) or before[key] != after[key]: return false
	var currents_known: bool = before.get("player_record_currents_known", false)
	if currents_known != after.get("player_record_currents_known", false): return false
	if currents_known:
		for key in ["record_hp_raw", "record_mana_raw"]:
			if not before.has(key) or not after.has(key) or before[key] != after[key]: return false
	return true
func journal_visible() -> bool:
	return scene != null and scene.quest_journal != null and scene.quest_journal.visible

func save_json() -> void:
	var summary := {"steps": rows, "refusals": refusal_counts,
		"screenshots": screenshots, "renderer": DisplayServer.get_name()}
	var file := FileAccess.open(out_dir.path_join("result.json"), FileAccess.WRITE)
	if file != null: file.store_string(JSON.stringify(summary, "  ", true) + "\n")
	print("PLAYTHROUGH_RESULT " + JSON.stringify(summary))

func walk_exit_probe() -> bool:
	# Step 6: walk the native player into a real Trigger's inclusive x/y box.
	var transition_before: int = scene.real.current
	var exit_targets: Array = scene.controller.script_exit_targets()
	var pending: Dictionary = scene.controller.script_level_request()
	var walk_attempts := 0
	var walked_into := false
	var queued_by_walk := false
	var entered_without_change := 0
	var navigation_stalls := 0
	var sources_examined := 1
	var initial_source := transition_before
	var multi_trigger_indices: Array[int] = []
	var other_trigger_indices: Array[int] = []
	for index in range(scene.real.levels.size()):
		if index == initial_source: continue
		var level: Dictionary = scene.real.levels[index]
		var objects: PackedByteArray = scene.real.read_entry(level.gob, level.stem + ".lmp", "objects.ob")
		if objects.is_empty(): continue
		var inventory: Dictionary = scene.controller.inspect_exit_records(objects)
		if not inventory.get("ok", false): continue
		var trigger_count := int(inventory.get("triggers", 0))
		if trigger_count >= 4: multi_trigger_indices.append(index)
		elif trigger_count > 0: other_trigger_indices.append(index)
	var sources: Array[int] = [initial_source]
	sources.append_array(multi_trigger_indices)
	sources.append_array(other_trigger_indices)
	# Probe nearby rectangles in every source before trying its farther ones.
	# Both passes retain every eligible live Trigger without relying on a
	# private level index or a guessed walk-in coordinate.
	for pass_index in range(2):
		for index in sources:
			if index == initial_source and pass_index == 0:
				pass
			else:
				if not scene.load_real_level(index, true): continue
				scene.start_play()
				help_close()
				if scene.paused and scene.session_started: scene.resume_session()
				sources_examined += 1
				exit_targets = scene.controller.script_exit_targets()
			pending = scene.controller.script_level_request()
			if pending.get("queued", false): continue
			var walk_targets: Array = exit_targets.filter(func(t): return t.get("walk_in", false))
			if walk_targets.is_empty(): continue
			var origin: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
			walk_targets.sort_custom(func(a, b): return trigger_distance(origin, a) < trigger_distance(origin, b))
			var first: int = 0 if pass_index == 0 else mini(2, walk_targets.size())
			var last: int = mini(2, walk_targets.size()) if pass_index == 0 else walk_targets.size()
			for target in walk_targets.slice(first, last):
				walk_attempts += 1
				var dispatches_before := int(scene.controller.script_trigger_status().get("queued_dispatches", 0))
				var walk: Dictionary = walk_into_trigger(target, 5000)
				if walk.get("navigation_stalled", false): navigation_stalls += 1
				walked_into = bool(walk.get("entered", false))
				pending = scene.controller.script_level_request()
				var dispatch: Dictionary = scene.controller.script_trigger_status()
				queued_by_walk = walked_into and bool(pending.get("queued", false)) and \
					int(dispatch.get("queued_dispatches", 0)) > dispatches_before and \
					int(dispatch.get("queued_record", -1)) == int(target.get("index", -2)) and \
					str(dispatch.get("queued_destination", "")) == str(pending.get("name", ""))
				if walked_into and not pending.get("queued", false): entered_without_change += 1
				if pending.get("queued", false) or scene.stopped: break
			if queued_by_walk:
				transition_before = scene.real.current
				break
		if queued_by_walk: break
	var record_before: Dictionary = scene.controller.get_observation()
	var trigger_status: Dictionary = scene.controller.script_trigger_status()
	var quests_before: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var transition_ok: bool = queued_by_walk and bool(pending.get("queued", false)) and bool(await scene._apply_level_change())
	var record_after: Dictionary = scene.controller.get_observation()
	var start: Dictionary = scene.controller.get_terrain().get("start", {})
	var arrived_position: Vector3 = record_after.get("position", Vector3.ZERO)
	var marker_position: Vector3 = start.get("position", Vector3.ZERO)
	var arrival_at_marker: bool = transition_ok and start.has("position") and \
		arrived_position.distance_to(marker_position) < 0.01
	var quests_after: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var record_carried: bool = transition_ok and scene.last_campaign_carried and scene.last_campaign_image_restored \
		and str(record_after.get("name", "")) == str(record_before.get("name", "")) \
		and int(record_after.get("gold", -1)) == int(record_before.get("gold", -2)) \
		and int(record_after.get("experience", -1)) == int(record_before.get("experience", -2)) \
		and int(record_after.get("level", -1)) == int(record_before.get("level", -2))
	mark("walk_in_exit_trigger", "changed" if record_carried and arrival_at_marker and scene.real.current != transition_before else "refused",
		{"level_index_before": transition_before, "level_index_after": scene.real.current,
		"exit_targets": exit_targets.size(), "walk_attempts": walk_attempts, "walked_into": walked_into,
		"queued_by_walk": queued_by_walk,
		"entered_without_change": entered_without_change,
		"navigation_stalls": navigation_stalls,
		"trigger_samples": int(trigger_status.get("samples", 0)),
		"trigger_dispatches": int(trigger_status.get("dispatches", 0)),
		"trigger_queued_dispatches": int(trigger_status.get("queued_dispatches", 0)),
		"trigger_missing": int(trigger_status.get("missing", 0)),
		"arrival_at_marker": arrival_at_marker,
		"arrival_pre_search": bool(start.get("pre_search", false)),
		"arrival_rank": int(start.get("rank", 0)),
		"sources_examined": sources_examined,
		"pending_script_level": pending.get("queued", false), "campaign_carried": scene.last_campaign_carried,
		"campaign_image_restored": scene.last_campaign_image_restored,
		"record_carried": record_carried, "quest_vars_before": int(quests_before.get("vars", 0)),
		"quest_vars_after": int(quests_after.get("vars", 0))})
	await capture("11-walk-exit-level-change")
	return record_carried and arrival_at_marker and scene.real.current != transition_before

func load_scripted_exit_source(indices: Array[int], kind: String) -> bool:
	for index in indices:
		if not scene.load_real_level(index, true): continue
		scene.start_play()
		help_close()
		if scene.paused and scene.session_started: scene.resume_session()
		if not scene.controller.get_play_capabilities().get("level_script_enabled", false): continue
		for target in scene.controller.script_exit_targets():
			if str(target.get("kind", "")) == kind: return true
	return false

func run() -> void:
	if not args.has("game-folder") or not args.has("out"):
		printerr("--game-folder and --out are required")
		quit(2)
		return
	out_dir = str(args["out"])
	if not out_dir.is_absolute_path() or not str(args["game-folder"]).is_absolute_path():
		printerr("paths must be absolute")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(out_dir)
	root.size = Vector2i(1120, 780)
	root.content_scale_size = root.size
	scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = out_dir.path_join("library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = out_dir.path_join("cache")
	fe = scene.frontend
	fe.options_path = out_dir.path_join("options.img")
	# A repeat run must exercise the blank memory-card branch again.
	if FileAccess.file_exists(fe.options_path): DirAccess.remove_absolute(fe.options_path)
	var opened: bool = scene.open_game_folder(str(args["game-folder"]))
	mark("game_folder", "opened" if opened else "refused", {"level_candidates": scene.real.levels.size()})
	if not opened:
		save_json()
		quit(1)
		return
	scene.level_script.set_pressed_no_signal(true)
	var boot: Dictionary = fe.boot()
	mark("boot_card", fe.screen() if boot.get("ok", false) else "refused")
	await capture("01-boot")
	if fe.screen() == "boot-card": tap(CROSS)
	mark("logo", fe.screen())
	for i in range(239): fe.step(0)
	mark("intro", fe.screen())
	if fe.screen() == "intro-movie":
		for i in range(30): fe.step(0)
		tap(CROSS)
	mark("main_menu", fe.screen())
	await capture("02-main")
	var name_q_owned := false
	if fe.screen() == "main":
		for i in range(4): tap(CROSS)
		if fe.screen() == "difficulty": tap(CROSS)
		mark("character_class", fe.screen())
		if fe.screen() == "char-class":
			# The assisted Valor capture used the model-11/class-6 character
			# (audited carousel position 1); the capture's equipped club is
			# still a separate item-state question.
			tap(DOWN)
			tap(CROSS)
		if fe.screen() == "char-appearance": tap(CROSS)
		if fe.screen() == "char-ability":
			for i in range(20): tap(RIGHT)
			tap(CROSS)
		if fe.screen() == "char-name":
			var name_q_handled := game_key(KEY_Q, "q".unicode_at(0))
			name_q_owned = name_q_handled and fe.name_buffer == "q" and not journal_visible()
			mark("character_name_q", "typed" if name_q_owned else "refused",
				{"key_handled": name_q_handled, "name_length": fe.name_buffer.length(),
				"journal_visible": journal_visible()})
			fe.type_name("Probe")
			tap(DOWN)
			tap(CROSS)
	var created_player: Dictionary = scene.controller.get_observation()
	var expected_character := int(created_player.get("player_class", -1)) == 6 \
		and int(created_player.get("player_model", -1)) == 11
	mark("character_created", fe.screen() if expected_character else "refused",
		{"player_ready": created_player.get("player_ready", false),
		"player_class": created_player.get("player_class", -1),
		"player_model": created_player.get("player_model", -1),
		"frontend_screen": fe.screen()})
	if not expected_character:
		refusal("character_created", "character creation expected class 6 and model 11")
		save_json()
		quit(1)
		return
	var pause_q_owned := false
	var game_menu_q_owned := false
	if fe.screen() == "playing":
		tap(START)
		var pause_q_handled := game_key(KEY_Q)
		pause_q_owned = fe.screen() == "pause" and pause_q_handled and not journal_visible()
		tap(0x20)
		pause_q_owned = pause_q_owned and fe.screen() == "playing" and not scene.paused
		mark("pause_menu_q", "owned" if pause_q_owned else "refused",
			{"key_handled": pause_q_handled, "journal_visible": journal_visible()})
		var game_menu_opened: bool = game_key(KEY_M) and scene.game_menus.open
		var game_menu_q_handled := game_key(KEY_Q)
		game_menu_q_owned = game_menu_opened and game_menu_q_handled and not journal_visible()
		game_key(KEY_ESCAPE)
		game_menu_q_owned = game_menu_q_owned and not scene.game_menus.open
		mark("game_menu_q", "owned" if game_menu_q_owned else "refused",
			{"key_handled": game_menu_q_handled, "journal_visible": journal_visible()})
	mark("first_level", "entered" if scene.real.current >= 0 and scene.play_mode else "refused",
		{"level_index": scene.real.current, "session_started": scene.session_started,
		"script_enabled": scene.controller.get_play_capabilities().get("level_script_enabled", false)})
	await capture("03-level")
	var class_mesh: bool = scene.real_player != null and scene.real_player.body_drawn \
		and scene.real_player.model_index == int(scene.controller.get_observation().get("player_model_index", -1)) \
		and scene.actor_visual.visible and scene.actor_visual.mesh is ArrayMesh \
		and scene.actor_visual.mesh != null and scene.actor_visual.mesh != scene._default_mesh \
		and not (scene.actor_visual.mesh is CapsuleMesh) and not (scene.actor_visual.mesh is SphereMesh) \
		and not scene.asset_label.text.begins_with("Practice figure")
	var actor_pixels := await player_actor_pixels() if DisplayServer.get_name() != "headless" else 0
	mark("player_visual", "not_applicable_headless" if DisplayServer.get_name() == "headless" else \
		("class_mesh" if class_mesh and actor_pixels > 0 else "refused"),
		{"class_mesh": class_mesh, "mesh_type": scene.actor_visual.mesh.get_class(), "actor_pixels": actor_pixels,
		"practice_label": scene.asset_label.text.begins_with("Practice figure"),
		"body_drawn": scene.real_player.body_drawn, "model_match": scene.real_player.model_index == int(scene.controller.get_observation().get("player_model_index", -1)),
		"actor_visible": scene.actor_visual.visible})
	var pose: Dictionary = await player_pose_pixels()
	mark("player_pose_sample", "sampled" if pose.get("ok", false) else "refused", pose)
	if args.has("visual-only"):
		save_json()
		quit(0 if class_mesh and pose.get("ok", false) and \
			(DisplayServer.get_name() == "headless" or actor_pixels > 0) else 1)
		return
	if scene.real.current < 0 and not scene.real.levels.is_empty():
		var direct: bool = scene.load_real_level(0)
		mark("direct_level_diagnostic", "loaded" if direct else "refused", {"level_index": scene.real.current})
		if direct and not scene.play_mode: scene.start_play()
	if scene.real.current >= 0 and not scene.controller.get_play_capabilities().get("level_script_enabled", false):
		mark("level_script", "enabled" if scene.attach_level_script() else "refused")
	else:
		mark("level_script", "enabled" if scene.controller.get_play_capabilities().get("level_script_enabled", false) else "unavailable")
	help_close()
	if scene.paused and scene.session_started: scene.resume_session()
	var portal_before_offer: Dictionary = scene.controller.script_run_exit(12) if scene.real.current >= 0 and str(scene.real.levels[scene.real.current].stem) == "hub" else {"ok": false}
	var portal_guarded: bool = portal_before_offer.get("ok", false) and int(portal_before_offer.get("portal_requests", -1)) == 0 \
		and not scene.controller.script_mission_menu_state().get("first_story_available", true)
	mark("hub_portal_locked", "guarded" if portal_guarded else "refused",
		{"requests_before_offer": int(portal_before_offer.get("portal_requests", -1))})
	var dialog: Dictionary = scene.controller.dialog_status()
	var hub_campaign_before: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var hub_player_before: Dictionary = scene.controller.get_observation()
	var opening_quests_before: Dictionary = scene.controller.quest_journal()
	var talkers := int(dialog.get("talkers", 0))
	var walk_start: Vector3 = scene.controller.get_presentation().get("current_position", Vector3.ZERO)
	var opened_dialog := false
	var physical_attempts := 0
	if dialog.get("ok", false) and talkers > 0 and scene.play_mode:
		# One ordinary E press establishes the explicit Player_interact
		# refusal; further navigation cannot supply its missing runtime state.
		game_key(KEY_E)
		advance(0, 2)
		help_close()
		physical_attempts = 1
		opened_dialog = scene.controller.dialog_status().get("open", false)
	var physical_dialog_opened := opened_dialog
	var direct_dialog_opened := false
	if not opened_dialog and talkers == 1 and dialog.get("ok", false):
		# Keep the separate route diagnostic running after the Player_interact
		# refusal. This explicit talker index is never counted as physical use.
		var direct_talk: Dictionary = scene.controller.dialog_talk_to(0)
		if direct_talk.get("ok", false):
			advance(0, 2)
			direct_dialog_opened = scene.controller.dialog_status().get("open", false)
			opened_dialog = direct_dialog_opened
	var walk_end: Vector3 = scene.controller.get_presentation().get("current_position", Vector3.ZERO)
	mark("walk_and_talk", "dialog_open" if physical_dialog_opened else \
		("physical_refused_direct_diagnostic" if direct_dialog_opened else "refused"),
		{"talkers": talkers, "physical_use_attempts": physical_attempts,
		"physical_dialog_opened": physical_dialog_opened, "direct_diagnostic_opened": direct_dialog_opened,
		"walk_displacement": walk_start.distance_to(walk_end),
		"nearest_target_known": false, "paused": scene.paused,
		"menu_visible": scene.menu.visible})
	var dialog_choice_completed := false
	if opened_dialog:
		# Opening the dialog starts a camera swoop. Its draw task returns no
		# operations until that move has arrived; sample only a drawable frame.
		for i in range(4000):
			advance()
			await process_frame
			if not scene.controller.dialog_status().get("camera_moving", false) and \
					 not scene.npc_dialog.frame.get("sprites", []).is_empty() and \
					 scene.npc_dialog.frame.get("glyphs", []).size() > 40: break
		var frame: Dictionary = scene.npc_dialog.frame
		for why in frame.get("refusals", []): refusal("dialog_draw", why)
		await capture("04-dialog-attempt")
		var box_pixels := await dialog_box_pixels() if DisplayServer.get_name() != "headless" else 0
		var visual_result := "not_applicable_headless" if DisplayServer.get_name() == "headless" else \
			("drawn" if frame.get("open", false) and not frame.get("sprites", []).is_empty() and \
			 not frame.get("glyphs", []).is_empty() and box_pixels > 0 else "refused")
		mark("dialog_visual", visual_result,
			{"box_sprites": frame.get("sprites", []).size(),
			"glyphs": frame.get("glyphs", []).size(),
			"box_pixels": box_pixels,
			"camera_moving": scene.controller.dialog_status().get("camera_moving", false),
			"frame_open": frame.get("open", false),
			"paused": scene.paused, "menu_visible": scene.menu.visible,
			"font_ready": scene.npc_dialog.font_texture != null,
			"box_ready": scene.npc_dialog.box_texture != null})
		# The later prompt is absent during the opening text reveal. Capture it
		# before sending a choice so its text and cursor can be inspected too.
		var prompt_drawn := false
		for i in range(4000):
			advance()
			await process_frame
			if int(scene.npc_dialog.frame.get("cursor_glyph_quads", 0)) > 0:
				prompt_drawn = true
				break
		mark("dialog_prompt", "drawn" if prompt_drawn else "unavailable",
			{"cursor_glyph_quads": int(scene.npc_dialog.frame.get("cursor_glyph_quads", 0))})
		if prompt_drawn: await capture("04-dialog-prompt")
	var point_menu_refused := false
	if opened_dialog:
		for target in scene.controller.hub_service_targets():
			if str(target.kind) == "Savepoint":
				var blocked: Dictionary = scene.controller.hub_service_use(int(target.index), false)
				point_menu_refused = not blocked.get("ok", true) and not blocked.get("request_save", true)
		game_key(KEY_DOWN)
		var closed := false
		for i in range(1200):
			await process_frame
			help_close()
			if scene.paused and not scene.stopped: scene.resume_session()
			if i % 8 == 0: game_key(KEY_ENTER)
			advance()
			if not scene.controller.dialog_status().get("open", false):
				closed = true
				break
		var dialog_counts: Dictionary = scene.controller.dialog_status().get("counts", {})
		for why in dialog_counts.get("refused_reasons", {}):
			refusal("dialog", "%s count=%d" % [safe(why), int(dialog_counts.refused_reasons[why])])
		dialog_choice_completed = closed and int(dialog_counts.get("choices_taken", 0)) > 0
		mark("dialog_choice_close", "closed" if dialog_choice_completed else "refused",
			{"closed": closed, "choices_taken": int(dialog_counts.get("choices_taken", 0)),
			"pending_levels": int(dialog_counts.get("pending_levels", 0))})
	else:
		mark("dialog_choice_close", "blocked_by_walk_and_talk")
	var opening_quests_after: Dictionary = scene.controller.quest_journal()
	var opening_quest_set: bool = dialog_choice_completed and opening_quests_before.get("ok", false) \
		and opening_quests_after.get("ok", false) and int(opening_quests_before.get("count", -1)) == 0 \
		and int(opening_quests_after.get("count", -1)) == 1
	mark("opening_choice_quest", "set" if opening_quest_set else "refused",
		{"before_count": opening_quests_before.get("count", -1),
		"after_count": opening_quests_after.get("count", -1)})
	var journal_verified := false
	if scene.quest_journal != null:
		var journal_key_handled := game_key(KEY_Q)
		for i in range(3): await process_frame
		var text_submissions := 0
		var glyph_submissions := 0
		var sprite_submissions := 0
		for item in scene.quest_journal.items:
			if item.get("kind", "") == "text":
				text_submissions += 1
				glyph_submissions += item.get("glyphs", []).size()
			elif item.get("kind", "") == "sprite":
				sprite_submissions += 1
		journal_verified = journal_key_handled and scene.quest_journal.visible and scene.quest_journal.art_ready \
			and int(scene.quest_journal.entry_count) == 1 and scene.quest_journal.layout_refused \
			and scene.quest_journal.refusal.begins_with("journal layout refused:") \
			and text_submissions == 0 and glyph_submissions == 0 and sprite_submissions == 0
		mark("journal_after_dialog", "layout_refused" if journal_verified else "refused",
			{"key_handled": journal_key_handled, "art_ready": scene.quest_journal.art_ready,
			"entry_count": scene.quest_journal.entry_count, "layout_refused": scene.quest_journal.layout_refused,
			"text_submissions": text_submissions, "glyph_submissions": glyph_submissions,
			"sprite_submissions": sprite_submissions, "refusal": scene.quest_journal.refusal})
		await capture("05-quest-journal")
		game_key(KEY_Q)
	else:
		mark("journal_after_dialog", "refused", {"reason": "journal control missing"})
	help_close()
	if fe.screen() == "pause": tap(START)
	if scene.paused and scene.session_started and not scene.stopped: scene.resume_session()
	var hub_campaign_after: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var hub_player_after: Dictionary = scene.controller.get_observation()
	var hub_quest_changed := int(hub_campaign_after.get("quest_lines", -1)) == int(hub_campaign_before.get("quest_lines", -1)) + 1 \
		and int(hub_campaign_after.get("mission_available_bits", -1)) == int(hub_campaign_before.get("mission_available_bits", -1)) + 1
	var hub_no_debit: bool = hub_player_after.get("gold", -1) == hub_player_before.get("gold", -2) \
		and hub_player_after.get("experience", -1) == hub_player_before.get("experience", -2)
	mark("hub_quest_service", "applied" if hub_quest_changed and hub_no_debit else "refused",
		{"quest_lines_added": int(hub_campaign_after.get("quest_lines", -1)) - int(hub_campaign_before.get("quest_lines", -1)),
		"mission_bits_added": int(hub_campaign_after.get("mission_available_bits", -1)) - int(hub_campaign_before.get("mission_available_bits", -1)),
		"gold_xp_unchanged": hub_no_debit})
	var point_index := -1
	var point_choice := -1
	var service_refusals := true
	scene.hub_services.refresh()
	for index in range(scene.hub_services.targets.size()):
		var target: Dictionary = scene.hub_services.targets[index]
		if str(target.kind) == "Savepoint":
			point_index = int(target.index)
			point_choice = index
		else:
			var refused: Dictionary = scene.controller.hub_service_use(int(target.index), false)
			service_refusals = service_refusals and not refused.get("ok", true) and not refused.get("request_save", true) \
				and not str(refused.get("error", "")).is_empty()
	var point_before: Dictionary = scene.controller.get_observation()
	var point_busy: Dictionary = scene.controller.hub_service_use(point_index, true)
	var point_bad: Dictionary = scene.controller.hub_service_use(-1, false)
	var point_result := {"ok": false}
	var point_saved := false
	var hub_controls_visible := true
	var point_path := out_dir.path_join("hub-savepoint.ocplay")
	if point_choice >= 0:
		scene.hub_services.choice.select(point_choice)
		if args.has("hub-only"):
			scene.set_paused(true)
			await process_frame
			scene.scene_scroll.ensure_control_visible(scene.hub_services.row)
			await process_frame
			var visible_bounds := Rect2(Vector2.ZERO, Vector2(root.size))
			hub_controls_visible = visible_bounds.encloses(scene.hub_services.choice.get_global_rect()) \
				and visible_bounds.encloses(scene.actions.hub_service.get_global_rect())
			mark("hub_service_controls", "drawn" if hub_controls_visible else "refused",
				{"selector_and_action_inside_viewport": hub_controls_visible})
			await capture("04-hub-services")
			scene.set_paused(false)
		fe.queue_path("save_session", point_path)
		point_result = scene.hub_services.use_selected()
		await capture("04-savepoint-save-screen")
		if fe.screen() == "save-game": tap(CROSS)
		point_saved = fe.screen() == "saved" and FileAccess.file_exists(point_path)
		if fe.screen() == "saved": tap(CROSS)
		# A refused save stays on its screen. Close that UI through the existing
		# cancel input; point_saved remains false and all refusal checks remain.
		elif fe.screen() == "save-game": tap(CIRCLE)
	var point_after: Dictionary = scene.controller.get_observation()
	var point_unchanged := true
	for key in ["gold", "experience", "level", "record_max_hp_raw", "record_max_mana_raw", "player_record_revision"]:
		point_unchanged = point_unchanged and point_before.has(key) and point_before.get(key) == point_after.get(key)
	for key in ["record_hp_raw", "record_mana_raw"]:
		point_unchanged = point_unchanged and point_before.has(key) == point_after.has(key) and point_before.get(key) == point_after.get(key)
	# A load restores level, player, quests and position but not in-flight script
	# state, so the reload is only exercised in the focused hub run. The full
	# journey keeps the hub script live for the mission menu and checks the
	# same continuation at load_and_carry and cold_load below.
	var point_readback_live: bool = args.has("hub-only")
	var point_loaded: bool = point_saved and (not point_readback_live or scene.load_session_file(point_path))
	var point_readback: Dictionary = scene.controller.get_observation() if point_readback_live else point_before
	var point_campaign: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var point_carried := point_loaded
	for key in ["gold", "experience", "level", "record_max_hp_raw", "record_max_mana_raw", "player_class", "player_model_index"]:
		point_carried = point_carried and point_before.has(key) and point_before.get(key) == point_readback.get(key)
	for key in ["record_hp_raw", "record_mana_raw"]:
		point_carried = point_carried and point_before.has(key) == point_readback.has(key) and point_before.get(key) == point_readback.get(key)
	for key in ["quest_lines", "mission_available_bits", "mission_complete_bits", "vars", "vars_nonzero"]:
		point_carried = point_carried and hub_campaign_after.has(key) and hub_campaign_after.get(key) == point_campaign.get(key)
	mark("hub_savepoint", "saved" if point_saved and point_unchanged and point_carried and service_refusals else "refused",
		{"accepted": point_result.get("ok", false), "busy_refused": not point_busy.get("ok", true),
		"bad_index_refused": not point_bad.get("ok", true), "mutex_refused": point_menu_refused, "player_unchanged": point_unchanged,
		"native_save_written": point_saved, "original_card_persistence": false,
		"save_readback_matches": point_carried, "readback_in_hub": point_readback_live,
		"other_services_refused": service_refusals})
	if not point_saved or not point_unchanged: refusal("hub_savepoint", point_result.get("error", "savepoint flow incomplete"))
	if not point_carried: refusal("hub_savepoint", "native continuation readback differs")
	if not service_refusals: refusal("hub_services", "an unsupported direct service did not refuse with a diagnostic")
	if args.has("hub-only"):
		var alternate_index := -1
		for index in range(scene.real.levels.size()):
			if str(scene.real.levels[index].stem) == "hub2": alternate_index = index
		var alternate_loaded: bool = alternate_index >= 0 and scene.load_real_level(alternate_index, true)
		var alternate_refused := false
		var alternate_point := false
		if alternate_loaded:
			scene.start_play()
			help_close()
			var alternate_use: Dictionary = scene.controller.dialog_use()
			var alternate_talk: Dictionary = scene.controller.dialog_talk_to(0)
			alternate_refused = not alternate_use.get("ok", true) and not alternate_talk.get("ok", true)
			for target in scene.controller.hub_service_targets():
				if str(target.kind) == "PushTrigger":
					var alternate_action: Dictionary = scene.controller.script_run_exit(int(target.index))
					alternate_refused = alternate_refused and not alternate_action.get("ok", true)
				elif str(target.kind) == "Savepoint":
					var alternate_save: Dictionary = scene.controller.hub_service_use(int(target.index), false)
					alternate_point = alternate_save.get("ok", false) and alternate_save.get("request_save", false)
		mark("alternate_hub_services", "bounded" if alternate_loaded and alternate_refused and alternate_point else "refused",
			{"loaded": alternate_loaded, "unknown_routes_refused": alternate_refused, "direct_savepoint_accepted": alternate_point})
		save_json()
		scene.queue_free()
		await process_frame
		quit(0 if hub_controls_visible and hub_quest_changed and hub_no_debit and service_refusals and point_carried and point_saved and point_unchanged and point_menu_refused and not point_busy.get("ok", true) and not point_bad.get("ok", true) and alternate_loaded and alternate_refused and alternate_point else 1)
		return
	help_close()
	if fe.screen() == "pause": tap(START)
	if scene.paused and scene.session_started and not scene.stopped: scene.resume_session()
	# The opening choice and this hub PushTrigger are checked separately in
	# research-story-route. Exercise their native integration in this session.
	var transition_before: int = scene.real.current
	var at_hub: bool = transition_before >= 0 and str(scene.real.levels[transition_before].stem) == "hub"
	var menu_before: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_1_unlock", "available" if at_hub and menu_before.get("first_story_available", false) else "refused",
		{"at_hub": at_hub, "available": menu_before.get("first_story_available", false)})
	# The separate rectangle Trigger requests trigger(portal, 0) once after
	# mission 1 is offered. The target method and rendering are unverified.
	var portal_trigger_found := false
	for target in scene.controller.script_exit_targets():
		if int(target.index) == 12 and str(target.kind) == "Trigger" and target.get("script_action", false):
			portal_trigger_found = true
			break
	var portal_result: Dictionary = scene.controller.script_run_exit(12) if at_hub and dialog_choice_completed and menu_before.get("first_story_available", false) and portal_trigger_found else {"ok": false}
	var portal_stop: Dictionary = scene.controller.script_command_counts()
	mark("hub_portal_script_stop", "refused_script_stopped" if portal_stop.get("script_stopped", false) else "not_stopped",
		{"stopped": portal_stop.get("script_stopped", false), "refusal": portal_stop.get("last_refusal", ""),
		"trigger_error": portal_result.get("error", ""), "refusal_stops": portal_stop.get("refusal_stops", 0)})
	var portal_repeat: Dictionary = scene.controller.script_run_exit(12) if portal_result.get("ok", false) else {"ok": false}
	var portal_request_ok: bool = portal_trigger_found and portal_result.get("ok", false) and int(portal_result.get("portal_requests", -1)) == 1 \
		and portal_repeat.get("ok", false) and int(portal_repeat.get("portal_requests", -1)) == 0
	mark("hub_portal_request", "requested" if portal_request_ok else "refused",
		{"trigger_found": portal_trigger_found, "first_requests": int(portal_result.get("portal_requests", -1)),
		"repeat_requests": int(portal_repeat.get("portal_requests", -1)), "rendering_verified": false})
	var hub_trigger_found := false
	for target in scene.controller.script_exit_targets():
		if int(target.index) == 11 and str(target.kind) == "PushTrigger" and target.get("script_action", false):
			hub_trigger_found = true
			break
	var menu_was_closed: bool = menu_before.get("ok", false) and not menu_before.get("open", false)
	var trigger_result: Dictionary = scene.controller.script_run_exit(11) if at_hub and hub_trigger_found and dialog_choice_completed and menu_was_closed and not scene.controller.dialog_status().get("open", false) else {"ok": false}
	var menu_after: Dictionary = scene.controller.script_mission_menu_state()
	var menu_open: bool = bool(menu_after.get("ok", false) and menu_after.get("open", false))
	var trigger_opened_menu: bool = hub_trigger_found and menu_was_closed and trigger_result.get("ok", false) and menu_open
	var trigger_counts: Dictionary = scene.controller.script_command_counts()
	scene._refresh_mission_menu()
	var opening_before: Dictionary = scene.controller.script_mission_menu_state()
	var early_key := game_key(KEY_ENTER)
	var opening_after: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_menu_opening_gate", "checked" if early_key and int(opening_after.get("state", -1)) == int(opening_before.get("state", -2)) and not opening_after.get("pending", "") else "refused")
	await capture("10-mission-menu-opening-gate")
	mark("hub_trigger_mission_menu", "opened" if trigger_opened_menu else "refused",
		{"hub_trigger_found": hub_trigger_found, "dialog_choice_completed": dialog_choice_completed,
		"menu_was_closed": menu_was_closed, "trigger_action_ok": trigger_result.get("ok", false),
		"trigger_error": str(trigger_result.get("error", "")),
		"script_fault": str(trigger_result.get("script_fault", "")),
		"script_refusal_stops": int(trigger_counts.get("refusal_stops", 0)),
		"script_faults": int(trigger_counts.get("faults", 0)),
		"script_executed_commands": trigger_counts.get("executed", {}),
		"script_refused_commands": trigger_counts.get("refused", {}), "menu_open": menu_open,
		"host_panel_visible": scene.mission_menu_panel.visible})
	for i in range(18): await process_frame
	var mission_art = scene.mission_menu_panel
	var table_state: Dictionary = scene.controller.script_mission_menu_state()
	var table_rows: Array = table_state.get("rows", [])
	var matched_rows := 0
	if table_rows.size() == 12:
		for i in range(12):
			var row: Dictionary = table_rows[i]
			if str(row.get("image", "")).to_lower() == MISSION_IMAGES[i] and int(row.get("x", -1)) == (140 if i % 2 == 0 else 70) and int(row.get("y", -1)) == 40 + 200 * i:
				matched_rows += 1
	mark("mission_menu_table", "checked" if matched_rows == 12 else "refused", {"matched_rows": matched_rows})
	mark("mission_menu_assets", "checked" if mission_art.assets_valid and mission_art.art.size() == 32 else "refused",
		{"textures": mission_art.art.size(), "small_font": mission_art.font_texture != null})
	await capture("10-mission-menu")
	var unsupported := false
	for name in ["cosmiclines.tex", "starfield.tex", "starsparkle.tex", "mainglow.tex", "amuletglow.tex", "bonusglow.tex"]:
		if mission_art.submitted_sprites.has(name): unsupported = true
	mark("mission_menu_unsupported_art", "checked" if not unsupported and mission_art.submitted_text_keys == ["AcceptBack"] else "refused")
	var missing_row: Dictionary = table_state.duplicate(true)
	missing_row.rows[0].available = false
	mission_art.present(missing_row)
	for i in range(2): await process_frame
	await capture("10-mission-menu-row-zero-missing-bit")
	mark("mission_menu_row_zero_draw", "checked" if missing_row.rows[0].draw_present and not missing_row.rows[0].available and int(mission_art.submitted_tints.get("misswar.tex", 0)) == 0x7f808080 else "refused")
	mission_art.present(table_state)
	var prior_art_valid: bool = mission_art.assets_valid
	var config_refused: bool = not mission_art.configure(scene.real, scene.controller, scene.inspector, PackedByteArray())
	scene._refresh_mission_menu()
	await capture("10-mission-menu-art-refused")
	var refused_before: Dictionary = scene.controller.script_mission_menu_state()
	var refused_input := game_key(KEY_ENTER)
	var refused_after: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_menu_art_gate", "checked" if prior_art_valid and config_refused and not mission_art.visible and refused_input and refused_before == refused_after else "refused",
		{"input_handled": refused_input})
	mission_art.configure(scene.real, scene.controller, scene.inspector, scene.real.hud_font_bytes())
	scene._refresh_mission_menu()
	for first_active in [false, true]:
		for pause_open in [false, true]:
			var prompt_state: Dictionary = table_state.duplicate(true)
			prompt_state.network = true
			prompt_state.prompt_first_active = first_active
			prompt_state.prompt_paused = pause_open
			mission_art.present(prompt_state)
			for i in range(2): await process_frame
			await capture("10-mission-menu-network-%d-%d" % [int(first_active), int(pause_open)])
			var expected_key := "AcceptBack" if first_active else ("" if pause_open else "clientMissionSelect")
			mark("mission_menu_network_prompt", "checked" if mission_art.submitted_text_keys == ([] if expected_key.is_empty() else [expected_key]) else "refused",
				{"first_active": first_active, "paused": pause_open})
	var unknown_prompt: Dictionary = table_state.duplicate(true)
	unknown_prompt.network = true
	mission_art.present(unknown_prompt)
	for i in range(2): await process_frame
	await capture("10-mission-menu-network-unknown")
	mark("mission_menu_network_unknown", "checked" if mission_art.submitted_text_keys.is_empty() else "refused")
	mission_art.present(table_state)
	if DisplayServer.get_name() != "headless":
		var menu_view = scene.mission_menu_panel
		var actual: Dictionary = scene.controller.script_mission_menu_state()
		var base_image := Image.load_from_file(out_dir.path_join("10-mission-menu.png"))
		var altered: Dictionary = actual.duplicate(true)
		var altered_rows: Array = altered.get("rows", [])
		if altered_rows.size() == 12:
			var second_row: Dictionary = altered_rows[1]
			second_row.available = true
			second_row.draw_present = true
			menu_view.present(altered)
			await capture("10-mission-menu-second-available")
		menu_view.present(actual)
		var second := Image.load_from_file(out_dir.path_join("10-mission-menu-second-available.png"))
		var differences := 0
		if base_image != null and second != null and base_image.get_size() == second.get_size():
			# The documented second node is (70,240), 411x156 on a 640x448 canvas.
			var scale := minf(float(base_image.get_width()) / 640.0, float(base_image.get_height()) / 448.0)
			var left := int((base_image.get_width() - 640.0 * scale) * 0.5)
			var top := int((base_image.get_height() - 448.0 * scale) * 0.5)
			for y in range(250, 385, 8):
				for x in range(80, 465, 8):
					var point := Vector2i(left + int(x * scale), top + int(y * scale))
					if base_image.get_pixelv(point) != second.get_pixelv(point): differences += 1
		mark("mission_menu_render", "checked" if menu_view.assets_valid and menu_view.art.size() == 32 and differences > 0 else "refused",
			{"textures": menu_view.art.size(), "locked_available_changed_samples": differences})
	if args.has("menu-cancel-only"):
		var cancel_handled := game_key(KEY_ESCAPE)
		for i in range(9): await process_frame
		var cancelled: Dictionary = scene.controller.script_mission_menu_state()
		var cancel_ok: bool = cancel_handled and int(cancelled.get("state", -1)) == 4 and int(cancelled.get("counter", 1)) <= 0 and not cancelled.get("open", true) and not scene.mission_menu_panel.visible
		mark("mission_menu_cancel", "checked" if cancel_ok else "refused",
			{"handled": cancel_handled, "closed": not cancelled.get("open", true), "counter": cancelled.get("counter", 1)})
		save_json()
		quit(0 if cancel_ok else 1)
		return
	var locked_move_handled := game_key(KEY_DOWN)
	var locked_move_state: Dictionary = scene.controller.script_mission_menu_state()
	mark("mission_menu_locked_navigation", "checked" if locked_move_handled and int(locked_move_state.get("row", -1)) == 0 and int(locked_move_state.get("column", -1)) == 0 else "refused",
		{"handled": locked_move_handled, "row": locked_move_state.get("row", -1), "column": locked_move_state.get("column", -1)})
	var selected := false
	if trigger_opened_menu:
		var ready: Dictionary = scene.controller.script_mission_menu_state()
		if int(ready.get("state", -1)) == 2 and ready.get("first_story_available", false):
			var confirm_handled := game_key(KEY_ENTER)
			var confirm: Dictionary = scene.controller.script_mission_menu_state()
			for i in range(4): await process_frame
			var waiting: Dictionary = scene.controller.script_mission_menu_state()
			await capture("10-mission-menu-fade-gate")
			mark("mission_menu_fade_gate", "checked" if confirm_handled and int(confirm.get("state", -1)) == 5 and int(waiting.get("state", -1)) == 6 and not waiting.get("pending", "") else "refused")
			# Controlled test fade; the live client refuses until its source is ported.
			var after_fade: Dictionary = scene.controller.script_mission_menu_tick(0, 128)
			if after_fade.get("ok", false): after_fade = scene.controller.script_mission_menu_tick(0, 128)
			if after_fade.get("ok", false): after_fade = scene.controller.script_mission_menu_tick(0, 128)
			selected = after_fade.get("launched", false) and str(after_fade.get("pending", "")) == "blood1"
	var pending: Dictionary = scene.controller.script_level_request()
	mark("mission_1_story_request", "blood1" if selected and str(pending.get("name", "")) == "blood1" else "refused",
		{"queued": pending.get("queued", false), "orientation": pending.get("orientation", -1)})
	var record_before: Dictionary = scene.controller.get_observation()
	var quests_before: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	if selected and DisplayServer.get_name() != "headless":
		scene.loading_start_submitted.connect(func(): call_deferred("capture", "10-start"), CONNECT_ONE_SHOT)
		scene.loading_started.connect(func(): call_deferred("capture", "10-loading"), CONNECT_ONE_SHOT)
	var transition_ok: bool = selected and bool(await scene._apply_level_change())
	var loading_frames: int = scene.loading_display.loading_frame
	var handoff_errors: int = scene.audio.bank_errors.size() if scene.audio != null and "bank_errors" in scene.audio else -1
	var loading_ok: bool = transition_ok and loading_frames >= 1 + LoadingDisplay.WARMUP_PRESENTS \
		and scene.loading_display.phase == "fade" and handoff_errors == 0
	var record_after: Dictionary = scene.controller.get_observation()
	var quests_after: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var mission1_still_available: bool = bool(scene.controller.script_mission_menu_state().get("first_story_available", false))
	var mission2_not_inferred: bool = not bool(scene.controller.script_mission_menu_state().get("second_story_available", true))
	var record_carried: bool = transition_ok and loading_ok and scene.last_level_script_attached \
		and scene.last_campaign_carried and scene.last_campaign_image_restored \
		and str(record_after.get("name", "")) == str(record_before.get("name", "")) \
		and int(record_after.get("player_class", -1)) == int(record_before.get("player_class", -2)) \
		and int(record_after.get("gold", -1)) == int(record_before.get("gold", -2)) \
		and int(record_after.get("experience", -1)) == int(record_before.get("experience", -2)) \
		and int(record_after.get("level", -1)) == int(record_before.get("level", -2)) \
		and int(record_after.get("record_max_hp_raw", -1)) == int(record_before.get("record_max_hp_raw", -2)) \
		and int(record_after.get("record_max_mana_raw", -1)) == int(record_before.get("record_max_mana_raw", -2))
	var at_blood1: bool = scene.real.current >= 0 and str(scene.real.levels[scene.real.current].stem) == "blood1"
	var markers: Array = scene.controller.script_player_markers() if at_blood1 else []
	var markers_exact: bool = markers.size() == 3
	if markers_exact:
		markers_exact = int(markers[0].index) == 227 and markers[0].properties == PackedStringArray(["default=1"]) \
			and int(markers[1].index) == 287 and markers[1].properties == PackedStringArray(["from=strife"]) \
			and int(markers[2].index) == 298 and markers[2].properties == PackedStringArray(["from=drundar"])
	mark("blood1_marker_records", "exact" if markers_exact else "refused",
		{"marker_count": markers.size(), "record_indices": [227, 287, 298] if markers_exact else [],
		"arrival_selection_verified": false})
	var route_carried: bool = at_hub and dialog_choice_completed and trigger_opened_menu and selected \
		and record_carried and at_blood1 and mission1_still_available and portal_guarded \
		and portal_request_ok and markers_exact and audit_menu_checks
	mark("blood1_arrival", "arrived" if record_carried and at_blood1 and mission1_still_available else "refused",
		{"level_index_before": transition_before, "level_index_after": scene.real.current,
		"pending_script_level": pending.get("queued", false),
		"script_attached": scene.last_level_script_attached, "campaign_carried": scene.last_campaign_carried,
		"loading_frames": loading_frames, "loading_ready": loading_ok,
		"loading_refusal": scene.loading_display.refusal, "audio_handoff_errors": handoff_errors,
		"campaign_image_restored": scene.last_campaign_image_restored,
		"record_carried": record_carried, "quest_vars_before": int(quests_before.get("vars", 0)),
		"quest_vars_after": int(quests_after.get("vars", 0)),
		"mission1_still_available": mission1_still_available,
			"mission2_not_inferred": mission2_not_inferred,
		"mission_available_before": int(quests_before.get("mission_available_bits", 0)),
		"mission_available_after": int(quests_after.get("mission_available_bits", 0))})
	mark("level_change", "changed" if record_carried and at_blood1 else "refused",
		{"level_index_before": transition_before, "level_index_after": scene.real.current})
	await capture("10-fade-start")
	for i in range(LoadingDisplay.FADE_PRESENTS + 2): await process_frame
	await capture("10-level-change")
	if args.has("route-only"):
		save_json()
		quit(0 if name_q_owned and pause_q_owned and game_menu_q_owned and opening_quest_set and journal_verified and mission2_not_inferred and at_hub and portal_guarded and dialog_choice_completed and portal_request_ok and trigger_opened_menu and selected and record_carried and at_blood1 and markers_exact and mission1_still_available and audit_menu_checks else 1)
		return
	# Continue from the real hub route through the host's blood1 entries. The
	# selected-object Trigger call is a diagnostic; physical occupancy and the
	# 15-list schedule are separate from this probe.
	var story: Dictionary = scene.controller.script_story_status()
	var initial_choice_clean: bool = int(story.get("decision", -1)) == 0 and int(story.get("evil", -1)) == 0
	mark("blood1_initial_choice_state", "clean" if initial_choice_clean else "refused",
		{"decision": int(story.get("decision", -1)), "evil": int(story.get("evil", -1))})
	var blood1_census: bool = at_blood1 and int(story.get("records", 0)) == 363 \
		and int(story.get("script_entries", 0)) == 21 \
		and int(story.get("arrival_markers", 0)) == 3 \
		and int(story.get("walkin_triggers", 0)) == 1 \
		and int(story.get("keyed_exits", 0)) == 2 \
		and int(story.get("doors", 0)) == 2 \
		and int(story.get("hostile_placements", 0)) == 140
	mark("blood1_record_census", "matched" if blood1_census else "refused",
		{"records": int(story.get("records", 0)), "script_entries": int(story.get("script_entries", 0)),
		"arrival_markers": int(story.get("arrival_markers", 0)),
		"walkin_triggers": int(story.get("walkin_triggers", 0)),
		"keyed_exits": int(story.get("keyed_exits", 0)), "doors": int(story.get("doors", 0)),
		"hostile_placements": int(story.get("hostile_placements", 0))})
	var objective_ready: bool = at_blood1 and story.get("mission_1_available", false) \
		and not story.get("mission_1_complete", true) and int(story.get("quest_lines", 0)) > 0
	mark("blood1_initial_objective", "recorded" if objective_ready else "refused",
		{"quest_lines": int(story.get("quest_lines", 0)),
		"mission_1_available": story.get("mission_1_available", false),
		"mission_1_complete": story.get("mission_1_complete", false)})
	mark("blood1_opening_cutscene", "refused_unported_binding" if int(story.get("cutscene_refusals", 0)) > 0 else "not_requested",
		{"cutscene_refusals": int(story.get("cutscene_refusals", 0)),
		"actor_cleanup_refusals": int(story.get("actor_cleanup_refusals", 0)),
		"post_action_pending": story.get("post_action_pending", false),
		"camera_playback_verified": false})
	var script_counts: Dictionary = scene.controller.script_command_counts()
	var executed: Dictionary = script_counts.get("executed", {})
	var refused: Dictionary = script_counts.get("refused", {})
	var script_spawn_calls: int = int(executed.get("generateMonster", 0)) + int(refused.get("generateMonster", 0))
	mark("blood1_hostile_boundary", "authored_placements_unverified" if script_spawn_calls == 0 else "unexpected_script_spawn",
		{"script_spawn_calls": script_spawn_calls,
		"hostile_placements": int(story.get("hostile_placements", 0)),
		"unbuilt_objects": int(story.get("unbuilt_objects", 0)), "combat_verified": false})
	var walkin := -1
	var keyed: Array[int] = []
	for target in scene.controller.script_exit_targets():
		if str(target.get("kind", "")) == "Trigger" and target.get("script_action", false):
			walkin = int(target.index)
		elif str(target.get("kind", "")) == "PushTrigger" and target.get("script_action", false):
			keyed.append(int(target.index))
	var trigger_action: Dictionary = scene.controller.script_run_exit(walkin) if at_blood1 and walkin >= 0 else {"ok": false}
	advance(0, 2)
	var blood_dialog_open: bool = scene.controller.dialog_status().get("open", false)
	mark("blood1_npc_trigger_host_action", "dialog_open" if trigger_action.get("ok", false) and blood_dialog_open else "refused",
		{"trigger_found": walkin >= 0, "selected_object_action_ok": trigger_action.get("ok", false),
		"dialog_open": blood_dialog_open, "physical_edge_verified": false})
	var blood_dialog_visual := false
	var blood_dialog_choice_ready := false
	if blood_dialog_open:
		for i in range(300):
			await process_frame
			advance()
			var draw: Dictionary = scene.npc_dialog.frame
			if not draw.get("sprites", []).is_empty() or not draw.get("glyphs", []).is_empty():
				blood_dialog_visual = true
			blood_dialog_choice_ready = scene.controller.dialog_status().get("choice_ready", false)
			if blood_dialog_visual and blood_dialog_choice_ready: break
	mark("blood1_dialog_visual", "drawn" if blood_dialog_visual else "unverified",
		{"drawn": blood_dialog_visual, "font_ready": scene.npc_dialog.font_texture != null,
		"box_ready": scene.npc_dialog.box_texture != null,
		"choice_ready": blood_dialog_choice_ready,
		"glyphs": scene.npc_dialog.frame.get("glyphs", []).size()})
	await capture("11-blood1-dialog")
	var choice_closed := false
	if blood_dialog_open:
		# The researched graph reaches the evil terminal through node 0's
		# second edge, node 3's second edge, then node 4's first edge.
		var evil_second := {0: true, 3: true}
		var moved_nodes := {}
		for i in range(1200):
			await process_frame
			help_close()
			if scene.paused and not scene.stopped: scene.resume_session()
			var state: Dictionary = scene.controller.dialog_status()
			var node: int = int(state.get("node_index", -1))
			var move_second: bool = args.has("blood1-choice-evil") and evil_second.has(node) \
				and int(state.get("visible_choices", 0)) > 1 and state.get("choice_ready", false)
			if move_second and not moved_nodes.has(node):
				game_key(KEY_DOWN)
				moved_nodes[node] = true
			elif i % 8 == 0 and (not move_second or int(state.get("cursor", 0)) == 1):
				game_key(KEY_ENTER)
			advance()
			if not scene.controller.dialog_status().get("open", false):
				choice_closed = true
				break
	story = scene.controller.script_story_status()
	# The fresh difficulty-1 campaign uses bit 1 in the decision/evil bytes.
	# Research maps the first terminal to good and the second to evil.
	var evil_choice_requested: bool = args.has("blood1-choice-evil")
	var expected_evil_bit := 2 if evil_choice_requested else 0
	var decision_made: bool = initial_choice_clean and choice_closed \
		and int(story.get("decision", -1)) == 2 and int(story.get("evil", -1)) == expected_evil_bit
	mark("blood1_dialog_decision", "recorded" if decision_made else "refused",
		{"closed": choice_closed, "decision": int(story.get("decision", 0)),
		"evil": int(story.get("evil", 0)), "quest_lines": int(story.get("quest_lines", 0)),
		"requested_evil_choice": evil_choice_requested, "expected_evil_bit": expected_evil_bit})
	mark("blood1_door_sequence", "refused_unported_binding" if decision_made and int(story.get("cutscene_refusals", 0)) > 1 else "not_verified",
		{"cutscene_refusals": int(story.get("cutscene_refusals", 0)),
		"door_refusals": int(story.get("door_refusals", 0)),
		"post_action_pending": story.get("post_action_pending", false),
		"live_collision_verified": false})
	mark("blood1_revisit_door_restore", "not_exercised",
		{"revisit_verified": false, "door_state_verified": false})
	mark("blood1_evil_shard_post_action", "blocked_by_cutscene" if args.has("blood1-choice-evil") and story.get("post_action_pending", false) else "not_applicable" if not args.has("blood1-choice-evil") else "unverified",
		{"post_action_pending": story.get("post_action_pending", false),
		"item_grant_verified": false, "deferred_count_verified": false})
	await capture("12-blood1-door-boundary")
	var exit_action := false
	var requested := ""
	for index in keyed:
		var use: Dictionary = scene.controller.script_run_exit(index) if decision_made else {"ok": false}
		var request: Dictionary = scene.controller.script_level_request()
		if use.get("ok", false) and request.get("queued", false):
			exit_action = true
			requested = str(request.get("name", ""))
			break
	var expected_destination := "drundar" if evil_choice_requested else "strife"
	var keyed_destination: bool = requested == expected_destination
	mark("blood1_keyed_exit_host_action", "queued" if exit_action and keyed_destination else "refused",
		{"keyed_targets": keyed.size(), "selected_object_action_ok": exit_action,
		"destination_matches_selected_branch": keyed_destination, "physical_use_verified": false})
	var destination_request_before: Dictionary = scene.controller.script_level_request()
	var destination_player_before: Dictionary = scene.controller.get_observation()
	var destination_choice_before: Dictionary = scene.controller.script_story_status()
	var exit_post_action_pending: bool = destination_request_before.get("post_action_pending", false)
	var destination_loaded: bool = exit_action and keyed_destination and await scene._apply_level_change()
	var at_destination: bool = destination_loaded and scene.real.current >= 0 \
		and str(scene.real.levels[scene.real.current].stem) == expected_destination
	var destination_state: Dictionary = scene.controller.script_story_status()
	var destination_player_after: Dictionary = scene.controller.get_observation()
	var destination_request_after: Dictionary = scene.controller.script_level_request()
	var destination_script_attached: bool = at_destination and scene.last_level_script_attached \
		and destination_state.get("ok", false) \
		and scene.controller.get_play_capabilities().get("level_script_enabled", false) \
		and int(destination_state.get("script_entries", 0)) > 0
	var destination_campaign_restored: bool = at_destination and scene.last_campaign_carried \
		and scene.last_campaign_image_restored
	var destination_player_carried: bool = at_destination and same_player_record(destination_player_before, destination_player_after)
	var destination_choice_carried: bool = at_destination \
		and int(destination_state.get("decision", -1)) == int(destination_choice_before.get("decision", -2)) \
		and int(destination_state.get("evil", -1)) == int(destination_choice_before.get("evil", -2))
	var destination_verified: bool = at_destination and destination_script_attached \
		and destination_campaign_restored and destination_player_carried and destination_choice_carried
	var evil_transition_refused: bool = evil_choice_requested and exit_action and keyed_destination \
		and exit_post_action_pending and not destination_loaded and scene.real.current >= 0 \
		and str(scene.real.levels[scene.real.current].stem) == "blood1" \
		and destination_request_after.get("queued", false) \
		and str(destination_request_after.get("name", "")) == expected_destination \
		and scene.status_label.text == "Level change refused: a pending post action cannot be carried."
	mark("blood1_destination", "loaded" if destination_verified else "refused_pending_post_action" if evil_transition_refused else "refused",
		{"campaign_carried": destination_campaign_restored,
		"campaign_image_restored": at_destination and scene.last_campaign_image_restored,
		"destination_script_attached": destination_script_attached,
		"destination_player_carried": destination_player_carried,
		"destination_choice_carried": destination_choice_carried,
		"exit_post_action_pending": exit_post_action_pending,
		"evil_transition_refused": evil_transition_refused,
		"mission_1_complete": destination_state.get("mission_1_complete", false),
		"completion_branch_verified": false, "hub_return_verified": false})
	await capture("11-blood1-destination")
	var hub_branch: Dictionary = scene.controller.script_level_request()
	var completed_in_destination: bool = destination_verified and destination_state.get("mission_1_complete", false)
	var hub_queued: bool = completed_in_destination and str(hub_branch.get("name", "")) == "hub"
	var hub_loaded: bool = hub_queued and await scene._apply_level_change()
	mark("mission_1_completion_hub_return", "returned" if hub_loaded and scene.real.current >= 0 and str(scene.real.levels[scene.real.current].stem) == "hub" else "branch_unverified",
		{"completed_in_destination": completed_in_destination,
		"hub_queued": hub_queued, "hub_loaded": hub_loaded,
		"destination_counter_inputs_verified": false})
	if args.has("blood1-only"):
		save_json()
		var blood1_steps_verified: bool = route_carried and initial_choice_clean and blood1_census \
			and objective_ready and script_spawn_calls == 0 and trigger_action.get("ok", false) \
			and blood_dialog_open and decision_made and exit_action and keyed_destination
		var branch_verified: bool = evil_transition_refused if evil_choice_requested else destination_verified
		quit(0 if blood1_steps_verified and branch_verified else 1)
		return
	# Exercise physical Trigger entry after the hub story route has reached blood1.
	var walk_exit_ok: bool = await walk_exit_probe()
	if args.has("walk-only"):
		save_json()
		quit(0 if walk_exit_ok else 1)
		return
	# Exercise the real-level use button boundary. A records-only object table
	# cannot establish the live query, so action effects must stay absent.
	var push_sources: Array[int] = []
	var portal_sources: Array[int] = []
	for index in range(scene.real.levels.size()):
		var level: Dictionary = scene.real.levels[index]
		var objects: PackedByteArray = scene.real.read_entry(level.gob, level.stem + ".lmp", "objects.ob")
		if objects.is_empty(): continue
		var inventory: Dictionary = scene.controller.inspect_exit_records(objects)
		if not inventory.get("ok", false): continue
		if int(inventory.get("pushes", 0)) > 0: push_sources.append(index)
		if int(inventory.get("portals", 0)) > 0: portal_sources.append(index)
	var physical_refused := false
	if load_scripted_exit_source(push_sources, "PushTrigger"):
		var player: Dictionary = scene.controller.get_observation()
		var before_use: int = int(scene.controller.script_use_status().get("uses", 0))
		var before_action: Dictionary = scene.controller.script_command_counts().get("executed", {})
		var use_request: Dictionary = scene.controller.dialog_use()
		advance()
		var after_use: Dictionary = scene.controller.script_use_status()
		var after_action: Dictionary = scene.controller.script_command_counts().get("executed", {})
		physical_refused = int(after_use.get("uses", 0)) == before_use and \
			not use_request.get("ok", true) and str(use_request.get("error", "")).contains("Player_interact")
		mark("push_trigger_use", "refused_expected" if physical_refused else "unsafe_dispatch",
			{"candidate_levels": push_sources.size(), "scripted_source": true,
			"player_ready": player.get("player_ready", false),
			"record_health_known": player.get("player_record_currents_known", false),
			"uses_unchanged": int(after_use.get("uses", 0)) == before_use,
			"action_counts_before": before_action, "action_counts_after": after_action,
			"refusal": after_use.get("refusal", use_request.get("error", ""))})
	else:
		mark("push_trigger_use", "blocked_no_real_source", {"candidate_levels": push_sources.size()})
	await capture("11-push-use")
	# Gate confirmation is visible under its own action ID. With no saved
	# GateList or constructed destination grid, the UI and API must refuse.
	var portal_refused := false
	if load_scripted_exit_source(portal_sources, "Portal") or load_scripted_exit_source(portal_sources, "PortalHurt"):
		scene._update_menu()
		var button: Button = scene.actions.real_gate_confirm
		var before_position: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		var request_before: Dictionary = scene.controller.script_level_request()
		var refusal: Dictionary = scene.controller.script_gate_confirm("unsupported")
		var request_after: Dictionary = scene.controller.script_level_request()
		portal_refused = button.visible and button.disabled and not refusal.get("ok", true) and \
			str(refusal.get("error", "")).contains("constructor") and \
			request_after.get("queued", false) == request_before.get("queued", false) and \
			scene.controller.get_observation().get("position", Vector3.ZERO) == before_position
		mark("portal_arrival", "refused_expected" if portal_refused else "unsafe_arrival",
			{"candidate_levels": portal_sources.size(), "real_button_visible": button.visible,
			"real_button_disabled": button.disabled, "request_unchanged": request_after.get("queued", false) == request_before.get("queued", false),
			"position_unchanged": scene.controller.get_observation().get("position", Vector3.ZERO) == before_position,
			"refusal": refusal.get("error", "")})
	else:
		mark("portal_arrival", "blocked_no_real_source", {"candidate_levels": portal_sources.size()})
	await capture("12-portal-arrival")
	if args.has("exits-only"):
		save_json()
		quit(0 if walk_exit_ok and physical_refused and portal_refused else 1)
		return
	help_close()
	if fe.screen() == "pause": tap(START)
	if scene.paused and scene.session_started and not scene.stopped: scene.resume_session()
	# campaign-combat-handoff.md ranks the hub-to-valor route first: hub has
	# no hostile objects, while this level has WarBoar placements. Select the
	# level from the opened game folder rather than relying on its sorted index.
	var fight_level := -1
	for i in range(scene.real.levels.size()):
		if str(scene.real.levels[i].stem) == "valor":
			fight_level = i
			break
	var fight_loaded := false
	if fight_level >= 0:
		fight_loaded = scene.load_real_level(fight_level)
		if not fight_loaded:
			mark("fight_level_departure", "refused", {"reason": scene.status_label.text, "ordinary_route_verified": false})
			# Independent diagnostic selection, like the save branch's object-level
			# census: do not claim a successful departure/checkpoint of that level.
			fight_loaded = scene.load_real_level(fight_level, false, "", "", "", false)
			mark("fight_level_diagnostic", "loaded" if fight_loaded else "refused", {"ordinary_route_verified": false})
		if fight_loaded:
			scene.start_play()
			help_close()
			if scene.paused and scene.session_started and not scene.stopped:
				scene.resume_session()
	mark("enemy_level", "entered" if fight_loaded and scene.real.current == fight_level and scene.play_mode else "refused",
		{"level_index": scene.real.current, "level_found": fight_level >= 0,
		"session_started": scene.session_started,
		"script_enabled": scene.controller.get_play_capabilities().get("level_script_enabled", false)})
	var moves_passed := await campaign_moves()
	if args.has("moves-only"):
		save_json()
		scene.queue_free()
		scene = null
		fe = null
		await process_frame
		quit(0 if moves_passed else 1)
		return
	var before: Dictionary = scene.controller.get_observation()
	var hp_before := float(before.get("hp", -1.0))
	var gold_before := int(before.get("gold", -1))
	var actors_before: Array = before.get("fight_enemies", [])
	var enemy_hp_before := {}
	var enemy_hp_before_total := 0.0
	for actor in actors_before:
		if actor.get("present", false) and not actor.get("dead", false):
			var enemy_hp := float(actor.get("hp", -1.0))
			if int(actor.get("handle", 0)) > 0 and enemy_hp > 0.0:
				enemy_hp_before[int(actor.handle)] = enemy_hp
				enemy_hp_before_total += enemy_hp
	var player_before: Vector3 = before.get("position", Vector3.ZERO)
	await capture("04-fight-start")
	# J goes through the ordinary key path to the shared-World player.
	var attack_handled := false
	var attack_event := InputEventKey.new()
	attack_event.keycode = KEY_J
	attack_event.physical_keycode = KEY_J
	attack_event.pressed = true
	attack_handled = scene.handle_input_event(attack_event)
	attack_event.pressed = false
	scene.handle_input_event(attack_event)
	# Walk to the nearest live enemy with WASD only. The stick is camera
	# relative, so measure what each key does first, then steer with the key
	# set whose measured displacement points at the target.
	var walk_ticks := 0
	var start_distance := 1.0e9
	var last_distance := 1.0e9
	var reached := false
	var target_handle := 0
	var basis := {}
	for bit in [1, 2, 4, 8]:
		var p0: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		advance(bit, 8)
		walk_ticks += 8
		var p1: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
		basis[bit] = Vector2(p1.x - p0.x, p1.y - p0.y)
	var candidates := [1, 2, 4, 8, 5, 6, 9, 10]
	for round in range(30):
		var observed: Dictionary = scene.controller.get_observation()
		var me: Vector3 = observed.get("position", Vector3.ZERO)
		var nearest := 1.0e9
		var aim := Vector2.ZERO
		for actor in observed.get("fight_enemies", []):
			if actor.get("dead", false): continue
			var d := Vector2(actor.position.x - me.x, actor.position.y - me.y)
			if d.length() < nearest:
				nearest = d.length()
				aim = d
				target_handle = int(actor.handle)
		if start_distance > 1.0e8: start_distance = nearest
		last_distance = nearest
		if nearest <= 45.0:
			reached = true
			break
		var best := 1
		var best_dot := -2.0
		for cmd in candidates:
			var v := Vector2.ZERO
			for bit in [1, 2, 4, 8]:
				if cmd & bit: v += basis[bit]
			if v.length() < 0.01: continue
			var dot := v.normalized().dot(aim.normalized())
			if dot > best_dot:
				best_dot = dot
				best = cmd
		advance(best, 12)
		walk_ticks += 12
		if observed.get("stopped", false) or observed.get("failed", false): break
	var organic_reached := reached
	var organic_end_distance := last_distance
	var placed := false
	var placed_at_weakest := false
	if not reached:
		# DIAGNOSTIC: the walk is blind to the level's walls, so a scripted run
		# puts the player next to a boar (controller.fight_place_player is not
		# an original routine). The target is the boar with the lowest hit
		# points: an unarmed level-1 player needs about 25,000 ticks for the
		# smallest boar and several times that for the large ones. Only the
		# placement is a diagnostic; every hit, miss and the death are the real
		# damage path. The organic walk result is reported above.
		var snapshot: Dictionary = scene.controller.get_observation()
		var weakest := 1.0e30
		for actor in snapshot.get("fight_enemies", []):
			if actor.get("dead", false): continue
			if float(actor.hp) < weakest:
				weakest = float(actor.hp)
				target_handle = int(actor.handle)
		for actor in snapshot.get("fight_enemies", []):
			if int(actor.handle) != target_handle: continue
			var spot: Vector3 = actor.position
			var put: Dictionary = scene.controller.fight_place_player(Vector3(spot.x - 40.0, spot.y, spot.z + 10.0), 0)
			placed = bool(put.get("ok", false))
			placed_at_weakest = placed
			scene.diagnostic_placement_at_weakest = placed
			break
		advance(0, 24)
		var after_place: Dictionary = scene.controller.get_observation()
		var me_now: Vector3 = after_place.get("position", Vector3.ZERO)
		for actor in after_place.get("fight_enemies", []):
			if int(actor.handle) == target_handle:
				last_distance = Vector2(me_now.x - actor.position.x, me_now.y - actor.position.y).length()
	await capture("04b-fight-approach")
	# Swing: J held for a few ticks, then released for the rest of the cycle.
	var swings := 0
	var target_dead := false
	var target_hp_start := 0.0
	var target_max_hp := 0.0
	var target_kind := ""
	for actor in scene.controller.get_observation().get("fight_enemies", []):
		if int(actor.handle) == target_handle:
			target_hp_start = float(actor.hp)
			target_max_hp = float(actor.max_hp)
			target_kind = str(actor.kind)
	var player_level_start := int(scene.controller.get_observation().get("fight_player_level", -1))
	var player_exp_start := int(scene.controller.get_observation().get("fight_player_experience", -1))
	var swing_start_msec := Time.get_ticks_msec()
	for cycle in range(SWING_CYCLES):
		advance(16, 6)
		advance(0, 24)
		swings += 1
		var polled: Dictionary = scene.controller.get_observation()
		for actor in polled.get("fight_enemies", []):
			if int(actor.handle) == target_handle and actor.get("dead", false): target_dead = true
		if target_dead or bool(polled.get("fight_failed", false)): break
	var swing_msec := Time.get_ticks_msec() - swing_start_msec
	if target_dead: advance(0, SETTLE_TICKS)
	var loot_retry_kills := 0
	var loot_retry_placements := 0
	for retry in range(24):
		var observed: Dictionary = scene.controller.get_observation()
		if not observed.get("fight_ground_drops", []).is_empty() or observed.get("fight_failed", false): break
		var next_target := {}
		for actor in observed.get("fight_enemies", []):
			if actor.get("dead", false) or float(actor.get("hp", 0.0)) <= 0.0: continue
			if next_target.is_empty() or float(actor.hp) < float(next_target.hp): next_target = actor
		if next_target.is_empty(): break
		var spot: Vector3 = next_target.position
		var put: Dictionary = scene.controller.fight_place_player(Vector3(spot.x - 40.0, spot.y, spot.z + 10.0), 0)
		if not put.get("ok", false): break
		loot_retry_placements += 1
		advance(0, 24)
		var retry_dead := false
		for cycle in range(SWING_CYCLES):
			advance(16, 6)
			advance(0, 24)
			if cycle % 16 != 0: continue
			var retry_observed: Dictionary = scene.controller.get_observation()
			for actor in retry_observed.get("fight_enemies", []):
				if int(actor.handle) == int(next_target.handle): retry_dead = bool(actor.get("dead", false))
			if retry_dead or retry_observed.get("fight_failed", false): break
		if not retry_dead: break
		loot_retry_kills += 1
		advance(0, SETTLE_TICKS)
		print("PLAYTHROUGH_LOOT_RETRY ", JSON.stringify({"kills": loot_retry_kills,
			"ground_objects": scene.controller.get_observation().get("fight_ground_drops", []).size()}))
	# Follow a real native ground object with ordinary movement. E goes through
	# pressed-use on Player_run; no pickup call or player placement is injected.
	var pickup_walk_ticks := 0
	var pickup_walked := false
	var pickup_start: Dictionary = scene.controller.get_observation()
	var pickup_start_position: Vector3 = pickup_start.get("position", Vector3.ZERO)
	var inventory_units_before := int(pickup_start.get("fight_inventory_units", 0))
	var produced := false
	var picked_up := false
	for attempt in range(360):
		var observed: Dictionary = scene.controller.get_observation()
		var me: Vector3 = observed.get("position", Vector3.ZERO)
		var target := {}
		for drop in observed.get("fight_ground_drops", []):
			produced = produced or bool(drop.get("spawned", false))
			picked_up = picked_up or bool(drop.get("picked_up", false))
			if drop.get("spawned", false) and not drop.get("picked_up", false) and not drop.get("lost", false):
				target = drop
				break
		if picked_up or target.is_empty() or observed.get("fight_failed", false): break
		var spot: Vector3 = target.position
		var aim := Vector2(spot.x - me.x, spot.y - me.y)
		if aim.length() < 20.0 and pickup_walked:
			game_key(KEY_E)
			advance(0, 1)
		else:
			var best := 1
			var best_dot := -2.0
			for cmd in candidates:
				var v := Vector2.ZERO
				for bit in [1, 2, 4, 8]:
					if cmd & bit: v += basis[bit]
				if v.length() < 0.01: continue
				var dot := v.normalized().dot(aim.normalized())
				if dot > best_dot:
					best_dot = dot
					best = cmd
			advance(best, 2)
			pickup_walk_ticks += 2
			var moved: Vector3 = scene.controller.get_observation().get("position", Vector3.ZERO)
			pickup_walked = pickup_walked or moved.distance_to(pickup_start_position) > 0.01
	var after: Dictionary = scene.controller.get_observation()
	for drop in after.get("fight_ground_drops", []): picked_up = picked_up or bool(drop.get("picked_up", false))
	var inventory_units_after := int(after.get("fight_inventory_units", 0))
	var pickup_passed := produced and picked_up and pickup_walked and inventory_units_after > inventory_units_before
	mark("loot_pickup", "passed" if pickup_passed else "refused",
		{"drop_produced": produced, "picked_up": picked_up, "walked": pickup_walked,
		"walk_ticks": pickup_walk_ticks, "inventory_units_before": inventory_units_before,
		"inventory_units_after": inventory_units_after, "rendered_pickup_verified": false,
		"retry_kills": loot_retry_kills, "diagnostic_retry_placements": loot_retry_placements})
	var actors_after: Array = after.get("fight_enemies", [])
	var enemy_damaged := false
	var enemy_died := false
	var enemy_hp_after_total := 0.0
	var live_enemies_after := 0
	var matched_enemies := 0
	var unmatched_enemies := 0
	for actor in actors_after:
		if actor.get("present", false) and not actor.get("dead", false):
			live_enemies_after += 1
		if enemy_hp_before.has(int(actor.get("handle", 0))):
			var enemy_hp := float(actor.get("hp", -1.0))
			matched_enemies += 1
			enemy_hp_after_total += enemy_hp
			enemy_damaged = enemy_damaged or enemy_hp < float(enemy_hp_before[int(actor.handle)])
			# The same handle had positive HP before the attack window.
			enemy_died = enemy_died or (enemy_hp <= 0.0 and bool(actor.get("dead", false)))
		else:
			unmatched_enemies += 1
	unmatched_enemies += maxi(0, enemy_hp_before.size() - matched_enemies)
	var fight_counts: Dictionary = after.get("fight_counts", {})
	var drops_before := 0
	var drops_after := int(fight_counts.get("loot_calls", 0))
	var target_hp_end := 0.0
	var target_state := -1
	var target_dead_flag := false
	for actor in actors_after:
		if int(actor.get("handle", 0)) == target_handle:
			target_hp_end = float(actor.hp)
			target_state = int(actor.get("state", -1))
			target_dead_flag = bool(actor.get("dead", false))
	var player_level_end := int(after.get("fight_player_level", -1))
	var player_exp_end := int(after.get("fight_player_experience", -1))
	var xp_gained := int(fight_counts.get("xp_gained", 0))
	var loot_items := int(fight_counts.get("loot_items", 0))
	var ground_drops: Array = after.get("fight_ground_drops", [])
	var kill_evidence := enemy_died and target_dead_flag and target_hp_end <= 0.0 and int(fight_counts.get("deaths", 0)) >= 1
	var gold_after := int(after.get("gold", -1))
	# WarBoar's documented zero chance must not be replaced by container Gold.
	var auto_container_gold := bool(after.get("gold_from_auto_container", false))
	var fight_started: bool = bool(after.get("fight_started", false)) and bool(after.get("fight_player_ready", false))
	var fight_passed: bool = fight_loaded and scene.real.current == fight_level \
		and fight_started and attack_handled \
		and not enemy_hp_before.is_empty() and enemy_damaged and kill_evidence \
		and xp_gained > 0 and pickup_passed \
		and gold_after == gold_before and not bool(after.get("fight_failed", false))
	var missing_fight_steps := {}
	if not fight_loaded or scene.real.current != fight_level: missing_fight_steps["enemy_level_entry"] = 1
	if not fight_started: missing_fight_steps["campaign_combat_runtime"] = 1
	if not attack_handled: missing_fight_steps["attack_input"] = 1
	if enemy_hp_before.is_empty(): missing_fight_steps["live_enemy"] = 1
	if not enemy_damaged: missing_fight_steps["enemy_damage"] = 1
	if not kill_evidence: missing_fight_steps["enemy_death"] = 1
	if xp_gained <= 0: missing_fight_steps["kill_xp"] = 1
	if not produced: missing_fight_steps["loot_ground_object_spawn"] = 1
	if not pickup_passed: missing_fight_steps["loot_item_pickup"] = 1
	if gold_after != gold_before: missing_fight_steps["warboar_zero_gold"] = 1
	var object_counts: Dictionary = scene.controller.script_command_counts()
	mark("enemy_fight", "passed" if fight_passed else "blocked_unported_combat",
		{"attack_input_handled": attack_handled, "live_enemies_before": enemy_hp_before.size(),
		"live_enemies_after": live_enemies_after,
		"enemy_hp_before_total": enemy_hp_before_total,
		"enemy_hp_after_total": enemy_hp_after_total,
		"matched_enemy_hp_after_total": enemy_hp_after_total,
		"matched_enemies": matched_enemies, "unmatched_enemies": unmatched_enemies,
		"scene_paused": scene.paused, "frontend_screen": fe.screen(),
		"actors_before": actors_before.size(), "actors_after": actors_after.size(),
		"enemy_damaged": enemy_damaged, "enemy_died": enemy_died,
		"kill_evidence": kill_evidence, "target_handle": target_handle, "target_kind": target_kind,
		"target_hp_start": target_hp_start, "target_max_hp": target_max_hp, "target_hp_end": target_hp_end,
		"target_state_end": target_state, "target_dead_flag": target_dead_flag,
		"target_died_by_real_damage": kill_evidence,
		"placement_diagnostic_at_weakest": placed_at_weakest, "swing_wall_msec": swing_msec,
		"xp_gained": xp_gained, "player_level_start": player_level_start, "player_level_end": player_level_end,
		"player_experience_start": player_exp_start, "player_experience_end": player_exp_end,
		"xp_awards": fight_counts.get("xp_awards", 0), "levels_gained": fight_counts.get("levels_gained", 0),
		"fight_awards": after.get("fight_awards", []),
		"loot_items_generated": loot_items, "loot_ground_drops": ground_drops,
		"loot_shared_draws": fight_counts.get("loot_shared_draws", 0),
		"loot_roll_yielded_item": loot_items > 0,
		"gold_reachable": false,
		"fight_started": fight_started, "fight_failed": bool(after.get("fight_failed", false)),
		"fight_error": str(after.get("fight_error", "")),
		"fight_counts": fight_counts, "fight_refusals": after.get("fight_refusals", {}),
		"player_action_state": int(after.get("fight_player_action_state", -1)),
		"player_weapon_class": int(after.get("fight_player_weapon_class", -1)),
		"starting_equipment": str(after.get("fight_starting_equipment", "")),
		"player_weapon_class_source": str(after.get("fight_player_weapon_class_source", "")),
		"player_start": player_before, "player_end": after.get("position", Vector3.ZERO),
		"walk_ticks": walk_ticks, "start_distance": start_distance, "end_distance": last_distance,
		"reached_enemy_by_walking": organic_reached, "walk_end_distance": organic_end_distance, "placed_for_swing": placed, "swing_cycles": swings,
		"ready_ticks": int(after.get("ready_ticks", -1)), "session_stopped": bool(after.get("stopped", false)),
		"hp_before": hp_before, "hp_after": float(after.get("hp", -1.0)),
		"hp_change": float(after.get("hp", -1.0)) - hp_before,
		"gold_before": gold_before, "gold_after": gold_after,
		"gold_from_auto_container": auto_container_gold,
		"drops_before": drops_before, "drops_after": drops_after,
		"objects_built": int(object_counts.get("objects_built", 0)),
		"objects_unbuilt": int(object_counts.get("objects_unbuilt", 0)),
		"warboar_unbuilt": int(object_counts.get("warboar_unbuilt", 0)),
		"minotaur_unbuilt": int(object_counts.get("minotaur_unbuilt", 0)),
		"player_model": int(before.get("player_model", -1)),
		"player_class": int(before.get("player_class", -1)),
		"player_imported": bool(before.get("player_imported", false)),
		"item_load_gate_refusals": int(object_counts.get("item_load_gate_refusals", 0)),
		"load_draws_unmodelled": int(object_counts.get("load_draws_unmodelled", 0)),
		"factory_build_refusals": object_counts.get("build_refusals", {}),
		"missing_fight_steps": missing_fight_steps})
	await capture("05-fight-attempt")
	# The first story level can have one terrain element. Select a real level
	# with multiple elements for the save probe; the level loader supplies its
	# WORLD collision and textured display independently of the save image.
	var terrain_elements := int(scene.controller.get_play_capabilities().get("terrain_elements", 0))
	if terrain_elements <= 1:
		for index in range(scene.real.levels.size()):
			if index == scene.real.current: continue
			if not scene.load_real_level(index): continue
			terrain_elements = int(scene.controller.get_play_capabilities().get("terrain_elements", 0))
			if terrain_elements > 1:
				if not scene.play_mode: scene.start_play()
				break
	mark("multi_surface_level", "loaded" if scene.real.current >= 0 and terrain_elements > 1 and scene.play_mode else "refused",
		{"terrain_elements": terrain_elements})
	# Use a level whose existing factory built a real Creature subclass, so the
	# save probe exercises a confirmed field. Diagnostic selection does not
	# checkpoint levels it merely inspected on the way to that candidate.
	var object_level_found := int(scene.controller.level_object_state_status().get("built_records", 0)) > 0 and \
		int(scene.controller.get_play_capabilities().get("terrain_elements", 0)) > 1
	if not object_level_found:
		for index in range(scene.real.levels.size()):
			if index == scene.real.current or not scene.load_real_level(index, false, "", "", "", false): continue
			if not scene.controller.get_play_capabilities().get("level_script_enabled", false):
				scene.attach_level_script()
			object_level_found = int(scene.controller.level_object_state_status().get("built_records", 0)) > 0 and \
				int(scene.controller.get_play_capabilities().get("terrain_elements", 0)) > 1
			if object_level_found: break
	if object_level_found: scene.start_play()
	terrain_elements = int(scene.controller.get_play_capabilities().get("terrain_elements", 0))
	mark("object_state_level", "loaded" if object_level_found else "refused",
		{"confirmed_creature_records": int(scene.controller.level_object_state_status().get("built_records", 0)),
		"terrain_elements": int(scene.controller.get_play_capabilities().get("terrain_elements", 0)),
		"session_started": scene.session_started})
	var saved_object_count := int(scene.controller.level_object_state_status().get("built_records", 0))
	var level_before: int = scene.real.current
	var saved_position: Vector3 = scene.controller.get_presentation().get("current_position", Vector3.ZERO)
	var saved_observation: Dictionary = scene.controller.get_observation()
	var saved_hp := float(saved_observation.get("hp", -1.0))
	var saved_gold := int(saved_observation.get("gold", -1))
	var saved_experience := int(saved_observation.get("experience", -1))
	var saved_level := int(saved_observation.get("level", -1))
	var saved_campaign: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	help_close()
	if scene.controller.dialog_status().get("open", false):
		refusal("pause", "dialog remained open before pause")
		if scene.controller.dialog_status().get("stalled", false): scene.leave_stalled_dialog()
	if fe.screen() == "playing": tap(START)
	mark("pause", fe.screen())
	await capture("06-pause")
	var pause_save_entry := false
	var pause_entries: int = fe.view.get("entries", []).size()
	if fe.screen() == "pause":
		for entry in fe.view.get("entries", []):
			if str(entry.get("id", "")) == "save": pause_save_entry = true
	# Research gives the original pause exactly five entries, with no Save.
	# The host's F5 route opens the documented in-game save screen.
	var native_pause_intact: bool = fe.screen() == "pause" and pause_entries == 5 and not pause_save_entry
	var save_path := out_dir.path_join("playthrough.ocplay")
	var pickup_refusal_verified: bool = pickup_passed and pickup_save_refused(scene.controller, save_path)
	mark("pickup_save_preflight", "refused_as_expected" if pickup_refusal_verified else "refused", {"existing_files_preserved": pickup_refusal_verified})
	for suffix in ["", ".player", ".menus", ".level", ".campaign", ".worldstate"]:
		if FileAccess.file_exists(save_path + suffix): DirAccess.remove_absolute(save_path + suffix)
	fe.queue_path("save_session", save_path)
	var route_handled := game_key(KEY_F5)
	mark("pause_save", "routed" if native_pause_intact and route_handled and fe.screen() == "save-game" else "refused",
		{"original_pause_entries": pause_entries})
	if fe.screen() == "save-game": tap(CROSS)
	var saved := FileAccess.file_exists(save_path) and FileAccess.file_exists(save_path + ".level") and FileAccess.file_exists(save_path + ".campaign") and FileAccess.file_exists(save_path + ".worldstate")
	var save_probe: Dictionary = scene.controller.save_level_object_directory(str(scene.real.levels[scene.real.current].stem)) if not saved else {}
	var dead_state_refused: bool = not saved and str(save_probe.get("error", "")) == "dead built-Creature save state is unverified"
	var refused_without_output: bool = pickup_refusal_verified and not saved and not FileAccess.file_exists(save_path) and not FileAccess.file_exists(save_path + ".level") and not FileAccess.file_exists(save_path + ".campaign") and not FileAccess.file_exists(save_path + ".player")
	mark("in_game_save", "saved" if saved else ("refused_as_expected" if dead_state_refused or refused_without_output else "refused"),
		{"screen": fe.screen(), "dead_state_refused": dead_state_refused})
	var saved_state := FileAccess.get_file_as_bytes(save_path + ".worldstate") if saved else PackedByteArray()
	await capture("07-save")
	if fe.screen() == "saved": tap(CROSS)
	if refused_without_output and fe.screen() == "save-game": tap(CIRCLE)
	if fe.screen() == "playing": tap(START)
	if fe.screen() == "pause":
		for i in range(4): tap(DOWN)
		tap(CROSS)
		if fe.screen() == "exit-confirm":
			tap(LEFT)
			tap(CROSS)
	mark("quit_to_main", fe.screen())
	await capture("08-main-after-quit")
	var quit_ok: bool = fe.screen() == "main"
	var load_attempted := false
	if quit_ok and saved:
		tap(DOWN)
		fe.queue_path("load_session", save_path)
		tap(CROSS)
		if fe.screen() == "load-game":
			load_attempted = true
			tap(CROSS)
	var restored: Dictionary = scene.controller.get_observation()
	var restored_campaign: Dictionary = scene.controller.script_command_counts().get("campaign", {})
	var quests_match := true
	for key in ["quest_lines", "mission_available_bits", "mission_complete_bits", "vars", "vars_nonzero"]:
		quests_match = quests_match and saved_campaign.get(key, -1) == restored_campaign.get(key, -2)
	var carried: bool = quit_ok and load_attempted and fe.screen() == "playing" and scene.real.current == level_before \
		and int(scene.controller.get_play_capabilities().get("terrain_elements", 0)) == terrain_elements \
		and scene.terrain_root.has_meta("level_texture_pair") \
		and int(restored.get("gold", -1)) == saved_gold \
		and is_equal_approx(float(restored.get("hp", -1.0)), saved_hp) \
		and scene.controller.get_presentation().get("current_position", Vector3.ZERO).distance_to(saved_position) < 0.01 \
		and quests_match
	mark("load_and_carry", "unavailable_save_refused" if refused_without_output else ("restored" if carried else "refused"),
		{"screen": fe.screen(), "load_attempted": load_attempted, "status": scene.status_label.text,
		"terrain_elements": terrain_elements,
		"level_index_before": level_before,
		"level_index_after": scene.real.current, "gold_before": saved_gold,
		"gold_after": int(restored.get("gold", -1)), "hp_before": saved_hp,
		"hp_after": float(restored.get("hp", -1.0)), "quests_match": quests_match,
		"experience_before": saved_experience, "experience_after": int(restored.get("experience", -1)),
		"level_before": saved_level, "level_after": int(restored.get("level", -1))})
	var warm_status: Dictionary = scene.controller.level_object_state_status()
	var warm_fields: Dictionary = scene.controller.save_level_object_directory(str(scene.real.levels[scene.real.current].stem)) if carried else {}
	var warm_object_match: bool = saved and saved_object_count > 0 and \
		int(warm_status.get("restored_records", -1)) == saved_object_count and \
		int(warm_fields.get("records", -1)) == saved_object_count and warm_fields.get("current_level_bytes", PackedByteArray()) == saved_state
	mark("level_object_roundtrip", "restored" if warm_object_match else "refused",
		{"confirmed_creature_records": saved_object_count, "restored_records": int(warm_status.get("restored_records", 0))})
	await capture("09-loaded")
	# A new scene has no in-memory WORLD/TEX save association. The load must
	# recover the level from the selected folder and its saved identity. Remove
	# the first scene before creating another process-lifetime audio mixer.
	scene.queue_free()
	await process_frame
	scene = null
	fe = null
	var cold = load("res://scenes/real_data_play.tscn").instantiate()
	cold.automatic_steps = false
	root.add_child(cold)
	await process_frame
	cold.components.library.history_path = out_dir.path_join("cold-library.json")
	cold.components.library._entries.clear()
	cold.frontend.options_path = out_dir.path_join("cold-options.img")
	cold.real.cache_dir = out_dir.path_join("cold-cache")
	var cold_opened: bool = cold.open_game_folder(str(args["game-folder"]))
	var cold_loaded: bool = cold_opened and cold.level_textures.saves.is_empty() and cold.load_session_file(save_path)
	var cold_state: Dictionary = cold.controller.get_observation()
	var cold_campaign: Dictionary = cold.controller.script_command_counts().get("campaign", {})
	var cold_match: bool = cold_loaded and cold.real.current == level_before and cold.terrain_root.has_meta("level_texture_pair") and \
		int(cold.controller.get_play_capabilities().get("terrain_elements", 0)) == terrain_elements and \
		int(cold_state.get("gold", -1)) == saved_gold and is_equal_approx(float(cold_state.get("hp", -1.0)), saved_hp) and \
		cold.controller.get_presentation().get("current_position", Vector3.ZERO).distance_to(saved_position) < 0.01
	for key in ["quest_lines", "mission_available_bits", "mission_complete_bits", "vars", "vars_nonzero"]:
		cold_match = cold_match and cold_campaign.get(key, -1) == saved_campaign.get(key, -2)
	var cold_refused: bool = refused_without_output and cold_opened and not cold_loaded and not FileAccess.file_exists(save_path)
	mark("cold_load", "restored" if cold_match else ("refused_as_expected" if cold_refused else "refused"), {"opened": cold_opened, "loaded": cold_loaded,
		"terrain_elements": int(cold.controller.get_play_capabilities().get("terrain_elements", 0))})
	var cold_status: Dictionary = cold.controller.level_object_state_status()
	var cold_fields: Dictionary = cold.controller.save_level_object_directory(str(cold.real.levels[cold.real.current].stem)) if cold_match else {}
	var cold_object_match: bool = cold_match and saved_object_count > 0 and \
		int(cold_status.get("restored_records", -1)) == saved_object_count and \
		int(cold_fields.get("records", -1)) == saved_object_count and cold_fields.get("current_level_bytes", PackedByteArray()) == saved_state
	mark("cold_level_object_roundtrip", "restored" if cold_object_match else "refused",
		{"confirmed_creature_records": saved_object_count, "restored_records": int(cold_status.get("restored_records", 0))})
	# A displayed level with different collision must not be reused for this save.
	var other_index := 0 if level_before != 0 else 1
	var other_loaded: bool = cold.load_real_level(other_index)
	var mismatch: Dictionary = cold.controller.prepare_save_file(save_path, true) if other_loaded else {}
	var mismatch_refused: bool = other_loaded and not mismatch.get("ok", false) and \
		"collision differs" in str(mismatch.get("error", ""))
	mark("mismatched_display", "unavailable_save_refused" if cold_refused else ("refused_as_expected" if mismatch_refused else "refused"),
		{"different_level_loaded": other_loaded})
	var revisited: bool = other_loaded and cold.load_real_level(level_before)
	var revisit_status: Dictionary = cold.controller.level_object_state_status()
	var revisit_fields: Dictionary = cold.controller.save_level_object_directory(str(cold.real.levels[cold.real.current].stem)) if revisited else {}
	var revisit_match: bool = revisited and saved_object_count > 0 and \
		int(revisit_status.get("restored_records", -1)) == saved_object_count and \
		int(revisit_fields.get("records", -1)) == saved_object_count and \
		revisit_fields.get("current_level_bytes", PackedByteArray()) == saved_state
	mark("level_revisit", "restored" if revisit_match else "refused",
		{"confirmed_creature_records": saved_object_count,
		"restored_records": int(revisit_status.get("restored_records", 0))})
	# A separate, explicitly diagnostic lethal hit on the real level's Player.
	# It queues Player_takeDamage; no HP/body/state/clip word is forced. The
	# source is one of the live factory enemies, whose attack AI is unported.
	var death_loaded: bool = cold.load_session_file(save_path)
	if death_loaded:
		cold.start_play()
		cold.diagnostic_player_death = true
		scene = cold
		fe = cold.frontend
		advance(0, 90) # let spawn/initial state settle before queuing the hit
		var death_before: Dictionary = cold.controller.get_observation()
		var queued: Dictionary = cold.controller.fight_diagnostic_lethal_hit()
		# A local inventory can be open during the hit. The native death closes
		# it immediately, then requests closure again on the wipe zero crossing.
		cold.game_menus.open_screen("inventory")
		advance(0, 1)
		var entry: Dictionary = cold.controller.get_observation()
		var menu_closed: bool = not cold.game_menus.open
		await capture("11-player-dead")
		advance(0, 254) # 239 countdown steps, then 15 fade steps
		await capture("12-player-death-fade")
		advance(0, 100)
		var dead: Dictionary = cold.controller.get_observation()
		var retained: bool = int(dead.get("gold", -1)) == int(death_before.get("gold", -2)) \
			and int(dead.get("experience", -1)) == int(death_before.get("experience", -2)) \
			and int(dead.get("level", -1)) == int(death_before.get("level", -2))
		var sequence: bool = queued.get("ok", false) and int(entry.get("fight_player_action_state", -1)) == 4 \
			and float(entry.get("hp", -1)) == 0.0 and int(entry.get("fight_death_countdown", -1)) == 239 \
			and menu_closed and dead.get("fight_death_animation_started", false) \
			and int(dead.get("fight_death_animation_steps", 0)) > 0 \
			and int(dead.get("fight_death_countdown", -1)) == 0 and int(dead.get("fight_dead_countdown", -1)) == 0 \
			and int(dead.get("fight_death_fade", -1)) == 128 and int(dead.get("fight_death_phase", -1)) == 1 \
			and not str(dead.get("fight_death_error", "")).is_empty() and retained \
			and not dead.get("fight_failed", true)
		mark("player_death", "blocked_death_reload" if sequence else "refused",
			{"diagnostic_lethal_hit": true, "death_sequence_verified": sequence,
			"actors": dead.get("fight_enemies", []).size(), "menu_closed": menu_closed,
			"hp_zero": float(dead.get("hp", -1)) == 0.0,
			"native_clip_started": dead.get("fight_death_animation_started", false),
			"native_clip_steps": dead.get("fight_death_animation_steps", 0),
			"pose_rendered": dead.get("fight_death_pose_rendered", false),
			"wipe_countdown": dead.get("fight_death_countdown", -1), "fade": dead.get("fight_death_fade", -1),
			"phase": dead.get("fight_death_phase", -1), "record_values_retained": retained,
			"reload_refused": not str(dead.get("fight_death_error", "")).is_empty(),
			"respawn_completed": false})
		await capture("13-player-reload-refused")
	else:
		mark("player_death", "refused", {"saved_level_reloaded": false})
	cold.queue_free()
	scene = null
	fe = null
	await process_frame
	save_json()
	var incomplete := false
	for row in rows:
		if str(row.result) in ["refused", "blocked_by_walk_and_talk", "blocked_unported_combat", "blocked_death_reload"]:
			incomplete = true
	quit(1 if incomplete else 0)
