extends "res://tests/playable_wave_smoke_animation.gd"

func continuation_cases() -> void:
	await super.continuation_cases()
	var fx = scene.fx
	check(fx != null and fx.definition_source.begins_with("synthetic"), "no user JSON: explicit synthetic test definitions")
	if fx == null: return
	check(scene.fx_status.visible and scene.fx_status.get_index() < scene.viewport.get_parent().get_index(), "synthetic FX disclosure appears above the play view")
	check(not "effects are omitted" in scene.scope_label.text, "play scope describes active effects")
	# The preceding journey has already lit WORLD, manual and native actor meshes.
	await capture("fx-01-lit-continuation")
	check(fx.lighting_calls.world > 0 and fx.lighting_calls.model > 0, "released WORLD/model vertex lighting used in play")
	var ground = scene.terrain_root.get_child(0)
	var colors: PackedColorArray = ground.mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
	check(not colors.is_empty() and colors[0] != Color.WHITE, "WORLD vertices carry native light colours")
	var first_mesh = ground.mesh
	fx.light_scene()
	check(ground.mesh == first_mesh, "unchanged WORLD does not rebake or multiply lighting")
	write(directory.path_join("bad-fx.json"), PackedByteArray([123]))
	var old_renderer = fx.renderer
	check(not fx.load_definition_file(directory.path_join("bad-fx.json")).ok and fx.renderer == old_renderer and fx.definition_source.begins_with("synthetic"), "invalid explicitly selected JSON preserves cosmetic resources")

	for detail in [0,1,2]:
		scene.return_to_selection()
		scene.combat_choice.button_pressed = true
		scene.actions.play.pressed.emit()
		fx.set_gore(detail)
		scene.resume_session()
		scene.advance_step() # no-ready network submission
		check(fx.last_tick == 0 and fx.hit_hooks == 0, "no-ready input emits no FX clock or event")
		# One catch-up call must deliver every native event, not only its last tick.
		scene._observe(scene.controller.advance_time(220000, 0))
		while scene.raw_observation.ready_ticks < 12: scene.advance_step()
		check(scene.raw_observation.hp == 84 and fx.hit_hooks == 2, "two actual combat hits reach FX through catch-up ticks")
		# Exact native-fx-2 fx_spawn 0 (returns at gore 0) and decal_add gates.
		print("FX_GORE ", detail, " particles ", fx.spawned_particles, " counts ", JSON.stringify(fx.renderer.bridge.counts()))
		check(fx.spawned_particles == (0 if detail == 0 else 600), "exact spawn-0 recipe: 300 particles per hit above gore 0")
		check(fx.renderer.bridge.counts().decals == (0 if detail == 0 else 2), "hit creates native red splat decals per setting")
		await capture("fx-02-hit-gore" + str(detail))
		check((fx.last_runs > 0) == (detail != 0), "the renderer presents native GS-order runs (bursts, decals) per setting")
		var before: Dictionary = scene.controller.get_observation()
		var ticks: int = fx.last_tick
		scene.set_paused(true)
		for i in range(3): scene.advance_step()
		check(fx.last_tick == ticks and fx.hit_hooks == 2, "pause and repeated observations never replay hit hooks")
		scene.resume_session()
		scene.actions.combat_hit.pressed.emit()
		scene.advance_step()
		check(fx.hit_hooks == 3 and fx.death_hooks == 0, "player retaliation reaches hit hook before death")
		scene.advance_step()
		check(fx.death_hooks == 1 and scene.raw_observation.combat_actors[1].dead, "native death queue reaches FX once")
		check(fx.renderer.bridge.counts().decals == (0 if detail == 0 else (3 if detail == 1 else 4)), "full-detail pool gated separately from blood splats")
		await capture("fx-03-death-gore" + str(detail))
		scene.resume_session()
		scene.advance_step()
		check(not scene.raw_observation.combat_actors[1].present and fx.death_hooks == 1, "removal does not duplicate death FX")
		check(scene.raw_observation.combat_shared_seed == 1140036074, "FX cosmetic RNG never changes combat RNG")
		check(fx.refusals.is_empty(), "supported fixture effects and lighting have no refusal: " + str(fx.refusals))
		var counts: Dictionary = fx.renderer.bridge.counts()
		check(counts.trails == 0 and counts.beams == 0 and counts.glows == 0, "the authored blood policy makes no trails, beams or glows")
		scene.reset_session()
		check(fx.hit_hooks == 0 and fx.death_hooks == 0 and fx.renderer.bridge.counts().decals == 0, "reset discards cosmetic history")
		scene.combat_choice.button_pressed = false
		check(scene.load_session_file(directory.path_join("combined.ocplay")), "ordinary continuation replaces combat and FX owners")

	# Native animation event -> combat damage -> FX, on the same documented tick.
	scene.return_to_selection()
	scene.animated_choice.button_pressed = true
	scene.actions.play.pressed.emit()
	fx.set_gore(2)
	scene.resume_session()
	for i in range(37): scene.advance_step() # first no-ready, then ticks 1..36
	check(fx.hit_hooks == 0, "no FX before animation event")
	scene.advance_step()
	check(scene.raw_observation.ready_ticks == 37 and scene.raw_observation.hp == 84 and fx.hit_hooks == 2, "exact tick-37 animation/combat/FX chain")
	await capture("fx-04-animated-hit-tick37")
	for slot in [0,1,2]:
		var instance = scene.combat_displays[slot]
		var arrays: Array = instance.mesh.surface_get_arrays(0)
		check(arrays[Mesh.ARRAY_VERTEX] == scene.raw_observation.combat_actors[slot].skinned_vertices and arrays[Mesh.ARRAY_COLOR].size() == 12, "lighting preserves current native skinned pose and supplies colours")
	var old_pose: PackedVector3Array = scene.combat_displays[1].mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	scene.resume_session()
	scene.advance_step()
	await capture("fx-05-updated-skinned-pose")
	var next_pose: PackedVector3Array = scene.combat_displays[1].mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	check(next_pose != old_pose and next_pose == scene.raw_observation.combat_actors[1].skinned_vertices, "lighting cache accepts each new native pose instead of freezing first mesh")
	check(scene.combat_displays[1].material_override.vertex_color_use_as_albedo, "material override consumes native vertex colours")
	check(fx.refusals.is_empty(), "animated lighting supported")
	print("PLAYABLE_FX_RESULT ", JSON.stringify({"hit_hooks":fx.hit_hooks,"death_hooks":fx.death_hooks,"particles":fx.spawned_particles,"decals":fx.renderer.bridge.counts().decals,"lighting":fx.lighting_calls,"definitions":fx.definition_source}))
	scene.animated_choice.button_pressed = false
	scene.combat_choice.button_pressed = false
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "load clears animated cosmetic pools")

	# Inspect the real decoded WORLD triangle close up, without changing its
	# geometry or adding any retail data. Only the display camera is moved.
	check(scene.load_terrain_file(directory.path_join("first.WORLD")), "FX close-up loads real synthetic WORLD decoder output")
	scene.resume_session()
	var world_mesh: Array = scene.terrain_root.get_child(0).mesh.surface_get_arrays(0)
	check(world_mesh[Mesh.ARRAY_COLOR].size() == 3 and "synthetic vertex lighting" in scene.terrain_label.text, "decoded WORLD triangle carries native lighting and accurate scope label")
	scene.camera.position = Vector3(16, 18, 22)
	scene.camera.look_at(Vector3(2, 1, -5), Vector3.UP)
	await capture("fx-06-lit-world-closeup")
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "close-up returns to ordinary continuation")

	# Simulate an explicit file choice using only an authored synthetic JSON file.
	var selected := directory.path_join("selected-synthetic-fx.json")
	write(selected, preload("res://fx/playable_fx_fixture.gd").definitions().to_utf8_buffer())
	scene.actions.fx_definitions.pressed.emit()
	scene.fx_definitions.file_selected.emit(selected)
	scene.fx_definitions.hide()
	check(fx.definition_source.begins_with("user-selected") and scene.paused, "explicit JSON picker loads runtime definitions while paused")
