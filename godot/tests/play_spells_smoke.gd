extends SceneTree

func _initialize() -> void:
	var controller := ChampionsSessionController.new()
	var recipe: String = preload("res://fx/playable_fx_fixture.gd").recipe_constants()
	if not controller.set_effect_recipe_constants_json(recipe, "synthetic spell smoke").get("ok", false):
		_fail("effect recipe setup")
		return
	if not controller.start_play_session(true, false, false, true).get("ok", false):
		_fail("Full fixture startup")
		return
	var selected: Dictionary = controller.combat_select_disease_bolt()
	if not selected.get("ok", false) or not selected.get("combat_spell_selected", false):
		_fail("rank-1 selection")
		return
	var initial: int = int(selected.get("combat_mana_raw", -1))
	var hp_initial: int = int(selected.get("combat_actors", [])[1].get("hp_raw", -1))
	controller.advance(0) # first submission has no ready tick
	controller.combat_press_disease_bolt(true)
	var admitted := false
	var spawned := false
	var paid := false
	var hit := false
	var hp_lost := false
	for tick in range(50):
		var state: Dictionary = controller.advance(0)
		var events: Array = state.get("combat_tick_events", [])
		admitted = admitted or int(state.get("combat_actors", [])[0].get("state", -1)) == 5
		if "diseasebolt_spawn" in events:
			controller.combat_release_disease_bolt()
		spawned = spawned or "diseasebolt_spawn" in events
		paid = paid or int(state.get("combat_mana_raw", initial)) < initial
		hit = hit or "diseasebolt_hit" in events
		hp_lost = hp_lost or int(state.get("combat_actors", [])[1].get("hp_raw", hp_initial)) != hp_initial
		if hit: break
		if state.get("failed", false):
			_fail("simulation refused before projectile hit: " + str(state.get("error", "")))
			return
	if not (admitted and spawned and paid and hit and hp_lost):
		_fail("admission/mana/spawn/hit/damage: %s/%s/%s/%s/%s" % [admitted, spawned, paid, hit, hp_lost])
		return
	print("PLAY_SPELLS_SMOKE PASS: mana, spawn, fixture hit")
	quit(0)

func _fail(message: String) -> void:
	printerr("PLAY_SPELLS_SMOKE FAIL: " + message)
	quit(1)
