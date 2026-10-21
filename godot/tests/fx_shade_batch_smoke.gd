extends SceneTree

# shade_world_many must give exactly the per-vertex shade_world colours, and
# relighting a moving model must keep reusing one unshaded material copy.
const Renderer = preload("res://fx/native_fx_renderer.gd")
var checks := 0
var failures := 0

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var renderer = Renderer.new()
	renderer.native_z_up = true
	root.add_child(renderer)
	check(renderer.bridge.set_level_light(Vector3(0.3, -0.5, 0.8), Color(0.5, 0.45, 0.4), Color(0.2, 0.25, 0.3), 128, 1.0, 0)["ok"], "level light")
	check(renderer.bridge.add_light_node(Vector3(10, -20, 40), Vector3(255, 64, 0), Vector3(128, 32, 0), 120.0, 90.0, 0.0, 0, true)["ok"], "light node")
	var random := RandomNumberGenerator.new()
	random.seed = 7
	var points := PackedVector3Array()
	var normals := PackedVector3Array()
	for i in 2000:
		points.push_back(Vector3(random.randf_range(-300, 300), random.randf_range(-300, 300), random.randf_range(-50, 150)))
		normals.push_back(Vector3(random.randf_range(-1, 1), random.randf_range(-1, 1), random.randf_range(-1, 1)).normalized())
	var many: Dictionary = renderer.bridge.shade_world_many(points, normals)
	check(many.get("ok", false) and many.rgb.size() == points.size(), "batch shades every vertex")
	var same := true
	for i in points.size():
		var one: Dictionary = renderer.bridge.shade_world(points[i], normals[i])
		same = same and one.ok and one.rgb == many.rgb[i]
	check(same, "batch equals per-vertex shade_world exactly")
	var bad := points.duplicate()
	bad[5] = Vector3(INF, 0, 0)
	check(renderer.bridge.shade_world_many(bad, normals).get("refusal", "") == renderer.bridge.shade_world(bad[5], normals[5]).get("refusal", "-"), "same refusal as one vertex")
	check(not renderer.bridge.shade_world_many(points, PackedVector3Array()).get("ok", true), "mismatched arrays refuse")
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0)])
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([Vector3.UP, Vector3.UP, Vector3.UP])
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var model := MeshInstance3D.new()
	model.mesh = mesh
	model.material_override = StandardMaterial3D.new()
	renderer.add_child(model)
	check(renderer.apply_vertex_lighting(model, true).ok, "model lit")
	var first: Material = model.material_override
	var first_surface: Material = model.mesh.surface_get_material(0)
	model.position = Vector3(5, 0, 0)
	check(renderer.apply_vertex_lighting(model, true).ok, "moved model relit")
	check(model.material_override == first and model.mesh.surface_get_material(0) == first_surface, "relight reuses the unshaded copies")
	check(first is StandardMaterial3D and first.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED and first.vertex_color_use_as_albedo, "copy is the unshaded vertex-colour material")
	var world := MeshInstance3D.new()
	world.mesh = mesh.duplicate()
	world.material_override = StandardMaterial3D.new()
	renderer.add_child(world)
	check(renderer.apply_vertex_lighting(world, false).ok, "world chunk lit through the batch")
	var lit: PackedColorArray = world.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
	var expected: Dictionary = renderer.bridge.shade_world(renderer.to_native(Vector3(1, 0, 0)), renderer.to_native(Vector3.UP))
	check(lit[1] == Color(expected.rgb.x / 255.0, expected.rgb.y / 255.0, expected.rgb.z / 255.0, 1.0), "world colours equal the per-vertex result")
	renderer.queue_free()
	print("fx shade batch: ", checks, " checks, ", failures, " failures")
	quit(1 if failures else 0)
