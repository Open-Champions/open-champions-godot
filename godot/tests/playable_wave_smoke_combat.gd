extends "res://tests/playable_wave_smoke_ui.gd"

# Full long-play/HUD/inventory acceptance, then a separate explicit combat
# fixture policy, saved as a fixture continuation (review finding 2).
func continuation_cases() -> void:
	await super.continuation_cases()
	var controller = scene.controller
	scene.return_to_selection()
	scene.combat_choice.button_pressed = true
	scene.actions.play.pressed.emit()
	check(scene.raw_observation.get("combat_enabled", false), "explicit Start play selects native combat fixture")
	if not scene.raw_observation.get("combat_enabled", false): return
	check(scene.raw_observation.combat_policy == "rta-combat-fixture-v1", "versioned combat observation policy")
	var actors: Array = scene.raw_observation.combat_actors
	check(actors.size() == 3 and actors[0].kind == "Player" and actors[1].kind == "Skelight" and actors[2].kind == "Skelight", "documented player and two Skelight fixtures")
	for row in actors: check(row.present and not row.dead, "three simultaneous living actors")
	check(not "W A S D" in scene.status_label.text and "Synthetic" in scene.status_label.text, "combat help describes supported fixture controls")
	check(scene.save_methods == ["save_file"], "combat advertises the fixture continuation")
	await capture("combat-01-three-alive")
	scene.resume_session()
	var before: Dictionary = controller.get_observation()
	for command in [1, 16, 31]:
		check(not controller.advance(command).ok and controller.get_observation() == before, "combat rejects legacy movement/damage without mutation")
	for request in [[0, 2], [1, 1], [-1, 1], [3, 1]]:
		check(not controller.combat_hit_event(request[0], request[1]).ok and controller.get_observation() == before, "hit event refusals preserve state")
	for method in ["set_recovery_test", "set_facing_test", "set_animation_cache_test"]:
		check(not controller.call(method, true).ok and controller.get_observation() == before, "combat rejects " + method)
	check(not controller.animation_cache_command(0, 1).ok, "combat rejects animation cache task")
	scene.advance_step()
	check(scene.raw_observation.ready_ticks == 0, "no-ready submission does not run combat")
	for i in range(100):
		if scene.raw_observation.hp < 100 or scene.stopped: break
		scene.advance_step()
	print("COMBAT_DAMAGE ", JSON.stringify(scene.raw_observation))
	check(not scene.stopped and scene.raw_observation.hp_raw == 1118306304 and scene.raw_observation.ready_ticks == 12, "creature animation attacks damage player on exact ready frame")
	check(scene.components.art.hud_view.records.size() > 0, "native HP feeds original-art HUD")
	await capture("combat-02-player-damaged")
	scene.resume_session()
	scene.actions.combat_hit.pressed.emit()
	scene.advance_step()
	actors = scene.raw_observation.combat_actors
	check(actors[1].present and actors[1].hp <= 0 and not actors[1].dead, "player mask-1 hit immediately lowers creature HP")
	await capture("combat-03-lethal-hit")
	scene.resume_session()
	scene.advance_step()
	actors = scene.raw_observation.combat_actors
	check(actors[1].present and actors[1].dead and actors[1].state == 20, "victim next own run queues death")
	await capture("combat-04-death-queued")
	scene.resume_session()
	scene.advance_step()
	actors = scene.raw_observation.combat_actors
	check(not actors[1].present and actors[2].present and not actors[2].dead, "next tick flush removes only dying Skelight")
	check(scene.raw_observation.combat_shared_seed == 1140036074, "exact integrated fight shared RNG")
	check(not scene.combat_displays[1].visible and scene.combat_displays[2].visible, "rendered creature removed from owned observation")
	await capture("combat-05-removed")
	scene.set_paused(true)
	before = controller.get_observation()
	for i in range(3): scene.advance_step()
	check(controller.get_observation() == before, "paused fight freezes native state")
	var fight_path := directory.path_join("combat-fight.ocplay")
	check(controller.save_file(fight_path).ok and FileAccess.file_exists(fight_path), "combat fixture continuation saved")
	check(controller.get_observation() == before, "saving retains the fight")
	var prepared: Dictionary = controller.prepare_save_file(fight_path)
	check(prepared.get("ok", false) and prepared.get("format", "") == "fixture-continuation-v1", "fixture continuation prepared")
	var loaded: Dictionary = controller.commit_terrain_file(prepared.get("token", -1))
	for field in ["ready_ticks", "combat_shared_seed", "hp_raw", "combat_actors", "paused"]:
		check(loaded.get(field) == before.get(field), "loaded fight keeps " + field)
	scene._observe(controller.get_observation())
	await capture("combat-06-paused-saved")
	print("COMBAT_FIGHT_RESULT ", JSON.stringify(before))
	scene.reset_session()
	check(scene.raw_observation.get("combat_enabled", false) and scene.raw_observation.ready_ticks == 0 and scene.raw_observation.combat_actors[1].present, "reset rebuilds combat fixture with single owner")
	# A valid older continuation explicitly replaces combat; never converts it.
	scene.combat_choice.button_pressed = false
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "load normal continuation atomically replaces combat")
	check(not scene.raw_observation.get("combat_enabled", false) and scene.save_methods == ["save_file"], "default bounded play and save capability restored")
	check(scene.controller == controller and scene.components.art.loaded.size() == 23, "same scene controller and HUD resources survive policy switch")
