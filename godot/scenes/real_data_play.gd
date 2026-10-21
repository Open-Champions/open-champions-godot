extends "res://scenes/play_flow.gd"

# Play flow plus a run-time game-folder source. The user picks their own
# read-only RTA folder; levels, textures, banks, effect sheets and creature
# archives are read from it at run time and never become project resources.
const RealData = preload("res://real_data/real_data.gd")
var real: Node
var real_folder: FileDialog
var real_recipes: FileDialog
var real_lights: FileDialog
var real_level_choice: OptionButton
var real_creature_choice: OptionButton
var real_font_choice: OptionButton
var level_script: CheckButton

func _ready() -> void:
	super._ready()
	if controller == null: return
	real = RealData.new()
	add_child(real)
	real.configure(self, "user://real-data-cache")
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
	status_label.text = "Game folder opened read-only: %d levels. Choose a level." % real.levels.size()
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
	var scripted := false
	if level_script != null and level_script.button_pressed: scripted = attach_level_script()
	else: controller.disable_level_script()
	status_label.text = "Game-folder level loaded with its textures. Start play for lighting, music and effects."
	if level_script != null and level_script.button_pressed:
		status_label.text += " Level script running." if scripted else " Level script refused."
	return true

func _level_script_toggled(on: bool) -> void:
	if real == null or real.current < 0: return
	if on: attach_level_script()
	else: controller.disable_level_script()

# The level's script.scr and objects.ob. Refuses an empty image and does not
# change the play observation. Entry names are the documented loader names.
func attach_level_script() -> bool:
	if real == null or real.current < 0 or controller == null: return false
	var level: Dictionary = real.levels[real.current]
	var script: PackedByteArray = real.read_entry(level.gob, level.stem + ".lmp", "script.scr")
	var objects: PackedByteArray = real.read_entry(level.gob, level.stem + ".lmp", "objects.ob")
	var result: Dictionary = controller.enable_level_script(script, objects)
	status_label.text = "Level script running." if result.get("ok", false) else "Level script refused."
	return result.get("ok", false)

func start_play() -> void:
	if level_script != null and level_script.button_pressed and real != null and real.current >= 0:
		if not controller.get_play_capabilities().get("level_script_enabled", false):
			attach_level_script()
	super.start_play()

func _observe(result: Dictionary) -> void:
	super._observe(result)
	if real == null: return
	# The preview rebuilds its mesh when it samples a new tick.
	if real.creature != null and components.has("animation") and components.animation.active:
		real.creature.apply(components.animation.mesh_instance)
	if real.world == null: return
	real.world.observe(raw_observation)
	var extra: String = real.world.label()
	if not extra.is_empty() and not extra in terrain_label.text: terrain_label.text += " · " + extra

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
