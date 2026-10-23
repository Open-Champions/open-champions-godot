extends "res://ui/text_hud.gd"

# Inventory quest page (page 4). The source of every entry is the live script
# host; parchment and font pixels are decoded from the selected disc at run
# time. Font measurement, wrapping and final pixel parity still need an
# original rendered capture (quest-log.md section 2).
const INK := 0x80323c3c
const TEXT_CLIP := Rect2(0, 119, 320, 321) # text_setClip(0, 119, 320, 440)
var art_ready := false
var entry_count := 0
var refusal := ""

func step_frame() -> void:
	items.clear()
	if not visible or controller == null: return
	var parchment: Dictionary = load_art("inventry.lmp", "parchment.tex", true)
	art_ready = not parchment.is_empty() and int(parchment.width) == 320 and int(parchment.height) == 350
	if not art_ready:
		refusal = "inventry.lmp/parchment.tex is unavailable or is not 320x350"
		queue_redraw()
		return
	refusal = ""
	add_item({"layer": 5, "kind": "sprite", "resource": "parchment.tex", "lump": "inventry.lmp",
		"x": 0, "y": -4, "clipped": true, "clip": {"left": 0, "top": 0, "right": 320, "bottom": 350},
		"tint": 0x80808080, "weave": true})
	var page: Dictionary = controller.quest_journal()
	if not page.get("ok", false):
		refusal = "quest state or localization is unavailable"
		queue_redraw()
		return
	entry_count = int(page.get("count", 0))
	var f := font()
	if f.is_empty():
		refusal = "gamefontmed.fnt is unavailable"
		queue_redraw()
		return
	if entry_count == 0:
		var empty: PackedInt32Array = page.get("empty", PackedInt32Array())
		add_item({"layer": 6, "kind": "text", "font": "gamefontmed.fnt", "glyphs": glyphs(f, empty, 24, 114, INK)})
	else:
		var y := 114
		for entry in page.get("entries", []):
			var title: PackedInt32Array = entry.get("title", PackedInt32Array())
			var body: PackedInt32Array = entry.get("body", PackedInt32Array())
			var heading := wrapped(f, title, 24, y, 288, 119, 440, 21, INK)
			add_item({"layer": 6, "kind": "text", "font": "gamefontmed.fnt", "glyphs": heading.glyphs})
			var body_y: int = y + maxi(0, int(heading.height))
			var detail := wrapped(f, body, 24, body_y, 288, 119, 440, 21, INK)
			add_item({"layer": 6, "kind": "text", "font": "gamefontmed.fnt", "glyphs": detail.glyphs})
			y = body_y + maxi(22, int(detail.height))
	queue_redraw()

func draw_glyphs(item: Dictionary) -> void:
	var f: Dictionary = fonts.get(item.font, {})
	if f.is_empty(): return
	var page: Vector2i = f.size
	for g in item.glyphs:
		var sw := int(g.source_width)
		var sh := int(g.source_height)
		var sx := int(g.source_x)
		var sy := int(g.source_y)
		if sw <= 0 or sh <= 0 or sx < 0 or sy < 0 or sx + sw > page.x or sy + sh > page.y: continue
		var original := Rect2(float(g.x_half) * 0.5, float(g.y), sw, sh)
		var dest := original.intersection(TEXT_CLIP)
		if not dest.has_area(): continue
		var source := Rect2(Vector2(sx, sy) + dest.position - original.position, dest.size)
		draw_texture_rect_region(f.texture, dest, source, colour_of(int(g.tint)), false)
