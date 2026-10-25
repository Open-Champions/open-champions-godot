extends SceneTree

# The play scene's game camera (playable_fx.gd game_camera) must place native-fx-2
# GS output where Godot's camera draws the same native point. One synthetic
# particle per probe point; its sprite centre is compared with unproject_position.
const Playable = preload("res://fx/playable_fx.gd")
const Fixture = preload("res://fx/playable_fx_fixture.gd")
var failures := 0

func check(ok: bool, label: String) -> void:
	if not ok:
		failures += 1
		push_error("FX camera: " + label)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(960, 720)
	get_root().add_child(viewport)
	var camera := Camera3D.new()
	camera.fov = 48
	camera.far = 2000
	viewport.add_child(camera)
	camera.position = Vector3(56, 70, 80)
	camera.look_at(Vector3(0, 3, 0), Vector3.UP)
	var fx = Playable.new()
	fx.definition_json = Fixture.definitions()
	viewport.add_child(fx)
	fx.reset()
	var worst := 0.0
	for native in [Vector3(80, 0, 0), Vector3(20, 0, 0), Vector3(0, 40, 0), Vector3(-40, 0, 10), Vector3(30, -30, 5)]:
		fx.reset()
		# Never inside assert(): release exports strip the call.
		var spawned: Dictionary = fx.renderer.bridge.spawn_particle(0x8704a8, native, Vector3.ZERO, 0.0)
		check(spawned.ok, "probe particle spawned")
		var camera_dict: Dictionary = fx.game_camera(camera)
		var frame: Dictionary = fx.renderer.bridge.draw(camera_dict, true)
		var expected := camera.unproject_position(fx.renderer.to_render(native))
		# The GS frame is 448 lines shown over the full height: a point outside
		# it is clipped by the original's own test, not drawn.
		if frame.ok and frame.runs.is_empty():
			print("FX_CAMERA native ", native, " clipped; godot ", expected, " counts ", fx.renderer.bridge.counts())
			check(expected.y > viewport.size.y or expected.y < 0 or expected.x < 0 or expected.x > viewport.size.x, "only off-frame points are clipped")
			continue
		check(frame.ok and frame.runs.size() == 1, "one particle run")
		if not frame.ok: continue
		var centre := Vector2.ZERO
		for v in frame.runs[0].vertices: centre += Vector2(v.x, v.y)
		centre /= frame.runs[0].vertices.size()
		# NativeFxRenderer: x scaled by (4/3) / aspect; GS ndc +y up.
		var size := Vector2(viewport.size)
		var gs := Vector2((centre.x * (4.0 / 3.0) / (size.x / size.y) + 1.0) * 0.5 * size.x, (1.0 - centre.y) * 0.5 * size.y)
		var godot := camera.unproject_position(fx.renderer.to_render(native))
		worst = maxf(worst, gs.distance_to(godot))
		print("FX_CAMERA native ", native, " gs ", gs, " godot ", godot)
	print("FX_CAMERA worst pixel distance ", worst)
	check(worst < 2.0, "GS output lands on the Godot projection: %f px" % worst)
	if DisplayServer.get_name() != "headless": await rendered(viewport, camera)
	quit(0 if failures == 0 else 1)

# Graphical runs: the drawn sprite (renderer shader, backend clip-space Y) lands
# where Godot draws the same native point.
func rendered(viewport: SubViewport, camera: Camera3D) -> void:
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	camera.current = true
	var fx = Playable.new()
	fx.definition_json = '[{"site":1,"call":"particleDef_ctor","def":9126056,"complete":true,"size_start":12,"size_end":12,"gravity":0,"start_rgba":[255,255,255,128],"end_rgba":[255,255,255,128],"life_class":2,"kind":0,"tile":0,"callback":0}]'
	viewport.add_child(fx)
	fx.reset()
	var native := Vector3(30, -30, 5)
	check(fx.renderer.bridge.spawn_particle(0x8b40a8, native, Vector3.ZERO, 0.0).ok, "rendered probe particle")
	check(fx.renderer.present(fx.game_camera(camera), true).ok, "rendered probe present")
	for i in 6: await process_frame
	await RenderingServer.frame_post_draw
	var image := viewport.get_texture().get_image()
	var sum := Vector2.ZERO
	var n := 0
	for y in image.get_height():
		for x in image.get_width():
			if image.get_pixel(x, y).r > 0.5:
				sum += Vector2(x, y)
				n += 1
	var expected := camera.unproject_position(fx.renderer.to_render(native))
	var centroid := sum / maxi(n, 1)
	print("FX_CAMERA rendered centroid ", centroid, " expected ", expected, " pixels ", n, " method ", ProjectSettings.get_setting("rendering/renderer/rendering_method"))
	check(n > 0 and centroid.distance_to(expected) < 3.0, "rendered sprite lands on the Godot projection")
