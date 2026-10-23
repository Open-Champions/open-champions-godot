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
	var round3 = ClassDB.instantiate("ChampionsUiHudRound3Service")
	check(round3 != null, "round3 service registered")
	if round3 != null:
		var params: Dictionary = round3.minimap_params(0, 1)
		check(params.get("ok", false) and params.words[0] == 0x3f666666, "minimap mode 0")
		check(round3.minimap_params(3, 1).get("ok", false) == false, "minimap mode refused")
		var blip: Dictionary = round3.minimap_player_blip(0)
		check(blip.get("ok", false) and blip.rgb == PackedInt32Array([182, 47, 58]), "player blip")
		check(round3.minimap_player_blip(4).get("drawn", true) == false, "other blip")
		var step: Dictionary = round3.flying_step(30, 128)
		check(step.get("ok", false) and step.life == 29 and step.alpha == 125 and step.erase == false, "flying step")
		var drawn: Dictionary = round3.flying_draw(1, true, 64, 9, 0, 0, 0, 0, 0x3f800000)
		check(drawn.get("ok", false) and drawn.x_off == 5 and drawn.stored_x_off == 9, "flying draw stays local")
		var present: Dictionary = round3.present_colour(128, 0, 255, 0x3f000000)
		check(present.get("ok", false) and present.colour == 0x00ff0099, "present brightness")
		var label: Dictionary = round3.compass_label()
		check(label.get("ok", false) and label.x == 65 and label.y == 394 and label.colour == 0x632f6159, "compass label")
		var bust: Dictionary = round3.bust_plan(0, 0x3f800000, 1, -1, 0, true, {"frame.tex": [64, 80], "<portrait>": [32, 32], "fieldnumber.tex": [32, 16]})
		check(bust.get("ok", false) and bust.calls.size() == 11 and bust.calls[0].name == "text_begin" and int(bust.calls[0].args[0]) == 0x4f7104, "bust draws the caption in gamefontsmall (word 0x4f7104)")
		var meter: Dictionary = round3.bust_plan(0, 0x3f800000, 1, 2, 0, true, {"frame.tex": [64, 80], "<portrait>": [32, 32], "bar_yellow00.tex": [8, 4]})
		check(meter.get("ok", false), "bust with a maximum is planned: one wrap, the meter sprites")
		var no_bar: Dictionary = round3.bust_plan(1, 0x3f800000, 1, 2, 0, false, {"frame.tex": [64, 80], "<portrait>": [32, 32]})
		check(no_bar.get("ok", false) and no_bar.calls.filter(func(c): return c.name == "sprite_clipped").is_empty() and int(no_bar.latch) == 0 and int(no_bar.fade) < 0x3f800000, "missing simulation frame omits the animated bar and the host latch fades the bust")
		check(round3.bust_plan(0, 0x3f800000, 1, 2, 0, true, {}).get("ok", true) == false, "a missing texture size refuses")
		check(round3.medal_prompt(true, true, true, true, true).get("shown", true) == false, "medal paused")
		check(round3.refused("minimap_projector").get("ok", false) == false, "projector refused")
		check(round3.refused("compass_mesh").get("ok", false) == false, "compass mesh refused")
		check(round3.refused("arena_point").get("ok", false) == false, "arena point refused")
		check(round3.refused("tex_flag").get("ok", false) == false, "tex flag refused")
		check(round3.refused("font_header").get("ok", false) == false, "font header refused")
		# Audit HUD-1: damage numbers come from damage_apply, never from an HP drop.
		check(not round3.has_method("hp_drop_text"), "HUD-1 no HP-drop damage-number trigger in the service")
		_check_round3_overlay()
	print("UI dynamic layout GDExtension smoke: ", "PASS" if failures == 0 else "FAIL")
	quit(0 if failures == 0 else 1)

# Audit HUD-1..HUD-5: the play overlay must not invent HUD elements. Feed it
# every input it used to turn into a drawing and require that nothing is kept
# for drawing and that each element names its refusal.
func _check_round3_overlay() -> void:
	var overlay: Control = load("res://ui/hud_round3.gd").new()
	root.add_child(overlay)
	var base := {"ready_ticks": 10, "hp_raw": 0x42c80000, "terrain_name": "practice ground",
		"hud_help": {"wrap_height": 40, "label_width": 60, "title": "T", "body": "B"},
		"hud_notice": {"word": 0, "wrap_height": 40},
		"hud_medal": {"armed": true, "mission_menu": true},
		"hud_net_cam": {"network": true, "other_slot": true, "name": "N", "prompt": "P"},
		"hud_bust": {"value": 1, "maximum": 1, "meter_width": 8, "text_alpha": 110, "sprite_alpha": 128, "body": "A", "value_text": "1"}}
	overlay.observe(base, true)
	var hit := base.duplicate(true)
	hit.ready_ticks = 11
	hit.hp_raw = 0x42b40000
	overlay.observe(hit, true)
	var kept := func(key: String) -> bool:
		var value = overlay.get(key)
		return value != null and not value.is_empty()
	check(not kept.call("numbers"), "HUD-1 no flying number spawned from the player's HP drop")
	var refusals = overlay.get("refusals")
	var has_refusal := func(key: String) -> bool:
		return refusals is Dictionary and not str(refusals.get(key, "")).is_empty()
	check(has_refusal.call("flying_numbers"), "HUD-1/HUD-2 flying numbers refused (no damage_apply event, no g_viewProj)")
	check(not kept.call("compass_text") and has_refusal.call("compass_label"), "HUD-3 compass label not drawn from the level name")
	check(not kept.call("blips") and has_refusal.call("minimap"), "HUD-4 no minimap swatch row")
	var boxes_clear := true
	for key in ["help", "notice", "medal", "net_cam", "bust"]:
		boxes_clear = boxes_clear and not kept.call(key) and has_refusal.call(key)
	check(boxes_clear, "HUD-5 help, notice, medal, net-cam and bust overlays refused")
	overlay.queue_free()
