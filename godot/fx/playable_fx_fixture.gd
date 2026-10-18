extends RefCounted

# Authored practice/test resources only. The address is the released spawn-0
# recipe's symbolic definition key, not retail definition values.
static func definitions() -> String:
	var rows: Array = JSON.parse_string(preload("res://tests/native_fx_smoke.gd").SYNTHETIC)
	rows[0]["def"] = 0x8704a8
	return JSON.stringify(rows)

static func configure(renderer: Node3D) -> void:
	var sheet := Image.create_empty(512, 256, false, Image.FORMAT_RGBA8)
	sheet.fill(Color.WHITE)
	renderer.particle_sheet = ImageTexture.create_from_image(sheet)
	renderer.particle_sheet_height = sheet.get_height()
	var floor_image := Image.create_empty(256, 128, false, Image.FORMAT_RGBA8)
	floor_image.fill(Color(0.55, 0.06, 0.04, 0.85))
	renderer.floor_sprite_sheet = ImageTexture.create_from_image(floor_image)
