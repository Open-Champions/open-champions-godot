extends "res://tests/playable_wave_smoke_level.gd"

func continuation_cases() -> void:
	await super.continuation_cases()
	var controller = scene.controller
	var before: Dictionary = controller.get_observation()
	check(not controller.start_play_session(false, false, true).ok and controller.get_observation() == before, "animated policy requires explicit combat opt-in atomically")
	scene.return_to_selection()
	scene.animated_choice.button_pressed = true
	scene.actions.play.pressed.emit()
	check(scene.raw_observation.get("animated_enabled", false), "scene opts into native animation dispatch")
	if not scene.raw_observation.get("animated_enabled", false): return
	check(scene.raw_observation.combat_policy == "rta-animated-combat-fixture-v1", "explicit synthetic cross-module policy")
	check(scene.save_methods == ["save_file"] and scene.actions.combat_hit.disabled, "unknown action timing; fixture continuation saves")
	check(scene.combat_displays.size() == 3 and not scene.actor_visual.visible, "three native skinned meshes replace static actor markers")
	var initial: PackedVector3Array = scene.raw_observation.combat_actors[1].skinned_vertices
	await capture("animation-01-native-poses")
	scene.resume_session()
	scene.advance_step()
	check(scene.raw_observation.ready_ticks == 0 and scene.raw_observation.combat_actors[1].animation_tick == 0, "no-ready submission does not advance animation")
	for tick in range(1,37):
		scene.advance_step()
		check(scene.raw_observation.ok and scene.raw_observation.hp == 100, "no damage before table event")
		for actor in scene.raw_observation.combat_actors:
			check(actor.animation_tick == tick and actor.skinned_vertices.size() == 12, "one real pose per actor per ready tick")
	check(scene.raw_observation.combat_actors[1].skinned_vertices != initial, "native child pose visibly deforms geometry")
	var rendered: PackedVector3Array = scene.combat_displays[1].mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	check(rendered == scene.raw_observation.combat_actors[1].skinned_vertices, "render mesh uses native skinned vertices")
	scene.set_paused(true)
	before = controller.get_observation()
	for i in range(3): scene.advance_step()
	check(controller.get_observation() == before, "pause freezes dispatch, poses and combat")
	await capture("animation-02-before-tick37")
	scene.resume_session()
	scene.advance_step()
	check(scene.raw_observation.ready_ticks == 37 and scene.raw_observation.hp == 84 and scene.raw_observation.damage_events == 2, "0.6 second attacks damage on exact ready tick 37")
	for slot in [1,2]:
		var events: Array = scene.raw_observation.combat_actors[slot].animation_events
		check(events.size() == 1 and events[0].kind == "hit_test" and events[0].mask == 1, "typed 0x1 hit reaches Session adapter")
	await capture("animation-03-tick37-hit")
	if DisplayServer.get_name() != "headless":
		check(scene.scene_scroll.get_child(0).size.x <= root.size.x, "animated labels keep health display within window width")
	scene.resume_session()
	scene.advance_step()
	check(scene.raw_observation.hp == 84 and scene.raw_observation.damage_events == 2, "hit not delivered twice")
	choose_pair(directory.path_join("cosmetic.lmp"))
	check(not scene.combat_displays[0].visible and scene.components.animation.active, "manual preview retains cosmetic ownership")
	await capture("animation-04-manual-cosmetic-tool")
	scene.components.animation.clear_preview()
	check(scene.combat_displays[0].visible and not scene.actor_visual.visible, "clearing cosmetic preview restores native player pose")
	press(scene.components.animation, "Hide picker (keep preview)")
	scene.resume_session()
	for i in range(52): scene.advance_step()
	check(scene.raw_observation.stopped and scene.raw_observation.ready_ticks == 90 and not scene.raw_observation.failed, "fixture stops at its advertised boundary")
	check(scene.raw_observation.hp == 74 and scene.raw_observation.damage_events == 4, "second real animation event reaches combat")
	check(scene.components.art.loaded.size() == 23 and scene.components.animation != null, "art and manual preview present")
	print("ANIMATED_COMBAT_RESULT ", JSON.stringify(scene.raw_observation))
	await capture("animation-05-bounded-complete")
	scene.reset_session()
	check(scene.raw_observation.animated_enabled and scene.raw_observation.ready_ticks == 0, "reset rebuilds animation and combat owners")
	scene.animated_choice.button_pressed = false
	scene.combat_choice.button_pressed = false
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "normal continuation atomically replaces animated fixture")
	check(not scene.raw_observation.get("animated_enabled", false) and scene.actor_visual.visible, "normal play ownership restored")
