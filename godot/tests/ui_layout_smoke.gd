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
	print("UI layout GDExtension smoke: ", "PASS" if failures == 0 else "FAIL")
	quit(0 if failures == 0 else 1)
