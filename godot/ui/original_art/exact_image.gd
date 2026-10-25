extends Control

# Displays one native-shaped RGBA image at 1x or an integer multiple. For the
# standalone diagnostic bar, clipping is an authored host rule only.
var source_image: Image
var source_texture: ImageTexture
var integer_scale := 1
var fill_ratio := 1.0

func set_art(image: Image, scale: int = 1) -> void:
	source_image = image
	source_texture = ImageTexture.create_from_image(image) if image != null else null
	integer_scale = clampi(scale, 1, 8)
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	custom_minimum_size = Vector2(image.get_size() * integer_scale) if image != null else Vector2.ZERO
	size = custom_minimum_size
	queue_redraw()

func set_integer_scale(scale: int) -> void:
	integer_scale = clampi(scale, 1, 8)
	if source_image != null:
		custom_minimum_size = Vector2(source_image.get_size() * integer_scale)
		size = custom_minimum_size
	queue_redraw()

func set_fill_ratio(ratio: float) -> void:
	fill_ratio = clampf(ratio, 0.0, 1.0) if is_finite(ratio) else 0.0
	queue_redraw()

func _draw() -> void:
	if source_texture == null:
		return
	var width := int(floor(source_image.get_width() * fill_ratio))
	if width < 1:
		return
	var height := source_image.get_height()
	draw_texture_rect_region(source_texture, Rect2(0, 0, width * integer_scale, height * integer_scale), Rect2(0, 0, width, height), Color.WHITE, false)
