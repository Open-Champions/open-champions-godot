extends RefCounted

const LevelTextureMaterial = preload("res://scenes/level_texture_material.gd")

static func build(result: Dictionary, native_lighting := false) -> Node3D:
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
		if native_lighting:
			instance.set_meta("native_fx_world_dynamic", bool(surface.get("dynamic_light", true)))
		instance.material_override = LevelTextureMaterial.create(textures[surface.image], surface, native_lighting)
		root_node.add_child(instance)
	return root_node
