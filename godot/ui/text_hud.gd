extends Control

# The level script's text HUD over the play view (docs/STRINGS.md): the
# one-line message of setInfo, the help box of addHelpMessage(F) and the bust
# panel of showBust. State comes from the native host (controller.text_hud);
# placement, colours and order are research-ui 3e39392 docs/hud-dynamic.md
# sections 1, 4, 7 and 9 through the native layout services. Glyphs are the
# game's own fonts and art read from the user's folder at run time.
#
# Logical canvas 640x448, shown square-pixel 4:3 like ui/original_art/hud_view.
# help_task and the draw-side fades run once per displayed frame at 60 Hz
# (the original's frame rate); confirm is Enter, an authored key.
const RealText = preload("res://real_data/real_text.gd")
const FRAME := 1.0 / 60.0
const MESSAGE_TINT := 0x1e6464
const HELP_TITLE_TINT := 0x8028444e
const HELP_BODY_TINT := 0x80464646
const HELP_FRAME_TINT := 0x80808080
const BUST_FADE_RAW := 0x3dacf3df # 0.08445 per drawn frame (hud-dynamic.md 7)
# Elements that are not drawn, and why (each needs evidence research lacks).
const REFUSED := {
	"help_inner_frame": "the style argument of help_task's inner ui_drawFrameImpl at (124, 104) is not given (hud-dynamic.md 9)",
	"help_button": "the glyph byte 0x41b978 and the source of the Affirmation string are not documented",
	"bust_meter": "with a maximum, the second wrap of the bust text batch is undocumented (bust_panel refuses)",
	"compass": "the compass mesh footprint is unknown (hud-dynamic.md 10); its keys are missing on the disc",
}
# Presentation choices research does not settle; drawn, and reported as such.
const UNVERIFIED := {
	"bust_text_font": "the bust text batch's font is not named; drawn in the default font (gamefontmed)",
	"bust_sprite_rgb": "only the bust sprite alpha is documented; RGB drawn neutral (0x80)",
	"bust_art_weave": "field storage of bust.lmp art is not documented; drawn woven, because decoded unwoven it shows two stacked half-height copies",
	"ctile_weave": "the message box ctile pieces are woven like the documented tile pieces",
	"ui_pulse": "g_uiPulse decays toward 1 by 0.8 of the excess per frame (mainloop.md step 13) in host floats",
	"value_text": "the bust value is drawn as a decimal integer",
}

var real: Node
var controller: Object
var inspector: Object
var layout_service: RefCounted
var dynamic_service: RefCounted
var round3: RefCounted
var fonts := {} # name -> {bytes, texture, size}
var art := {} # entry name -> {texture, width, height}
var art_missing := {}
var state := {}
var accumulator := 0.0
var message_alpha := 0
var bust_fade := 0.0
var pulse := 1.0
var pulse_starts := 0
var items: Array = [] # {layer, order, kind, ...} in submission order
var drawn := {"message": 0, "help": 0, "bust": 0}
var confirm_override := -1 # tests: 0 or 1 replaces the key state
var last_error := ""

func _ready() -> void:
	mouse_filter = MOUSE_FILTER_IGNORE
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	if ClassDB.class_exists("ChampionsUiLayoutService"):
		layout_service = ClassDB.instantiate("ChampionsUiLayoutService")
	if ClassDB.class_exists("ChampionsUiDynamicLayoutService"):
		dynamic_service = ClassDB.instantiate("ChampionsUiDynamicLayoutService")
	if ClassDB.class_exists("ChampionsUiHudRound3Service"):
		round3 = ClassDB.instantiate("ChampionsUiHudRound3Service")

func configure(real_data: Node, session_controller: Object, archive_inspector: Object) -> void:
	real = real_data
	controller = session_controller
	inspector = archive_inspector
	fonts.clear()
	art.clear()
	art_missing.clear()
	for name in RealText.fonts(real).keys():
		var bytes: PackedByteArray = RealText.fonts(real)[name]
		var decoded: Dictionary = layout_service.decode_hud_font(bytes, true) if layout_service != null else {}
		if not decoded.get("ok", false): continue
		var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
		fonts[name] = {"bytes": bytes, "texture": ImageTexture.create_from_image(image), "size": image.get_size()}

func _process(delta: float) -> void:
	if controller == null: return
	accumulator = minf(accumulator + delta, FRAME * 4.0)
	var stepped := false
	while accumulator >= FRAME:
		accumulator -= FRAME
		step_frame()
		stepped = true
	if stepped: queue_redraw()

func confirm_pressed() -> bool:
	if confirm_override >= 0: return confirm_override == 1
	return Input.is_key_pressed(KEY_ENTER) or Input.is_key_pressed(KEY_KP_ENTER)

# One displayed frame: help_task (draw task 50) then the HUD draw (51).
func step_frame() -> void:
	if controller == null: return
	controller.help_frame(confirm_pressed())
	state = controller.text_hud()
	items.clear()
	if not state.get("ok", false): return
	var starts := int(state.get("pulse_starts", 0))
	if starts > pulse_starts: pulse = 2.0
	else: pulse = 1.0 + (pulse - 1.0) * 0.8
	pulse_starts = starts
	if state.get("help_open", false): build_help()
	build_message()
	build_bust()

func font(name := "gamefontmed.fnt") -> Dictionary:
	return fonts.get(name, {})

func scale_colour(colour: int, factor: float) -> int:
	# color_scaleClamped 0x165428: each byte cvt.w.s(float(byte) * factor), 0..255.
	var out := 0
	for shift in [0, 8, 16, 24]:
		var v := int(float((colour >> shift) & 255) * factor)
		out |= clampi(v, 0, 255) << shift
	return out

func add_item(item: Dictionary) -> void:
	item.order = items.size()
	items.append(item)

func glyphs(f: Dictionary, units: PackedInt32Array, x: int, y: int, tint: int, count := -1) -> Array:
	var n := units.size() if count < 0 else count
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, x, y, n, true, 0, 0, 0, 0, false, 0, 100000, tint, -1, -1)
	return r.get("glyphs", []) if r.get("ok", false) else []

func centred(f: Dictionary, units: PackedInt32Array, x: int, y: int, tint: int) -> Array:
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, x, y, units.size(), true, 0, 0, 0, 0, false, 0, 100000, tint, -1, -1)
	if not r.get("ok", false): return []
	return glyphs(f, units, int(r.centered_x), y, tint)

func text_width(f: Dictionary, units: PackedInt32Array) -> int:
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, 0, 0, units.size(), true, 0, 0, 0, 0, false, 0, 100000, 0x80808080, -1, -1)
	return int(r.get("width", 0))

func wrapped(f: Dictionary, units: PackedInt32Array, x: int, y: int, width: int, top: int, bottom: int, line: int, tint: int) -> Dictionary:
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, x, y, units.size(), true, width, top, bottom, line, false, 0, 100000, tint, -1, -1)
	var out := {"glyphs": [], "height": int(r.get("height", 0))}
	if not r.get("ok", false): return out
	for run in r.get("runs", []):
		var part := units.slice(int(run.start), int(run.start) + int(run.count))
		out.glyphs.append_array(glyphs(f, part, int(run.x), int(run.y), tint))
	return out

func frame_items(x: int, y: int, w: int, h: int, style: int, layer: int, tint: int) -> void:
	var r: Dictionary = layout_service.menu_frame(x, y, w, h, style, false, false)
	if not r.get("ok", false): return
	for d in r.draws:
		add_item({"layer": layer, "kind": "sprite", "resource": str(d.resource), "lump": "interfac.lmp", "x": int(d.x), "y": int(d.y), "clipped": bool(d.clipped), "clip": d.get("clip", {}), "tint": tint, "weave": true})

# hud-dynamic.md 9: title centred at (324, 100), body wrapped at (132, 130),
# width 404, top 0, bottom 448, line 21; frame 0x17fbb8 at (116, 94), 416 by
# (wrap height + 76); colours scaled by g_uiPulse. Font 0x4f7100 (gamefontmed).
func build_help() -> void:
	var f := font()
	if f.is_empty(): return
	var title: PackedInt32Array = state.get("help_title", PackedInt32Array())
	var body: PackedInt32Array = state.get("help_body", PackedInt32Array())
	if state.get("help_has_title", false):
		add_item({"layer": 6, "kind": "text", "font": "gamefontmed.fnt", "glyphs": centred(f, title, 324, 100, scale_colour(HELP_TITLE_TINT, pulse))})
	var body_run := wrapped(f, body, 132, 130, 404, 0, 448, 21, scale_colour(HELP_BODY_TINT, pulse))
	add_item({"layer": 6, "kind": "text", "font": "gamefontmed.fnt", "glyphs": body_run.glyphs})
	var box: Dictionary = round3.help_box(int(body_run.height), 0) if round3 != null else {}
	if not box.get("ok", false): return
	# Wrapper 0x17fbb8: x-1, y-4, width+2, height+8, style 2 (hud-layout.md).
	frame_items(int(box.frame_x) - 1, int(box.frame_y) - 4, int(box.frame_width) + 2, int(box.frame_height) + 8, 2, 6, scale_colour(HELP_FRAME_TINT, pulse))
	drawn.help += 1

# hud-dynamic.md 4: while timer > 0 and (not suppressed or forced), default
# font, centred at (320, 78), layer 6 (7 forced), ctile frame behind it.
func build_message() -> void:
	var f := font()
	var timer := int(state.get("message_frames", 0))
	var force := int(state.get("message_force", 0)) != 0
	var units: PackedInt32Array = state.get("message", PackedInt32Array())
	if f.is_empty() or dynamic_service == null: return
	var r: Dictionary = dynamic_service.message_frame(timer, message_alpha, force, bool(state.get("suppressed", false)), text_width(f, units))
	if not r.get("ok", false): return
	if r.submissions.is_empty(): return
	message_alpha = int(r.alpha)
	for s in r.submissions:
		if s.kind == "text":
			add_item({"layer": int(s.layer), "kind": "text", "font": "gamefontmed.fnt", "glyphs": centred(f, units, int(s.x), int(s.y), int(s.colour))})
		elif s.kind == "frame":
			frame_items(int(s.x), int(s.y), int(s.width), int(s.height), int(s.style), int(s.layer), int(s.tint))
	drawn.message += 1

func load_art(lump: String, name: String, weave: bool) -> Dictionary:
	var key := lump + "/" + name
	if art.has(key): return art[key]
	if art_missing.has(key) or real == null or not real.files.has(lump): return {}
	var path: String = real.files[lump]
	var listing: Dictionary = inspector.inspect_ui_candidates(path, "rta")
	if not listing.get("ok", false):
		art_missing[key] = true
		return {}
	for row in listing.rows:
		if str(row.get("name", "")).to_lower() != name.to_lower() or row.get("kind", "") == "font": continue
		var decoded: Dictionary = inspector.inspect_texture(path, int(row.row_index), "rta", listing.fingerprint)
		if not decoded.get("ok", false): break
		var w := int(decoded.width)
		var h := int(decoded.height)
		var rgba: PackedByteArray = decoded.rgba
		if weave and h % 2 == 0:
			var shaped: Dictionary = inspector.shape_ui_art(w, h, rgba, h / 2, [0, 0, w, h])
			if shaped.get("ok", false): rgba = shaped.rgba
		var image := Image.create_from_data(w, h, false, Image.FORMAT_RGBA8, rgba)
		art[key] = {"texture": ImageTexture.create_from_image(image), "width": w, "height": h}
		return art[key]
	art_missing[key] = true
	return {}

# hud-dynamic.md 7, no maximum (the 11 disc calls with -1): text batch, then
# fieldnumber.tex, the portrait and frame.tex from bust.lmp, all layer 6.
func build_bust() -> void:
	var shown := int(state.get("bust_shown", 0)) > 0
	if not shown: return
	var fade := bytes_float(BUST_FADE_RAW)
	bust_fade = clampf(bust_fade - fade if int(state.get("latch", 0)) != 0 else bust_fade + fade, 0.0, 1.0)
	if int(state.get("bust_max", -1)) != -1: return
	var portrait := load_art("bust.lmp", str(state.get("bust_texture", "")), true)
	var frame := load_art("bust.lmp", "frame.tex", true)
	if portrait.is_empty() or frame.is_empty() or round3 == null: return
	var f := font()
	var text_alpha := int(bust_fade * 110.0)
	var sprite_alpha := int(bust_fade * 128.0)
	var value := maxi(0, int(state.get("bust_value", 0)))
	var r: Dictionary = round3.bust_panel(false, value, -1, 0, 0, text_alpha, sprite_alpha, portrait.width, portrait.height, frame.width, frame.height, "", str(value))
	if not r.get("ok", false): return
	var body: PackedInt32Array = state.get("bust_text", PackedInt32Array())
	var value_units := PackedInt32Array()
	for c in str(value).to_ascii_buffer(): value_units.append(c)
	for s in r.submissions:
		if s.kind == "text":
			var tint := int(s.colour)
			if str(s.get("text_kind", "")) == "wrapped":
				# text_drawWrapped (523, 391, width 112, top -100, bottom 500, line 14).
				add_item({"layer": 6, "kind": "text", "font": "gamefontmed.fnt", "glyphs": wrapped(f, body, 523, 391, 112, -100, 500, 14, tint).glyphs})
			else:
				add_item({"layer": 6, "kind": "text", "font": "gamefontmed.fnt", "glyphs": centred(f, value_units, int(s.x), int(s.y), tint)})
		elif s.kind == "sprite":
			var resource := str(s.resource)
			var lump := "bust.lmp"
			var name := str(state.get("bust_texture", "")) if resource == "bust-portrait" else resource
			add_item({"layer": 6, "kind": "sprite", "resource": name, "lump": lump, "x": int(s.x), "y": int(s.y), "clipped": false, "clip": {}, "tint": (sprite_alpha << 24) | 0x808080, "weave": true})
	drawn.bust += 1

func bytes_float(raw: int) -> float:
	var b := PackedByteArray()
	b.resize(4)
	b.encode_u32(0, raw)
	return b.decode_float(0)

func colour_of(tint: int) -> Color:
	return Color(float(tint & 255) / 128.0, float((tint >> 8) & 255) / 128.0, float((tint >> 16) & 255) / 128.0, float((tint >> 24) & 255) / 128.0)

# Painter rule (hud-dynamic.md 1): layers 0..7, and inside a layer reverse
# submission order. help_task (50) submits before HUD Draw (51).
func painted() -> Array:
	var out := items.duplicate()
	out.sort_custom(func(a, b): return a.layer < b.layer or (a.layer == b.layer and a.order > b.order))
	return out

func _draw() -> void:
	if items.is_empty(): return
	var factor := minf(size.x / 640.0, size.y / 480.0)
	var offset := Vector2((size.x - 640.0 * factor) * 0.5, (size.y - 480.0 * factor) * 0.5)
	draw_set_transform(offset, 0, Vector2(factor, factor * 480.0 / 448.0))
	for item in painted():
		if item.kind == "sprite": draw_sprite(item)
		else: draw_glyphs(item)

func draw_sprite(item: Dictionary) -> void:
	var a := load_art(item.lump, item.resource, item.weave)
	if a.is_empty(): return
	var original := Rect2(item.x, item.y, a.width, a.height)
	if not item.clipped: original.position.y += 1
	var dest := original
	if item.clipped:
		var c: Dictionary = item.clip
		dest = dest.intersection(Rect2(c.left, c.top, c.right - c.left, mini(447, c.bottom) - c.top))
	if not dest.has_area(): return
	draw_texture_rect_region(a.texture, dest, Rect2(dest.position - original.position, dest.size), colour_of(item.tint), false)

func draw_glyphs(item: Dictionary) -> void:
	var f: Dictionary = fonts.get(item.font, {})
	if f.is_empty(): return
	var page: Vector2i = f.size
	for g in item.glyphs:
		var sw := int(g.source_width)
		var sh := int(g.source_height)
		var sx := int(g.source_x)
		var sy := int(g.source_y)
		if sw <= 0 or sh <= 0 or sx < 0 or sy < 0 or sx + sw > page.x or sy + sh > page.y: continue
		draw_texture_rect_region(f.texture, Rect2(float(g.x_half) * 0.5, float(g.y), sw, sh), Rect2(sx, sy, sw, sh), colour_of(int(g.tint)), false)
