extends SceneTree

# Opt-in read-only acceptance on one owner's level. Reports counts only.
const CROSS := 0x40
const DOWN := 0x4000
const UP := 0x1000
const RIGHT := 0x2000
var args := {}

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--") and "=" in arg:
			var parts := arg.substr(2).split("=", true, 1)
			args[parts[0]] = parts[1]
	call_deferred("run")

func run() -> void:
	var counts := {"folder": 0, "level": 0, "player": 0, "current_refused": 0,
		"xp_thresholds": 0, "xp_bar": 0, "health_bar": 0, "mana_refused": 0,
		"direct_gold_hidden": 0, "quick_refused": 0, "potion_refused": 0,
		"minimap_refused": 0, "minimap_tiles_refused": 0, "minimap_icons_refused": 0,
		"pending_gate": 0, "pending_missing_dead_refused": 0}
	if not args.has("disc") or not args.has("cache"):
		print("NATIVE_HUD_PLAY_REAL " + JSON.stringify(counts))
		quit(2)
		return
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	scene.real.cache_dir = str(args.cache)
	counts.folder = int(scene.open_game_folder(str(args.disc)))
	if counts.folder == 1 and not scene.real.levels.is_empty():
		var frontend = scene.frontend
		var options_path := "user://native-hud-smoke-options-%d.img" % OS.get_process_id()
		frontend.options_path = options_path
		frontend.boot()
		frontend.tap(CROSS)
		for i in range(239): frontend.step(0)
		frontend.tap(CROSS)
		for i in range(30): frontend.step(0)
		frontend.tap(CROSS)
		for i in range(4): frontend.tap(CROSS)
		frontend.tap(DOWN)
		frontend.tap(CROSS)
		frontend.tap(UP)
		frontend.tap(CROSS)
		frontend.tap(CROSS)
		frontend.tap(CROSS)
		for i in range(20): frontend.tap(RIGHT)
		frontend.tap(CROSS)
		frontend.type_name("HudSmoke")
		frontend.tap(DOWN)
		frontend.tap(CROSS)
		scene.resume_session()
		await process_frame
		var value: Dictionary = scene.controller.get_observation()
		counts.level = int(scene.session_started and scene.real.current >= 0 and scene.play_mode and not scene.paused)
		counts.player = int(value.get("player_ready", false))
		counts.current_refused = int(not value.get("player_record_currents_known", true) and not value.has("record_hp_raw") and not value.has("record_mana_raw"))
		counts.xp_thresholds = int(value.has("experience_base") and value.has("experience_next") and int(value.experience_next) > int(value.experience_base))
		for key in ["experience", "health_bar"]:
			counts["xp_bar" if key == "experience" else key] = int(key in scene.play_hud.drawn)
		counts.mana_refused = int(not "mana_bar" in scene.play_hud.drawn and not str(scene.play_hud.refusals.get("mana_bar", "")).is_empty())
		counts.direct_gold_hidden = int(not "gold" in scene.play_hud.drawn and not str(scene.play_hud.refusals.get("gold", "")).is_empty())
		counts.quick_refused = int(not str(scene.play_hud.refusals.get("quick_slots", "")).is_empty())
		counts.potion_refused = int(not str(scene.play_hud.refusals.get("potion_notice", "")).is_empty())
		counts.minimap_refused = int(not str(scene.play_hud.refusals.get("minimap_projector", "")).is_empty())
		counts.minimap_tiles_refused = int(not str(scene.play_hud.refusals.get("minimap_tiles", "")).is_empty())
		counts.minimap_icons_refused = int(not str(scene.play_hud.refusals.get("minimap_icons", "")).is_empty())
		var pending: Dictionary = value.duplicate()
		pending["pending_health"] = 5.0
		pending.erase("dead")
		scene.play_hud.observe_play(pending, true)
		counts.pending_missing_dead_refused = int(not "pending_health" in scene.play_hud.drawn and scene.play_hud.refusals.has("pending_health"))
		pending["hp"] = 0.0
		pending["dead"] = false
		scene.play_hud.observe_play(pending, true)
		counts.pending_gate = int("pending_health" in scene.play_hud.drawn)
		if args.has("capture"):
			scene.play_hud.observe_play(value, true)
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png(str(args.capture))
		if FileAccess.file_exists(options_path): DirAccess.remove_absolute(ProjectSettings.globalize_path(options_path))
	print("NATIVE_HUD_PLAY_REAL " + JSON.stringify(counts))
	var complete := true
	for key in counts: complete = complete and int(counts[key]) == 1
	scene.queue_free()
	quit(0 if complete else 1)
