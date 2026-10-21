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
	"experience": "the experience bar needs acknowledged level +0x58 > 0, experience +0x5c and the 0x41b554/0x41b558 rows; play observes no level or experience (hud-layout.md)",
	"level_text": "the level text is the signed byte at player +0x59 with the acknowledged level +0x58 for its colour; play observes neither (hud-layout.md, hud-dynamic.md section 3)",
	"name_text": "the name colour needs the arena colour 0x3054c4, player_slotColor, the blink mask 0x505b04 and ui_fitText, and play has no player name (hud-dynamic.md section 3)",
	"level_up_glow": "hud_statusbar00glow.tex is drawn only while a level is pending (acknowledged +0x58 < level +0x59); play observes no level (hud-dynamic.md section 3)",
	"portrait": "hud_drawPlayerStatus submits no portrait; the only HUD portrait is the bust panel 0x1dfb6c, shown by script showBust (0x4f66d4); with a level script, ui/text_hud.gd draws it (docs/STRINGS.md), not this view (hud-dynamic.md section 7)",
	"quick_slots": "the notice and count icons (icons.lmp counthealth/countmana/...) draw only with a nonzero timer 0x41b828 set by their starters, and the quick-cast selector 0x1dd270 opens only on the local D-pad with skill icons and the resref 0x86aa30; play supplies none (hud-dynamic.md sections 3 and 6)",
	"gold": "hud_drawGold 0x1dc708 draws only while the counter alpha 0x4f6950 > 0, which rises only after a gold floater from hud_tickGold; play credits no gold (hud-dynamic.md section 5)",
	"minimap": "the minimap projector 0x168cac is undocumented (hud-dynamic.md sections 9 and 10)",
	"boss_meter": "g_bossMeterHp 0x41b6a0 has no producer in play (hud-dynamic.md section 4)",
	"message": "hud_showMessage 0x1d92a0: only the level script's setInfo produces it in play, and ui/text_hud.gd draws that (docs/STRINGS.md); its other callers have no producer (hud-dynamic.md section 4)",
}
const ASSUMED := {
	"status_gates": "0x41b4f8, the HUD-suppressed flag 0x4f66a8, g_shopOpen 0x41b50c, camera +0x34 != 1, net_isSlotActive, a missing player object and vehicle +0x540 (hud-layout.md) are not observed; play is offline slot 0 on foot with no shop, so status is drawn",
	"health_flash": "bit 0 of 0x41b930 is ORed by Player_takeDamage (docs/issues/draw-sim-leaks.md); the session's damage is not that call, so the health tint stays 0x80808080",
	"mana_cooldown": "signed short player-object +0x4ae is not observed, so mana keeps tint 0x80acacac (alpha 42 only while it is > 0)",
	"pending_dead_bit": "the pending overlays need the object's dead bit 0x8 clear; it is not observed, so they are drawn only while health > 0",
}
const SLOT := 0
var play_mode := false
var refusals: Dictionary = {}
var assumed: Dictionary = {}
var drawn: Array[String] = []

func _ready() -> void:
	super._ready()
	custom_minimum_size = Vector2.ZERO
	mouse_filter = MOUSE_FILTER_IGNORE

func observe_play(value: Dictionary, playing: bool) -> void:
	play_mode = playing
	drawn.clear()
	refusals = {}
	assumed = {}
	if not playing:
		records.clear()
		painter_order = PackedInt32Array()
		queue_redraw()
		return
	observe(value)
	if error.is_empty(): _add_pending(value)
	refusals = REFUSALS.duplicate()
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
	queue_redraw()

func _element(record: Dictionary) -> String:
	if record.get("kind", "") == "rect": return str(record.element)
	var resource := str(record.resource)
	if resource.begins_with("bar_red"): return "health_bar"
	if resource.begins_with("bar_blue"): return "mana_bar"
	if resource.begins_with("bar_green"): return "experience"
	if resource == "hud_statusbar00glow.tex": return "level_up_glow"
	return "status_frame"

# Pending overlays follow the health bar in submission order (hud-dynamic.md
# section 1), so they are spliced in after it before the painter order is taken.
func _add_pending(value: Dictionary) -> void:
	if not valid_bar(value, "hp", "max_hp") or float(value.hp) <= 0.0: return
	var mana_ok := valid_bar(value, "mana", "max_mana")
	var pending_health := float(value.get("pending_health", 0.0)) if typeof(value.get("pending_health")) in [TYPE_INT, TYPE_FLOAT] else 0.0
	var pending_mana := float(value.get("pending_mana", 0.0)) if mana_ok and typeof(value.get("pending_mana")) in [TYPE_INT, TYPE_FLOAT] else 0.0
	if not (pending_health > 0.0 or pending_mana > 0.0): return
	var origins: Dictionary = layout_service.hud_origins(false, PackedInt32Array([SLOT]), 1)
	var result: Dictionary = layout_service.hud_pending_rects(int(origins.origins[SLOT]), SLOT, float(value.hp), float(value.max_hp), pending_health, float(value.mana) if mana_ok else 0.0, float(value.max_mana) if mana_ok else 1.0, pending_mana)
	if not result.get("ok", false):
		error = str(result.get("error", "pending overlay refused"))
		return
	var at := 1 if not records.is_empty() and str(records[0].resource).begins_with("bar_red") else 0
	for rect in result.rects:
		var entry: Dictionary = rect.duplicate()
		entry["kind"] = "rect"
		entry["element"] = "pending_health" if int(rect.top) == 47 else "pending_mana"
		records.insert(at, entry)
		at += 1
	var layers := PackedInt32Array()
	for record in records: layers.append(int(record.layer))
	var order: Dictionary = dynamic_service.painter_indices(layers)
	painter_order = order.get("indices", PackedInt32Array())
	if not order.get("ok", false): error = str(order.get("error", "HUD order refused."))

# Logical 640x448 to a 4:3 rectangle centred in the view (640x480 square pixels).
func _canvas_transform() -> void:
	var height := minf(size.y, size.x * 3.0 / 4.0)
	var width := height * 4.0 / 3.0
	var origin := Vector2((size.x - width) * 0.5, (size.y - height) * 0.5)
	draw_set_transform(origin, 0, Vector2(width / 640.0, height / 448.0))

func _draw() -> void:
	if not play_mode or size.x <= 0.0 or size.y <= 0.0: return
	super._draw()
