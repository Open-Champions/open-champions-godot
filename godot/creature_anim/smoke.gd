extends SceneTree

# Synthetic adapter test only; no original assets or scene-preview dependencies.
var checks := 0
var api: RefCounted
func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		push_error(message)
		quit(1)
		assert(value, message)
func zero_service(_args: Array) -> int:
	return 0
func _initialize() -> void:
	call_deferred("run")
func run() -> void:
	check(ClassDB.class_exists("ChampionsCreatureAnimation"), "new binding registered")
	api = ClassDB.instantiate("ChampionsCreatureAnimation")
	check(not api.configure({"profile":"norrath", "class":"SmallSpider"}).ok, "Norrath refuses")
	var fixture = load("res://tests/skinning_fixture.gd")
	var memory: Array = [[0x1024,4,0x502818], [0x502824,4,0x9000], [0x50282c,4,0xa000],
		[0x502854,4,0x6000], [0x502864,4,0x6100], [0x1048,4,0x50fd40],
		[0x50fd40+27*8+4,4,0x2ab704], [0x50fd40+28*8+4,4,0x10beb8],
		[0x10dc,4,0x3f800000], [0x13c4,2,600], [0x403a08,1,1], [0x1228,4,0x42c80000]]
	var clips := {}
	for i in range(6):
		clips[0x6000+0x100*i] = fixture.animation(64+64*i)
	for i in range(4):
		memory.append([0x502834+8*i,4,0x6200+0x100*i])
	var config := {"profile":"rta-us-v2.00", "class":"SmallSpider", "address":0x1000,
		"definition":0x502818, "model_resource":0x9000, "texture_resource":0xa000,
		"base_resource":0x6000, "clips":clips, "memory":memory, "model_bytes":fixture.model(), "units":1.0}
	var result: Dictionary = api.configure(config)
	check(result.ok, "configure: "+str(result))
	check(not api.change_state(0).ok, "missing engine service refuses")
	for name in ["handle_clear", "handle_link", "handle_removeNode", "Creature.v17", "Creature_applyKnockback"]:
		check(api.set_service(name, Callable(self,"zero_service")).ok, "service installed")
	check(not api.set_service("anim_advance", Callable(self,"zero_service")).ok, "native animation callbacks protected")
	check(api.change_state(0).ok, "original idle dispatch")
	result = api.tick(1,10)
	check(result.ok, "tick: "+str(result))
	check(result.positions.size()==3 and result.normals.size()==3, "three owned skinned corners")
	check(result.joint_count==4 and result.pose_rows.size()==48, "four quantized poses")
	check(result.model_resource==0x9000 and result.texture_resource==0xa000, "class pairing retained")
	var positions: PackedVector3Array = result.positions
	check(not api.tick(1,11).ok, "duplicate tick refuses")
	var drawn: Dictionary = api.draw(11)
	check(drawn.ok and drawn.positions==positions, "draw does not advance simulation")
	result = api.tick(2,12)
	check(result.ok and result.positions!=positions, "original velocity changes skinning next tick")
	check(api.write_memory(0x403a08,1,0).ok, "disable activity centre")
	var inactive: Dictionary = api.tick(3,13)
	check(inactive.ok and inactive.positions==result.positions, "inactive original run keeps pose")
	# Two actors share mutable class event storage and resource identities.
	check(api.write_memory(0x6018,1,4).ok, "mutate shared event mask")
	var second: RefCounted = ClassDB.instantiate("ChampionsCreatureAnimation")
	var shared_config: Dictionary = config.duplicate(true)
	shared_config["shared_with"] = api
	shared_config["address"] = 0x2000
	var shared_memory: Array = []
	for write in memory:
		var copy: Array = write.duplicate()
		if copy[0]>=0x1000 and copy[0]<0x1400:
			copy[0] += 0x1000
		shared_memory.append(copy)
	shared_config["memory"] = shared_memory
	check(second.configure(shared_config).ok, "second actor shares environment")
	check(second.read_memory(0x6018,1).value==4, "second configuration keeps shared event mutation")
	for name in ["handle_clear", "handle_link", "handle_removeNode", "Creature.v17", "Creature_applyKnockback"]:
		check(second.set_service(name, Callable(self,"zero_service")).ok, "second actor service")
	check(second.change_state(0).ok, "second actor idle dispatch")
	var second_tick: Dictionary = second.tick(4,14)
	check(second_tick.ok and second_tick.events==4, "shared event consumed by second actor")
	# Build an actual mesh from the returned values in a separate synthetic context.
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = result.positions
	arrays[Mesh.ARRAY_NORMAL] = result.normals
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES,arrays)
	check(mesh.get_surface_count()==1, "Godot accepts skinned pose as a mesh")
	check(api.clear_history().ok and second.clear_history().ok, "explicit history consumption")
	if "--render" in OS.get_cmdline_user_args():
		await render_pose(mesh, result.object_transform)
	print("CREATURE_ANIMATION_SMOKE: ",checks," checks pass")
	quit(0)

func render_pose(mesh: ArrayMesh, object_transform: Transform3D) -> void:
	root.size = Vector2i(640,480)
	var scene := Node3D.new()
	root.add_child(scene)
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.transform = object_transform
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.albedo_color = Color(0.15,0.75,0.95)
	instance.material_override = material
	scene.add_child(instance)
	var bounds := mesh.get_aabb()
	var center := object_transform * bounds.get_center()
	var camera := Camera3D.new()
	scene.add_child(camera)
	camera.position = center + Vector3(1.0,0.7,1.0).normalized() * maxf(bounds.size.length()*2.0,4.0)
	camera.look_at(center,Vector3.UP)
	camera.current = true
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color(0.03,0.045,0.07)
	scene.add_child(environment)
	for _i in range(12):
		await process_frame
	await RenderingServer.frame_post_draw
	var capture := root.get_texture().get_image()
	check(capture.save_png("res://render-smoke.png")==OK,"render capture saved")
