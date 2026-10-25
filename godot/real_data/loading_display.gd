extends Control

# RTA US v2.00 loading presentation. All images come from the chosen disc folder.
# Coordinates and frame steps are the executed 640x448 UI contract in
# docs/loading-transitions.md (research-loading 3ba61c7).
const DESIGN := Vector2(640, 448)
const SHOT_RECT := Rect2(0, 100, 640, 320)
const TITLE_POS := Vector2(228, 185)
const FADE_PRESENTS := 32
const WARMUP_PRESENTS := 10
const BLANK_FIELDS := 6
const START_TINT := 0x80606060
const NORMAL_TINT := 0x80808080
const STRIP_TINT := 0x14808080
const TITLE_NAMES := {1: "titleloadsp.tex", 2: "titleloadit.tex", 3: "titleloadfr.tex", 4: "titleloadgr.tex"}

var textures := {}
var selected_lump := ""
var loading_frame := 0
var fade_present := 0
var phase := "hidden"
var refusal := ""

func _ready() -> void:
	if DisplayServer.get_name() != "headless":
		RenderingServer.frame_post_draw.connect(_advance_present)

static func alias(name: String) -> String:
	if "blo" in name: return "blood1"
	if name == "innovat2": return "innova2"
	if "val" in name: return "valor"
	if "bas" in name: return "bastion1"
	if name == "nitdung1": return "nitdun1"
	if name == "nitdung2": return "nitdun2"
	if name == "nitdung4": return "nitdun4"
	if name == "tormenta": return "torma"
	if name == "tormentb": return "tormb"
	if name == "torment2": return "torm2"
	if name == "torment3": return "torm3"
	if name == "pvpfire2": return "pvpfir2"
	return name

static func candidate(name: String) -> String:
	return ("l" + alias(name)).substr(0, 8) + ".lmp"

static func select_lump(name: String, available: Dictionary) -> String:
	var wanted := candidate(name)
	return wanted if available.has(wanted) else "lmiss.lmp"

static func fade_channel(value: int) -> int:
	if value >= 133: return value - 4
	if value < 124: return value + 4
	return 128

static func fade_value(presents: int) -> int:
	var value := 0
	for i in range(maxi(presents, 0)): value = fade_channel(value)
	return value

static func strip_positions(frame: int) -> Vector2i:
	var x := frame % 640
	return Vector2i(x, x - 640)

static func warmup_remaining(remaining: int, terrain_busy: bool,
		equipment_pending: bool, displayed: int) -> int:
	if displayed >= 1500: return 0
	if terrain_busy: return WARMUP_PRESENTS
	var next := maxi(remaining - 1, 0)
	return maxi(next, 3) if equipment_pending else next

# ui_drawSpriteImpl's TFX modulate and alpha use 0x80 as neutral. This is
# the same sprite adapter used by original_art/hud_view.gd, not Color8 /255.
static func sprite_tint(word: int) -> Color:
	return Color(float(word & 255) / 128.0, float((word >> 8) & 255) / 128.0,
		float((word >> 16) & 255) / 128.0, float((word >> 24) & 255) / 128.0)

static func submissions(which: String, frame: int, title_name: String) -> Array[Dictionary]:
	var records: Array[Dictionary] = []
	if which == "start":
		for name in [title_name, "shot.tex", "scrollbar.tex", "scrollbar2.tex", "scrollbar3.tex"]:
			records.append({"name": name, "position": Vector2.ZERO, "layer": 6, "tint": START_TINT})
	elif which == "loading":
		records.append({"name": "shot.tex", "position": SHOT_RECT.position, "layer": 6, "tint": NORMAL_TINT})
		records.append({"name": title_name, "position": TITLE_POS, "layer": 6, "tint": NORMAL_TINT})
		var positions := strip_positions(frame)
		for strip in [["scrollbar2.tex", 0], ["scrollbar3.tex", 352]]:
			for x in [positions.x, positions.y]:
				records.append({"name": strip[0], "position": Vector2(x, strip[1]),
					"layer": 6, "tint": STRIP_TINT})
	return records

static func painter_records(records: Array[Dictionary]) -> Array[Dictionary]:
	# The UI packet list prepends each record within its layer. These retained
	# loading records all use layer 6, so the GS consumes them in reverse.
	var out: Array[Dictionary] = []
	for index in range(records.size() - 1, -1, -1): out.append(records[index])
	return out

func configure(real: Node, inspector: Object, level_name: String, language := 0) -> bool:
	textures.clear()
	refusal = ""
	selected_lump = select_lump(level_name, real.files)
	var shot_path: String = real.files.get(selected_lump, "")
	var misc_path: String = real.files.get("loadmisc.lmp", "")
	if shot_path.is_empty() or misc_path.is_empty():
		refusal = "Loading art lump unavailable."
		return false
	var title_name: String = TITLE_NAMES.get(language, "titleload.tex")
	var title_size: Vector2i = {"titleloadsp.tex": Vector2i(216, 72),
		"titleloadit.tex": Vector2i(382, 90), "titleloadfr.tex": Vector2i(244, 70),
		"titleloadgr.tex": Vector2i(160, 72), "titleload.tex": Vector2i(184, 78)}[title_name]
	for spec in [[shot_path, "shot.tex", Vector2i(640, 320)], [misc_path, title_name, title_size],
			[misc_path, "scrollbar.tex", Vector2i(640, 128)],
			[misc_path, "scrollbar2.tex", Vector2i(640, 96)],
			[misc_path, "scrollbar3.tex", Vector2i(640, 96)]]:
		var listing: Dictionary = inspector.inspect_archive(spec[0])
		if not listing.get("ok", false):
			refusal = "Loading art archive refused."
			return false
		var rows: Array = listing.get("rows", [])
		var found := false
		for i in range(rows.size()):
			if str(rows[i].get("name", "")).to_lower() != spec[1]: continue
			var decoded: Dictionary = inspector.inspect_texture(spec[0], i, "rta", str(listing.get("fingerprint", "")))
			if not decoded.get("ok", false): break
			var width := int(decoded.get("width", 0))
			var height := int(decoded.get("height", 0))
			var rgba: PackedByteArray = decoded.get("rgba", PackedByteArray())
			if Vector2i(width, height) != spec[2] or rgba.size() != width * height * 4: break
			var image := Image.create_from_data(width, height, false, Image.FORMAT_RGBA8, rgba)
			textures[spec[1]] = ImageTexture.create_from_image(image)
			found = true
			break
		if not found:
			refusal = "Loading art texture refused."
			return false
	loading_frame = 0
	fade_present = 0
	phase = "start"
	visible = true
	queue_redraw()
	return true

func _process(_delta: float) -> void:
	if DisplayServer.get_name() != "headless": return
	_advance_present()

func _advance_present() -> void:
	if phase == "hidden": return
	if phase == "loading": loading_frame += 1
	elif phase == "fade":
		fade_present += 1
		if fade_present > FADE_PRESENTS:
			phase = "hidden"
			visible = false
	queue_redraw()

func begin_fade() -> void:
	phase = "fade"
	fade_present = 0
	visible = true
	queue_redraw()

func _draw() -> void:
	if phase == "hidden": return
	var scale_factor := minf(size.x / DESIGN.x, size.y / DESIGN.y)
	if scale_factor <= 0: return
	var origin := (size - DESIGN * scale_factor) * 0.5
	if phase != "fade": draw_rect(Rect2(Vector2.ZERO, size), Color.BLACK)
	if phase == "blank": return
	draw_set_transform(origin, 0.0, Vector2.ONE * scale_factor)
	if phase == "fade":
		# gfx_present uses the pre-step multiplier. Present 33 first uses 128.
		var c := float(fade_value(fade_present)) / 128.0
		draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)
		draw_rect(Rect2(Vector2.ZERO, size), Color(0, 0, 0, 1.0 - c))
		return
	var title_name: String = "titleload.tex"
	for name in TITLE_NAMES.values():
		if textures.has(name): title_name = name
	for record in painter_records(submissions(phase, loading_frame, title_name)):
		if textures.has(record.name):
			draw_texture(textures[record.name], record.position, sprite_tint(record.tint))
