extends SceneTree

# Opt-in run of the real-data play scene against the user's own game folder.
# Prints and writes aggregate counts only; level identities are indices. Never
# part of CI. Arguments (all paths are explicit and outside the project):
#   --game-folder=DIR --cache=DIR [--summary=FILE] [--capture-dir=DIR]
#   [--fx-defs=FILE] [--fx-recipes=FILE] [--levels=N] [--all] [--ticks=N]
#   [--new-only]  play/capture only levels with water, moving pieces or more
#                 than the session's static-mesh cap (refused before round 2)
#   [--from=I] [--to=J]  only level indices I..J-1 (for parallel slices)
#   [--scripts]   opt into the level script after load and before play
#   [--water]     capture only levels that have water tiles
#   [--wade]      walk the player toward the nearest water and capture that view
#   [--wade-probe] with --wade: once in water, one harness water_disturb with
#                 the Player jump-state strength and radius (ref_player.py
#                 state_jump, 0x453b8000 and 0x41c00000) at the player
#                 position, then 24 ticks and a capture. A test stimulus: the
#                 scene does not run that state, and the jump's 12-unit drop
#                 in z is not applied.
#   [--wade-diagnostic] run the native Wader on the observed player once per
#                 observation. OFF by default: the control walk has no frame
#                 counter or stick speed (player-wading.md 3, 13), so the
#                 step is refused and its ticks counted (water.wading_refused)
#   [--level-lights=FILE] level-lights JSON; also supplies the level flag word
#   [--diverse]   with --new-only: capture a level only when it adds a kind
#                 (water, claimed doors, mesh cap) not captured yet
var args := {}
var scene: Control
var counts := {}
var reasons := {}
var failures := 0
var captured_kinds := {}
const RealAudio = preload("res://real_data/real_audio.gd")
const RealFx = preload("res://real_data/real_fx.gd")
const RealWorld = preload("res://real_data/real_world.gd")

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
		elif argument.begins_with("--"):
			args[argument.substr(2)] = true
	RealWorld.wade_diagnostic = args.has("wade-diagnostic")
	run.call_deferred()

func add(key: String, n := 1) -> void:
	counts[key] = int(counts.get(key, 0)) + n

func peak(key: String, n: int) -> void:
	counts[key] = maxi(int(counts.get(key, 0)), n)

func refuse(stage: String, text: String) -> void:
	# Keep only the native message head; strip anything path- or name-like.
	var head := text.split("(", true, 1)[0].strip_edges()
	var name := RegEx.create_from_string("[^ ]*(/|\\.[A-Za-z0-9]{2,})[^ ]*")
	head = name.sub(head, "<name>", true)
	var key := stage + " | " + head.left(120)
	reasons[key] = int(reasons.get(key, 0)) + 1

func check(ok: bool, what: String) -> bool:
	if ok: add("checks.passed")
	else:
		failures += 1
		add("checks.failed")
		printerr("FAILED: " + what)
	return ok

func capture(label: String) -> void:
	for i in range(6): await process_frame
	if not args.has("capture-dir") or DisplayServer.get_name() == "headless": return
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	if image != null and image.save_png(str(args["capture-dir"]).path_join(label + ".png")) == OK: add("captures")

func terrain_stats() -> Dictionary:
	var stats := {"meshes": 0, "surfaces": 0, "textured": 0, "aabb": AABB()}
	var first := true
	for node in scene.terrain_root.find_children("*", "MeshInstance3D", true, false):
		var mesh: MeshInstance3D = node
		if mesh.mesh == null: continue
		stats.meshes += 1
		stats.surfaces += mesh.mesh.get_surface_count()
		var material = mesh.material_override
		if material is ShaderMaterial and material.get_shader_parameter("base_texture") != null: stats.textured += 1
		var box: AABB = mesh.global_transform * mesh.get_aabb()
		stats.aabb = box if first else stats.aabb.merge(box)
		first = false
	return stats

func overview(box: AABB) -> void:
	var centre := box.get_center()
	var reach := maxf(box.size.x, box.size.z) * 0.55 + 10.0
	scene.camera.position = centre + Vector3(reach * 0.6, reach * 0.7, reach * 0.6)
	scene.camera.look_at(centre, Vector3.UP)
	# Camera-only: a whole large level needs a farther far plane.
	scene.camera.far = maxf(scene.camera.far, reach * 4.0)

func run() -> void:
	root.content_scale_size = root.size
	var cache := str(args.get("cache", ""))
	if cache.is_empty() or not args.has("game-folder"):
		printerr("--game-folder and --cache are required")
		quit(2)
		return
	if args.has("capture-dir"): DirAccess.make_dir_recursive_absolute(str(args["capture-dir"]))
	scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = cache.path_join("level-library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = cache
	check(scene.open_game_folder(str(args["game-folder"])), "game folder opens")
	add("levels.candidates", scene.real.levels.size())
	add("creature.archive_candidates", scene.creature_archives().size())
	add("arena.tables_read" if scene.real.arena_tables.get("ok", false) else "arena.tables_unavailable")
	check(scene.audio is RealAudio and scene.fx is RealFx, "real presenters installed")
	add("audio.global_bank_found" if not scene.audio.global_bank.is_empty() else "audio.global_bank_missing")
	var sheets: Dictionary = scene.fx.sheet_result
	add("fx.sheets_loaded" if sheets.get("ok", false) else "fx.sheets_refused")
	add("fx.sheet_textures", sheets.get("textures", {}).size())
	add("fx.sheet_missing", sheets.get("missing", []).size())
	for why in sheets.get("refusals", []): refuse("fx.sheets", str(why))
	if args.has("fx-defs"):
		var defs: Dictionary = scene.fx.load_definition_file(str(args["fx-defs"]))
		add("fx.definitions_loaded" if defs.get("ok", false) else "fx.definitions_refused")
		if not defs.get("ok", false): refuse("fx.definitions", str(defs.get("error", "")))
	if args.has("level-lights"):
		var lights: Dictionary = scene.load_level_lights(str(args["level-lights"]))
		add("lights.json_loaded" if lights.get("ok", false) else "lights.json_refused")
		add("lights.json_rows", int(lights.get("rows", 0)))
	if args.has("fx-recipes"):
		var parsed = JSON.parse_string(FileAccess.get_file_as_string(str(args["fx-recipes"])))
		if parsed is Dictionary: add("fx.recipe_light_records", parsed.get("light_records", {}).size())
		var recipes: Dictionary = scene.load_recipe_constants(str(args["fx-recipes"]))
		add("fx.recipes_loaded" if recipes.get("ok", false) else "fx.recipes_refused")
		if not recipes.get("ok", false): refuse("fx.recipes", str(recipes.get("error", "")))
		add("fx.full_fixture_available" if scene.play_capabilities.get("full_fixture_available", false) else "fx.full_fixture_unavailable")
	add("hud.play_art_loaded", int(scene.hud_art_counts.get("loaded", 0)))
	add("hud.play_art_refused", int(scene.hud_art_counts.get("refused", 0)) + int(scene.hud_art_counts.get("missing", 0)))
	add("hud.extras_green_loaded", int(scene.hud_extras_counts.get("loaded", 0)))
	add("hud.extras_green_refused", int(scene.hud_extras_counts.get("refused", 0)))
	add("hud.play_font_loaded" if scene.hud_font_loaded else "hud.play_font_missing")
	await hud_art()
	var wanted := int(args.get("levels", "3"))
	var ticks := int(args.get("ticks", "120"))
	var shown := 0
	var archives: PackedStringArray = scene.creature_archives()
	var next_archive := 0
	var first_index := int(args.get("from", "0"))
	var end_index := mini(int(args.get("to", str(scene.real.levels.size()))), scene.real.levels.size())
	for index in range(first_index, end_index):
		if not args.has("all") and shown >= wanted: break
		scene.return_to_selection()
		# Let queued frees of the previous level run before the next load.
		for i in range(3): await process_frame
		var started := Time.get_ticks_msec()
		var result: Dictionary = scene.real.load_level(index)
		add("time.load_ms", Time.get_ticks_msec() - started)
		add("levels.tried")
		if not result.ok:
			add("levels.refused." + str(result.stage))
			refuse("level." + str(result.stage), str(result.get("error", "")))
			write_summary()
			continue
		if args.has("water") and int(result.get("water_tiles", 0)) <= 0:
			add("levels.skipped_without_water")
			write_summary()
			continue
		add("levels.loaded")
		var scripted := false
		if args.has("scripts"):
			scene.level_script.set_pressed_no_signal(true)
			scripted = scene.attach_level_script()
			add("scripts.attached" if scripted else "scripts.attach_refused")
		var kinds: Array[String] = []
		if int(result.get("water_tiles", 0)) > 0: kinds.append("water")
		if int(result.get("pieces", 0)) > 0: kinds.append("pieces")
		if int(result.get("session_surfaces", 0)) > 4096 or int(result.get("session_triangles", 0)) > 1000000: kinds.append("over_old_cap")
		for kind in kinds: add("levels.loaded_with_" + kind)
		if kinds.is_empty(): add("levels.loaded_static")
		add("water.tiles_shown", int(result.get("water_tiles", 0)))
		if int(result.get("water_tiles", 0)) > 0:
			add("water.texture_ok" if result.get("water_texture_ok", false) else "water.texture_refused")
			add("water.tiles_not_drawn", int(result.get("water_tiles_refused", 0)))
			peak("water.texture_width", int(result.get("water_texture_width", 0)))
			peak("water.texture_height", int(result.get("water_texture_height", 0)))
			add("water.lines_applied", int(result.get("water_lines", 0)))
			add("water.lines_refused", int(result.get("water_lines_refused", 0)))
			if int(result.get("water_lines", 0)) > 0: add("water.levels_with_lines")
			if result.get("water_negative_fix", false): add("water.levels_additive")
			add("water.level_flags_loaded" if result.get("level_flags_loaded", false) else "water.level_flags_absent")
			if not str(result.get("level_flags_error", "")).is_empty(): refuse("water.level_flags", str(result.level_flags_error))
		add("pieces.total", int(result.get("pieces", 0)))
		add("pieces.claimed", int(result.get("claimed", 0)))
		add("pieces.drawn_claimed", int(result.get("drawn_claimed", 0)))
		peak("session.max_surfaces", int(result.get("session_surfaces", 0)))
		peak("session.max_triangles", int(result.get("session_triangles", 0)))
		var start: Dictionary = result.get("start", {})
		add("start.story_ground" if not start.is_empty() else "start.origin")
		if start.get("marker_taken", false): add("start.marker_taken")
		if start.get("pre_search", false): add("start.pre_search")
		if str(start.get("arena_mode", "")) == "A": add("start.arena_mode_a")
		peak("memory.static_peak_bytes", int(Performance.get_monitor(Performance.MEMORY_STATIC_MAX)))
		peak("memory.video_peak_bytes", int(Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED)))
		var decoded: Dictionary = scene.terrain_root.get_meta("level_texture_pair").decoded if scene.terrain_root.has_meta("level_texture_pair") else {}
		peak("display.max_level_surfaces", int(decoded.get("surfaces", []).size()))
		peak("display.max_level_batches", int(decoded.get("batches", 0)))
		peak("display.max_level_triangles", int(decoded.get("triangles", 0)))
		peak("display.max_level_rgba_bytes", int(decoded.get("rgba_bytes", 0)))
		var stats := terrain_stats()
		add("levels.meshes", stats.meshes)
		add("levels.surfaces", stats.surfaces)
		add("levels.textured_meshes", stats.textured)
		var capture_kinds := kinds.duplicate()
		if int(result.get("drawn_claimed", 0)) > 0: capture_kinds.append("doors")
		var capture_this := shown < wanted and (not args.has("new-only") or not kinds.is_empty())
		if capture_this and args.has("diverse"):
			capture_this = false
			for kind in capture_kinds:
				if kind != "pieces" and not captured_kinds.has(kind): capture_this = true
			if capture_this:
				for kind in capture_kinds: captured_kinds[kind] = true
		started = Time.get_ticks_msec()
		scene.actions.play.pressed.emit()
		if not check(scene.play_mode and scene.fx.enabled, "play starts on a real level"):
			if scripted: take_script_counts()
			refuse("play.start", scene.status_label.text)
			continue
		add("levels.played")
		if scene.fx.level_light_active:
			add("lights.level_record_default_row" if scene.fx.level_light_record.get("default_row", false) else "lights.level_record_named_row")
		else:
			add("lights.synthetic_sun")
		add("audio.level_bank_found" if not scene.audio.level_bank.is_empty() else "audio.level_bank_missing")
		var frames_before: int = scene.audio.pushed_frames
		var creature := {"ok": false}
		if capture_this:
			while next_archive < archives.size() and not creature.ok:
				creature = scene.real.load_creature(archives[next_archive])
				add("creature.archives_tried")
				next_archive += 1
			add("creature.previews" if creature.ok else "creature.none")
			if creature.ok:
				add("creature.textured" if creature.get("textured", false) else "creature.untextured")
				if not creature.get("textured", false): refuse("creature.texture", str(creature.get("texture_error", "")))
				elif not creature.get("alpha1_documented", false): add("creature.blend_gated")
				if creature.get("two_pass", false): add("creature.two_pass")
				if creature.get("environment", false): add("creature.environment")
		if scripted: confirm_help()
		if args.has("wade") and int(result.get("water_tiles", 0)) > 0:
			wade(maxi(ticks, 2400))
		else:
			for t in range(ticks):
				scene.advance_step()
				if scripted: confirm_help()
		# Constant-light chunks (flags 0x100 / 0x2000 / 0x200) take the loaded
		# level's base level from its light record; without one they are gated.
		var constant_chunks := 0
		var constant_lit := 0
		for child in scene.terrain_root.get_children():
			if child is MeshInstance3D and child.has_meta("level_constant_light"):
				constant_chunks += 1
				if int(child.get_meta("level_constant_base", -1)) > 0: constant_lit += 1
		add("lights.constant_chunks", constant_chunks)
		add("lights.constant_chunks_on_level_base", constant_lit)
		# Environment-pass groups keep their own draw order: a streamed display
		# must never pack one into a shared atlas group (order_dependent).
		var env_groups := 0
		var env_in_atlas := 0
		for child in scene.terrain_root.get_children():
			if child is MeshInstance3D and child.has_meta("level_draw_surface") and bool(child.get_meta("level_draw_surface").get("environment", false)):
				env_groups += 1
				if not child.has_meta("level_draw_slot"): env_in_atlas += 1
		add("display.environment_groups", env_groups)
		add("display.environment_in_atlas", env_in_atlas)
		if scene.fx.level_light_active: add("lights.level_base_0x%x" % int(scene.fx.level_light_record.get("base", 0)))
		# The play-view base HUD (godot/ui/play_hud.gd) after the ticks.
		if scene.play_hud != null:
			if check(not scene.play_hud.drawn.is_empty(), "play HUD draws on a real level"): add("hud.play_levels_drawn")
			for element in scene.play_hud.drawn: add("hud.play_drawn." + str(element))
			if not scene.play_hud.error.is_empty(): refuse("hud.play", scene.play_hud.error)
		var started_before := int(counts.get("scripts.started", 0))
		if scripted: take_script_counts()
		if int(counts.get("scripts.started", 0)) > started_before: print("script_started_index ", index)
		add("time.play_ms", Time.get_ticks_msec() - started)
		add("play.ready_ticks", int(scene.raw_observation.get("ready_ticks", 0)))
		if scene.real.world != null:
			add("water.ticks_observed", scene.real.world.ticks_observed)
			add("water.ticks_in_water", scene.real.world.ticks_in_water)
			add("water.entries", scene.real.world.entries)
			# Player_move's water step on the observed player (native Wader):
			# refused unless --wade-diagnostic; the ripple ran ahead of the water tick.
			add("water.wading_refused", scene.real.world.wade_refused_ticks)
			add("water.wading_skipped", scene.real.world.wade_skipped_ticks)
			if RealWorld.wade_diagnostic: add("water.wading_order_ripple_before_tick")
			for key in ["steps", "ticks_in_water", "entries", "exits", "draws", "hurts", "disturbs", "touched", "splashes"]:
				if scene.real.world.wade.has(key): add("water.wading_" + key, int(scene.real.world.wade[key]))
			for why in scene.real.world.errors: refuse("water.service", why)
		add("audio.frames_pushed", scene.audio.pushed_frames - frames_before)
		add("audio.bank_errors", scene.audio.bank_errors.size())
		for why in scene.audio.bank_errors: refuse("audio", why)
		scene.audio.bank_errors.clear()
		add("fx.lighting_world_calls", int(scene.fx.lighting_calls.get("world", 0)))
		for why in scene.fx.refusals: refuse("fx.runtime", str(why))
		if capture_this:
			if args.has("wade"):
				var look: Vector3 = scene.actor.position
				scene.camera.position = look + Vector3(24, 16, 28)
				scene.camera.look_at(look, Vector3.UP)
				await capture("level-%d-wade" % shown)
				if args.has("wade-probe"): await probe(shown)
			await capture("level-%d-play" % shown)
			if creature.ok:
				var shape: MeshInstance3D = scene.components.animation.mesh_instance
				var box: AABB = shape.global_transform * shape.get_aabb()
				var reach := maxf(box.size.length(), 0.5)
				scene.camera.position = box.get_center() + Vector3(1.1, 0.7, 1.1) * reach
				scene.camera.look_at(box.get_center(), Vector3.UP)
				add("creature.preview_triangles", int(shape.mesh.get_faces().size() / 3) if shape.mesh != null else 0)
				await capture("level-%d-creature" % shown)
			# Camera-only overview; no further ready ticks are requested.
			var far: float = scene.camera.far
			overview(stats.aabb)
			await capture("level-%d-overview" % shown)
			await door_capture(shown)
			scene.camera.far = far
		if capture_this: shown += 1
		write_summary() # progressive: a timeout keeps the levels already done
	scene.return_to_selection()
	scene.real.clear_cache()
	counts["complete"] = 1
	print(JSON.stringify(write_summary(), "  ", true))
	quit(0 if failures == 0 else 1)

# Water tile centres, nearest first, at the tile's minimum height.
func _water_targets() -> Array:
	var here: Vector3 = scene.actor.position
	var out: Array = []
	for tile in scene.real.world.info.get("water", []):
		var lo: Vector3 = tile.min
		var hi: Vector3 = tile.max
		out.append(Vector3((lo.x + hi.x) * 0.5, lo.y, (lo.z + hi.z) * 0.5))
	out.sort_custom(func(a, b): return Vector2(a.x - here.x, a.z - here.z).length() < Vector2(b.x - here.x, b.z - here.z).length())
	return out

func _stick_toward(target: Vector3) -> int:
	# Godot (x, z, -y) from native (x, y, z). Bit 8 decreases native y.
	var here: Vector3 = scene.actor.position
	var bits := 0
	if target.x > here.x + 4.0: bits |= 1
	elif target.x < here.x - 4.0: bits |= 2
	if target.z > here.z + 4.0: bits |= 8
	elif target.z < here.z - 4.0: bits |= 4
	return bits

# Stick toward the nearest tile centre; after 90 ticks without getting 8 units
# closer, try the next tile. Stops after 48 ticks in water.
func wade(max_ticks: int) -> void:
	var world = scene.real.world
	if world == null or world.info.get("water", []).is_empty():
		add("water.wade_unavailable")
		return
	var before := int(world.ticks_in_water)
	var targets := _water_targets()
	var pick := 0
	var best := INF
	var since := 0
	var held_in := 0
	for _t in range(max_ticks):
		if scene.stopped:
			add("water.wade_stopped")
			break
		var target: Vector3 = targets[pick % targets.size()]
		var bits := _stick_toward(target)
		if bits == 0: bits = 1
		scene.held_commands = bits
		scene.advance_step()
		if world.in_water:
			held_in += 1
			if held_in >= 48: break
			continue
		held_in = 0
		var here: Vector3 = scene.actor.position
		var d := Vector2(target.x - here.x, target.z - here.z).length()
		if d < best - 8.0:
			best = d
			since = 0
		else:
			since += 1
			if since >= 90:
				pick += 1
				best = INF
				since = 0
	scene.held_commands = 0
	add("water.wade_targets_tried", pick + 1)
	add("water.wade_ticks_in", int(world.ticks_in_water) - before)
	add("water.wade_reached" if world.in_water else "water.wade_missed")

const PROBE_STRENGTH := 0x453B8000 # 3000, Player state 7 (ref_player.py state_jump)
const PROBE_RADIUS := 0x41C00000 # 24
func probe(shown: int) -> void:
	var world = scene.real.world
	if world == null or not world.in_water or not scene.raw_observation.has("position_raw"):
		add("water.probe_skipped")
		return
	var position := PackedInt64Array()
	for word in scene.raw_observation.position_raw: position.push_back(int(word))
	var count := int(world.native.water_disturb(PROBE_STRENGTH, PROBE_RADIUS, position))
	add("water.probe_chunks", maxi(count, 0))
	for _t in range(24): scene.advance_step()
	var look: Vector3 = scene.actor.position
	scene.camera.position = look + Vector3(24, 16, 28)
	scene.camera.look_at(look, Vector3.UP)
	await capture("level-%d-probe" % shown)

# An offline help box pauses the game until confirmed (docs/STRINGS.md). The
# harness confirms on every displayed frame, so the box closes after its
# 90-frame delay and the measured ticks are the same as without a box.
func confirm_help() -> void:
	var hud: Dictionary = scene.controller.text_hud()
	if not hud.get("help_open", false): return
	add("text.help_boxes_confirmed_by_harness")
	if hud.get("help_paused", false): add("text.help_paused_session")
	var frames := 0
	while frames < 200:
		var frame: Dictionary = scene.controller.help_frame(true)
		frames += 1
		if frame.get("closed", false):
			if frame.get("post_action", false): add("text.help_post_actions")
			break
	add("text.help_frames", frames)
	peak("text.help_frames_max", frames)

func take_text_counts() -> void:
	var hud: Dictionary = scene.controller.text_hud()
	if not hud.get("ok", false): return
	if hud.get("has_text", false): add("text.levels_with_table")
	for reason in hud.get("text_refusals", {}):
		add("text.refused." + str(reason), int(hud.text_refusals[reason]))
	if scene.text_hud != null:
		for element in scene.text_hud.drawn:
			add("text.frames_drawn." + str(element), int(scene.text_hud.drawn[element]))
			scene.text_hud.drawn[element] = 0

func take_script_counts() -> void:
	take_text_counts()
	var counts: Dictionary = scene.controller.script_command_counts()
	if not counts.get("ok", false):
		add("scripts.counts_unavailable")
		return
	add("scripts.started", int(counts.get("started", 0)))
	add("scripts.reached_wait", int(counts.get("reached_wait", 0)))
	add("scripts.reached_complete", int(counts.get("reached_complete", 0)))
	add("scripts.faults", int(counts.get("faults", 0)))
	add("scripts.missing_entries", int(counts.get("missing_entries", 0)))
	add("scripts.objects_registered", int(counts.get("objects_registered", 0)))
	add("scripts.objects_refused", int(counts.get("objects_refused", 0)))
	add("scripts.refusal_stops", int(counts.get("refusal_stops", 0)))
	add("scripts.stub_refusals", int(counts.get("stub_refusals", 0)))

	add("scripts.callee_refusals", int(counts.get("callee_refusals", 0)))

	for key in ["objects_built", "objects_unbuilt", "objects_not_created", "objects_pool_refused",
			"objects_unclassified", "load_draws_unmodelled",
			"item_load_gate_refusals", "item_load_draws_inferred"]:
		add("scripts." + key, int(counts.get(key, 0)))
	var refusals: Dictionary = counts.get("build_refusals", {})
	for reason in refusals: add("scripts.build_refused." + str(reason), int(refusals[reason]))
	var stub_reasons: Dictionary = counts.get("stub_refusal_reasons", {})
	for reason in stub_reasons: add("scripts.stub_refusal." + str(reason), int(stub_reasons[reason]))
	if counts.get("seed_bound", false): add("scripts.seed_bound")
	if counts.get("seed_paired", false): add("scripts.seed_paired")
	if counts.get("seed_unpaired", false): add("scripts.seed_unpaired")

	for name in counts.get("executed", {}):
		add("executed." + str(name), int(counts.executed[name]))
		add("levels_executed." + str(name))
	for name in counts.get("refused", {}):
		add("refused." + str(name), int(counts.refused[name]))
		add("levels_refused." + str(name))
	for name in counts.get("inferred", {}):
		add("inferred." + str(name), int(counts.inferred[name]))
	if int(counts.get("started", 0)) > 0: add("scripts.levels_started")
	var campaign: Dictionary = counts.get("campaign", {})
	for key in campaign:
		if key != "num_players": add("campaign." + str(key), int(campaign[key]))
	# The NPC dialog attached to the level script (docs/DIALOG.md).
	var talk: Dictionary = counts.get("dialog", {})
	for key in talk:
		if key == "interact" or key == "mouth" or key == "refused_reasons":
			for result in talk[key]: add("dialog.%s.%s" % [key, str(result)], int(talk[key][result]))
		else:
			add("dialog." + str(key), int(talk[key]))
	var status: Dictionary = scene.controller.dialog_status()
	if status.get("ok", false):
		add("dialog.levels_attached")
		add("dialog.talkers", int(status.get("talkers", 0)))
		if status.get("has_font", false): add("dialog.levels_with_font")
		if status.get("has_text", false): add("dialog.levels_with_text")
		if status.get("open", false): add("dialog.open_at_end_of_play")

func write_summary() -> Dictionary:
	var summary := {"counts": counts, "refusals": reasons, "failures": failures}
	if args.has("summary"):
		var file := FileAccess.open(str(args["summary"]), FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(summary, "  ", true) + "\n")
	return summary

# Open the first drawn, claimed piece through level flow and look at it
# closed and open (camera only; no ready ticks).
func door_capture(index: int) -> void:
	var world = scene.real.world
	if world == null: return
	var piece: int = world.first_drawn_claimed_piece()
	if piece < 0: return
	var chunk := int(world.info.pieces[piece].chunks[0])
	var instance: MeshInstance3D = world.piece_instances[chunk][0]
	var box: AABB = instance.global_transform * instance.get_aabb()
	for other in world.piece_instances[chunk]:
		box = box.merge(other.global_transform * other.get_aabb())
	# Top-down over the closed door, so dropping it 256 uncovers the floor.
	var reach := maxf(box.size.length() * 1.5, 60.0)
	scene.camera.position = box.get_center() + Vector3(0.0, reach, 0.01)
	scene.camera.look_at(box.get_center(), Vector3.UP)
	add("pieces.door_box_size", int(box.size.length()))
	await capture("level-%d-door-closed" % index)
	var before := instance.global_position
	if check(world.set_door(piece, true), "a claimed piece opens through level flow"):
		add("pieces.doors_opened")
		check(is_equal_approx(before.y - instance.global_position.y, 256.0), "an open AutoDoor is drawn 256 lower")
		await capture("level-%d-door-open" % index)
		world.set_door(piece, false)

# Every catalogued HUD/frame identity through the released original-art path,
# then every FNT row in the folder through the ui-fonts-2 page decoder.
func hud_art() -> void:
	var art = scene.components.get("art")
	if art == null: return
	add("fonts.rows", scene.real.fonts.size())
	for i in range(scene.real.fonts.size()):
		add("fonts.decoded" if scene.real.load_font(i) else "fonts.refused")
	var catalogue: Dictionary = art.catalogue
	for id in catalogue:
		add("hud.identities")
		var source: String = scene.real.files.get(str(catalogue[id].lump).to_lower(), "")
		if source.is_empty():
			add("hud.source_missing")
			continue
		if art.source_path != source: art.open_source(source)
		var selected := false
		for index in range(art.identity_choice.item_count):
			if art.identity_choice.get_item_text(index) == id:
				art.identity_choice.select(index)
				selected = true
		art._refresh_rows()
		if not selected or art.row_list.item_count != 1:
			add("hud.rows_%s" % ("none" if art.row_list.item_count == 0 else "multiple"))
			continue
		art.row_list.select(0)
		if art.load_selection(): add("hud.loaded")
		else:
			add("hud.refused")
			refuse("hud", art.picker_status.text)
