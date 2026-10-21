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
var text_drawn: Array[String] = []
# HUD text that is not drawn, keyed by element, with the missing evidence.
var text_refusals: Dictionary = {}
var glow_alpha := 0
# Set by observe() while every glow gate holds: the tick and origin the next
# displayed draw uses. Empty when a gate is false.
var glow_context: Dictionary = {}

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
		if name.begins_with("bar_") or name == "hud_statusbar00.tex" or name == "hud_statusbar00glow.tex":
			textures[name] = ImageTexture.create_from_image(art[id].image)
	queue_redraw()

func observe(value: Dictionary) -> void:
	records.clear()
	missing.clear()
	glow_context = {}
	error = ""
	if layout_service == null or dynamic_service == null:
		error = "Native HUD layout service unavailable."
		queue_redraw()
		return
	var health_ok := valid_bar(value, "hp", "max_hp")
	var mana_ok := valid_bar(value, "mana", "max_mana")
	var experience_ok := typeof(value.get("acknowledged_level")) == TYPE_INT and int(value.acknowledged_level) > 0 and typeof(value.get("experience")) == TYPE_INT and typeof(value.get("experience_base")) == TYPE_INT and typeof(value.get("experience_next")) == TYPE_INT and int(value.experience_next) > int(value.experience_base)
	var origins: Dictionary = layout_service.hud_origins(false, PackedInt32Array([0]), 1)
	# Slot zero is this authored offline singleton view. Unknown cooldown and
	# flash gates are not submitted. Neutral inputs only obtain
	# unaffected geometry when a bar is absent; that bar is discarded below.
	var result: Dictionary = layout_service.hud_base_layers(origins.origins[0], int(value.get("ready_ticks", 0)), value.hp if health_ok else 0.0, value.max_hp if health_ok else 1.0, value.mana if mana_ok else 0.0, value.max_mana if mana_ok else 1.0, int(value.experience) if experience_ok else 0, int(value.experience_base) if experience_ok else 0, int(value.experience_next) if experience_ok else 1, int(value.acknowledged_level) if experience_ok else 0, true, 0, false)
	if not result.get("ok", false):
		error = str(result.get("error", "HUD layout refused."))
		queue_redraw()
		return
	var layers := PackedInt32Array()
	for record in result.layers:
		if (record.resource.begins_with("bar_red") and not health_ok) or (record.resource.begins_with("bar_blue") and not mana_ok) or (record.resource.begins_with("bar_green") and not experience_ok): continue
		records.append(record)
		layers.append(record.layer)
		if not textures.has(record.resource): missing.append(record.resource)
	# hud-dynamic.md section 3: local slot, present live object, HP > 0,
	# pending level. Section 2: the alpha rises by 4 per *displayed draw* and
	# persists until hud_run opens the level menu. observe() only decides the
	# gates, reserves the glow's place in the painter order and resets the
	# alpha on the menu-open event; advance_glow() steps it once per draw.
	# HP comes from the simulation's own player values (the same ones the bars
	# draw), not from the character record.
	glow_context = {}
	var level_menu_opened: bool = value.get("level_menu_opened", false) == true
	if level_menu_opened:
		glow_alpha = 0
	var pending_level := typeof(value.get("level")) == TYPE_INT and typeof(value.get("acknowledged_level")) == TYPE_INT and int(value.level) > int(value.acknowledged_level)
	var glow_ready: bool = not level_menu_opened and pending_level and value.get("player_object_present", false) == true and value.get("player_object_dead", true) == false and valid_bar(value, "hp", "max_hp") and float(value.hp) > 0.0
	if glow_ready:
		var tick := int(value.get("ready_ticks", 0))
		var preview: Dictionary = _glow_record(tick, int(origins.origins[0]), glow_alpha)
		if not preview.is_empty():
			glow_context = {"tick": tick, "origin": int(origins.origins[0])}
			# Native status submission is glow, then frame (statusSequence).
			var frame_at := records.size() - 1
			records.insert(frame_at, preview)
			layers.insert(frame_at, 6)
			if not textures.has(preview.resource): missing.append(preview.resource)
	var order: Dictionary = dynamic_service.painter_indices(layers)
	painter_order = order.get("indices", PackedInt32Array())
	if not order.get("ok", false): error = str(order.get("error", "HUD order refused."))
	text_glyphs.clear()
	text_drawn.clear()
	text_refusals.clear()
	if not font_bytes.is_empty():
		var origin := int(origins.origins[0])
		var tick := int(value.get("ready_ticks", 0))
		if typeof(value.get("name")) == TYPE_STRING and not str(value.name).is_empty():
			if typeof(value.get("arena_colour")) != TYPE_INT or typeof(value.get("name_blink_mask")) != TYPE_INT:
				text_refusals["name"] = "HUD arena colour and blink mask have no observed session source (hud-dynamic.md section 3)"
			else:
				var status: Dictionary = dynamic_service.status_adjuncts(0, tick, origin, 0, int(value.arena_colour), false, 0, int(value.name_blink_mask), false, false, 0)
				if status.get("ok", false):
					var fitted: Dictionary = dynamic_service.fit_ascii(font_bytes, str(value.name).to_utf8_buffer(), int(status.name_width_budget), true)
					if fitted.get("ok", false):
						var named: Dictionary = layout_service.hud_text_glyphs(font_bytes, fitted.ascii, origin + 73, 17, int(status.name_colour))
						if named.get("ok", false):
							text_glyphs.append_array(named.glyphs)
							text_drawn.append("name_text")
						else: text_refusals["name"] = "HUD font rejected the fitted name"
					else: text_refusals["name"] = "ui_fitText refused the player name"
				else: text_refusals["name"] = "HUD name colour inputs were refused"
		if typeof(value.get("level")) == TYPE_INT:
			if typeof(value.get("acknowledged_level")) != TYPE_INT:
				text_refusals["level"] = "HUD level colour needs the acknowledged level +0x58 (hud-dynamic.md section 3)"
			else:
				# Section 3: base colour 0x80323c3c; while a level is pending
				# (acknowledged +0x58 < level +0x59) R=G=p/2+60, B=p/2+50, alpha 0x80,
				# with the shared pulse p of g_simFrame.
				var tint := 0x80323c3c
				if int(value.acknowledged_level) < int(value.level):
					var level_status: Dictionary = dynamic_service.status_adjuncts(0, tick, origin, 0, 0, false, 0, 0, false, false, 0)
					tint = int(level_status.get("pending_level_colour", 0)) if level_status.get("ok", false) else -1
				if tint < 0:
					text_refusals["level"] = "HUD pending level colour refused by the native service"
				else:
					var leveled: Dictionary = layout_service.hud_text_glyphs(font_bytes, str(int(value.level)).to_utf8_buffer(), origin + 134, 44, tint)
					if leveled.get("ok", false):
						text_glyphs.append_array(leveled.glyphs)
						text_drawn.append("level_text")
	queue_redraw()

# The native glow sprite for an input alpha; the native service also returns
# the stepped alpha (+4, capped at 128), which advance_glow() commits.
func _glow_record(tick: int, origin: int, alpha: int, commit := false) -> Dictionary:
	var adjunct: Dictionary = dynamic_service.status_adjuncts(0, tick, origin, alpha, 0, false, 0, 0, false, false, 0)
	if not adjunct.get("ok", false): return {}
	if commit: glow_alpha = int(adjunct.glow_alpha)
	var glow: Dictionary = adjunct.glow_sprite
	glow["layer"] = 6
	glow["clipped"] = true
	glow["clip"] = {"left": 0, "top": 0, "right": 1000, "bottom": 1000}
	return glow

# One displayed draw: step the alpha and use the stepped sprite. Called by
# _draw(); tests call it to stand in for a displayed frame.
func advance_glow() -> void:
	if glow_context.is_empty() or dynamic_service == null: return
	var stepped: Dictionary = _glow_record(int(glow_context.tick), int(glow_context.origin), glow_alpha, true)
	if stepped.is_empty(): return
	for index in records.size():
		if records[index].get("resource", "") == "hud_statusbar00glow.tex":
			records[index] = stepped
			return

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
	advance_glow()
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
