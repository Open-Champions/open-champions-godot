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

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	custom_minimum_size = Vector2(640, 120)
	if ClassDB.class_exists("ChampionsUiLayoutService"):
		layout_service = ClassDB.instantiate("ChampionsUiLayoutService")
	if ClassDB.class_exists("ChampionsUiDynamicLayoutService"):
		dynamic_service = ClassDB.instantiate("ChampionsUiDynamicLayoutService")

func set_art(art: Dictionary, catalogue: Dictionary) -> void:
	textures.clear()
	for id in art:
		var name: String = catalogue[id].entry
		if name.begins_with("bar_") or name == "hud_statusbar00.tex":
			textures[name] = ImageTexture.create_from_image(art[id].image)
	queue_redraw()

func observe(value: Dictionary) -> void:
	records.clear()
	missing.clear()
	error = ""
	if layout_service == null or dynamic_service == null:
		error = "Native HUD layout service unavailable."
		queue_redraw()
		return
	var health_ok := valid_bar(value, "hp", "max_hp")
	var mana_ok := valid_bar(value, "mana", "max_mana")
	var origins: Dictionary = layout_service.hud_origins(false, PackedInt32Array([0]), 1)
	# Slot zero is this authored offline singleton view. Unknown experience,
	# cooldown and flash gates are not submitted. Neutral inputs only obtain
	# unaffected geometry when a bar is absent; that bar is discarded below.
	var result: Dictionary = layout_service.hud_base_layers(origins.origins[0], int(value.get("ready_ticks", 0)), value.hp if health_ok else 0.0, value.max_hp if health_ok else 1.0, value.mana if mana_ok else 0.0, value.max_mana if mana_ok else 1.0, 0, 0, 1, 0, true, 0, false)
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
	queue_redraw()

func valid_bar(value: Dictionary, current: String, maximum: String) -> bool:
	for key in [current, maximum]:
		if typeof(value.get(key)) not in [TYPE_INT, TYPE_FLOAT] or not is_finite(float(value[key])): return false
	return float(value[current]) >= 0.0 and float(value[maximum]) > 0.0

func _draw() -> void:
	if layout_service == null: return
	var factor := minf(1.0, size.x / 640.0)
	draw_set_transform(Vector2.ZERO, 0, Vector2(factor, factor * 480.0 / 448.0))
	for index in painter_order:
		var record: Dictionary = records[index]
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
