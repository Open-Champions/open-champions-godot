extends RefCounted

func run(cases: RefCounted, inspector: RefCounted, browser: Control) -> void:
	var fixture = load("res://tests/skinning_fixture.gd")
	var models = load("res://tests/model_fixture.gd")
	var archives = load("res://tests/texture_fixture.gd")
	var names: Array[String] = ["note", "shape.vif", "shape.vif", "motion.anm", "motion.anm"]
	var payloads: Array[PackedByteArray] = [PackedByteArray([0]),models.model(),fixture.model(),fixture.animation(),fixture.animation(512)]
	var child: PackedByteArray = archives.lmp(names,payloads)
	var path: String = cases.write_file("skin-viewer.LMP",child)
	browser.open_archive(path)
	var viewer = browser.skinning_viewer
	cases.select_row(browser,1)
	cases.check(not browser.use_model_button.disabled and browser.skinning_button.disabled, "model selection enables explicit retention only")
	browser._use_selected_model()
	cases.check(browser.skinning_model_row_index == 1 and browser.skinning_fingerprint == browser.current_fingerprint and not viewer.visible, "Use model retains exact original row and archive fingerprint without inventing a clip")
	cases.select_row(browser,3)
	cases.check(not browser.skinning_button.disabled and browser.skinning_model_row_index == 1 and not browser.animation_viewer.visible, "clip selection retains model and enables explicit sampling without competing joint window")
	browser._preview_skinned_model(false)
	cases.check(viewer.mesh_count == 1 and viewer.triangle_count == 1 and viewer.total_mesh_count == 2 and "unverified" in viewer.pairing_label.text.to_lower(), "default-only unlit viewer shows visible mesh and explicit unverified pairing")
	var mapped := PackedVector3Array([Vector3(1,3,-2),Vector3(1,3,-4),Vector3(0,5,-2)])
	cases.check(viewer.cached_meshes.size() == 2 and cases.skin_positions_equal(viewer.cached_meshes[0]["positions"], mapped), "native Z-up is mapped once while hidden mesh remains owned metadata")
	var instance: MeshInstance3D = viewer.mesh_root.get_child(0)
	var material: StandardMaterial3D = instance.material_override
	cases.check(material != null and material.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED and material.cull_mode == BaseMaterial3D.CULL_DISABLED and material.albedo_texture == null, "skinned geometry is unlit and two-sided without stale textures")
	viewer.orbit(0.3,0.2)
	viewer.zoom(0.8)
	var camera_before: Transform3D = viewer.camera.transform
	var origin_before: Vector3 = viewer.display_origin
	browser._sample_skinned_model(1.5,false,true)
	cases.check(viewer.sampled_tick == 1.5 and viewer.camera.transform.is_equal_approx(camera_before) and viewer.display_origin == origin_before, "fractional sampling preserves camera and render origin")
	cases.check(viewer.cached_meshes[0]["positions"] == PackedVector3Array([Vector3(2.5,3,-2),Vector3(2.5,3,-4),Vector3(1.5,5,-2)]), "sampled skin positions actually change in viewer")
	viewer.tick_input.get_line_edit().text = "0"
	viewer.sample_button.pressed.emit()
	cases.check(viewer.sampled_tick == 0 and cases.skin_positions_equal(viewer.cached_meshes[0]["positions"], mapped), "viewer Sample button supports typed backward time through browser and native bridge")
	var press := InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = Vector2(30,40)
	viewer.input_surface.gui_input.emit(press)
	var motion := InputEventMouseMotion.new()
	motion.position = Vector2(42,45)
	var prior_drag: Transform3D = viewer.camera.transform
	viewer.input_surface.gui_input.emit(motion)
	cases.check(not viewer.camera.transform.is_equal_approx(prior_drag), "skinned viewer drag uses positions without relative or button mask")
	var after_drag: Transform3D = viewer.camera.transform
	var release := InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	viewer.window_input.emit(release)
	motion.position = Vector2(55,65)
	viewer.input_surface.gui_input.emit(motion)
	cases.check(not viewer.dragging and viewer.camera.transform.is_equal_approx(after_drag), "outside release ends skin viewer drag")
	viewer.input_surface.gui_input.emit(press)
	viewer.focus_exited.emit()
	cases.check(not viewer.dragging, "focus loss ends skin viewer pointer capture")
	var wheel := InputEventMouseButton.new()
	wheel.button_index = MOUSE_BUTTON_WHEEL_UP
	wheel.pressed = true
	var old_distance: float = viewer.distance
	viewer.input_surface.gui_input.emit(wheel)
	cases.check(viewer.distance < old_distance and not viewer.input_surface.mouse_force_pass_scroll_events, "skin viewer wheel zoom prevents surrounding page scroll")
	cases.select_row(browser,4)
	cases.check(viewer.mesh_count == 0 and browser.skinning_model_row_index == 1, "new selected clip clears old geometry but retains explicit model")
	browser._preview_skinned_model(false)
	cases.check(viewer.cached_meshes[0]["positions"][0] == Vector3(8,3,-2) and browser.skinning_animation_row_index == 4, "duplicate clip names preserve exact selected row identity")
	browser.search.text = "shape"
	browser.search.text_changed.emit(browser.search.text)
	cases.check(browser.skinning_model_row_index == -1 and viewer.mesh_count == 0 and browser.skinning_button.disabled, "filter invalidates explicit pair and old skin geometry")
	cases.select_row(browser,2)
	browser._use_selected_model()
	cases.check(browser.skinning_model_row_index == 2, "filtered model retention uses original index")
	browser.open_archive(path)
	cases.check(browser.skinning_model_row_index == -1, "reopening archive clears explicit model identity")
	cases.select_row(browser,1)
	browser._use_selected_model()
	cases.select_row(browser,3)
	browser._preview_skinned_model(false)
	var replacements: Array[PackedByteArray] = payloads.duplicate()
	replacements[3] = fixture.animation(512)
	cases.write_file("skin-viewer.LMP",archives.lmp(names,replacements))
	browser._sample_skinned_model(1,false,true)
	cases.check(browser.skinning_model_row_index == -1 and browser.skinning_animation_row_index == -1 and viewer.mesh_count == 0 and not viewer.visible, "stale archive sample clears both retained pair and geometry")
	cases.write_file("skin-viewer.LMP",child)
	browser.open_archive(path)
	cases.select_row(browser,1)
	browser._use_selected_model()
	cases.select_row(browser,3)
	browser._preview_skinned_model(false)
	browser.game_choice.select(1)
	browser._profile_changed(1)
	cases.check(browser.skinning_model_row_index == -1 and viewer.mesh_count == 0 and browser.use_model_button.disabled and browser.skinning_button.disabled, "Norrath profile clears pair and disables unsupported skin workflow")
	browser.game_choice.select(0)
	browser._profile_changed(0)
	browser.open_archive(path)
	cases.select_row(browser,1)
	browser._use_selected_model()
	cases.select_row(browser,3)
	browser._preview_skinned_model(false)
	browser._clear()
	cases.check(viewer.cached_meshes.is_empty() and not viewer.visible and browser.skinning_model_row_index == -1, "Clear removes sampled geometry and retained selection")
	var listing: Dictionary = inspector.inspect_archive(path)
	var result: Dictionary = inspector.sample_skinned_model(path,1,3,"rta",listing["fingerprint"],0)
	cases.check(viewer.show_skinned_model(result,false,false), "viewer accepts validated owned sample result")
	result["meshes"][0]["positions"][0] = Vector3(999,999,999)
	cases.check(viewer.cached_meshes[0]["positions"][0] == mapped[0], "viewer owns positions independently from caller dictionaries")
	browser.open_archive(path)
	cases.select_row(browser,1)
	browser._use_selected_model()
	browser._clear_skin_pair()
	browser.file_dialog.canceled.emit()
	cases.check(browser.skinning_model_row_index == -1 and not browser.use_model_button.disabled, "canceled Open dialog keeps pair cleared but restores explicit model choice")
	var clean: Dictionary = inspector.sample_skinned_model(path,1,3,"rta",listing["fingerprint"],0)
	for flag in [true,0,"false",null]:
		var bad: Dictionary = clean.duplicate(true)
		bad["pairing_verified"] = flag
		cases.check(not viewer.show_skinned_model(bad,false,true) and viewer.cached_meshes.is_empty(), "invalid pairing evidence flag cannot enter viewer")
	var missing: Dictionary = clean.duplicate(true)
	missing.erase("pairing_verified")
	cases.check(not viewer.show_skinned_model(missing,false,false), "missing pairing evidence flag is rejected")
	for mode in ["hidden_nan","duplicate","triples","bad_visibility","wrong_type","time"]:
		var bad: Dictionary = clean.duplicate(true)
		match mode:
			"hidden_nan": bad["meshes"][1]["positions"][0] = Vector3(NAN,0,0)
			"duplicate": bad["meshes"][1]["index"] = 0
			"triples": bad["meshes"][0]["positions"] = PackedVector3Array([Vector3.ZERO])
			"bad_visibility": bad["meshes"][0]["visible_by_default"] = 1
			"wrong_type": bad["meshes"][0]["positions"] = []
			"time": bad["tick"] = INF
		cases.check(not viewer.show_skinned_model(bad,false,true) and viewer.mesh_root.get_child_count() == 0 and viewer.cached_meshes.is_empty(), "malformed visible or hidden geometry and metadata reject atomically: " + mode)
	var hidden: Dictionary = clean.duplicate(true)
	for mesh: Dictionary in hidden["meshes"]:
		mesh["visible_by_default"] = false
	cases.check(viewer.show_skinned_model(hidden,false,false) and viewer.mesh_count == 0 and viewer.mesh_root.get_child_count() == 0 and viewer.frame_button.disabled, "all-hidden result keeps honest empty viewport without fabricated geometry")
	var empty: Dictionary = clean.duplicate(true)
	empty["meshes"] = []
	empty["triangle_count"] = 0
	cases.check(viewer.show_skinned_model(empty,false,false) and viewer.mesh_count == 0, "empty mesh result is explicitly empty")
	var large: Dictionary = clean.duplicate(true)
	for mesh: Dictionary in large["meshes"]:
		var points: PackedVector3Array = mesh["positions"]
		for i in range(points.size()):
			points[i] += Vector3(5e11,5e11,5e11)
		mesh["positions"] = points
	cases.check(viewer.show_skinned_model(large,false,false) and viewer.camera.position.is_finite() and not viewer.camera.position.is_equal_approx(viewer.center), "large coordinate Frame preserves finite nondegenerate camera")
	var large_origin: Vector3 = viewer.display_origin
	var large_camera: Transform3D = viewer.camera.transform
	cases.check(viewer.show_skinned_model(clean,false,true) and viewer.display_origin == large_origin and viewer.camera.transform.is_equal_approx(large_camera), "backward sample keeps render origin and camera until explicit Frame")
	viewer.frame_model()
	cases.check(viewer.display_origin.length() < 1000 and viewer.camera.position.is_finite(), "Frame rebases current geometry and camera together")
	viewer.close_requested.emit()
	cases.check(not viewer.visible and viewer.cached_meshes.is_empty() and viewer.sample_button.disabled, "closing skin viewer clears owned geometry and controls")
	browser._clear()
