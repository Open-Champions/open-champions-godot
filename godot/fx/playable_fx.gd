extends Node3D

# Authored cosmetic integration policy. No gameplay inference or shared RNG.
const Renderer = preload("res://fx/native_fx_renderer.gd")
const Fixture = preload("res://fx/playable_fx_fixture.gd")
var renderer: Node3D
var scene: Control
var gore_detail := 2
var definition_source := "synthetic test definitions / test sheets"
var definition_json := ""
var last_tick := 0
var hit_hooks := 0
var death_hooks := 0
var spawned_particles := 0
var last_events: Array = []
var refusals: PackedStringArray = []
var lighting_calls := {"model": 0, "world": 0}
var enabled := false
var last_render_frame := -1
var policy := ""

func configure(owner: Control) -> void:
	scene = owner
	definition_json = Fixture.definitions()
	reset()

func reset() -> void:
	if renderer != null:
		renderer.clear_vertex_lighting()
		remove_child(renderer)
		renderer.queue_free()
	renderer = Renderer.new()
	renderer.native_z_up = true
	Fixture.configure(renderer)
	add_child(renderer)
	var loaded: Dictionary = renderer.bridge.load_definitions_json(definition_json)
	assert(loaded.ok)
	# Explicit synthetic level lighting; no inferred WORLD lighting chunks.
	renderer.bridge.set_level_light(Vector3(0, 0, 1), Color(0.4,0.4,0.4), Color(0.2,0.2,0.2), 128, 1.0, 0)
	renderer.bridge.add_light_node(Vector3(0,0,40), Vector3(80,40,10), Vector3(64,32,8), 160, 160, 0, 0, true)
	last_tick = 0
	last_render_frame = -1
	hit_hooks = 0
	death_hooks = 0
	spawned_particles = 0
	last_events.clear()
	refusals.clear()
	lighting_calls = {"model": 0, "world": 0}

# Only an explicit user selection reaches this loader. Failed JSON is atomic;
# valid replacement starts a fresh cosmetic pool, never alters game saves.
func load_definition_file(path: String) -> Dictionary:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null: return {"ok": false, "error": "Cannot open selected definitions JSON."}
	var candidate := file.get_as_text()
	var probe = ClassDB.instantiate("ChampionsFxBridge")
	var result: Dictionary = probe.load_definitions_json(candidate)
	if not result.ok: return result
	definition_json = candidate
	definition_source = "user-selected JSON / synthetic test sheets"
	var tick := last_tick
	reset()
	last_tick = tick
	return result

func set_gore(value: int) -> void:
	if value >= 0 and value <= 2: gore_detail = value

func combat_tick(packet: Dictionary) -> void:
	if not enabled: return
	if packet.tick != last_tick + 1:
		refusals.append("non-contiguous cosmetic ready tick")
		return
	renderer.advance_tick()
	last_tick = packet.tick
	last_events = packet.events.duplicate(true)
	for event in last_events:
		var death: bool = event.kind == "death"
		if death: death_hooks += 1
		else: hit_hooks += 1
		# Explicit red-material fixture: original spawn-0 burst. No actor-class
		# material or unsupported body-part/death recipe is guessed here.
		var result: Dictionary = renderer.bridge.spawn_event(1 if death else 0, packet.tick, 0, event.position, event.position, gore_detail)
		if result.ok: spawned_particles += result.count
		else: refusals.append("burst refused: " + str(result.refusal))
		# Type 1 is a red splat; type 0 is the full-detail red pool. Native
		# gore gates remain responsible for omitting these at lower settings.
		var decal: Dictionary = renderer.bridge.add_decal(8.0 if death else 4.0, event.position, 0, 0 if death else 1, 0, gore_detail)
		if not decal.ok: refusals.append("decal refused: " + str(decal.refusal))

func observe(observation: Dictionary) -> void:
	var next_enabled: bool = observation.get("session_mode", "") == "play"
	var next_policy: String = observation.get("combat_policy", observation.get("level_policy", "ordinary"))
	if next_policy != policy or observation.get("ready_ticks", 0) < last_tick or (enabled and not next_enabled):
		reset()
	policy = next_policy
	enabled = next_enabled
	visible = enabled
	# Publish static lighting with the terrain, before any later load refusal
	# can observe a different mesh resource merely because a frame rendered.
	if enabled: light_world()

func _process(_delta: float) -> void:
	if enabled:
		light_scene()
		present()
		if scene.fx_status != null:
			scene.fx_status.text = "FX: " + definition_source + " · synthetic lighting · glow / trails / beams disabled" + (" · " + "; ".join(refusals) if not refusals.is_empty() else "")

func present() -> void:
	if not enabled: return
	var frame := Engine.get_process_frames()
	var result: Dictionary = renderer.present(frame, scene.paused or scene.stopped or frame == last_render_frame)
	last_render_frame = frame
	if not result.ok and not result.refusal in refusals: refusals.append(result.refusal)

# Flat normals are derived for the scene's position-only triangle fixtures.
# They are a display preparation step, not reconstructed retail model normals.
func prepare_mesh(source: Mesh) -> ArrayMesh:
	var mesh := ArrayMesh.new()
	for surface in source.get_surface_count():
		if source is ArrayMesh and source.surface_get_primitive_type(surface) != Mesh.PRIMITIVE_TRIANGLES: return null
		var arrays: Array = source.surface_get_arrays(surface)
		var points: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL] if arrays[Mesh.ARRAY_NORMAL] is PackedVector3Array else PackedVector3Array()
		if normals.size() != points.size():
			normals.resize(points.size())
			var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX] if arrays[Mesh.ARRAY_INDEX] is PackedInt32Array else PackedInt32Array()
			if indices.is_empty():
				for i in points.size(): indices.append(i)
			if indices.size() % 3: return null
			for i in range(0,indices.size(),3):
				var a := indices[i]
				var b := indices[i+1]
				var c := indices[i+2]
				var normal := (points[b]-points[a]).cross(points[c]-points[a]).normalized()
				for index in [a,b,c]: normals[index] += normal
			for i in normals.size(): normals[i] = normals[i].normalized()
			arrays[Mesh.ARRAY_NORMAL] = normals
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		mesh.surface_set_material(surface, source.surface_get_material(surface))
	return mesh

func light(instance: MeshInstance3D, model: bool) -> void:
	if instance == null or instance.mesh == null or not instance.visible: return
	if instance.has_meta("play_fx_mesh") and instance.get_meta("play_fx_mesh") == instance.mesh and instance.get_meta("play_fx_transform", Transform3D()) == instance.global_transform: return
	var source: Mesh = instance.mesh
	var needs_normals: bool = not source is ArrayMesh
	for i in source.get_surface_count():
		var arrays: Array = source.surface_get_arrays(i)
		if not arrays[Mesh.ARRAY_NORMAL] is PackedVector3Array or arrays[Mesh.ARRAY_NORMAL].size() != arrays[Mesh.ARRAY_VERTEX].size(): needs_normals = true
	if needs_normals:
		var prepared := prepare_mesh(source)
		if prepared == null:
			if not "unsupported scene mesh" in refusals: refusals.append("unsupported scene mesh")
			return
		instance.mesh = prepared
	var result: Dictionary = renderer.apply_vertex_lighting(instance, model)
	if not result.ok:
		if not result.refusal in refusals: refusals.append(result.refusal)
		return
	instance.set_meta("play_fx_mesh", instance.mesh)
	instance.set_meta("play_fx_transform", instance.global_transform)
	lighting_calls["model" if model else "world"] += 1

func light_world() -> void:
	for instance in scene.terrain_root.get_children():
		if instance is MeshInstance3D: light(instance, false)

func light_scene() -> void:
	if not enabled: return
	light_world()
	light(scene.actor_visual, true)
	for instance in scene.combat_displays.values(): light(instance, true)
	if scene.components.has("animation"):
		var preview = scene.components.animation
		if preview.active: light(preview.mesh_instance, true)
