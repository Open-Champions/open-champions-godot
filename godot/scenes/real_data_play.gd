extends "res://scenes/play_flow.gd"

# Play flow plus a run-time game-folder source. The user picks their own
# read-only RTA folder; levels, textures, banks, effect sheets and creature
# archives are read from it at run time and never become project resources.
const RealData = preload("res://real_data/real_data.gd")
const RealText = preload("res://real_data/real_text.gd")
const LoadingDisplay = preload("res://real_data/loading_display.gd")
signal loading_started
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
var real_exit_choice: OptionButton
var mission_menu_panel: PanelContainer
var mission_menu_status: Label
var mission_first_story: Button
var loading_layer: CanvasLayer
var loading_display: Control
var _level_change_queued := false
var _level_change_loading := false
var _refused_level_request := ""
var last_campaign_carried := false
var last_campaign_image_restored := false
var hud_art_counts: Dictionary = {}
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
	real_exit_choice = OptionButton.new()
	real_exit_choice.add_item("No level exit loaded", -1)
	menu.add_child(real_exit_choice)
	_add_action("real_exit", "Use selected level action", activate_selected_exit)
	mission_menu_panel = PanelContainer.new()
	mission_menu_panel.set_anchors_preset(Control.PRESET_CENTER)
	mission_menu_panel.custom_minimum_size = Vector2(380, 160)
	mission_menu_panel.offset_left = -190
	mission_menu_panel.offset_top = -80
	mission_menu_panel.offset_right = 190
	mission_menu_panel.offset_bottom = 80
	mission_menu_panel.visible = false
	var mission_box := VBoxContainer.new()
	mission_menu_panel.add_child(mission_box)
	var mission_title := Label.new()
	mission_title.text = "Mission menu · host layout"
	mission_box.add_child(mission_title)
	mission_menu_status = Label.new()
	mission_box.add_child(mission_menu_status)
	mission_first_story = Button.new()
	mission_first_story.text = "Mission 1 · Story"
	mission_first_story.pressed.connect(mission_menu_select_first_story)
	mission_box.add_child(mission_first_story)
	if viewport != null: viewport.add_child(mission_menu_panel)
	else: add_child(mission_menu_panel)
	loading_layer = CanvasLayer.new()
	loading_layer.layer = 100
	add_child(loading_layer)
	loading_display = LoadingDisplay.new()
	loading_display.set_anchors_preset(Control.PRESET_FULL_RECT)
	loading_display.mouse_filter = Control.MOUSE_FILTER_STOP
	loading_display.visible = false
	loading_layer.add_child(loading_display)
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
		mode_label.text = "Play mode · level script actions can change real levels; combat and other campaign paths remain incomplete."

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

func load_real_level(index: int, carry_campaign := false, previous := "", entrance := "", qualified := "") -> bool:
	if index < 0: return false
	var result: Dictionary = real.load_level(index, previous, entrance, qualified)
	if not result.ok:
		status_label.text = "Level refused at %s. %s" % [result.stage, str(result.get("error", ""))]
		return false
	var scripted := false
	if level_script != null and level_script.button_pressed: scripted = attach_level_script(carry_campaign)
	else: controller.disable_level_script()
	_refresh_exit_targets()
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
	var result: Dictionary = controller.enable_level_script(script, objects, carry_campaign, real.dialog_data(real.current), volume_bits)
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
		var suffix := " · script action" if target.get("script_action", false) else " · arrival/prompt only"
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

func mission_menu_select_first_story() -> bool:
	var menu_state: Dictionary = controller.script_mission_menu_state()
	if not menu_state.get("ok", false) or not menu_state.get("open", false) or not menu_state.get("first_story_available", false):
		status_label.text = "Mission selection refused: mission 1 is not available."
		return false
	# The native state machine supplies the row, availability gate, fade and
	# direct pending-world write. The host button submits its documented pad.
	for i in range(17):
		if int(menu_state.get("state", -1)) == 2: break
		menu_state = controller.script_mission_menu_tick(0, 0)
		if not menu_state.get("ok", false): return false
	if int(menu_state.get("state", -1)) != 2 or int(menu_state.get("row", -1)) != 0 or int(menu_state.get("column", -1)) != 0:
		status_label.text = "Mission selection refused: the first story row is not selected."
		return false
	menu_state = controller.script_mission_menu_tick(0x40, 0)
	for i in range(4):
		if not menu_state.get("ok", false): return false
		menu_state = controller.script_mission_menu_tick(0, 128)
	if not menu_state.get("launched", false) or str(menu_state.get("pending", "")) != "blood1":
		status_label.text = "Mission selection refused: the documented first story request was not produced."
		return false
	mission_menu_panel.visible = false
	_queue_level_change()
	return true

func _refresh_mission_menu() -> void:
	if mission_menu_panel == null or controller == null: return
	var state: Dictionary = controller.script_mission_menu_state()
	mission_menu_panel.visible = bool(state.get("ok", false) and state.get("open", false))
	if mission_menu_panel.visible:
		mission_first_story.disabled = not bool(state.get("first_story_available", false))
		mission_menu_status.text = "Mission 1 is available." if not mission_first_story.disabled else "Mission 1 is locked."

func _queue_level_change() -> void:
	if _level_change_queued or _level_change_loading: return
	_level_change_queued = true
	call_deferred("_apply_level_change")

func _wait_loading_present() -> void:
	if DisplayServer.get_name() == "headless": await get_tree().process_frame
	else: await RenderingServer.frame_post_draw

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
	var old_automatic := automatic_steps
	automatic_steps = false
	release_controls()
	var art_ready: bool = loading_display.configure(real, inspector, base)
	if not art_ready:
		status_label.text = "Loading art refused: " + loading_display.refusal
		_refused_level_request = requested
		automatic_steps = old_automatic
		_level_change_loading = false
		return false
	# The start slice makes three dark submissions before the animated display.
	for i in range(3): await _wait_loading_present()
	loading_display.phase = "loading"
	loading_display.queue_redraw()
	loading_started.emit()
	# The session owns input while the display and ten idle-stream presents run.
	# Loading itself is synchronous in this client; frames around it remain
	# observable, while actual concurrent renderer-thread timing is unverified.
	await _wait_loading_present()
	var was_playing: bool = play_mode and session_started and not stopped
	var changed := load_real_level(destination, true, real.current_qualified,
		str(pending.get("entrance", "")), requested)
	if changed and was_playing: start_play()
	if changed:
		for i in range(LoadingDisplay.WARMUP_PRESENTS): await _wait_loading_present()
	else:
		loading_display.visible = false
		loading_display.phase = "hidden"
	if not changed: _refused_level_request = requested
	if changed:
		loading_display.phase = "blank"
		loading_display.queue_redraw()
		for i in range(LoadingDisplay.BLANK_FIELDS): await _wait_loading_present()
		loading_display.begin_fade()
		# The first normal frame and control opportunity follow the blank fields.
		# The colour offset recovers on subsequent presents, not simulation ticks.
	automatic_steps = old_automatic
	_level_change_loading = false
	return changed

# The use button and the dialog's pad words, taken at the next ready tick.
func handle_input_event(event: InputEvent) -> bool:
	if _level_change_loading: return true
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
	if not _level_change_loading and controller != null:
		_refresh_mission_menu()
		var pending: Dictionary = controller.script_level_request()
		if pending.get("queued", false) and str(pending.get("name", "")) != _refused_level_request:
			_queue_level_change()
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
