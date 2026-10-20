extends SceneTree

# Visual check with synthetic values: blend and additive sprites, a kind-2 line fading to
# alpha 0, a trail and a beam, drawn from the exact GS output; an opaque box 100 units away
# hides what lies behind it on the left (depth from each vertex's clip w).
const FIXTURE := '[{"site":1,"call":"particleDef_ctor","def":256,"complete":true,"size_start":6,"size_end":6,"gravity":0,"start_rgba":[128,32,16,128],"end_rgba":[128,32,16,128],"life_class":2,"kind":0,"tile":0,"callback":0},{"site":2,"call":"particleDef_ctor","def":257,"complete":true,"size_start":6,"size_end":6,"gravity":0,"start_rgba":[16,32,128,128],"end_rgba":[16,32,128,128],"life_class":2,"kind":1,"tile":0,"callback":0},{"site":3,"call":"particleDef_ctor","def":258,"complete":true,"size_start":30,"size_end":30,"gravity":0,"start_rgba":[255,255,64,128],"end_rgba":[255,255,64,128],"life_class":2,"kind":2,"tile":0,"callback":0}]'

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(640, 480)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	get_root().add_child(viewport)
	var scene := Node3D.new()
	viewport.add_child(scene)
	var camera := Camera3D.new()
	camera.near = 1.0
	camera.far = 2000.0
	scene.add_child(camera)
	camera.current = true
	var box := MeshInstance3D.new()
	var cube := BoxMesh.new()
	cube.size = Vector3(60, 300, 1)
	box.mesh = cube
	box.position = Vector3(-45, 0, -100)   # in front of the effects, left of centre
	var opaque := StandardMaterial3D.new()
	opaque.albedo_color = Color(0.2, 0.6, 0.2)
	opaque.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	cube.material = opaque
	scene.add_child(box)
	var renderer := NativeFxRenderer.new()
	scene.add_child(renderer)
	var sheet := Image.create_empty(512, 160, false, Image.FORMAT_RGBA8)
	sheet.fill(Color(1, 1, 1, 0.5))
	renderer.textures["particletex.tex"] = ImageTexture.create_from_image(sheet)
	var trail := Image.create_empty(128, 448, false, Image.FORMAT_RGBA8)
	trail.fill(Color(1, 1, 1, 0.5))
	renderer.textures["trail.tex"] = ImageTexture.create_from_image(trail)
	assert(renderer.bridge.load_definitions_json(FIXTURE)["ok"])
	for x in [-60, -20, 20]:
		assert(renderer.bridge.spawn_particle(256, Vector3(x, -20, 200), Vector3.ZERO, 0.0)["ok"])
		assert(renderer.bridge.spawn_particle(257, Vector3(x + 10, -15, 200), Vector3.ZERO, 0.0)["ok"])
	assert(renderer.bridge.spawn_particle(258, Vector3(-40, 30, 200), Vector3(120, 0, 0), 0.0)["ok"])
	for t in 8:
		renderer.advance_tick(t + 1)
	var made: Dictionary = renderer.bridge.create_trail(1.0, 0, 200, 4, Color(1, 0.5, 0.2, 0.5))
	assert(made["ok"])
	var handle: int = made["handle"]
	for k in 12:
		assert(renderer.bridge.add_trail_point(handle, Vector3(-60 + k * 10, 60 + (k % 2) * 6, 200))["ok"])
	assert(renderer.bridge.create_beam(3.0, 2.0, Vector3(-50, -60, 200), Vector3(50, -60, 200), 120, 4,
		Color(0.5, 0.5, 1.0, 0.5))["ok"])
	var game := preload("res://tests/native_fx_smoke.gd").game_camera()
	game["sim_frame"] = 8
	game["net_frame"] = 8
	var frame: Dictionary = renderer.present(game)
	assert(frame["ok"])
	print("native FX visual runs: ", frame["runs"], " refusals: ", frame["refusals"])
	await process_frame
	await process_frame
	var image := viewport.get_texture().get_image()
	var path := ProjectSettings.globalize_path("res://../work/native-fx-2-visual.png")
	print("native FX visual capture: ", image.save_png(path), " ", path)
	quit(0)
