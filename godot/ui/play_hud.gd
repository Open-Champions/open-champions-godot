extends "res://ui/original_art/hud_view.gd"

# The base HUD over the play view. Evidence: research-ui 3e39392
# docs/hud-layout.md (base HUD resources, positions, clips, fills, tints and
# the pending potion overlays), docs/hud-dynamic.md (order and painter rule)
# and docs/formats/ui-fonts.md (the HUD reads the small-font pointer 0x4f7104,
# FONT.LMP gamefontsmall.fnt). hud_drawPlayerStatus 0x1d9cd4 for offline slot
# 0 submits the health bar, the pending health and mana rectangles, the mana
# bar and the local status frame; the native layout service computes all of
# them from the observation. Textures are the documented HUD.LMP entries read
# from the user's game folder at run time. The 640x448 canvas is shown as a
# 4:3 picture centred in the play view; widescreen anchoring is not original
# (hud-layout.md), so the sides stay empty on a wider view.
#
# Everything else the HUD task draws needs an input play does not supply; it
# is not drawn and `refusals` says why. `assumed` lists the documented gates
# that play does not observe and that the drawn elements take as clear.
const REFUSALS := {
	"experience": "the experience bar needs observed acknowledged level, experience, executable thresholds and all eleven bar_green frames (hud-xp.md)",
	"level_text": "the level text needs the observed signed player level, acknowledged level and the small HUD font (hud-dynamic.md section 3)",
	"name_text": "the name colour needs an observed arena/player colour and blink mask before ui_fitText can draw it (hud-dynamic.md section 3)",
	"level_up_glow": "hud_statusbar00glow.tex needs a pending level, an observed live player object and simulation HP > 0 (hud-dynamic.md section 3)",
	"portrait": "hud_drawPlayerStatus submits no portrait; the only HUD portrait is the bust panel 0x1dfb6c, shown by script showBust (0x4f66d4); with a level script, ui/text_hud.gd draws it (docs/STRINGS.md), not this view (hud-dynamic.md section 7)",
	"quick_slots": "the notice and count icons (icons.lmp counthealth/countmana/...) draw only with a nonzero timer 0x41b828 set by their starters, and the quick-cast selector 0x1dd270 opens only on the local D-pad with skill icons and the resref 0x86aa30; play supplies none (hud-dynamic.md sections 3 and 6)",
	"gold": "hud_drawGold draws only after a gold floater from Gold_onTouch or chest gold (observation key gold_pickups); direct record gold writers leave the counter hidden (hud-gold.md)",
	"gold_coin": "gold03.tex dimensions are not in the pinned research (hud-gold.md section 8 gives only its position), so the coin is not drawn",
	"gold_cosmetic_seed": "floater start owes a cosmetic seed draw, but sound-path draws are Unknown; the play HUD counts the debt and does not consume the shared seed (hud-gold.md)",
	"minimap": "the projector, scissor and blips are documented, but map tiles need the unexecuted VU1 program and icons call scripts from draw code; both remain refused (hud-minimap.md)",
	"minimap_tiles": "the VU1 tile program was read but not executed, and its numeric output is Unknown (hud-minimap.md)",
	"minimap_icons": "draw-time vars_get and script_runByName effects, icon writer and field +0x10 are Unknown (hud-minimap.md)",
	"other_notices": "ammunition, thrown and camera notice starters were not executed in the pinned research (hud-quickslots.md)",
	"boss_meter": "g_bossMeterHp 0x41b6a0 has no producer in play (hud-dynamic.md section 4)",
	"message": "hud_showMessage 0x1d92a0: only the level script's setInfo produces it in play, and ui/text_hud.gd draws that (docs/STRINGS.md); its other callers have no producer (hud-dynamic.md section 4)",
}
const ASSUMED := {
	"status_gates": "0x41b4f8, the HUD-suppressed flag 0x4f66a8, g_shopOpen 0x41b50c, camera +0x34 != 1, net_isSlotActive, a missing player object and vehicle +0x540 (hud-layout.md) are not observed; play is offline slot 0 on foot with no shop, so status is drawn",
	"health_flash": "bit 0 of 0x41b930 is ORed by Player_takeDamage (docs/issues/draw-sim-leaks.md); the session's damage is not that call, so the health tint stays 0x80808080",
	"gold_language": "the separator byte 0x44fddc is 0 here (a comma); its source in mainloop_runSession is not pinned for this build, only that 0 gives ',' and any other value '.' (hud-gold.md section 7)",
	"mana_cooldown": "signed short player-object +0x4ae is not observed, so mana keeps tint 0x80acacac (alpha 42 only while it is > 0)",
	"pending_dead_bit": "the pending overlays need the observed object dead bit 0x8 clear and a local slot (hud-status.md section 4); without that bit they are refused at every HP",
}
const SLOT := 0
var play_mode := false
var refusals: Dictionary = {}
var assumed: Dictionary = {}
var drawn: Array[String] = []
var extras: RefCounted
var gold_language := 0
var gold_seen := false
var quick_slot_state: Dictionary = {}
var potion_state: Dictionary = {}
var potion_event_seen := -1
var minimap_projection: Dictionary = {}

func _ready() -> void:
	super._ready()
	if ClassDB.class_exists("ChampionsHudExtrasService"):
		extras = ClassDB.instantiate("ChampionsHudExtrasService")
	custom_minimum_size = Vector2.ZERO
	mouse_filter = MOUSE_FILTER_IGNORE

func observe_play(value: Dictionary, playing: bool) -> void:
	play_mode = playing
	drawn.clear()
	refusals = {}
	assumed = {}
	if not playing:
		gold_seen = false
		potion_event_seen = -1
		quick_slot_state.clear()
		potion_state.clear()
		minimap_projection.clear()
		records.clear()
		glow_context = {}
		painter_order = PackedInt32Array()
		queue_redraw()
		return
	observe(value)
	if error.is_empty(): _add_status_extras(value)
	refusals = REFUSALS.duplicate()
	if typeof(value.get("dead")) != TYPE_BOOL:
		if _number(value, "pending_health") > 0.0:
			refusals["pending_health"] = "observed player-object dead bit 0x8 is absent or invalid"
		if _number(value, "pending_mana") > 0.0:
			refusals["pending_mana"] = "observed player-object dead bit 0x8 is absent or invalid"
	for key in text_refusals: refusals[key + "_text"] = text_refusals[key]
	if not error.is_empty(): refusals["status"] = error
	# hud_view omits a bar whose value or positive maximum is not observed.
	if not valid_bar(value, "hp", "max_hp"): refusals["health_bar"] = "play observes no health value with a positive maximum (player +0xac/+0xb0)"
	if not valid_bar(value, "mana", "max_mana"): refusals["mana_bar"] = "play observes no mana value with a positive maximum (player +0xc0/+0xc4)"
	assumed = ASSUMED.duplicate()
	for index in painter_order:
		var record: Dictionary = records[index]
		var element := _element(record)
		if record.get("kind", "") != "rect" and not textures.has(record.resource):
			refusals[element] = "documented texture %s not loaded from the game folder" % record.resource
		elif not element in drawn:
			drawn.append(element)
	for element in text_drawn:
		if not element in drawn: drawn.append(element)
	for element in drawn: refusals.erase(element)
	_observe_gold(value)
	_observe_quick_slots(value)
	_observe_minimap(value)
	queue_redraw()

func _element(record: Dictionary) -> String:
	if record.get("kind", "") == "rect": return str(record.element)
	var resource := str(record.resource)
	if resource.begins_with("bar_red"): return "health_bar"
	if resource.begins_with("bar_blue"): return "mana_bar"
	if resource.begins_with("bar_green"): return "experience"
	if resource == "hud_statusbar00glow.tex": return "level_up_glow"
	return "status_frame"

# The documented gap fillers (research t3/research-hud-gaps, hud-status.md and
# hud-xp.md), computed by native hud_extras. Pending rectangles follow the
# health bar in submission order (hud-dynamic.md section 1); the experience bar
# comes after them and before the mana bar. Rectangles need a local slot, the
# dead bit clear and their own pending value above zero: no health gate.
# Inverted rectangles are passed through; they cover no pixels.
func _add_status_extras(value: Dictionary) -> void:
	if extras == null or not valid_bar(value, "hp", "max_hp"): return
	# hud_view's older layout path also submits an XP bar after player-state
	# merged. Use the executed hud_extras draw once, with its exact clip.
	for i in range(records.size() - 1, -1, -1):
		if str(records[i].get("resource", "")).begins_with("bar_green"):
			records.remove_at(i)
	var mana_ok := valid_bar(value, "mana", "max_mana")
	var origins: Dictionary = layout_service.hud_origins(false, PackedInt32Array([SLOT]), 1)
	var request := {
		"slot": SLOT, "x": int(origins.origins[SLOT]), "local": true,
		"dead": typeof(value.get("dead")) != TYPE_BOOL or bool(value.dead),
		"health": float(value.hp), "max_health": float(value.max_hp),
		"pending_health": _number(value, "pending_health"),
		"mana": float(value.mana) if mana_ok else 0.0, "max_mana": float(value.max_mana) if mana_ok else 1.0,
		"pending_mana": _number(value, "pending_mana") if mana_ok else 0.0,
		"tick": int(value.get("ready_ticks", 0)), "ack_level": 0}
	var have_xp := true
	for key in ["acknowledged_level", "experience", "experience_base", "experience_next"]:
		if typeof(value.get(key)) != TYPE_INT: have_xp = false
	if have_xp:
		request.ack_level = int(value.acknowledged_level)
		request.exp = int(value.experience)
		request.exp_base = int(value.experience_base)
		request.exp_next = int(value.experience_next)
	var result: Dictionary = extras.status_draws(request)
	if not result.get("ok", false):
		error = str(result.get("error", "HUD status extras refused"))
		return
	var at := 1 if not records.is_empty() and str(records[0].resource).begins_with("bar_red") else 0
	for draw in result.draws:
		if draw.kind == "rect":
			var entry: Dictionary = draw.duplicate()
			entry["element"] = "pending_health" if int(draw.top) == 47 else "pending_mana"
			records.insert(at, entry)
			at += 1
		elif str(draw.resource).begins_with("bar_green"):
			var mana_index := -1
			for i in range(records.size()):
				if str(records[i].get("resource", "")).begins_with("bar_blue"): mana_index = i
			records.insert(mana_index if mana_index >= 0 else records.size(), draw.duplicate())
	var layers := PackedInt32Array()
	for record in records: layers.append(int(record.layer))
	var order: Dictionary = dynamic_service.painter_indices(layers)
	painter_order = order.get("indices", PackedInt32Array())
	if not order.get("ok", false): error = str(order.get("error", "HUD order refused."))

func _number(value: Dictionary, key: String) -> float:
	return float(value[key]) if typeof(value.get(key)) in [TYPE_INT, TYPE_FLOAT] and is_finite(float(value[key])) else 0.0

# Extra art outside the 34 identities: the eleven bar_green frames.
func set_extras_art(images: Dictionary) -> void:
	for name in images: textures[name] = ImageTexture.create_from_image(images[name])
	queue_redraw()

# The gold counter (hud-gold.md): floaters credit after 43 ticks, the counter
# fades per displayed frame. One displayed frame per observation (per tick); the
# console's frame/tick ratio is Unknown (hud-gold.md section 11). The floater
# start is taken at the given screen point with no drift (presentation only).
func configure_gold(language: int) -> void:
	gold_language = language

func _observe_gold(value: Dictionary) -> void:
	if extras == null: return
	var pickups = value.get("gold_pickups", [])
	if not pickups is Array: return
	if not gold_seen and pickups.is_empty() and typeof(value.get("gold")) != TYPE_INT: return
	if not gold_seen:
		gold_seen = true
		extras.gold_reset(PackedInt64Array([int(value.get("gold", 0)), 0, 0, 0]))
	var gold_refused := false
	if typeof(value.get("gold")) == TYPE_INT:
		var synced: Dictionary = extras.gold_sync(SLOT, int(value.gold))
		if not synced.get("ok", false):
			refusals["gold_record"] = str(synced.get("error", "record gold refused"))
			refusals["gold"] = refusals["gold_record"]
			gold_refused = true
	# Set the observation frame before a new floater captures its start frame.
	extras.gold_tick(int(value.get("ready_ticks", 0)))
	for pickup in pickups:
		if pickup is Dictionary and typeof(pickup.get("amount")) == TYPE_INT:
			var added: Dictionary = extras.gold_add(int(pickup.get("slot", SLOT)), int(pickup.amount), float(pickup.get("x", 0)), float(pickup.get("y", 0)))
			if not added.get("ok", false): refusals["gold_pickup"] = str(added.get("error", "gold floater refused"))
	var frame: Dictionary = extras.gold_draw(1, gold_language, font_bytes)
	if gold_refused: return
	if not frame.get("ok", false):
		refusals["gold"] = str(frame.get("error", "gold counter refused"))
		return
	if font_bytes.is_empty() or layout_service == null: return
	for floater in frame.floaters: _gold_text(str(floater.text), int(floater.x), int(floater.y), int(floater.colour))
	for counter in frame.counters: _gold_text(str(counter.text), int(counter.x), int(counter.y), int(counter.colour))
	if not frame.counters.is_empty():
		drawn.append("gold")
		refusals.erase("gold")
		refusals["gold_coin"] = load("res://ui/original_art/hud_extras_art.gd").COIN_REFUSAL

func _gold_text(text: String, x: int, y: int, colour: int) -> void:
	var glyphs: Dictionary = layout_service.hud_text_glyphs(font_bytes, text.to_utf8_buffer(), x, y, colour)
	if glyphs.get("ok", false): text_glyphs.append_array(glyphs.glyphs)

# These inputs are accepted only from an observed player/raw-pad and bag-2
# source. The current session does not expose those records. A host keyboard
# key is not substituted for the original configured button mask.
func _observe_quick_slots(value: Dictionary) -> void:
	if extras == null:
		refusals["quick_slots"] = "ChampionsHudExtrasService is unavailable"
		return
	if value.get("hud_selector_input") is Dictionary:
		quick_slot_state = extras.selector_input(value.hud_selector_input)
		if not quick_slot_state.get("ok", false):
			refusals["quick_slots"] = str(quick_slot_state.get("error", "selector input refused"))
		else:
			refusals["quick_slots"] = "selector input and message are computed, but the play session has no quick-slot handler or observed skill icon records"
	else:
		quick_slot_state.clear()
		refusals["quick_slots"] = "raw D-pad, nine gates, configured button masks and skill records are not observed in play (hud-quickslots.md)"
	var event = value.get("hud_potion_use", null)
	if event is Dictionary and typeof(event.get("id")) == TYPE_INT and int(event.id) != potion_event_seen:
		potion_state = extras.potion_use(event)
		if potion_state.get("ok", false): potion_event_seen = int(event.id)
		else: refusals["potion_notice"] = str(potion_state.get("error", "potion input refused"))
	if potion_state.get("ok", false) and not potion_state.get("notices", []).is_empty():
		refusals["potion_notice"] = "potion notice timers were computed, but icons.lmp notice art and a Player_usePotion producer are not bound to play"
	elif not event is Dictionary:
		refusals["potion_notice"] = "the play session has no bag-2 inventory or Player_usePotion event (hud-quickslots.md)"

# The projector consumes the local camera, grid and world automap texture gate.
# No player-position or WORLD-renderer substitute is used for those inputs.
func _observe_minimap(value: Dictionary) -> void:
	minimap_projection.clear()
	if extras == null:
		refusals["minimap"] = "ChampionsHudExtrasService is unavailable"
		return
	var input = value.get("hud_minimap", null)
	if not input is Dictionary:
		refusals["minimap_projector"] = "local camera yaw/position, automap grid, world texture and zoom are not observed in play (hud-minimap.md)"
		return
	for key in ["world_texture", "free_units", "yaw", "centre_x", "centre_y", "origin_x", "origin_y", "offx", "offy", "zoom", "half_w", "half_h", "width", "height"]:
		if not input.has(key):
			refusals["minimap_projector"] = "minimap projector input missing " + key
			return
	minimap_projection = extras.minimap_view(input)
	if not minimap_projection.get("ok", false):
		refusals["minimap_projector"] = str(minimap_projection.get("error", "projector refused"))
	elif minimap_projection.get("gate", false):
		refusals["minimap_tiles"] = str(extras.refusal("minimap_tiles").get("error", "VU1 tiles refused"))
		refusals["minimap_icons"] = str(extras.refusal("minimap_icons").get("error", "scripted icons refused"))

# Logical 640x448 to a 4:3 rectangle centred in the view (640x480 square pixels).
func _canvas_transform() -> void:
	var height := minf(size.y, size.x * 3.0 / 4.0)
	var width := height * 4.0 / 3.0
	var origin := Vector2((size.x - width) * 0.5, (size.y - height) * 0.5)
	draw_set_transform(origin, 0, Vector2(width / 640.0, height / 448.0))

func _draw() -> void:
	if not play_mode or size.x <= 0.0 or size.y <= 0.0: return
	super._draw()
