extends "res://scenes/session_sandbox.gd"

# Authored host navigation. All simulation, limits and save semantics remain native.
enum Flow { SELECTION, RUNNING, PAUSED }
var flow := Flow.SELECTION
var session_started := false
var play_mode := false
var menu: VBoxContainer
var menu_title: Label
var mode_label: Label
var capability_label: Label
var menu_actions: HFlowContainer
var actions: Dictionary = {}
var debug_toggle: CheckButton
var play_capabilities: Dictionary = {}

func _ready() -> void:
	# Preserve both exported legacy routes on their original scene.
	if "--sandbox-smoke-test" in OS.get_cmdline_user_args() or "--browser-smoke-test" in OS.get_cmdline_user_args():
		get_tree().change_scene_to_file.call_deferred("res://scenes/session_sandbox.tscn")
		return
	super._ready()
	_build_flow()
	refresh_capabilities()
	set_paused(true)
	_show_menu(Flow.SELECTION)

func _build_flow() -> void:
	# Scroll the authored diagnostic region on short/narrow windows. No camera
	# transform or native coordinate is derived from menu layout.
	var parent := main_layout.get_parent()
	parent.remove_child(main_layout)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	parent.add_child(scroll)
	scroll.add_child(main_layout)
	main_layout.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	main_layout.get_child(1).custom_minimum_size.y = 220
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
	menu.add_child(mode_label)
	capability_label = _label("", 13, MUTED)
	capability_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	menu.add_child(capability_label)
	menu_actions = HFlowContainer.new()
	menu_actions.add_theme_constant_override("h_separation", 8)
	menu.add_child(menu_actions)
	_add_action("terrain", "Open local WORLD…", func(): open_dialog(terrain_dialog))
	_add_action("play", "Start play session", start_play)
	_add_action("diagnostic", "Start diagnostic", start_diagnostic)
	_add_action("resume", "Resume", resume_session)
	_add_action("reset", "Reset current level", reset_session)
	_add_action("save", "Save…", func(): open_dialog(save_dialog))
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
	bar.add_child(_button("Pause / menu", func(): set_paused(true)))
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
	actions.play.tooltip_text = "Install the session-play-runtime component to enable this action." if actions.play.disabled else "Start a fresh native play session on the selected terrain."
	capability_label.text = actions.play.tooltip_text if actions.play.disabled else str(play_capabilities.get("history_policy", "Native play runtime available."))
	for id in ["terrain", "diagnostic", "reset", "save", "load", "resume"]:
		actions[id].disabled = controller == null
		if controller == null:
			actions[id].tooltip_text = "Install the native ChampionsSessionController extension."
	actions.appearance.disabled = inspector == null
	_update_menu()

func _update_menu() -> void:
	if menu == null:
		return
	mode_label.text = "Play mode · authored dry movement; original game progression and combat are unimplemented." if play_mode else "Bounded diagnostic mode · %s ready ticks maximum; original game is unimplemented." % raw_observation.get("ready_tick_limit", 512)
	actions.resume.disabled = controller == null or not session_started or stopped
	actions.resume.visible = session_started
	actions.selection.visible = flow != Flow.SELECTION
	actions.diagnostic.visible = not session_started
	actions.play.visible = not session_started
	debug_toggle.disabled = play_mode or controller == null
	if debug_toggle.disabled:
		debug_toggle.button_pressed = false

func _show_menu(state: Flow) -> void:
	flow = state
	if menu == null:
		return
	menu.visible = state != Flow.RUNNING
	menu_title.text = "Local level selection" if state == Flow.SELECTION else "Paused session"
	_update_menu()
	if menu.visible:
		var target: Button = actions.resume if session_started and not stopped else actions.terrain
		if not target.disabled:
			target.grab_focus.call_deferred()
	else:
		var focused := get_viewport().gui_get_focus_owner()
		if focused != null:
			focused.release_focus()
		diagnostic_controls.hide()
		cache_controls.hide()
		debug_toggle.set_pressed_no_signal(false)

func set_paused(value: bool) -> void:
	super.set_paused(value)
	if menu != null:
		_show_menu(Flow.SELECTION if not session_started else (Flow.PAUSED if paused or stopped else Flow.RUNNING))

func start_diagnostic() -> void:
	if controller == null or session_started:
		return
	session_started = true
	play_mode = false
	set_paused(false)

func start_play() -> void:
	if controller == null or session_started or not play_capabilities.get("available", false):
		return
	var result: Dictionary = controller.call("start_play_session")
	if not result.get("ok", false):
		status_label.text = str(result.get("error", "Native play start refused."))
		return
	play_mode = true
	session_started = true
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	_observe(result)
	set_paused(false)
	refresh_capabilities()

func resume_session() -> void:
	if session_started and not stopped:
		set_paused(false)

func return_to_selection() -> void:
	# Retain the live session; starting another one requires explicit Reset/load.
	set_paused(true)
	_show_menu(Flow.SELECTION)

func open_dialog(dialog: FileDialog) -> void:
	set_paused(true)
	dialog.popup_centered_ratio(0.8)

func _dialog_cancelled() -> void:
	status_label.text = "Selection cancelled. Current session retained; use Resume when ready."
	_show_menu(flow)

func handle_input_event(event: InputEvent) -> bool:
	if event is InputEventKey and event.pressed and not event.echo and (event.physical_keycode == KEY_ESCAPE or event.keycode == KEY_ESCAPE):
		if terrain_dialog.visible or asset_dialog.visible or model_dialog.visible or save_dialog.visible or load_dialog.visible:
			return false
		if session_started and not stopped:
			set_paused(not paused)
		return true
	return super.handle_input_event(event)

func reset_session() -> void:
	super.reset_session()
	if controller != null:
		session_started = true
		set_paused(true)

func load_terrain_file(path: String) -> bool:
	var ok := super.load_terrain_file(path)
	if ok:
		session_started = false
		set_paused(true)
	return ok

func load_session_file(path: String) -> bool:
	var ok := super.load_session_file(path)
	if ok:
		session_started = true
		set_paused(true)
	return ok

func _observe(result: Dictionary) -> void:
	super._observe(result)
	if menu != null and stopped and flow == Flow.RUNNING:
		_show_menu(Flow.PAUSED)
	_update_menu()
