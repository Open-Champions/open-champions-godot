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
	# No pending overlays when dead-bit evidence is missing (health 0) or
	# nothing is pending; no mana when the observation has none.
	hud.observe_play({"hp": 0.0, "max_hp": 100.0, "pending_health": 5.0, "ready_ticks": 44}, true)
	check(hud.drawn == ["status_frame", "health_bar"], "health 0: empty bar, frame, no overlay, no mana: %s" % [hud.drawn])
	check(hud.records[0].resource == "bar_red00.tex", "tick 44 is animation frame 0")
	check(not str(hud.refusals.get("mana_bar", "")).is_empty() and not hud.refusals.has("health_bar"), "unobserved mana refused, never fabricated")
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
