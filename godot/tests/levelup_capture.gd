extends SceneTree

# Renders a level-up from an authored save slot using the selected RTA executable.
# The only written artifact is the requested image under ignored work/.
func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var folder := ""
	var capture := ""
	var cache := ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="):
			folder = argument.trim_prefix("--game-folder=")
		elif argument.begins_with("--capture="):
			capture = argument.trim_prefix("--capture=")
		elif argument.begins_with("--cache="):
			cache = argument.trim_prefix("--cache=")
	if folder.is_empty() or cache.is_empty() or not capture.contains("/work/") or DisplayServer.get_name() == "headless":
		push_error("rendered level-up needs a game folder, cache and work/ capture path")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(cache)
	root.size = Vector2i(1400, 1000)
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	await process_frame
	scene.real.cache_dir = cache
	if scene.components.has("library"):
		scene.components.library.history_path = cache.path_join("level-library.json")
	if not scene.open_game_folder(folder) or not scene.load_real_font(0):
		push_error("selected RTA folder or font refused")
		quit(1)
		return
	var loaded := false
	for index in range(mini(8, scene.real.levels.size())):
		if scene.real.load_level(index).get("ok", false):
			loaded = true
			break
	if not loaded:
		push_error("no playable level loaded")
		quit(1)
		return
	scene.actions.play.pressed.emit()
	await process_frame
	var slot: PackedByteArray = ClassDB.class_call_static("ChampionsPs2SaveImport", "synthetic_slot", "Aria", 777)
	var imported: Dictionary = scene.ps2_import.import_bytes(slot, 0)
	if imported.get("ok", false):
		var authored: PackedByteArray = scene.ps2_import.record_bytes()
		# This fixture has no linked player-object image. Exercise the documented
		# no-object recalc branch with coherent base/effective stats instead.
		authored.encode_u32(0x43d8, 0)
		authored.encode_u32(0x43f0, 0)
		authored[0x58] = authored[0x59] # valid acknowledged level for the pre-award HUD
		authored.encode_u32(0x7c, 100) # authored Intelligence for a positive mana maximum
		for index in range(4):
			authored.encode_u32(0x90 + index * 4, authored.decode_u32(0x78 + index * 4))
		authored.encode_u32(0xac, 0x3f800000) # alive HP, enabling the observed refill
		authored.encode_u32(0xb0, 0x3f800000) # authored positive maximum in this detached fixture
		authored.encode_u32(0xc0, 0x3f800000) # current mana
		authored.encode_u32(0xc4, 0x3f800000) # authored positive maximum in this detached fixture
		imported = scene.ps2_import.replace_record(authored)
	var adopted: Dictionary = scene.game_menus.adopt_imported() if imported.get("ok", false) else imported
	if not adopted.get("ok", false):
		push_error("authored save slot or selected executable refused: " + str(adopted.get("error", "")))
		quit(1)
		return
	# Align the authored slot's XP with the selected executable's table before
	# rendering the HUD or crossing the next threshold.
	var normalized: PackedByteArray = scene.ps2_import.record_bytes()
	normalized.encode_u32(0x5c, int(scene.game_menus.service.state().xp_base))
	if not scene.ps2_import.replace_record(normalized).get("ok", false) or not scene.game_menus.adopt_imported().get("ok", false):
		push_error("authored XP normalization refused")
		quit(1)
		return
	var before: Dictionary = scene.game_menus.service.state()
	var award := maxi(0, int(before.next_xp) - int(before.experience))
	var granted: Dictionary = scene.game_menus.grant_xp(award)
	if not granted.get("ok", false) or int(granted.get("levels_gained", 0)) < 1:
		push_error("authored XP grant did not cross a threshold: " + str(granted.get("error", "")))
		quit(1)
		return
	var persisted: PackedByteArray = scene.ps2_import.record_bytes()
	if int(scene.ps2_import.record_word(0x5c)) != int(granted.experience) or int(persisted[0x59]) != int(granted.level):
		push_error("XP grant did not update the imported player record")
		quit(1)
		return
	scene.game_menus.open_screen("level-up")
	scene.game_menus.sync_font(scene.components.art)
	scene.refresh_menu_hud()
	if scene.play_hud == null or not scene.play_hud.drawn.has("experience") or not scene.play_hud.drawn.has("level_text"):
		push_error("player HUD did not draw the XP bar and earned level")
		quit(1)
		return
	for _frame in range(4):
		await process_frame
		await RenderingServer.frame_post_draw
	var image: Image = root.get_viewport().get_texture().get_image()
	if image == null:
		push_error("rendered viewport image is absent")
		quit(1)
		return
	var saved := image.save_png(capture)
	var before_spend: Dictionary = scene.game_menus.service.state()
	await create_timer(3.05).timeout
	scene.game_menus._spend_stat(3)
	var after_spend: Dictionary = scene.game_menus.service.state()
	if not after_spend.get("ok", false) or int(after_spend.stat_points) != int(before_spend.stat_points) - 1 or int(after_spend.base_stats[3]) != int(before_spend.base_stats[3]) + 1 or int(scene.ps2_import.record_word(0x84)) != int(after_spend.base_stats[3]):
		push_error("rendered level-up stat interaction did not update the imported player record")
		quit(1)
		return
	print("LEVELUP_CAPTURE transitions=1 stat_spends=1 saved=%d" % saved)
	quit(0 if saved == OK else 1)
