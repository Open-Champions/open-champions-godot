extends SceneTree

# Wave 12: the Full combat fixture extensions that reach merged executed
# bodies. One own-machine creature (creature-ai World::spawn_class) per run,
# and thrown potions 1..3 (combat-runtime-4 playerProjectile_run). Each case
# starts a fresh Full session on the default synthetic flat terrain with the
# labelled synthetic recipe constants, then runs 300 ready ticks with the
# player holding attack. A refusal is a result, not a failure: it is printed
# with its reason so the report can list what stays refused.
# Prints one PLAYABLE_WAVE12_RESULT JSON line.
const TICKS := 300
var checks := 0
var failures := 0

func _initialize() -> void:
	run.call_deferred()

func check(ok: bool, what: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAILED: " + what)

func fresh(controller: Object) -> Dictionary:
	var started: Dictionary = controller.start_play_session(true, false, false, true)
	if started.get("ok", true) == false: return started
	controller.set_paused(false)
	return started

func run_ticks(controller: Object, attack: bool) -> Dictionary:
	var result := {"ticks": 0, "error": "", "events": {}}
	var o: Dictionary = {}
	for t in range(TICKS):
		if attack and t % 12 == 0: controller.combat_attack(true)
		o = controller.advance(0)
		if o.get("failed", false) or o.get("stopped", false) or o.get("ok", true) == false:
			result.error = str(o.get("error", "stopped"))
			break
		result.ticks = int(o.get("ready_ticks", 0))
		for name in o.get("combat_tick_events", []):
			result.events[name] = int(result.events.get(name, 0)) + 1
	result["observation"] = o
	return result

func run() -> void:
	var controller: Object = ClassDB.instantiate("ChampionsSessionController")
	var constants: Dictionary = controller.set_effect_recipe_constants_json(preload("res://fx/playable_fx_fixture.gd").recipe_constants(), "synthetic recipe constants")
	check(constants.get("ok", false), "synthetic recipe constants load")
	var classes: PackedStringArray = ChampionsSessionController.own_creature_classes()
	check(classes.size() == 10, "ten own-machine classes are offered")
	var out := {"classes": {}, "potions": {}}
	for i in range(classes.size()):
		var row := {}
		var started := fresh(controller)
		if started.get("ok", true) == false:
			check(false, "Full session starts")
			continue
		var spawned: Dictionary = controller.combat_spawn_creature(i)
		row.spawn = "ok" if spawned.get("ok", true) != false else "refused: " + str(spawned.get("error", ""))
		if spawned.get("ok", true) != false:
			var actors: Array = spawned.get("combat_actors", [])
			check(actors.size() == 4 and str(actors[3].kind) == classes[i], "%s is observed in slot 3" % classes[i])
			var again: Dictionary = controller.combat_spawn_creature(i)
			check(again.get("ok", true) == false, "a second creature is refused")
			var ran := run_ticks(controller, true)
			row.ticks = ran.ticks
			row.error = ran.error
			var last: Dictionary = ran.observation
			var creature: Dictionary = last.get("combat_actors", [{}, {}, {}, {}])[3] if last.get("combat_actors", []).size() > 3 else {}
			row.present = creature.get("present", false)
			row.state = creature.get("state", -1)
		out.classes[classes[i]] = row
	for kind in [1, 2, 3]:
		var row := {}
		fresh(controller)
		var dropped: Dictionary = controller.combat_drop_potion(kind)
		row.drop = "ok" if dropped.get("ok", true) != false else "refused: " + str(dropped.get("error", ""))
		if dropped.get("ok", true) != false:
			var ran := run_ticks(controller, false)
			row.ticks = ran.ticks
			row.error = ran.error
			var notable := {}
			for name in ran.events:
				if name in ["LooseFire_ctor", "decal_add", "world_damageInRadius", "LightEffect_ctor", "SpellPoisonGas_ctor", "fx_spawn", "obj_delete"] or "Fire" in name or "Gas" in name or "decal" in name:
					notable[name] = ran.events[name]
			row.events = notable
		out.potions[str(kind)] = row
	# Observed on the merged runtime (a change here is a behaviour change).
	for name in ["Critter", "Gnaw", "GnomeNavigator", "Mermaid", "NPC", "Natasla", "Skelight"]:
		var row: Dictionary = out.classes.get(name, {})
		check(row.get("spawn", "") == "ok" and int(row.get("ticks", 0)) == TICKS - 1 and str(row.get("error", "x")).is_empty() and row.get("present", false), "%s runs %d ready ticks beside the fight" % [name, TICKS - 1])
	for refused in [["AntQueen", "aq_ground"], ["Skeleton", "slot 25 force"], ["Soul", "demon"]]:
		var row: Dictionary = out.classes.get(refused[0], {})
		check(str(row.get("error", "")).contains(refused[1]), "%s stops on its documented refusal (%s)" % refused)
	var p1: Dictionary = out.potions.get("1", {}).get("events", {})
	var p2: Dictionary = out.potions.get("2", {}).get("events", {})
	var p3: Dictionary = out.potions.get("3", {}).get("events", {})
	check(int(p1.get("LooseFire_ctor", 0)) == 5 and int(p1.get("presentation:decal_add", 0)) == 5, "potion 1: five LooseFire call-outs and five decals")
	check(int(p2.get("poisonGasSpell_ctor", 0)) == 1, "potion 2: one poison-gas spell")
	check(int(p3.get("LightEffect_ctor", 0)) == 1 and int(p3.get("presentation:decal_add", 0)) == 1, "potion 3: the ring's light and floor decal")
	for kind in ["1", "2", "3"]:
		check(str(out.potions[kind].get("error", "x")).is_empty() and int(out.potions[kind].get("ticks", 0)) == TICKS - 1, "potion %s session keeps running" % kind)
	check(controller.combat_drop_potion(4).get("ok", true) == false, "potion kind 4 is refused by the client")
	controller.start_play_session()
	check(controller.combat_spawn_creature(0).get("ok", true) == false, "creatures need the Full fixture")
	out["checks"] = checks
	out["failures"] = failures
	print("PLAYABLE_WAVE12_RESULT " + JSON.stringify(out))
	quit(0 if failures == 0 else 1)
