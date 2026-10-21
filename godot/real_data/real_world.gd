extends RefCounted

# Water tiles and moving pieces of the loaded game-folder level, through the
# native ChampionsRealWorld adapter (water services and level-flow pieces).
# Drawn water is the documented colour, slope UVs and layer-3 blend
# (docs/WATER_DRAW.md). The wave steps on the simulation tick. The claim-frame
# seed stays refused, so a new block is a zero grid.
# Piece poses come from level_flow::move_piece; collision changes stay in the
# adapter's terrain copy (the play session keeps its own collision).
const WATER_SHADER := """shader_type spatial;
render_mode unshaded, cull_disabled, blend_mix, depth_draw_never, depth_test_disabled;
uniform sampler2D water_tex : source_color, filter_linear, repeat_enable;
uniform float fix = 0.796875;
void fragment() {
	vec3 texel = texture(water_tex, UV / vec2(textureSize(water_tex, 0))).rgb;
	ALBEDO = texel * COLOR.rgb;
	ALPHA = fix;
}
"""
const ENTER_RADIUS_RAW := 0x41400000 # water_test(12), level-flow section 12

var native: RefCounted
var info: Dictionary = {}
var water_root: Node3D
var water_instance: MeshInstance3D
var water_shader: Shader
var water_materials: Array = []
var water_texture: Texture2D
var water_texture_refused := false
var water_tiles_refused := 0 # tiles not drawn because waterenv did not decode
var wave_frame := -1
var piece_instances: Dictionary = {} # chunk -> Array of MeshInstance3D
var streamed_root: Node3D
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

func set_water_texture(bytes: PackedByteArray) -> Dictionary:
	var image: Image = null
	if native != null and not bytes.is_empty():
		var decoded: Dictionary = native.water_texture(bytes)
		if decoded.get("ok", false):
			image = decoded.image
	water_texture_refused = image == null
	if image == null:
		# The texture is the waterenv image water_setup was given (water-draw.md
		# sections 1 and 7.1 at research-textures a9486c7). No substitute texel
		# is documented, so water whose image does not decode is not drawn.
		water_texture = null
		water_tiles_refused = info.get("water", []).size()
		if water_instance != null: water_instance.visible = false
		if not "water texture refused" in errors:
			errors.append("water texture refused")
		return {"ok": false, "width": 0, "height": 0, "tiles_refused": water_tiles_refused}
	water_texture = ImageTexture.create_from_image(image)
	water_tiles_refused = 0
	for material in water_materials:
		material.set_shader_parameter("water_tex", water_texture)
	if water_instance != null: water_instance.visible = true
	return {"ok": true, "width": image.get_width(), "height": image.get_height(), "tiles_refused": 0}

func _water_material(fix: float) -> ShaderMaterial:
	if water_shader == null:
		water_shader = Shader.new()
		water_shader.code = WATER_SHADER
	var material := ShaderMaterial.new()
	material.shader = water_shader
	material.render_priority = 3
	material.set_shader_parameter("fix", fix)
	if water_texture != null:
		material.set_shader_parameter("water_tex", water_texture)
	water_materials.append(material)
	return material

func _build_water(terrain_root: Node3D) -> void:
	if info.water.is_empty() or terrain_root == null or native == null: return
	var sync: Dictionary = native.water_sync(0, false)
	if not sync.get("ok", false) or sync.get("mesh") == null:
		if not "water mesh refused" in errors: errors.append("water mesh refused")
		return
	wave_frame = 0
	var instance := MeshInstance3D.new()
	instance.name = "WaterTiles"
	instance.mesh = sync.mesh
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var surfaces: Array = sync.get("surfaces", [])
	for i in surfaces.size():
		instance.set_surface_override_material(i, _water_material(float(surfaces[i].fix)))
	water_instance = instance
	# Hidden until set_water_texture decodes the level's waterenv image.
	instance.visible = water_texture != null
	# A child of the terrain root, inserted before the first layer-4 surface.
	# render_priority 3 is the documented layer: after opaque WORLD, before
	# flag 0x200. Water does not write or test depth.
	water_root = Node3D.new()
	water_root.name = "RealWater"
	water_root.add_child(instance)
	var insert := terrain_root.get_child_count()
	for i in terrain_root.get_child_count():
		var child := terrain_root.get_child(i)
		if child is MeshInstance3D:
			var mat: Material = child.get_active_material(0)
			if mat != null and mat.render_priority >= 4:
				insert = i
				break
	terrain_root.add_child(water_root)
	terrain_root.move_child(water_root, insert)

# Textured terrain children are built one per decoded surface, in order. A
# streamed display tags each moving-piece node with its chunk instead and
# reports when its resident cells change.
func _index_pieces(terrain_root: Node3D) -> void:
	piece_instances.clear()
	if info.pieces.is_empty() or terrain_root == null or not terrain_root.has_meta("level_texture_pair"): return
	var wanted := {}
	for piece in info.pieces:
		for chunk in piece.chunks: wanted[int(chunk)] = true
	if terrain_root.has_meta("level_texture_streamed"):
		for child in terrain_root.get_children():
			var chunk := int(child.get_meta("level_draw_chunk", -1))
			if wanted.has(chunk):
				if not piece_instances.has(chunk): piece_instances[chunk] = []
				piece_instances[chunk].append(child)
		streamed_root = terrain_root
		if not terrain_root.cells_changed.is_connected(_cells_changed):
			terrain_root.cells_changed.connect(_cells_changed)
		return
	var surfaces: Array = terrain_root.get_meta("level_texture_pair").decoded.surfaces
	var children := terrain_root.get_children()
	for i in range(mini(surfaces.size(), children.size())):
		var chunk := int(surfaces[i].chunk)
		if wanted.has(chunk) and children[i] is MeshInstance3D:
			if not piece_instances.has(chunk): piece_instances[chunk] = []
			piece_instances[chunk].append(children[i])

func _cells_changed() -> void:
	if not is_instance_valid(streamed_root): return
	_index_pieces(streamed_root)
	apply_pieces()

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

func _advance_waves(raw: Dictionary) -> void:
	if water_instance == null or native == null or not raw.has("ready_ticks"): return
	var tick := int(raw.ready_ticks)
	if tick < wave_frame: return
	var frozen := bool(raw.get("paused", false))
	var last := {}
	if frozen:
		last = native.water_sync(tick, true)
	else:
		var frame := wave_frame + 1
		while frame <= tick:
			last = native.water_sync(frame, false)
			frame += 1
	wave_frame = tick
	if last.get("dirty", false):
		water_instance.mesh = last.mesh

# water_test at the session's player position after each observation.
func observe(raw: Dictionary) -> void:
	if native == null: return
	_advance_waves(raw)
	if info.get("water", []).is_empty() or not raw.has("position_raw"): return
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
		var texture := "texture refused, not drawn" if water_texture_refused else "textured"
		parts.append("%d water tiles (layer 3, %s, wave on the simulation tick)%s" % [info.water.size(), texture, " · in water" if in_water else ""])
	if not info.get("pieces", []).is_empty():
		parts.append("%d moving pieces, %d claimed (level-flow poses)" % [info.pieces.size(), int(info.get("claimed", 0))])
	return " · ".join(parts)
