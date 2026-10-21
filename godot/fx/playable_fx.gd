extends Node3D

# Play-mode presentation of native-fx-2's exact GS-order output (NativeFxRenderer
# draws the bridge's runs). It owns no game state and never touches shared RNG.
# Full combat fixture: the session's native EffectSimulation runs inside every
# ready tick (also headless) and fills the attached bridge world; this script
# only attaches the bridge, runs the per-frame draw slots and presents.
# Combat / animated fixtures keep the authored blood policy (docs/PLAYABLE_FX.md)
# through the exact fx_spawn 0 recipe and decal_add on the same bridge.
const Renderer = preload("res://fx/native_fx_renderer.gd")
const Fixture = preload("res://fx/playable_fx_fixture.gd")
const LevelTerrain = preload("res://scenes/level_texture_terrain.gd")
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
var full := false
var last_render_frame := -1
var last_runs := 0
var policy := ""
var level_light_active := false
var level_light_path := ""
var level_light_name := ""
var level_light_sub := ""
var level_light_record: Dictionary = {}

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
	add_child(renderer)
	Fixture.configure(renderer)
	var loaded: Dictionary = renderer.bridge.load_definitions_json(definition_json)
	assert(loaded.ok)
	renderer.bridge.set_detail(gore_detail)
	# Explicit synthetic level lighting; no inferred WORLD lighting chunks.
	# A supplied per-level record replaces the sun, ambient and base below.
	# The synthetic point node stays in both states.
	_synthetic_sun()
	renderer.bridge.add_light_node(Vector3(0,0,40), Vector3(80,40,10), Vector3(64,32,8), 160, 160, 0, 0, true)
	# The native session draws into this machine's world (Full fixture only).
	if scene != null and scene.controller != null and scene.controller.has_method("attach_effect_bridge"):
		scene.controller.attach_effect_bridge(renderer.bridge)
	last_tick = 0
	last_render_frame = -1
	last_runs = 0
	hit_hooks = 0
	death_hooks = 0
	spawned_particles = 0
	last_events.clear()
	refusals.clear()
	lighting_calls = {"model": 0, "world": 0}
	_apply_level_light_record()

func _synthetic_sun() -> void:
	renderer.bridge.set_level_light(Vector3(0, 0, 1), Color(0.4,0.4,0.4), Color(0.2,0.2,0.2), 128, 1.0, 0)

# Choose the record for a newly loaded level now (reset() runs when play ends,
# so a later level must not inherit the record resolved before it loaded).
func select_level_light(path: String, level_name: String, sub := "") -> void:
	level_light_path = path
	level_light_name = level_name
	level_light_sub = sub
	_synthetic_sun()
	_apply_level_light_record()

# game_loadLevel selects the row by the loaded level's name and
# g_levelSubName (lighting.md 10.1 at research-fx 8e66747). Only the caller
# that loaded the level supplies them (select_level_light); the command line
# may name the JSON file but not the row.
func _level_light_request() -> Dictionary:
	var path := level_light_path
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--level-lights="): path = arg.substr(15)
	return {"path": path, "name": level_light_name, "sub": level_light_sub}

# The user's extractor JSON replaces the labelled synthetic sun. A missing path
# leaves that sun in place. A file that does not load leaves it in place too.
func _apply_level_light_record() -> void:
	level_light_active = false
	level_light_record = {}
	# The refusal describes the current request only. reset() runs before the
	# loader supplies a name; a later select_level_light must not keep it.
	refusals.erase("level light refused: loaded level name unknown")
	var request := _level_light_request()
	if str(request.path).is_empty(): return
	# The null-name row is what a miss selects, not a stand-in for a level
	# whose name is unknown: without the loaded level's name, refuse.
	if str(request.name).is_empty():
		if not "level light refused: loaded level name unknown" in refusals:
			refusals.append("level light refused: loaded level name unknown")
		return
	var file := FileAccess.open(str(request.path), FileAccess.READ)
	if file == null:
		refusals.append("level light JSON refused")
		return
	if not ClassDB.class_exists("ChampionsLevelLights"):
		refusals.append("level light JSON refused")
		return
	var service = ClassDB.instantiate("ChampionsLevelLights")
	var loaded: Dictionary = service.load_json(file.get_as_text())
	if not loaded.get("ok", false):
		refusals.append("level light JSON refused")
		return
	var record: Dictionary = service.resolve(str(request.name), str(request.sub))
	if not record.get("ok", false):
		refusals.append("level light JSON refused")
		return
	var sun: PackedInt32Array = record.sun
	var ambient: PackedInt32Array = record.ambient
	var applied: Dictionary = renderer.bridge.set_level_light(record.direction, Color8(sun[0], sun[1], sun[2], sun[3]), Color8(ambient[0], ambient[1], ambient[2], ambient[3]), int(record.base), 1.0, 0)
	if not applied.get("ok", false):
		refusals.append("level light JSON refused")
		return
	level_light_active = true
	level_light_record = record
	level_light_path = str(request.path)
	level_light_name = str(request.name)
	level_light_sub = str(request.sub)

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
	if value >= 0 and value <= 2:
		gore_detail = value
		if renderer != null: renderer.bridge.set_detail(value)

func combat_tick(packet: Dictionary) -> void:
	if not enabled: return
	if packet.tick != last_tick + 1:
		refusals.append("non-contiguous cosmetic ready tick")
		return
	last_tick = packet.tick
	last_events = packet.events.duplicate(true)
	for event in last_events:
		if event.kind == "death": death_hooks += 1
		else: hit_hooks += 1
	# Full: the native EffectSimulation already ran this tick's effects and the
	# bridge world's priority-15 update inside the ready tick.
	if full: return
	var ticked: Dictionary = renderer.advance_tick(packet.tick)
	if not ticked.ok:
		if not str(ticked.refusal) in refusals: refusals.append(str(ticked.refusal))
		return
	var before: int = renderer.bridge.counts().particles
	for event in last_events:
		var death: bool = event.kind == "death"
		# Explicit red-material fixture: the exact spawn-0 recipe (it returns at
		# gore 0 natively). No actor-class material or death recipe is guessed.
		var result: Dictionary = renderer.bridge.fx_spawn(0, event.position, true)
		if not result.ok: refusals.append("burst refused: " + str(result.refusal))
		# Type 1 is a red splat; type 0 the full-detail red pool. The native
		# decal gate omits them at lower settings.
		var decal: Dictionary = renderer.bridge.add_decal(8.0 if death else 4.0, event.position, 0, 0 if death else 1, 0)
		if not decal.ok: refusals.append("decal refused: " + str(decal.refusal))
	spawned_particles += renderer.bridge.counts().particles - before

func observe(observation: Dictionary) -> void:
	var next_enabled: bool = observation.get("session_mode", "") == "play"
	var next_policy: String = observation.get("combat_policy", observation.get("level_policy", "ordinary"))
	if next_policy != policy or observation.get("ready_ticks", 0) < last_tick or (enabled and not next_enabled):
		reset()
	policy = next_policy
	enabled = next_enabled
	full = observation.get("full_enabled", false)
	visible = enabled
	# Full: ready ticks are observed through the native effect counts.
	if full: last_tick = int(observation.get("ready_ticks", last_tick))
	# Publish static lighting with the terrain, before any later load refusal
	# can observe a different mesh resource merely because a frame rendered.
	if enabled: light_world()

func status_text() -> String:
	var lighting := "level light record" if level_light_active else "synthetic lighting"
	var text := "FX: " + definition_source + " · native-fx-2 GS-order output · " + lighting
	if full: text += " · Full combat effects simulated every tick"
	if not refusals.is_empty(): text += " · " + "; ".join(refusals)
	return text

func _process(_delta: float) -> void:
	if enabled:
		light_scene()
		present()
		if scene.fx_status != null:
			scene.fx_status.text = status_text()

# The game camera from the scene camera (client derivation; docs/NATIVE_FX.md
# leaves it to the caller): camera x right, y down, z forward in native XY / Z-up,
# row-vector view mat34, proj_scale for the 480-line half height at 4:3.
func game_camera(camera: Camera3D) -> Dictionary:
	var t := camera.global_transform
	var right: Vector3 = renderer.to_native(t.basis.x)
	var down: Vector3 = renderer.to_native(-t.basis.y)
	var forward: Vector3 = renderer.to_native(-t.basis.z)
	var origin: Vector3 = renderer.to_native(t.origin)
	var view := PackedFloat32Array([right.x, down.x, forward.x, right.y, down.y, forward.y, right.z, down.z, forward.z,
		-origin.dot(right), -origin.dot(down), -origin.dot(forward)])
	return renderer.bridge.game_camera(view, 480.0 / tan(deg_to_rad(camera.fov) * 0.5))

func present() -> void:
	if not enabled or scene.camera == null: return
	var frame := Engine.get_process_frames()
	var frozen: bool = scene.paused or scene.stopped or frame == last_render_frame
	var camera: Dictionary = game_camera(scene.camera)
	if not camera.get("ok", false):
		if not "game camera refused" in refusals: refusals.append("game camera refused")
		return
	# Draw slots queue glows that only the following draw consumes: queue them
	# only for a frame that is drawn (review finding 12).
	if full and not frozen and scene.controller != null:
		scene.controller.draw_effect_slots()
	if full:
		var sim: int = int(scene.raw_observation.get("combat_sim_frame", 0))
		camera["sim_frame"] = sim
		camera["net_frame"] = sim
	var result: Dictionary = renderer.present(camera, frozen)
	last_render_frame = frame
	if not result.has("runs"):
		var why := str(result.get("refusal", "present refused"))
		if not why in refusals: refusals.append(why)
		return
	last_runs = int(result.runs)
	for why in result.get("refusals", []):
		if not str(why) in refusals: refusals.append(str(why))

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
				normals[a] += normal
				normals[b] += normal
				normals[c] += normal
			for i in normals.size(): normals[i] = normals[i].normalized()
			arrays[Mesh.ARRAY_NORMAL] = normals
		var flags: int = source.surface_get_format(surface) if source is ArrayMesh else 0
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, flags)
		mesh.surface_set_material(surface, source.surface_get_material(surface))
	return mesh

func light(instance: MeshInstance3D, model: bool, prepare_hidden := false) -> void:
	if instance == null or instance.mesh == null or (not instance.visible and not prepare_hidden): return
	# Constant-light chunks take world_constant_channel on the loaded level's
	# base level; shade_world stays on the dynamic chunks only.
	if not model and not instance.get_meta("native_fx_world_dynamic", true):
		light_constant(instance)
		return
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

# The base level is the loaded level's record (its flag word); without one the
# constant colour is refused and the chunk keeps its base texture only.
func light_constant(instance: MeshInstance3D) -> void:
	if not instance.has_meta("level_constant_light"): return
	var base := int(level_light_record.get("base", -1)) if level_light_active else -1
	if int(instance.get_meta("level_constant_base", -1)) == base: return
	var loader: Object = ClassDB.instantiate("ChampionsLevelTexturePreview") if ClassDB.class_exists("ChampionsLevelTexturePreview") else null
	LevelTerrain.set_constant_light(instance, base, loader)
	if loader != null: loader.free()

# Set while a caller swaps the terrain display in several steps (the game-folder
# level load): an intermediate display is never lit, the final one is lit on
# the next frame.
var world_lighting_held := false

func light_world() -> void:
	if world_lighting_held: return
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
