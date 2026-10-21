extends "res://tests/playable_wave_smoke_fx.gd"

func continuation_cases() -> void:
	await super.continuation_cases()
	check(ClassDB.class_exists("ChampionsLevelTexturePreview") and scene.level_textures != null, "real texture extension and play composition loaded")
	if scene.level_textures == null: return
	var world := WorldFixture.world(SandboxCases.new().flat_world())
	world.encode_u32(0xa4, 0x40)
	world.encode_u16(0xb8, 5049)
	var packet: int = world.decode_u32(0x84)
	for i in range(6): world.encode_u16(packet + 0x90 + 2 * i, [0,0,640,0,0,320][i])
	var world_path := directory.path_join("lit-texture.WORLD")
	var tex_path := directory.path_join("authored-level.TEX")
	write(world_path, world)
	write(tex_path, FileAccess.get_file_as_bytes("res://tests/fixtures/level_texture_synthetic.bin"))
	choose_level(world_path)
	scene.actions.level_tex.pressed.emit()
	check(scene.paused and scene.level_tex_dialog.visible, "texture selection pauses the real scene")
	scene.level_tex_dialog.file_selected.emit(tex_path)
	scene.level_tex_dialog.hide()
	check(scene.terrain_root.has_meta("level_texture_pair"), "selected pair owns decoded texture resources")
	scene.actions.play.pressed.emit()
	check(scene.fx.enabled and scene.play_mode, "textured native play starts")
	var mesh: MeshInstance3D = scene.terrain_root.get_child(0)
	var arrays: Array = mesh.mesh.surface_get_arrays(0)
	var material: ShaderMaterial = mesh.material_override
	var texture: Texture2D = material.get_shader_parameter("base_texture")
	check(texture.get_width() == 40 and texture.get_height() == 20 and roundi(texture.get_image().get_pixel(1,0).r*255) == 3, "exact decoded synthetic pixels reach play material")
	check(arrays[Mesh.ARRAY_VERTEX][0] == Vector3(1,-1,-1) and arrays[Mesh.ARRAY_TEX_UV].size() == 3, "real WORLD positions and documented UVs retained")
	check(material.shader.code.contains("texel.rgb * COLOR.rgb") and material.get_meta("native_fx_vertex_colors",false), "texture shader consumes native FX vertex colours")
	var colours: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
	check(colours.size() == 3, "native WORLD lighting supplies every textured vertex")
	for i in range(3):
		var position: Vector3 = scene.fx.renderer.to_native(mesh.global_transform * arrays[Mesh.ARRAY_VERTEX][i])
		var normal: Vector3 = scene.fx.renderer.to_native(arrays[Mesh.ARRAY_NORMAL][i])
		var native_rgb: Vector3 = scene.fx.renderer.bridge.shade_world(position,normal)["rgb"]
		check(Vector3(colours[i].r,colours[i].g,colours[i].b).is_equal_approx(native_rgb/255.0), "textured light colour matches released native FX result")
	check(colours[0].r > colours[0].b and colours[0] != Color.WHITE, "native point-light tint reaches textured geometry")
	var old_root: Node3D = scene.terrain_root
	var old_state: Dictionary = scene.controller.get_observation()
	check(not scene.load_textured_terrain(world_path,directory.path_join("absent.TEX")) and scene.terrain_root == old_root and same_state(scene.controller.get_observation(),old_state), "invalid TEX keeps native state and lit texture root atomically")
	for i in range(6): scene.advance_step()
	check(scene.terrain_root == old_root and scene.fx.refusals.is_empty(), "ready play retains textures and native lighting")
	# Camera-only close-up of actual decoded geometry; no authored replacement
	# mesh or texture. The actor is hidden for this inspection, then restored.
	scene.actor.hide()
	scene.camera.position = Vector3(14,17,13)
	scene.camera.look_at(Vector3(1,1,-5),Vector3.UP)
	await capture("texture-01-native-lit-world")
	if DisplayServer.get_name() != "headless":
		await graphical_light_check(mesh)
	scene.actor.show()
	scene.reset_session()
	check(scene.terrain_root == old_root and scene.terrain_root.has_meta("level_texture_pair"), "reset preserves selected texture ownership")
	scene.resume_session()
	for i in range(5): scene.advance_step()
	scene.set_paused(true)
	var save := directory.path_join("textured.ocplay")
	check(scene.save_session_file(save), "native continuation saves textured play without altering format")
	var saved: Dictionary = play_state(scene.controller.get_observation())
	choose_level(directory.path_join("second.WORLD"))
	check(not scene.terrain_root.has_meta("level_texture_pair"), "unpaired level does not inherit another level's textures")
	check(scene.load_session_file(save) and same_state(play_state(scene.controller.get_observation()),saved), "continuation restores native state")
	check(scene.terrain_root.has_meta("level_texture_pair"), "same-session exact save restores its owned decoded textures")
	scene.resume_session()
	scene.actor.hide()
	scene.camera.position = Vector3(14,17,13)
	scene.camera.look_at(Vector3(1,1,-5),Vector3.UP)
	await capture("texture-03-loaded-lit-continuation")
	scene.actor.show()
	choose_level(directory.path_join("second.WORLD"))
	choose_level(world_path)
	check(scene.terrain_root.has_meta("level_texture_pair"), "library revisit reopens only previously selected WORLD/TEX pair")
	# Unknown save copies have no client-owned asset association; no asset path
	# is inferred from collision bytes or saved simulation state.
	var copy := directory.path_join("unknown-textured.ocplay")
	write(copy, FileAccess.get_file_as_bytes(save))
	check(scene.load_session_file(copy) and not scene.terrain_root.has_meta("level_texture_pair"), "unknown save copy loads neutral without guessing texture association")
	write(save, FileAccess.get_file_as_bytes(directory.path_join("combined.ocplay")))
	check(scene.load_session_file(save) and not scene.terrain_root.has_meta("level_texture_pair"), "changed bytes at a known save path cannot reuse stale texture ownership")
	world.encode_u32(0xa8, 0x2200)
	var special := directory.path_join("constant-texture.WORLD")
	write(special, world)
	check(scene.load_textured_terrain(special,tex_path), "special chunk retains decoded base texture")
	scene.actions.play.pressed.emit()
	var static_mesh: MeshInstance3D = scene.terrain_root.get_child(0)
	check(not static_mesh.get_meta("native_fx_world_dynamic", true) and "constant chunk light" in scene.terrain_label.text, "0x2000 wins over 0x200 and does not take point lights")
	# The base level is the loaded level's (render-pipeline 5.1). The synthetic
	# fixture has none, so the constant colour is refused: base texture only.
	var unlit_colours = static_mesh.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
	check((unlit_colours == null or unlit_colours.is_empty()) and "gated" in scene.terrain_label.text, "constant chunk light is refused without the level's base level")
	# The loaded level's light record (synthetic row alpha, base 0x40) supplies it.
	scene.fx.select_level_light(ProjectSettings.globalize_path("res://tests/fixtures/level_lights_synthetic.json"), "alpha")
	scene.fx.light_world()
	var static_colours: PackedColorArray = static_mesh.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
	check(scene.fx.level_light_active and static_colours.size() == 3 and static_colours[0].is_equal_approx(Color.WHITE) and static_colours[1].is_equal_approx(Color.WHITE), "0x2000 at the level's base 0x40 clamps 294 to 255")
	var modulate_scale = static_mesh.material_override.get_shader_parameter("modulate_scale")
	check(modulate_scale != null and is_equal_approx(float(modulate_scale), 255.0 / 128.0), "the constant colour is presented as texel x colour / 128")
	scene.fx.select_level_light("", "")
	check(static_mesh.material_override.render_priority == 4 and static_mesh.sorting_offset == 0.0, "0x2200 is layer 4 and the only packet keeps send index 0")
	print("PLAYABLE_TEXTURE_RESULT ",JSON.stringify({"decoded_size":[40,20],"native_vertex_colours":colours.size(),"save_restore":true,"environment":true,"console_depth":true,"automap_mapping":true,"automap_pixels":false}))

func graphical_light_check(mesh: MeshInstance3D) -> void:
	await RenderingServer.frame_post_draw
	var lit: Image = scene.viewport.get_texture().get_image()
	var original: Mesh = mesh.mesh
	# A display-only white-colour reference proves that the shader actually
	# modulates decoded pixels, beyond just checking stored mesh arrays.
	var reference: Array = original.surface_get_arrays(0)
	var whites := PackedColorArray([Color.WHITE,Color.WHITE,Color.WHITE])
	reference[Mesh.ARRAY_COLOR] = whites
	var unlit := ArrayMesh.new()
	unlit.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES,reference)
	mesh.mesh = unlit
	scene.fx.set_process(false)
	for i in range(4): await process_frame
	await RenderingServer.frame_post_draw
	var full: Image = scene.viewport.get_texture().get_image()
	var changed := 0
	for y in range(lit.get_height()):
		for x in range(lit.get_width()):
			var a := lit.get_pixel(x,y)
			var b := full.get_pixel(x,y)
			if absf(a.r-b.r)+absf(a.g-b.g)+absf(a.b-b.b) > 0.02: changed += 1
	check(changed > 100, "rendered textured pixels visibly change with native FX vertex lighting")
	mesh.mesh = original
	scene.fx.set_process(true)
	await capture("texture-02-lit-colour-restored")
	print("TEXTURED_LIGHT_PIXELS_CHANGED ",changed)
