extends Node3D

# Feeds the documented environment rows from the current camera. Dynamic WORLD
# colour still comes from native-fx shade_world; this node only updates the
# environment matrix and installs a caller-supplied environment image.
const LevelTextureMaterial = preload("res://scenes/level_texture_material.gd")
var loader: Object
var level_options := 0
var environment_texture: Texture2D

func _ready() -> void:
	_loader()
	_apply_rows()

func _process(_delta: float) -> void:
	_apply_rows()

func set_environment_texture(texture: Texture2D) -> void:
	environment_texture = texture
	for child in get_children():
		if not child is MeshInstance3D:
			continue
		var surface: Dictionary = child.get_meta("level_draw_surface", {})
		if not surface.get("environment", false):
			continue
		surface["env_ready"] = true
		var base: Texture2D = child.material_override.get_shader_parameter("base_texture")
		var lit: bool = bool(child.get_meta("native_fx_world_dynamic", false)) or bool(child.material_override.get_meta("native_fx_vertex_colors", false))
		child.material_override = LevelTextureMaterial.create(base, surface, lit)
		child.material_override.set_shader_parameter("env_texture", texture)
		child.material_override.set_shader_parameter("use_env", true)
		child.set_meta("level_draw_surface", surface)
	_apply_rows()

func set_level_options(options: int) -> void:
	level_options = options

func set_automap_atlas(atlas: Texture2D) -> MeshInstance3D:
	var previous := get_node_or_null("AutomapTiles")
	if previous != null:
		previous.free()
	var builder = load("res://scenes/level_texture_terrain.gd")
	var instance: MeshInstance3D = builder.build_automap(get_meta("level_draw_automap", {}), atlas)
	if instance == null:
		return null
	add_child(instance)
	return instance

func _loader() -> Object:
	if loader == null and ClassDB.class_exists("ChampionsLevelTexturePreview"):
		loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
	return loader

func _sim_frame() -> int:
	var node: Node = self
	while node:
		if "raw_observation" in node:
			return int(node.raw_observation.get("combat_sim_frame", 0))
		node = node.get_parent()
	return 0

func _camera_components(camera: Camera3D) -> PackedFloat64Array:
	# Native world is (x, z, -y) from this scene. g_cameraMatrix (0x44f110) is
	# three rows that are the camera-space images of the world x, y, z axes
	# (render-pipeline.md section 3, research-textures 391cf6b): row i is
	# (right[i], down[i], forward[i]). axis_xy reads x and y of each row.
	var right := _to_native(camera.global_transform.basis.x)
	var down := _to_native(-camera.global_transform.basis.y)
	var forward := _to_native(-camera.global_transform.basis.z)
	return PackedFloat64Array([
		right.x, down.x, forward.x,
		right.y, down.y, forward.y,
		right.z, down.z, forward.z])

func _to_native(value: Vector3) -> Vector3:
	return Vector3(value.x, -value.z, value.y)

func _apply_rows() -> void:
	var native := _loader()
	if native == null:
		return
	var camera := get_viewport().get_camera_3d() if is_inside_tree() else null
	if camera == null:
		return
	var components := _camera_components(camera)
	var frame := _sim_frame()
	for child in get_children():
		if not child is MeshInstance3D or child.material_override == null:
			continue
		var surface: Dictionary = child.get_meta("level_draw_surface", {})
		if not surface.get("environment", false):
			continue
		var quarter := int(surface.get("quarter", -1))
		var rows: Dictionary = native.environment_rows(components, frame, level_options, quarter)
		if not rows.get("ok", false):
			continue
		var row4: PackedFloat64Array = rows.row4
		var row5: PackedFloat64Array = rows.row5
		child.material_override.set_shader_parameter("env_row4", Vector4(row4[0], row4[1], row4[2], row4[3]))
		child.material_override.set_shader_parameter("env_row5", Vector4(row5[0], row5[1], row5[2], row5[3]))
		if environment_texture != null:
			child.material_override.set_shader_parameter("env_texture", environment_texture)
			child.material_override.set_shader_parameter("use_env", true)
