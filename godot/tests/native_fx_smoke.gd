extends SceneTree

const SYNTHETIC := '[{"site":1,"call":"particleDef_ctor","def":256,"complete":true,"size_start":1,"size_end":3,"gravity":0,"start_rgba":[128,64,32,128],"end_rgba":[0,64,255,0],"life_class":1,"kind":1,"tile":18,"callback":0}]'

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var renderer := NativeFxRenderer.new()
    var image := Image.create_empty(512, 256, false, Image.FORMAT_RGBA8)
    image.fill(Color.WHITE)
    renderer.particle_sheet = ImageTexture.create_from_image(image)
    renderer.particle_sheet_height = 256
    var floor_image := Image.create_empty(256, 128, false, Image.FORMAT_RGBA8)
    floor_image.fill(Color.WHITE)
    renderer.floor_sprite_sheet = ImageTexture.create_from_image(floor_image)
    get_root().add_child(renderer)
    var loaded: Dictionary = renderer.bridge.load_definitions_json(SYNTHETIC)
    assert(loaded["ok"])
    var spawned: Dictionary = renderer.bridge.spawn_particle(256, Vector3(0, 0, -30), Vector3.ZERO, 0.0)
    assert(spawned["ok"])
    assert(renderer.bridge.particle_snapshot().size() == 1)
    renderer.advance_tick()
    assert(renderer.bridge.particle_snapshot()[0]["age"] == 1)
    assert(renderer.present(1)["ok"])
    assert(renderer._add.multimesh.instance_count == 1)
    renderer.bridge.set_level_light(Vector3.UP, Color(0.4, 0.4, 0.4),
        Color(0.2, 0.2, 0.2), 128, 1.0, 0)
    var decal: Dictionary = renderer.bridge.add_decal(12.0, Vector3.ZERO, 0, 9, 0, 2)
    assert(decal["ok"])
    assert(renderer.present(2)["ok"])
    assert(renderer._decals.multimesh.instance_count == 1)
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
    var world := MeshInstance3D.new()
    world.mesh = mesh
    renderer.add_child(world)
    assert(renderer.apply_vertex_lighting(world, false)["ok"])
    assert(world.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR].size() == 3)
    print("native FX Godot smoke passed")
    quit(0)
