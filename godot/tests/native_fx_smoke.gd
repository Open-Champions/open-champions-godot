extends SceneTree

# Synthetic values only: a made-up definition and textures, a hand-made camera.
const SYNTHETIC := '[{"site":1,"call":"particleDef_ctor","def":256,"complete":true,"size_start":2,"size_end":3,"gravity":0,"start_rgba":[128,64,32,128],"end_rgba":[0,64,255,0],"life_class":1,"kind":1,"tile":18,"callback":0},{"site":2,"call":"particleDef_ctor","def":512,"complete":true,"size_start":4,"size_end":4,"gravity":0,"start_rgba":[200,200,255,128],"end_rgba":[200,200,255,128],"life_class":2,"kind":2,"tile":0,"callback":0}]'

static func game_camera() -> Dictionary:
	# Looking along +z from the origin (the game's view mat34 rows, then its translation).
	var view := PackedFloat32Array([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0])
	var viewproj := PackedFloat32Array([400, 0, 0, 0, 0, 400, 0, 0, 0, 0, 1, 1, 0, 0, -18, 0])
	return {"view": view, "proj_scale": 477.5, "camera": view, "viewproj": viewproj,
		"sim_frame": 1, "net_frame": 1}

static func white(width: int, height: int) -> Texture2D:
	var image := Image.create_empty(width, height, false, Image.FORMAT_RGBA8)
	image.fill(Color.WHITE)
	return ImageTexture.create_from_image(image)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var renderer := NativeFxRenderer.new()
	get_root().add_child(renderer)
	renderer.textures["particletex.tex"] = white(512, 160)
	renderer.textures["trail.tex"] = white(128, 448)
	renderer.textures["floorsprite.tex"] = white(256, 128)
	var loaded: Dictionary = renderer.bridge.load_definitions_json(SYNTHETIC)
	assert(loaded["ok"])
	assert(renderer.bridge.spawn_particle(256, Vector3(0, 0, 200), Vector3.ZERO, 0.0)["ok"])
	assert(not renderer.bridge.spawn_particle(999, Vector3(0, 0, 200), Vector3.ZERO, 0.0)["ok"])
	assert(renderer.bridge.spawn_particle(512, Vector3(0, 0, 200), Vector3(30, 0, 0), 0.0)["ok"])
	renderer.advance_tick(1)
	assert(renderer.bridge.counts()["particles"] == 2)
	# fx_spawn id 5 is gameplay (shared seed): the presentation bridge refuses it.
	assert(not renderer.bridge.fx_spawn(5, Vector3.ZERO, true)["ok"])
	# id 0x11 spawns definitions this synthetic JSON does not have: the call is
	# made and its missing definitions are a refusal, not silence (review finding 5).
	var burst: Dictionary = renderer.bridge.fx_spawn(0x11, Vector3(0, 0, 200), true)
	assert(not burst["ok"] and burst["calls"] > 0)
	assert(str(burst["refusal"]).begins_with("particle definitions missing from the user's JSON: 0x"))
	var made: Dictionary = renderer.bridge.create_trail(1.0, 0, 64, 0, Color.WHITE)
	assert(made["ok"] and made["handle"] != 0)
	var trail: int = made["handle"]
	assert(renderer.bridge.add_trail_point(trail, Vector3(0, 0, 200))["ok"])
	assert(renderer.bridge.add_trail_point(trail, Vector3(10, 0, 200))["ok"])
	made = renderer.bridge.create_beam(2.0, 1.0, Vector3(0, 0, 200), Vector3(20, 0, 200), 64, 8,
		Color(0.5, 0.5, 0.5, 0.37))
	assert(made["ok"] and made["handle"] != 0)
	var beam: int = made["handle"]
	assert(renderer.bridge.update_beam(beam, Vector3(0, 0, 200), Vector3(0, 20, 200), 64, -1)["ok"])
	assert(renderer.bridge.set_level_light(Vector3.UP, Color(0.4, 0.4, 0.4), Color(0.2, 0.2, 0.2), 128, 1.0, 0)["ok"])
	assert(renderer.bridge.add_decal(12.0, Vector3(0, 0, 197), 0, 9, 0)["ok"])
	assert(renderer.bridge.counts()["decals"] == 1)
	var frame: Dictionary = renderer.present(game_camera())
	# The one particle of definition 999 (not in the JSON) is a refusal of the
	# frame: ok is false, the drawable runs are still drawn (review finding 6).
	assert(not frame["ok"])
	assert(frame["refusals"].size() == 1 and str(frame["refusals"][0]).contains("0x3e7 x1"))
	assert(frame["runs"] >= 4)
	var textures := {}
	for node in renderer._runs:
		if node.visible:
			textures[node.mesh.surface_get_material(0).get_shader_parameter("textured")] = true
	assert(textures.has(true) and textures.has(false))   # sprites and the kind-2 line
	assert(renderer.bridge.add_light_node(Vector3(0, 0, 200), Vector3(255, 0, 0), Vector3(255, 0, 0),
		100.0, 100.0, 0.0, 0, true)["ok"])
	assert(renderer.bridge.counts()["lights"] == 1)
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([
		Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0)])
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([
		Vector3.UP, Vector3.UP, Vector3.UP])
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	mesh.surface_set_material(0, StandardMaterial3D.new())
	var model := MeshInstance3D.new()
	model.mesh = mesh
	renderer.add_child(model)
	assert(renderer.apply_vertex_lighting(model, true)["ok"])
	assert(model.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR].size() == 3)
	print("native FX Godot smoke passed")
	quit(0)
