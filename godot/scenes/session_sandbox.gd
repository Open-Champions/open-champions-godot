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
# Set by a scripted run that put the player beside the lowest-HP boar with
# controller.fight_place_player (DIAGNOSTIC; not an original routine). Shown on
# the health label so a screenshot cannot be read as an organic approach.
var diagnostic_placement_at_weakest := false
var held_commands := 0
var pending_damage := false
var paused := false
var stopped := false
var automatic_steps := true
var animation_cache_label: Label
var health_label: Label
var health_bar: ProgressBar
var status_label: Label
var terrain_label: Label
var asset_label: Label
var scope_label: Label
var pause_button: Button
var save_format: OptionButton
var save_dialog: FileDialog
var load_dialog: FileDialog
var terrain_dialog: FileDialog
var asset_dialog: FileDialog
var model_dialog: ConfirmationDialog
var model_list: ItemList
var asset_archive_path := ""
var asset_fingerprint := ""
var asset_rows: Array = []
var displayed_asset_name := "Practice figure"
var _default_mesh: CapsuleMesh
var _last_clock_usec := 0
var _world: Node3D
var main_layout: VBoxContainer
var diagnostic_controls: HFlowContainer
var cache_controls: HFlowContainer

func _ready() -> void:
	# Archive tools retain their legacy return path. Only a shell-owned visit
	# opts that return back into the entry flow; standalone diagnostics remain.
	if get_script().resource_path == "res://scenes/session_sandbox.gd" and get_tree().root.get_meta("scene_play_flow_return", false) and "--sandbox-smoke-test" not in OS.get_cmdline_user_args() and "--browser-smoke-test" not in OS.get_cmdline_user_args():
		get_tree().root.remove_meta("scene_play_flow_return")
		get_tree().change_scene_to_file.call_deferred("res://scenes/play_flow.tscn")
		return
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

func _release_native_resources() -> void:
	automatic_steps = false
	held_commands = 0
	pending_damage = false
	# The bridge owns ArrayMesh resources as well as the simulation. Drop it
	# before the renderer shuts down, including ordinary window close/navigation.
	controller = null
	inspector = null
	raw_observation.clear()

func _exit_tree() -> void:
	_release_native_resources()

func _process(_delta: float) -> void:
	# Monotonic wall time belongs to presentation. Godot delta may be clamped
	# after a slow frame, so it must not control the fixed native tick count.
	var now := Time.get_ticks_usec()
	var elapsed := now - _last_clock_usec if _last_clock_usec != 0 else 0
	_last_clock_usec = now
	if automatic_steps and controller != null and not paused and not stopped:
		var before: int = raw_observation.get("submissions", 0)
		var command := held_commands | (16 if pending_damage else 0)
		var result: Dictionary = controller.advance_time(elapsed, command)
		if result.get("submissions", before) != before:
			pending_damage = false
			_observe(result)
		elif not result.get("ok", false):
			_observe(result)
		_apply_presentation()

func _apply_presentation() -> void:
	if controller == null:
		return
	var poses: Dictionary = controller.get_presentation()
	var native_position: Vector3 = poses.previous_position.lerp(poses.current_position, poses.alpha)
	actor.position = Vector3(native_position.x, native_position.z, -native_position.y)
	actor.rotation.y = lerp_angle(poses.previous_facing, poses.current_facing, poses.alpha)
	# Camera motion is display-only and follows the interpolated actor.
	camera.position = actor.position + Vector3(56, 70, 80)
	camera.look_at(actor.position + Vector3(0, 3, 0), Vector3.UP)
	_capture_player_camera()

func _native_camera_vector(v: Vector3) -> Vector3:
	return Vector3(v.x, -v.z, v.y)

func _capture_player_camera() -> void:
	# The camera and viewport are host presentation state. The native bundle
	# flattens these four actual side planes at each queried player's height.
	if controller == null or camera == null:
		return
	var forward := -camera.global_transform.basis.z.normalized()
	var inside := camera.global_position + forward * 10.0
	var sides := PackedVector3Array()
	for plane in camera.get_frustum():
		var normal: Vector3 = plane.normal
		if absf(normal.dot(forward)) > 0.99:
			continue
		if plane.distance_to(inside) < 0.0:
			normal = -normal
		sides.push_back(_native_camera_vector(normal))
	if sides.size() != 4:
		return
	var native_forward := _native_camera_vector(forward)
	var yaw := roundi(atan2(native_forward.y, native_forward.x) * 65536.0 / TAU) & 0xffff
	# HOST-SOURCED: the yaw above is this sandbox camera's, not Camera_getYaw
	# (+0x68), so Player_move steers relative to the host camera. Camera3D also
	# has no source for the original live follow mode (flag stays null).
	controller.set_player_camera(0, yaw, null, _native_camera_vector(camera.global_position), sides)

func _input(event: InputEvent) -> void:
	if handle_input_event(event):
		get_viewport().set_input_as_handled()

func handle_input_event(event: InputEvent) -> bool:
	if not event is InputEventKey or event.echo:
		return false
	var key: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
	if event.pressed and (terrain_dialog.visible or asset_dialog.visible or model_dialog.visible or save_dialog.visible or load_dialog.visible):
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
	if paused or stopped or terrain_dialog.visible or asset_dialog.visible or model_dialog.visible or save_dialog.visible or load_dialog.visible:
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
	_last_clock_usec = 0
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

func set_facing_test(enabled: bool) -> void:
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	if controller != null:
		_observe(controller.set_facing_test(enabled))

func set_recovery_test(enabled: bool) -> void:
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	if controller != null:
		_observe(controller.set_recovery_test(enabled))

func set_animation_cache_test(enabled: bool) -> void:
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	if controller != null:
		_observe(controller.set_animation_cache_test(enabled))

func animation_cache_command(kind: int) -> void:
	if controller != null:
		var result: Dictionary = controller.animation_cache_command(kind, 1)
		if result.get("ok", false):
			_observe(result)
		else:
			_show_error(str(result.get("error", "Animation command refused.")))

func reset_session() -> void:
	_last_clock_usec = 0
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
	_apply_presentation()
	paused = result.paused
	stopped = result.stopped
	health_bar.max_value = maxf(float(result.max_hp), 0.01)
	health_bar.value = clampf(float(result.hp), 0.0, health_bar.max_value)
	health_label.text = "Health %s / %s" % [str(snappedf(float(result.hp), 0.01)), str(snappedf(float(result.max_hp), 0.01))]
	if result.get("recovery_enabled", false):
		health_label.text += " · Mana %.2f / %.2f\nPending HP %.2f · Mana %.2f" % [result.mana, result.max_mana, result.pending_health, result.pending_mana]
	if result.get("facing_enabled", false) and not result.get("fight_player_ready", false):
		health_label.text += "\nFacing test · angle %d · velocity %d · mode %d" % [result.facing_raw, result.angular_velocity_raw, result.turn_mode]
	if result.get("fight_player_ready", false):
		health_label.text += "\nStarting equipment: %s · weapon class %d (%s, not campaign evidence)" % [
			str(result.get("fight_starting_equipment", "unknown")), int(result.get("fight_player_weapon_class", -1)),
			str(result.get("fight_player_weapon_class_source", "unknown"))]
		var fight_counts: Dictionary = result.get("fight_counts", {})
		health_label.text += "\nLevel %d · Experience %d · kills %d · loot rolls %d (items %d, no ground object) · Gold %d" % [
			int(result.get("fight_player_level", -1)), int(result.get("fight_player_experience", -1)),
			int(fight_counts.get("deaths", 0)), int(fight_counts.get("loot_calls", 0)),
			int(fight_counts.get("loot_items", 0)), int(result.get("gold", -1))]
		if diagnostic_placement_at_weakest:
			health_label.text += "\nDIAGNOSTIC placement at the lowest-HP boar"
	animation_cache_label.visible = result.get("animation_cache_enabled", false)
	if animation_cache_label.visible:
		animation_cache_label.text = "Animation-cache diagnostic · synthetic data, no model pose\nTrack %s · sample track %s · time raw %s · event %s\nRaw joint values %s" % [result.animation_track, result.animation_sample_track, result.animation_time_raw, result.animation_event, str(result.animation_joints_raw)]
	terrain_label.text = "%s · %s" % [result.terrain_name, result.get("terrain_note", "")]
	pause_button.text = "Resume" if paused else "Pause"
	if result.failed:
		status_label.text = "Stopped: %s · Reset to start again." % result.error
	elif stopped and result.error == "ready_tick_limit":
		status_label.text = "Diagnostic limit reached (%s ready ticks) · Reset explicitly to start again." % result.get("ready_tick_limit", 512)
	elif stopped:
		status_label.text = "Stopped · Reset to start again."
	elif paused:
		status_label.text = "Paused · Resume when you are ready."
	else:
		status_label.text = "Move with W A S D · H tests health · Esc pauses"
	if result.get("save_history_pending", false):
		status_label.text += " · Historical save data not yet checked."
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
	var prepared: Dictionary = controller.prepare_terrain_file(path)
	if not prepared.get("ok", false):
		status_label.text = str(prepared.get("error", "The terrain could not be loaded."))
		return false
	var candidate := _prepare_terrain_root(prepared.terrain)
	if candidate == null:
		controller.cancel_terrain_file(prepared.token)
		status_label.text = "The ground could not be displayed."
		return false
	var result: Dictionary = controller.commit_terrain_file(prepared.token)
	if not result.get("ok", false):
		candidate.free()
		status_label.text = str(result.get("error", "The terrain could not be loaded."))
		return false
	_publish_terrain_root(candidate)
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	_observe(result)
	return true

func save_session_file(path: String) -> bool:
	if controller == null:
		return false
	var result: Dictionary
	if save_format.get_selected_id() == 1:
		result = controller.save_snapshot_file(path)
	else:
		result = controller.save_file(path)
	status_label.text = "Saved native scene." if result.get("ok", false) else str(result.get("error", "Save failed."))
	return result.get("ok", false)

func load_session_file(path: String) -> bool:
	if controller == null:
		return false
	var prepared: Dictionary = _prepare_native_save(path)
	if not prepared.get("ok", false):
		status_label.text = str(prepared.get("error", "Load failed."))
		return false
	var candidate := _prepare_terrain_root(prepared.terrain)
	if candidate == null:
		controller.cancel_terrain_file(prepared.token)
		status_label.text = "The saved ground could not be displayed."
		return false
	var result: Dictionary = controller.commit_terrain_file(prepared.token)
	if not result.get("ok", false):
		candidate.free()
		status_label.text = str(result.get("error", "Load failed."))
		return false
	_publish_terrain_root(candidate)
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	_observe(result)
	return true

func _prepare_native_save(path: String) -> Dictionary:
	return controller.prepare_save_file(path)

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

func _prepare_terrain_root(result: Dictionary) -> Node3D:
	# Mesh resources were already created by native preparation. Build the whole
	# subtree/material before publishing the corresponding native controller.
	if not result.get("ok", false) or not result.get("surfaces", []) is Array or result.surfaces.is_empty():
		return null
	var candidate := Node3D.new()
	candidate.name = "OwnedTerrainDisplay"
	var material: Material
	if result.get("mode", "") == "static_world":
		var neutral := _material(Color("9faeab"))
		neutral.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		material = neutral
	else:
		material = _ground_material()
	for surface in result.surfaces:
		if not surface is Dictionary or not surface.get("mesh") is ArrayMesh or surface.mesh.get_surface_count() != 1:
			candidate.free()
			return null
		var instance := MeshInstance3D.new()
		instance.mesh = surface.mesh
		instance.material_override = material
		candidate.add_child(instance)
	return candidate

func _publish_terrain_root(candidate: Node3D) -> void:
	var previous := terrain_root
	_world.add_child(candidate)
	terrain_root = candidate
	_world.remove_child(previous)
	previous.queue_free()

func _rebuild_terrain(result: Dictionary) -> void:
	var candidate := _prepare_terrain_root(result)
	if candidate != null:
		_publish_terrain_root(candidate)
	_last_clock_usec = 0

func _material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.9
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material

func _ground_material() -> ShaderMaterial:
	# Grid is a world-space display pattern; it does not create collision.
	var shader := Shader.new()
	shader.code = "shader_type spatial; render_mode cull_disabled; varying vec3 world; void vertex(){world=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;} void fragment(){vec2 q=abs(fract(world.xz/16.0+0.5)-0.5);float line=1.0-step(0.018,min(q.x,q.y)); ALBEDO=mix(vec3(0.10,0.20,0.21),vec3(0.23,0.36,0.36),line)*COLOR.rgb; ROUGHNESS=0.95;}"
	var material := ShaderMaterial.new()
	material.shader = shader
	material.set_meta("native_fx_vertex_colors", true)
	material.set_meta("native_fx_unshaded_code", shader.code.replace("render_mode cull_disabled;", "render_mode unshaded, cull_disabled;"))
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
	main_layout = layout
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
	diagnostic_controls = controls
	controls.add_theme_constant_override("h_separation", 8)
	layout.add_child(controls)
	pause_button = _button("Pause", func(): set_paused(not paused))
	controls.add_child(pause_button)
	controls.add_child(_button("Reset", reset_session))
	controls.add_child(_button("Stop", stop_session))
	controls.add_child(_button("Facing test (opt in)", func(): set_facing_test(true)))
	controls.add_child(_button("Facing off / restart", func(): set_facing_test(false)))
	controls.add_child(_button("Recovery test (opt in)", func(): set_recovery_test(true)))
	controls.add_child(_button("Recovery off / restart", func(): set_recovery_test(false)))
	save_format = OptionButton.new()
	save_format.add_item("Replay (smaller file)", 0)
	save_format.add_item("Snapshot (faster load)", 1)
	save_format.tooltip_text = "Choose the format for Save. Load accepts either format."
	save_format.custom_minimum_size.y = 40
	controls.add_child(save_format)
	controls.add_child(_button("Save…", func(): set_paused(true); save_dialog.popup_centered_ratio(0.8)))
	controls.add_child(_button("Load…", func(): set_paused(true); load_dialog.popup_centered_ratio(0.8)))
	controls.add_child(_button("Open terrain…", func(): set_paused(true); terrain_dialog.popup_centered_ratio(0.8)))
	controls.add_child(_button("Choose appearance…", func(): set_paused(true); asset_dialog.popup_centered_ratio(0.8)))
	controls.add_child(_button("Default appearance", restore_default_actor))
	controls.add_child(_button("Archive browser", open_archive_browser))
	var animation_controls := HFlowContainer.new()
	cache_controls = animation_controls
	animation_controls.add_theme_constant_override("h_separation", 8)
	layout.add_child(animation_controls)
	animation_controls.add_child(_button("Animation-cache diagnostic (opt in)", func(): set_animation_cache_test(true)))
	animation_controls.add_child(_button("Cache off / restart", func(): set_animation_cache_test(false)))
	animation_controls.add_child(_button("Clip start", func(): animation_cache_command(0)))
	animation_controls.add_child(_button("Clip stop", func(): animation_cache_command(1)))
	animation_controls.add_child(_button("Clip restart", func(): animation_cache_command(2)))
	animation_cache_label = _label("", 12, MUTED)
	animation_cache_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	animation_cache_label.visible = false
	layout.add_child(animation_cache_label)
	status_label = _label("Starting…", 16)
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(status_label)
	terrain_label = _label("Practice ground", 14, MUTED)
	terrain_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(terrain_label)
	asset_label = _label("Practice figure · appearance only", 14, MUTED)
	asset_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(asset_label)
	scope_label = _label("Creature movement with practice controls · Dry ground only; no original player dispatch or Norrath support.\nWORLD ground uses its static shapes; models, lights and effects are omitted. Private files stay on your computer.", 12, MUTED)
	scope_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(scope_label)
	save_dialog = FileDialog.new()
	save_dialog.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	save_dialog.title = "Save native scene"
	save_dialog.access = FileDialog.ACCESS_FILESYSTEM
	save_dialog.filters = PackedStringArray(["*.ocnsave ; Native scene save"])
	save_dialog.current_file = "scene.ocnsave"
	save_dialog.file_selected.connect(save_session_file)
	add_child(save_dialog)
	load_dialog = FileDialog.new()
	load_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	load_dialog.title = "Load native scene"
	load_dialog.access = FileDialog.ACCESS_FILESYSTEM
	load_dialog.filters = save_dialog.filters
	load_dialog.file_selected.connect(load_session_file)
	add_child(load_dialog)
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
	# This node must remain alive until its coroutine finishes; release owners
	# and queue its display children, then let the renderer process destruction.
	tests = null
	_release_native_resources()
	for child in get_children():
		child.queue_free()
	await get_tree().process_frame
	await get_tree().process_frame
	get_tree().quit(0 if result else 1)
