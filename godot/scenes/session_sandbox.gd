extends Control

const AssetMesh = preload("res://scenes/session_asset_mesh.gd")
const GOLD := Color("dec384")
const INK := Color("eef0e9")
const MUTED := Color("a7bbba")
const PANEL := Color("132023")
const COMMANDS := {KEY_D: 1, KEY_A: 2, KEY_W: 4, KEY_S: 8, KEY_H: 16}

var controller: RefCounted
var inspector: RefCounted
var actor: Node3D
var actor_visual: MeshInstance3D
var terrain_root: Node3D
var camera: Camera3D
var viewport: SubViewport
var raw_observation: Dictionary = {}
var held_commands := 0
var pending_damage := false
var paused := false
var stopped := false
var automatic_steps := true
var health_label: Label
var health_bar: ProgressBar
var status_label: Label
var terrain_label: Label
var asset_label: Label
var pause_button: Button
var terrain_dialog: FileDialog
var asset_dialog: FileDialog
var model_dialog: ConfirmationDialog
var model_list: ItemList
var asset_archive_path := ""
var asset_fingerprint := ""
var asset_rows: Array = []
var displayed_asset_name := "Practice figure"
var _default_mesh: CapsuleMesh
var _world: Node3D

func _ready() -> void:
	if "--browser-smoke-test" in OS.get_cmdline_user_args():
		get_tree().change_scene_to_file.call_deferred("res://ui/archive_browser.tscn")
		return
	_build_interface()
	if not ClassDB.class_exists("ChampionsSessionController"):
		_show_error("The scene is unavailable. Restart after installing the native build.")
		return
	controller = ClassDB.instantiate("ChampionsSessionController")
	if ClassDB.class_exists("ChampionsArchiveInspector"):
		inspector = ClassDB.instantiate("ChampionsArchiveInspector")
	_rebuild_terrain(controller.get_terrain())
	_observe(controller.get_observation())
	if "--sandbox-smoke-test" in OS.get_cmdline_user_args() and not get_tree().root.has_meta("sandbox_smoke_started"):
		get_tree().root.set_meta("sandbox_smoke_started", true)
		automatic_steps = false
		call_deferred("_run_export_smoke")

func _physics_process(_delta: float) -> void:
	# The presentation clock chooses when to submit ONE integer command frame.
	# Neither elapsed time nor Godot transform/physics enters the native model.
	if automatic_steps and controller != null and not paused and not stopped:
		advance_step()

func _input(event: InputEvent) -> void:
	if handle_input_event(event):
		get_viewport().set_input_as_handled()

func handle_input_event(event: InputEvent) -> bool:
	if not event is InputEventKey or event.echo:
		return false
	var key: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
	if event.pressed and (terrain_dialog.visible or asset_dialog.visible or model_dialog.visible):
		return false
	if key == KEY_ESCAPE and event.pressed:
		set_paused(not paused)
		return true
	if key not in COMMANDS:
		return false
	var bit: int = COMMANDS[key]
	if not event.pressed:
		held_commands &= ~bit
		return true
	if paused or stopped or terrain_dialog.visible or asset_dialog.visible or model_dialog.visible:
		return false
	held_commands |= bit
	if bit == 16:
		pending_damage = true
	return true

func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT or what == NOTIFICATION_WM_WINDOW_FOCUS_OUT:
		if controller != null:
			set_paused(true)

func advance_step() -> Dictionary:
	if controller == null:
		return {"ok": false, "error": "The native scene is unavailable."}
	var command := held_commands | (16 if pending_damage else 0)
	pending_damage = false
	var result: Dictionary = controller.advance(command)
	_observe(result)
	return result.duplicate(true)

func release_controls() -> void:
	held_commands = 0
	pending_damage = false
	if controller != null:
		_observe(controller.release_input())

func set_paused(value: bool) -> void:
	release_controls()
	if controller != null:
		_observe(controller.set_paused(value))
	else:
		paused = value

func reset_session() -> void:
	held_commands = 0
	pending_damage = false
	if controller != null:
		_observe(controller.reset_session())

func stop_session() -> void:
	release_controls()
	if controller != null:
		_observe(controller.stop_session())

func _observe(result: Dictionary) -> void:
	if not result.has("position"):
		_show_error(str(result.get("error", "The scene stopped.")))
		return
	raw_observation = result.duplicate(true)
	var native_position: Vector3 = result.position
	actor.position = Vector3(native_position.x, native_position.z, -native_position.y)
	# Camera motion is display-only. It cannot move or collide with the actor.
	camera.position = actor.position + Vector3(56, 70, 80)
	camera.look_at(actor.position + Vector3(0, 3, 0), Vector3.UP)
	paused = result.paused
	stopped = result.stopped
	health_bar.value = clampf(float(result.hp), 0.0, 100.0)
	health_label.text = "Health %s / 100" % str(snappedf(float(result.hp), 0.01))
	terrain_label.text = str(result.terrain_name)
	pause_button.text = "Resume" if paused else "Pause"
	if result.failed:
		status_label.text = "Stopped: %s · Reset to start again." % result.error
	elif stopped and result.error == "ready_tick_limit":
		status_label.text = "Practice run finished · Reset to start again."
	elif stopped:
		status_label.text = "Stopped · Reset to start again."
	elif paused:
		status_label.text = "Paused · Resume when you are ready."
	else:
		status_label.text = "Move with W A S D · H tests health · Esc pauses"
	if stopped or paused:
		held_commands = 0
		pending_damage = false

func open_archive_browser() -> void:
	set_paused(true)
	get_tree().change_scene_to_file("res://ui/archive_browser.tscn")

func _show_error(message: String) -> void:
	stopped = true
	if status_label != null:
		status_label.text = message

func load_terrain_file(path: String) -> bool:
	if controller == null:
		return false
	var result: Dictionary = controller.load_terrain_file(path)
	if not result.get("ok", false):
		status_label.text = str(result.get("error", "The terrain could not be loaded."))
		return false
	_rebuild_terrain(controller.get_terrain())
	held_commands = 0
	pending_damage = false
	_observe(result)
	return true

func select_asset_archive(path: String) -> Dictionary:
	if inspector == null:
		return {"ok": false, "error": "The model reader is unavailable."}
	var result: Dictionary = inspector.inspect_archive(path)
	if not result.get("ok", false):
		status_label.text = str(result.get("error", "The archive could not be opened."))
		return result
	asset_archive_path = path
	asset_fingerprint = result.fingerprint
	asset_rows = result.rows.duplicate(true)
	model_list.clear()
	for i in range(asset_rows.size()):
		var row: Dictionary = asset_rows[i]
		if str(row.name).to_lower().ends_with(".vif"):
			model_list.add_item("%s / %s" % [row.parent, row.name])
			model_list.set_item_metadata(model_list.item_count - 1, i)
	return result

func apply_asset_model(row_index: int) -> bool:
	if inspector == null or asset_fingerprint.is_empty():
		return false
	# Re-read the original archive; the reader validates the full structure and
	# fingerprint before resolving the exact row. No arbitrary offset shortcuts.
	var result: Dictionary = inspector.inspect_model(asset_archive_path, row_index, "rta", asset_fingerprint)
	var visual: Dictionary = AssetMesh.build(result)
	if not visual.get("ok", false):
		status_label.text = str(result.get("error", "")) if not result.get("ok", false) else visual.error
		return false
	actor_visual.mesh = visual.mesh
	actor_visual.scale = Vector3.ONE * float(visual.scale)
	actor_visual.position = visual.offset
	actor_visual.material_override = null
	displayed_asset_name = str(result.name)
	asset_label.text = "%s · static appearance only" % displayed_asset_name
	return true

func restore_default_actor() -> void:
	actor_visual.mesh = _default_mesh
	actor_visual.scale = Vector3.ONE
	actor_visual.position = Vector3(0, 6, 0)
	actor_visual.material_override = _material(GOLD)
	displayed_asset_name = "Practice figure"
	asset_label.text = "Practice figure · appearance only"

func _rebuild_terrain(result: Dictionary) -> void:
	for child in terrain_root.get_children():
		terrain_root.remove_child(child)
		child.queue_free()
	for surface in result.surfaces:
		var source: PackedVector3Array = surface.positions
		var positions := PackedVector3Array()
		var normals := PackedVector3Array()
		for p in source:
			positions.append(Vector3(p.x, p.z, -p.y))
			normals.append(Vector3.UP)
		var arrays: Array = []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = positions
		arrays[Mesh.ARRAY_NORMAL] = normals
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		instance.material_override = _ground_material()
		terrain_root.add_child(instance)

func _material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.9
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material

func _ground_material() -> ShaderMaterial:
	# Grid is a world-space display pattern; it does not create collision.
	var shader := Shader.new()
	shader.code = "shader_type spatial; render_mode cull_disabled; varying vec3 world; void vertex(){world=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;} void fragment(){vec2 q=abs(fract(world.xz/16.0+0.5)-0.5);float line=1.0-step(0.018,min(q.x,q.y)); ALBEDO=mix(vec3(0.10,0.20,0.21),vec3(0.23,0.36,0.36),line); ROUGHNESS=0.95;}"
	var material := ShaderMaterial.new()
	material.shader = shader
	return material

func _label(text: String, size := 16, color := INK) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", size)
	label.add_theme_color_override("font_color", color)
	return label

func _button(text: String, action: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(96, 40)
	button.pressed.connect(action)
	return button

func _build_interface() -> void:
	var bg := ColorRect.new()
	bg.color = Color("0b1112")
	bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(bg)
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 18)
	add_child(margin)
	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 10)
	margin.add_child(layout)
	var header := HBoxContainer.new()
	layout.add_child(header)
	if ResourceLoader.exists("res://assets/open_champions_avatar.png"):
		var badge := TextureRect.new()
		badge.texture = load("res://assets/open_champions_avatar.png")
		badge.custom_minimum_size = Vector2(56, 56)
		badge.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		badge.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		header.add_child(badge)
	var titles := VBoxContainer.new()
	titles.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(titles)
	titles.add_child(_label("OPEN CHAMPIONS", 24, GOLD))
	titles.add_child(_label("Return to Arms · Creature movement test", 15, MUTED))
	var health := VBoxContainer.new()
	header.add_child(health)
	health_label = _label("Health 100 / 100", 18)
	health.add_child(health_label)
	health_bar = ProgressBar.new()
	health_bar.custom_minimum_size = Vector2(190, 14)
	health_bar.show_percentage = false
	health_bar.value = 100
	health.add_child(health_bar)
	var container := SubViewportContainer.new()
	container.stretch = true
	container.size_flags_vertical = Control.SIZE_EXPAND_FILL
	container.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	container.custom_minimum_size = Vector2(0, 160)
	layout.add_child(container)
	viewport = SubViewport.new()
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	container.add_child(viewport)
	_world = Node3D.new()
	viewport.add_child(_world)
	terrain_root = Node3D.new()
	terrain_root.name = "OwnedTerrainDisplay"
	_world.add_child(terrain_root)
	actor = Node3D.new()
	actor.name = "SessionActorDisplay"
	_world.add_child(actor)
	actor_visual = MeshInstance3D.new()
	actor.add_child(actor_visual)
	_default_mesh = CapsuleMesh.new()
	_default_mesh.radius = 4
	_default_mesh.height = 12
	camera = Camera3D.new()
	camera.fov = 48
	camera.far = 2000
	camera.current = true
	_world.add_child(camera)
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-55, -35, 0)
	light.light_energy = 1.3
	_world.add_child(light)
	var environment := WorldEnvironment.new()
	var sky := Environment.new()
	sky.background_mode = Environment.BG_COLOR
	sky.background_color = Color("102126")
	sky.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	sky.ambient_light_color = Color("adc8ce")
	sky.ambient_light_energy = 0.55
	environment.environment = sky
	_world.add_child(environment)
	var controls := HFlowContainer.new()
	controls.add_theme_constant_override("h_separation", 8)
	layout.add_child(controls)
	pause_button = _button("Pause", func(): set_paused(not paused))
	controls.add_child(pause_button)
	controls.add_child(_button("Reset", reset_session))
	controls.add_child(_button("Stop", stop_session))
	controls.add_child(_button("Open terrain…", func(): set_paused(true); terrain_dialog.popup_centered_ratio(0.8)))
	controls.add_child(_button("Choose appearance…", func(): set_paused(true); asset_dialog.popup_centered_ratio(0.8)))
	controls.add_child(_button("Default appearance", restore_default_actor))
	controls.add_child(_button("Archive browser", open_archive_browser))
	status_label = _label("Starting…", 16)
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(status_label)
	terrain_label = _label("Practice ground", 14, MUTED)
	layout.add_child(terrain_label)
	asset_label = _label("Practice figure · appearance only", 14, MUTED)
	layout.add_child(asset_label)
	var scope := _label("Creature movement with practice controls · Dry ground only; no original player controls, animation or Norrath support.\nTerrain display: one plain, unrotated, constant-height surface. Private files stay on your computer.", 12, MUTED)
	scope.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(scope)
	terrain_dialog = FileDialog.new()
	terrain_dialog.title = "Open your terrain file"
	terrain_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	terrain_dialog.access = FileDialog.ACCESS_FILESYSTEM
	terrain_dialog.filters = PackedStringArray(["*.world ; Terrain file"])
	terrain_dialog.file_selected.connect(load_terrain_file)
	add_child(terrain_dialog)
	asset_dialog = FileDialog.new()
	asset_dialog.title = "Choose your model archive"
	asset_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	asset_dialog.access = FileDialog.ACCESS_FILESYSTEM
	asset_dialog.filters = PackedStringArray(["*.gob, *.lmp ; Model archive"])
	asset_dialog.file_selected.connect(_asset_file_chosen)
	add_child(asset_dialog)
	model_dialog = ConfirmationDialog.new()
	model_dialog.title = "Choose a static appearance"
	model_dialog.ok_button_text = "Use appearance"
	model_dialog.min_size = Vector2i(500, 320)
	model_list = ItemList.new()
	model_list.custom_minimum_size = Vector2(480, 260)
	model_dialog.add_child(model_list)
	model_dialog.confirmed.connect(_asset_confirmed)
	add_child(model_dialog)
	restore_default_actor()

func _asset_file_chosen(path: String) -> void:
	var result := select_asset_archive(path)
	if result.get("ok", false):
		if model_list.item_count == 0:
			status_label.text = "This archive has no supported model entries."
		else:
			model_list.select(0)
			model_dialog.popup_centered()

func _asset_confirmed() -> void:
	var selected := model_list.get_selected_items()
	if selected.size() == 1:
		apply_asset_model(int(model_list.get_item_metadata(selected[0])))

func _run_export_smoke() -> void:
	# Test script is supplied by the independent test lane.
	var path := "res://tests/session_sandbox_cases.gd"
	if not ResourceLoader.exists(path):
		push_error("Session scene checks are unavailable.")
		get_tree().quit(1)
		return
	var tests = load(path).new()
	var result = await tests.run(self)
	get_tree().quit(0 if result else 1)
