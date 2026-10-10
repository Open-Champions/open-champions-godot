extends Window

signal sample_requested(tick: float)

# Positions stay owned in mapped Y-up space. Only drawable vertices are rebased.
var mesh_root: Node3D
var camera: Camera3D
var viewport: SubViewport
var input_surface: SubViewportContainer
var info: Label
var pairing_label: Label
var tick_input: SpinBox
var sample_button: Button
var frame_button: Button
var cached_meshes: Array[Dictionary] = []
var mesh_count := 0
var total_mesh_count := 0
var hidden_mesh_count := 0
var triangle_count := 0
var total_triangle_count := 0
var model_bounds := AABB()
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
var model_name := ""
var animation_name := ""

func _ready() -> void:
	title = "Open Champions · Sampled model positions"
	size = Vector2i(820, 620)
	min_size = Vector2i(500, 400)
	close_requested.connect(clear_skinned_model)
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
	pairing_label = Label.new()
	pairing_label.text = "Pairing unverified"
	layout.add_child(pairing_label)
	info = Label.new()
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	info.text = "No model and animation selected"
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
	mesh_root = Node3D.new()
	world.add_child(mesh_root)
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
	frame_button = _add_button(controls, "Frame", frame_model)
	frame_button.disabled = true
	var scope := Label.new()
	scope.text = "Unlit positions only · default-visible meshes · no normals or textures.\nDrag to orbit · scroll to zoom · Frame to fit. 60 ticks = 1 second.\nExplicit model and clip selection; no automatic matching or playback."
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
	if mesh_root != null:
		for child in mesh_root.get_children():
			mesh_root.remove_child(child)
			child.queue_free()

func _remove_geometry() -> void:
	dragging = false
	_remove_drawables()
	cached_meshes.clear()
	mesh_count = 0
	total_mesh_count = 0
	hidden_mesh_count = 0
	triangle_count = 0
	total_triangle_count = 0
	model_bounds = AABB()

func clear_skinned_model() -> void:
	hide()
	_remove_geometry()
	has_framed_geometry = false
	last_error = ""
	sampled_tick = 0
	duration_ticks = 0
	model_name = ""
	animation_name = ""
	center = Vector3.ZERO
	display_origin = Vector3.ZERO
	radius = 1.0
	distance = 3.0
	yaw = 0.55
	pitch = 0.3
	if info != null:
		info.text = "No model and animation selected"
		tick_input.set_value_no_signal(0)
		tick_input.max_value = 0
		tick_input.editable = false
		sample_button.disabled = true
		frame_button.disabled = true

func clear_model() -> void:
	clear_skinned_model()

func _fail(message: String) -> bool:
	clear_skinned_model()
	last_error = message
	return false

func show_skinned_model(result: Dictionary, open_window := true, preserve_camera := false) -> bool:
	if typeof(result.get("ok")) != TYPE_BOOL or not result["ok"]:
		return _fail(str(result.get("error", "These model positions could not be sampled.")))
	if typeof(result.get("pairing_verified")) != TYPE_BOOL or result["pairing_verified"]:
		return _fail("This viewer requires an explicitly unverified model and animation pair.")
	for field in ["model_name", "animation_name"]:
		if typeof(result.get(field)) != TYPE_STRING or result[field].is_empty() or result[field].length() > 4096:
			return _fail("The selected model or animation name is missing or invalid.")
	if typeof(result.get("duration_ticks")) != TYPE_INT or typeof(result.get("tick")) not in [TYPE_INT, TYPE_FLOAT]:
		return _fail("The sample has an invalid time range.")
	var duration: int = result["duration_ticks"]
	var tick: float = result["tick"]
	if duration < 0 or duration > 4294967295 or not is_finite(tick) or tick < 0 or tick > duration:
		return _fail("The sample time is outside this clip.")
	if typeof(result.get("meshes")) != TYPE_ARRAY or result["meshes"].size() > 64:
		return _fail("The sample has an invalid mesh list.")
	if typeof(result.get("triangle_count")) != TYPE_INT or result["triangle_count"] < 0 or result["triangle_count"] > 200000:
		return _fail("The sample has an invalid triangle count.")
	var prepared: Array[Dictionary] = []
	var indices := {}
	var corner_count := 0
	# Validate every mesh, including hidden ones, before publishing any geometry.
	for entry in result["meshes"]:
		if typeof(entry) != TYPE_DICTIONARY or typeof(entry.get("index")) != TYPE_INT or typeof(entry.get("visible_by_default")) != TYPE_BOOL or typeof(entry.get("positions")) != TYPE_PACKED_VECTOR3_ARRAY:
			return _fail("The sample has incomplete mesh data.")
		var index: int = entry["index"]
		if index < 0 or index >= 64 or indices.has(index):
			return _fail("The sample has invalid or duplicate mesh indices.")
		indices[index] = true
		var source: PackedVector3Array = entry["positions"]
		if source.size() % 3 != 0 or source.size() > 600000 - corner_count:
			return _fail("The sample exceeds the supported triangle limit or has incomplete triangles.")
		corner_count += source.size()
		var mapped := PackedVector3Array()
		mapped.resize(source.size())
		for i in range(source.size()):
			var point := source[i]
			if not point.is_finite() or maxf(absf(point.x), maxf(absf(point.y), absf(point.z))) > 1e15:
				return _fail("The mesh positions are outside the supported display range.")
			mapped[i] = _point(point)
		prepared.append({"index": index, "visible_by_default": entry["visible_by_default"], "positions": mapped})
	if corner_count != result["triangle_count"] * 3:
		return _fail("The sample triangle count does not match its mesh data.")
	if not preserve_camera:
		clear_skinned_model()
	else:
		_remove_geometry()
	last_error = ""
	cached_meshes = prepared
	total_mesh_count = prepared.size()
	total_triangle_count = result["triangle_count"]
	sampled_tick = tick
	duration_ticks = duration
	model_name = result["model_name"]
	animation_name = result["animation_name"]
	var has_bounds := false
	for entry in cached_meshes:
		if not entry["visible_by_default"]:
			hidden_mesh_count += 1
			continue
		var positions: PackedVector3Array = entry["positions"]
		if positions.is_empty():
			continue
		mesh_count += 1
		triangle_count += positions.size() / 3
		for point in positions:
			model_bounds = model_bounds.expand(point) if has_bounds else AABB(point, Vector3.ZERO)
			has_bounds = true
	_build_geometry()
	tick_input.max_value = duration_ticks
	tick_input.set_value_no_signal(sampled_tick)
	tick_input.editable = duration_ticks > 0
	sample_button.disabled = false
	frame_button.disabled = triangle_count == 0
	info.text = "Model: %s\nAnimation: %s · time %s / %d ticks\n%d of %d meshes shown · %d triangles · %d hidden meshes retained" % [model_name, animation_name, str(sampled_tick), duration_ticks, mesh_count, total_mesh_count, triangle_count, hidden_mesh_count]
	if triangle_count == 0:
		info.text += "\nNo default-visible triangles at this sample."
	if open_window and not visible:
		var available := get_tree().root.size
		popup_centered(Vector2i(mini(820, available.x), mini(620, available.y)))
	if triangle_count > 0 and (not preserve_camera or not has_framed_geometry):
		frame_model()
	return true

func _build_geometry() -> void:
	_remove_drawables()
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.albedo_color = Color("70c5d9")
	for entry in cached_meshes:
		var positions: PackedVector3Array = entry["positions"]
		if not entry["visible_by_default"] or positions.is_empty():
			continue
		var vertices := PackedVector3Array()
		vertices.resize(positions.size())
		for i in range(positions.size()):
			vertices[i] = positions[i] - display_origin
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = vertices
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		var instance := MeshInstance3D.new()
		instance.name = "Mesh_%d" % entry["index"]
		instance.mesh = mesh
		instance.material_override = material
		mesh_root.add_child(instance)

func _request_sample() -> void:
	if not sample_button.disabled:
		tick_input.apply()
		sample_requested.emit(tick_input.value)

func frame_model() -> void:
	if triangle_count == 0:
		return
	has_framed_geometry = true
	# Rebase presentation only. A small camera offset added to huge float32
	# coordinates can round back to the target and make look_at degenerate.
	# Explicit Frame updates this origin; manual samples keep it and the camera.
	display_origin = model_bounds.get_center()
	center = Vector3.ZERO
	_build_geometry()
	radius = maxf(model_bounds.size.length() * 0.5, 0.1)
	yaw = 0.55
	pitch = 0.3
	var aspect := maxf(float(viewport.size.x) / maxf(viewport.size.y, 1), 0.1)
	var half_angle := minf(deg_to_rad(camera.fov) * 0.5, atan(tan(deg_to_rad(camera.fov) * 0.5) * aspect))
	distance = radius / sin(half_angle) * 1.15
	_update_camera()

func orbit(horizontal: float, vertical: float) -> void:
	if triangle_count == 0:
		return
	yaw += horizontal
	pitch = clampf(pitch + vertical, -1.45, 1.45)
	_update_camera()

func zoom(factor: float) -> void:
	if triangle_count == 0:
		return
	distance = clampf(distance * factor, radius * 1.05, radius * 40)
	_update_camera()

func _update_camera() -> void:
	camera.near = maxf(radius * 0.001, 0.0001)
	camera.far = maxf(radius * 100, 10)
	camera.position = center + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
	camera.look_at(center, Vector3.UP)

func _viewport_input(event: InputEvent) -> void:
	if triangle_count == 0:
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
