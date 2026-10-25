extends CanvasLayer

# Local presentation preferences; never stored in Session or in the checkout.
const CONFIG_PATH := "user://frame_rate.cfg"
const CAPS := [0, 30, 60, 120, 144, 240, -1]
var config_path := CONFIG_PATH
var cap := 0
var vsync := DisplayServer.VSYNC_DISABLED
var readout := false
var settings_window: Window
var cap_picker: OptionButton
var sync_picker: OptionButton
var meter: Label
var elapsed := 0.0
var frames := 0
var last_frame_usec := 0

func _ready() -> void:
	_load_preferences()
	_apply()
	_build_ui()

func _load_preferences() -> void:
	var config := ConfigFile.new()
	if config.load(config_path) == OK:
		var saved_cap = config.get_value("presentation", "cap", 0)
		var saved_sync = config.get_value("presentation", "vsync", 0)
		if typeof(saved_cap) == TYPE_INT and saved_cap in CAPS:
			cap = saved_cap
		if typeof(saved_sync) == TYPE_INT and saved_sync in [0, 1, 2]:
			vsync = saved_sync
		readout = config.get_value("presentation", "readout", false) == true
	if vsync == DisplayServer.VSYNC_ADAPTIVE and not supports_adaptive():
		vsync = DisplayServer.VSYNC_ENABLED

func supports_adaptive() -> bool:
	return DisplayServer.get_name() != "headless" and DisplayServer.has_feature(DisplayServer.FEATURE_SWAP_BUFFERS) and RenderingServer.get_current_rendering_method() != "gl_compatibility"

func _apply() -> void:
	Engine.max_fps = _display_cap() if cap == -1 else cap
	if DisplayServer.get_name() != "headless":
		DisplayServer.window_set_vsync_mode(vsync)

func _display_cap() -> int:
	var refresh := DisplayServer.screen_get_refresh_rate(DisplayServer.window_get_current_screen()) if DisplayServer.get_name() != "headless" else -1.0
	# Godot reports -1 when unknown. Do not invent a display frequency.
	return maxi(1, roundi(refresh)) if refresh > 0 else 0

func set_preferences(new_cap: int, new_vsync: int, show_readout: bool, persist := true) -> void:
	if new_cap not in CAPS or new_vsync not in [0, 1, 2]:
		return
	cap = new_cap
	vsync = DisplayServer.VSYNC_ENABLED if new_vsync == DisplayServer.VSYNC_ADAPTIVE and not supports_adaptive() else new_vsync
	readout = show_readout
	_apply()
	if meter != null:
		meter.visible = readout
	if persist:
		var config := ConfigFile.new()
		config.set_value("presentation", "cap", cap)
		config.set_value("presentation", "vsync", vsync)
		config.set_value("presentation", "readout", readout)
		if config.save(config_path) != OK:
			push_warning("Frame-rate settings could not be saved.")

func _build_ui() -> void:
	var overlay := Control.new()
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(overlay)
	var button := Button.new()
	button.text = "Frame rate…"
	button.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
	button.position = Vector2(-150, -44)
	button.size = Vector2(132, 30)
	button.pressed.connect(func(): settings_window.popup_centered())
	overlay.add_child(button)
	meter = Label.new()
	meter.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT)
	meter.position = Vector2(-260, 6)
	meter.size = Vector2(242, 24)
	meter.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	meter.mouse_filter = Control.MOUSE_FILTER_IGNORE
	meter.visible = readout
	overlay.add_child(meter)
	settings_window = Window.new()
	settings_window.title = "Frame rate"
	settings_window.visible = false
	settings_window.size = Vector2i(380, 240)
	settings_window.unresizable = true
	settings_window.transient = true
	settings_window.close_requested.connect(settings_window.hide)
	add_child(settings_window)
	var layout := VBoxContainer.new()
	layout.position = Vector2(20, 20)
	layout.size = Vector2(340, 200)
	layout.add_theme_constant_override("separation", 12)
	settings_window.add_child(layout)
	cap_picker = OptionButton.new()
	for title in ["Unlimited", "30 FPS", "60 FPS", "120 FPS", "144 FPS", "240 FPS", "Display refresh"]:
		cap_picker.add_item(title)
	cap_picker.select(CAPS.find(cap))
	cap_picker.item_selected.connect(func(index): set_preferences(CAPS[index], vsync, readout))
	layout.add_child(cap_picker)
	sync_picker = OptionButton.new()
	for title in ["V-Sync off", "V-Sync on", "V-Sync adaptive"]:
		sync_picker.add_item(title)
	sync_picker.set_item_disabled(2, not supports_adaptive())
	sync_picker.set_item_tooltip(2, "Requires a supported display and Forward+ or Mobile renderer.")
	sync_picker.select(vsync)
	sync_picker.item_selected.connect(func(index): set_preferences(cap, index, readout))
	layout.add_child(sync_picker)
	var toggle := CheckButton.new()
	toggle.text = "Show FPS and frame time"
	toggle.button_pressed = readout
	toggle.toggled.connect(func(value): set_preferences(cap, vsync, value))
	layout.add_child(toggle)
	var note := Label.new()
	note.text = "V-Sync may limit FPS to your display.\nUnknown display refresh leaves FPS unlimited."
	layout.add_child(note)

func _process(_delta: float) -> void:
	var now := Time.get_ticks_usec()
	if last_frame_usec != 0:
		elapsed += (now - last_frame_usec) / 1e6
	last_frame_usec = now
	frames += 1
	if elapsed >= 0.5:
		if readout:
			meter.text = "%.0f FPS · %.2f ms" % [frames / elapsed, elapsed * 1000.0 / frames]
		if cap == -1:
			_apply()
		elapsed = 0
		frames = 0
