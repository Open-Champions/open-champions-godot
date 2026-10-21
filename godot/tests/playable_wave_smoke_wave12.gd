extends SceneTree

# Wave 12: the Full combat fixture extensions that reach merged executed
# bodies. One own-machine creature (creature-ai World::spawn_class) per run
# (Gnaw, Skelight, AntQueen, Skeleton and Soul build; the other five classes
# refuse), and thrown potions 1..3 (combat-runtime-4 playerProjectile_run with
# the executed LooseFire and poison gas constructors of research-combat-ctors;
# 1 and 2 stop later, at a body that is still not read). Each case
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
		# Holy Shield's absorb light follows its particleGroup_end (with only the
		# light's pool_new between); count it apart so the potions' own lights
		# stay comparable.
		var previous := ""
		for name in o.get("combat_tick_events", []):
			var key: String = "HolyShield:" + name if name == "LightEffect_ctor" and previous == "particleGroup_end" else name
			result.events[key] = int(result.events.get(key, 0)) + 1
			if name != "pool_new": previous = name
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
			# The Full fixture's Soul (slot 6) follows the creature.
			check(actors.size() == 5 and str(actors[3].kind) == classes[i] and int(actors[3].slot) == 3 and str(actors[4].kind) == "Soul", "%s is observed in slot 3" % classes[i])
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
				if name in ["LooseFire_ctor", "decal_add", "world_damageInRadius", "LightEffect_ctor", "SpellPoisonGas_ctor", "fx_spawn", "obj_delete", "shared_draw"] or "Fire" in name or "Gas" in name or "decal" in name or "glow" in name:
					notable[name] = ran.events[name]
			row.events = notable
		out.potions[str(kind)] = row
	# Observed on the merged runtime (a change here is a behaviour change).
	# Skelight, Gnaw, AntQueen, Skeleton and Soul have executed constructors
	# (research-combat-ctors e9777eb for the last three). Placed at (0, 80) the
	# Soul (team 0, a permanent charm) wanders for the whole run; the Skeleton's
	# first target scan calls Skeleton_isEnemy and the AntQueen sees the player
	# and asks for state 3, both still refused. The other five classes refuse
	# to spawn (audit CAI-4) and leave the session untouched.
	for name in ["Gnaw", "Soul"]:
		var row: Dictionary = out.classes.get(name, {})
		check(row.get("spawn", "") == "ok" and int(row.get("ticks", 0)) == TICKS - 1 and str(row.get("error", "x")).is_empty() and row.get("present", false), "%s runs %d ready ticks beside the fight" % [name, TICKS - 1])
	# A Skelight at (0, 80) is nearer the fixture's Soul (team 0, an enemy of
	# the team-less monsters) than the player, attacks it, and the hit stops at
	# Soul_takeDamage, a recording stub of the research.
	var skelight: Dictionary = out.classes.get("Skelight", {})
	check(skelight.get("spawn", "") == "ok" and "Soul_takeDamage 0x2aa908" in str(skelight.get("error", "")), "Skelight builds, then its hit on the Soul stops at Soul_takeDamage")
	# The AntQueen at (0, 80) is hit by the attacking player first; she bleeds,
	# and the blood call GutChunk_ctor has no World handler (as for a Gnaw).
	var queen: Dictionary = out.classes.get("AntQueen", {})
	check(queen.get("spawn", "") == "ok" and "GutChunk_ctor" in str(queen.get("error", "")), "AntQueen builds, then the player's hit stops at GutChunk_ctor")
	var skeleton: Dictionary = out.classes.get("Skeleton", {})
	check(skeleton.get("spawn", "") == "ok" and "Skeleton_isEnemy 0x2a7e04" in str(skeleton.get("error", "")), "Skeleton builds, then stops at Skeleton_isEnemy")
	for name in ["Critter", "GnomeNavigator", "Mermaid", "NPC", "Natasla"]:
		var row: Dictionary = out.classes.get(name, {})
		check("constructor is not executed" in str(row.get("spawn", "")), "%s spawn is refused" % name)
	var p1: Dictionary = out.potions.get("1", {}).get("events", {})
	var p2: Dictionary = out.potions.get("2", {}).get("events", {})
	var p3: Dictionary = out.potions.get("3", {}).get("events", {})
	# Potion 1 builds five LooseFire objects, each running fx_spawn id 5
	# (3 or 4 Fire objects); the first Fire to end reaches Fire_dtor 0x3c5050,
	# whose body was not read. Potion 2's cloud runs poisonGasSpell_run 0x1fd0f0
	# (combat round 6, not ported) on the next frame.
	check(str(out.potions["1"].get("error", "")).contains("Fire_dtor 0x3c5050") and int(p1.get("fx_spawn", 0)) == 5 and int(p1.get("presentation:glow_add", 0)) > 0, "potion 1 burns, then stops at Fire_dtor")
	check(str(out.potions["2"].get("error", "")).contains("poisonGasSpell_run 0x1fd0f0"), "potion 2 builds its cloud, then stops at the gas run")
	check(int(p3.get("LightEffect_ctor", 0)) == 1 and int(p3.get("presentation:decal_add", 0)) == 1, "potion 3: the ring's light and floor decal")
	check(str(out.potions["3"].get("error", "x")).is_empty() and int(out.potions["3"].get("ticks", 0)) == TICKS - 1, "potion 3 session keeps running")
	check(controller.combat_drop_potion(4).get("ok", true) == false, "potion kind 4 is refused by the client")
	controller.start_play_session()
	check(controller.combat_spawn_creature(0).get("ok", true) == false, "creatures need the Full fixture")
	out["checks"] = checks
	out["failures"] = failures
	print("PLAYABLE_WAVE12_RESULT " + JSON.stringify(out))
	quit(0 if failures == 0 else 1)
