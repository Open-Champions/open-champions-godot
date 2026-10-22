extends "res://scenes/play_flow.gd"

# Play flow plus a run-time game-folder source. The user picks their own
# read-only RTA folder; levels, textures, banks, effect sheets and creature
# archives are read from it at run time and never become project resources.
const RealData = preload("res://real_data/real_data.gd")
const RealText = preload("res://real_data/real_text.gd")
const TextHud = preload("res://ui/text_hud.gd")
var real: Node
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
var hud_art_counts: Dictionary = {}
var hud_extras_counts: Dictionary = {}
var hud_font_loaded := false
const NpcDialogView = preload("res://ui/npc_dialog_view.gd")
var npc_dialog: Control
# Talk to an NPC: E (use), Up / Down (choice), Enter or Space (confirm).
const TALK_KEYS := {KEY_E: "use", KEY_UP: 0x1000, KEY_DOWN: 0x4000, KEY_ENTER: 0x40, KEY_KP_ENTER: 0x40, KEY_SPACE: 0x40}

func _ready() -> void:
	super._ready()
	if controller == null: return
	real = RealData.new()
	add_child(real)
	real.configure(self, "user://real-data-cache")
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
	for argument in OS.get_cmdline_user_args():
		if argument == "--level-script": level_script.set_pressed_no_signal(true)
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--skill-recipes="): load_recipe_constants(argument.trim_prefix("--skill-recipes="))
		elif argument.begins_with("--level-lights="): load_level_lights(argument.trim_prefix("--level-lights="))
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="): open_game_folder(argument.trim_prefix("--game-folder="))

func open_game_folder(path: String) -> bool:
	if not real.open_disc(path):
		status_label.text = "Game folder refused: " + real.last_error
		return false
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
	status_label.text = "Game folder opened read-only: %d levels. Choose a level." % real.levels.size()
	if npc_dialog != null:
		npc_dialog.set_font(real.dialog_font())
		npc_dialog.set_box(real.dialog_box())
	return true

# Loose LMPs that are not a level name; each is only an explicit candidate.
func creature_archives() -> PackedStringArray:
	var out := PackedStringArray()
	var stems := {}
	for level in real.levels: stems[level.stem] = true
	for name in real.files:
		if name.get_extension() == "lmp" and not stems.has(name.get_basename()): out.append(real.files[name])
	out.sort()
	return out

func load_real_level(index: int) -> bool:
	if index < 0: return false
	var result: Dictionary = real.load_level(index)
	if not result.ok:
		status_label.text = "Level refused at %s. %s" % [result.stage, str(result.get("error", ""))]
		return false
	if displayed_asset_name == "Practice figure":
		asset_label.text = "Practice figure · class model resource mapping undocumented"
	var scripted := false
	if level_script != null and level_script.button_pressed: scripted = attach_level_script()
	else: controller.disable_level_script()
	status_label.text = "Game-folder level loaded with its textures. Start play for lighting, music and effects."
	if level_script != null and level_script.button_pressed:
		status_label.text += " Level script running." if scripted else " Level script refused."
	return true

func restore_default_actor() -> void:
	super.restore_default_actor()
	if real != null and real.current >= 0:
		asset_label.text = "Practice figure · class model resource mapping undocumented"

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

func attach_level_script() -> bool:
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
	var result: Dictionary = controller.enable_level_script(script, objects, false, real.dialog_data(real.current), volume_bits, str(level.stem), balance, world)
	status_label.text = "Level script running." if result.get("ok", false) else "Level script refused."
	return result.get("ok", false)

# The use button and the dialog's pad words, taken at the next ready tick.
func handle_input_event(event: InputEvent) -> bool:
	# The offline help box owns confirmation while it holds the session. A
	# simultaneous dialog or game-menu handler must not consume that key.
	if controller != null and controller.text_hud().get("help_open", false) and event is InputEventKey:
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
	# The preview rebuilds its mesh when it samples a new tick.
	if real.creature != null and components.has("animation") and components.animation.active:
		real.creature.apply(components.animation.mesh_instance)
	_present_npc_dialog()
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
