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
	check(actor(0).has("locomotion_speed_raw") and actor(0).has("locomotion_blend_raw")
		and actor(0).has("weapon_animation_class") and actor(0).has("player_model_index"),
		"Full player publishes executed locomotion and record selection cells")
	check(scene.actions.combat_attack.visible and not scene.actions.combat_hit.visible, "Full pad buttons replace the round-1 hit event")
	check(scene.save_methods == ["save_file"], "the Full fixture advertises the fixture continuation")

	# (a) stick walk, attack/combo, creature takes the hits and dies; block.
	step()
	check(scene.raw_observation.ready_ticks == 0, "first submission has no ready tick")
	var x0: float = actor(0).position.x
	for i in range(40):
		step(1) # D: stick +x
		if i == 2:
			var blend_raw: int = int(actor(0).locomotion_blend_raw)
			check(blend_raw != 0 and blend_raw != 0x3f800000
				and int(actor(0).locomotion_walk_weight_raw) == blend_raw,
				"mode-0 walk weight equals the rising blend before saturation")
			var probe: Dictionary = scene.controller.probe_full_walk_weight_test()
			check(probe.get("ok", false) and int(probe.get("expected_weight_raw", 0)) == blend_raw
				and int(probe.get("observed_weight_raw", 0)) == blend_raw,
				"walk observation reads weight when synthetic life differs")
	check(actor(0).position.x > x0 + 40.0, "the stick walks the player along +x (executed Player_move)")
	check(int(actor(0).locomotion_speed_raw) != 0 and int(actor(0).locomotion_blend_raw) != 0
		and int(actor(0).locomotion_walk_weight_raw) != 0,
		"executed stick walk publishes nonzero speed and blend cells")
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
	# Review finding 12: while the game camera is refused nothing draws, so no
	# HolyStrike_draw glows may be queued (only a drawn frame clears the queue).
	var real_camera: Camera3D = scene.camera
	var broken := Camera3D.new()
	real_camera.get_parent().add_child(broken)
	broken.transform = Transform3D(Basis(Vector3(NAN, 0, 0), Vector3.UP, Vector3.BACK), Vector3.ZERO)
	scene.camera = broken
	var queued: int = fx.renderer.bridge.counts().glows
	for i in range(3): await process_frame
	check("game camera refused" in fx.refusals and fx.renderer.bridge.counts().glows == queued, "a refused game camera queues no draw-slot glows (%d -> %d)" % [queued, fx.renderer.bridge.counts().glows])
	scene.camera = real_camera
	broken.queue_free()
	fx.refusals.erase("game camera refused")
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
	var drawn_presentation: String = last_presentation
	var drawn_presents: int = last_presents
	# (d) review finding 2: saved mid-fight (Holy Strike cast on 170, in flight on
	# 174) through the scene's Save / Load, then continued: byte-identical.
	if "--diag-repeat" in OS.get_cmdline_user_args():
		var again: String = await full_trace(true)
		print("FULL_REPEAT_DIAG ", JSON.stringify({"trace_equal": again == full_trace_drawn, "first": drawn_presentation, "second": last_presentation}))
	var continued: String = await full_trace(true, 174)
	check(continued == full_trace_drawn, "mid-fight save/load (Holy Strike in flight): byte-identical seeds and health")
	# The bridge state is per machine: it advances once per drawn frame (beam drift,
	# row-4 flipbooks). It can match only when both runs drew the same frames; a
	# rendered window can drop one present under load (seen: 299 of 300).
	if last_presents == drawn_presents:
		check(last_presentation == drawn_presentation, "the continued run's per-machine effect world (bridge state) equals the uninterrupted one")
	else:
		print("FULL_SAVE_LOAD presentation not compared: ", drawn_presents, " vs ", last_presents, " drawn frames")
	print("FULL_SAVE_LOAD ", JSON.stringify({"trace_sha256": continued.sha256_text(), "presentation_uninterrupted": drawn_presentation, "presentation_continued": last_presentation, "presents": [drawn_presents, last_presents]}))
	print("FULL_TRACE ", JSON.stringify({"sha256": full_trace_drawn.sha256_text(), "bytes": full_trace_drawn.length(), "renderer": DisplayServer.get_name()}))
	check(fx.refusals.is_empty() or not "game camera refused" in fx.refusals, "game camera derived from the scene camera")
	print("PLAYABLE_FULL_COMBAT_RESULT ", JSON.stringify({"effects": scene.raw_observation.effects, "refusals": fx.refusals, "runs": fx.last_runs}))
	# Separate opt-in spell fixture after FULL_TRACE, so the established trace
	# continues to measure its original scenario without a spell selection.
	start_full()
	check(scene.actions.combat_spell_select.visible and scene.actions.combat_spell_cast.disabled,
		"Full fixture shows spell selection before casting")
	scene.actions.combat_spell_select.pressed.emit()
	check(scene.raw_observation.get("combat_spell_selected", false) and not scene.actions.combat_spell_cast.disabled,
		"spell button selects Disease Bolt in the live scene: %s / %s / %s" % [scene.raw_observation.get("combat_spell_selected", false), scene.actions.combat_spell_cast.disabled, scene.status_label.text])
	var spell_save := directory.path_join("full-spell-selected.ocplay")
	check(scene.save_session_file(spell_save), "selected spell fixture continuation saves")
	check(scene.load_session_file(spell_save), "selected spell fixture continuation loads")
	scene.resume_session()
	check(scene.raw_observation.get("combat_spell_selected", false) and not scene.actions.combat_spell_cast.disabled,
		"selected spell and cast control survive fixture continuation")
	if not output.is_empty(): await capture("full-spell-selected")
	var mana_before: float = scene.raw_observation.combat_mana
	var hp_before: float = actor(1).hp
	step() # lockstep's initial no-ready submission
	key(KEY_U, true)
	key(KEY_U, false)
	for i in range(3):
		step()
		check(actor(0).state != 5 and not tick_has("diseasebolt_spawn"),
			"released ability A does not admit a later cast")
	key(KEY_U, true)
	var spell_admitted := false
	var bolt_spawned := false
	var bolt_hit := false
	for i in range(50):
		step()
		spell_admitted = spell_admitted or actor(0).state == 5
		if tick_has("diseasebolt_spawn"):
			key(KEY_U, false)
		bolt_spawned = bolt_spawned or tick_has("diseasebolt_spawn")
		bolt_hit = bolt_hit or tick_has("diseasebolt_hit")
		if tick_has("diseasebolt_spawn"):
			check(scene.raw_observation.combat_mana < mana_before, "cast spends mana after animation admission")
		if bolt_hit: break
	check(spell_admitted, "held ability A enters documented cast state 5")
	check(bolt_spawned and bolt_hit and actor(1).hp < hp_before,
		"scene input spawns Disease Bolt and damages a Full-fixture enemy")
	check(scene.stopped and "unverified destructor" in scene.status_label.text,
		"projectile expiry stops the fixture with its explicit research refusal")
	print("PLAY_SPELLS_SCENE ", JSON.stringify({"selected": true, "spawned": bolt_spawned,
		"hit": bolt_hit, "mana_spent": scene.raw_observation.combat_mana < mana_before,
		"fixture_hp_lost": actor(1).hp < hp_before}))
	if not output.is_empty(): await capture("full-spell-hit")

	scene.full_choice.button_pressed = false
	scene.combat_choice.button_pressed = false
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "ordinary continuation replaces the Full fixture and its effects")
	check(not scene.raw_observation.get("combat_enabled", false), "ordinary play restored")

# One scripted scenario: turn and block, walk, combo, Holy Strike. The drawing
# run presents every tick (draw slots + GS runs); the other draws nothing.
var last_presentation := ""
var last_presents := 0
func full_trace(draw: bool, save_at := -1) -> String:
	start_full()
	var fx = scene.fx
	var attached: Dictionary = scene.controller.attach_effect_bridge(fx.renderer.bridge if draw else null)
	fx.set_process(draw)
	scene._observe(scene.controller.get_observation())
	check(attached.ok and scene.raw_observation.effects.drawing == draw, "effect world attached only for the drawing run")
	step()
	var trace := ""
	var presents := 0
	var seen_frame: int = fx.last_render_frame
	for t in range(300):
		if t == save_at:
			var path := directory.path_join("full-mid-fight.ocplay")
			var flying := false
			for name in scene.raw_observation.get("combat_tick_events", []): flying = flying or name == "presentation:particle_spawn"
			check(scene.save_session_file(path), "mid-fight fixture continuation saved")
			check(scene.load_session_file(path), "mid-fight fixture continuation loaded")
			scene.resume_session()
			fx = scene.fx
			fx.set_process(draw)
			check(scene.raw_observation.get("full_enabled", false) and scene.raw_observation.effects.drawing == draw, "the loaded session is the drawing Full fixture")
			check(flying, "the Holy Strike was in flight (puffs) when saved")
		var command := 0
		if t < 3: command = 2
		if t == 3: scene.controller.combat_block(true)
		if t == 50: scene.controller.combat_block(false)
		if t >= 52 and t < 90: command = 1
		if t >= 95 and t % 12 == 0: scene.controller.combat_attack(true)
		if t == 170: scene.controller.combat_cast_holy_strike()
		step(command)
		if draw: await process_frame
		if fx.last_render_frame != seen_frame:
			presents += 1
			seen_frame = fx.last_render_frame
		var o: Dictionary = scene.raw_observation
		trace += "%d %d %d" % [o.ready_ticks, o.combat_shared_seed, o.hp_raw]
		for row in o.combat_actors: trace += " %s:%d" % [row.present, row.hp_raw]
		trace += "\n"
	check(scene.raw_observation.effects.ticks == 300 and scene.raw_observation.effects.particle_spawns > 0, "EffectSimulation ran every ready tick (drawing: %s)" % draw)
	if draw:
		last_presentation = fx.renderer.bridge.save_state().hex_encode().sha256_text()
		last_presents = presents
		print("FULL_PRESENT_DIAG ", save_at, " ", JSON.stringify(fx.game_camera(scene.camera)), " vp ", scene.get_viewport().size, " presents ", presents, " counts ", fx.renderer.bridge.counts())
	fx.set_process(true)
	scene.controller.attach_effect_bridge(fx.renderer.bridge)
	return trace
