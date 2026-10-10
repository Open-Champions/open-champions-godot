extends Window

# Presentation only: original Z-up positions/normal vectors become Godot Y-up.
# Native UVs remain texel coordinates; normalize once for an optional companion.
var model_root: Node3D
var camera: Camera3D
var viewport: SubViewport
var input_surface: SubViewportContainer
var dragging := false
var last_pointer_position := Vector2.ZERO
var info: Label
var scope: Label
var mesh_count := 0
var triangle_count := 0
var model_bounds := AABB()
var center := Vector3.ZERO
var radius := 1.0
var distance := 3.0
var yaw := 0.55
var pitch := 0.3
var last_error := ""
var texture_colors: CheckButton
var texture_note: Label
var model_material: StandardMaterial3D
var model_texture: ImageTexture
var texture_pixels := PackedByteArray()
var texture_dimensions := Vector2.ZERO
var texture_source_name := ""
var texture_reason := ""

func _ready() -> void:
	title = "Open Champions · rest-pose model"
	size = Vector2i(820, 620)
	min_size = Vector2i(500, 400)
	close_requested.connect(clear_model)
	window_input.connect(_window_pointer_guard)
	focus_exited.connect(func(): dragging = false)
	mouse_exited.connect(func(): dragging = false)
	var page_scroll := ScrollContainer.new()
	page_scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	page_scroll.follow_focus = true
	add_child(page_scroll)
	var margin := MarginContainer.new()
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	margin.size_flags_vertical = Control.SIZE_EXPAND_FILL
	for edge in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + edge, 16)
	page_scroll.add_child(margin)
	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 10)
	margin.add_child(layout)
	info = Label.new()
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(info)
	texture_colors = CheckButton.new()
	texture_colors.text = "Texture colors"
	texture_colors.disabled = true
	texture_colors.toggled.connect(_apply_texture_colors)
	layout.add_child(texture_colors)
	texture_note = Label.new()
	texture_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(texture_note)
	var container := SubViewportContainer.new()
	input_surface = container
	container.stretch = true
	container.mouse_force_pass_scroll_events = false
	container.size_flags_vertical = Control.SIZE_EXPAND_FILL
	container.custom_minimum_size = Vector2(300, 180)
	container.gui_input.connect(_viewport_input)
	layout.add_child(container)
	viewport = SubViewport.new()
	viewport.size = Vector2i(640, 400)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_WHEN_VISIBLE
	container.add_child(viewport)
	var world := Node3D.new()
	viewport.add_child(world)
	model_root = Node3D.new()
	world.add_child(model_root)
	var environment := WorldEnvironment.new()
	var settings := Environment.new()
	settings.background_mode = Environment.BG_COLOR
	settings.background_color = Color("111c22")
	settings.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	settings.ambient_light_color = Color("c9d7dd")
	settings.ambient_light_energy = 0.65
	environment.environment = settings
	world.add_child(environment)
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-45, -25, 0)
	light.light_energy = 1.1
	world.add_child(light)
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
	_add_button(controls, "Frame", frame_model)
	scope = Label.new()
	scope.text = "Drag to orbit · scroll to zoom. Rest-pose inspection only.\nDefault visible meshes · two-sided inspection · no animation or gameplay."
	scope.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(scope)

func _add_button(parent: HBoxContainer, text: String, action: Callable) -> void:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(44, 36)
	button.pressed.connect(action)
	parent.add_child(button)

func _point(value: Vector3) -> Vector3:
	return Vector3(value.x, value.z, -value.y)

func clear_model() -> void:
	dragging = false
	hide()
	if model_root != null:
		for child in model_root.get_children():
			model_root.remove_child(child)
			child.queue_free()
	mesh_count = 0
	triangle_count = 0
	model_bounds = AABB()
	last_error = ""
	model_material = null
	model_texture = null
	texture_pixels = PackedByteArray()
	texture_dimensions = Vector2.ZERO
	texture_source_name = ""
	texture_reason = ""
	if texture_colors != null:
		texture_colors.set_pressed_no_signal(false)
		texture_colors.disabled = true
		texture_note.text = ""
	if info != null:
		info.text = "No model selected"

func show_model(result: Dictionary, open_window := true) -> bool:
	clear_model()
	if not result.get("ok", false):
		last_error = str(result.get("error", "This model is unsupported."))
		return false
	model_material = StandardMaterial3D.new()
	model_material.roughness = 0.9
	model_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	model_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	model_material.texture_repeat = false
	model_material.transparency = BaseMaterial3D.TRANSPARENCY_DISABLED
	_prepare_texture(result.get("texture", {}))
	var first_point := true
	var hidden_count := 0
	for entry: Dictionary in result.get("meshes", []):
		if not entry.get("visible_by_default", false):
			hidden_count += 1
			continue
		var source: PackedVector3Array = entry.get("positions", PackedVector3Array())
		var source_normals: PackedVector3Array = entry.get("normals", PackedVector3Array())
		var uv: PackedVector2Array = entry.get("uv_texels", PackedVector2Array())
		if source.is_empty():
			continue
		if source.size() % 3 != 0 or source_normals.size() != source.size() or uv.size() != source.size():
			clear_model()
			last_error = "The model has inconsistent triangle attributes."
			return false
		var vertices := PackedVector3Array()
		var normals := PackedVector3Array()
		for i in range(source.size()):
			var point := _point(source[i])
			vertices.append(point)
			normals.append(_point(source_normals[i]).normalized())
			if first_point:
				model_bounds = AABB(point, Vector3.ZERO)
				first_point = false
			else:
				model_bounds = model_bounds.expand(point)
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = vertices
		arrays[Mesh.ARRAY_NORMAL] = normals
		var display_uv := uv.duplicate()
		if model_texture != null:
			for corner in range(display_uv.size()):
				display_uv[corner] /= texture_dimensions
		arrays[Mesh.ARRAY_TEX_UV] = display_uv
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		instance.material_override = model_material
		model_root.add_child(instance)
		mesh_count += 1
		triangle_count += source.size() / 3
	if first_point:
		clear_model()
		last_error = "No triangles are visible under this model's default hidden-mask selection."
		return false
	center = model_bounds.get_center()
	radius = maxf(model_bounds.size.length() * 0.5, 0.01)
	info.text = "%s · %d meshes · %d triangles · %d hidden meshes omitted" % [str(result.get("name", "Model")), mesh_count, triangle_count, hidden_count]
	if open_window:
		var available := get_tree().root.size
		popup_centered(Vector2i(mini(820, available.x), mini(620, available.y)))
	frame_model()
	return true

func _prepare_texture(texture: Dictionary) -> void:
	texture_reason = str(texture.get("error", "No matching companion texture was found in this LMP."))
	if texture_reason.is_empty():
		texture_reason = "No compatible companion texture is available."
	if texture.get("ok", false):
		var width: int = texture.get("width", 0)
		var height: int = texture.get("height", 0)
		var rgba: PackedByteArray = texture.get("rgba", PackedByteArray())
		if width > 0 and height > 0 and width <= 1024 and height <= 1024 and rgba.size() == width * height * 4:
			texture_pixels = rgba.duplicate()
			for offset in range(3, texture_pixels.size(), 4):
				texture_pixels[offset] = 255
			var image := Image.create_from_data(width, height, false, Image.FORMAT_RGBA8, texture_pixels)
			model_texture = ImageTexture.create_from_image(image)
			texture_dimensions = Vector2(width, height)
			texture_source_name = str(texture.get("name", "Companion texture"))
			texture_colors.disabled = false
			texture_colors.set_pressed_no_signal(true)
		else:
			texture_reason = "The companion texture has invalid dimensions or pixel data."
	_apply_texture_colors()

func _apply_texture_colors(_enabled := false) -> void:
	if model_material == null:
		return
	if model_texture != null and texture_colors.button_pressed:
		model_material.albedo_texture = model_texture
		model_material.albedo_color = Color.WHITE
		model_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		texture_note.text = "%s · %d × %d · opaque RGB (stored alpha ignored).\nNearest / clamp, stored top-left rows. Original lighting and blending are not reproduced." % [texture_source_name, int(texture_dimensions.x), int(texture_dimensions.y)]
	else:
		model_material.albedo_texture = null
		model_material.albedo_color = Color("bfc9cf")
		model_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
		texture_note.text = "Neutral material: " + ("texture colors are switched off." if model_texture != null else texture_reason)

func frame_model() -> void:
	yaw = 0.55
	pitch = 0.3
	var aspect := maxf(float(viewport.size.x) / maxf(viewport.size.y, 1), 0.1)
	var half_angle := minf(deg_to_rad(camera.fov) * 0.5, atan(tan(deg_to_rad(camera.fov) * 0.5) * aspect))
	distance = radius / sin(half_angle) * 1.15
	_update_camera()

func orbit(horizontal: float, vertical: float) -> void:
	yaw += horizontal
	pitch = clampf(pitch + vertical, -1.45, 1.45)
	_update_camera()

func zoom(factor: float) -> void:
	distance = clampf(distance * factor, radius * 1.05, radius * 40)
	_update_camera()

func _update_camera() -> void:
	camera.near = maxf(radius * 0.001, 0.0001)
	camera.far = maxf(radius * 100, 10)
	camera.position = center + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
	camera.look_at(center, Vector3.UP)

# Capture begins only on the 3D panel. Do not depend on motion.button_mask:
# synthesized pointer motion can omit it even between a press and release.
func _viewport_input(event: InputEvent) -> void:
	if mesh_count == 0:
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
		# Some synthesized drags update position but leave relative at zero.
		var pointer_delta: Vector2 = event.position - last_pointer_position
		last_pointer_position = event.position
		orbit(-pointer_delta.x * 0.01, pointer_delta.y * 0.01)

# A release over another control must end capture without consuming its event.
func _window_pointer_guard(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and not event.pressed:
		dragging = false
