extends "res://ui/text_hud.gd"

# Inventory page 4 is established as a read-only view of the script host's
# quest records. The parchment header is known, but its displayed crop and
# the font, text layout and empty-state placement are not (quest-log.md 2).
# Refuse presentation until those contracts are pinned; do not submit sprites
# or glyphs using host-selected layout values.
var art_ready := false
var entry_count := 0
var layout_refused := false
var refusal := ""

func step_frame() -> void:
	items.clear()
	art_ready = false
	entry_count = 0
	layout_refused = false
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
	layout_refused = true
	refusal = "journal layout refused: font, wrap, entry positions, empty-state position and parchment crop unresolved"
	queue_redraw()
