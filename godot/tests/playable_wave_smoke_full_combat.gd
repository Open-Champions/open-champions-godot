extends "res://tests/playable_wave_smoke_audio.gd"

# Wave 11: the opt-in Full combat fixture (combat-runtime-2 Composition::Full)
# with native-fx-2's EffectSimulation inside every ready tick, presented as the
# bridge's exact GS-order runs. Synthetic fixtures, clips, services and recipe
# constants only. Exact frame timing is also proven natively (playable-combat-fx).

# combat-spells.md 4.1: speed 450 (7.5 per 1/60 s frame); bolt and Skelight
# radius 12 (HolyStrike_ctor +0x1c, fixture spawn); Skelight 80 ahead.
const HOLY_STRIKE_FLIGHT := 8 # ceil((80 - 12 - 12) / 7.5)
var full_trace_headless := ""
var full_trace_drawn := ""

func actor(slot: int) -> Dictionary:
	return scene.raw_observation.combat_actors[slot]

func tick_has(name: String) -> bool:
	return name in scene.raw_observation.get("combat_tick_events", [])

func start_full() -> void:
	scene.return_to_selection()
	scene.level_choice.button_pressed = false
	scene.combat_choice.button_pressed = true
	scene.full_choice.button_pressed = true
	scene.actions.play.pressed.emit()
	scene.resume_session()
	scene.pending_damage = false
	scene.held_commands = 0

func key(code: int, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	check(scene.handle_input_event(event), "Full fixture consumes key %s" % OS.get_keycode_string(code))

func step(command := 0) -> void:
	scene.held_commands = command
	scene.pending_damage = false
	scene.advance_step()

func continuation_cases() -> void:
	await super.continuation_cases()
	var fx = scene.fx
	check(scene.play_capabilities.get("full_fixture_available", false) and fx != null, "synthetic recipe constants enable the Full fixture")
	if fx == null: return
	choose_level(directory.path_join("first.WORLD"))
	scene.combat_choice.button_pressed = false
	scene.animated_choice.button_pressed = false
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "ordinary continuation before the Full fixture")
	start_full()
	var o: Dictionary = scene.raw_observation
	check(o.get("full_enabled", false) and o.combat_policy == "rta-full-combat-fixture-v1", "explicit Full selection: versioned policy")
	check(fx.full and o.effects.drawing and actor(0).kind == "Player" and actor(1).kind == "Skelight", "scene bridge attached; player and Skelights")
	check(scene.actions.combat_attack.visible and not scene.actions.combat_hit.visible, "Full pad buttons replace the round-1 hit event")
	check(scene.save_methods.is_empty(), "Full fixture has no save format")

	# (a) stick walk, attack/combo, creature takes the hits and dies; block.
	step()
	check(scene.raw_observation.ready_ticks == 0, "first submission has no ready tick")
	var x0: float = actor(0).position.x
	for i in range(40): step(1) # D: stick +x
	check(actor(0).position.x > x0 + 40.0, "the stick walks the player along +x (executed Player_move)")
	await capture("full-01-walk")
	var hp0: float = actor(1).hp
	var hits := 0
	var removed_tick := -1
	var melee_states := 0
	for t in range(240):
		if t % 12 == 0: scene.actions.combat_attack.button_down.emit() # research press_attack: pressed and held
		step()
		if actor(0).get("action_state", -1) == 1: melee_states += 1
		if actor(1).present and tick_has("damage_result") and actor(1).hp < hp0: hits += 1
		if hits == 2 and t % 12 == 1: await capture("full-02-combo-hit")
		if not actor(1).present:
			removed_tick = scene.raw_observation.ready_ticks
			break
	scene.actions.combat_attack.button_up.emit()
	check(melee_states > 0, "attack presses enter the melee state (Player_run state 1)")
	check(hits >= 2, "the combo's swings hit the Skelight: %d" % hits)
	check(removed_tick > 0 and actor(2).present, "the killed Skelight is removed; the other remains")
	await capture("full-03-skelight-removed")
	scene.reset_session()
	scene.resume_session()
	check(scene.raw_observation.full_enabled and actor(1).present, "reset rebuilds the Full fixture")
	step()
	for i in range(3): step(2) # A: turn toward the Skelight behind
	key(KEY_K, true)
	var php: float = scene.raw_observation.hp
	var blocked := 0
	for t in range(60):
		step()
		if actor(0).action_state == 8 and tick_has("damage_result"): blocked += 1
		if blocked == 1 and t % 30 == 10: await capture("full-04-block")
	check(blocked >= 1 and scene.raw_observation.hp == php, "a held front block meets the Skelight's swing without losing health")
	check(scene.raw_observation.effects.unrouted.has("particle_spawn/6"), "block sparks have no definition in the trace: counted, never drawn as guesses")
	key(KEY_K, false)
	for t in range(120):
		step()
		if scene.raw_observation.hp < php: break
	check(actor(0).action_state != 8 and scene.raw_observation.hp < php, "released block: the same swing hurts")

	# (b) Holy Strike: effects spawn and the bolt damages on the documented tick.
	scene.reset_session()
	scene.resume_session()
	step()
	step()
	var shp: float = actor(1).hp
	var before: Dictionary = scene.raw_observation.effects
	key(KEY_L, true)
	key(KEY_L, false)
	step()
	check(tick_has("cast"), "Holy Strike is cast in the player's next run")
	var cast_tick: int = scene.raw_observation.ready_ticks
	var damage_tick := -1
	var captured := false
	for t in range(30):
		step()
		await process_frame # present: HolyStrike_draw slots and GS runs
		if not captured and t == 3:
			await capture("full-05-holy-strike-flight")
			captured = true
		if actor(1).hp < shp:
			damage_tick = scene.raw_observation.ready_ticks
			check(tick_has("light"), "the hit tick makes the LightEffect")
			break
	var effects: Dictionary = scene.raw_observation.effects
	print("FULL_HOLY_STRIKE ", JSON.stringify({"cast": cast_tick, "damage": damage_tick, "effects": effects, "bridge": fx.renderer.bridge.counts(), "runs": fx.last_runs}))
	check(damage_tick == cast_tick + HOLY_STRIKE_FLIGHT, "Holy Strike damages on the documented flight tick (cast + 8)")
	check(effects.particle_spawns > before.particle_spawns and fx.renderer.bridge.counts().particles > 0, "the bolt's own-generator puffs reach the drawn effect world")
	check(effects.glows > before.glows, "HolyStrike_draw makes its glow while the bolt flies")
	check(fx.last_runs > 0, "the renderer presents native GS-order runs")
	await process_frame
	await capture("full-06-holy-strike-hit")

	# (c) headless and drawing machines: byte-identical seeds and health.
	full_trace_headless = await full_trace(false)
	full_trace_drawn = await full_trace(true)
	check(not full_trace_headless.is_empty() and full_trace_headless == full_trace_drawn, "headless and drawing runs keep byte-identical seeds and health")
	print("FULL_TRACE ", JSON.stringify({"sha256": full_trace_drawn.sha256_text(), "bytes": full_trace_drawn.length(), "renderer": DisplayServer.get_name()}))
	check(fx.refusals.is_empty() or not "game camera refused" in fx.refusals, "game camera derived from the scene camera")
	print("PLAYABLE_FULL_COMBAT_RESULT ", JSON.stringify({"effects": scene.raw_observation.effects, "refusals": fx.refusals, "runs": fx.last_runs}))

	scene.full_choice.button_pressed = false
	scene.combat_choice.button_pressed = false
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "ordinary continuation replaces the Full fixture and its effects")
	check(not scene.raw_observation.get("combat_enabled", false), "ordinary play restored")

# One scripted scenario: turn and block, walk, combo, Holy Strike. The drawing
# run presents every tick (draw slots + GS runs); the other draws nothing.
func full_trace(draw: bool) -> String:
	start_full()
	var fx = scene.fx
	var attached: Dictionary = scene.controller.attach_effect_bridge(fx.renderer.bridge if draw else null)
	fx.set_process(draw)
	scene._observe(scene.controller.get_observation())
	check(attached.ok and scene.raw_observation.effects.drawing == draw, "effect world attached only for the drawing run")
	step()
	var trace := ""
	for t in range(300):
		var command := 0
		if t < 3: command = 2
		if t == 3: scene.controller.combat_block(true)
		if t == 50: scene.controller.combat_block(false)
		if t >= 52 and t < 90: command = 1
		if t >= 95 and t % 12 == 0: scene.controller.combat_attack(true)
		if t == 170: scene.controller.combat_cast_holy_strike()
		step(command)
		if draw: await process_frame
		var o: Dictionary = scene.raw_observation
		trace += "%d %d %d" % [o.ready_ticks, o.combat_shared_seed, o.hp_raw]
		for row in o.combat_actors: trace += " %s:%d" % [row.present, row.hp_raw]
		trace += "\n"
	check(scene.raw_observation.effects.ticks == 300 and scene.raw_observation.effects.particle_spawns > 0, "EffectSimulation ran every ready tick (drawing: %s)" % draw)
	fx.set_process(true)
	scene.controller.attach_effect_bridge(fx.renderer.bridge)
	return trace
