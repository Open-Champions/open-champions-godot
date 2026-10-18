extends SceneTree

var failures := 0
func check(ok: bool, label: String) -> void:
	if not ok:
		failures += 1
		push_error("UI dynamic layout: " + label)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var service = ClassDB.instantiate("ChampionsUiDynamicLayoutService")
	check(service != null, "service registered")
	if service == null:
		quit(1)
		return
	check(service.draw_sequence()[0] == "gold" and service.draw_sequence()[9] == "net_status" and service.status_sequence()[10] == "spell selector", "draw order")
	check(service.painter_indices(PackedInt32Array([6,7,6]))["indices"] == PackedInt32Array([2,0,1]), "painter order")
	check(service.pulse(16) == 25, "pulse")
	var status: Dictionary = service.status_adjuncts(2, 32, 167, 124, 0, true, 0x80a0b0c1, 4, false, true, 20)
	check(status.get("ok", false) and status["name_colour"] == 0x80505860 and status["name_width_budget"] == 111 and status["glow_alpha"] == 128 and status["glow_sprite"]["x"] == 163, "status adjuncts")
	var notice: Dictionary = service.notice_frame(0, PackedInt32Array([5,0,1,0,100,0,0,0]), PackedInt32Array([0,0,0,0,0,0,0,0]), PackedInt32Array([-1,-1,-1,-1,3,-1,-1,-1]), 0, false, false)
	check(notice.get("ok", false) and notice["submissions"].size() == 4 and notice["timers"][0] == 4, "notice frame")
	check(service.count_active_notices(PackedInt32Array([1,1,0,0,1,0,0,0]), -1)["count"] == 2, "notice count")
	var message: Dictionary = service.message_frame(40, 10, false, false, 211)
	check(message.get("ok", false) and message["alpha"] == 13 and message["submissions"][1]["x"] == 150, "message frame")
	var boss: Dictionary = service.boss_meter_frame(50, 0, 0, 0, 0, 0)
	check(boss.get("ok", false) and boss["maximum"] == 50 and boss["submissions"][1]["x1"] == 412, "boss meter")
	var gold: Dictionary = service.gold_counter(3, 8, 30)
	check(gold.get("ok", false) and gold["submissions"][0]["x"] == 560, "gold counter")
	var gold_fade: Dictionary = service.gold_fade(110, 100, 60, 124)
	check(gold_fade.get("ok", false) and gold_fade["floater_alpha"] == 98 and gold_fade["alpha"] == 128, "gold fade")
	check(service.selector_direction(1, -1) == 1, "selector direction")
	var selector: Dictionary = service.selector_frame(1, 16, 128, 0, -1, true, false, false)
	check(selector.get("ok", false) and selector["x"] == 538 and selector["ring"].size() == 8, "selector")
	var scissor: Dictionary = service.text_scissor(10, 20, 30, 40)
	check(scissor.get("ok", false) and scissor["gs_right"] == 59 and scissor["bottom"] == 41, "text scissor")
	var fnt := PackedByteArray()
	fnt.resize(0x264)
	fnt.encode_u16(0, 1)
	fnt.encode_u32(8, 0x214)
	fnt.encode_u32(12, 0x224)
	fnt.encode_u32(16, 0x224)
	for i in range(256):
		fnt.encode_u16(0x14 + i * 2, 0xffff)
	fnt.encode_u16(0x14 + 45 * 2, 0)
	fnt.encode_u16(0x14 + 65 * 2, 0)
	fnt.encode_u16(0x214 + 12, 3)
	fnt.encode_u16(0x214 + 14, 0xffff)
	var text: Dictionary = service.text_layout(fnt, PackedInt32Array([65]), 100, 50, 1, true, 20, -1, 448, 17, false, 0, 100000, 0x80808080, -1, -1)
	check(text.get("ok", false) and text["width"] == 3 and text["glyphs"].size() == 0, "text layout")
	var fitted: Dictionary = service.fit_ascii(PackedByteArray(), PackedByteArray([65,66,67,68,69,70,71,72]), 6, true)
	check(fitted.get("ok", false) and fitted["ascii"] == PackedByteArray([65,66,67,46,46,46]), "text fit")
	print("UI dynamic layout GDExtension smoke: ", "PASS" if failures == 0 else "FAIL")
	quit(0 if failures == 0 else 1)
