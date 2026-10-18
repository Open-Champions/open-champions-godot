extends SceneTree

const FIXTURE := '[{"site":1,"call":"particleDef_ctor","def":256,"complete":true,"size_start":20,"size_end":20,"gravity":0,"start_rgba":[128,32,16,128],"end_rgba":[128,32,16,128],"life_class":1,"kind":0,"tile":18,"callback":0},{"site":2,"call":"particleDef_ctor","def":257,"complete":true,"size_start":20,"size_end":20,"gravity":0,"start_rgba":[16,32,128,128],"end_rgba":[16,32,128,128],"life_class":1,"kind":1,"tile":18,"callback":0}]'

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var viewport := SubViewport.new()
    viewport.size = Vector2i(640, 360)
    viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
    get_root().add_child(viewport)
    var scene := Node3D.new()
    viewport.add_child(scene)
    var camera := Camera3D.new()
    camera.position = Vector3(0, 0, 100)
    scene.add_child(camera)
    camera.look_at(Vector3.ZERO)
    camera.current = true
    var renderer := NativeFxRenderer.new()
    var sheet := Image.create_empty(512, 256, false, Image.FORMAT_RGBA8)
    sheet.fill(Color.WHITE)
    renderer.particle_sheet = ImageTexture.create_from_image(sheet)
    renderer.particle_sheet_height = 256
    scene.add_child(renderer)
    assert(renderer.bridge.load_definitions_json(FIXTURE)["ok"])
    assert(renderer.bridge.spawn_particle(256, Vector3.ZERO, Vector3.ZERO, 0.0)["ok"])
    assert(renderer.bridge.spawn_particle(257, Vector3(10, 0, 5), Vector3.ZERO, 0.0)["ok"])
    assert(renderer.present(0)["ok"])
    await process_frame
    await process_frame
    var image := viewport.get_texture().get_image()
    var path := ProjectSettings.globalize_path("res://../work/native-fx-visual.png")
    var saved := image.save_png(path)
    print("native FX visual capture: ", saved, " center=", image.get_pixel(320, 180))
    for i in 32:
        renderer.advance_tick()
    var floor_image := Image.create_empty(256, 128, false, Image.FORMAT_RGBA8)
    floor_image.fill(Color.WHITE)
    renderer.floor_sprite_sheet = ImageTexture.create_from_image(floor_image)
    renderer.bridge.set_level_light(Vector3.UP, Color(0.4, 0.4, 0.4),
        Color(0.2, 0.2, 0.2), 128, 1.0, 0)
    assert(renderer.bridge.add_decal(30.0, Vector3.ZERO, 0, 9, 0, 2)["ok"])
    camera.position = Vector3(0, 100, 0)
    camera.look_at(Vector3.ZERO, Vector3.FORWARD)
    assert(renderer.present(32)["ok"])
    await process_frame
    await process_frame
    image = viewport.get_texture().get_image()
    path = ProjectSettings.globalize_path("res://../work/native-fx-decal.png")
    saved = image.save_png(path)
    print("native FX decal capture: ", saved, " center=", image.get_pixel(320, 180))
    quit(0)
