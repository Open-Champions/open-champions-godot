extends "res://scenes/play_flow.gd"

# Play flow plus a run-time game-folder source. The user picks their own
# read-only RTA folder; levels, textures, banks, effect sheets and creature
# archives are read from it at run time and never become project resources.
const RealData = preload("res://real_data/real_data.gd")
const RealText = preload("res://real_data/real_text.gd")
const RealPlayer = preload("res://real_data/real_player.gd")
const TextHud = preload("res://ui/text_hud.gd")
const FightEnemies = preload("res://real_data/real_fight_enemies.gd")
const MissionMenuView = preload("res://ui/mission_menu_view.gd")
var real: Node
var real_player: RefCounted
var fight_enemies: RefCounted
var imported_appearance: Array = []
var imported_record: Dictionary = {}
# The level script's setInfo, help box and bust panel (docs/STRINGS.md).
var text_hud: Control
var text_sources := {}
var real_folder: FileDialog
var real_recipes: FileDialog
var real_lights: FileDialog
var real_level_choice: OptionButton
var real_creature_choice: OptionButton
var real_font_choice: OptionButton
var level_script: CheckButton
var real_exit_choice: OptionButton
var mission_menu_panel
var _level_change_queued := false
var _level_change_loading := false
var _refused_level_request := ""
var last_campaign_carried := false
var last_campaign_image_restored := false
var hud_art_counts: Dictionary = {}
var hud_extras_counts: Dictionary = {}
var hud_font_loaded := false
const NpcDialogView = preload("res://ui/npc_dialog_view.gd")
var npc_dialog: Control
var hub_services: RefCounted
var restoring_real_level := false
# Talk to an NPC: E (use), Up / Down (choice), Enter or Space (confirm).
const TALK_KEYS := {KEY_E: "use", KEY_UP: 0x1000, KEY_DOWN: 0x4000, KEY_ENTER: 0x40, KEY_KP_ENTER: 0x40, KEY_SPACE: 0x40}
const MISSION_KEYS := {KEY_UP: 0x1000, KEY_DOWN: 0x4000, KEY_RIGHT: 0x8000,
	KEY_LEFT: 0x2000, KEY_ENTER: 0x40, KEY_KP_ENTER: 0x40, KEY_SPACE: 0x40,
	KEY_ESCAPE: 0x20, KEY_BACKSPACE: 0x20}

func _ready() -> void:
	super._ready()
	if controller == null: return
	real = RealData.new()
	add_child(real)
	real.configure(self, "user://real-data-cache")
	real_player = RealPlayer.new()
	fight_enemies = FightEnemies.new()
	if viewport != null:
		text_hud = TextHud.new()
		text_hud.set_anchors_preset(Control.PRESET_FULL_RECT)
		viewport.add_child(text_hud)
	real_folder = FileDialog.new()
	real_folder.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	real_folder.access = FileDialog.ACCESS_FILESYSTEM
	real_folder.title = "Choose your game folder (read only)"
	real_folder.dir_selected.connect(open_game_folder)
	real_folder.canceled.connect(_dialog_cancelled)
	add_child(real_folder)
	real_recipes = FileDialog.new()
	real_recipes.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	real_recipes.access = FileDialog.ACCESS_FILESYSTEM
	real_recipes.filters = PackedStringArray(["*.json ; fx_skill_recipes.py --json output"])
	real_recipes.file_selected.connect(load_recipe_constants)
	real_recipes.canceled.connect(_dialog_cancelled)
	add_child(real_recipes)
	real_lights = FileDialog.new()
	real_lights.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	real_lights.access = FileDialog.ACCESS_FILESYSTEM
	real_lights.filters = PackedStringArray(["*.json ; fx_level_lights.py --json output"])
	real_lights.file_selected.connect(load_level_lights)
	real_lights.canceled.connect(_dialog_cancelled)
	add_child(real_lights)
	_add_action("real_folder", "Game folder…", func(): open_dialog(real_folder))
	_add_action("real_recipes", "Skill recipe JSON…", func(): open_dialog(real_recipes))
	_add_action("real_lights", "Level lights JSON…", func(): open_dialog(real_lights))
	_add_action("npc_leave", "Leave stalled dialog", leave_stalled_dialog)
	real_level_choice = OptionButton.new()
	real_level_choice.disabled = true
	real_level_choice.add_item("Game-folder level", -1)
	real_level_choice.item_selected.connect(func(index): load_real_level(real_level_choice.get_item_id(index)))
	menu.add_child(real_level_choice)
	real_creature_choice = OptionButton.new()
	real_creature_choice.disabled = true
	real_creature_choice.add_item("Game-folder creature archive", -1)
	real_creature_choice.item_selected.connect(func(index): load_real_creature(real_creature_choice.get_item_id(index)))
	menu.add_child(real_creature_choice)
	real_font_choice = OptionButton.new()
	real_font_choice.disabled = true
	real_font_choice.add_item("Game-folder HUD font", -1)
	real_font_choice.item_selected.connect(func(index): load_real_font(real_font_choice.get_item_id(index)))
	menu.add_child(real_font_choice)
	npc_dialog = NpcDialogView.new()
	npc_dialog.set_anchors_preset(Control.PRESET_FULL_RECT)
	npc_dialog.configure(controller)
	npc_dialog.audio_owner = self
	if viewport != null: viewport.add_child(npc_dialog)
	else: add_child(npc_dialog)
	# The help box takes input until confirmation; keep it visible above a
	# dialog that happened to be open when the script requested help.
	if viewport != null and text_hud != null:
		viewport.move_child(text_hud, viewport.get_child_count() - 1)
	level_script = CheckButton.new()
	level_script.text = "Level script (opt in)"
	level_script.toggled.connect(_level_script_toggled)
	menu.add_child(level_script)
	real_exit_choice = OptionButton.new()
	real_exit_choice.add_item("No level exit loaded", -1)
	menu.add_child(real_exit_choice)
	_add_action("real_exit", "Use selected level action", activate_selected_exit)
	mission_menu_panel = MissionMenuView.new()
	mission_menu_panel.set_anchors_preset(Control.PRESET_FULL_RECT)
	mission_menu_panel.visible = false
	if viewport != null: viewport.add_child(mission_menu_panel)
	else: add_child(mission_menu_panel)
	hub_services = preload("res://ui/hub_services.gd").new()
	hub_services.configure(self, menu)
	for argument in OS.get_cmdline_user_args():
		if argument == "--level-script": level_script.set_pressed_no_signal(true)
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--skill-recipes="): load_recipe_constants(argument.trim_prefix("--skill-recipes="))
		elif argument.begins_with("--level-lights="): load_level_lights(argument.trim_prefix("--level-lights="))
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="): open_game_folder(argument.trim_prefix("--game-folder="))

func _update_menu() -> void:
	super._update_menu()
	if mode_label != null and play_mode and real != null and real.current >= 0:
		mode_label.text = "Play mode · walk-in triggers and level script actions can change real levels; combat and other campaign paths remain incomplete."
		if raw_observation.get("fight_player_ready", false):
			mode_label.text = "Play mode · shared-World player (WASD moves, J attacks) and WarBoar-family enemies. The player is unarmed (starting equipment refused_unknown). Enemy AI (sight and everything past idle), hit reactions, loot objects, Gold and the Minotaurs remain refused."

func open_game_folder(path: String) -> bool:
	if not real.open_disc(path):
		status_label.text = "Game folder refused: " + real.last_error
		return false
	_clear_imported_appearance()
	real_player.clear()
	super.restore_default_actor()
	real_level_choice.clear()
	real_level_choice.add_item("Game-folder level (%d)" % real.levels.size(), -1)
	for i in range(real.levels.size()): real_level_choice.add_item(str(real.levels[i].stem), i)
	real_level_choice.disabled = false
	real_creature_choice.clear()
	real_creature_choice.add_item("Game-folder creature archive", -1)
	var archives := creature_archives()
	for i in range(archives.size()): real_creature_choice.add_item(archives[i].get_file(), i)
	real_creature_choice.disabled = archives.is_empty()
	real_font_choice.clear()
	var found: Array[Dictionary] = real.find_fonts()
	real_font_choice.add_item("Game-folder HUD font (%d rows)" % found.size(), -1)
	for i in range(found.size()): real_font_choice.add_item("%s row %d" % [str(found[i].path).get_file(), int(found[i].row_index)], i)
	real_font_choice.disabled = found.is_empty()
	hud_art_counts = real.load_hud_art()
	hud_font_loaded = play_hud != null and play_hud.set_font(real.hud_font_bytes())
	hud_extras_counts = {"loaded": 0, "refused": 0}
	var hud_lmp: String = real.files.get("hud.lmp", "")
	if play_hud != null and not hud_lmp.is_empty():
		var extra: Dictionary = load("res://ui/original_art/hud_extras_art.gd").load_green(inspector, hud_lmp)
		hud_extras_counts = {"loaded": int(extra.loaded), "refused": int(extra.refused)}
		play_hud.set_extras_art(extra.images)
		play_hud.configure_gold(0)
	# The string tables and help chime from the folder, for the level script.
	text_sources = RealText.install(real, controller)
	if text_hud != null: text_hud.configure(real, controller, inspector)
	if mission_menu_panel != null:
		mission_menu_panel.configure(real, controller, inspector, real.hud_font_bytes())
	status_label.text = "Game folder opened read-only: %d levels. Choose a level." % real.levels.size()
	if npc_dialog != null:
		npc_dialog.set_font(real.dialog_font())
		npc_dialog.set_box(real.dialog_box())
	return true

func reset_session() -> void:
	_clear_imported_appearance()
	super.reset_session()
	if real != null and real.current >= 0: _install_player_model()

# Loose LMPs that are not a level name; each is only an explicit candidate.
func creature_archives() -> PackedStringArray:
	var out := PackedStringArray()
	var stems := {}
	for level in real.levels: stems[level.stem] = true
	for name in real.files:
		if name.get_extension() == "lmp" and not stems.has(name.get_basename()): out.append(real.files[name])
	out.sort()
	return out

func load_real_level(index: int, carry_campaign := false, previous := "", entrance := "", qualified := "") -> bool:
	if index < 0: return false
	var result: Dictionary = real.load_level(index, previous, entrance, qualified)
	if not result.ok:
		status_label.text = "Level refused at %s. %s" % [result.stage, str(result.get("error", ""))]
		return false
	_install_player_model()
	var scripted := false
	if level_script != null and level_script.button_pressed: scripted = attach_level_script(carry_campaign)
	else: controller.disable_level_script()
	_refresh_exit_targets()
	_install_fight_enemies()
	status_label.text = "Game-folder level loaded with its textures. Start play for lighting, music and effects."
	if level_script != null and level_script.button_pressed:
		status_label.text += " Level script running." if scripted else " Level script refused."
	return true

func restore_default_actor() -> void:
	super.restore_default_actor()
	if real != null and real.current >= 0: _install_player_model()

# Host-viewer camera constants below frame the mesh bounds. The original
# level camera is unported; these values are presentation choices only.
func _apply_presentation() -> void:
	super._apply_presentation()
	_frame_player_camera()
	if fight_enemies != null and raw_observation.get("campaign_fight", false):
		fight_enemies.update(raw_observation)

# The shared-World fight's enemies: the class archive's rest-pose model.
func _install_fight_enemies() -> void:
	if fight_enemies == null: return
	fight_enemies.clear()
	if controller == null or not controller.get_observation().get("campaign_fight", false): return
	var names: Array = controller.combat_resource_lump_names()
	if names.is_empty() or _world == null: return
	fight_enemies.install(real, inspector, _world, str(names[0]), "")

func _frame_player_camera() -> void:
	if real_player == null or not real_player.body_drawn or actor_visual.mesh == null or camera == null: return
	var height: float = actor_visual.mesh.get_aabb().size.y
	if height <= 0.0: return
	var focus := actor.position + Vector3(0, height * 0.5, 0) # Host-viewer focus.
	var half_fov := deg_to_rad(camera.fov * 0.5)
	var distance := maxf(120.0, height * 0.7 / tan(half_fov)) # Host-viewer distance.
	camera.position = focus + Vector3(56, 70, 80).normalized() * distance # Host-viewer direction.
	camera.look_at(focus, Vector3.UP)
	_capture_player_camera()

func _clear_imported_appearance() -> void:
	imported_appearance = []
	imported_record = {}

func _imported_record_matches(record: Dictionary) -> bool:
	return not imported_record.is_empty() and record.get("player_ready", false) \
		and int(record.get("player_record_revision", -1)) == int(imported_record.revision) \
		and str(record.get("name", "")) == str(imported_record.name) \
		and int(record.get("player_class", -1)) == int(imported_record.class_index) \
		and int(record.get("player_model_index", -1)) == int(imported_record.model_index)

func _install_player_model() -> void:
	if real == null or real.current < 0 or real_player == null: return
	var record: Dictionary = controller.get_observation()
	if not record.get("player_ready", false):
		_clear_imported_appearance()
		asset_label.text = "Practice figure · no player record selected"
		return
	var model := int(record.get("player_model_index", -1))
	var player_count := int(frontend.view.get("num_players", 1)) if frontend != null else 1
	if not imported_record.is_empty() and not _imported_record_matches(record):
		_clear_imported_appearance()
	var appearance := imported_appearance if _imported_record_matches(record) else []
	if real_player.install(real, actor_visual, inspector, model, appearance, player_count,
			int(record.get("player_class", -1))):
		displayed_asset_name = "Class body model %d" % model
		_frame_player_camera()
	else:
		super.restore_default_actor()
	asset_label.text = real_player.status

func _show_ps2_import(result: Dictionary) -> Dictionary:
	var accepted: Dictionary = super._show_ps2_import(result)
	if accepted.get("ok", false):
		var record: Dictionary = controller.get_observation()
		_clear_imported_appearance()
		if record.get("player_ready", false) and str(record.get("name", "")) == str(accepted.get("name", "")) \
				and int(record.get("player_class", -1)) == int(accepted.get("class_index", -1)) \
				and int(record.get("player_model_index", -1)) == int(accepted.get("model_index", -1)):
			imported_appearance = accepted.get("appearance", []).duplicate()
			imported_record = {"revision": int(record.get("player_record_revision", -1)),
				"name": str(record.name), "class_index": int(record.player_class),
				"model_index": int(record.player_model_index)}
		if real != null and real.current >= 0: _install_player_model()
	return accepted
# The OCPLAY image owns simulation and position. This small host association
# identifies the level data the user selected; it never embeds disc content.
func save_session_file(path: String) -> bool:
	if real == null or real.current < 0:
		return super.save_session_file(path)
	var talk: Dictionary = controller.dialog_status()
	if talk.get("open", false) or talk.get("mutex_held", false) or controller.text_hud().get("help_open", false):
		status_label.text = "Save refused: finish the current dialog or help box first."
		return false
	var level: Dictionary = real.levels[real.current]
	var source: String = str(real.cache_dir.path_join("level-%03d.world" % real.current))
	var world_hash := FileAccess.get_sha256(source)
	var tex_hash := FileAccess.get_sha256(str(level.tex))
	if world_hash.is_empty() or tex_hash.is_empty():
		status_label.text = "Save refused: this level's WORLD/TEX data cannot be verified."
		return false
	var scripted: bool = controller.get_play_capabilities().get("level_script_enabled", false)
	var campaign := PackedByteArray()
	if scripted:
		var state: Dictionary = controller.save_campaign_bytes()
		if not state.get("ok", false):
			status_label.text = "Save refused: campaign state could not be captured."
			return false
		campaign = state.bytes
	if not super.save_session_file(path): return false
	var player_hash := FileAccess.get_sha256(path + ".player")
	if player_hash.is_empty():
		status_label.text = "Save refused: the player record companion is absent."
		return false
	if scripted and not _write_companion(path + ".campaign", campaign):
		status_label.text = "Save refused: campaign companion could not be written."
		return false
	if not scripted and FileAccess.file_exists(path + ".campaign"):
		DirAccess.remove_absolute(path + ".campaign")
	# The shared-World player is not in the native image: its position, facing and
	# health go in a companion (enemies and other active objects are not saved).
	var fight_obs: Dictionary = controller.get_observation()
	var has_fight: bool = fight_obs.get("fight_player_ready", false)
	if has_fight:
		var where: Vector3 = fight_obs.get("position", Vector3.ZERO)
		var fight_state := {"position": [where.x, where.y, where.z], "facing": int(fight_obs.get("facing_raw", 0)),
			"hp_raw": int(fight_obs.get("hp_raw", 0))}
		if not _write_companion(path + ".fight", JSON.stringify(fight_state).to_utf8_buffer()):
			status_label.text = "Save refused: shared-World player companion could not be written."
			return false
	elif FileAccess.file_exists(path + ".fight"):
		DirAccess.remove_absolute(path + ".fight")
	var manifest := {"version": 1, "save_sha256": FileAccess.get_sha256(path),
		"fight_sha256": FileAccess.get_sha256(path + ".fight") if has_fight else "",
		"player_sha256": player_hash,
		"level_index": real.current, "level_stem": str(level.stem),
		"world_sha256": world_hash, "tex_sha256": tex_hash,
		"scripted": scripted,
		"campaign_sha256": FileAccess.get_sha256(path + ".campaign") if scripted else ""}
	if manifest.save_sha256.is_empty() or (scripted and manifest.campaign_sha256.is_empty()) or \
			not _write_companion(path + ".level", JSON.stringify(manifest).to_utf8_buffer()):
		status_label.text = "Save refused: level association could not be written."
		return false
	status_label.text = "Saved level, player, quests and position. World-object and in-flight script continuation refused."
	return true

func _write_companion(path: String, bytes: PackedByteArray) -> bool:
	var temporary := path + ".tmp"
	if FileAccess.file_exists(temporary): return false
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null: return false
	file.store_buffer(bytes)
	file.flush()
	var ok := file.get_error() == OK
	file.close()
	if not ok or DirAccess.rename_absolute(temporary, path) != OK:
		DirAccess.remove_absolute(temporary)
		return false
	return true

func load_session_file(path: String) -> bool:
	if not FileAccess.file_exists(path + ".level"):
		return super.load_session_file(path)
	var file := FileAccess.open(path + ".level", FileAccess.READ)
	if file == null or file.get_length() > 4096:
		status_label.text = "Load refused: level association is unreadable or too large."
		return false
	var manifest = JSON.parse_string(file.get_as_text())
	if not (manifest is Dictionary) or manifest.get("version", -1) != 1 or \
			FileAccess.get_sha256(path) != manifest.get("save_sha256", ""):
		status_label.text = "Load refused: level association does not match this native save."
		return false
	if FileAccess.get_sha256(path + ".player") != manifest.get("player_sha256", ""):
		status_label.text = "Load refused: player companion is absent or differs."
		return false
	if real == null or real.levels.is_empty():
		status_label.text = "Load refused: open the original game folder first."
		return false
	var index := int(manifest.get("level_index", -1))
	if index < 0 or index >= real.levels.size() or str(real.levels[index].stem) != str(manifest.get("level_stem", "")):
		status_label.text = "Load refused: saved level is absent from this game folder."
		return false
	var level: Dictionary = real.levels[index]
	if FileAccess.get_sha256(str(level.tex)) != manifest.get("tex_sha256", ""):
		status_label.text = "Load refused: saved level TEX differs from the selected folder."
		return false
	var scripted: bool = manifest.get("scripted", false)
	var campaign := PackedByteArray()
	if scripted:
		var campaign_path := path + ".campaign"
		var source := FileAccess.open(campaign_path, FileAccess.READ)
		if source == null or source.get_length() > 1024 * 1024 or \
				FileAccess.get_sha256(campaign_path) != manifest.get("campaign_sha256", ""):
			status_label.text = "Load refused: campaign companion is absent or differs."
			return false
		campaign = source.get_buffer(source.get_length())
	level_script.set_pressed_no_signal(scripted)
	if not load_real_level(index): return false
	var world_path: String = str(real.cache_dir.path_join("level-%03d.world" % index))
	if FileAccess.get_sha256(world_path) != manifest.get("world_sha256", ""):
		status_label.text = "Level reloaded; load refused: saved WORLD differs from the selected folder."
		return false
	restoring_real_level = true
	var loaded := super.load_session_file(path)
	restoring_real_level = false
	if not loaded:
		status_label.text = "Level reloaded; " + status_label.text.replace("Current session retained; ", "")
		return false
	if scripted:
		var restored: Dictionary = controller.load_campaign_bytes(campaign)
		if not restored.get("ok", false):
			status_label.text = "Session replaced; load refused: campaign state is unsupported: " + str(restored.get("error", "unknown"))
			return false
		_observe(restored)
	var fight_hash := str(manifest.get("fight_sha256", ""))
	if not fight_hash.is_empty():
		var fight_path := path + ".fight"
		var fight_file := FileAccess.open(fight_path, FileAccess.READ)
		if fight_file == null or fight_file.get_length() > 4096 or FileAccess.get_sha256(fight_path) != fight_hash:
			status_label.text = "Session replaced; load refused: shared-World player companion is absent or differs."
			return false
		var saved = JSON.parse_string(fight_file.get_as_text())
		var numbers_ok: bool = saved is Dictionary and saved.get("position") is Array and saved.position.size() == 3
		if numbers_ok:
			for field in [saved.position[0], saved.position[1], saved.position[2], saved.get("facing"), saved.get("hp_raw")]:
				numbers_ok = numbers_ok and (typeof(field) == TYPE_INT or typeof(field) == TYPE_FLOAT)
		if not numbers_ok:
			status_label.text = "Session replaced; load refused: shared-World player companion is malformed."
			return false
		var put: Dictionary = controller.fight_restore_player(Vector3(float(saved.position[0]), float(saved.position[1]), float(saved.position[2])),
			int(saved.get("facing", 0)), int(saved.get("hp_raw", 0)))
		if not put.get("ok", false):
			status_label.text = "Session replaced; shared-World player could not be restored: " + str(put.get("error", "unknown"))
			return false
		_observe(controller.get_observation())
	status_label.text = "Loaded level, player, quests and position. Active world objects are not saved; scripts restart from level data."
	return true

func _prepare_native_save(path: String) -> Dictionary:
	return controller.prepare_save_file(path, restoring_real_level)

func _prepare_level_restore(path: String) -> void:
	if restoring_real_level and terrain_root.has_meta("level_texture_pair"):
		level_textures.restore = terrain_root.get_meta("level_texture_pair")
	else:
		super._prepare_level_restore(path)

func _level_script_toggled(on: bool) -> void:
	if real == null or real.current < 0: return
	if on: attach_level_script()
	else: controller.disable_level_script()

# The level's script.scr and objects.ob. Refuses an empty image and does not
# change the play observation. Entry names are the documented loader names.
func _sync_saved_music() -> bool:
	# Direct game-folder play can enter before the front-end has booted. Read
	# its saved options through the native parser so the first script entry sees
	# the same unclamped music word as a front-end-started session.
	if frontend != null and not frontend.booted and FileAccess.file_exists(frontend.options_path):
		var options_probe = ClassDB.instantiate("ChampionsFrontendService")
		var options: Dictionary = options_probe.boot(FileAccess.get_file_as_bytes(frontend.options_path))
		if not options.get("ok", false) or audio == null or not audio.load_options_file_music(int(options.get("music_image", -1))):
			return false
	return true

func attach_level_script(carry_campaign := false) -> bool:
	if real == null or real.current < 0 or controller == null: return false
	if not _sync_saved_music():
		status_label.text = "Level script refused: saved music options could not be loaded."
		return false
	var level: Dictionary = real.levels[real.current]
	var script: PackedByteArray = real.read_entry(level.gob, level.stem + ".lmp", "script.scr")
	var objects: PackedByteArray = real.read_entry(level.gob, level.stem + ".lmp", "objects.ob")
	controller.set_level_text(RealText.level_tables(real, level))
	# The NPC dialog is attached before the level entry runs (docs/DIALOG.md).
	var volume_bits: int = audio.music_volume_bits if audio != null else -1
	var world: PackedByteArray = real.read_entry(level.gob, level.stem + ".lmp", ".world")
	var balance := PackedByteArray()
	if real.files.has("balance.lmp"):
		balance = real.read_entry(real.files["balance.lmp"], "", "monsters.bal")
	var resource_lumps := {}
	for name in controller.combat_resource_lump_names():
		if real.files.has(name):
			resource_lumps[name] = FileAccess.get_file_as_bytes(real.files[name])
	var result: Dictionary = controller.enable_level_script(script, objects, carry_campaign, real.dialog_data(real.current), volume_bits, str(level.stem), balance, world, resource_lumps)
	last_campaign_carried = bool(result.get("campaign_carried", false))
	last_campaign_image_restored = bool(result.get("campaign_image_restored", false))
	if result.get("ok", false): _refused_level_request = ""
	status_label.text = "Level script running." if result.get("ok", false) else "Level script refused."
	_refresh_exit_targets()
	return result.get("ok", false)

func _refresh_exit_targets() -> void:
	if real_exit_choice == null or controller == null: return
	real_exit_choice.clear()
	real_exit_choice.add_item("Level exits and portals", -1)
	for target in controller.script_exit_targets():
		var kind: String = str(target.get("kind", "unknown"))
		var suffix := " · walk-in" if target.get("walk_in", false) else \
			(" · walk-in refused" if target.has("walk_in_refusal") else \
			(" · script action" if target.get("script_action", false) else " · arrival/prompt only"))
		real_exit_choice.add_item("%s #%d%s" % [kind, int(target.index), suffix], int(target.index))

func activate_selected_exit() -> Dictionary:
	var out := {"ok": false, "error": "Select a documented level script action."}
	if controller == null or real_exit_choice == null: return out
	var index: int = real_exit_choice.get_selected_id()
	if index < 0: return out
	out = controller.script_run_exit(index)
	if not out.get("ok", false): status_label.text = "Level action refused: " + str(out.get("error", "unknown"))
	if out.get("queued", false): _queue_level_change()
	return out

func _refresh_mission_menu() -> void:
	if mission_menu_panel == null or controller == null: return
	var state: Dictionary = controller.script_mission_menu_state()
	mission_menu_panel.visible = bool(state.get("ok", false) and state.get("open", false) and mission_menu_panel.assets_valid)
	if mission_menu_panel.visible:
		if status_label.text.begins_with("Mission art refused:"):
			status_label.text = ""
		mission_menu_panel.present(state)
	elif state.get("open", false) and not mission_menu_panel.assets_valid:
		status_label.text = "Mission art refused: " + mission_menu_panel.last_error

func _process(_delta: float) -> void:
	super._process(_delta)
	if mission_menu_panel == null or not mission_menu_panel.visible or controller == null: return
	var state: Dictionary = controller.script_mission_menu_state()
	# State 6 reads an external fade value that this client cannot supply yet.
	if int(state.get("state", -1)) in [0, 1, 4, 5]:
		controller.script_mission_menu_tick(0, 0)
		_refresh_mission_menu()

func _queue_level_change() -> void:
	if _level_change_queued or _level_change_loading: return
	_level_change_queued = true
	call_deferred("_apply_level_change")

func _apply_level_change() -> bool:
	_level_change_queued = false
	if _level_change_loading or controller == null or real == null: return false
	var pending: Dictionary = controller.script_level_request()
	if not pending.get("queued", false): return false
	var requested: String = str(pending.get("name", ""))
	if requested == _refused_level_request: return false
	if pending.get("post_action_pending", false):
		_refused_level_request = requested
		status_label.text = "Level change refused: a pending post action cannot be carried."
		return false
	# level_flow::split_request uses the first underscore for a subname. The
	# resource lookup takes the base; no guessed destination is substituted.
	var base: String = requested.get_slice("_", 0).to_lower()
	var destination := -1
	for i in range(real.levels.size()):
		if str(real.levels[i].stem).to_lower() == base:
			destination = i
			break
	if destination < 0:
		_refused_level_request = requested
		status_label.text = "Level change refused: the requested level has no selected game-folder resource."
		return false
	if destination == real.current:
		_refused_level_request = requested
		status_label.text = "Level change refused: same-level reload state is not ported."
		return false
	_level_change_loading = true
	var was_playing: bool = play_mode and session_started and not stopped
	var changed := load_real_level(destination, true, real.current_qualified,
		str(pending.get("entrance", "")), requested)
	if changed and was_playing: start_play()
	if not changed: _refused_level_request = requested
	_level_change_loading = false
	return changed

# The use button and the dialog's pad words, taken at the next ready tick.
func handle_input_event(event: InputEvent) -> bool:
	if controller != null and mission_menu_panel != null and not mission_menu_panel.assets_valid:
		var refused_menu: Dictionary = controller.script_mission_menu_state()
		if refused_menu.get("open", false) and event is InputEventKey:
			status_label.text = "Mission art refused: " + mission_menu_panel.last_error
			return true
	# The offline help box owns confirmation while it holds the session.
	if controller != null and controller.text_hud().get("help_open", false) and event is InputEventKey:
		return true
	if mission_menu_panel != null and mission_menu_panel.visible and controller != null and event is InputEventKey:
		if event.pressed and not event.echo:
			var mission_key: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
			if MISSION_KEYS.has(mission_key):
				var menu_state: Dictionary = controller.script_mission_menu_state()
				if int(menu_state.get("state", -1)) == 2:
					menu_state = controller.script_mission_menu_tick(int(MISSION_KEYS[mission_key]), 0)
					if int(menu_state.get("state", -1)) == 5:
						status_label.text = "Mission transition refused: external fade completion is unavailable."
				_refresh_mission_menu()
			return true
		return true
	if event is InputEventKey and event.pressed and not event.echo and controller != null and not paused and not stopped:
		var key: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
		if TALK_KEYS.has(key) and controller.dialog_status().get("ok", false):
			var open: bool = controller.dialog_status().get("open", false)
			if TALK_KEYS[key] is String:
				if not open: controller.dialog_use()
				return true
			if open:
				controller.dialog_press(int(TALK_KEYS[key]))
				return true
	return super.handle_input_event(event)

func start_play() -> void:
	if level_script != null and level_script.button_pressed and real != null and real.current >= 0:
		if not controller.get_play_capabilities().get("level_script_enabled", false):
			attach_level_script()
	super.start_play()
	# A help box the level entry opened pauses the offline game at once.
	if controller != null and controller.get_play_capabilities().get("level_script_enabled", false):
		controller.help_frame(false)

func _observe(result: Dictionary) -> void:
	super._observe(result)
	if real == null: return
	if not _level_change_loading and controller != null:
		_refresh_mission_menu()
		var pending: Dictionary = controller.script_level_request()
		if pending.get("queued", false) and str(pending.get("name", "")) != _refused_level_request:
			_queue_level_change()
	# The preview rebuilds its mesh when it samples a new tick.
	var appearance_expired := not imported_record.is_empty() and not _imported_record_matches(raw_observation)
	if appearance_expired: _clear_imported_appearance()
	if real.current >= 0 and raw_observation.get("player_ready", false):
		if appearance_expired or real_player.model_index != int(raw_observation.get("player_model_index", -1)):
			_install_player_model()
		if real_player.body_drawn and asset_label.text != real_player.status: asset_label.text = real_player.status
	# Creature preview playback remains separate from the static player rest pose.
	if real.creature != null and components.has("animation") and components.animation.active:
		real.creature.apply(components.animation.mesh_instance)
	_present_npc_dialog()
	if hub_services != null: hub_services.refresh()
	if real.world == null: return
	real.world.observe(raw_observation)
	var extra: String = real.world.label()
	if not extra.is_empty() and not extra in terrain_label.text: terrain_label.text += " · " + extra

func _present_npc_dialog() -> void:
	if controller == null: return
	var talk: Dictionary = controller.dialog_status()
	if not talk.get("ok", false): return
	if frontend != null: frontend.set_dialog_open(bool(talk.get("open", false)))
	if talk.get("stalled", false):
		status_label.text = "Dialog stopped: %s. \"Leave stalled dialog\" ends it (not in the original)." % str(talk.get("stall_reason", ""))
	elif talk.get("open", false):
		status_label.text = "Talking · Up / Down choose · Enter confirms"
	elif talk.get("mutex_held", false):
		status_label.text = "Talking is blocked: the menu lock is held (offline the inventory's close frees it). \"Leave stalled dialog\" frees it."
	elif not str(talk.get("last_use", "")).is_empty():
		status_label.text = "Use: " + str(talk.last_use)

func leave_stalled_dialog() -> void:
	if controller == null: return
	var result: Dictionary = controller.dialog_leave()
	status_label.text = "Stalled dialog or held menu lock released (client action, not in the original)." if result.get("ok", false) else "Nothing to release."

func load_real_creature(index: int) -> bool:
	var archives := creature_archives()
	if index < 0 or index >= archives.size(): return false
	var result: Dictionary = real.load_creature(archives[index])
	status_label.text = "Creature preview: explicit choice, pairing unverified." if result.ok else "No model/clip in that archive was accepted."
	return result.ok

# Explicit choice: the original HUD's font is not documented.
func load_real_font(index: int) -> bool:
	if index < 0: return false
	var ok: bool = real.load_font(index)
	status_label.text = "HUD font page decoded (explicit choice, pairing unverified)." if ok else "That font row was refused; previous font kept."
	return ok

# The user's per-level light records (tools/fx_level_lights.py --json). Each
# game-folder level then replaces the synthetic sun with its own record when
# play starts; the file is validated now so a bad one is refused at once.
func load_level_lights(path: String) -> Dictionary:
	var result := {"ok": false}
	var file := FileAccess.open(path, FileAccess.READ)
	if file != null and ClassDB.class_exists("ChampionsLevelLights"):
		var service = ClassDB.instantiate("ChampionsLevelLights")
		result = service.load_json(file.get_as_text())
	if result.get("ok", false):
		real.level_lights_path = path
		if fx != null: fx.level_light_path = path
		status_label.text = "Level light records loaded; each level uses its own record in play."
	else:
		status_label.text = "Level light JSON refused: " + str(result.get("error", "unreadable"))
	return result

func load_recipe_constants(path: String) -> Dictionary:
	var text := FileAccess.get_file_as_string(path)
	var result: Dictionary = controller.set_effect_recipe_constants_json(text, "user executable recipe constants")
	status_label.text = "Skill recipe constants loaded." if result.get("ok", false) else "Skill recipe constants refused: " + str(result.get("error", ""))
	refresh_capabilities()
	return result
