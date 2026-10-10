extends Window

signal sample_requested(tick: float)

# Joint-only presentation. Native Z-up points become Godot Y-up exactly once.
var joint_root: Node3D
var camera: Camera3D
var viewport: SubViewport
var input_surface: SubViewportContainer
var info: Label
var tick_input: SpinBox
var sample_button: Button
var frame_button: Button
var points := PackedVector3Array()
var parents := PackedInt32Array()
var valid := PackedByteArray()
var drives_joint := PackedByteArray()
var total_joint_count := 0
var joint_count := 0
var link_count := 0
var masked_count := 0
var invalid_count := 0
var joint_bounds := AABB()
var center := Vector3.ZERO
var display_origin := Vector3.ZERO
var radius := 1.0
var distance := 3.0
var yaw := 0.55
var pitch := 0.3
var dragging := false
var last_pointer_position := Vector2.ZERO
var has_framed_geometry := false
var last_error := ""
var sampled_tick := 0.0
var duration_ticks := 0

func _ready() -> void:
	title = "Open Champions · Animation joints"
	size = Vector2i(820, 620)
	min_size = Vector2i(500, 400)
	close_requested.connect(clear_animation)
	window_input.connect(_window_pointer_guard)
	focus_exited.connect(func(): dragging = false)
	mouse_exited.connect(func(): dragging = false)
	var scroll := ScrollContainer.new()
	scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	scroll.follow_focus = true
	add_child(scroll)
	var margin := MarginContainer.new()
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	margin.size_flags_vertical = Control.SIZE_EXPAND_FILL
	for edge in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + edge, 16)
	scroll.add_child(margin)
	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 10)
	margin.add_child(layout)
	info = Label.new()
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	info.text = "No animation selected"
	layout.add_child(info)
	var time_row := HBoxContainer.new()
	time_row.add_theme_constant_override("separation", 10)
	layout.add_child(time_row)
	var time_label := Label.new()
	time_label.text = "Time (ticks)"
	time_row.add_child(time_label)
	tick_input = SpinBox.new()
	tick_input.min_value = 0
	tick_input.max_value = 0
	tick_input.step = 0.001
	tick_input.custom_minimum_size.x = 200
	tick_input.editable = false
	time_row.add_child(tick_input)
	sample_button = Button.new()
	sample_button.text = "Sample"
	sample_button.custom_minimum_size = Vector2(90, 36)
	sample_button.disabled = true
	sample_button.pressed.connect(_request_sample)
	time_row.add_child(sample_button)
	input_surface = SubViewportContainer.new()
	input_surface.stretch = true
	input_surface.mouse_force_pass_scroll_events = false
	input_surface.size_flags_vertical = Control.SIZE_EXPAND_FILL
	input_surface.custom_minimum_size = Vector2(300, 220)
	input_surface.gui_input.connect(_viewport_input)
	layout.add_child(input_surface)
	viewport = SubViewport.new()
	viewport.size = Vector2i(640, 400)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_WHEN_VISIBLE
	input_surface.add_child(viewport)
	var world := Node3D.new()
	viewport.add_child(world)
	joint_root = Node3D.new()
	world.add_child(joint_root)
	var environment := WorldEnvironment.new()
	var settings := Environment.new()
	settings.background_mode = Environment.BG_COLOR
	settings.background_color = Color("111c22")
	environment.environment = settings
	world.add_child(environment)
	camera = Camera3D.new()
	camera.fov = 50
	world.add_child(camera)
	camera.make_current()
	var controls := HBoxContainer.new()
	controls.alignment = BoxContainer.ALIGNMENT_CENTER
	layout.add_child(controls)
	_add_button(controls, "Left", func(): orbit(-0.2, 0))
	_add_button(controls, "Right", func(): orbit(0.2, 0))
	_add_button(controls, "Up", func(): orbit(0, 0.15))
	_add_button(controls, "Down", func(): orbit(0, -0.15))
	_add_button(controls, "+", func(): zoom(0.8))
	_add_button(controls, "−", func(): zoom(1.25))
	frame_button = _add_button(controls, "Frame", frame_animation)
	frame_button.disabled = true
	var scope := Label.new()
	scope.text = "Blue: driven joints · amber: not driven by this clip (shown for reference).\nDrag to orbit · scroll to zoom · Frame to fit. 60 ticks = 1 second.\nManual joint inspection only; no character model or playback."
	scope.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(scope)

func _add_button(parent: HBoxContainer, text: String, action: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(44, 36)
	button.pressed.connect(action)
	parent.add_child(button)
	return button

func _point(value: Vector3) -> Vector3:
	return Vector3(value.x, value.z, -value.y)

func _remove_drawables() -> void:
	if joint_root != null:
		for child in joint_root.get_children():
			joint_root.remove_child(child)
			child.queue_free()

func _remove_geometry() -> void:
	dragging = false
	_remove_drawables()
	points = PackedVector3Array()
	parents = PackedInt32Array()
	valid = PackedByteArray()
	drives_joint = PackedByteArray()
	total_joint_count = 0
	joint_count = 0
	link_count = 0
	masked_count = 0
	invalid_count = 0
	joint_bounds = AABB()

func clear_animation() -> void:
	hide()
	_remove_geometry()
	has_framed_geometry = false
	last_error = ""
	sampled_tick = 0
	duration_ticks = 0
	center = Vector3.ZERO
	display_origin = Vector3.ZERO
	radius = 1.0
	distance = 3.0
	yaw = 0.55
	pitch = 0.3
	if info != null:
		info.text = "No animation selected"
		tick_input.set_value_no_signal(0)
		tick_input.max_value = 0
		tick_input.editable = false
		sample_button.disabled = true
		frame_button.disabled = true

func _fail(message: String) -> bool:
	clear_animation()
	last_error = message
	return false

func show_animation(result: Dictionary, open_window := true, preserve_camera := false) -> bool:
	if not result.get("ok", false):
		return _fail(str(result.get("error", "This animation could not be sampled.")))
	if typeof(result.get("parents")) != TYPE_PACKED_INT32_ARRAY or typeof(result.get("origins")) != TYPE_PACKED_VECTOR3_ARRAY or typeof(result.get("valid")) != TYPE_PACKED_BYTE_ARRAY or typeof(result.get("drives_joint")) != TYPE_PACKED_BYTE_ARRAY:
		return _fail("The animation has incomplete joint data.")
	var source: PackedVector3Array = result["origins"]
	var source_parents: PackedInt32Array = result["parents"]
	var source_valid: PackedByteArray = result["valid"]
	var source_drives: PackedByteArray = result["drives_joint"]
	if typeof(result.get("duration_ticks")) != TYPE_INT or typeof(result.get("tick")) not in [TYPE_INT, TYPE_FLOAT]:
		return _fail("The animation has an invalid time range.")
	var duration: int = result["duration_ticks"]
	var tick: float = result["tick"]
	if duration < 0 or duration > 4294967295 or not is_finite(tick) or tick < 0 or tick > duration:
		return _fail("The animation time is outside this clip.")
	if source.size() > 64 or source_parents.size() != source.size() or source_valid.size() != source.size() or source_drives.size() != source.size():
		return _fail("The animation has inconsistent joint counts.")
	for i in range(source.size()):
		if source_parents[i] < -1 or source_parents[i] >= i or source_valid[i] > 1 or source_drives[i] > 1:
			return _fail("The animation has invalid joint links or visibility flags.")
		if not source[i].is_finite() or maxf(absf(source[i].x), maxf(absf(source[i].y), absf(source[i].z))) > 1e15:
			return _fail("The joint positions are outside the supported display range.")
		if source_valid[i] == 1 and source_parents[i] >= 0 and source_valid[source_parents[i]] == 0:
			return _fail("The animation has inconsistent joint validity.")
	if not preserve_camera:
		clear_animation()
	else:
		_remove_geometry()
	last_error = ""
	parents = source_parents.duplicate()
	valid = source_valid.duplicate()
	drives_joint = source_drives.duplicate()
	total_joint_count = source.size()
	sampled_tick = tick
	duration_ticks = duration
	for i in range(source.size()):
		var point := _point(source[i])
		points.append(point)
		if valid[i] == 0:
			invalid_count += 1
			continue
		joint_bounds = AABB(point, Vector3.ZERO) if joint_count == 0 else joint_bounds.expand(point)
		joint_count += 1
		if drives_joint[i] == 0:
			masked_count += 1
	_build_geometry()
	tick_input.max_value = duration_ticks
	tick_input.set_value_no_signal(sampled_tick)
	tick_input.editable = duration_ticks > 0
	sample_button.disabled = false
	frame_button.disabled = joint_count == 0
	var name: String = str(result.get("name", "Animation"))
	if total_joint_count == 0:
		info.text = "%s · no joints in this clip. Time %s / %d ticks." % [name, str(sampled_tick), duration_ticks]
	elif joint_count == 0:
		info.text = "%s · no valid joints at time %s / %d ticks. Try another time." % [name, str(sampled_tick), duration_ticks]
	else:
		info.text = "%s · time %s / %d ticks\n%d of %d joints shown · %d links · %d not driven · %d unavailable" % [name, str(sampled_tick), duration_ticks, joint_count, total_joint_count, link_count, masked_count, invalid_count]
	if open_window and not visible:
		var available := get_tree().root.size
		popup_centered(Vector2i(mini(820, available.x), mini(620, available.y)))
	if joint_count > 0 and (not preserve_camera or not has_framed_geometry):
		frame_animation()
	return true

func _material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	return material

func _build_geometry() -> void:
	_remove_drawables()
	link_count = 0
	if joint_count == 0:
		return
	var blue := _material(Color("70d6ff"))
	var amber := _material(Color("ffc477"))
	var sphere := SphereMesh.new()
	sphere.radius = maxf(joint_bounds.size.length() * 0.008, 0.025)
	sphere.height = sphere.radius * 2
	sphere.radial_segments = 12
	sphere.rings = 6
	var links := ImmediateMesh.new()
	var has_links := false
	for i in range(points.size()):
		if valid[i] == 0:
			continue
		var marker := MeshInstance3D.new()
		marker.mesh = sphere
		marker.material_override = blue if drives_joint[i] == 1 else amber
		marker.position = points[i] - display_origin
		joint_root.add_child(marker)
		var parent: int = parents[i]
		if parent >= 0 and valid[parent] == 1:
			if not has_links:
				links.surface_begin(Mesh.PRIMITIVE_LINES, _material(Color("9aaeb8")))
				has_links = true
			links.surface_add_vertex(points[parent] - display_origin)
			links.surface_add_vertex(points[i] - display_origin)
			link_count += 1
	if has_links:
		links.surface_end()
		var instance := MeshInstance3D.new()
		instance.mesh = links
		joint_root.add_child(instance)

func _request_sample() -> void:
	if not sample_button.disabled:
		tick_input.apply()
		sample_requested.emit(tick_input.value)

func frame_animation() -> void:
	if joint_count == 0:
		return
	has_framed_geometry = true
	# Rebase presentation only. A small camera offset added to huge float32
	# coordinates can round back to the target and make look_at degenerate.
	# Explicit Frame updates this origin; manual samples keep it and the camera.
	display_origin = joint_bounds.get_center()
	center = Vector3.ZERO
	_build_geometry()
	radius = maxf(joint_bounds.size.length() * 0.5, 0.1)
	yaw = 0.55
	pitch = 0.3
	var aspect := maxf(float(viewport.size.x) / maxf(viewport.size.y, 1), 0.1)
	var half_angle := minf(deg_to_rad(camera.fov) * 0.5, atan(tan(deg_to_rad(camera.fov) * 0.5) * aspect))
	distance = radius / sin(half_angle) * 1.15
	_update_camera()

func orbit(horizontal: float, vertical: float) -> void:
	if joint_count == 0:
		return
	yaw += horizontal
	pitch = clampf(pitch + vertical, -1.45, 1.45)
	_update_camera()

func zoom(factor: float) -> void:
	if joint_count == 0:
		return
	distance = clampf(distance * factor, radius * 1.05, radius * 40)
	_update_camera()

func _update_camera() -> void:
	camera.near = maxf(radius * 0.001, 0.0001)
	camera.far = maxf(radius * 100, 10)
	camera.position = center + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
	camera.look_at(center, Vector3.UP)

func _viewport_input(event: InputEvent) -> void:
	if joint_count == 0:
		return
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			dragging = event.pressed
			last_pointer_position = event.position
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
			zoom(0.85)
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			zoom(1.0 / 0.85)
	elif event is InputEventMouseMotion and dragging:
		var pointer_delta: Vector2 = event.position - last_pointer_position
		last_pointer_position = event.position
		orbit(-pointer_delta.x * 0.01, pointer_delta.y * 0.01)

func _window_pointer_guard(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and not event.pressed:
		dragging = false
