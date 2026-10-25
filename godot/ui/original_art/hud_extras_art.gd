extends RefCounted

# Art the HUD extras need beyond the 34 round-4 identities (research
# t3/research-hud-gaps f7c3964, docs/spec/hud-xp.md and hud-gold.md): the eleven
# bar_green00..10.tex frames (HUD.LMP, 100x6 used, PSMT4 128x8 surface, 8x2
# palette, 3 field rows, measured 11/11) and the gold coin gold03.tex. The coin's
# size is not documented in the pinned research, so it is not loaded: the
# counter text is drawn without the coin and the coin is refused with a reason.
# Same checks as original_art_panel.load_selection: exact lump, name, decoded
# size equal to the pinned page, native shape to the used rectangle.
const GREEN_PAGE := [100, 6]
const GREEN_ROWS := 3
const COIN_REFUSAL := "gold03.tex dimensions are not in the pinned research (hud-gold.md section 8 gives only its position), so the coin is not drawn"

static func load_green(inspector: Object, hud_lmp_path: String) -> Dictionary:
	var out := {"images": {}, "loaded": 0, "refused": 0, "reason": ""}
	if inspector == null or hud_lmp_path.is_empty():
		out.reason = "no inspector or HUD.LMP"
		return out
	var listing: Dictionary = inspector.inspect_ui_candidates(hud_lmp_path, "rta")
	if not listing.get("ok", false) or not listing.get("rows", null) is Array:
		out.reason = str(listing.get("error", "listing refused"))
		return out
	var rows := {}
	for row in listing.rows:
		if row is Dictionary: rows[str(row.get("name", "")).to_lower()] = row
	for frame in range(11):
		var name := "bar_green%02d.tex" % frame
		var row: Dictionary = rows.get(name, {})
		if row.is_empty() or int(row.get("width", -1)) != GREEN_PAGE[0] or int(row.get("height", -1)) != GREEN_PAGE[1] or int(row.get("psm", -1)) not in [0, 0x13, 0x14]:
			out.refused += 1
			continue
		var decoded: Dictionary = inspector.inspect_texture(hud_lmp_path, int(row.row_index), "rta", str(listing.fingerprint))
		if not decoded.get("ok", false) or int(decoded.get("width", -1)) != GREEN_PAGE[0] or int(decoded.get("height", -1)) != GREEN_PAGE[1]:
			out.refused += 1
			continue
		var shaped: Dictionary = inspector.shape_ui_art(int(decoded.width), int(decoded.height), decoded.rgba, GREEN_ROWS, [0, 0, GREEN_PAGE[0], GREEN_PAGE[1]])
		if not shaped.get("ok", false) or int(shaped.get("width", -1)) != GREEN_PAGE[0] or int(shaped.get("height", -1)) != GREEN_PAGE[1]:
			out.refused += 1
			continue
		out.images[name] = Image.create_from_data(int(shaped.width), int(shaped.height), false, Image.FORMAT_RGBA8, shaped.rgba)
		out.loaded += 1
	return out
