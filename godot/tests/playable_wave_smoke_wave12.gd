extends SceneTree

# Wave 12: the Full combat fixture extensions that reach merged executed
# bodies. One own-machine creature (creature-ai World::spawn_class) per run
# (Gnaw, Skelight, AntQueen, Skeleton and Soul build; the other five classes
# refuse), and thrown potions 1..3 (combat-runtime-4 playerProjectile_run with
# the executed LooseFire and poison gas constructors of research-combat-ctors,
# and since research-combat-ctors-2 their destructors, the fire entry and the
# gas run). Each case
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
			# The Full fixture's AntQueen, Skeleton and Soul occupy slots 4-6.
			check(actors.size() == 7 and str(actors[3].kind) == classes[i] and int(actors[3].slot) == 3 and str(actors[4].kind) == "AntQueen" and str(actors[5].kind) == "Skeleton" and str(actors[6].kind) == "Soul", "%s is observed in slot 3" % classes[i])
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
	# (research-combat-ctors e9777eb for the last three); research-combat-ctors-2
	# (5b92837) executes AntQueen_run in every state, Skeleton_isEnemy and the
	# take-damage slots. Placed at (0, 80) the Soul (team 0, a permanent charm)
	# wanders for the whole run. The other five classes refuse to spawn (audit
	# CAI-4) and leave the session untouched.
	for name in ["Gnaw", "Soul"]:
		var row: Dictionary = out.classes.get(name, {})
		check(row.get("spawn", "") == "ok" and int(row.get("ticks", 0)) == TICKS - 1 and str(row.get("error", "x")).is_empty() and row.get("present", false), "%s runs %d ready ticks beside the fight" % [name, TICKS - 1])
	# A Skelight at (0, 80) is nearer the fixture's Soul (team 0) than the
	# player and attacks it; Soul_takeDamage ignores an attacker that is not a
	# Demon (combat-ctors-2.md section 5), so the session runs on.
	var skelight: Dictionary = out.classes.get("Skelight", {})
	check(skelight.get("spawn", "") == "ok" and int(skelight.get("ticks", 0)) == TICKS - 1 and str(skelight.get("error", "x")).is_empty(), "Skelight builds and its hits on the Soul do nothing")
	# The AntQueen at (0, 80) bleeds when hit. GutChunk_ctor now builds the
	# chunk. Its rigid body now initializes, then the synthetic fixture refuses
	# at the missing loaded model hull record before collision integration.
	var queen: Dictionary = out.classes.get("AntQueen", {})
	check(queen.get("spawn", "") == "ok" and "model_getHullPoints returned null" in str(queen.get("error", "")), "AntQueen's blood chunk reaches its missing model hull")
	# Skeleton_run and the line-of-fire body are executed in round 3. The
	# spawned Skeleton continues for the whole authored 300-tick case.
	var skeleton: Dictionary = out.classes.get("Skeleton", {})
	check(skeleton.get("spawn", "") == "ok" and int(skeleton.get("ticks", 0)) == TICKS - 1 and str(skeleton.get("error", "x")).is_empty() and skeleton.get("present", false), "Skeleton builds and runs 299 ready ticks")
	for name in ["Critter", "GnomeNavigator", "Mermaid", "NPC", "Natasla"]:
		var row: Dictionary = out.classes.get(name, {})
		check("constructor is not executed" in str(row.get("spawn", "")), "%s spawn is refused" % name)
	var p1: Dictionary = out.potions.get("1", {}).get("events", {})
	var p2: Dictionary = out.potions.get("2", {}).get("events", {})
	var p3: Dictionary = out.potions.get("3", {}).get("events", {})
	# Potion 1 builds five LooseFire objects, each running fx_spawn id 5
	# (3 or 4 Fire objects); the fires and the LooseFire end through their
	# executed destructors. Potion 2's cloud runs poisonGasSpell_run 0x1fd0f0
	# (combat round 6) and ends through poisonGasSpell_dtor.
	check(str(out.potions["1"].get("error", "x")).is_empty() and int(out.potions["1"].get("ticks", 0)) == TICKS - 1 and int(p1.get("fx_spawn", 0)) == 5 and int(p1.get("presentation:glow_add", 0)) > 0, "potion 1 burns and the session runs on")
	check(str(out.potions["2"].get("error", "x")).is_empty() and int(out.potions["2"].get("ticks", 0)) == TICKS - 1, "potion 2's cloud runs and the session runs on")
	check(int(p3.get("LightEffect_ctor", 0)) == 1 and int(p3.get("presentation:decal_add", 0)) == 1, "potion 3: the ring's light and floor decal")
	check(str(out.potions["3"].get("error", "x")).is_empty() and int(out.potions["3"].get("ticks", 0)) == TICKS - 1, "potion 3 session keeps running")
	var mounted_start := fresh(controller)
	check(mounted_start.get("ok", true) != false, "mounted Full fixture starts")
	var mounted: Dictionary = controller.combat_spawn_mounted_horseman(0)
	check(mounted.get("ok", true) != false, "mounted Horsemen pair registers")
	var rider_seen := false
	var mount_seen := false
	for actor in mounted.get("combat_actors", []):
		rider_seen = rider_seen or (int(actor.get("slot", -1)) == 7 and str(actor.get("kind", "")) == "Horsemen")
		mount_seen = mount_seen or (int(actor.get("slot", -1)) == 8 and str(actor.get("kind", "")) == "Nightmare")
	check(rider_seen and mount_seen, "mounted actor identities reach Godot")
	check(controller.combat_spawn_mounted_horseman(0).get("ok", true) == false, "a second mounted pair is refused")
	controller.advance(0)
	var mounted_tick: Dictionary = controller.advance(0)
	check(mounted_tick.get("ok", true) != false and not mounted_tick.get("failed", false), "mounted pair completes a ready tick in Godot")
	check(controller.combat_drop_potion(4).get("ok", true) == false, "potion kind 4 is refused by the client")
	controller.start_play_session()
	check(controller.combat_spawn_creature(0).get("ok", true) == false, "creatures need the Full fixture")
	check(controller.combat_spawn_mounted_horseman(0).get("ok", true) == false, "mounted pair needs the Full fixture")
	out["checks"] = checks
	out["failures"] = failures
	print("PLAYABLE_WAVE12_RESULT " + JSON.stringify(out))
	quit(0 if failures == 0 else 1)
