extends Control

signal preview_changed(active: bool)

const MeshBuilder = preload("res://scenes/scene_animation_mesh.gd")

var inspector: RefCounted
var visual_parent: Node3D
var archive_path := ""
var archive_fingerprint := ""
var profile := "rta"
var rows: Array = []
var model_row := -1
var animation_row := -1
var duration_ticks := 0
var sample_tick := 0
var last_ready_ticks := -1
var playing := false
var suspended := false
var active := false
var last_error := ""
var frozen_fit := {"scale": 1.0, "offset": Vector3.ZERO}
var mesh_instance: MeshInstance3D
var panel: PanelContainer
var file_dialog: FileDialog
var model_choice: OptionButton
var animation_choice: OptionButton
var status_label: Label
var pair_label: Label
var pair_button: Button
var play_button: Button
var pause_button: Button
var restart_button: Button

func _ready() -> void:
	_build_picker()
	if get_window() != null:
		get_window().focus_exited.connect(func(): set_suspended(true))
		get_window().focus_entered.connect(func(): set_suspended(false))

func _exit_tree() -> void:
	clear_preview()

func configure(value: RefCounted, parent: Node3D) -> void:
	clear_preview()
	inspector = value
	visual_parent = parent

func show_picker() -> void:
	visible = true
	panel.visible = true
	_update_controls()

func _build_picker() -> void:
	panel = PanelContainer.new()
	# The integrated shell has text behind this overlay. Keep it readable.
	var background := StyleBoxFlat.new()
	background.bg_color = Color(0.04, 0.07, 0.12, 1.0)
	for side in [SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM]:
		background.set_content_margin(side, 8)
	panel.add_theme_stylebox_override("panel", background)
	panel.custom_minimum_size = Vector2(440, 0)
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
	panel.offset_left = -220
	panel.offset_right = 220
	panel.offset_top = 24
	add_child(panel)
	var layout := VBoxContainer.new()
	panel.add_child(layout)
	var title_label := Label.new()
	title_label.text = "Cosmetic scene animation preview"
	layout.add_child(title_label)
	pair_label = Label.new()
	pair_label.text = "Pairing unverified · explicit VIF + ANM rows · no original animation dispatch"
	pair_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(pair_label)
	var open_button := Button.new()
	open_button.text = "Open local GOB / LMP"
	open_button.pressed.connect(func(): file_dialog.popup_centered_ratio(0.65))
	layout.add_child(open_button)
	model_choice = OptionButton.new()
	model_choice.add_item("Choose a .vif model row", -1)
	layout.add_child(model_choice)
	animation_choice = OptionButton.new()
	animation_choice.add_item("Choose an .anm clip row", -1)
	layout.add_child(animation_choice)
	var controls := HBoxContainer.new()
	layout.add_child(controls)
	pair_button = Button.new()
	pair_button.text = "Use selected pair"
	pair_button.pressed.connect(_use_picker_selection)
	controls.add_child(pair_button)
	play_button = Button.new()
	play_button.text = "Play"
	play_button.pressed.connect(play)
	controls.add_child(play_button)
	pause_button = Button.new()
	pause_button.text = "Pause"
	pause_button.pressed.connect(pause)
	controls.add_child(pause_button)
	restart_button = Button.new()
	restart_button.text = "Restart"
	restart_button.pressed.connect(restart)
	controls.add_child(restart_button)
	var clear_button := Button.new()
	clear_button.text = "Clear"
	clear_button.pressed.connect(clear_preview)
	controls.add_child(clear_button)
	var hide_button := Button.new()
	hide_button.text = "Hide picker (keep preview)"
	hide_button.pressed.connect(func(): hide())
	layout.add_child(hide_button)
	var close_button := Button.new()
	close_button.text = "Close preview"
	close_button.pressed.connect(func():
		clear_preview()
		hide()
	)
	layout.add_child(close_button)
	status_label = Label.new()
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(status_label)
	file_dialog = FileDialog.new()
	file_dialog.access = FileDialog.ACCESS_FILESYSTEM
	file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	file_dialog.use_native_dialog = true
	file_dialog.add_filter("*.gob, *.GOB, *.lmp, *.LMP", "Local archive")
	file_dialog.file_selected.connect(func(path: String): open_archive(path))
	add_child(file_dialog)
	_update_controls()

func open_archive(path: String, selected_profile := "rta") -> bool:
	clear_preview()
	archive_path = ""
	archive_fingerprint = ""
	rows.clear()
	profile = selected_profile
	model_choice.clear()
	animation_choice.clear()
	model_choice.add_item("Choose a .vif model row", -1)
	animation_choice.add_item("Choose an .anm clip row", -1)
	if inspector == null:
		_fail("Archive reader unavailable. Build the pinned native extension and reopen the scene.")
		return false
	if profile != "rta":
		_fail("Only the Return to Arms profile supports this position preview.")
		return false
	var listing: Dictionary = inspector.inspect_archive(path)
	if not listing.get("ok", false) or typeof(listing.get("rows")) != TYPE_ARRAY or typeof(listing.get("fingerprint")) != TYPE_STRING or listing["fingerprint"].is_empty():
		_fail(str(listing.get("error", "The archive could not be inspected.")))
		return false
	archive_path = path
	archive_fingerprint = listing["fingerprint"]
	rows = listing["rows"]
	for index in range(rows.size()):
		var row = rows[index]
		if typeof(row) != TYPE_DICTIONARY or str(row.get("kind", "")) != "LMP":
			continue
		var name := str(row.get("name", ""))
		var label := "%d · %s / %s" % [index, str(row.get("parent", "")), name]
		if name.to_lower().ends_with(".vif"):
			model_choice.add_item(label, index)
		elif name.to_lower().ends_with(".anm"):
			animation_choice.add_item(label, index)
	last_error = ""
	_update_controls()
	return true

func _use_picker_selection() -> void:
	select_pair(model_choice.get_selected_id(), animation_choice.get_selected_id())

func select_pair(selected_model_row: int, selected_animation_row: int) -> bool:
	clear_preview()
	if archive_path.is_empty() or archive_fingerprint.is_empty() or profile != "rta":
		_fail("Open a supported Return to Arms archive first.")
		return false
	if not _row_has_extension(selected_model_row, ".vif") or not _row_has_extension(selected_animation_row, ".anm"):
		_fail("Choose explicit .vif and .anm archive rows.")
		return false
	model_row = selected_model_row
	animation_row = selected_animation_row
	return _sample(0, true)

func _row_has_extension(index: int, extension: String) -> bool:
	return index >= 0 and index < rows.size() and typeof(rows[index]) == TYPE_DICTIONARY and str(rows[index].get("kind", "")) == "LMP" and str(rows[index].get("name", "")).to_lower().ends_with(extension)

func _sample(tick: int, initial: bool = false) -> bool:
	if inspector == null or visual_parent == null or not is_instance_valid(visual_parent):
		_fail("The archive reader or actor visual parent is unavailable. Select the pair again.")
		return false
	var sample: Dictionary = inspector.sample_skinned_model(archive_path, model_row, animation_row, profile, archive_fingerprint, tick)
	if not sample.get("ok", false) or typeof(sample.get("tick")) not in [TYPE_INT, TYPE_FLOAT] or float(sample.get("tick", -1)) != float(tick):
		_fail(str(sample.get("error", "The selected pose is unavailable. Reopen the archive and select the pair again.")))
		return false
	var prepared: Dictionary = MeshBuilder.prepare(sample)
	if not prepared.get("ok", false):
		_fail(str(prepared.get("error", "The pose cannot be displayed.")))
		return false
	if initial:
		duration_ticks = sample["duration_ticks"]
		frozen_fit = MeshBuilder.fit(prepared["bounds"])
		mesh_instance = MeshInstance3D.new()
		mesh_instance.name = "CosmeticAnimationPreview"
		var material := StandardMaterial3D.new()
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		material.cull_mode = BaseMaterial3D.CULL_DISABLED
		material.albedo_color = Color("70c5d9")
		mesh_instance.material_override = material
		visual_parent.add_child(mesh_instance)
		mesh_instance.position = frozen_fit["offset"]
		mesh_instance.scale = Vector3.ONE * frozen_fit["scale"]
	else:
		if sample["duration_ticks"] != duration_ticks:
			_fail("The selected clip changed duration. Reopen the archive and select the pair again.")
			return false
	mesh_instance.mesh = prepared["mesh"]
	sample_tick = tick
	last_error = ""
	if not active:
		active = true
		preview_changed.emit(true)
	_update_controls()
	return true

func observe(observation: Dictionary) -> void:
	if not active or typeof(observation.get("ready_ticks")) != TYPE_INT:
		return
	var ready: int = observation["ready_ticks"]
	if ready < 0:
		return
	if last_ready_ticks < 0 or ready < last_ready_ticks:
		last_ready_ticks = ready
		return
	var elapsed := ready - last_ready_ticks
	last_ready_ticks = ready
	if suspended or not playing or elapsed == 0:
		return
	var target := sample_tick + mini(duration_ticks - sample_tick, elapsed)
	if target != sample_tick and not _sample(target):
		return
	if target == duration_ticks:
		playing = false
	_update_controls()

func play() -> void:
	# The shell pauses while this picker is open. Record explicit playback
	# intent now; observe() still freezes sampling until the shell resumes.
	if active and sample_tick < duration_ticks and _sample(sample_tick):
		playing = true
		last_ready_ticks = -1
		_update_controls()

func pause() -> void:
	playing = false
	last_ready_ticks = -1
	_update_controls()

func restart() -> void:
	if active and _sample(0):
		playing = duration_ticks > 0
		last_ready_ticks = -1
		_update_controls()

func set_suspended(value: bool) -> void:
	if suspended == value:
		return
	suspended = value
	last_ready_ticks = -1
	_update_controls()

func clear_preview() -> void:
	playing = false
	last_error = ""
	last_ready_ticks = -1
	sample_tick = 0
	duration_ticks = 0
	model_row = -1
	animation_row = -1
	frozen_fit = {"scale": 1.0, "offset": Vector3.ZERO}
	if mesh_instance != null and is_instance_valid(mesh_instance):
		mesh_instance.mesh = null
		mesh_instance.material_override = null
		if mesh_instance.get_parent() != null:
			mesh_instance.get_parent().remove_child(mesh_instance)
		mesh_instance.queue_free()
	mesh_instance = null
	if active:
		active = false
		preview_changed.emit(false)
	_update_controls()

func _fail(message: String) -> void:
	clear_preview()
	last_error = message if not message.is_empty() else "The selected pose is unsupported or stale. Reopen the archive and select the pair again."
	_update_controls()

func _update_controls() -> void:
	if status_label == null:
		return
	play_button.disabled = not active or sample_tick >= duration_ticks or playing
	pause_button.disabled = not active or not playing
	restart_button.disabled = not active
	if not last_error.is_empty():
		status_label.text = "Error: " + last_error
	elif active:
		var state := "waiting for session resume" if playing and suspended else ("playing" if playing else "paused")
		status_label.text = "Pairing unverified · cosmetic preview · tick %d / %d · %s" % [sample_tick, duration_ticks, state]
	else:
		status_label.text = "Select an archive and both original row indices. The actor stays static until a valid tick-zero pose is sampled."
