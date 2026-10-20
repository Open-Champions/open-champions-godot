extends "res://scenes/play_flow.gd"

# Play flow plus a run-time game-folder source. The user picks their own
# read-only RTA folder; levels, textures, banks, effect sheets and creature
# archives are read from it at run time and never become project resources.
const RealData = preload("res://real_data/real_data.gd")
var real: Node
var real_folder: FileDialog
var real_recipes: FileDialog
var real_level_choice: OptionButton
var real_creature_choice: OptionButton

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
	_add_action("real_folder", "Game folder…", func(): open_dialog(real_folder))
	_add_action("real_recipes", "Skill recipe JSON…", func(): open_dialog(real_recipes))
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
	status_label.text = "Game-folder level loaded with its textures. Start play for lighting, music and effects."
	return true

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

func load_recipe_constants(path: String) -> Dictionary:
	var text := FileAccess.get_file_as_string(path)
	var result: Dictionary = controller.set_effect_recipe_constants_json(text, "user executable recipe constants")
	status_label.text = "Skill recipe constants loaded." if result.get("ok", false) else "Skill recipe constants refused: " + str(result.get("error", ""))
	refresh_capabilities()
	return result
