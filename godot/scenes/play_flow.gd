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


func _ready() -> void:
	# Preserve both exported legacy routes on their original scene.
	if "--sandbox-smoke-test" in OS.get_cmdline_user_args() or "--browser-smoke-test" in OS.get_cmdline_user_args():
		get_tree().change_scene_to_file.call_deferred("res://scenes/session_sandbox.tscn")
		return
	super._ready()
	_build_flow()
	_attach_components()
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
	cosmetic_label = _label("Cosmetic model / clip pairing unverified.", 12, MUTED)
	main_layout.add_child(cosmetic_label)
	art_region = VBoxContainer.new()
	main_layout.add_child(art_region)
	art_state = _label("Original art not loaded · authored diagnostic placement", 12, MUTED)
	art_region.add_child(art_state)
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
	for id in COMPONENTS:
		actions[id].disabled = not components.has(id) or (controller == null if id == "library" else inspector == null)
		actions[id].tooltip_text = str(component_reasons.get(id, ""))
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
	if session_started and not stopped and not _component_modal_open():
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
		session_started = true
		set_paused(true)

func load_terrain_file(path: String) -> bool:
	var ok := super.load_terrain_file(path)
	if ok:
		_clear_preview()
		session_started = false
		set_paused(true)
	return ok

func load_session_file(path: String) -> bool:
	var ok := super.load_session_file(path)
	if ok:
		_clear_preview()
		session_started = true
		set_paused(true)
	return ok

func _observe(result: Dictionary) -> void:
	super._observe(result)
	if components.has("animation"):
		components.animation.set_suspended(paused or stopped)
		components.animation.observe(raw_observation)
	if components.has("art"):
		components.art.update_observation(raw_observation)
	if menu != null and stopped and flow == Flow.RUNNING:
		_show_menu(Flow.PAUSED)
	_update_menu()

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
			art_region.add_child(component)
			component.configure(inspector)
			component.art_ready.connect(func(ready): art_state.text = "Original art loaded · authored diagnostic placement" if ready else "Original art not loaded · authored diagnostic placement")
			# The embedded component can open its own picker. Pause on its UI
			# controls too, not only on the host's Original art action.
			_watch_component_controls(component)
		elif id == "animation":
			add_child(component)
			component.configure(inspector, actor)
			component.preview_changed.connect(func(active): actor_visual.visible = not active)
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
	_clear_preview()
	if components.has("art"):
		components.art.clear_art()
	# Break callback/inspector ownership before parent/native teardown.
	for component in components.values():
		component.free()
	components.clear()
	super._release_native_resources()
