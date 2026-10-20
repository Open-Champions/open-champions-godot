extends SceneTree

# Before/after captures for a gitignored manifest of generic WORLD/TEX paths.
# The manifest's lookup name is not printed.
const Terrain = preload("res://scenes/level_texture_terrain.gd")
const Playable = preload("res://fx/playable_fx.gd")

func _initialize() -> void:
	call_deferred("_run")

func _arg(key: String) -> String:
	var args := OS.get_cmdline_user_args()
	for i in args.size() - 1:
		if args[i] == key:
			return args[i + 1]
	return ""

func _run() -> void:
	var manifest_path := _arg("--manifest")
	var lights_path := _arg("--level-lights")
	var out_dir := _arg("--out")
	if manifest_path.is_empty() or lights_path.is_empty() or out_dir.is_empty():
		push_error("capture needs --manifest, --level-lights and --out")
		quit(1)
		return
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(manifest_path))
	if parsed == null or not parsed is Dictionary:
		push_error("capture manifest refused")
		quit(1)
		return
	DisplayServer.window_set_size(Vector2i(1280, 720))
	var saved := 0
	for level in parsed.levels:
		var loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
		var decoded: Dictionary = loader.load_world_tex(str(level.world), str(level.tex))
		loader.free()
		if not decoded.get("ok", false):
			print("level %s refused: %s" % [str(level.id), str(decoded.get("error", "refused"))])
			continue
		var terrain := Terrain.build(decoded, true)
		if terrain == null:
			push_error("level %s has no terrain" % str(level.id))
			quit(1)
			return
		var holder := Node3D.new()
		root.add_child(holder)
		holder.add_child(terrain)
		var bounds := AABB()
		var first := true
		for instance in terrain.get_children():
			if instance is MeshInstance3D and instance.mesh != null:
				var box: AABB = instance.global_transform * instance.mesh.get_aabb()
				if first:
					bounds = box
					first = false
				else:
					bounds = bounds.merge(box)
		var center := bounds.get_center()
		var radius := maxf(bounds.size.length(), 8.0)
		var camera := Camera3D.new()
		camera.current = true
		camera.near = 0.05
		camera.far = maxf(radius * 8.0, 4000.0)
		holder.add_child(camera)
		camera.position = center + Vector3(radius * 0.35, radius * 0.55, radius * 0.35)
		camera.look_at(center, Vector3.UP)
		var probe := _probe(maxf(radius * 0.08, 4.0))
		probe.position = camera.position.lerp(center, 0.55)
		holder.add_child(probe)
		print("capture %s surfaces %d radius %.0f" % [str(level.id), terrain.get_child_count(), radius])
		var fx = Playable.new()
		holder.add_child(fx)
		fx.configure(null)
		_light(fx, terrain, probe)
		await _shot(out_dir.path_join("level-%s-before.png" % str(level.id)))
		fx.level_light_path = lights_path
		fx.level_light_name = str(level.name)
		fx.reset()
		if not fx.level_light_active:
			push_error("level %s light record was not applied" % str(level.id))
			quit(1)
			return
		_light(fx, terrain, probe)
		await _shot(out_dir.path_join("level-%s-after.png" % str(level.id)))
		saved += 1
		holder.queue_free()
		await process_frame
	print("level light captures %d" % saved)
	quit(0 if saved >= 3 else 1)

func _probe(size: float) -> MeshInstance3D:
	var box := BoxMesh.new()
	box.size = Vector3(size, size, size)
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, box.surface_get_arrays(0))
	var material := StandardMaterial3D.new()
	material.albedo_color = Color.WHITE
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	mesh.surface_set_material(0, material)
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	return instance

func _light(fx, terrain: Node3D, probe: MeshInstance3D) -> void:
	for instance in terrain.get_children():
		if instance is MeshInstance3D:
			fx.light(instance, false)
	fx.light(probe, true)

func _shot(path: String) -> void:
	for i in 6:
		await process_frame
	var image := root.get_texture().get_image()
	if image == null or image.save_png(path) != OK:
		push_error("screenshot failed")
		quit(1)
