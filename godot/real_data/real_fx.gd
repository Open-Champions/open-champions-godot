extends "res://fx/playable_fx.gd"

# After every released reset, replace the synthetic sheets with the particle,
# trail, floor and glow textures the native bridge decodes from the user's
# engine.lmp / fx.lmp. Lighting stays the released labelled synthetic set.
var sheet_archives := PackedStringArray()
var sheet_result: Dictionary = {}

func reset() -> void:
	super.reset()
	var paths := PackedStringArray()
	for path in sheet_archives:
		if not path.is_empty(): paths.append(path)
	if paths.is_empty(): return
	sheet_result = renderer.load_textures(paths)

func status_text() -> String:
	var sheets := "game-folder sheets" if sheet_result.get("ok", false) else "synthetic sheets (%d missing)" % sheet_result.get("missing", []).size()
	return super.status_text().replace("synthetic test sheets", sheets)
