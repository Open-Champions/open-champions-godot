extends Control

# RTA US v2.00 missionMenu_draw, 640x448. All pixels and glyphs come from the
# owner-selected disc at run time. The native campaign supplies the table and
# state. Moving background rectangles, scroll interpolation, localized glyph
# placement and GPU blend parity are not established by the function probe.
const SIZES := {
	"frame.tex": Vector2i(32, 128), "cosmiclines.tex": Vector2i(640, 448),
	"starfield.tex": Vector2i(640, 448), "starsparkle.tex": Vector2i(640, 448),
	"misswar.tex": Vector2i(411, 156), "missinnovation.tex": Vector2i(411, 156),
	"misspit.tex": Vector2i(411, 156), "misswater.tex": Vector2i(411, 156),
	"misstorment.tex": Vector2i(411, 156), "missdisease.tex": Vector2i(411, 156),
	"missvalor.tex": Vector2i(411, 156), "missfire.tex": Vector2i(411, 156),
	"missstorms.tex": Vector2i(411, 156), "missfeydark.tex": Vector2i(411, 156),
	"missnightmare.tex": Vector2i(411, 156), "missfear.tex": Vector2i(411, 156),
	"missairend.tex": Vector2i(411, 156), "missair.tex": Vector2i(411, 156),
	"pipe1.tex": Vector2i(249, 74), "pipe2.tex": Vector2i(249, 74),
	"pipe3.tex": Vector2i(249, 74), "pipe4.tex": Vector2i(91, 250),
	"pipe5.tex": Vector2i(324, 16), "pipe6.tex": Vector2i(84, 228),
	"shard.tex": Vector2i(35, 34), "amulet1.tex": Vector2i(53, 52),
	"amulet2.tex": Vector2i(53, 52), "amulet3.tex": Vector2i(53, 52),
	"bonus.tex": Vector2i(33, 34), "mainglow.tex": Vector2i(256, 146),
	"amuletglow.tex": Vector2i(88, 88), "bonusglow.tex": Vector2i(64, 62),
}
const BRIGHT := 0x7f808080
const DARK := 0x7f3c3c3c
const PROMPT := 0x64464646

var real: Node
var controller: Object
var inspector: Object
var layout: RefCounted
var art := {} # name -> ImageTexture
var font_bytes := PackedByteArray()
var font_texture: ImageTexture
var state := {}
var last_error := ""
var assets_valid := false
var frames_drawn := 0
var submitted_sprites: Array[String] = []
var submitted_tints := {}
var submitted_text_keys: Array[String] = []

func _ready() -> void:
	mouse_filter = MOUSE_FILTER_STOP
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	if ClassDB.class_exists("ChampionsUiDynamicLayoutService"):
		layout = ClassDB.instantiate("ChampionsUiDynamicLayoutService")

func configure(source: Node, owner: Object, archive_inspector: Object, small_font: PackedByteArray) -> bool:
	real = source
	controller = owner
	inspector = archive_inspector
	art.clear()
	font_bytes = small_font
	font_texture = null
	assets_valid = false
	last_error = ""
	if real == null or inspector == null or not real.files.has("msel.lmp"):
		last_error = "MSEL.LMP is absent from the selected game folder."
		return false
	var path: String = real.files["msel.lmp"]
	var listing: Dictionary = inspector.inspect_ui_candidates(path, "rta")
	if not listing.get("ok", false):
		last_error = "MSEL.LMP could not be inspected."
		return false
	var names := {}
	for row in listing.get("rows", []):
		if str(row.get("name", "")).to_lower().ends_with(".tex"):
			names[str(row.name).to_lower()] = row
	if names.size() != SIZES.size():
		last_error = "MSEL.LMP does not have the documented 32 textures."
		return false
	for name in SIZES:
		if not names.has(name):
			last_error = "MSEL.LMP texture set differs from the documented revision."
			return false
		var row: Dictionary = names[name]
		var decoded: Dictionary = inspector.inspect_texture(path, int(row.row_index), "rta", listing.fingerprint)
		if not decoded.get("ok", false) or Vector2i(int(decoded.get("width", 0)), int(decoded.get("height", 0))) != SIZES[name]:
			last_error = "MSEL.LMP texture dimensions or decoding differ from the documented revision."
			return false
		var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
		art[name] = ImageTexture.create_from_image(image)
	if layout == null or font_bytes.is_empty():
		last_error = "The documented gamefontsmall.fnt is unavailable."
		return false
	var decoded_font: Dictionary = ClassDB.instantiate("ChampionsUiLayoutService").decode_hud_font(font_bytes, true)
	if not decoded_font.get("ok", false):
		last_error = "gamefontsmall.fnt could not be decoded."
		return false
	var page := Image.create_from_data(int(decoded_font.width), int(decoded_font.height), false, Image.FORMAT_RGBA8, decoded_font.rgba)
	font_texture = ImageTexture.create_from_image(page)
	assets_valid = true
	queue_redraw()
	return true

func present(next: Dictionary) -> void:
	state = next
	queue_redraw()

func _colour(tint: int) -> Color:
	return Color(float(tint & 255) / 128.0, float((tint >> 8) & 255) / 128.0,
		float((tint >> 16) & 255) / 128.0, float((tint >> 24) & 255) / 128.0)

func _sprite(name: String, x: float, y: float, tint: int, src := Rect2()) -> void:
	var texture: ImageTexture = art.get(name)
	if texture == null: return
	submitted_sprites.append(name)
	submitted_tints[name] = tint
	var region := src if src.has_area() else Rect2(Vector2.ZERO, texture.get_size())
	var dest := Rect2(x, y + 1.0, region.size.x, region.size.y)
	var clipped := dest.intersection(Rect2(0, 0, 640, 448))
	if not clipped.has_area(): return
	region.position += clipped.position - dest.position
	region.size = clipped.size
	draw_texture_rect_region(texture, clipped, region, _colour(tint), false)

func _text(key: String, x: int, y: int, tint: int, width := 0, top := 0, bottom := 448, line := 21) -> void:
	if font_texture == null or controller == null: return
	# The detail call carries zero in its high byte; the font renderer's blend
	# interpretation was not captured; refuse this text.
	if (tint >> 24) == 0: return # Detail alpha and blend are unresolved.
	submitted_text_keys.append(key)
	var render_tint := tint
	var found: Dictionary = controller.text_lookup(key)
	if not found.get("ok", false): return
	var units: PackedInt32Array = found.units
	var run: Dictionary = layout.text_layout(font_bytes, units, x, y, units.size(), true,
		width, top, bottom, line, width > 0, 0, 100000, render_tint, -1, -1)
	if not run.get("ok", false): return
	var drawn: Array = run.get("glyphs", [])
	if width == 0:
		var centred_run: Dictionary = layout.text_layout(font_bytes, units, int(run.centered_x), y, units.size(), true,
			0, 0, 448, 21, false, 0, 100000, render_tint, -1, -1)
		drawn = centred_run.get("glyphs", [])
	else:
		drawn = []
		for part in run.get("runs", []):
			var subset := units.slice(int(part.start), int(part.start) + int(part.count))
			var piece: Dictionary = layout.text_layout(font_bytes, subset, int(part.x), int(part.y), subset.size(), true,
				0, 0, 448, 21, false, 0, 100000, render_tint, -1, -1)
			drawn.append_array(piece.get("glyphs", []))
	for glyph in drawn:
		var source := Rect2(int(glyph.source_x), int(glyph.source_y), int(glyph.source_width), int(glyph.source_height))
		if not source.has_area(): continue
		var dest := Rect2(float(glyph.x_half) * 0.5, float(glyph.y), source.size.x, source.size.y)
		var clipped := dest.intersection(Rect2(25, 230, 590, 215))
		if not clipped.has_area(): continue
		source.position += clipped.position - dest.position
		source.size = clipped.size
		draw_texture_rect_region(font_texture, clipped, source, _colour(render_tint), false)

func _draw() -> void:
	submitted_sprites.clear()
	submitted_tints.clear()
	submitted_text_keys.clear()
	if not assets_valid or not state.get("open", false): return
	var factor := minf(size.x / 640.0, size.y / 448.0)
	draw_set_transform(Vector2((size.x - 640.0 * factor) * 0.5, (size.y - 448.0 * factor) * 0.5), 0, Vector2(factor, factor))
	# Backdrop rectangles and sparkle placement await an executed draw capture.
	var selected := clampi(int(state.get("row", 0)), 0, 11)
	var rows: Array = state.get("rows", [])
	if rows.size() != 12: return
	# Controlled scroll endpoints: the selected node is at y=40.
	for i in range(selected, mini(selected + 3, 12)):
		var row: Dictionary = rows[i]
		var picture := str(row.evil_image) if state.get("evil", false) and not str(row.evil_image).is_empty() else str(row.image)
		_sprite(picture.to_lower(), int(row.x), int(row.y) - selected * 200, BRIGHT if row.draw_present else DARK)
		if selected == 0 and i < 3:
			var link_y := int(row.y) - selected * 200 + 141
			var next: Dictionary = rows[i + 1]
			_sprite("pipe1.tex" if i % 2 == 0 else "pipe2.tex", 186, link_y, BRIGHT if next.draw_present else DARK)
	# The documented row-zero icon placements. Other-row placements
	# are refused until their draw submissions are captured.
	if selected == 0:
		var first: Dictionary = rows[0]
		if first.complete:
			_sprite("shard.tex", 283, 101, BRIGHT)
		if int(first.medal) >= 1:
			_sprite("amulet%d.tex" % mini(int(first.medal), 3), 215, 91, BRIGHT)
		if first.complete:
			_sprite("amulet2.tex", 215, 91, DARK)
		if int(first.medal) >= 3:
			_sprite("bonus.tex", 167, 99, BRIGHT)
		# Glow pulse/fade tint is unresolved; no fixed-tint substitute.
	for strip in range(4):
		_sprite("frame.tex", 0, strip * 128, BRIGHT)
		_sprite("frame.tex", 608, strip * 128, BRIGHT)
	# The original submits the detail frame at (25,230,590,215); its style and
	# raster result were not captured. The detail text blend is also unresolved.
	# Detail glyph alpha/blend is unresolved; keep both calls refused.
	if not state.get("network", false):
		_text("AcceptBack", 320, 410, PROMPT)
	elif state.has("prompt_first_active") and state.has("prompt_paused"):
		if state.prompt_first_active: _text("AcceptBack", 320, 410, PROMPT)
		elif not state.prompt_paused: _text("clientMissionSelect", 320, 410, PROMPT)
	frames_drawn += 1
