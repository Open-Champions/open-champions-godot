extends Control

# The NPC dialog box (docs/DIALOG.md). The native controller owns the dialog
# machine, its draw calls and the gamefontmed glyph layout; this view only
# paints the 640x448 logical canvas: box.tex at (35, 280) plus the one row
# ui_drawSprite adds, then the text glyphs with their scissor. Godot blending
# is presentation, not PS2 framebuffer parity.
var controller: RefCounted
var layout_service: RefCounted
var font_texture: ImageTexture
var font_size := Vector2i.ZERO
var box_texture: ImageTexture
var frame: Dictionary = {}
var refusals: PackedStringArray = []
# The play scene; its `audio` presenter owns the ChampionsNativeAudio bridge.
var audio_owner: Node
var voice_calls := 0

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	if ClassDB.class_exists("ChampionsUiLayoutService"):
		layout_service = ClassDB.instantiate("ChampionsUiLayoutService")

func configure(owner_controller: RefCounted) -> void:
	controller = owner_controller

# gamefontmed.fnt bytes from the game folder.
func set_font(fnt: PackedByteArray) -> bool:
	font_texture = null
	if layout_service == null or fnt.is_empty(): return false
	var decoded: Dictionary = layout_service.decode_hud_font(fnt, true)
	if not decoded.get("ok", false): return false
	var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
	font_texture = ImageTexture.create_from_image(image)
	font_size = image.get_size()
	return true

# box.tex of dlgbox.lmp, decoded by the archive inspector.
func set_box(decoded: Dictionary) -> bool:
	box_texture = null
	if not decoded.get("ok", false): return false
	var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
	box_texture = ImageTexture.create_from_image(image)
	return true

func _process(_delta: float) -> void:
	if controller == null or not is_visible_in_tree(): return
	var status: Dictionary = controller.dialog_status()
	if not status.get("ok", false):
		if not frame.is_empty():
			frame = {}
			queue_redraw()
		return
	_forward_voice()
	# One dialog_drawTask per displayed frame, as the original's draw pass.
	frame = controller.dialog_frame(1)
	refusals = PackedStringArray(frame.get("refusals", []))
	queue_redraw()

# The dialog's stream-1 calls go to the native mixer; its stream-1 state comes
# back for the run task (docs/dialog.md 4 step 4 and 5.1 step 6).
func _forward_voice() -> void:
	var bridge = null
	if audio_owner != null and audio_owner.get("audio") != null:
		bridge = audio_owner.audio.get("bridge")
	for event in controller.dialog_events():
		if bridge == null: continue
		var args: Array = event.get("args", [])
		match str(event.get("kind", "")):
			"stream_play":
				if args.size() >= 3 and int(args[0]) == 1:
					# A line with no entry queues nothing (stream_playLevel returns).
					if bridge.queue_level_entry(1, str(args[1]), int(args[2])): voice_calls += 1
			"stream_stop":
				if args.size() >= 1 and int(args[0]) == 1: bridge.stream_stop(1)
			"clear_hold":
				if args.size() >= 1 and int(args[0]) == 1: bridge.release_stream(1)
	if bridge == null: return
	var observed: Dictionary = bridge.get_observation()
	var streams: Array = observed.get("streams", [])
	if streams.size() > 1:
		var s: Dictionary = streams[1]
		controller.dialog_voice_state({"queue_count": int(s.get("queued", 0)), "status": int(s.get("status", 0)), "position": int(s.get("entry_position", 0))})

func is_open() -> bool:
	return frame.get("open", false)

func _colour(tint: int) -> Color:
	return Color(float(tint & 255) / 128.0, float((tint >> 8) & 255) / 128.0, float((tint >> 16) & 255) / 128.0, float((tint >> 24) & 255) / 128.0)

func _draw() -> void:
	if not frame.get("ok", false): return
	var factor := minf(size.x / 640.0, size.y / 448.0)
	var origin := Vector2((size.x - 640.0 * factor) * 0.5, (size.y - 448.0 * factor) * 0.5)
	draw_set_transform(origin, 0, Vector2(factor, factor))
	# Layer 5 (the box) is under layer 6 (all dialog text).
	for sprite in frame.get("sprites", []):
		if box_texture == null: continue
		var w := mini(int(sprite.width), box_texture.get_width())
		var h := mini(int(sprite.height), box_texture.get_height())
		draw_texture_rect_region(box_texture, Rect2(float(sprite.x), float(sprite.y) + 1.0, w, h), Rect2(0, 0, w, h), _colour(int(sprite.tint)), false)
	if font_texture == null: return
	for glyph in frame.get("glyphs", []):
		var sw := int(glyph.source_width)
		var sh := int(glyph.source_height)
		var sx := int(glyph.source_x)
		var sy := int(glyph.source_y)
		if sw <= 0 or sh <= 0 or sx < 0 or sy < 0 or sx + sw > font_size.x or sy + sh > font_size.y: continue
		var dest := Rect2(float(glyph.x_half) * 0.5, float(glyph.y), sw, sh)
		var clip: Rect2i = glyph.clip
		var clipped := dest.intersection(Rect2(clip))
		if not clipped.has_area(): continue
		var region := Rect2(Vector2(sx, sy) + (clipped.position - dest.position), clipped.size)
		draw_texture_rect_region(font_texture, clipped, region, _colour(int(glyph.tint)), false)
