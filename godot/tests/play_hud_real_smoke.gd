extends SceneTree

# Opt-in: the play-view HUD on one level of the user's own game folder. Never
# part of CI. Prints counts and element names only; level identity is an index.
#   --game-folder=DIR --cache=DIR [--level=N] [--ticks=N] [--capture=FILE.png]
# --capture saves the play view (3D level plus HUD) when not headless; keep it
# under the ignored work/ folder, never in the repository.
var args := {}
var failures := 0

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and "=" in argument:
			var parts := argument.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	run.call_deferred()

func check(ok: bool, what: String) -> bool:
	if not ok:
		failures += 1
		printerr("FAILED: " + what)
	return ok

func run() -> void:
	if not args.has("game-folder") or not args.has("cache"):
		printerr("--game-folder and --cache are required")
		quit(2)
		return
	root.content_scale_size = root.size
	var scene: Control = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.components.library.history_path = str(args.cache).path_join("level-library.json")
	scene.components.library._entries.clear()
	scene.real.cache_dir = str(args.cache)
	check(scene.open_game_folder(str(args["game-folder"])), "game folder opens")
	var counts := {"art": scene.hud_art_counts, "font_loaded": scene.hud_font_loaded}
	check(int(scene.hud_art_counts.get("loaded", 0)) > 0, "documented HUD art loads from the folder")
	check(scene.hud_font_loaded, "FONT.LMP gamefontsmall.fnt is the play HUD font")
	check(scene.play_hud.textures.size() == 23, "play HUD has the 22 bar frames and the status frame")
	var index := int(args.get("level", "0"))
	var loaded: Dictionary = scene.real.load_level(index)
	if not check(loaded.get("ok", false), "level %d loads" % index):
		quit(1)
		return
	scene.actions.play.pressed.emit()
	check(scene.play_mode, "play starts")
	for t in range(int(args.get("ticks", "120"))): scene.advance_step()
	await process_frame
	var hud: Control = scene.play_hud
	counts["level_index"] = index
	counts["ready_ticks"] = int(scene.raw_observation.get("ready_ticks", 0))
	counts["drawn"] = hud.drawn
	counts["records"] = hud.records.size()
	counts["refused"] = hud.refusals.keys()
	counts["assumed"] = hud.assumed.keys()
	counts["round3_refused"] = scene.hud_round3.refusals.keys() if scene.hud_round3 != null else []
	counts["view_size"] = [int(hud.size.x), int(hud.size.y)]
	check(not hud.drawn.is_empty(), "the overlay draws a non-empty set of elements")
	check("health_bar" in hud.drawn and "status_frame" in hud.drawn, "health bar and status frame drawn")
	check(hud.error.is_empty(), "no HUD layout error: " + hud.error)
	check(hud.visible and hud.is_visible_in_tree() and hud.size.x > 0 and hud.size.y > 0, "overlay visible with an area")
	if args.has("capture") and DisplayServer.get_name() != "headless":
		for i in range(6): await process_frame
		await RenderingServer.frame_post_draw
		var picture: Image = scene.viewport.get_texture().get_image()
		counts["captured"] = picture != null and picture.save_png(str(args.capture)) == OK
		check(counts.captured, "play view captured")
	print(JSON.stringify(counts, "  ", true))
	scene.return_to_selection()
	scene.real.clear_cache()
	quit(1 if failures else 0)
