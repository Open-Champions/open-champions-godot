extends RefCounted

# A static, fitted display mesh. It never supplies position, bounds, collision,
# health or animation state to the native Session.
static func build(result: Dictionary) -> Dictionary:
	if not result.get("ok", false) or not result.get("meshes") is Array:
		return {"ok": false, "error": "The selected model could not be displayed."}
	var surfaces: Array[PackedVector3Array] = []
	var first := true
	var bounds := AABB()
	var corners := 0
	for item in result.meshes:
		if not item is Dictionary or not item.get("positions") is PackedVector3Array:
			return {"ok": false, "error": "The selected model has invalid geometry."}
		var source: PackedVector3Array = item.positions
		corners += source.size()
		if source.size() % 3 != 0 or corners > 600000:
			return {"ok": false, "error": "The selected model exceeds the display limit."}
		var positions := PackedVector3Array()
		for p in source:
			if not p.is_finite():
				return {"ok": false, "error": "The selected model has invalid coordinates."}
			var mapped := Vector3(p.x, p.z, -p.y)
			positions.append(mapped)
			if item.get("visible_by_default", false):
				if first:
					bounds = AABB(mapped, Vector3.ZERO)
					first = false
				else:
					bounds = bounds.expand(mapped)
		if item.get("visible_by_default", false) and not positions.is_empty():
			surfaces.append(positions)
	if first or surfaces.is_empty() or bounds.size.length() < 0.0001:
		return {"ok": false, "error": "The selected model has no visible geometry."}
	var mesh := ArrayMesh.new()
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("dec384")
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.roughness = 0.85
	for positions in surfaces:
		var arrays: Array = []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = positions
		# Flat face normals are presentation only; no animation/skinning claim.
		var normals := PackedVector3Array()
		for i in range(0, positions.size(), 3):
			var n := (positions[i + 1] - positions[i]).cross(positions[i + 2] - positions[i]).normalized()
			if n.length_squared() == 0.0:
				n = Vector3.UP
			normals.append_array(PackedVector3Array([n, n, n]))
		arrays[Mesh.ARRAY_NORMAL] = normals
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		mesh.surface_set_material(mesh.get_surface_count() - 1, material)
	var fit := minf(8.0 / maxf(bounds.size.x, 0.001), 8.0 / maxf(bounds.size.z, 0.001))
	fit = minf(fit, 12.0 / maxf(bounds.size.y, 0.001))
	var offset := Vector3(-bounds.get_center().x, -bounds.position.y, -bounds.get_center().z) * fit
	return {"ok": true, "mesh": mesh, "scale": fit, "offset": offset, "bounds": bounds}
