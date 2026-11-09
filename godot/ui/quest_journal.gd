extends "res://ui/text_hud.gd"

# Inventory page 4: a read-only view of the script host's quest records,
# research t3/research-quest-log 965ec52 docs/spec/quest-log.md sections 2
# and 6. The native page (quest_log::page_calls via quest_journal_calls)
# carries the original's text-call arguments. Only the empty page's calls are
# complete: text_begin(gamefontsmall, 6, 0x80323c3c, -1), the NoQuests lookup,
# text_drawWideCentered(160, 224, text, 1000000), text_end (quest-log.md 77,
# 263-265). They are interpreted like the other text plans; glyph placement is
# this client's centring of that call, not a measured pixel.
# Refused, and why:
const REFUSED_JOURNAL := {
	"parchment": "the parchment sprite's displayed crop and its third argument's role are Unknown (quest-log.md 74, 290): the 320x350 header is validated, nothing is submitted",
	"entries": "a loaded font's text_drawWrapped return is Unknown (quest-log.md 76, 288), so the y of every draw after the first title is not a value, and the stores inside 0x1802e0 are Unknown (75, 289)",
	"pixel_parity": "no retail frame was measured for this page (quest-log.md 82-84)",
}
const JOURNAL_FONT := "gamefontsmall.fnt"
var art_ready := false
var entry_count := 0
var layout_refused := false
var empty_drawn := false
var refusal := ""

func step_frame() -> void:
	items.clear()
	art_ready = false
	entry_count = 0
	layout_refused = false
	empty_drawn = false
	refusal = ""
	if not visible or controller == null:
		queue_redraw()
		return
	# Header validation only. No weave, crop, tint or sprite submission follows.
	var parchment: Dictionary = load_art("inventry.lmp", "parchment.tex", false)
	art_ready = not parchment.is_empty() and int(parchment.width) == 320 and int(parchment.height) == 350
	if not art_ready:
		refusal = "journal parchment header unavailable or not 320x350"
		queue_redraw()
		return
	var page: Dictionary = controller.quest_journal()
	if not page.get("ok", false):
		refusal = "journal quest state or localization unavailable"
		queue_redraw()
		return
	entry_count = int(page.get("count", 0))
	if entry_count > 0:
		layout_refused = true
		refusal = "journal layout refused: " + str(REFUSED_JOURNAL.entries) + "; " + str(REFUSED_JOURNAL.parchment)
		queue_redraw()
		return
	var plan: Dictionary = controller.quest_journal_calls() if controller.has_method("quest_journal_calls") else {}
	if not plan.get("ok", false) or not plan.get("empty", false) or font(JOURNAL_FONT).is_empty() or dynamic_service == null:
		layout_refused = true
		refusal = "journal layout refused: empty-page calls or the gamefontsmall font unavailable"
		queue_redraw()
		return
	# Pointer stand-in 1 is the first lookup, the NoQuests key.
	interpret(plan.calls, {1: page.get("empty", PackedInt32Array())})
	empty_drawn = true
	queue_redraw()
