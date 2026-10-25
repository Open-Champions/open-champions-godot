extends SceneTree

var failures := 0
func verify(ok: bool, label: String) -> void:
	if not ok:
		failures += 1
		push_error("UI layout: " + label)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var service = ClassDB.instantiate("ChampionsUiLayoutService")
	verify(service != null, "service registered")
	if service == null:
		quit(1)
		return
	var frame: Dictionary = service.menu_frame(100, 80, 105, 100, 2, false, false)
	verify(frame.get("ok", false) and frame["draws"].size() > 5, "frame returned")
	if frame.get("ok", false):
		verify(frame["draws"][0]["resource"] == "tilebotleft.tex" and frame["draws"][0]["y"] == 147, "first submission")
		verify(frame["draws"][5]["clip"]["right"] == 177, "style two overlap")
	var source: Dictionary = service.frame_source_rect("tilecenter.tex")
	verify(source.get("ok", false) and source["rect"]["width"] == 320 and source["rect"]["height"] == 224, "source rectangle")
	verify(not service.frame_source_rect("unknown.tex").get("ok", true), "unknown resource refused")
	verify(service.hud_source_rect("bar_green00.tex").get("rect", {}).get("height", 0) == 6, "HUD source size")
	var origins: Dictionary = service.hud_origins(true, PackedInt32Array([2]), 4)
	verify(origins.get("ok", false) and origins["origins"] == PackedInt32Array([167, 478, 12, 323]), "local slot promotion")
	var fill: Dictionary = service.bar_clip_right(27, 1.0, 4.0)
	verify(fill.get("ok", false) and fill["right"] == 51, "PS2 fill")
	var hud: Dictionary = service.hud_base_layers(12, 44, 1.0, 4.0, 2.0, 4.0, 50, 0, 100, 1, true, 1, false)
	verify(hud.get("ok", false) and hud["layers"].size() == 4, "base layers")
	if hud.get("ok", false):
		verify(hud["layers"][0]["resource"] == "bar_red00.tex" and hud["layers"][3]["x"] == 8, "HUD draw data")
	var fnt := PackedByteArray()
	fnt.resize(0x264)
	fnt.encode_u16(0, 1)
	fnt.encode_u32(8, 0x214)
	fnt.encode_u32(12, 0x224)
	fnt.encode_u32(16, 0x224)
	for i in range(256):
		fnt.encode_u16(0x14 + i * 2, 0xffff)
	fnt.encode_u16(0x214 + 12, 3)
	fnt.encode_u16(0x214 + 14, 0xffff)
	var metrics: Dictionary = service.font_metrics(fnt, PackedByteArray([65]), true, 1, 0, 100, 50)
	verify(metrics.get("ok", false) and metrics["glyph_count"] == 1 and metrics["ascii_width"] == 3, "font width binding")
	verify(metrics.get("destination", {}).get("x", -1) == 100, "glyph rectangle binding")
	verify(not service.font_metrics(PackedByteArray(), PackedByteArray(), true, 0, -1, 0, 0).get("ok", true), "malformed FNT refused")
	verify(not service.font_header_line_height().get("ok", true), "FNT header +2 is refused")
	var arts: Dictionary = service.hud_art_round4()
	verify(arts.get("ok", false) and arts.entries.size() == 34, "thirty-four HUD art identities")
	var catalogue = load("res://ui/original_art/catalogue.gd").hud_entries()
	var matched := 0
	for art in arts.get("entries", []):
		for id in catalogue:
			if catalogue[id].entry == art.entry:
				matched += 1
				verify(catalogue[id].page == [art.used_width, art.used_height], "catalogue page matches used size")
				verify(catalogue[id].rect == [0, 0, art.used_width, art.used_height], "catalogue crop is the used origin")
	verify(matched == 34, "catalogue lists every round-4 identity")
	var page: Dictionary = service.decode_font_page(PackedByteArray([0]), false)
	verify(not page.get("ok", true), "short font page refused")
	var glyphs: Dictionary = service.hud_text_glyphs(fnt, PackedByteArray(), 73, 17, 0x80808080)
	verify(glyphs.get("ok", false) and glyphs.glyphs.is_empty(), "empty HUD text draws nothing")
	# Audit FONT-1: no default tint, and the HUD view does not guess one.
	# Two-glyph synthetic font: '3' and 'A' map to a drawn glyph 1.
	var hud_fnt := PackedByteArray()
	hud_fnt.resize(0x274)
	hud_fnt.encode_u16(0, 2)
	hud_fnt.encode_u32(8, 0x214)
	hud_fnt.encode_u32(12, 0x234)
	hud_fnt.encode_u32(16, 0x234)
	for i in range(256):
		hud_fnt.encode_u16(0x14 + i * 2, 0xffff)
	hud_fnt.encode_u16(0x14 + 45 * 2, 0)
	hud_fnt.encode_u16(0x14 + 0x33 * 2, 1)
	hud_fnt.encode_u16(0x14 + 0x41 * 2, 1)
	for glyph in [[0x214, 0], [0x224, 0x33]]:
		hud_fnt.encode_u16(glyph[0], glyph[1])
		hud_fnt.encode_u16(glyph[0] + 12, 3)
		hud_fnt.encode_u16(glyph[0] + 14, 0xffff)
	var explicit: Dictionary = service.hud_text_glyphs(hud_fnt, PackedByteArray([0x33]), 73, 17, 0x80323c3c)
	verify(explicit.get("ok", false) and explicit.glyphs.size() == 1 and explicit.glyphs[0].tint == 0x80323c3c, "HUD text uses the caller tint")
	var defaulted = service.callv("hud_text_glyphs", [hud_fnt, PackedByteArray([0x33]), 73, 17])
	verify(not (defaulted is Dictionary and defaulted.get("ok", false)), "FONT-1 HUD text tint is a required argument")
	var view: Control = load("res://ui/original_art/hud_view.gd").new()
	root.add_child(view)
	view.font_bytes = hud_fnt
	var observed := {"ready_ticks": 16, "hp": 1.0, "max_hp": 1.0, "name": "A", "level": 3}
	view.observe(observed)
	var refusals = view.get("text_refusals")
	verify(view.text_glyphs.is_empty() and refusals is Dictionary and refusals.has("name") and refusals.has("level"), "FONT-1 name refused; level refused without the acknowledged level")
	# hud-dynamic.md section 3: base colour 0x80323c3c; a pending level uses
	# R=G=p/2+60, B=p/2+50, alpha 0x80, with p = 25 at sim tick 16.
	for case in [[3, 0x80323c3c], [2, 0x803e4848]]:
		observed["acknowledged_level"] = case[0]
		view.observe(observed)
		var tinted: bool = not view.text_glyphs.is_empty()
		for glyph in view.text_glyphs:
			tinted = tinted and int(glyph.tint) == case[1]
		verify(tinted, "FONT-1 level colour %08x from hud-dynamic.md section 3" % case[1])
	view.queue_free()
	print("UI layout GDExtension smoke: ", "PASS" if failures == 0 else "FAIL")
	quit(0 if failures == 0 else 1)
