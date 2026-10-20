extends RefCounted

# Water tiles and moving pieces of the loaded game-folder level, through the
# native ChampionsRealWorld adapter (water services and level-flow pieces).
# Water placement is documented: each tile spans its chunk bounds and is drawn
# at chunk translation z / 4. Its colour, alpha FIX, waves and UV mapping are
# not documented, so tiles use one labelled neutral translucent material.
# Piece poses come from level_flow::move_piece; collision changes stay in the
# adapter's terrain copy (the play session keeps its own collision).
const NEUTRAL_WATER := Color(0.55, 0.6, 0.66, 0.45)
const ENTER_RADIUS_RAW := 0x41400000 # water_test(12), level-flow section 12

var native: RefCounted
var info: Dictionary = {}
var water_root: Node3D
var piece_instances: Dictionary = {} # chunk -> Array of MeshInstance3D
var ticks_observed := 0
var ticks_in_water := 0
var entries := 0
var in_water := false
var last_height := 0.0
var errors: Array[String] = []

func open(world_path: String, objects: PackedByteArray, terrain_root: Node3D) -> Dictionary:
	if not ClassDB.class_exists("ChampionsRealWorld"): return {"ok": false, "error": "adapter unavailable"}
	native = ClassDB.instantiate("ChampionsRealWorld")
	info = native.open(world_path, objects)
	if not info.get("ok", false): return info
	_build_water(terrain_root)
	_index_pieces(terrain_root)
	apply_pieces()
	return info

func _build_water(terrain_root: Node3D) -> void:
	if info.water.is_empty() or terrain_root == null: return
	var vertices := PackedVector3Array()
	for tile in info.water:
		var a: Vector3 = tile.min
		var b: Vector3 = tile.max
		var y := float(tile.drawn_height)
		var p00 := Vector3(a.x, y, a.z)
		var p10 := Vector3(b.x, y, a.z)
		var p11 := Vector3(b.x, y, b.z)
		var p01 := Vector3(a.x, y, b.z)
		vertices.append_array(PackedVector3Array([p00, p10, p11, p00, p11, p01]))
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.albedo_color = NEUTRAL_WATER
	var instance := MeshInstance3D.new()
	instance.name = "NeutralWaterTiles"
	instance.mesh = mesh
	instance.material_override = material
	# Not a direct terrain child, so native world lighting leaves it alone.
	water_root = Node3D.new()
	water_root.name = "RealWater"
	water_root.add_child(instance)
	terrain_root.add_child(water_root)

# Textured terrain children are built one per decoded surface, in order.
func _index_pieces(terrain_root: Node3D) -> void:
	piece_instances.clear()
	if info.pieces.is_empty() or terrain_root == null or not terrain_root.has_meta("level_texture_pair"): return
	var wanted := {}
	for piece in info.pieces:
		for chunk in piece.chunks: wanted[int(chunk)] = true
	var surfaces: Array = terrain_root.get_meta("level_texture_pair").decoded.surfaces
	var children := terrain_root.get_children()
	for i in range(mini(surfaces.size(), children.size())):
		var chunk := int(surfaces[i].chunk)
		if wanted.has(chunk) and children[i] is MeshInstance3D:
			if not piece_instances.has(chunk): piece_instances[chunk] = []
			piece_instances[chunk].append(children[i])

func apply_pieces() -> int:
	var moved := 0
	if native == null: return moved
	for draw in native.piece_draws():
		for instance in piece_instances.get(int(draw.chunk), []):
			instance.transform = draw.transform
			moved += 1
	return moved

# AutoProp_setPassable on a claimed piece; false for an unclaimed one.
func set_door(piece: int, open: bool) -> bool:
	if native == null or not native.set_door(piece, open): return false
	apply_pieces()
	return true

func drawn_claimed_pieces() -> int:
	var n := 0
	for i in range(info.get("pieces", []).size()):
		var piece: Dictionary = info.pieces[i]
		if not piece.claimed: continue
		for chunk in piece.chunks:
			if piece_instances.has(int(chunk)):
				n += 1
				break
	return n

func first_drawn_claimed_piece() -> int:
	for i in range(info.get("pieces", []).size()):
		var piece: Dictionary = info.pieces[i]
		if not piece.claimed: continue
		for chunk in piece.chunks:
			if piece_instances.has(int(chunk)): return i
	return -1

# water_test at the session's player position after each observation.
func observe(raw: Dictionary) -> void:
	if native == null or info.get("water", []).is_empty() or not raw.has("position_raw"): return
	var position := PackedInt64Array()
	for word in raw.position_raw: position.push_back(int(word))
	var result: Dictionary = native.water_test(position)
	if not result.get("ok", false):
		if not str(result.get("error", "")) in errors: errors.append(str(result.get("error", "")))
		return
	ticks_observed += 1
	if result.in_water:
		ticks_in_water += 1
		last_height = float(result.height)
		if not in_water: entries += 1
	in_water = bool(result.in_water)

func label() -> String:
	var parts: Array[String] = []
	if not info.get("water", []).is_empty():
		parts.append("%d water tiles (placement exact; neutral colour, waves gated)%s" % [info.water.size(), " · in water" if in_water else ""])
	if not info.get("pieces", []).is_empty():
		parts.append("%d moving pieces, %d claimed (level-flow poses)" % [info.pieces.size(), int(info.get("claimed", 0))])
	return " · ".join(parts)
