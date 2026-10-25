extends "res://scenes/session_sandbox.gd"

# Authored host navigation. All simulation, limits and save semantics remain native.
enum Flow { SELECTION, RUNNING, PAUSED }
var audio: Node
var fx: Node3D
var level_textures: RefCounted
var level_tex_dialog: FileDialog
var fx_status: Label
var fx_definitions: FileDialog
var gore_choice: OptionButton
var flow := Flow.SELECTION
var session_started := false
var play_mode := false
var animated_choice: CheckButton
var cosmetic_active := false
var combat_choice: CheckButton
var full_choice: CheckButton
var effect_constants_refusal := ""
var combat_displays: Dictionary = {}
var combat_status: Label
var creature_choice: OptionButton
var level_choice: CheckButton
var level_status: Label
var gate_display: MeshInstance3D
var level_generation := -1
var mode_known := true
var save_methods: Array[String] = []
var save_capability_error := ""
var menu: VBoxContainer
var menu_title: Label
var mode_label: Label
var capability_label: Label
var menu_actions: HFlowContainer
var actions: Dictionary = {}
var debug_toggle: CheckButton
var play_capabilities: Dictionary = {}
const COMPONENTS := {
	"library": {"path": "res://scenes/level_library.tscn", "methods": ["configure", "show_library"], "signal": "level_loaded"},
	"animation": {"path": "res://scenes/scene_animation_preview.tscn", "methods": ["configure", "show_picker", "observe", "set_suspended", "clear_preview"], "signal": "preview_changed"},
	"art": {"path": "res://ui/original_art/original_art_panel.tscn", "methods": ["configure", "show_picker", "update_observation", "clear_art"], "signal": "art_ready"},
}
# Tests may inject isolated Controls before _ready. Production always resolves
# only the three frozen resource paths, never arbitrary persisted paths.
var component_factory: Callable
var components: Dictionary = {}
var component_reasons: Dictionary = {}
var art_region: VBoxContainer
var art_state: Label
var cosmetic_label: Label
var frame_settings: Window
var scene_scroll: ScrollContainer
var hud_round3: Control
var game_menus: Control
# Boot screens, front end, character creation, options, load/save and the game
# pause menu (ui/frontend_menus.gd over the native ChampionsFrontendService).
var frontend: Control
# PS2 memory-card character import (native ps2_save). Held apart from the
# movement session: the import fills the native player record, inventory
# words and quest table that the script and item natives read.
var ps2_import: RefCounted
var ps2_dialog: FileDialog
var ps2_status: Label
var play_hud: Control


func _ready() -> void:
	# Preserve both exported legacy routes on their original scene.
	if "--sandbox-smoke-test" in OS.get_cmdline_user_args() or "--browser-smoke-test" in OS.get_cmdline_user_args():
		get_tree().change_scene_to_file.call_deferred("res://scenes/session_sandbox.tscn")
		return
	super._ready()
	_build_flow()
	_attach_components()
	_attach_fx()
	_attach_play_hud()
	_attach_hud_round3()
	_attach_game_menus()
	_attach_level_textures()
	_attach_audio()
	_attach_ps2_import()
	_attach_frontend()
	refresh_capabilities()
	var frame_rate := get_node_or_null("/root/FrameRate")
	if frame_rate != null:
		frame_settings = frame_rate.get("settings_window")
		if frame_settings != null:
			frame_settings.visibility_changed.connect(_frame_settings_changed)
	set_paused(true)
	_show_menu(Flow.SELECTION)
	if controller != null:
		status_label.text = "Choose a local WORLD, or Start diagnostic on the practice ground."

func _build_flow() -> void:
	# Scroll the authored diagnostic region on short/narrow windows. No camera
	# transform or native coordinate is derived from menu layout.
	var parent := main_layout.get_parent()
	parent.remove_child(main_layout)
	var scroll := ScrollContainer.new()
	scene_scroll = scroll
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	parent.add_child(scroll)
	scroll.add_child(main_layout)
	main_layout.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	viewport.get_parent().custom_minimum_size.y = 220
	resized.connect(_frame_layout)
	diagnostic_controls.hide()
	cache_controls.hide()
	menu = VBoxContainer.new()
	main_layout.add_child(menu)
	main_layout.move_child(menu, 1)
	menu_title = _label("Choose a local level or diagnostic ground", 20, GOLD)
	menu_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	menu.add_child(menu_title)
	mode_label = _label("", 14, MUTED)
	mode_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	main_layout.add_child(mode_label)
	main_layout.move_child(mode_label, 1)
	capability_label = _label("", 13, MUTED)
	capability_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	menu.add_child(capability_label)
	menu_actions = HFlowContainer.new()
	menu_actions.add_theme_constant_override("h_separation", 8)
	menu.add_child(menu_actions)
	_add_action("library", "Level library…", func(): open_component("library"))
	_add_action("animation", "Cosmetic model / clip…", func(): open_component("animation"))
	_add_action("art", "Original art…", func(): open_component("art"))
	_add_action("inventory", "Inventory / equipment…", func():
		if game_menus != null: game_menus.open_screen("inventory"))
	_add_action("inventory_pictures", "Inspect inventory pictures…", func():
		set_paused(true)
		if components.has("art") and components.art.has_method("show_inventory"): components.art.show_inventory())
	_add_action("terrain", "Open local WORLD…", func(): open_dialog(terrain_dialog))
	# The real-data scene reads a folder the user picks at run time; the
	# export contains no game data (docs/BUILDS.md).
	_add_action("game_folder", "Your game folder…", func(): get_tree().change_scene_to_file("res://scenes/real_data_play.tscn"))
	combat_choice = CheckButton.new()
	combat_choice.text = "Combat fixture: player + two Skelights (no movement)"
	menu.add_child(combat_choice)
	animated_choice = CheckButton.new()
	animated_choice.text = "Animated fixture: Agony dispatch + synthetic combat carriers (90 ticks)"
	menu.add_child(animated_choice)
	full_choice = CheckButton.new()
	full_choice.text = "Full combat fixture: J attack, K block, L Holy Strike, U Disease Bolt"
	menu.add_child(full_choice)
	animated_choice.toggled.connect(func(on):
		if on:
			combat_choice.button_pressed = true
			full_choice.button_pressed = false
			level_choice.button_pressed = false)
	full_choice.toggled.connect(func(on):
		if on:
			combat_choice.button_pressed = true
			animated_choice.button_pressed = false
			level_choice.button_pressed = false)
	level_choice = CheckButton.new()
	level_choice.text = "Level fixture: 180-tick gate trip + saved-level return"
	menu.add_child(level_choice)
	level_choice.toggled.connect(func(on):
		if on:
			combat_choice.button_pressed = false
			animated_choice.button_pressed = false
			full_choice.button_pressed = false)
	combat_choice.toggled.connect(func(on):
		if on: level_choice.button_pressed = false
		else:
			animated_choice.button_pressed = false
			full_choice.button_pressed = false)
	_add_action("play", "Start play session", start_play)
	_add_action("diagnostic", "Start diagnostic", start_diagnostic)
	_add_action("resume", "Resume", resume_session)
	_add_action("reset", "Reset current level", reset_session)
	_add_action("save", "Save game…", open_in_game_save)
	_add_action("load", "Load…", func(): open_dialog(load_dialog))
	# Reuse the exact legacy save-format selector and its IDs.
	save_format.reparent(menu_actions)
	_add_action("selection", "Return to selection", return_to_selection)
	_add_action("appearance", "Static appearance…", func(): open_dialog(asset_dialog))
	_add_action("archive", "Archive tools", open_archive_browser)
	debug_toggle = CheckButton.new()
	debug_toggle.text = "Show diagnostic controls (restarts are explicit)"
	debug_toggle.toggled.connect(func(value):
		diagnostic_controls.visible = value
		cache_controls.visible = value)
	menu.add_child(debug_toggle)
	var bar := HFlowContainer.new()
	main_layout.add_child(bar)
	main_layout.move_child(bar, 2)
	actions["pause"] = _button("Pause / menu", func(): set_paused(true))
	bar.add_child(actions.pause)
	actions["combat_hit"] = _button("Fixture: player hit event", func():
		if controller != null:
			var result: Dictionary = controller.combat_hit_event(0, 1)
			if not result.get("ok", false): status_label.text = str(result.get("error")))
	bar.add_child(actions.combat_hit)
	# Full fixture pad buttons (keys J / K held / L / U). Masks are the client's
	# controller mapping; the executed Player_run decides swings and blocks.
	actions["combat_attack"] = _button("Attack (J)", func(): pass)
	actions.combat_attack.button_down.connect(func(): _combat_call("combat_attack", [true]))
	actions.combat_attack.button_up.connect(func(): _combat_call("combat_release_attack", []))
	bar.add_child(actions.combat_attack)
	actions["combat_block"] = _button("Hold block (K)", func(): pass)
	actions.combat_block.toggle_mode = true
	actions.combat_block.toggled.connect(func(on): _combat_call("combat_block", [on]))
	bar.add_child(actions.combat_block)
	actions["combat_cast"] = _button("Holy Strike (L)", func(): _combat_call("combat_cast_holy_strike", []))
	bar.add_child(actions.combat_cast)
	actions["combat_spell_select"] = _button("Select Disease Bolt", _select_disease_bolt)
	bar.add_child(actions.combat_spell_select)
	actions["combat_spell_cast"] = _button("Cast Disease Bolt (U)", func(): pass)
	actions.combat_spell_cast.button_down.connect(func(): _combat_call("combat_press_disease_bolt", [true]))
	actions.combat_spell_cast.button_up.connect(func(): _combat_call("combat_release_disease_bolt", []))
	bar.add_child(actions.combat_spell_cast)
	# Wave 12: authored Full-fixture set-up that reaches merged executed bodies:
	# one own-machine creature (creature-ai) at (0, 80), thrown potions 1..3
	# at the player (combat-runtime-4). Placement is the fixture's, not spawn data.
	creature_choice = OptionButton.new()
	creature_choice.add_item("Add own-machine creature…", -1)
	if ClassDB.class_exists("ChampionsSessionController"):
		var classes: PackedStringArray = ClassDB.class_call_static("ChampionsSessionController", "own_creature_classes")
		for i in range(classes.size()): creature_choice.add_item(classes[i], i)
	creature_choice.item_selected.connect(func(index):
		var id := creature_choice.get_item_id(index)
		creature_choice.select(0)
		if id >= 0: _combat_call("combat_spawn_creature", [id]))
	bar.add_child(creature_choice)
	for kind in [1, 2, 3]:
		var potion := _button("Potion %d" % kind, func(): _combat_call("combat_drop_potion", [kind]))
		actions["combat_potion_%d" % kind] = potion
		bar.add_child(potion)
	combat_status = _label("", 13, MUTED)
	combat_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	main_layout.add_child(combat_status)
	for spec in [["gate_open", "Open gate", "level_gate_open", ""], ["gate_travel", "Travel to aab", "level_gate_confirm", "aab"], ["level_return", "Return to saved aa", "level_request", "aa"]]:
		var action := _button(spec[1], func():
			var result: Dictionary = controller.call(spec[2]) if spec[3] == "" else controller.call(spec[2], spec[3])
			_observe(result))
		actions[spec[0]] = action
		bar.add_child(action)
	level_status = _label("", 13, MUTED)
	main_layout.add_child(level_status)
	cosmetic_label = _label("Cosmetic model / clip pairing unverified.", 12, MUTED)
	main_layout.add_child(cosmetic_label)
	art_region = VBoxContainer.new()
	main_layout.add_child(art_region)
	art_state = _label("Original art not loaded · authored diagnostic placement", 12, MUTED)
	art_region.add_child(art_state)
	main_layout.move_child(status_label, main_layout.get_children().find(viewport.get_parent()))
	for dialog in [save_dialog, load_dialog, terrain_dialog, asset_dialog, model_dialog]:
		dialog.canceled.connect(_dialog_cancelled)

func _add_action(id: String, title: String, action: Callable) -> void:
	var button := _button(title, action)
	menu_actions.add_child(button)
	actions[id] = button

func refresh_capabilities() -> void:
	play_capabilities = {}
	if controller != null and controller.has_method("get_play_capabilities") and controller.has_method("start_play_session"):
		var result = controller.call("get_play_capabilities")
		if result is Dictionary:
			play_capabilities = result
	if menu == null:
		return
	actions.play.disabled = not play_capabilities.get("available", false)
	actions.play.tooltip_text = ("Disable recovery, facing and animation-cache diagnostics to start play." if not play_capabilities.is_empty() else "Install the session-play-runtime component to enable this action.") if actions.play.disabled else "Start a fresh native play session on the selected terrain."
	capability_label.text = actions.play.tooltip_text if actions.play.disabled else "Play available · recent history retained; continuation saves resume current state."
	if not effect_constants_refusal.is_empty() and not play_capabilities.get("full_fixture_available", false):
		capability_label.text += " Full combat unavailable: " + effect_constants_refusal
	if raw_observation.get("level_enabled", false):
		capability_label.text = "Level fixture selected · in-memory saved levels; fixture continuation disk saves."
	elif raw_observation.get("combat_enabled", false):
		capability_label.text = "Animated fixture selected · supported dispatch on synthetic carriers; fixture continuation saves." if raw_observation.get("animated_enabled", false) else "Combat fixture selected · one player and two Skelights; fixture continuation saves."
		if raw_observation.get("full_enabled", false): capability_label.text = "Full combat fixture selected · Composition::Full player and two Skelights; native effects; fixture continuation saves."
	for id in ["terrain", "diagnostic", "reset", "save", "load", "resume", "pause"]:
		actions[id].disabled = controller == null
		if controller == null:
			actions[id].tooltip_text = "Install the native ChampionsSessionController extension."
	actions.appearance.disabled = inspector == null
	actions.appearance.tooltip_text = "Install ChampionsArchiveInspector to select a static appearance." if inspector == null else "Explicit cosmetic appearance only."
	for id in COMPONENTS:
		actions[id].disabled = not components.has(id) or (controller == null if id == "library" else inspector == null)
		actions[id].tooltip_text = str(component_reasons.get(id, ""))
		if components.has(id) and actions[id].disabled:
			actions[id].tooltip_text = "Install the required native controller/inspector extension."
	actions.inventory.disabled = game_menus == null or game_menus.service == null
	actions.inventory_pictures.disabled = inspector == null or not components.has("art") or not components.art.has_method("show_inventory")
	_configure_save_formats()
	_update_menu()

func _update_menu() -> void:
	if menu == null:
		return
	mode_label.text = "Play mode · authored dry movement; original game progression and combat are unimplemented." if play_mode else "Bounded diagnostic mode · %s ready ticks maximum; original game is unimplemented." % raw_observation.get("ready_tick_limit", 512)
	var combat: bool = raw_observation.get("combat_enabled", false)
	animated_choice.disabled = flow != Flow.SELECTION or not play_capabilities.get("animated_fixture_available", false)
	combat_choice.disabled = flow != Flow.SELECTION or not play_capabilities.get("combat_fixture_available", false)
	full_choice.disabled = flow != Flow.SELECTION or not play_capabilities.get("full_fixture_available", false)
	full_choice.tooltip_text = effect_constants_refusal if not effect_constants_refusal.is_empty() else ("Load effect recipe constants (fx_skill_recipes JSON) to enable the Full fixture." if full_choice.disabled and flow == Flow.SELECTION else "")
	var level: bool = raw_observation.get("level_enabled", false)
	level_choice.disabled = flow != Flow.SELECTION or not play_capabilities.get("level_fixture_available", false)
	for id in ["gate_open", "gate_travel", "level_return"]: actions[id].visible = level
	actions.gate_open.disabled = stopped or paused or raw_observation.get("gate_travel_ticks", 0) > 0
	actions.gate_travel.disabled = stopped or not raw_observation.get("gate_menu_open", false)
	actions.level_return.disabled = stopped or paused or raw_observation.get("level_name", "") != "aab" or raw_observation.get("gate_travel_ticks", 0) > 0
	if level: mode_label.text = "Level fixture · native loading / gates / saved-level state. No movement or disk saves."
	var full: bool = combat and raw_observation.get("full_enabled", false)
	actions.combat_hit.visible = combat and not full
	actions.combat_hit.disabled = stopped or paused or raw_observation.get("animated_enabled", false)
	for id in ["combat_attack", "combat_block", "combat_cast", "combat_spell_select", "combat_spell_cast", "combat_potion_1", "combat_potion_2", "combat_potion_3"]:
		actions[id].visible = full
		actions[id].disabled = stopped or paused
	if full:
		actions.combat_spell_select.disabled = stopped or paused or raw_observation.get("combat_spell_selected", false)
		actions.combat_spell_cast.disabled = stopped or paused or not raw_observation.get("combat_spell_selected", false)
	if creature_choice != null:
		creature_choice.visible = full
		var spawned := false
		for row in raw_observation.get("combat_actors", []):
			spawned = spawned or int(row.get("slot", -1)) == 3
		creature_choice.disabled = stopped or paused or spawned
	# Pause/stop release the native pad (release_input); mirror it on the toggle.
	if paused or stopped or not full: actions.combat_block.set_pressed_no_signal(false)
	if combat:
		mode_label.text = "Combat fixture · native player / Skelights; synthetic animation events. Movement and action selection unavailable; fixture continuation saves."
	if raw_observation.get("animated_enabled", false):
		mode_label.text = "Animated fixture · real Agony dispatch / player idle; authored meshes and combat carriers. No player action timing or saves."
	if full:
		mode_label.text = "Full combat fixture · executed Player_move / Player_run: WASD stick, J attack / combo, K hold block, L Holy Strike, U selected Disease Bolt. Synthetic clips, services and effect constants; fixture continuation saves (with the effect world)."
	if not mode_known:
		mode_label.text = "Unknown native session mode · install a matching runtime before saving."
	actions.save.disabled = controller == null or save_methods.is_empty() or not mode_known
	actions.save.tooltip_text = save_capability_error
	actions.resume.disabled = controller == null or not session_started or stopped
	actions.resume.visible = session_started
	actions.selection.visible = flow != Flow.SELECTION
	actions.diagnostic.visible = not session_started and not play_mode and mode_known
	actions.play.visible = flow == Flow.SELECTION
	actions.play.text = "Start fresh play session" if session_started else "Start play session"
	debug_toggle.disabled = play_mode or controller == null or not mode_known
	debug_toggle.tooltip_text = "Diagnostic switches require a known diagnostic native session." if debug_toggle.disabled else "Explicit diagnostic switches restart the session."
	if debug_toggle.disabled:
		debug_toggle.button_pressed = false

func _show_menu(state: Flow) -> void:
	flow = state
	if menu == null:
		return
	menu.visible = state != Flow.RUNNING and not _component_modal_open()
	menu_title.text = "Local level selection" if state == Flow.SELECTION else ("Session stopped" if stopped else "Paused session")
	_update_menu()
	if menu.visible:
		_focus_menu.call_deferred()
	else:
		var focused := get_viewport().gui_get_focus_owner()
		if focused != null:
			focused.release_focus()
		diagnostic_controls.hide()
		cache_controls.hide()
		debug_toggle.set_pressed_no_signal(false)
	_frame_layout()

func set_paused(value: bool) -> void:
	super.set_paused(value)
	if menu != null:
		_show_menu(Flow.SELECTION if not session_started else (Flow.PAUSED if paused or stopped else Flow.RUNNING))
		if not session_started and paused and not stopped:
			status_label.text = "Choose a local WORLD or an available session mode to start."

func start_diagnostic() -> void:
	if controller == null or session_started or play_mode or not mode_known:
		return
	session_started = true
	play_mode = false
	mode_known = true
	_configure_save_formats()
	set_paused(false)

func start_play() -> void:
	if controller == null or not play_capabilities.get("available", false):
		return
	var result: Dictionary
	if level_choice.button_pressed:
		result = controller.call("start_play_session", false, true)
	elif combat_choice.button_pressed and full_choice.button_pressed:
		result = controller.call("start_play_session", true, false, false, true)
	elif combat_choice.button_pressed:
		result = controller.call("start_play_session", true, false, true) if animated_choice.button_pressed else controller.call("start_play_session", true)
	else:
		result = controller.call("start_play_session")
	if not result.get("ok", false):
		status_label.text = str(result.get("error", "Native play start refused."))
		return
	_clear_preview()
	play_mode = true
	mode_known = true
	session_started = true
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	_observe(result)
	if audio != null: audio.reset(true,int(raw_observation.ready_ticks))
	set_paused(false)
	refresh_capabilities()

func resume_session() -> void:
	if session_started and not stopped and not _component_modal_open():
		set_paused(false)

func return_to_selection() -> void:
	# Retain the live session; starting another one requires explicit Reset/load.
	set_paused(true)
	_show_menu(Flow.SELECTION)

func open_dialog(dialog: FileDialog) -> void:
	set_paused(true)
	dialog.popup_centered_ratio(0.8)

func open_in_game_save() -> void:
	if frontend != null and frontend.booted and session_started and not stopped:
		frontend.open_save_screen()
	else:
		open_dialog(save_dialog)

func _dialog_cancelled() -> void:
	status_label.text = "Selection cancelled. Current session retained; use Resume when ready."
	_show_menu(flow)

const FULL_KEYS := {KEY_J: "attack", KEY_K: "block", KEY_L: "cast", KEY_U: "disease_bolt"}

func _combat_call(method: String, arguments: Array) -> void:
	if controller == null or paused or stopped: return
	var result: Dictionary = controller.callv(method, arguments)
	if not result.get("ok", false): status_label.text = str(result.get("error", method + " refused"))

func _select_disease_bolt() -> void:
	if controller == null or paused or stopped: return
	_observe(controller.combat_select_disease_bolt())

func _full_key(event: InputEvent) -> bool:
	if not event is InputEventKey or event.echo or not raw_observation.get("full_enabled", false): return false
	var key: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
	if not FULL_KEYS.has(key): return false
	if _component_modal_open() or terrain_dialog.visible or asset_dialog.visible or model_dialog.visible or save_dialog.visible or load_dialog.visible: return false
	match FULL_KEYS[key]:
		"attack": _combat_call("combat_attack" if event.pressed else "combat_release_attack", [true] if event.pressed else [])
		"block": _combat_call("combat_block", [event.pressed])
		"cast":
			if event.pressed: _combat_call("combat_cast_holy_strike", [])
		"disease_bolt": _combat_call("combat_press_disease_bolt" if event.pressed else "combat_release_disease_bolt", [true] if event.pressed else [])
	return true

# Ordinary real-level play with the shared-World fight: J is the attack button
# of the executed player tick (native command bit 16 = pad attack).
func _fight_key(event: InputEvent) -> bool:
	if not event is InputEventKey or event.echo or not raw_observation.get("fight_player_ready", false): return false
	var key: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
	if key != KEY_J: return false
	if not event.pressed:
		held_commands &= ~16
		return true
	if paused or stopped or _component_modal_open() or terrain_dialog.visible or asset_dialog.visible or model_dialog.visible or save_dialog.visible or load_dialog.visible: return false
	held_commands |= 16
	return true

func handle_input_event(event: InputEvent) -> bool:
	if frontend != null and frontend.handle_key(event):
		held_commands = 0
		pending_damage = false
		return true
	if game_menus != null and game_menus.open and event is InputEventKey:
		held_commands = 0
		pending_damage = false
		game_menus.handle_host_key(event)
		return true
	if game_menus != null and game_menus.handle_host_key(event):
		return true
	if _full_key(event): return true
	if _fight_key(event): return true
	if event is InputEventKey and event.pressed and not event.echo and (event.physical_keycode == KEY_ESCAPE or event.keycode == KEY_ESCAPE):
		if _component_modal_open() or terrain_dialog.visible or asset_dialog.visible or model_dialog.visible or save_dialog.visible or load_dialog.visible:
			return false
		if session_started and not stopped:
			set_paused(not paused)
		return true
	return super.handle_input_event(event)

func reset_session() -> void:
	_clear_preview()
	super.reset_session()
	if controller != null:
		if audio != null: audio.reset(play_mode,int(raw_observation.ready_ticks))
		session_started = true
		set_paused(true)

func load_terrain_file(path: String) -> bool:
	if level_textures != null and level_textures.pairs.has(path):
		return load_textured_terrain(path, level_textures.pairs[path])
	var ok := super.load_terrain_file(path)
	if ok:
		_clear_preview()
		_sync_loaded_mode()
		session_started = false
		set_paused(true)
		status_label.text = "Local WORLD loaded. Choose an available session mode to start."
		if level_textures != null: level_textures.world_path = path
	else:
		status_label.text = "WORLD refused: %s. Current level retained; choose a supported dry static WORLD." % status_label.text
	return ok

func load_session_file(path: String) -> bool:
	_prepare_level_restore(path)
	var ok := super.load_session_file(path)
	if level_textures != null: level_textures.restore = {}
	if ok:
		_clear_preview()
		_sync_loaded_mode()
		if audio != null: audio.reset(play_mode,int(raw_observation.ready_ticks))
		session_started = true
		set_paused(true)
		if game_menus != null:
			game_menus.load_companion(path)
		if controller.has_method("load_player_companion"):
			var player_loaded: Dictionary = controller.load_player_companion(path)
			if player_loaded.get("ok", false):
				_observe(player_loaded)
			else:
				status_label.text = "Native scene loaded, but player record refused: " + str(player_loaded.get("error", "unknown"))
				return false
	else:
		status_label.text = "Load refused: %s. Current session retained; choose a valid native save." % status_label.text
	return ok

func _prepare_level_restore(path: String) -> void:
	if level_textures != null: level_textures.prepare_restore(path)

func _observe(result: Dictionary) -> void:
	super._observe(result)
	_present_combat()
	_present_level()
	if audio != null: audio.observe(raw_observation)
	if result.get("ok", false) and raw_observation.get("combat_enabled", false) and not paused and not stopped:
		status_label.text = "Synthetic creature swings · Player hit event is explicit · Esc pauses"
		if raw_observation.get("full_enabled", false):
			status_label.text = "Full combat · tick %d · HP %.1f · WASD walk · J attack · K block · L Holy Strike · U Disease Bolt · Esc pauses" % [int(raw_observation.get("ready_ticks", 0)), float(raw_observation.get("hp", 0.0))]
			if raw_observation.get("combat_spell_selected", false):
				status_label.text += " · Mana %.2f / %.2f" % [raw_observation.get("combat_mana", 0.0), raw_observation.get("combat_max_mana", 0.0)]
	if raw_observation.get("animated_enabled", false):
		status_label.text = "Agony animation hit timing · tick %s / 90 · synthetic audio cues" % raw_observation.get("ready_ticks", 0)
	if components.has("animation"):
		components.animation.set_suspended(paused or stopped)
		components.animation.observe(raw_observation)
	if components.has("art"):
		components.art.update_observation(raw_observation)
	if menu != null and stopped and flow == Flow.RUNNING:
		_show_menu(Flow.PAUSED)
	if play_hud != null:
		play_hud.observe_play(raw_observation, play_mode)
	if hud_round3 != null:
		hud_round3.observe(raw_observation, play_mode)
	if game_menus != null and components.has("art"):
		game_menus.sync_font(components.art)
	if frontend != null and components.has("art"):
		frontend.sync_font(components.art)
	if fx != null:
		fx.observe(raw_observation)
		fx_status.visible = fx.enabled
		if fx.enabled and raw_observation.get("terrain_mode") == "static_world":
			var ground := "this level's light record" if fx.level_light_active else "synthetic vertex lighting"
			terrain_label.text = str(raw_observation.terrain_name) + " · Static WORLD ground with " + ground + "; WORLD object / light / effect chunks omitted."
		if fx.enabled and fx.level_light_active:
			scope_label.text = "Play uses this level's light record and test effects. WORLD shows static ground; private files stay on your computer."
		else:
			scope_label.text = "Play uses synthetic lighting and test effects. WORLD shows static ground; private files stay on your computer." if fx.enabled else str(scope_label.get_meta("diagnostic_text"))
		fx_status.text = fx.status_text()
	if level_textures != null and not level_textures.label().is_empty():
		terrain_label.text = level_textures.label()
	_update_menu()

# The base HUD over the play view (godot/ui/play_hud.gd). It shares the
# original-art panel's checked textures.
func _attach_play_hud() -> void:
	if viewport == null:
		return
	play_hud = load("res://ui/play_hud.gd").new()
	play_hud.set_anchors_preset(Control.PRESET_FULL_RECT)
	play_hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	viewport.add_child(play_hud)
	if components.has("art"):
		var art: Control = components.art
		art.art_ready.connect(func(_ready): play_hud.set_art(art.loaded, art.catalogue))

func _attach_hud_round3() -> void:
	if viewport == null:
		return
	hud_round3 = load("res://ui/hud_round3.gd").new()
	hud_round3.set_anchors_preset(Control.PRESET_FULL_RECT)
	hud_round3.mouse_filter = Control.MOUSE_FILTER_IGNORE
	viewport.add_child(hud_round3)

func _attach_game_menus() -> void:
	if viewport == null:
		return
	game_menus = load("res://ui/game_menus.gd").new()
	game_menus.configure(self)
	game_menus.set_anchors_preset(Control.PRESET_FULL_RECT)
	game_menus.mouse_filter = Control.MOUSE_FILTER_IGNORE
	viewport.add_child(game_menus)

func _attach_components() -> void:
	for id in COMPONENTS:
		var spec: Dictionary = COMPONENTS[id]
		var component: Control
		if component_factory.is_valid():
			component = component_factory.call(id)
		elif ResourceLoader.exists(spec.path):
			var packed = ResourceLoader.load(spec.path)
			if packed is PackedScene:
				var instance = packed.instantiate()
				if instance is Control:
					component = instance
				else:
					instance.free()
		if component == null:
			component_reasons[id] = "Install component %s to enable this action." % spec.path
			continue
		var valid: bool = component.has_signal(spec.signal)
		for method in spec.methods:
			valid = valid and component.has_method(method)
		if not valid:
			component_reasons[id] = "Component contract mismatch: %s. Install the matching integration build." % spec.path
			component.free()
			continue
		components[id] = component
		if id == "art":
			component.custom_minimum_size.y = 360
			art_region.add_child(component)
			component.configure(inspector)
			component.art_ready.connect(func(ready): art_state.text = "Original art loaded · native HUD placement, full frame parity unverified" if ready else "Original art not loaded · select local resources")
			# The embedded component can open its own picker. Pause on its UI
			# controls too, not only on the host's Original art action.
			_watch_component_controls(component)
		elif id == "animation":
			add_child(component)
			component.configure(inspector, actor)
			component.preview_changed.connect(func(active):
				cosmetic_active = active
				_present_combat())
			component.hide()
			component.visibility_changed.connect(func(): _component_visibility_changed(component))
		else:
			add_child(component)
			component.configure(load_terrain_file, func(): set_paused(true))
			component.level_loaded.connect(func(_path): _show_menu(Flow.SELECTION))
			component.hide()
			component.visibility_changed.connect(func(): _component_visibility_changed(component))
		component_reasons[id] = ""

func _watch_component_controls(node: Node) -> void:
	if node is BaseButton:
		node.button_down.connect(func(): set_paused(true))
	for child in node.get_children():
		_watch_component_controls(child)

func _component_visibility_changed(component: Control) -> void:
	if not is_inside_tree():
		return
	if component.visible:
		set_paused(true)
	else:
		_show_menu(flow)

func _component_modal_open() -> bool:
	if frontend != null and frontend.active(): return true
	if level_tex_dialog != null and level_tex_dialog.visible: return true
	if fx_definitions != null and fx_definitions.visible: return true
	if frame_settings != null and frame_settings.visible:
		return true
	for id in ["library", "animation"]:
		if components.has(id) and components[id].visible:
			return true
	for component in components.values():
		for window in component.find_children("*", "Window", true, false):
			if window.visible:
				return true
	return false

func open_component(id: String) -> void:
	if not components.has(id) or actions[id].disabled:
		status_label.text = str(component_reasons.get(id, "Required native capability is unavailable."))
		return
	set_paused(true)
	if id == "library":
		components[id].show_library()
	else:
		components[id].show_picker()

func _clear_preview() -> void:
	if components.has("animation"):
		components.animation.clear_preview()
	if actor_visual != null:
		actor_visual.show()

func apply_asset_model(row_index: int) -> bool:
	var ok := super.apply_asset_model(row_index)
	if ok:
		_clear_preview()
	return ok

func restore_default_actor() -> void:
	_clear_preview()
	super.restore_default_actor()

func _release_native_resources() -> void:
	if audio != null: audio.release()
	if frame_settings != null and frame_settings.visibility_changed.is_connected(_frame_settings_changed):
		frame_settings.visibility_changed.disconnect(_frame_settings_changed)
	frame_settings = null
	_clear_preview()
	if components.has("art"):
		components.art.clear_art()
	# Break callback/inspector ownership before parent/native teardown.
	for component in components.values():
		component.free()
	components.clear()
	super._release_native_resources()

func _frame_settings_changed() -> void:
	if frame_settings.visible:
		set_paused(true)
	else:
		_show_menu(flow)

func _frame_layout() -> void:
	if viewport != null and menu != null:
		viewport.get_parent().custom_minimum_size.y = 220 if menu.visible else maxf(220, size.y - 350)

func set_facing_test(enabled: bool) -> void:
	if not play_mode:
		_clear_preview()
		super.set_facing_test(enabled)
		session_started = true
		set_paused(true)
		refresh_capabilities()

func set_recovery_test(enabled: bool) -> void:
	if not play_mode:
		_clear_preview()
		super.set_recovery_test(enabled)
		session_started = true
		set_paused(true)
		refresh_capabilities()

func set_animation_cache_test(enabled: bool) -> void:
	if not play_mode:
		_clear_preview()
		super.set_animation_cache_test(enabled)
		session_started = true
		set_paused(true)
		refresh_capabilities()

func _sync_loaded_mode() -> void:
	# The legacy bridge has no mode field. New runtimes must identify loaded
	# mode; never infer it from filename, tick count or save magic in GDScript.
	mode_known = true
	if raw_observation.has("play_mode") and raw_observation.play_mode is bool:
		play_mode = raw_observation.play_mode
	elif raw_observation.has("mode") or raw_observation.has("session_mode"):
		var mode: String = str(raw_observation.get("mode", raw_observation.get("session_mode", "")))
		play_mode = mode in ["play", "rta-play-session-v1"]
		mode_known = play_mode or mode == "diagnostic"
	elif controller.has_method("start_play_session"):
		mode_known = false
	else:
		play_mode = false
	refresh_capabilities()

func _configure_save_formats() -> void:
	if save_format == null:
		return
	# Native magic/version dispatch owns decoding; the picker must expose both
	# continuation files and legacy saves regardless of the current mode.
	load_dialog.filters = PackedStringArray(["*.ocplay,*.ocnsave ; Native scene saves"])
	save_dialog.filters = PackedStringArray(["*.ocplay ; Play continuation"] if play_mode else ["*.ocnsave ; Native scene save"])
	if save_dialog.current_file in ["scene.ocnsave", "scene.ocplay"]:
		save_dialog.current_file = "scene.ocplay" if play_mode else "scene.ocnsave"
	var selected_method := ""
	if save_format.selected >= 0 and save_format.selected < save_methods.size():
		selected_method = save_methods[save_format.selected]
	save_methods.clear()
	save_format.clear()
	save_capability_error = ""
	if controller == null or not mode_known:
		save_capability_error = "A native controller with an identified session mode is required."
	elif not play_mode:
		_add_save_format("Replay (smaller file)", "save_file")
		_add_save_format("Snapshot (faster load)", "save_snapshot_file")
	else:
		# Capability descriptors are native-owned, never user/resource metadata.
		# Unknown formats fail closed; no legacy format is relabelled as play.
		var formats = play_capabilities.get("save_formats", play_capabilities.get("supported_save_formats", []))
		if formats is Array or formats is PackedStringArray:
			for format in formats:
				if format is Dictionary:
					_add_save_format(str(format.get("label", format.get("id", "Native save"))), str(format.get("method", "")))
				elif format == "play-continuation-v1":
					_add_save_format("Continuation (current state)", "save_file")
				elif format is String and format in ["replay", "snapshot", "continuation"]:
					_add_save_format(format.capitalize(), "save_snapshot_file" if format == "snapshot" else "save_file")
	if save_methods.is_empty():
		save_capability_error = "Native runtime has not advertised a supported save format/method. Install the matching runtime; Save is disabled."
		save_format.add_item("No supported save format", -1)
		save_format.disabled = true
	else:
		save_format.disabled = false
		var index := save_methods.find(selected_method)
		save_format.select(maxi(0, index))
	# Rebuilding a hidden OptionButton can leave its cached minimum at arrow
	# width. Reserve space from the actual labels before showing the menu.
	var font := save_format.get_theme_font("font")
	var font_size := save_format.get_theme_font_size("font_size")
	var label_width := 0.0
	for item in range(save_format.item_count):
		label_width = maxf(label_width, font.get_string_size(save_format.get_item_text(item), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x)
	save_format.custom_minimum_size.x = label_width + 48.0
	save_format.tooltip_text = save_capability_error if not save_capability_error.is_empty() else "Only native-supported formats for the current session."

func _add_save_format(label: String, method: String) -> void:
	# Existing bridge writes files through these two entrypoints. A future new
	# method needs an explicit adapter update, not arbitrary dynamic dispatch.
	if method not in ["save_file", "save_snapshot_file"] or not controller.has_method(method) or method in save_methods:
		return
	save_format.add_item(label, save_methods.size())
	save_methods.append(method)

func save_session_file(path: String) -> bool:
	if controller == null or not mode_known or save_format.selected < 0 or save_format.selected >= save_methods.size():
		status_label.text = save_capability_error
		return false
	var result: Dictionary = controller.call(save_methods[save_format.selected], path)
	status_label.text = "Saved native scene." if result.get("ok", false) else "Save refused: %s. Current session retained; check the selected path and native state." % str(result.get("error", "Save failed."))
	if result.get("ok", false) and level_textures != null: level_textures.remember_save(path)
	if result.get("ok", false) and game_menus != null:
		game_menus.save_companion(path)
	if result.get("ok", false) and controller.has_method("save_player_companion"):
		var player_saved: Dictionary = controller.save_player_companion(path)
		if not player_saved.get("ok", false):
			status_label.text = "Native scene saved, but player record refused: " + str(player_saved.get("error", "unknown"))
			return false
	return result.get("ok", false)

func _focus_menu() -> void:
	if not is_inside_tree() or is_queued_for_deletion() or menu == null or not menu.is_visible_in_tree() or _component_modal_open():
		return
	var target: Button = actions.resume if session_started and not stopped else actions.terrain
	if not target.disabled:
		target.grab_focus()

func open_archive_browser() -> void:
	get_tree().root.set_meta("scene_play_flow_return", true)
	super.open_archive_browser()

func _present_combat() -> void:
	if combat_status == null: return
	var rows: Array = raw_observation.get("combat_actors", [])
	var visible_slots: Array = []
	var labels: PackedStringArray = []
	for row in rows:
		if not row.get("present", false): continue
		var slot: int = row.slot
		labels.append("%s %s: %s HP%s" % [row.kind, slot, row.hp, " · dying" if row.dead else ""])
		if slot == 0 and not row.has("skinned_vertices"): continue
		visible_slots.append(slot)
		if not combat_displays.has(slot):
			var marker := MeshInstance3D.new()
			var mesh := CapsuleMesh.new()
			mesh.radius = 4.0
			mesh.height = 12.0
			marker.mesh = mesh
			marker.material_override = _material(Color("bc7270"))
			_world.add_child(marker)
			combat_displays[slot] = marker
		var p: Vector3 = row.position
		if row.has("skinned_vertices"):
			var arrays: Array = []
			arrays.resize(Mesh.ARRAY_MAX)
			arrays[Mesh.ARRAY_VERTEX] = row.skinned_vertices
			var skin := ArrayMesh.new()
			skin.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
			combat_displays[slot].mesh = skin
			var skin_material := _material(Color("72c9b0") if slot == 0 else Color("bc7270"))
			skin_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
			combat_displays[slot].material_override = skin_material
		elif not combat_displays[slot].mesh is CapsuleMesh:
			var capsule := CapsuleMesh.new()
			capsule.radius = 4.0
			capsule.height = 12.0
			combat_displays[slot].mesh = capsule
		combat_displays[slot].position = Vector3(p.x, p.z + (0 if row.has("skinned_vertices") else 6), -p.y)
		combat_displays[slot].visible = true
	for slot in combat_displays:
		combat_displays[slot].visible = slot in visible_slots and not (slot == 0 and cosmetic_active)
	actor_visual.visible = not cosmetic_active and not raw_observation.get("animated_enabled", false)
	combat_status.text = "Authored actor markers · " + " | ".join(labels) if not rows.is_empty() else ""
	if raw_observation.get("full_enabled", false) and raw_observation.get("combat_spell_selected", false):
		combat_status.text += " · Disease Bolt selected · mana %.2f / %.2f" % [raw_observation.get("combat_mana", 0.0), raw_observation.get("combat_max_mana", 0.0)]
	combat_status.visible = not rows.is_empty()

func _present_level() -> void:
	if level_status == null: return
	var enabled: bool = raw_observation.get("level_enabled", false)
	level_status.visible = enabled
	if gate_display != null: gate_display.visible = enabled
	if not enabled:
		level_generation = -1
		return
	var generation: int = raw_observation.level_generation
	if generation != level_generation:
		_rebuild_terrain(controller.get_terrain())
		level_generation = generation
	if gate_display == null:
		gate_display = MeshInstance3D.new()
		var mesh := BoxMesh.new()
		mesh.size = Vector3(96, 0.4, 96)
		gate_display.mesh = mesh
		gate_display.material_override = _material(Color("386773"))
		_world.add_child(gate_display)
	var p: Vector3 = raw_observation.gate_marker
	gate_display.position = Vector3(p.x, p.z + 0.3, -p.y)
	gate_display.visible = true
	level_status.text = "Synthetic gate · %s · travel %s / 180 · saved levels %s · restored %s (%s bytes)" % [raw_observation.level_name, maxi(0, int(raw_observation.gate_travel_ticks) - 1), raw_observation.level_saved_count, raw_observation.level_restored, raw_observation.level_restored_bytes]
	if raw_observation.gate_menu_open:
		status_label.text = "Gate menu · choose Travel to aab; simulation paused"
	elif not paused and not stopped:
		status_label.text = "Stand on the authored gate · Open gate / Travel · Return restores saved level state"


func _attach_fx() -> void:
	if not ClassDB.class_exists("ChampionsFxBridge") or controller == null or not controller.has_signal("combat_visual_tick"): return
	fx = load("res://fx/playable_fx.gd").new()
	_world.add_child(fx)
	fx.configure(self)
	controller.combat_visual_tick.connect(fx.combat_tick)
	# The labelled synthetic recipe constants enable the Full fixture's effect
	# classes; an explicit user JSON (below) replaces them for later sessions.
	if controller.has_method("set_effect_recipe_constants_json"):
		var constants: Dictionary = controller.set_effect_recipe_constants_json(preload("res://fx/playable_fx_fixture.gd").recipe_constants(), "synthetic recipe constants")
		# Review finding 11: a refusal is shown where the Full fixture is offered,
		# not only as an editor warning.
		if not constants.get("ok", false):
			effect_constants_refusal = "Effect recipe constants refused: " + str(constants.get("error"))
			push_warning(effect_constants_refusal)
	fx_status = _label("FX: synthetic test definitions / test sheets", 13, MUTED)
	fx_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	main_layout.add_child(fx_status)
	main_layout.move_child(fx_status, main_layout.get_children().find(viewport.get_parent()))
	fx_status.visible = false
	scope_label.set_meta("diagnostic_text", scope_label.text)
	gore_choice = OptionButton.new()
	gore_choice.add_item("Blood off", 0)
	gore_choice.add_item("Blood splats", 1)
	gore_choice.add_item("Blood and pools (body parts unsupported)", 2)
	gore_choice.select(2)
	gore_choice.item_selected.connect(func(index): fx.set_gore(gore_choice.get_item_id(index)))
	menu.add_child(gore_choice)
	fx_definitions = FileDialog.new()
	fx_definitions.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	fx_definitions.access = FileDialog.ACCESS_FILESYSTEM
	fx_definitions.filters = PackedStringArray(["*.json ; User-supplied particle definitions"])
	fx_definitions.file_selected.connect(func(path):
		var result: Dictionary = fx.load_definition_file(path)
		status_label.text = "FX definitions loaded; test sheets remain synthetic." if result.ok else "FX definitions refused: " + str(result.get("error", "invalid JSON"))
		fx_status.text = fx.status_text())
	add_child(fx_definitions)
	_add_action("fx_definitions", "FX definitions JSON…", func(): open_dialog(fx_definitions))


func _attach_level_textures() -> void:
	if not ClassDB.class_exists("ChampionsLevelTexturePreview"): return
	level_textures = preload("res://scenes/playable_level_textures.gd").new()
	level_textures.configure(self)
	level_tex_dialog = FileDialog.new()
	level_tex_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	level_tex_dialog.access = FileDialog.ACCESS_FILESYSTEM
	level_tex_dialog.title = "Choose matching level TEX"
	level_tex_dialog.filters = PackedStringArray(["*.tex,*.TEX ; Level textures"])
	level_tex_dialog.file_selected.connect(func(path): load_textured_terrain(level_textures.world_path, path))
	level_tex_dialog.canceled.connect(_dialog_cancelled)
	add_child(level_tex_dialog)
	_add_action("level_tex", "Level textures…", func():
		if level_textures.world_path.is_empty():
			status_label.text = "Choose a local WORLD in the level library first, then its matching TEX."
		else: open_dialog(level_tex_dialog))

func load_textured_terrain(world: String, tex: String) -> bool:
	if level_textures == null: return false
	var ok: bool = level_textures.load_pair(world, tex)
	if ok:
		_clear_preview()
		_sync_loaded_mode()
		session_started = false
		set_paused(true)
		status_label.text = "Textured WORLD loaded. Start play to apply native lighting."
	else:
		status_label.text = "Textures refused: " + status_label.text + " Current level retained."
	return ok

func _prepare_terrain_root(result: Dictionary) -> Node3D:
	if level_textures != null and not level_textures.restore.is_empty():
		return level_textures.build(level_textures.restore)
	return super._prepare_terrain_root(result)

func _publish_terrain_root(candidate: Node3D) -> void:
	super._publish_terrain_root(candidate)
	if level_textures != null: level_textures.published(candidate)

func _attach_audio() -> void:
	if controller == null or not ClassDB.class_exists("ChampionsNativeAudio"): return
	var status := _label("Audio: synthetic bank",13,MUTED)
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	main_layout.add_child(status)
	main_layout.move_child(status,main_layout.get_children().find(viewport.get_parent()))
	audio = preload("res://audio/playable_audio.gd").new()
	add_child(audio)
	audio.configure(self,status)
	controller.play_audio_tick.connect(audio.ready_tick)
	_add_action("audio_boss", "Synthetic boss music (Broon)", func():
		status_label.text = "Synthetic Broon music queued" if audio.boss_enter("FatCyclops","Broon") else "Boss music refused (play required / override gated)")
	_add_action("audio_boss_stop", "Stop boss music", func(): audio.boss_leave())

func _attach_ps2_import() -> void:
	ps2_status = _label("", 13, MUTED)
	ps2_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	ps2_status.visible = false
	main_layout.add_child(ps2_status)
	main_layout.move_child(ps2_status, main_layout.get_children().find(viewport.get_parent()))
	_add_action("ps2_import", "Import character from PS2 save…", func(): open_dialog(ps2_dialog))
	if not ClassDB.class_exists("ChampionsPs2SaveImport"):
		actions.ps2_import.disabled = true
		actions.ps2_import.tooltip_text = "Install the native extension with ChampionsPs2SaveImport."
		return
	ps2_import = ClassDB.instantiate("ChampionsPs2SaveImport")
	ps2_dialog = FileDialog.new()
	ps2_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	ps2_dialog.access = FileDialog.ACCESS_FILESYSTEM
	ps2_dialog.title = "Choose a Return to Arms (US v2.00) memory-card slot file"
	ps2_dialog.filters = PackedStringArray(["*_slot* ; Memory-card slot file (BASLUS-20973_slotN)", "* ; Any file"])
	ps2_dialog.file_selected.connect(import_ps2_save_file)
	ps2_dialog.canceled.connect(_dialog_cancelled)
	add_child(ps2_dialog)
	actions.ps2_import.tooltip_text = "Read one character from a raw %d-byte RTA US v2.00 slot file. Bad or unknown fields are refused." % int(ClassDB.class_call_static("ChampionsPs2SaveImport", "file_size"))

func import_ps2_save_file(path: String) -> Dictionary:
	return _show_ps2_import(ps2_import.import_file(path, 0) if ps2_import != null else {"ok": false, "error": "ChampionsPs2SaveImport is not installed"})

func _show_ps2_import(result: Dictionary) -> Dictionary:
	ps2_status.visible = true
	if not result.get("ok", false):
		status_label.text = "PS2 import refused: %s" % result.get("error", "unknown error")
		ps2_status.text = status_label.text + (" · previous import kept" if ps2_import != null and ps2_import.has_character() else "")
		_show_menu(flow)
		return result
	var stats: Array = result.base_stats
	var bags: Array = result.bag_counts
	status_label.text = "Imported %s from the PS2 save into native player state." % result.name
	ps2_status.text = "PS2 character · %s · class %d · level %d · %d XP · %d gold · Str %d Int %d Dex %d Sta %d · bags %d/%d/%d · %d skills · %d quest values · saved at %s (act %d, difficulty %d)" % [
		result.name, result.class_index, result.level, result.experience, result.gold,
		stats[0], stats[1], stats[2], stats[3], bags[0], bags[1], bags[2],
		result.skills.size(), result.quests.size(), result.level_name if result.subname == "" else "%s_%s" % [result.level_name, result.subname],
		result.act, result.difficulty]
	if not result.refusals.is_empty():
		ps2_status.text += " · not applied: " + "; ".join(PackedStringArray(result.refusals))
	if controller != null and controller.has_method("set_player_character"):
		var accepted: Dictionary = controller.set_player_character(result)
		if accepted.get("ok", false):
			_observe(accepted)
		else:
			ps2_status.text += " · session record refused: " + str(accepted.get("error", "unknown"))
	_show_menu(flow)
	return result

func _attach_frontend() -> void:
	frontend = load("res://ui/frontend_menus.gd").new()
	frontend.configure(self)
	add_child(frontend)
	_add_action("frontend", "Title / front end", func(): frontend.boot())
	if not frontend.available():
		actions.frontend.disabled = true
		actions.frontend.tooltip_text = "Install the native extension with ChampionsFrontendService."
		return
	actions.frontend.tooltip_text = "Boot card check, logo, film, main menu, character creation, options and load. Layouts are host-drawn: the research documents the flow, not the screens' positions. In play, P is the start button (pause menu)."
	for argument in OS.get_cmdline_user_args():
		if argument == "--frontend":
			frontend.call_deferred("boot")
