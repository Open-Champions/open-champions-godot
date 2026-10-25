extends SceneTree

# Synthetic selection and present-count cases from research-loading's executed
# alias/start/fade slices. No game bytes are part of this test.
const Display = preload("res://real_data/loading_display.gd")
var checks := 0

func expect(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		push_error("Loading display: " + message)
		quit(1)

func _initialize() -> void:
	var art := {"lblood1.lmp": true, "linnova2.lmp": true, "lvalor.lmp": true,
		"lbastion.lmp": true, "lnitdun1.lmp": true, "ltorma.lmp": true,
		"lpvpfir2.lmp": true, "lmiss.lmp": true}
	for row in [["blood1", "lblood1.lmp"], ["blood2", "lblood1.lmp"],
			["innovat2", "linnova2.lmp"], ["valord", "lvalor.lmp"],
			["bastion1", "lbastion.lmp"], ["nitdung1", "lnitdun1.lmp"],
			["tormenta", "ltorma.lmp"], ["pvpfire2", "lpvpfir2.lmp"],
			["unknown", "lmiss.lmp"]]:
		expect(Display.select_lump(row[0], art) == row[1], "art selection " + row[0])
	expect(Display.candidate("BLOOD1") == "lBLOOD1.lmp", "alias comparison is case-sensitive")
	for row in [["nitdung2", "lnitdun2.lmp"], ["nitdung4", "lnitdun4.lmp"],
			["tormentb", "ltormb.lmp"], ["torment2", "ltorm2.lmp"],
			["torment3", "ltorm3.lmp"], ["innovation", "linnovat.lmp"],
			["ordinary", "lordinar.lmp"]]:
		expect(Display.candidate(row[0]) == row[1], "candidate " + row[0])
	expect(Display.select_lump("blood1", {"lmiss.lmp": true}) == "lmiss.lmp", "fallback")
	for count in [0, 1, 31, 32, 33]:
		expect(Display.fade_value(count) == mini(count * 4, 128), "fade present %d" % count)
	expect(Display.fade_channel(123) == 127 and Display.fade_channel(124) == 128 and
		Display.fade_channel(132) == 128 and Display.fade_channel(133) == 129,
		"fade boundary channels")
	expect(Display.strip_positions(639) == Vector2i(639, -1) and
		Display.strip_positions(640) == Vector2i(0, -640), "strip wrap")
	var display := Display.new()
	display.begin_fade()
	expect(display.fade_present == 0 and Display.fade_value(display.fade_present) == 0,
		"first fade present starts black")
	for i in range(Display.FADE_PRESENTS): display._advance_present()
	expect(display.fade_present == 32 and Display.fade_value(display.fade_present) == 128,
		"neutral multiplier follows 32 presents")
	display._advance_present()
	expect(display.phase == "hidden" and display.fade_present == 33,
		"fade ends after the neutral present")
	display.free()
	print("LOADING_CASES %d/%d" % [checks, checks])
	quit(0)
