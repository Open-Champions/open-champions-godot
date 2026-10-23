extends SceneTree

# The play-view base HUD (godot/ui/play_hud.gd) on authored images only.
# Expected positions and clips are research-ui 3e39392 docs/hud-layout.md
# values for offline slot 0 (X = 12): health (27,46), mana (27,63), frame
# (8,36), fill_right = trunc(27 + C/M*99), pending health rows 47..51 in
# 0x80000060 and mana rows 66..70 in 0x805a4632.
var failures := 0
func check(ok: bool, label: String) -> void:
	if not ok:
		failures += 1
		push_error("play HUD: " + label)

func _initialize() -> void:
	call_deferred("_run")

func dict_with(base: Dictionary, over: Dictionary) -> Dictionary:
	var out := base.duplicate()
	out.merge(over, true)
	return out

func image(width: int, height: int, value: int) -> Image:
	var out := Image.create(width, height, false, Image.FORMAT_RGBA8)
	out.fill(Color8(value, 255 - value, 128, 255))
	return out

func _run() -> void:
	var hud: Control = load("res://ui/play_hud.gd").new()
	root.add_child(hud)
	hud.size = Vector2(1280, 720)
	check(hud.layout_service != null and hud.dynamic_service != null, "native layout services")
	var catalogue: Dictionary = load("res://ui/original_art/catalogue.gd").hud_entries()
	var art := {}
	for id in catalogue:
		var entry: Dictionary = catalogue[id]
		art[id] = {"image": image(int(entry.rect[2]), int(entry.rect[3]), 40 + art.size())}
	hud.set_art(art, catalogue)
	check(hud.textures.size() == 24, "22 bar frames, status frame and level-up glow from the catalogue")
	var value := {"hp": 50.0, "max_hp": 100.0, "mana": 30.0, "max_mana": 60.0, "pending_health": 20.0, "pending_mana": 10.0, "ready_ticks": 7}
	hud.observe_play(value, false)
	check(hud.records.is_empty() and hud.drawn.is_empty(), "nothing outside play")
	hud.observe_play(value, true)
	var by_element := {}
	for record in hud.records: by_element[hud._element(record)] = record
	check(hud.drawn.size() == 3, "health, mana and frame drawn without an observed dead bit: %s" % [hud.drawn])
	for element in ["health_bar", "mana_bar", "status_frame"]:
		check(element in hud.drawn, element + " drawn")
	check(hud.refusals.has("pending_health") and hud.refusals.has("pending_mana"), "both pending rectangles refuse a missing dead bit at positive HP")
	hud.observe_play(dict_with(value, {"dead": null}), true)
	check(not "pending_health" in hud.drawn and hud.refusals.has("pending_health"), "invalid dead-bit value also refuses pending")
	value["dead"] = false
	hud.observe_play(value, true)
	by_element.clear()
	for record in hud.records: by_element[hud._element(record)] = record
	var health: Dictionary = by_element.get("health_bar", {})
	check(health.get("x") == 27 and health.get("y") == 46 and health.get("resource") == "bar_red01.tex" and health.clip.right == 76, "health bar position, tick-7 frame and 50/100 fill")
	var mana: Dictionary = by_element.get("mana_bar", {})
	check(mana.get("y") == 63 and mana.get("tint") == 0x80acacac and mana.clip.right == 76, "mana bar position, tint and 30/60 fill")
	var frame: Dictionary = by_element.get("status_frame", {})
	check(frame.get("x") == 8 and frame.get("y") == 36 and frame.get("resource") == "hud_statusbar00.tex", "local status frame")
	var ph: Dictionary = by_element.get("pending_health", {})
	check(ph.get("left") == 76 and ph.get("right") == 96 and ph.get("top") == 47 and ph.get("bottom") == 51 and ph.get("colour") == 0x80000060, "pending health overlay (hud_pending.tsv row 1)")
	var pm: Dictionary = by_element.get("pending_mana", {})
	check(pm.get("left") == 76 and pm.get("right") == 92 and pm.get("top") == 66 and pm.get("bottom") == 70 and pm.get("colour") == 0x805a4632, "pending mana overlay (hud_pending.tsv row 1)")
	# Submission order health, pending health, pending mana, mana, frame; one
	# layer, so the painter reverses it and the frame is painted first.
	var painted: Array[String] = []
	for index in hud.painter_order: painted.append(hud._element(hud.records[index]))
	check(painted == ["status_frame", "mana_bar", "pending_mana", "pending_health", "health_bar"], "painter order %s" % [painted])
	var pending_level: Dictionary = value.duplicate()
	pending_level["level"] = 2
	pending_level["acknowledged_level"] = 1
	pending_level["player_object_present"] = true
	pending_level["player_object_dead"] = false
	hud.observe_play(pending_level, true)
	check("level_up_glow" in hud.drawn and hud.records.any(func(r): return r.get("resource", "") == "hud_statusbar00glow.tex" and r.get("x", -1) == 8 and r.get("y", -1) == 36), "pending level glow at the documented origin")
	check(hud.glow_alpha == 0, "observing alone does not advance the glow fade")
	hud.observe_play(pending_level, true)
	hud.observe_play(pending_level, true)
	check(hud.glow_alpha == 0, "repeated observations between draws do not speed the fade")
	hud.advance_glow()
	check(hud.glow_alpha == 4, "glow advances by 4 for one displayed draw")
	check(hud.records.any(func(r): return r.get("resource", "") == "hud_statusbar00glow.tex" and r.get("clipped", false) and r.get("clip", {}).get("left", -1) == 0 and r.get("clip", {}).get("right", -1) == 1000), "glow retains documented clip")
	var dead_level := pending_level.duplicate()
	dead_level["hp"] = 0.0
	hud.observe_play(dead_level, true)
	hud.advance_glow()
	check(not "level_up_glow" in hud.drawn and hud.glow_alpha == 4, "dead HP refuses glow without resetting fade")
	var unknown_level := pending_level.duplicate()
	unknown_level.erase("hp")
	hud.observe_play(unknown_level, true)
	hud.advance_glow()
	check(not "level_up_glow" in hud.drawn and hud.glow_alpha == 4, "an unobserved simulation HP refuses glow")
	var missing_object := pending_level.duplicate()
	missing_object.erase("player_object_present")
	hud.observe_play(missing_object, true)
	check(not "level_up_glow" in hud.drawn, "missing original player object refuses glow")
	hud.observe_play(pending_level, true)
	hud.advance_glow()
	check(hud.glow_alpha == 8, "pending glow resumes its prior alpha")
	# The record's own values never reach the bars: a record-only key is ignored.
	var record_only := value.duplicate()
	record_only.erase("hp")
	record_only["record_hp"] = 99.0
	hud.observe_play(record_only, true)
	check(not "health_bar" in hud.drawn, "record_hp is not drawn as a health bar")
	var opened_menu := pending_level.duplicate()
	opened_menu["level_menu_opened"] = true
	hud.observe_play(opened_menu, true)
	check(hud.glow_alpha == 0 and not "level_up_glow" in hud.drawn, "level menu opening resets and suppresses the glow")
	for key in ["experience", "level_text", "name_text", "level_up_glow", "portrait", "quick_slots", "gold", "minimap"]:
		if key != "level_up_glow": check(not str(hud.refusals.get(key, "")).is_empty(), key + " refused with a reason")
	for key in ["status_gates", "health_flash", "mana_cooldown", "pending_dead_bit"]:
		check(not str(hud.assumed.get(key, "")).is_empty(), key + " listed as an unobserved gate")
	# Health 0 with an explicitly clear dead bit still draws pending health.
	# Missing dead-bit timing at health 0 leaves the rectangle refused.
	hud.observe_play({"hp": 0.0, "max_hp": 100.0, "pending_health": 5.0, "ready_ticks": 44}, true)
	check(not "pending_health" in hud.drawn, "health 0 with no dead bit refuses pending health")
	hud.observe_play({"hp": 0.0, "max_hp": 100.0, "pending_health": 5.0, "dead": false, "ready_ticks": 44}, true)
	check(hud.drawn == ["status_frame", "pending_health", "health_bar"], "health 0, dead bit clear: pending health still drawn: %s" % [hud.drawn])
	check(hud.records[0].resource == "bar_red00.tex", "tick 44 is animation frame 0")
	check(not str(hud.refusals.get("mana_bar", "")).is_empty() and not hud.refusals.has("health_bar"), "unobserved mana refused, never fabricated")
	hud.observe_play({"hp": 0.0, "max_hp": 100.0, "pending_health": 5.0, "dead": true, "ready_ticks": 44}, true)
	check(hud.drawn == ["status_frame", "health_bar"], "dead bit set: no pending rectangle: %s" % [hud.drawn])
	hud.observe_play({"hp": 20.0, "max_hp": 100.0, "mana": 10.0, "max_mana": 50.0, "pending_mana": 3.0, "ready_ticks": 44}, true)
	check(not "pending_mana" in hud.drawn and not "pending_health" in hud.drawn and hud.refusals.has("pending_mana"), "positive HP does not supply a missing dead bit")
	hud.observe_play({"hp": 20.0, "max_hp": 100.0, "mana": 10.0, "max_mana": 50.0, "pending_mana": 3.0, "dead": false, "ready_ticks": 44}, true)
	check("pending_mana" in hud.drawn and not "pending_health" in hud.drawn, "observed clear dead bit admits only the pending mana rectangle")
	# The experience bar (hud-xp.md): drawn for acknowledged level > 0 with the
	# bar_green frame (tick >> 2) % 11, origin (27, 56), clip right
	# trunc(27 + (xp - lower) / (upper - lower) * 99).
	var xp_value := {"hp": 50.0, "max_hp": 100.0, "mana": 30.0, "max_mana": 60.0, "ready_ticks": 7, "acknowledged_level": 1, "experience": 550, "experience_base": 0, "experience_next": 1100}
	hud.observe_play(xp_value, true)
	check(not "experience" in hud.drawn and not str(hud.refusals.get("experience", "")).is_empty(), "no bar_green texture loaded: refused, not substituted")
	var green := {}
	for index in range(11): green["bar_green%02d.tex" % index] = image(100, 6, 90 + index)
	hud.set_extras_art(green)
	hud.observe_play(xp_value, true)
	var xp_record := {}
	for record in hud.records:
		if hud._element(record) == "experience": xp_record = record
	check(xp_record.get("resource") == "bar_green01.tex" and xp_record.get("x") == 27 and xp_record.get("y") == 56 and xp_record.clip.right == 76 and xp_record.clip.top == 24 and xp_record.clip.bottom == 124 and xp_record.tint == 0x80808080, "experience bar position, frame, tint and half fill")
	check("experience" in hud.drawn and not hud.refusals.has("experience"), "experience drawn")
	var order: Array[String] = []
	for record in hud.records: order.append(hud._element(record))
	check(order == ["health_bar", "experience", "mana_bar", "status_frame"], "experience bar sits between health and mana in submission order: %s" % [order])
	hud.observe_play(dict_with(xp_value, {"acknowledged_level": 0}), true)
	check(not "experience" in hud.drawn, "acknowledged level 0 draws no experience bar")
	hud.observe_play(dict_with(xp_value, {"experience": 0}), true)
	for record in hud.records:
		if hud._element(record) == "experience": xp_record = record
	check(xp_record.clip.right == 27, "a fresh character (ack 1, xp 0) has an empty bar")
	# The gold counter (hud-gold.md): a pickup credits after 43 ticks and the
	# counter fades per displayed frame. No font here, so only state is checked.
	check(hud.extras != null, "native HUD extras service")
	var credited := -1
	for tick in range(0, 60):
		var gold_value := {"hp": 50.0, "max_hp": 100.0, "ready_ticks": tick, "gold": 500, "gold_pickups": [{"slot": 0, "amount": 100, "x": 300, "y": 200}] if tick == 0 else []}
		hud.observe_play(gold_value, true)
		var state: Dictionary = hud.extras.gold_state()
		if credited < 0 and int(state.gold[0]) != 500: credited = tick
		if tick == 20:
			check(int(state.alpha[0]) > 0 and int(state.floaters_busy) == 1 and int(state.cosmetic_draws) == 1, "counter visible while the floater flies, one cosmetic draw owed")
			var during_floater: Dictionary = hud.extras.gold_sync(0, 777)
			check(not during_floater.get("ok", true) and str(during_floater.get("error", "")).contains("composite order"), "direct record change during a flying floater is refused")
	check(credited == 43, "gold arrives at tick 43, got %d" % credited)
	var final_state: Dictionary = hud.extras.gold_state()
	check(int(final_state.gold[0]) + int(final_state.pending[0]) == 600 and int(final_state.pending[0]) > 0, "the amount arrives in eighths: credited plus pending is the pickup")
	var busy_sync: Dictionary = hud.extras.gold_sync(0, 777)
	check(not busy_sync.get("ok", true) and str(busy_sync.get("error", "")).contains("composite order"), "independent record change during pending credit is refused")
	var gold_before_refusal := int(hud.extras.gold_state().gold[0])
	hud.observe_play({"hp": 50.0, "max_hp": 100.0, "ready_ticks": 60, "gold": 777}, true)
	check(hud.refusals.has("gold") and not "gold" in hud.drawn and int(hud.extras.gold_state().gold[0]) < 777 and int(hud.extras.gold_state().gold[0]) >= gold_before_refusal, "busy direct write cannot display a stale gold counter")
	for tick in range(60, 150):
		hud.observe_play({"hp": 50.0, "max_hp": 100.0, "ready_ticks": tick, "gold": 600}, true)
	hud.observe_play({"hp": 50.0, "max_hp": 100.0, "ready_ticks": 150, "gold": 777}, true)
	check(int(hud.extras.gold_state().gold[0]) == 777, "idle gold counter follows a direct record write")
	check(not str(hud.refusals.get("gold_coin", "")).is_empty() or not hud.drawn.has("gold"), "the coin is refused with its reason when the counter shows")
	check(str(hud.extras.format_gold(1234567, 0)) == "1,234,567" and str(hud.extras.format_gold(1234567, 1)) == "1.234.567", "separator by language byte")
	check(not hud.extras.refusal("minimap_tiles").ok and str(hud.extras.refusal("minimap_icons").error).contains("script_runByName"), "minimap tiles and icons refused with reasons")
	check(hud.extras.minimap_view({"world_texture": true, "free_units": 40000, "yaw": 0, "half_w": 1.0, "zoom": 1.0}).gate == false, "minimap gate: half width below 2 draws nothing")
	# Conditional play wiring consumes complete observed records. Unknown
	# native-session producers stay refused even when the arithmetic succeeds.
	var selector := {"local": true, "hp": 50.0,
		"gates": [false, false, false, false, false, false, false, false, false],
		"held": 0x2000, "pressed": 0x20, "direction": 0,
		"a": 0x20, "b": 0x10, "use": 0x80, "accept": 0x40}
	var potion := {"id": 1, "hp": 50.0, "max_hp": 100.0, "pending_hp": 0.0,
		"mana": 30.0, "max_mana": 60.0, "pending_mana": 0.0, "bag": [], "kind": 0}
	var projector := {"world_texture": true, "free_units": 40000, "yaw": 0,
		"centre_x": 0.0, "centre_y": 0.0, "origin_x": 0.0, "origin_y": 0.0,
		"offx": 0.0, "offy": 0.0, "zoom": 1.0, "half_w": 60.0,
		"half_h": 60.0, "width": 216, "height": 216}
	hud.observe_play(dict_with(value, {"hud_selector_input": selector, "hud_potion_use": potion, "hud_minimap": projector}), true)
	check(hud.quick_slot_state.get("ok", false) and hud.quick_slot_state.get("dx", 0) == 1 and hud.quick_slot_state.get("mask_frames", 0) == 3 and hud.quick_slot_state.get("message_id", -1) == 0, "selector raw-pad input reaches native gates, mask and message")
	var remote_selector: Dictionary = selector.duplicate()
	remote_selector.local = false
	check(not hud.extras.selector_input(remote_selector).get("readable", true), "nonlocal selector returns before raw-pad read")
	var dead_selector: Dictionary = selector.duplicate()
	dead_selector.hp = 0.0
	check(not hud.extras.selector_input(dead_selector).get("readable", true), "zero-current selector returns before raw-pad read")
	check(not hud.extras.selector_input(dict_with(selector, {"direction": 8})).get("ok", true), "selector refuses an undocumented direction")
	check(not hud.extras.selector_input(dict_with(selector, {"slot": 4})).get("ok", true), "selector refuses an undocumented slot")
	check(not hud.extras.potion_use(dict_with(potion, {"kind": 2})).get("ok", true), "potion use refuses count-only kind 2")
	check(not hud.extras.selector_input(dict_with(selector, {"direction": 4294967296})).get("ok", true), "selector refuses a direction before integer narrowing")
	check(not hud.extras.selector_input(dict_with(selector, {"slot": 4294967296})).get("ok", true), "selector refuses a slot before integer narrowing")
	check(not hud.extras.potion_use(dict_with(potion, {"kind": 4294967296})).get("ok", true), "potion use refuses a kind before integer narrowing")
	check(hud.potion_state.get("ok", false) and hud.potion_state.get("notices", []).size() == 1 and hud.potion_state.notices[0][0] == 1, "missing health potion starts line-1 notice")
	check(hud.minimap_projection.get("ok", false) and hud.minimap_projection.get("gate", false) and not str(hud.refusals.get("minimap_tiles", "")).is_empty() and not str(hud.refusals.get("minimap_icons", "")).is_empty(), "projector runs while Unknown tiles and icons stay refused")
	hud.observe_play(value, true)
	check(not str(hud.refusals.get("quick_slots", "")).is_empty() and not str(hud.refusals.get("minimap_projector", "")).is_empty(), "missing play producers are labelled")
	hud.observe_play(value, false)
	check(not hud.gold_seen, "leaving play resets the gold state")
	# A texture that was not loaded is refused, never drawn as a stand-in.
	var partial := art.duplicate()
	partial.erase("statusbar")
	hud.set_art(partial, catalogue)
	hud.observe_play(value, true)
	check(not "status_frame" in hud.drawn and str(hud.refusals.get("status_frame", "")).contains("hud_statusbar00.tex"), "missing frame texture refused")
	# The canvas is a centred 4:3 picture: 1280x720 shows 960x720 at x 160.
	await process_frame
	check(is_equal_approx(minf(hud.size.y, hud.size.x * 0.75) * 4.0 / 3.0, 960.0), "4:3 width inside a 16:9 view")
	hud.queue_free()
	print("play HUD smoke: %s" % ("passed" if failures == 0 else "%d failed" % failures))
	quit(1 if failures else 0)
