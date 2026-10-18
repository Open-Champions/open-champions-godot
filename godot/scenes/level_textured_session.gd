extends "res://scenes/session_sandbox.gd"
const LevelTextureMaterial = preload("res://scenes/level_texture_material.gd")

# Optional textured RTA base layer over the existing Session terrain controller.
# No data is bundled; the caller must select a matching WORLD/TEX pair.
func _ready() -> void:
	super._ready()
	var args := OS.get_cmdline_user_args()
	var world_path := ""
	var tex_path := ""
	for i in range(args.size() - 1):
		if args[i] == "--world": world_path = args[i + 1]
		if args[i] == "--tex": tex_path = args[i + 1]
	if not world_path.is_empty() and not tex_path.is_empty():
		call_deferred("load_textured_terrain", world_path, tex_path)

func load_textured_terrain(world_path: String, tex_path: String) -> bool:
	if controller == null or not ClassDB.class_exists("ChampionsLevelTexturePreview"):
		return false
	var loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
	var result: Dictionary = loader.load_world_tex(world_path, tex_path)
	loader.free()
	if not result.get("ok", false):
		status_label.text = str(result.get("error", "The WORLD/TEX pair was refused."))
		return false
	var candidate := _prepare_textured_root(result)
	if candidate == null:
		status_label.text = "The textured ground could not be prepared."
		return false
	# Prepare both the native collision state and the textured subtree before
	# publishing either. This mirrors the inherited candidate/commit sequence.
	var prepared: Dictionary = controller.prepare_terrain_file(world_path)
	if not prepared.get("ok", false):
		candidate.free()
		status_label.text = str(prepared.get("error", "The WORLD could not be loaded."))
		return false
	var committed: Dictionary = controller.commit_terrain_file(prepared.token)
	if not committed.get("ok", false):
		candidate.free()
		status_label.text = str(committed.get("error", "The WORLD could not be committed."))
		return false
	_publish_terrain_root(candidate)
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	_observe(committed)
	terrain_label.text = "Textured RTA ground"
	return true

func _prepare_textured_root(result: Dictionary) -> Node3D:
	var textures: Array[Texture2D] = []
	for source in result.images:
		var image := Image.create_from_data(source.width, source.height, false, Image.FORMAT_RGBA8, source.rgba)
		if image == null: return null
		var texture := ImageTexture.create_from_image(image)
		if texture == null: return null
		textures.append(texture)
	var root_node := Node3D.new()
	root_node.name = "OwnedTexturedTerrainDisplay"
	for surface in result.surfaces:
		if surface.image < 0 or surface.image >= textures.size():
			root_node.free()
			return null
		var arrays: Array = []
		arrays.resize(Mesh.ARRAY_MAX)
		var mapped := PackedVector3Array()
		for point in surface.positions:
			mapped.push_back(Vector3(point.x, point.z, -point.y))
		arrays[Mesh.ARRAY_VERTEX] = mapped
		arrays[Mesh.ARRAY_TEX_UV] = surface.uv
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		if mesh.get_surface_count() != 1:
			root_node.free()
			return null
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		instance.material_override = LevelTextureMaterial.create(textures[surface.image], surface)
		root_node.add_child(instance)
	return root_node
