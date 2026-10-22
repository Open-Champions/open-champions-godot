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
	check(hud.textures.size() == 23, "22 bar frames and the status frame from the catalogue")
	var value := {"hp": 50.0, "max_hp": 100.0, "mana": 30.0, "max_mana": 60.0, "pending_health": 20.0, "pending_mana": 10.0, "ready_ticks": 7}
	hud.observe_play(value, false)
	check(hud.records.is_empty() and hud.drawn.is_empty(), "nothing outside play")
	hud.observe_play(value, true)
	var by_element := {}
	for record in hud.records: by_element[hud._element(record)] = record
	check(hud.drawn.size() == 5, "health, pending health, pending mana, mana and frame drawn: %s" % [hud.drawn])
	for element in ["health_bar", "pending_health", "pending_mana", "mana_bar", "status_frame"]:
		check(element in hud.drawn, element + " drawn")
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
	for key in ["experience", "level_text", "name_text", "level_up_glow", "portrait", "quick_slots", "gold", "minimap"]:
		check(not str(hud.refusals.get(key, "")).is_empty(), key + " refused with a reason")
	for key in ["status_gates", "health_flash", "mana_cooldown", "pending_dead_bit"]:
		check(not str(hud.assumed.get(key, "")).is_empty(), key + " listed as an unobserved gate")
	# hud-status.md section 4: no health > 0 gate. Health 0 with the dead bit
	# clear still draws the pending health rectangle; the dead bit hides it.
	hud.observe_play({"hp": 0.0, "max_hp": 100.0, "pending_health": 5.0, "ready_ticks": 44}, true)
	check(hud.drawn == ["status_frame", "pending_health", "health_bar"], "health 0, dead bit clear: pending health still drawn: %s" % [hud.drawn])
	check(hud.records[0].resource == "bar_red00.tex", "tick 44 is animation frame 0")
	check(not str(hud.refusals.get("mana_bar", "")).is_empty() and not hud.refusals.has("health_bar"), "unobserved mana refused, never fabricated")
	hud.observe_play({"hp": 0.0, "max_hp": 100.0, "pending_health": 5.0, "dead": true, "ready_ticks": 44}, true)
	check(hud.drawn == ["status_frame", "health_bar"], "dead bit set: no pending rectangle: %s" % [hud.drawn])
	hud.observe_play({"hp": 20.0, "max_hp": 100.0, "mana": 10.0, "max_mana": 50.0, "pending_mana": 3.0, "ready_ticks": 44}, true)
	check("pending_mana" in hud.drawn and not "pending_health" in hud.drawn, "each rectangle needs its own pending value")
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
		if tick == 20: check(int(state.alpha[0]) > 0 and int(state.floaters_busy) == 1 and int(state.cosmetic_draws) == 1, "counter visible while the floater flies, one cosmetic draw owed")
	check(credited == 43, "gold arrives at tick 43, got %d" % credited)
	var final_state: Dictionary = hud.extras.gold_state()
	check(int(final_state.gold[0]) + int(final_state.pending[0]) == 600 and int(final_state.pending[0]) > 0, "the amount arrives in eighths: credited plus pending is the pickup")
	check(not str(hud.refusals.get("gold_coin", "")).is_empty() or not hud.drawn.has("gold"), "the coin is refused with its reason when the counter shows")
	check(str(hud.extras.format_gold(1234567, 0)) == "1,234,567" and str(hud.extras.format_gold(1234567, 1)) == "1.234.567", "separator by language byte")
	check(not hud.extras.refusal("minimap_tiles").ok and str(hud.extras.refusal("minimap_icons").error).contains("script_runByName"), "minimap tiles and icons refused with reasons")
	check(hud.extras.minimap_view({"world_texture": true, "free_units": 40000, "yaw": 0, "half_w": 1.0, "zoom": 1.0}).gate == false, "minimap gate: half width below 2 draws nothing")
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
