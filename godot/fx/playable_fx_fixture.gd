extends RefCounted

# Authored practice/test resources only. Addresses are symbolic definition keys
# the released code uses (spawn-0 burst 0x8704a8; Holy Strike's three puffs,
# combat_runtime_spells.cpp HOLYSTRIKE_DEFS); every value is synthetic.
const HOLY_STRIKE_PUFFS := [0x8e61d0, 0x8e6220, 0x8e6270]
# The labelled synthetic stand-in for fx_skill_recipes.py --json (native test
# fixture; no reference constant appears in it). A user's JSON replaces it.
const RECIPE_CONSTANTS := "res://fx/fx_skill_recipes_synthetic.json"
const GLOW_TEXTURES := ["halo1.tex", "halo2.tex", "halo3.tex", "halo4.tex", "wisphalo.tex", "halo5.tex", "halo7.tex", "halo8.tex"]

static func definitions() -> String:
	var rows: Array = JSON.parse_string(preload("res://tests/native_fx_smoke.gd").SYNTHETIC)
	var burst: Dictionary = rows[0]
	burst["def"] = 0x8704a8
	var out: Array = [burst]
	var site := 2
	for address in HOLY_STRIKE_PUFFS:
		var puff: Dictionary = burst.duplicate(true)
		puff["site"] = site
		puff["def"] = address
		puff["start_rgba"] = [128, 120, 60, 128]
		puff["end_rgba"] = [96, 80, 20, 0]
		out.append(puff)
		site += 1
	return JSON.stringify(out)

static func recipe_constants() -> String:
	return FileAccess.get_file_as_string(RECIPE_CONSTANTS)

static func image(width: int, height: int, color: Color, radial := false) -> Texture2D:
	var picture := Image.create_empty(width, height, false, Image.FORMAT_RGBA8)
	picture.fill(color)
	if radial:
		var centre := Vector2(width, height) * 0.5
		for y in height:
			for x in width:
				var falloff := clampf(1.0 - Vector2(x + 0.5, y + 0.5).distance_to(centre) / centre.x, 0.0, 1.0)
				picture.set_pixel(x, y, Color(color.r, color.g, color.b, color.a * falloff))
	return ImageTexture.create_from_image(picture)

# Generated test sheets at the documented sizes (not extracted textures).
static func configure(renderer: Node3D) -> void:
	renderer.textures["particletex.tex"] = image(512, 160, Color.WHITE)
	renderer.textures["trail.tex"] = image(128, 448, Color(1, 1, 1, 0.75))
	renderer.textures["floorsprite.tex"] = image(256, 128, Color(0.55, 0.06, 0.04, 0.85))
	for name in GLOW_TEXTURES:
		renderer.textures[name] = image(32, 32, Color(1.0, 0.95, 0.7, 1.0), true)
		renderer.bridge.set_glow_texture_size(name, 32, 32)
