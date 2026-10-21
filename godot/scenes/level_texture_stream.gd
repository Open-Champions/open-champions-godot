extends "res://scenes/level_draw_camera.gd"

# Textured WORLD display that keeps only the 3 x 3 texture cells around the
# camera focus resident, as the original does (research docs/level-flow.md
# 13.1, docs/formats/world.md): a chunk whose cell is not resident is not
# drawn, the missing cell nearest the focus is read first, one per frame, and
# at most nine cells are held. g_cameraFocus is the camera position plus
# forward x 600 (camera_updateFocus 0x18a520). Before play the focus is the
# session start. Display only; the simulation never reads these nodes.
const FOCUS_AHEAD := 600.0
const SLOTS := 9

signal cells_changed

var streamer: RefCounted
var seed_focus := Vector2.ZERO
var focus := -1
var wanted: PackedInt32Array = PackedInt32Array()
var resident: Dictionary = {} # cell -> Array of MeshInstance3D
var cell_loads := 0
var cell_evictions := 0

func _init() -> void:
	name = "OwnedTexturedTerrainDisplay"
	set_meta("level_texture_streamed", true)

func _ready() -> void:
	super._ready()
	var summary: Dictionary = get_meta("level_texture_summary", {})
	var automap: Dictionary = summary.get("automap", {})
	set_meta("level_draw_automap", automap)
	set_meta("automap_quads", int(automap.get("quads", []).size()) if automap.get("ok", false) else 0)
	if focus == -1 and streamer != null: show_complete(_focus())

# The committed session start; the first window is complete before publishing.
func setup_focus(committed: Dictionary) -> void:
	var start: Variant = committed.get("position", null)
	if start is Vector3: seed_focus = Vector2(start.x, start.y)
	show_complete(seed_focus)

func show_complete(point: Vector2) -> void:
	if streamer == null: return
	_retarget(point)
	while _load_next(): pass

func _process(delta: float) -> void:
	super._process(delta)
	if streamer == null: return
	_retarget(_focus())
	_load_next()

func _focus() -> Vector2:
	if not is_inside_tree(): return seed_focus
	if not _playing():
		var start: Variant = _observation().get("position", null)
		return Vector2(start.x, start.y) if start is Vector3 else seed_focus
	var camera := get_viewport().get_camera_3d()
	if camera == null: return seed_focus
	var position := _to_native(camera.global_position)
	var forward := _to_native(-camera.global_transform.basis.z)
	return Vector2(position.x + forward.x * FOCUS_AHEAD, position.y + forward.y * FOCUS_AHEAD)

func _playing() -> bool:
	var node: Node = get_parent()
	while node != null:
		if "play_mode" in node: return bool(node.play_mode)
		node = node.get_parent()
	return false

func _observation() -> Dictionary:
	var node: Node = get_parent()
	while node != null:
		if "raw_observation" in node: return node.raw_observation
		node = node.get_parent()
	return {}

func _retarget(point: Vector2) -> void:
	var cell := int(streamer.focus_cell(point.x, point.y))
	if cell == focus: return
	focus = cell
	wanted = streamer.window(cell)

# Reads the first missing window cell, evicting a resident cell outside the
# window when all nine slots are taken. Returns false when nothing was missing.
func _load_next() -> bool:
	for id in wanted:
		var cell := int(id)
		if resident.has(cell): continue
		if resident.size() >= SLOTS:
			_drop(_farthest_outside())
		var view: Dictionary = streamer.cell_view(cell, LevelTextureMaterial.PLAY_LIGHT_BASE)
		resident[cell] = _build(cell, view) if view.get("ok", false) else []
		cell_loads += 1
		_order()
		cells_changed.emit()
		return true
	return false

# Which slot the original reuses is not documented; the client reuses the
# resident cell farthest from the focus cell.
@warning_ignore("integer_division")
func _farthest_outside() -> int:
	var pick := -1
	var reach := -1
	for key in resident:
		var cell := int(key)
		if cell in wanted: continue
		var dr := cell / 100 - focus / 100
		var dc := cell % 100 - focus % 100
		if dr * dr + dc * dc > reach:
			reach = dr * dr + dc * dc
			pick = cell
	return pick

func _drop(cell: int) -> void:
	for instance in resident.get(cell, []):
		if is_instance_valid(instance):
			remove_child(instance)
			instance.free()
	resident.erase(cell)
	cell_evictions += 1

func _build(cell: int, view: Dictionary) -> Array:
	var textures: Array[Texture2D] = []
	for source in view.images:
		var image := Image.create_from_data(int(source.width), int(source.height), false, Image.FORMAT_RGBA8, source.rgba)
		textures.append(ImageTexture.create_from_image(image) if image != null else null)
	var materials: Dictionary = {}
	var built: Array = []
	for group in view.groups:
		var texture: Texture2D = textures[int(group.image)]
		if texture == null: continue
		var arrays: Array = []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = group.positions
		arrays[Mesh.ARRAY_TEX_UV] = group.uv
		if group.has("colors"): arrays[Mesh.ARRAY_COLOR] = group.colors
		var flags := 0
		if group.has("custom0"):
			arrays[Mesh.ARRAY_CUSTOM0] = group.custom0
			flags = Mesh.ARRAY_CUSTOM_RGBA_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, flags)
		var surface := {
			"alpha1": int(group.alpha1), "test1": int(group.test1),
			"blend_enabled": bool(group.blend_enabled), "environment": bool(group.environment),
			"sort_layer": int(group.sort_layer), "dynamic_light": bool(group.dynamic_light),
			"light_constant": int(group.light_constant), "quarter": int(group.quarter),
			"draw_order": int(group.draw_order), "cell": cell,
		}
		if environment_texture != null and surface.environment: surface["env_ready"] = true
		var key := str([int(group.image), surface.alpha1, surface.test1, surface.blend_enabled, surface.dynamic_light, surface.light_constant, surface.sort_layer])
		var material: ShaderMaterial = materials.get(key)
		if material == null or surface.environment:
			material = LevelTextureMaterial.create(texture, surface, true)
			if surface.get("env_ready", false):
				material.set_shader_parameter("env_texture", environment_texture)
				material.set_shader_parameter("use_env", true)
			materials[key] = material
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		instance.material_override = material
		instance.set_meta("native_fx_world_dynamic", surface.dynamic_light)
		instance.set_meta("level_draw_surface", surface)
		instance.set_meta("level_draw_cell", cell)
		instance.set_meta("level_draw_triangles", int(group.triangles))
		if not bool(group.atlas):
			instance.set_meta("level_draw_slot", Vector2i(int(group.chunk), int(group.batch)))
			if bool(group.moving): instance.set_meta("level_draw_chunk", int(group.chunk))
		instance.sorting_offset = -float(surface.draw_order) * 100000.0
		add_child(instance)
		built.append(instance)
	return built

# Draw order over the resident cells (draw_slots): later batches of a layer
# sort nearer, as level_texture_terrain.gd does for the whole level.
func _order() -> void:
	var cells := PackedInt32Array()
	for cell in resident: cells.push_back(int(cell))
	var slots: PackedInt64Array = streamer.window_order(cells)
	var order: Dictionary = {}
	for i in range(0, slots.size(), 3):
		order[Vector2i(int(slots[i]), int(slots[i + 1]))] = int(slots[i + 2])
	for list in resident.values():
		for instance in list:
			if not instance.has_meta("level_draw_slot"): continue
			var slot: Vector2i = instance.get_meta("level_draw_slot")
			if order.has(slot): instance.sorting_offset = -float(order[slot]) * 100000.0

func resident_cells() -> PackedInt32Array:
	var cells := PackedInt32Array()
	for cell in resident: cells.push_back(int(cell))
	return cells

func release_resources() -> void:
	for cell in resident.keys(): _drop(int(cell))
	streamer = null

func _exit_tree() -> void:
	release_resources()
