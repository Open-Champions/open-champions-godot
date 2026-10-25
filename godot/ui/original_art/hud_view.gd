extends Control

# Native layout owns coordinates, animation frame, fill arithmetic and order.
# This view shows the top 112 rows of the 640x448 logical canvas, mapped to
# square-pixel 4:3. Godot blending is presentation, not PS2 framebuffer parity.
var layout_service: RefCounted
var dynamic_service: RefCounted
var records: Array = []
var painter_order := PackedInt32Array()
var textures: Dictionary = {}
var missing: Array[String] = []
var error := ""
var font_bytes := PackedByteArray()
var font_texture: ImageTexture
var font_size := Vector2i.ZERO
var text_glyphs: Array = []
# HUD text that is not drawn, keyed by element, with the missing evidence.
var text_refusals: Dictionary = {}

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	custom_minimum_size = Vector2(640, 120)
	if ClassDB.class_exists("ChampionsUiLayoutService"):
		layout_service = ClassDB.instantiate("ChampionsUiLayoutService")
	if ClassDB.class_exists("ChampionsUiDynamicLayoutService"):
		dynamic_service = ClassDB.instantiate("ChampionsUiDynamicLayoutService")

func set_font(fnt: PackedByteArray) -> bool:
	if layout_service == null or fnt.is_empty():
		return false
	var decoded: Dictionary = layout_service.decode_hud_font(fnt, true)
	if not decoded.get("ok", false):
		return false
	var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
	font_texture = ImageTexture.create_from_image(image)
	font_size = image.get_size()
	font_bytes = fnt
	queue_redraw()
	return true

func set_art(art: Dictionary, catalogue: Dictionary) -> void:
	textures.clear()
	for id in art:
		var name: String = catalogue[id].entry
		if name.begins_with("bar_") or name == "hud_statusbar00.tex":
			textures[name] = ImageTexture.create_from_image(art[id].image)
	queue_redraw()

func observe(value: Dictionary) -> void:
	records.clear()
	painter_order = PackedInt32Array()
	missing.clear()
	error = ""
	if layout_service == null or dynamic_service == null:
		error = "Native HUD layout service unavailable."
		queue_redraw()
		return
	var health_ok := valid_bar(value, "hp", "max_hp")
	var mana_ok := valid_bar(value, "mana", "max_mana")
	var origins: Dictionary = layout_service.hud_origins(false, PackedInt32Array([0]), 1)
	# Slot zero is this authored offline singleton view. The player record may
	# supply the documented XP interval; cooldown and flash gates are absent. Neutral inputs only obtain
	# unaffected geometry when a bar is absent; that bar is discarded below.
	var xp_ok := typeof(value.get("experience")) == TYPE_INT and typeof(value.get("experience_base")) == TYPE_INT and typeof(value.get("experience_next")) == TYPE_INT and typeof(value.get("acknowledged_level")) == TYPE_INT and int(value.experience_next) > int(value.experience_base) and int(value.acknowledged_level) > 0
	var result: Dictionary = layout_service.hud_base_layers(origins.origins[0], int(value.get("ready_ticks", 0)), value.hp if health_ok else 0.0, value.max_hp if health_ok else 1.0, value.mana if mana_ok else 0.0, value.max_mana if mana_ok else 1.0, int(value.experience) if xp_ok else 0, int(value.experience_base) if xp_ok else 0, int(value.experience_next) if xp_ok else 1, int(value.acknowledged_level) if xp_ok else 0, true, 0, false)
	if not result.get("ok", false):
		error = str(result.get("error", "HUD layout refused."))
		queue_redraw()
		return
	var layers := PackedInt32Array()
	for record in result.layers:
		if (record.resource.begins_with("bar_red") and not health_ok) or (record.resource.begins_with("bar_blue") and not mana_ok): continue
		records.append(record)
		layers.append(record.layer)
		if not textures.has(record.resource): missing.append(record.resource)
	var order: Dictionary = dynamic_service.painter_indices(layers)
	painter_order = order.get("indices", PackedInt32Array())
	if not order.get("ok", false): error = str(order.get("error", "HUD order refused."))
	text_glyphs.clear()
	text_refusals.clear()
	if not font_bytes.is_empty():
		var origin := int(origins.origins[0])
		var tick := int(value.get("ready_ticks", 0))
		if typeof(value.get("name")) == TYPE_STRING and not str(value.name).is_empty():
			# research-ui 3e39392 docs/hud-dynamic.md section 3: the name colour
			# depends on the arena colour 0x3054c4, player_slotColor with two or
			# more players and the blink mask 0x505b04, and the text goes through
			# ui_fitText (budget 131) or a debug save notice. None is observed.
			text_refusals["name"] = "HUD name colour needs the arena colour, player_slotColor, the blink mask 0x505b04 and ui_fitText, which the observation does not supply (hud-dynamic.md section 3)"
		if typeof(value.get("level")) == TYPE_INT:
			if typeof(value.get("acknowledged_level")) != TYPE_INT:
				text_refusals["level"] = "HUD level colour needs the acknowledged level +0x58 (hud-dynamic.md section 3)"
			else:
				# Section 3: base colour 0x80323c3c; while a level is pending
				# (acknowledged +0x58 < level +0x59) R=G=p/2+60, B=p/2+50, alpha 0x80,
				# with the shared pulse p of g_simFrame.
				var tint := 0x80323c3c
				if int(value.acknowledged_level) < int(value.level):
					var status: Dictionary = dynamic_service.status_adjuncts(0, tick, origin, 0, 0, false, 0, 0, false, false, 0)
					tint = int(status.get("pending_level_colour", 0)) if status.get("ok", false) else -1
				if tint < 0:
					text_refusals["level"] = "HUD pending level colour refused by the native service"
				else:
					var leveled: Dictionary = layout_service.hud_text_glyphs(font_bytes, str(int(value.level)).to_utf8_buffer(), origin + 134, 44, tint)
					if leveled.get("ok", false):
						text_glyphs.append_array(leveled.glyphs)
	queue_redraw()

func valid_bar(value: Dictionary, current: String, maximum: String) -> bool:
	for key in [current, maximum]:
		if typeof(value.get(key)) not in [TYPE_INT, TYPE_FLOAT] or not is_finite(float(value[key])): return false
	return float(value[current]) >= 0.0 and float(value[maximum]) > 0.0

# 640x448 logical canvas to this control; a 4:3 square-pixel picture is 640x480.
func _canvas_transform() -> void:
	var factor := minf(1.0, size.x / 640.0)
	draw_set_transform(Vector2.ZERO, 0, Vector2(factor, factor * 480.0 / 448.0))

func _draw() -> void:
	if layout_service == null: return
	_canvas_transform()
	for index in painter_order:
		var record: Dictionary = records[index]
		if record.get("kind", "") == "rect":
			# ui_drawRect 0x17bd30: untextured, RGBAQ colour as given, ALPHA 0x44
			# with 0x80 = 1.0; a GS sprite covers [x0, x1) x [y0, y1) (ui-parity.md 1).
			var word: int = record.colour
			var flat := Color(float(word & 255) / 255.0, float((word >> 8) & 255) / 255.0, float((word >> 16) & 255) / 255.0, minf(1.0, float((word >> 24) & 255) / 128.0))
			if record.right > record.left and record.bottom > record.top:
				draw_rect(Rect2(record.left, record.top, record.right - record.left, record.bottom - record.top), flat, true)
			continue
		if not textures.has(record.resource): continue
		var source: Dictionary = layout_service.hud_source_rect(record.resource)
		if not source.get("ok", false): continue
		var original := Rect2(record.x, record.y, source.rect.width, source.rect.height)
		if not record.clipped: original.position.y += 1
		var dest := original
		if record.clipped:
			var clip: Dictionary = record.clip
			dest = dest.intersection(Rect2(clip.left, clip.top, clip.right - clip.left, mini(447, clip.bottom) - clip.top))
		if not dest.has_area(): continue
		var region := Rect2(dest.position - original.position, dest.size)
		var tint: int = record.tint
		var colour := Color(float(tint & 255) / 128.0, float((tint >> 8) & 255) / 128.0, float((tint >> 16) & 255) / 128.0, float((tint >> 24) & 255) / 128.0)
		draw_texture_rect_region(textures[record.resource], dest, region, colour, false)
	if font_texture == null:
		return
	for glyph in text_glyphs:
		var sw := int(glyph.source_width)
		var sh := int(glyph.source_height)
		var sx := int(glyph.source_x)
		var sy := int(glyph.source_y)
		if sw <= 0 or sh <= 0 or sx < 0 or sy < 0 or sx + sw > font_size.x or sy + sh > font_size.y:
			continue
		var tint: int = int(glyph.tint)
		var colour := Color(float(tint & 255) / 128.0, float((tint >> 8) & 255) / 128.0, float((tint >> 16) & 255) / 128.0, float((tint >> 24) & 255) / 128.0)
		draw_texture_rect_region(font_texture, Rect2(float(glyph.x_half) * 0.5, float(glyph.y), sw, sh), Rect2(sx, sy, sw, sh), colour, false)
