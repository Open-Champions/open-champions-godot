extends Node3D
const LevelTextureMaterial = preload("res://scenes/level_texture_material.gd")

# Isolated base-colour preview. Call load_level with an explicitly selected RTA
# WORLD and its matching <LEVEL>.TEX; no original data is bundled.
var status := "Select a WORLD and matching TEX file."
var terrain_root: Node3D
var camera: Camera3D

func _ready() -> void:
	terrain_root = Node3D.new()
	terrain_root.name = "TexturedGround"
	add_child(terrain_root)
	camera = Camera3D.new()
	camera.name = "PreviewCamera"
	camera.current = true
	add_child(camera)
	var args := OS.get_cmdline_user_args()
	var world_path := ""
	var tex_path := ""
	for i in range(args.size() - 1):
		if args[i] == "--world": world_path = args[i + 1]
		if args[i] == "--tex": tex_path = args[i + 1]
	if not world_path.is_empty() and not tex_path.is_empty():
		load_level(world_path, tex_path)

func load_level(world_path: String, tex_path: String) -> bool:
	if not ClassDB.class_exists("ChampionsLevelTexturePreview"):
		status = "Level texture extension is unavailable."
		return false
	var loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
	var result: Dictionary = loader.load_world_tex(world_path, tex_path)
	loader.free()
	if not result.get("ok", false):
		status = str(result.get("error", "The selected level was refused."))
		return false
	var images: Array = []
	for source in result.images:
		var image := Image.create_from_data(source.width, source.height, false, Image.FORMAT_RGBA8, source.rgba)
		if image == null:
			status = "A decoded texture could not be prepared."
			return false
		var texture := ImageTexture.create_from_image(image)
		if texture == null:
			status = "A decoded texture could not be uploaded."
			return false
		images.append(texture)
	var candidate := Node3D.new()
	candidate.name = "TexturedGround"
	var bounds := AABB()
	var first := true
	for surface in result.surfaces:
		var arrays: Array = []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = surface.positions
		arrays[Mesh.ARRAY_TEX_UV] = surface.uv
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		if mesh.get_surface_count() != 1:
			candidate.free()
			status = "A textured ground mesh could not be prepared."
			return false
		var material := LevelTextureMaterial.create(images[surface.image], surface)
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		instance.material_override = material
		candidate.add_child(instance)
		for point in surface.positions:
			if first:
				bounds = AABB(point, Vector3.ZERO)
				first = false
			else:
				bounds = bounds.expand(point)
	if first:
		candidate.free()
		status = "The level has no supported textured triangles."
		return false
	var old := terrain_root
	remove_child(old)
	old.free()
	terrain_root = candidate
	add_child(terrain_root)
	var center := bounds.get_center()
	var radius := maxf(bounds.size.length(), 8.0)
	camera.position = center + Vector3(radius * 0.6, -radius * 0.8, radius * 0.7)
	camera.look_at(center, Vector3(0, 0, 1))
	status = "Textured ground: %d triangles, %d textures." % [result.triangles, images.size()]
	return true
