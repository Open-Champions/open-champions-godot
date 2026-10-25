extends Control

# Authored inspection grid, not original inventory ownership/equipment slots.
var entries: Array = []
func _ready() -> void:
	custom_minimum_size = Vector2(286, 290)
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
func _draw() -> void:
	for x in range(9): draw_line(Vector2(15 + x * 32, 6), Vector2(15 + x * 32, 278), Color(0.22, 0.28, 0.3))
	for y in range(9): draw_line(Vector2(15, 6 + y * 34), Vector2(271, 6 + y * 34), Color(0.22, 0.28, 0.3))
	for entry in entries:
		draw_texture(entry.texture, Vector2(entry.placement.x, entry.placement.y))
