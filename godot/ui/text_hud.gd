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
const FONT_WORDS := {0x4f7100: "gamefontmed.fnt", 0x4f7104: "gamefontsmall.fnt"}
const BUST_TEXT_POINTER := 0x86aab0
const COMPASS_LABEL_POINTER := 0x86aaf0
const OBSERVED_LAYOUT_BYTE := 1 # 0x41b69c holds 1 in the executable's data
# Elements that are not drawn, and why (each needs evidence research lacks).
# docs/UI_TEXT_GAPS.md: research t3/research-ui-text-gaps settled the help box
# frame, glyph and label, the bust meter, font and wraps; what stays refused:
const REFUSED := {
	"bust_bar_frame": "g_simFrame (0x44fdcc) is unavailable from the host: the animated meter bar is omitted",
	"compass_mesh": "the host has not supplied the 0x4f8424 gate, player slot, and player object: the mesh and label are refused. The mesh also needs the game camera's view matrix and model path; the second pass's light state is Unknown (ui-text-gaps.md 13)",
	"button_layout_other": "a layout byte other than the observed 1 is inferred or unwritten (ui-text-gaps.md 7): its glyph is not drawn",
	"help_notice_boxes": "the notice-box half of help_task (0x1dcff4 onward) was not re-executed (ui-text-gaps.md 13)",
	"chime_slot_life": "the chime slot frees on its second update (Inferred): the sound cursor of anim_advance is refused by the native runtime, so SoundSeqs slots are not freed",
}
# Presentation choices research does not settle; drawn, and reported as such.
const UNVERIFIED := {
	"bust_latch": "the host exposes Hud::tick_message's latch; hud_spellSelect is not modelled",
	"help_display_list": "the display-list free-units gate is not modelled; the native help plan assumes at least 3000 units free",
	"label_text": "the Affirmation label is the peeked text of the key; the original looks it up twice per frame and advances the miss ring only on a miss",
	"ui_fade_initial": "the data value of g_uiFadeIn is not documented: the host starts at 1.0",
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
var bust_fade_raw := 0
var pulse := 1.0
var pulse_raw := 0x3f800000
var ramps_driven := false
var items: Array = [] # {layer, order, kind, ...} in submission order
var drawn := {"message": 0, "help": 0, "bust": 0, "compass": 0}
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
	# Session step 13 is stepped here, once per displayed frame (60 Hz).
	if controller != null and controller.has_method("set_ui_frame_driver"):
		controller.set_ui_frame_driver(true)
		ramps_driven = true
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
	# The frame draws with g_uiPulse as it stands; step 13 runs after the draw.
	var ramps: Dictionary = controller.ui_frame()
	if ramps.get("ok", false):
		pulse_raw = int(ramps.pulse[0])
		pulse = bytes_float(pulse_raw)
	if state.get("help_open", false): build_help()
	build_message()
	build_bust()
	build_compass()

func font(name := "gamefontmed.fnt") -> Dictionary:
	return fonts.get(name, {})

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

func wrapped(f: Dictionary, units: PackedInt32Array, x: int, y: int, width: int, top: int, bottom: int, line: int, tint: int, centered := false) -> Dictionary:
	var r: Dictionary = dynamic_service.text_layout(f.bytes, units, x, y, units.size(), true, width, top, bottom, line, centered, 0, 100000, tint, -1, -1)
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

# docs/ui-text-gaps.md 2 (research t3/research-ui-text-gaps, executed): the draw
# calls of help_task come from the native plan; this lays out their text and
# frames. The title, body and label are measured here because the plan needs the
# wrap height and the label width the original's text calls return.
func build_help() -> void:
	var f := font()
	if f.is_empty() or round3 == null: return
	var title: PackedInt32Array = state.get("help_title", PackedInt32Array())
	var body: PackedInt32Array = state.get("help_body", PackedInt32Array())
	var has_title: bool = state.get("help_has_title", false)
	var has_body := body.size() > 0
	var wrap_height := 0
	if has_body: wrap_height = int(wrapped(f, body, 132, 130, 404, 0, 448, 21, 0x80808080).height)
	var label: PackedInt32Array = controller.text_key_units("Affirmation")
	var label_width := text_width(f, label)
	var glyph: Dictionary = round3.confirm_glyph(OBSERVED_LAYOUT_BYTE)
	if not glyph.get("ok", false): return
	var plan: Dictionary = round3.help_plan(int(state.get("help_close_delay", 0)), pulse_raw, int(glyph.glyph), wrap_height, label_width, has_title, has_body)
	if not plan.get("ok", false): return
	interpret(plan.calls, {1: title, 2: body, "label": label})
	drawn.help += 1

# One plan: calls of the original's text and frame functions, in order, mapped to
# draw items. Pointers are the plan's stand-ins (see `texts`); a call this client
# has no drawing for is skipped and counted in `skipped`.
var skipped := {}
func interpret(calls: Array, texts: Dictionary, resource_names := {}) -> void:
	var f := font()
	var layer := 6
	var colour := 0
	for c in calls:
		var a: Array = c.args
		match str(c.name):
			"text_begin":
				f = font(str(FONT_WORDS.get(int(a[0]), "gamefontmed.fnt")))
				layer = int(a[1])
				colour = int(a[2])
			"text_setColor":
				colour = int(a[0])
			"text_drawWideCentered":
				add_item({"layer": layer, "kind": "text", "font": font_name(f), "glyphs": centred(f, texts.get(int(a[2]), PackedInt32Array()), s32(a[0]), s32(a[1]), colour)})
			"text_drawWrapped", "text_drawWrappedCentered":
				var units: PackedInt32Array = texts.get(int(a[2]), PackedInt32Array())
				var centered := str(c.name) == "text_drawWrappedCentered"
				add_item({"layer": layer, "kind": "text", "font": font_name(f), "glyphs": wrapped(f, units, s32(a[0]), s32(a[1]), int(a[3]), s32(a[4]), s32(a[5]), int(a[6]), colour, centered).glyphs})
			"text_drawAscii", "text_drawAsciiCentered":
				var codes := PackedInt32Array()
				for ch in str(a[2]).to_utf8_buffer(): codes.append(ch)
				if str(c.name) == "text_drawAscii":
					# sprintf("%c", word): the plan's string is one raw byte, escaped when above 0x7f.
					add_item({"layer": layer, "kind": "text", "font": font_name(f), "glyphs": glyphs(f, codes, s32(a[0]), s32(a[1]), colour)})
				else:
					add_item({"layer": layer, "kind": "text", "font": font_name(f), "glyphs": centred(f, codes, s32(a[0]), s32(a[1]), colour)})
			"text_drawWide":
				add_item({"layer": layer, "kind": "text", "font": font_name(f), "glyphs": glyphs(f, texts.get(str(a[2]), PackedInt32Array()), s32(a[0]), s32(a[1]), colour)})
			"ui_drawFrameImpl":
				frame_items(s32(a[0]), s32(a[1]), s32(a[2]), s32(a[3]), int(a[6]), int(a[4]), int(a[5]))
			"sprite":
				var name := str(a[0])
				if resource_names.has(name): name = str(resource_names[name])
				add_item({"layer": int(a[3]), "kind": "sprite", "resource": name, "lump": "bust.lmp", "x": s32(a[1]), "y": s32(a[2]), "clipped": false, "clip": {}, "tint": int(a[4]), "weave": true})
			"sprite_clipped":
				var bar := str(a[0])
				add_item({"layer": int(a[3]), "kind": "sprite", "resource": bar, "lump": "bust.lmp", "x": s32(a[1]), "y": s32(a[2]), "clipped": true, "clip": {"left": s32(a[5]), "top": s32(a[6]), "right": s32(a[7]), "bottom": s32(a[8])}, "tint": int(a[4]), "weave": true})
			"text_end", "text_lookup", "text_widthWide", "res_find", "dialog_isOpen", "light_resetObjectLight", "model_draw":
				pass
			_:
				skipped[str(c.name)] = int(skipped.get(str(c.name), 0)) + 1

func s32(v) -> int:
	var n := int(v)
	return n - 0x100000000 if n >= 0x80000000 else n

func font_name(f: Dictionary) -> String:
	for key in fonts.keys():
		if fonts[key] == f: return key
	return "gamefontmed.fnt"

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

# docs/ui-text-gaps.md 3 (executed): hud_drawBust for both branches. The caption
# is gamefontsmall, one wrap per branch, the meter's clip right is
# 521 + round(value / max * bar width) - 2 on the run-time maximum, sprites are
# neutral RGB 0x808080 with only the fade in alpha, and the art stays woven.
const BUST_BARS := ["bar_yellow00.tex", "bar_yellow01.tex", "bar_yellow02.tex", "bar_yellow03.tex"]
func build_bust() -> void:
	if int(state.get("bust_shown", 0)) <= 0 or round3 == null: return
	var maximum := int(state.get("bust_max", -1))
	var texture := str(state.get("bust_texture", ""))
	var sizes := {}
	var has_sim_frame := state.has("sim_frame")
	var needed := ["frame.tex", texture, "fieldnumber.tex"] if maximum == -1 else ["frame.tex", texture, "fieldmeter.tex"] + (BUST_BARS if has_sim_frame else [])
	for name in needed:
		var a := load_art("bust.lmp", name, true)
		if a.is_empty(): return
		sizes[name if name != texture else "<portrait>"] = [int(a.width), int(a.height)]
	if not state.has("latch"):
		last_error = "bust latch unavailable"
		return
	var plan: Dictionary = round3.bust_plan(int(state.latch), bust_fade_raw, int(state.get("bust_value", 0)), maximum, int(state.get("sim_frame", 0)), has_sim_frame, sizes)
	if not plan.get("ok", false):
		last_error = str(plan.get("error", ""))
		return
	bust_fade_raw = int(plan.fade)
	var texts := {BUST_TEXT_POINTER: state.get("bust_text", PackedInt32Array())}
	interpret(plan.calls, texts, {"<portrait>": texture})
	drawn.bust += 1

# docs/ui-text-gaps.md 5: the label plan of hud_drawCompass. The host must
# supply all three researched gates before any compass draw is allowed.
func build_compass() -> void:
	if int(state.get("compass_shown", 0)) <= 0 or round3 == null: return
	if not (state.has("compass_gate_word") and state.has("compass_player_slot") and state.has("compass_player_object")): return
	if int(state.compass_gate_word) == 0 or int(state.compass_player_slot) == -1 or not bool(state.compass_player_object): return
	var plan: Dictionary = round3.compass_plan(bool(state.get("dialog_open", false)), true)
	if not plan.get("ok", false) or plan.calls.is_empty(): return
	interpret(plan.calls, {COMPASS_LABEL_POINTER: state.get("compass_label", PackedInt32Array())})
	drawn["compass"] = int(drawn.get("compass", 0)) + 1

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
