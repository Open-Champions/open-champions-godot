extends RefCounted

# Rest-pose presentation of shared-World actors. Definition resource identities,
# visibility and scale come from native constructors at runtime. Animated
# appearance, held-part attachment and root trajectories remain unverified.
const MeshBuilder = preload("res://scenes/scene_animation_mesh.gd")
const RealCreature = preload("res://real_data/real_creature.gd")

var status := "No enemy model loaded."
var instances: Dictionary = {} # native address -> MeshInstance3D
var identities: Dictionary = {}
var models: Dictionary = {} # runtime model identity + visibility -> presentation
var parent: Node3D
var real: Node
var reader: RefCounted

static func _row_index(rows: Array, name: String) -> int:
	for i in range(rows.size()):
		if rows[i] is Dictionary and str(rows[i].get("kind", "")) == "LMP" and str(rows[i].get("name", "")).to_lower() == name:
			return i
	return -1

func clear() -> void:
	for address in instances:
		if is_instance_valid(instances[address]): instances[address].queue_free()
	instances.clear()
	identities.clear()
	models.clear()
	status = "No enemy model loaded."

func install(source: Node, inspector: RefCounted, host: Node3D) -> void:
	clear()
	real = source
	reader = inspector
	parent = host

static func _identity(row: Dictionary) -> String:
	return JSON.stringify([row.get("model_lump", ""), row.get("model_entry", ""),
		int(row.get("model_mask", 0)), row.get("texture_entry", "")])

func _retire(address: int) -> void:
	var instance: MeshInstance3D = instances.get(address)
	if is_instance_valid(instance):
		instance.get_parent().remove_child(instance)
		instance.queue_free()
	instances.erase(address)
	identities.erase(address)

func _model(row: Dictionary) -> Dictionary:
	var key := _identity(row)
	if models.has(key): return models[key]
	models[key] = {}
	var path := str(real.files.get(str(row.get("model_lump", "")).to_lower(), ""))
	if path.is_empty():
		status = "Enemy model refused: class archive is absent."
		return {}
	var listing: Dictionary = reader.inspect_archive(path)
	if not listing.get("ok", false):
		status = "Enemy model refused: class archive could not be inspected."
		return {}
	var rows: Array = listing.rows
	var index := _row_index(rows, str(row.get("model_entry", "")).to_lower())
	if index < 0:
		status = "Enemy model refused: definition body entry is absent."
		return {}
	var rest: Dictionary = reader.inspect_model(path, index, "rta", str(listing.fingerprint))
	var mask := int(row.get("model_mask", 0))
	for part in rest.get("meshes", []):
		part["visible_by_default"] = bool(mask & (1 << int(part.index)))
	var prepared: Dictionary = MeshBuilder.prepare_rest(rest)
	if not prepared.get("ok", false):
		status = "Enemy model refused: " + str(prepared.get("error", "rest pose unavailable"))
		return {}
	var binder: RefCounted = RealCreature.new()
	binder.environment = real.environment
	var bound: Dictionary = binder.bind_named(path, rows, index, str(row.get("texture_entry", "")))
	for part in binder.meshes:
		part["visible_by_default"] = bool(mask & (1 << int(part.index)))
	var result := {"mesh": prepared.mesh, "binder": binder,
		"textured": bool(bound.get("ok", false)), "foot": -float(prepared.bounds.position.y)}
	models[key] = result
	status = "Class body rest poses drawn; animated appearance and held-part attachment remain unverified."
	return result

func update(observation: Dictionary) -> int:
	if parent == null or real == null or reader == null: return 0
	var seen := {}
	var drawn := 0
	for row in observation.get("fight_enemies", []):
		var address := int(row.handle)
		seen[address] = true
		var identity := _identity(row)
		var model := _model(row)
		if model.is_empty():
			_retire(address)
			continue
		var instance: MeshInstance3D = instances.get(address)
		if identities.get(address, "") != identity:
			_retire(address)
			instance = null
		if instance == null or not is_instance_valid(instance):
			instance = MeshInstance3D.new()
			instance.mesh = model.mesh
			if not model.textured or not model.binder.apply(instance):
				var neutral := StandardMaterial3D.new()
				neutral.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
				neutral.cull_mode = BaseMaterial3D.CULL_DISABLED
				neutral.albedo_color = Color("a07858")
				instance.material_override = neutral
			parent.add_child(instance)
			instances[address] = instance
			identities[address] = identity
		var size := float(row.get("model_scale", 1.0))
		instance.scale = Vector3.ONE * size
		var p: Vector3 = row.position
		instance.position = Vector3(p.x, p.z + float(model.foot) * size, -p.y)
		instance.rotation.y = float(int(row.facing) & 0xffff) * TAU / 65536.0
		instance.transparency = 0.6 if bool(row.get("dead", false)) else 0.0
		instance.visible = true
		drawn += 1
	for address in instances.keys():
		if not seen.has(address):
			_retire(address)
	return drawn
