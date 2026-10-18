extends SceneTree

const Fixture = preload("res://tests/skinning_fixture.gd")
const Archives = preload("res://tests/texture_fixture.gd")

var output := ""
var failures := 0
var checks := 0

func check(value: bool, reason: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(reason)

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="):
			output = arg.trim_prefix("--capture-dir=")
	call_deferred("_run")

func capture(name: String) -> void:
	await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and not image.is_empty(), "exported viewport is available")
	if image != null:
		check(image.save_png(output.path_join(name)) == OK, "exported frame saved: " + name)

func _run() -> void:
	if output.is_empty() or DisplayServer.get_name() == "headless":
		push_error("Graphical renderer and --capture-dir are required.")
		quit(1)
		return
	check(DirAccess.make_dir_recursive_absolute(output) == OK, "capture directory")
	root.size = Vector2i(900, 650)
	var environment := WorldEnvironment.new()
	var settings := Environment.new()
	settings.background_mode = Environment.BG_COLOR
	settings.background_color = Color("13222b")
	environment.environment = settings
	root.add_child(environment)
	var actor := Node3D.new()
	root.add_child(actor)
	var static_visual := MeshInstance3D.new()
	var static_mesh := SphereMesh.new()
	static_mesh.radius = 1.0
	static_mesh.height = 2.0
	static_visual.mesh = static_mesh
	static_visual.position.y = 2
	var static_material := StandardMaterial3D.new()
	static_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	static_material.albedo_color = Color("ce9245")
	static_visual.material_override = static_material
	actor.add_child(static_visual)
	var camera := Camera3D.new()
	camera.position = Vector3(0, 5.5, 15)
	root.add_child(camera)
	camera.look_at(Vector3(0, 3, 0))
	camera.make_current()
	var inspector: RefCounted = ClassDB.instantiate("ChampionsArchiveInspector")
	var preview: Control = load("res://scenes/scene_animation_preview.tscn").instantiate()
	root.add_child(preview)
	preview.configure(inspector, actor)
	preview.preview_changed.connect(func(active: bool): static_visual.visible = not active)
	var path := output.path_join("authored-animation.lmp")
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_buffer(Archives.lmp(["shape.vif", "motion.anm"], [Fixture.model(), Fixture.animation()]))
	file.close()
	preview.show_picker()
	preview.file_dialog.file_selected.emit(path)
	check(preview.archive_path == path and preview.model_choice.item_count == 2 and preview.animation_choice.item_count == 2, "exported file-picker signal reads synthetic archive and lists exact rows")
	await capture("00-picker-static.png")
	preview.model_choice.select(1)
	preview.animation_choice.select(1)
	preview.pair_button.pressed.emit()
	check(preview.active and not static_visual.visible, "picker button activates cosmetic mesh")
	await capture("01-tick-zero.png")
	# The desktop test runner can lose focus to another project window between
	# captured frames. Explicitly resume this scripted interaction.
	preview.set_suspended(false)
	preview.play_button.pressed.emit()
	preview.observe({"ready_ticks": 0})
	preview.observe({"ready_ticks": 4})
	check(preview.sample_tick == 4 and preview.playing, "play button advances four observed native ticks")
	await capture("02-tick-four.png")
	for button in preview.find_children("*", "Button", true, false):
		if button.text == "Hide picker (keep preview)":
			button.pressed.emit()
	check(not preview.visible and preview.active and not static_visual.visible, "picker can hide while cosmetic playback remains")
	await capture("02b-unobstructed.png")
	preview.show_picker()
	preview.pause_button.pressed.emit()
	preview.observe({"ready_ticks": 7})
	check(preview.sample_tick == 4 and not preview.playing, "pause button freezes exported pose")
	await capture("03-paused.png")
	var changed := FileAccess.open(path, FileAccess.WRITE)
	changed.store_buffer(Archives.lmp(["shape.vif", "motion.anm"], [Fixture.model(), Fixture.animation(512)]))
	changed.close()
	preview.restart_button.pressed.emit()
	check(not preview.active and static_visual.visible and not preview.last_error.is_empty(), "stale restart restores original static visual")
	await capture("04-error-static.png")
	preview.queue_free()
	actor.queue_free()
	camera.queue_free()
	environment.queue_free()
	await process_frame
	print("SCENE_ANIMATION_PREVIEW_VISUAL_PASS checks=%d failures=%d renderer=%s" % [checks, failures, DisplayServer.get_name()])
	quit(0 if failures == 0 else 1)
