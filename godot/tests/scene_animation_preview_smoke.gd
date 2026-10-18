extends SceneTree

const Fixture = preload("res://tests/skinning_fixture.gd")
const Archives = preload("res://tests/texture_fixture.gd")
const MeshBuilder = preload("res://scenes/scene_animation_mesh.gd")

var failures := 0
var checks := 0
var signals: Array[bool] = []

func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("_run")

func _corners(preview: Control) -> PackedVector3Array:
	return preview.mesh_instance.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]

func _mapped(source: PackedVector3Array) -> PackedVector3Array:
	var result := PackedVector3Array()
	for p in source:
		result.append(Vector3(p.x, p.z, -p.y))
	return result

func _run() -> void:
	var data := Archives.lmp(["same.vif", "same.vif", "same.anm", "same.anm"], [Fixture.model(), Fixture.model(), Fixture.animation(), Fixture.animation(512)])
	var path := ProjectSettings.globalize_path("user://scene_animation_preview_test.lmp")
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, "synthetic archive written")
	if file == null:
		quit(1)
		return
	file.store_buffer(data)
	file.close()
	var inspector: RefCounted = ClassDB.instantiate("ChampionsArchiveInspector")
	var listing: Dictionary = inspector.inspect_archive(path)
	check(listing.get("ok", false), "native inspection accepts authored archive")
	var actor := Node3D.new()
	root.add_child(actor)
	var preview: Control = load("res://scenes/scene_animation_preview.tscn").instantiate()
	root.add_child(preview)
	preview.configure(inspector, actor)
	preview.preview_changed.connect(func(active: bool): signals.append(active))
	check(preview.open_archive(path), "archive picker retains fingerprint")
	check(preview.model_choice.item_count == 3 and preview.animation_choice.item_count == 3, "duplicate names retain distinct row identities")
	check(preview.select_pair(1, 3), "second duplicate pair samples at zero")
	check(preview.active and actor.get_child_count() == 1 and signals == [true], "valid tick-zero pose publishes cosmetic child")
	check(preview.model_row == 1 and preview.animation_row == 3 and preview.archive_fingerprint == listing["fingerprint"], "exact rows and fingerprint retained")
	var native_zero: Dictionary = inspector.sample_skinned_model(path, 1, 3, "rta", listing["fingerprint"], 0)
	var initial_vertices := _corners(preview)
	check(initial_vertices == _mapped(native_zero["meshes"][0]["positions"]), "tick zero corners equal bridge positions after one axis mapping")
	var original_scale: Vector3 = preview.mesh_instance.scale
	var original_position: Vector3 = preview.mesh_instance.position
	var material: StandardMaterial3D = preview.mesh_instance.material_override
	check(material.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED and material.cull_mode == BaseMaterial3D.CULL_DISABLED and material.albedo_texture == null, "unlit two-sided position-only material")
	preview.show_picker()
	check(preview.visible and "unverified" in preview.pair_label.text.to_lower(), "picker carries pairing boundary")
	preview.play_button.pressed.emit()
	preview.observe({"ready_ticks": 10})
	check(preview.sample_tick == 0, "first observation anchors native counter")
	preview.observe({"ready_ticks": 11})
	var native_one: Dictionary = inspector.sample_skinned_model(path, 1, 3, "rta", listing["fingerprint"], 1)
	check(preview.sample_tick == 1 and _corners(preview) == _mapped(native_one["meshes"][0]["positions"]), "one native ready tick advances one bridge sample")
	check(_corners(preview) != initial_vertices, "advancing tick visibly changes vertices")
	check(preview.mesh_instance.scale == original_scale and preview.mesh_instance.position == original_position, "fit and origin frozen from initial sample")
	preview.observe({"ready_ticks": 11})
	check(preview.sample_tick == 1, "duplicate observation does not advance")
	preview.pause_button.pressed.emit()
	preview.observe({"ready_ticks": 14})
	check(preview.sample_tick == 1, "pause freezes pose")
	preview.play_button.pressed.emit()
	preview.observe({"ready_ticks": 14})
	preview.observe({"ready_ticks": 17})
	var native_four: Dictionary = inspector.sample_skinned_model(path, 1, 3, "rta", listing["fingerprint"], 4)
	check(preview.sample_tick == 4 and _corners(preview) == _mapped(native_four["meshes"][0]["positions"]), "skipped observation advances deterministically by native counter difference")
	preview.set_suspended(true)
	preview.observe({"ready_ticks": 22})
	preview.set_suspended(false)
	preview.observe({"ready_ticks": 23})
	check(preview.sample_tick == 4, "focus suspend resumes without catch-up")
	preview.observe({"ready_ticks": 24})
	check(preview.sample_tick == 5, "play continues after focus rebase")
	preview.observe({"ready_ticks": 1})
	check(preview.sample_tick == 5, "counter decrease rebases without replay")
	preview.observe({"ready_ticks": 20})
	check(preview.sample_tick == 8 and not preview.playing and _corners(preview) == _mapped(inspector.sample_skinned_model(path, 1, 3, "rta", listing["fingerprint"], 8)["meshes"][0]["positions"]), "exact end clamps and stops")
	preview.restart_button.pressed.emit()
	check(preview.sample_tick == 0 and preview.playing and _corners(preview) == initial_vertices, "user restart returns to tick zero")
	preview.observe({"ready_ticks": 20})
	preview.observe({"ready_ticks": 21})
	check(preview.sample_tick == 1, "restart rebases native observation")
	preview.clear_preview()
	check(not preview.active and actor.get_child_count() == 0 and signals.back() == false, "clear frees mesh and restores static-visual signal")
	check(not preview.open_archive(path, "norrath") and not preview.active, "wrong profile refused")
	check(preview.open_archive(path), "reopen after wrong profile")
	check(preview.select_pair(0, 2), "first duplicate pair remains selectable")
	var changed := FileAccess.open(path, FileAccess.WRITE)
	changed.store_buffer(Archives.lmp(["same.vif", "same.vif", "same.anm", "same.anm"], [Fixture.model(), Fixture.model(), Fixture.animation(512), Fixture.animation()]))
	changed.close()
	preview.play()
	preview.observe({"ready_ticks": 0})
	preview.observe({"ready_ticks": 1})
	check(not preview.active and actor.get_child_count() == 0 and not preview.last_error.is_empty(), "changed fingerprint clears stale pose and cosmetic mesh")
	preview.clear_preview()
	check(preview.last_error.is_empty() and not preview.active, "Clear dismisses a stale selection error")
	var valid: Dictionary = inspector.sample_skinned_model(path, 0, 2, "rta", inspector.inspect_archive(path)["fingerprint"], 0)
	var hidden: Dictionary = valid.duplicate(true)
	for entry in hidden["meshes"]:
		entry["visible_by_default"] = false
	check(not MeshBuilder.prepare(hidden).get("ok", true), "all-hidden geometry refused")
	var invalid: Dictionary = valid.duplicate(true)
	invalid["meshes"][0]["positions"][0] = Vector3(INF, 0, 0)
	check(not MeshBuilder.prepare(invalid).get("ok", true), "nonfinite geometry refused")
	var animation_writer = load("res://tests/animation_fixture.gd")
	var rest_positions := [[64,128,192], [-64,256,128], [320,-128,64], [-256,64,-128]]
	var rest_rotations := [[2896,0,0,2896], [2896,2896,0,0], [4096,0,0,0], [0,0,4096,0]]
	var zero_clip: PackedByteArray = animation_writer._clip([0,0,0,0], rest_positions, rest_rotations, [], 0)
	var short_clip: PackedByteArray = animation_writer._clip([0,0,0,0], rest_positions, rest_rotations, [[0,1,0,[256,0,0]]], 1)
	var attachment: PackedByteArray = Fixture.model()
	attachment[0x13] = 0
	var partial: PackedByteArray = Fixture.animation()
	partial[0x48] = 1
	var edges := Archives.lmp(["shape.vif", "zero.anm", "short.anm", "attachment.vif", "partial.anm"], [Fixture.model(), zero_clip, short_clip, attachment, partial])
	var edge_file := FileAccess.open(path, FileAccess.WRITE)
	edge_file.store_buffer(edges)
	edge_file.close()
	check(preview.open_archive(path), "edge archive inspected")
	check(preview.select_pair(0, 1) and preview.duration_ticks == 0, "zero-duration clip shows checked tick-zero pose")
	preview.play()
	preview.observe({"ready_ticks": 0})
	preview.observe({"ready_ticks": 5})
	check(preview.sample_tick == 0 and not preview.playing, "zero-duration clip cannot advance")
	check(preview.select_pair(0, 2) and preview.duration_ticks == 1, "short clip selected")
	preview.play()
	preview.observe({"ready_ticks": 0})
	preview.observe({"ready_ticks": 2})
	check(preview.sample_tick == 1 and not preview.playing, "short clip clamps to exact end on skipped observation")
	check(not preview.select_pair(3, 2) and not preview.active and actor.get_child_count() == 0, "attachment refusal clears prior cosmetic mesh")
	check(not preview.select_pair(0, 4) and not preview.active and not preview.last_error.is_empty(), "partial-body refusal is actionable")
	check(preview.select_pair(0, 2), "valid pair can recover after refusal")
	for button in preview.find_children("*", "Button", true, false):
		if button.text == "Close preview":
			button.pressed.emit()
	check(not preview.active and not preview.visible and actor.get_child_count() == 0, "close releases mesh and restores static visual")
	check(preview.select_pair(0, 2), "pair can reopen after close")
	FileAccess.open(path, FileAccess.WRITE).close()
	actor.queue_free()
	preview.queue_free()
	await process_frame
	check(not is_instance_valid(preview), "scene change frees preview node and mesh")
	print("SCENE_ANIMATION_PREVIEW_PASS checks=", checks, " failures=", failures)
	quit(0 if failures == 0 else 1)
