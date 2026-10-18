extends RefCounted

# RTA US v2.00 metadata only. Source: champions-return-to-arms-re
# a2b145b6b22f3c0451f861ed70faa1827f94bf09,
# tools/godot/project/gameart/catalogue.gd, SHA-256
# db1e91dd7f19806ed727dd40c16ae8304313fcfdcf6e3510ae5100257fddbee5.
# rect is a crop in woven pixels. It is never a nine-slice margin.
const ENTRIES := {
	"bar_blue": {"lump":"HUD.LMP", "entry":"bar_blue10.tex", "page":[104,8], "rows":4, "rect":[0,0,104,8]},
	"bar_red": {"lump":"HUD.LMP", "entry":"bar_red10.tex", "page":[104,8], "rows":4, "rect":[0,0,104,8]},
	"button": {"lump":"INTERFAC.LMP", "entry":"button.tex", "page":[88,40], "rows":19, "rect":[0,0,88,38]},
	"frame_bot": {"lump":"INTERFAC.LMP", "entry":"tilebot.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_botleft": {"lump":"INTERFAC.LMP", "entry":"tilebotleft.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_botright": {"lump":"INTERFAC.LMP", "entry":"tilebotright.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_center": {"lump":"INTERFAC.LMP", "entry":"tilecenter.tex", "page":[320,224], "rows":112, "rect":[0,0,320,224]},
	"frame_left": {"lump":"INTERFAC.LMP", "entry":"tileleft.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_right": {"lump":"INTERFAC.LMP", "entry":"tileright.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_top": {"lump":"INTERFAC.LMP", "entry":"tiletop.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_topleft": {"lump":"INTERFAC.LMP", "entry":"tiletopleft.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"frame_topright": {"lump":"INTERFAC.LMP", "entry":"tiletopright.tex", "page":[32,32], "rows":16, "rect":[0,0,32,32]},
	"statusbar": {"lump":"HUD.LMP", "entry":"hud_statusbar00.tex", "page":[160,48], "rows":23, "rect":[0,0,160,46]},
	"statusbar_glow": {"lump":"HUD.LMP", "entry":"hud_statusbar00glow.tex", "page":[160,48], "rows":23, "rect":[0,0,160,46]},
}

static func matches_source(id: String, path: String, row: Dictionary) -> bool:
	if not ENTRIES.has(id):
		return false
	var meta: Dictionary = ENTRIES[id]
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
