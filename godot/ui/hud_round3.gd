extends Control

# Play-scene overlay for the round-3 HUD contracts. Positions and colours come
# from ChampionsUiHudRound3Service. The minimap projector, compass mesh and
# arena hermite point stay refused, so this draws no projected blips, no
# compass model and no arena step string.
# Flying numbers are refused (audit HUD-1/HUD-2): damage_apply creates FlyingText
# only for non-player victims, with the damage number, crit, kind and a
# shared-seed constructor draw (research-ui 1ed740a docs/gameplay.md step 30,
# docs/hud-dynamic.md section 7). The combat runtime refuses that 0x98
# allocation, and play supplies no g_viewProj 0x44f140 or world anchor.
const FLYING_NUMBERS_REFUSAL := "FlyingText comes only from damage_apply; the combat runtime refuses that allocation and play supplies no g_viewProj 0x44f140 or world anchor"
var service: Object
var play_mode := false
var compass_text := ""
var compass: Dictionary = {}
var blips: Array = []
var refusals: Dictionary = {}
var help: Dictionary = {}
var notice: Dictionary = {}
var medal: Dictionary = {}
var net_cam: Dictionary = {}
var bust: Dictionary = {}

func _ready() -> void:
	mouse_filter = MOUSE_FILTER_IGNORE
	if ClassDB.class_exists("ChampionsUiHudRound3Service"):
		service = ClassDB.instantiate("ChampionsUiHudRound3Service")
	queue_redraw()

func observe(observation: Dictionary, playing: bool) -> void:
	play_mode = playing
	compass_text = ""
	blips.clear()
	help = {}
	notice = {}
	medal = {}
	net_cam = {}
	bust = {}
	refusals = {}
	if service == null or not playing:
		queue_redraw()
		return
	refusals["flying_numbers"] = FLYING_NUMBERS_REFUSAL
	if observation.get("terrain_name", "") is String and not str(observation.terrain_name).is_empty():
		compass = service.compass_label()
		if compass.get("ok", false):
			compass_text = str(observation.terrain_name)
	for kind in range(4):
		var blip: Dictionary = service.minimap_player_blip(kind)
		if blip.get("ok", false) and blip.get("drawn", false):
			blips.append(blip.rgb)
	var gold: Dictionary = service.minimap_object_blip(0x20, 1 << 11, 0)
	var grey: Dictionary = service.minimap_object_blip(0, 0, 0)
	if gold.get("ok", false):
		blips.append(gold.rgb)
	if grey.get("ok", false):
		blips.append(grey.rgb)
	if observation.get("hud_help") is Dictionary:
		var spec: Dictionary = observation.hud_help
		help = service.help_box(int(spec.get("wrap_height", 0)), int(spec.get("label_width", 0)))
		help["title"] = str(spec.get("title", ""))
		help["body"] = str(spec.get("body", ""))
	if observation.get("hud_notice") is Dictionary:
		var spec: Dictionary = observation.hud_notice
		notice = service.notice_box(int(spec.get("word", 0)), int(spec.get("wrap_height", 0)))
	if observation.get("hud_medal") is Dictionary:
		var spec: Dictionary = observation.hud_medal
		medal = service.medal_prompt(bool(spec.get("pause_menu", false)), bool(spec.get("armed", false)), bool(spec.get("mission_menu", false)), bool(spec.get("network", false)), bool(spec.get("first_active", false)))
		medal["title"] = str(medal.get("title_key", ""))
		medal["body"] = str(medal.get("body_key", ""))
	if observation.get("hud_net_cam") is Dictionary:
		var spec: Dictionary = observation.hud_net_cam
		net_cam = service.net_cam(bool(spec.get("network", false)), bool(spec.get("other_slot", false)), bool(spec.get("health_zero", false)), bool(spec.get("has_object", false)), int(spec.get("countdown", 0)), int(spec.get("alive", 0)))
		net_cam["name"] = str(spec.get("name", ""))
		net_cam["prompt_text"] = str(spec.get("prompt", ""))
	if observation.get("hud_bust") is Dictionary:
		var spec: Dictionary = observation.hud_bust
		bust = service.bust_panel(bool(spec.get("has_maximum", false)), int(spec.get("value", 0)), int(spec.get("maximum", 1)), int(spec.get("meter_width", 1)), int(spec.get("tick", 0)), int(spec.get("text_alpha", 0)), int(spec.get("sprite_alpha", 0)), int(spec.get("portrait_w", 0)), int(spec.get("portrait_h", 0)), int(spec.get("frame_w", 0)), int(spec.get("frame_h", 0)), str(spec.get("body", "")), str(spec.get("value_text", "")))
	queue_redraw()

func _draw() -> void:
	if service == null or not play_mode:
		return
	var factor := minf(1.0, size.x / 640.0)
	if factor <= 0.0:
		return
	draw_set_transform(Vector2.ZERO, 0.0, Vector2(factor, factor * 480.0 / 448.0))
	var font := ThemeDB.fallback_font
	var font_size := ThemeDB.fallback_font_size
	if not help.is_empty() and help.get("ok", false):
		draw_rect(Rect2(help.frame_x, help.frame_y, help.frame_width, help.frame_height), _presented(0x80, 0x80, 0x80, 0x80), false)
		draw_rect(Rect2(help.inner_x, help.inner_y, help.inner_width, help.inner_height), _word(0x3f000000), true)
		_text(font, font_size, help.title_x, help.title_y, str(help.get("title", "")), _word(0x8028444e))
		_text(font, font_size, help.body_x, help.body_y, str(help.get("body", "")), _word(0x80464646))
	if not notice.is_empty() and notice.get("ok", false):
		draw_rect(Rect2(notice.frame_x, notice.frame_y, notice.frame_width, notice.frame_height), _presented(0x80, 0x80, 0x80, 0x80), false)
		draw_rect(Rect2(notice.rect_x0, notice.rect_y0, notice.rect_x1 - notice.rect_x0, notice.rect_y1 - notice.rect_y0), _word(0x3f000000), true)
	if not medal.is_empty() and medal.get("ok", false) and medal.get("shown", false):
		_text(font, font_size, medal.list_x, medal.list_y, str(medal.get("body", "")), _presented(0x80, 0x80, 0x80, 0xff))
	if not net_cam.is_empty() and net_cam.get("ok", false) and net_cam.get("show", false):
		_text(font, font_size, net_cam.name_x, net_cam.name_y, str(net_cam.get("name", "")), _presented(0xff, 0xff, 0xff, 0xff))
		if net_cam.get("prompt", false):
			_text(font, font_size, net_cam.prompt_x, net_cam.prompt_y, str(net_cam.get("prompt_text", "")), _word(int(net_cam.prompt_colour)))
	if not bust.is_empty() and bust.get("ok", false):
		for index in bust.order:
			var record: Dictionary = bust.submissions[index]
			if record.kind != "text":
				continue
			_text(font, font_size, record.x, record.y, str(record.text), _word(int(record.colour)))
	# Colour table only. The projector that would place these in the world is refused.
	var chip := 0
	for rgb in blips:
		var presented := _presented(int(rgb[0]), int(rgb[1]), int(rgb[2]), 255)
		draw_rect(Rect2(8 + chip * 14, 430, 10, 10), presented, true)
		chip += 1
	if not compass_text.is_empty() and compass.get("ok", false):
		_text(font, font_size, compass.x, compass.y, compass_text, _word(int(compass.colour)))

func _presented(r: int, g: int, b: int, alpha: int) -> Color:
	var packed: Dictionary = service.present_colour(r, g, b, 0x3f000000)
	var word := int(packed.get("colour", 0)) if packed.get("ok", false) else (r | (g << 8) | (b << 16))
	return Color(float(word & 255) / 255.0, float((word >> 8) & 255) / 255.0, float((word >> 16) & 255) / 255.0, clampf(float(alpha) / 128.0, 0.0, 1.0))

func _word(tint: int) -> Color:
	var channel := func(shift: int) -> float:
		return clampf(float((tint >> shift) & 255) / 128.0, 0.0, 1.0)
	return Color(channel.call(0), channel.call(8), channel.call(16), channel.call(24))

func _text(font: Font, font_size: int, x: int, y: int, text: String, colour: Color) -> void:
	if text.is_empty() or font == null:
		return
	draw_string(font, Vector2(x, y + font.get_ascent(font_size)), text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, colour)
