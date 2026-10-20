extends RefCounted

const LevelTextureMaterial = preload("res://scenes/level_texture_material.gd")
const LevelDrawCamera = preload("res://scenes/level_draw_camera.gd")

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
	root_node.set_script(LevelDrawCamera)
	var loader: Object = null
	if native_lighting and ClassDB.class_exists("ChampionsLevelTexturePreview"):
		loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
	for surface in result.surfaces:
		if surface.image < 0 or surface.image >= textures.size():
			root_node.free()
			if loader != null: loader.free()
			return null
		var arrays: Array = []
		arrays.resize(Mesh.ARRAY_MAX)
		var mapped := PackedVector3Array()
		for point in surface.positions:
			mapped.push_back(Vector3(point.x, point.z, -point.y))
		arrays[Mesh.ARRAY_VERTEX] = mapped
		arrays[Mesh.ARRAY_TEX_UV] = surface.uv
		var flags := 0
		if surface.get("environment", false):
			var custom := PackedFloat32Array()
			custom.resize(mapped.size() * 4)
			var normals: PackedVector3Array = surface.get("stored_normals", PackedVector3Array())
			for i in mapped.size():
				if i < normals.size():
					custom[i * 4] = normals[i].x
					custom[i * 4 + 1] = normals[i].y
					custom[i * 4 + 2] = normals[i].z
			arrays[Mesh.ARRAY_CUSTOM0] = custom
			flags = Mesh.ARRAY_CUSTOM_RGBA_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
		if native_lighting and not surface.get("dynamic_light", true):
			if loader == null:
				root_node.free()
				return null
			var channel := int(loader.world_constant_channel(LevelTextureMaterial.PLAY_LIGHT_BASE, int(surface.light_constant)))
			var color := Color(channel / 255.0, channel / 255.0, channel / 255.0, 1.0)
			var colors := PackedColorArray()
			colors.resize(mapped.size())
			colors.fill(color)
			arrays[Mesh.ARRAY_COLOR] = colors
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, flags)
		if mesh.get_surface_count() != 1:
			root_node.free()
			if loader != null: loader.free()
			return null
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		if native_lighting:
			instance.set_meta("native_fx_world_dynamic", bool(surface.get("dynamic_light", true)))
		instance.set_meta("level_draw_surface", surface)
		# Later packets in a layer are closer in the transparent sort. The depth
		# test still keeps nearer fragments. Layers use the material priority.
		instance.sorting_offset = -float(surface.get("draw_order", 0)) * 100000.0
		instance.material_override = LevelTextureMaterial.create(textures[surface.image], surface, native_lighting)
		root_node.add_child(instance)
	if loader != null:
		loader.free()
	var automap: Dictionary = result.get("automap", {})
	root_node.set_meta("level_draw_automap", automap)
	root_node.set_meta("automap_quads", int(automap.get("quads", []).size()) if automap.get("ok", false) else 0)
	return root_node

static func build_automap(automap: Dictionary, atlas: Texture2D) -> MeshInstance3D:
	if atlas == null or not automap.get("ok", false):
		return null
	var quads: Array = automap.get("quads", [])
	if quads.is_empty():
		return null
	var vertices := PackedVector3Array()
	var uvs := PackedVector2Array()
	var colors := PackedColorArray()
	var width := float(atlas.get_width())
	var height := float(atlas.get_height())
	if width <= 0.0 or height <= 0.0:
		return null
	for quad in quads:
		var uv: PackedInt32Array = quad.uv
		var alpha: PackedInt32Array = quad.alpha
		if uv.size() != 8 or alpha.size() != 4:
			return null
		var column := float(quad.column)
		var row := float(quad.row)
		# Cell corners in grid order. Screen placement of this grid is not
		# documented, so the mesh stays in cell coordinates.
		var corners := [Vector3(column, 0, -row), Vector3(column, 0, -(row + 1)), Vector3(column + 1, 0, -row), Vector3(column + 1, 0, -row), Vector3(column, 0, -(row + 1)), Vector3(column + 1, 0, -(row + 1))]
		var corner_index := [0, 1, 2, 2, 1, 3]
		for n in 6:
			var index: int = corner_index[n]
			vertices.push_back(corners[n])
			uvs.push_back(Vector2(float(uv[index * 2]) / 16.0 / width, float(uv[index * 2 + 1]) / 16.0 / height))
			colors.push_back(Color(1, 1, 1, float(alpha[index]) / 255.0))
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_COLOR] = colors
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var instance := MeshInstance3D.new()
	instance.name = "AutomapTiles"
	instance.mesh = mesh
	instance.material_override = LevelTextureMaterial.automap_material(atlas)
	instance.sorting_offset = -1.0e9
	instance.visible = false
	return instance
