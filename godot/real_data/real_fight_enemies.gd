extends RefCounted

# The shared-World fight's WarBoar-family actors, drawn at their native
# positions with the class model's rest pose (warboar.vif and its companion
# texture, read from the owner's folder at run time). Animation playback of the
# boar clips is not wired to the draw: the rest pose is shown. Scale is the
# class constructor's +0xdc (0.75, 0.6 for WarBoarSmall; handoff round 2).
const MeshBuilder = preload("res://scenes/scene_animation_mesh.gd")
const RealCreature = preload("res://real_data/real_creature.gd")
const LARGE_SCALE := 0.75
const SMALL_SCALE := 0.6

var status := "No enemy model loaded."
var mesh: ArrayMesh
var binder: RefCounted
var foot_offset := 0.0
var instances: Dictionary = {} # native address -> MeshInstance3D
var parent: Node3D
var textured := false

static func _row_index(rows: Array, name: String) -> int:
	for i in range(rows.size()):
		if rows[i] is Dictionary and str(rows[i].get("kind", "")) == "LMP" and str(rows[i].get("name", "")).to_lower() == name:
			return i
	return -1

func clear() -> void:
	for address in instances:
		if is_instance_valid(instances[address]): instances[address].queue_free()
	instances.clear()
	mesh = null
	binder = null
	textured = false
	status = "No enemy model loaded."

# lump_name: the class definition's archive, read from the executable's table.
func install(real: Node, reader: RefCounted, host: Node3D, lump_name: String, model_name: String) -> bool:
	clear()
	parent = host
	var path := str(real.files.get(lump_name.to_lower(), ""))
	if path.is_empty():
		status = "Enemy model refused: class archive is absent from the folder."
		return false
	var listing: Dictionary = reader.inspect_archive(path)
	if not listing.get("ok", false):
		status = "Enemy model refused: class archive could not be inspected."
		return false
	var rows: Array = listing.rows
	var row := _row_index(rows, model_name.to_lower()) if not model_name.is_empty() else -1
	if row < 0 and model_name.is_empty():
		# The class archive holds one body model (the definition's slot 1).
		for i in range(rows.size()):
			if rows[i] is Dictionary and str(rows[i].get("kind", "")) == "LMP" and str(rows[i].get("name", "")).to_lower().ends_with(".vif"):
				row = i
				break
	if row < 0:
		status = "Enemy model refused: class model entry is absent."
		return false
	var rest: Dictionary = reader.inspect_model(path, row, "rta", str(listing.fingerprint))
	var prepared: Dictionary = MeshBuilder.prepare_rest(rest)
	if not prepared.get("ok", false):
		status = "Enemy model refused: " + str(prepared.get("error", "rest pose unavailable"))
		return false
	mesh = prepared.mesh
	foot_offset = -float(prepared.bounds.position.y)
	binder = RealCreature.new()
	binder.environment = real.environment
	var bound: Dictionary = binder.bind(path, rows, row)
	textured = bound.get("ok", false)
	status = "Class model rest pose drawn%s; boar clips are not wired to the draw." % ("" if textured else " untextured (" + str(bound.get("error", "texture refused")) + ")")
	return true

func _make_instance(small: bool) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var scale := SMALL_SCALE if small else LARGE_SCALE
	instance.scale = Vector3(scale, scale, scale)
	if textured and binder != null and binder.apply(instance):
		pass
	else:
		var neutral := StandardMaterial3D.new()
		neutral.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		neutral.cull_mode = BaseMaterial3D.CULL_DISABLED
		neutral.albedo_color = Color("a07858")
		instance.material_override = neutral
	return instance

# One call per presented frame with the controller observation.
func update(observation: Dictionary) -> int:
	if mesh == null or parent == null: return 0
	var rows: Array = observation.get("fight_enemies", [])
	var seen := {}
	var drawn := 0
	for row in rows:
		var address := int(row.handle)
		seen[address] = true
		var instance: MeshInstance3D = instances.get(address)
		if instance == null or not is_instance_valid(instance):
			instance = _make_instance(str(row.kind) == "WarBoarSmall")
			parent.add_child(instance)
			instances[address] = instance
		var p: Vector3 = row.position
		var scale := SMALL_SCALE if str(row.kind) == "WarBoarSmall" else LARGE_SCALE
		instance.position = Vector3(p.x, p.z + foot_offset * scale, -p.y)
		instance.rotation.y = float(int(row.facing) & 0xffff) * TAU / 65536.0
		# PRESENTATION ONLY: the death-animation clip choice is undocumented, so
		# a dead boar (hit points at or below zero, state 0x14) keeps the rest
		# pose and is drawn faded. This is not the original's death look.
		instance.transparency = 0.6 if bool(row.get("dead", false)) else 0.0
		instance.visible = true
		drawn += 1
	for address in instances.keys():
		if not seen.has(address) and is_instance_valid(instances[address]):
			instances[address].visible = false
	return drawn
