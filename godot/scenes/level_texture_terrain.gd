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
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, flags)
		if mesh.get_surface_count() != 1:
			root_node.free()
			return null
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		if native_lighting:
			instance.set_meta("native_fx_world_dynamic", bool(surface.get("dynamic_light", true)))
			if not surface.get("dynamic_light", true):
				# The constant colour waits for the loaded level's base level
				# (set_constant_light). Until then: base texture only.
				instance.set_meta("level_constant_light", int(surface.light_constant))
				instance.set_meta("level_constant_base", -1)
		instance.set_meta("level_draw_surface", surface)
		# Later packets in a layer are closer in the transparent sort. The depth
		# test still keeps nearer fragments. Layers use the material priority.
		instance.sorting_offset = -float(surface.get("draw_order", 0)) * 100000.0
		instance.material_override = LevelTextureMaterial.create(textures[surface.image], surface, native_lighting)
		root_node.add_child(instance)
	var automap: Dictionary = result.get("automap", {})
	root_node.set_meta("level_draw_automap", automap)
	root_node.set_meta("automap_quads", int(automap.get("quads", []).size()) if automap.get("ok", false) else 0)
	return root_node

# Constant-light chunk (flags 0x100 / 0x2000 / 0x200): world_constant_channel of
# the level's base level and the chunk constant. The base level is 0x80, or
# 0x40 / 0x20 by the level's flag word (render-pipeline 5.1; lighting.md 10.2
# at research-fx 8e66747); any other value is refused and the chunk keeps its
# base texture only. Presented as texel x colour / 128 (render-pipeline 9.3).
static func set_constant_light(instance: MeshInstance3D, base: int, loader: Object) -> bool:
	if instance == null or instance.mesh == null or not instance.has_meta("level_constant_light"):
		return false
	var documented := (base == 0x80 or base == 0x40 or base == 0x20) and loader != null
	var arrays: Array = instance.mesh.surface_get_arrays(0)
	var count: int = arrays[Mesh.ARRAY_VERTEX].size()
	if documented:
		var channel := int(loader.world_constant_channel(base, int(instance.get_meta("level_constant_light"))))
		var colors := PackedColorArray()
		colors.resize(count)
		colors.fill(Color(channel / 255.0, channel / 255.0, channel / 255.0, 1.0))
		arrays[Mesh.ARRAY_COLOR] = colors
	else:
		arrays[Mesh.ARRAY_COLOR] = null
	var flags := 0
	if arrays[Mesh.ARRAY_CUSTOM0] != null:
		flags = Mesh.ARRAY_CUSTOM_RGBA_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, flags)
	instance.mesh = mesh
	if instance.material_override is ShaderMaterial:
		instance.material_override.set_shader_parameter("modulate_scale", 255.0 / 128.0 if documented else 1.0)
	instance.set_meta("level_constant_base", base if documented else -1)
	return documented

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
