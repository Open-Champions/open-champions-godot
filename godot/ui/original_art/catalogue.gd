extends RefCounted

# RTA US v2.00 metadata only. The entry list started from research
# a2b145b6b22f3c0451f861ed70faa1827f94bf09 catalogue.gd. Round 4
# (research 3e39392) sets page and rect to the TEX used size: bars 100x8,
# button 83x38, status frame and glow 160x46. Frame tiles are unchanged.
# rect is that used size at (0, 0) after field weaving. It is never a nine-slice margin.
const ENTRIES := {
	"bar_blue": {"lump":"HUD.LMP", "entry":"bar_blue10.tex", "page":[100,8], "rows":4, "rect":[0,0,100,8]},
	"bar_red": {"lump":"HUD.LMP", "entry":"bar_red10.tex", "page":[100,8], "rows":4, "rect":[0,0,100,8]},
	"button": {"lump":"INTERFAC.LMP", "entry":"button.tex", "page":[83,38], "rows":19, "rect":[0,0,83,38]},
	"frame_bot": {"lump":"INTERFAC.LMP", "entry":"tilebot.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_botleft": {"lump":"INTERFAC.LMP", "entry":"tilebotleft.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_botright": {"lump":"INTERFAC.LMP", "entry":"tilebotright.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_center": {"lump":"INTERFAC.LMP", "entry":"tilecenter.tex", "page":[320,224], "rows":112, "rect":[0,0,320,224]},
	"frame_left": {"lump":"INTERFAC.LMP", "entry":"tileleft.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_right": {"lump":"INTERFAC.LMP", "entry":"tileright.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_top": {"lump":"INTERFAC.LMP", "entry":"tiletop.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_topleft": {"lump":"INTERFAC.LMP", "entry":"tiletopleft.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_topright": {"lump":"INTERFAC.LMP", "entry":"tiletopright.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"statusbar": {"lump":"HUD.LMP", "entry":"hud_statusbar00.tex", "page":[160,46], "rows":23, "rect":[0,0,160,46]},
	"statusbar_glow": {"lump":"HUD.LMP", "entry":"hud_statusbar00glow.tex", "page":[160,46], "rows":23, "rect":[0,0,160,46]},
}

static func hud_entries() -> Dictionary:
	var entries := ENTRIES.duplicate(true)
	for colour in ["red", "blue"]:
		for frame in range(10):
			var id := "bar_%s%02d" % [colour, frame]
			entries[id] = {"lump":"HUD.LMP", "entry":id + ".tex", "page":[100,8], "rows":4, "rect":[0,0,100,8]}
	return entries

static func matches_source(id: String, path: String, row: Dictionary) -> bool:
	var entries := hud_entries()
	if not entries.has(id):
		return false
	var meta: Dictionary = entries[id]
	var parent := str(row.get("parent", ""))
	var container := parent.get_file() if not parent.is_empty() else path.get_file()
	# A loose TEX is explicitly chosen and named, but its archive provenance cannot
	# be verified from that file. The picker labels this as a user assertion.
	if path.get_extension().to_lower() == "tex":
		container = str(meta["lump"])
	return container.to_lower() == str(meta["lump"]).to_lower() \
		and str(row.get("name", "")).to_lower() == str(meta["entry"]).to_lower() \
		and int(row.get("width", -1)) == int(meta["page"][0]) \
		and int(row.get("height", -1)) == int(meta["page"][1]) \
		and row.get("kind", "") != "font"
