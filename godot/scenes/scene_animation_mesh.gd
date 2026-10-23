extends RefCounted

# Presentation geometry only. The bridge supplies the positions in native Z-up.
static func prepare_rest(model: Dictionary) -> Dictionary:
	if not model.get("ok", false):
		return {"ok": false, "error": str(model.get("error", "The body rest pose is unavailable."))}
	# The model reader returns the same bounded per-corner geometry as a sample.
	# Supply only the shape metadata expected by the shared mesh builder.
	return prepare({"ok": true, "pairing_verified": false, "duration_ticks": 0,
		"tick": 0.0, "meshes": model.get("meshes", []),
		"triangle_count": model.get("triangle_count", -1)})

static func prepare(sample: Dictionary) -> Dictionary:
	if typeof(sample.get("ok")) != TYPE_BOOL or not sample["ok"]:
		return {"ok": false, "error": str(sample.get("error", "The selected pair could not be sampled."))}
	if typeof(sample.get("pairing_verified")) != TYPE_BOOL or sample["pairing_verified"]:
		return {"ok": false, "error": "Expected an explicitly unverified model and clip pair."}
	if typeof(sample.get("duration_ticks")) != TYPE_INT or typeof(sample.get("tick")) not in [TYPE_INT, TYPE_FLOAT]:
		return {"ok": false, "error": "The sample has an invalid time range."}
	var duration: int = sample["duration_ticks"]
	var tick: float = sample["tick"]
	if duration < 0 or duration > 4294967295 or not is_finite(tick) or tick < 0 or tick > duration:
		return {"ok": false, "error": "The sample time is outside the clip."}
	if typeof(sample.get("meshes")) != TYPE_ARRAY or sample["meshes"].size() > 64:
		return {"ok": false, "error": "The sample has an invalid mesh list."}
	if typeof(sample.get("triangle_count")) != TYPE_INT or sample["triangle_count"] < 0 or sample["triangle_count"] > 200000:
		return {"ok": false, "error": "The sample has an invalid triangle count."}
	var mesh := ArrayMesh.new()
	var bounds := AABB()
	var first := true
	var corners := 0
	var visible_corners := 0
	var indices := {}
	for entry in sample["meshes"]:
		if typeof(entry) != TYPE_DICTIONARY or typeof(entry.get("index")) != TYPE_INT or typeof(entry.get("visible_by_default")) != TYPE_BOOL or typeof(entry.get("positions")) != TYPE_PACKED_VECTOR3_ARRAY:
			return {"ok": false, "error": "The sample has incomplete mesh data."}
		var index: int = entry["index"]
		if index < 0 or index >= 64 or indices.has(index):
			return {"ok": false, "error": "The sample has invalid or duplicate mesh indices."}
		indices[index] = true
		var source: PackedVector3Array = entry["positions"]
		if source.size() % 3 != 0 or source.size() > 600000 - corners:
			return {"ok": false, "error": "The sample exceeds the corner limit or has incomplete triangles."}
		corners += source.size()
		var mapped := PackedVector3Array()
		if entry["visible_by_default"]:
			mapped.resize(source.size())
		for i in range(source.size()):
			var p := source[i]
			if not p.is_finite() or maxf(absf(p.x), maxf(absf(p.y), absf(p.z))) > 1e15:
				return {"ok": false, "error": "The sample contains nonfinite or out-of-range positions."}
			if entry["visible_by_default"]:
				# Exactly one native Z-up to Godot Y-up conversion.
				var point := Vector3(p.x, p.z, -p.y)
				mapped[i] = point
				bounds = bounds.expand(point) if not first else AABB(point, Vector3.ZERO)
				first = false
		if not mapped.is_empty():
			visible_corners += mapped.size()
			var arrays := []
			arrays.resize(Mesh.ARRAY_MAX)
			arrays[Mesh.ARRAY_VERTEX] = mapped
			# Posed normals, same corner order, native Z-up mapped with positions.
			# Absent on older samples; the environment pass needs them.
			if entry.get("normals") is PackedVector3Array:
				var source_normals: PackedVector3Array = entry.normals
				if source_normals.size() != source.size():
					return {"ok": false, "error": "The sample normals do not match its positions."}
				var normals := PackedVector3Array()
				normals.resize(source_normals.size())
				for i in range(source_normals.size()):
					var n: Vector3 = source_normals[i]
					if not n.is_finite():
						return {"ok": false, "error": "The sample contains a nonfinite normal."}
					normals[i] = Vector3(n.x, n.z, -n.y)
				arrays[Mesh.ARRAY_NORMAL] = normals
			mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	if corners != sample["triangle_count"] * 3:
		return {"ok": false, "error": "The sample triangle count does not match its meshes."}
	if first or visible_corners == 0 or not bounds.size.is_finite() or bounds.size.length() < 0.0001:
		return {"ok": false, "error": "The selected pair has no drawable default-visible geometry."}
	return {"ok": true, "mesh": mesh, "bounds": bounds, "visible_corners": visible_corners}

static func fit(bounds: AABB) -> Dictionary:
	var scale_value := minf(8.0 / maxf(bounds.size.x, 0.001), 8.0 / maxf(bounds.size.z, 0.001))
	scale_value = minf(scale_value, 12.0 / maxf(bounds.size.y, 0.001))
	var offset := Vector3(-bounds.get_center().x, -bounds.position.y, -bounds.get_center().z) * scale_value
	return {"scale": scale_value, "offset": offset}
