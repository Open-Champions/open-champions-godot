extends RefCounted

# Draws the front end's documented draw calls (docs/FRONTEND.md "Layout"; native
# champions::frontend::Flow::layout, from research-menus-layout dc458d5). The
# native flow decides what is drawn, where, with which font word, layer and tint;
# this class only loads the user's own fonts and textures at run time and paints.
# Nothing read from the game folder is stored outside this process.
#
# Canvas: 640x448 UI draw coordinates, shown square-pixel 4:3 like ui/text_hud.gd.
# Painter rule (research-ui hud-dynamic.md 1, the same UI primitive queue): layers
# 0..7, and inside a layer the later submission is painted first.
const RealText = preload("res://real_data/real_text.gd")
const FONT_FILES := {"medium": "gamefontmed.fnt", "small": "gamefontsmall.fnt"}
const HOST_PX := {"medium": 16, "small": 13}

var real: Node
var inspector: Object
var layout_service: RefCounted
var dynamic_service: RefCounted
var fonts := {}        # "medium"/"small" -> {bytes, texture, size}
var art := {}          # lump/entry -> {texture, width, height}
var art_missing := {}
var body_listing := {}    # full SELECT.LMP listing; body textures are not UI candidates
var items: Array = []  # sorted at paint time
var drawn := {"sprite": 0, "frame_piece": 0, "rect": 0, "text": 0, "text_fallback": 0, "sprite_missing": 0}
var last_error := ""

func configure(real_data: Node, archive_inspector: Object) -> void:
	real = real_data
	inspector = archive_inspector
	fonts.clear()
	art.clear()
	art_missing.clear()
	body_listing.clear()
	if ClassDB.class_exists("ChampionsUiLayoutService"):
		layout_service = ClassDB.instantiate("ChampionsUiLayoutService")
	if ClassDB.class_exists("ChampionsUiDynamicLayoutService"):
		dynamic_service = ClassDB.instantiate("ChampionsUiDynamicLayoutService")
	if real == null or layout_service == null:
		return
	var bytes := RealText.fonts(real)
	for key in FONT_FILES.keys():
		var raw: PackedByteArray = bytes.get(FONT_FILES[key], PackedByteArray())
		if raw.is_empty():
			continue
		var decoded: Dictionary = layout_service.decode_hud_font(raw, true)
		if not decoded.get("ok", false):
			continue
		var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
		fonts[key] = {"bytes": raw, "texture": ImageTexture.create_from_image(image), "size": image.get_size()}

func has_fonts() -> bool:
	return fonts.has("medium") and fonts.has("small") and dynamic_service != null

func _units(text: String) -> PackedInt32Array:
	var out := PackedInt32Array()
	for c in text.to_utf32_buffer().to_int32_array():
		out.append(c)
	return out

func _glyphs(f: Dictionary, units: PackedInt32Array, x: int, y: int, tint: int) -> Array:
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, x, y, units.size(), true, 0, 0, 0, 0, false, 0, 100000, tint, -1, -1)
	return r.get("glyphs", []) if r.get("ok", false) else []

func _centred(f: Dictionary, units: PackedInt32Array, x: int, y: int, tint: int) -> Array:
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, x, y, units.size(), true, 0, 0, 0, 0, false, 0, 100000, tint, -1, -1)
	if not r.get("ok", false):
		return []
	return _glyphs(f, units, int(r.centered_x), y, tint)

func _wrapped(f: Dictionary, units: PackedInt32Array, x: int, y: int, width: int, line: int, centred: bool, tint: int) -> Array:
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, x, y, units.size(), true, width, 0, 448, line, centred, 0, 100000, tint, -1, -1)
	var out := []
	if not r.get("ok", false):
		return out
	for run in r.get("runs", []):
		var part := units.slice(int(run.start), int(run.start) + int(run.count))
		out.append_array(_glyphs(f, part, int(run.x), int(run.y), tint))
	return out

func _add(item: Dictionary) -> void:
	item.order = items.size()
	items.append(item)

# Expands the native scene into paintable items. Returns the item count.
func build(scene: Dictionary) -> int:
	items.clear()
	for d in scene.get("draws", []):
		match str(d.kind):
			"sprite":
				_add({"layer": int(d.layer), "kind": "sprite", "lump": str(d.lump).to_lower(), "resource": str(d.resource), "x": int(d.x), "y": int(d.y), "clipped": false, "clip": {}, "tint": int(d.colour), "weave": true, "evidence": str(d.evidence)})
			"frame":
				_add_frame(d)
			"rect":
				_add({"layer": int(d.layer), "kind": "rect", "rect": Rect2(int(d.x), int(d.y), int(d.w), int(d.h)), "tint": _tint(d)})
			"text":
				_add_text(d)
	return items.size()

# The native draw retains the research colour word. Text here is host-authored
# placeholder copy, so the host paints it readable at its own opacity. Only
# documented fade batches set Draw::settled; this is not a fade interpretation.
func _tint(d: Dictionary) -> int:
	var c := int(d.colour)
	if str(d.kind) == "text" or bool(d.get("settled", false)):
		return (c & 0xffffff) | 0x80000000
	return c

func _add_frame(d: Dictionary) -> void:
	if layout_service == null:
		return
	var r: Dictionary = layout_service.menu_frame(int(d.x), int(d.y), int(d.w), int(d.h), int(d.style), false, false)
	if not r.get("ok", false):
		last_error = str(r.get("error", "menu_frame refused"))
		return
	_add({"layer": int(d.layer), "kind": "frame_box", "rect": Rect2(int(d.x), int(d.y), int(d.w), int(d.h)), "tint": _tint(d), "pieces": r.draws.size()})
	for piece in r.draws:
		_add({"layer": int(d.layer), "kind": "sprite", "lump": "interfac.lmp", "resource": str(piece.resource), "x": int(piece.x), "y": int(piece.y), "clipped": bool(piece.clipped), "clip": piece.get("clip", {}), "tint": _tint(d), "weave": true, "frame": true})

func _add_text(d: Dictionary) -> void:
	var key := str(d.font)
	var text := str(d.text)
	if text.is_empty():
		return
	if not fonts.has(key) or dynamic_service == null:
		_add({"layer": int(d.layer), "kind": "host_text", "font": key, "x": int(d.x), "y": int(d.y), "text": text, "align": str(d.align), "width": int(d.wrap_width), "tint": _tint(d)})
		return
	var f: Dictionary = fonts[key]
	var units := _units(text)
	var tint := _tint(d)
	var glyphs := []
	match str(d.align):
		"centred": glyphs = _centred(f, units, int(d.x), int(d.y), tint)
		"wrapped": glyphs = _wrapped(f, units, int(d.x), int(d.y), int(d.wrap_width) if int(d.wrap_width) > 0 else 560, int(d.line) if int(d.line) > 0 else 17, false, tint)
		"wrapped_centred": glyphs = _wrapped(f, units, int(d.x), int(d.y), int(d.wrap_width) if int(d.wrap_width) > 0 else 560, int(d.line) if int(d.line) > 0 else 17, true, tint)
		_: glyphs = _glyphs(f, units, int(d.x), int(d.y), tint)
	_add({"layer": int(d.layer), "kind": "text", "font": key, "glyphs": glyphs})

func painted() -> Array:
	var out := items.duplicate()
	out.sort_custom(func(a, b): return a.layer < b.layer or (a.layer == b.layer and a.order > b.order))
	return out

func colour_of(tint: int) -> Color:
	return Color(float(tint & 255) / 128.0, float((tint >> 8) & 255) / 128.0, float((tint >> 16) & 255) / 128.0, float((tint >> 24) & 255) / 128.0)

func load_art(lump: String, name: String, weave: bool) -> Dictionary:
	var key := lump + "/" + name
	if art.has(key):
		return art[key]
	if art_missing.has(key) or real == null or inspector == null or not real.files.has(lump):
		return {}
	var path: String = real.files[lump]
	var listing: Dictionary = inspector.inspect_ui_candidates(path, "rta")
	if not listing.get("ok", false):
		art_missing[key] = true
		return {}
	for row in listing.rows:
		if str(row.get("name", "")).to_lower() != name.to_lower() or row.get("kind", "") == "font":
			continue
		var decoded: Dictionary = inspector.inspect_texture(path, int(row.row_index), "rta", listing.fingerprint)
		if not decoded.get("ok", false):
			break
		var w := int(decoded.width)
		var h := int(decoded.height)
		var rgba: PackedByteArray = decoded.rgba
		if weave and h % 2 == 0:
			var shaped: Dictionary = inspector.shape_ui_art(w, h, rgba, h / 2, [0, 0, w, h])
			if shaped.get("ok", false):
				rgba = shaped.rgba
		var image := Image.create_from_data(w, h, false, Image.FORMAT_RGBA8, rgba)
		art[key] = {"texture": ImageTexture.create_from_image(image), "width": w, "height": h}
		return art[key]
	art_missing[key] = true
	return {}

func load_body_texture(code: String) -> Dictionary:
	var name := code + "_bodyv1.tex"
	var key := "select.lmp/" + name
	if art.has(key):
		return art[key]
	if art_missing.has(key) or real == null or inspector == null or not real.files.has("select.lmp"):
		return {}
	var path: String = real.files["select.lmp"]
	if body_listing.is_empty():
		body_listing = inspector.inspect_archive(path)
	if not body_listing.get("ok", false):
		art_missing[key] = true
		return {}
	var rows: Array = body_listing.get("rows", [])
	for index in range(rows.size()):
		var row: Dictionary = rows[index]
		if str(row.get("name", "")).to_lower() != name:
			continue
		var decoded: Dictionary = inspector.inspect_texture(path, index, "rta", body_listing.fingerprint)
		if not decoded.get("ok", false):
			break
		# Match the existing model viewer's neutral preview policy. Stored alpha
		# is not a documented front-end blend rule; show RGB opaquely and label it.
		var rgba: PackedByteArray = decoded.rgba.duplicate()
		for offset in range(3, rgba.size(), 4):
			rgba[offset] = 255
		var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, rgba)
		art[key] = {"texture": ImageTexture.create_from_image(image), "width": int(decoded.width), "height": int(decoded.height)}
		return art[key]
	art_missing[key] = true
	return {}

# Paints into canvas (a CanvasItem inside its own _draw). The caller has set the
# 640x448 transform.
func paint(canvas: CanvasItem) -> void:
	drawn = {"sprite": 0, "frame_piece": 0, "rect": 0, "text": 0, "text_fallback": 0, "sprite_missing": 0}
	for item in painted():
		match item.kind:
			"sprite": _paint_sprite(canvas, item)
			"frame_box": _paint_frame_box(canvas, item)
			"rect":
				canvas.draw_rect(item.rect, colour_of(item.tint), true)
				drawn.rect += 1
			"text": _paint_glyphs(canvas, item)
			"host_text": _paint_host_text(canvas, item)

func _paint_sprite(canvas: CanvasItem, item: Dictionary) -> void:
	var a := load_art(item.lump, item.resource, item.weave)
	if a.is_empty():
		if not item.get("frame", false):
			drawn.sprite_missing += 1
		return
	var original := Rect2(item.x, item.y, a.width, a.height)
	if not item.clipped:
		original.position.y += 1 # the plain sprite primitive adds one to y (research-ui)
	var dest := original
	if item.clipped:
		var c: Dictionary = item.clip
		dest = dest.intersection(Rect2(c.left, c.top, c.right - c.left, mini(447, c.bottom) - c.top))
	if not dest.has_area():
		return
	canvas.draw_texture_rect_region(a.texture, dest, Rect2(dest.position - original.position, dest.size), colour_of(item.tint), false)
	if item.get("frame", false):
		drawn.frame_piece += 1
	else:
		drawn.sprite += 1

# With no game folder the frame pieces cannot load: a plain tinted box stands in.
func _paint_frame_box(canvas: CanvasItem, item: Dictionary) -> void:
	if real != null and real.files.has("interfac.lmp"):
		return
	var c := colour_of(item.tint)
	c.a = clampf(c.a, 0.0, 1.0) * 0.6
	canvas.draw_rect(item.rect, c, true)

func _paint_glyphs(canvas: CanvasItem, item: Dictionary) -> void:
	var f: Dictionary = fonts.get(item.font, {})
	if f.is_empty():
		return
	var page: Vector2i = f.size
	for g in item.glyphs:
		var sw := int(g.source_width)
		var sh := int(g.source_height)
		var sx := int(g.source_x)
		var sy := int(g.source_y)
		if sw <= 0 or sh <= 0 or sx < 0 or sy < 0 or sx + sw > page.x or sy + sh > page.y:
			continue
		canvas.draw_texture_rect_region(f.texture, Rect2(float(g.x_half) * 0.5, float(g.y), sw, sh), Rect2(sx, sy, sw, sh), colour_of(int(g.tint)), false)
	drawn.text += 1

func _paint_host_text(canvas: CanvasItem, item: Dictionary) -> void:
	var size_px: int = HOST_PX.get(item.font, 14)
	var font := ThemeDB.fallback_font
	var tint := colour_of(item.tint)
	var pos := Vector2(item.x, item.y + size_px)
	match item.align:
		"centred":
			# a 640-wide box centred on the anchor
			canvas.draw_string(font, Vector2(item.x - 320, item.y + size_px), item.text, HORIZONTAL_ALIGNMENT_CENTER, 640, size_px, tint)
		"wrapped", "wrapped_centred":
			var w := float(item.width) if item.width > 0 else 560.0
			var align := HORIZONTAL_ALIGNMENT_CENTER if item.align == "wrapped_centred" else HORIZONTAL_ALIGNMENT_LEFT
			var origin := Vector2(item.x - w * 0.5, item.y + size_px) if item.align == "wrapped_centred" else pos
			canvas.draw_multiline_string(font, origin, item.text, align, w, size_px, -1, tint)
		_:
			canvas.draw_string(font, pos, item.text, HORIZONTAL_ALIGNMENT_LEFT, -1, size_px, tint)
	drawn.text_fallback += 1
