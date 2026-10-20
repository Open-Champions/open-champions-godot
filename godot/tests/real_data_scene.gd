extends SceneTree

# Opt-in run of the real-data play scene against the user's own game folder.
# Prints and writes aggregate counts only; level identities are indices. Never
# part of CI. Arguments (all paths are explicit and outside the project):
#   --game-folder=DIR --cache=DIR [--summary=FILE] [--capture-dir=DIR]
#   [--fx-defs=FILE] [--fx-recipes=FILE] [--levels=N] [--all] [--ticks=N]
var args := {}
var scene: Control
var counts := {}
var reasons := {}
var failures := 0
const RealAudio = preload("res://real_data/real_audio.gd")
const RealFx = preload("res://real_data/real_fx.gd")

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
		elif argument.begins_with("--"):
			args[argument.substr(2)] = true
	run.call_deferred()

func add(key: String, n := 1) -> void:
	counts[key] = int(counts.get(key, 0)) + n

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
	if args.has("fx-recipes"):
		var recipes: Dictionary = scene.load_recipe_constants(str(args["fx-recipes"]))
		add("fx.recipes_loaded" if recipes.get("ok", false) else "fx.recipes_refused")
		if not recipes.get("ok", false): refuse("fx.recipes", str(recipes.get("error", "")))
		add("fx.full_fixture_available" if scene.play_capabilities.get("full_fixture_available", false) else "fx.full_fixture_unavailable")
	await hud_art()
	var wanted := int(args.get("levels", "3"))
	var ticks := int(args.get("ticks", "120"))
	var shown := 0
	var archives: PackedStringArray = scene.creature_archives()
	var next_archive := 0
	for index in range(scene.real.levels.size()):
		if not args.has("all") and shown >= wanted: break
		scene.return_to_selection()
		var result: Dictionary = scene.real.load_level(index)
		add("levels.tried")
		if not result.ok:
			add("levels.refused." + str(result.stage))
			refuse("level." + str(result.stage), str(result.get("error", "")))
			continue
		add("levels.loaded")
		var stats := terrain_stats()
		add("levels.meshes", stats.meshes)
		add("levels.surfaces", stats.surfaces)
		add("levels.textured_meshes", stats.textured)
		scene.actions.play.pressed.emit()
		if not check(scene.play_mode and scene.fx.enabled, "play starts on a real level"):
			refuse("play.start", scene.status_label.text)
			continue
		add("levels.played")
		add("audio.level_bank_found" if not scene.audio.level_bank.is_empty() else "audio.level_bank_missing")
		var frames_before: int = scene.audio.pushed_frames
		var creature := {"ok": false}
		if shown < wanted:
			while next_archive < archives.size() and not creature.ok:
				creature = scene.real.load_creature(archives[next_archive])
				add("creature.archives_tried")
				next_archive += 1
			add("creature.previews" if creature.ok else "creature.none")
		for t in range(ticks): scene.advance_step()
		add("play.ready_ticks", int(scene.raw_observation.get("ready_ticks", 0)))
		add("audio.frames_pushed", scene.audio.pushed_frames - frames_before)
		add("audio.bank_errors", scene.audio.bank_errors.size())
		for why in scene.audio.bank_errors: refuse("audio", why)
		scene.audio.bank_errors.clear()
		add("fx.lighting_world_calls", int(scene.fx.lighting_calls.get("world", 0)))
		for why in scene.fx.refusals: refuse("fx.runtime", str(why))
		if shown < wanted:
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
			overview(stats.aabb)
			await capture("level-%d-overview" % shown)
		shown += 1
	scene.return_to_selection()
	scene.real.clear_cache()
	var summary := {"counts": counts, "refusals": reasons, "failures": failures}
	print(JSON.stringify(summary, "  ", true))
	if args.has("summary"):
		var file := FileAccess.open(str(args["summary"]), FileAccess.WRITE)
		if file != null: file.store_string(JSON.stringify(summary, "  ", true) + "\n")
	quit(0 if failures == 0 else 1)

# Every catalogued HUD/frame identity through the released original-art path.
func hud_art() -> void:
	var art = scene.components.get("art")
	if art == null: return
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
