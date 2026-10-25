extends SceneTree

# Synthetic selection and present-count cases from research-loading's executed
# alias/start/fade slices. No game bytes are part of this test.
const Display = preload("res://real_data/loading_display.gd")
const TerrainStream = preload("res://scenes/level_texture_stream.gd")
var checks := 0
var failures := 0

func expect(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("Loading display: " + message)

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
	for frame in range(3):
		var start := Display.submissions("start", frame, "titleload.tex")
		expect(start.size() == 5 and start.map(func(r): return r.name) ==
			["titleload.tex", "shot.tex", "scrollbar.tex", "scrollbar2.tex", "scrollbar3.tex"],
			"five start submissions in documented order")
		var start_fields_ok := true
		for record in start:
			start_fields_ok = start_fields_ok and record.position == Vector2.ZERO and \
				record.layer == 6 and record.tint == Display.START_TINT
		expect(start_fields_ok, "start position, layer and tint")
	var normal := Display.submissions("loading", 639, "titleload.tex")
	expect(normal.map(func(r): return r.name) == ["shot.tex", "titleload.tex",
		"scrollbar2.tex", "scrollbar2.tex", "scrollbar3.tex", "scrollbar3.tex"],
		"normal submission order")
	expect(normal[0].position == Vector2(0, 100) and normal[1].position == Vector2(228, 185) and
		normal[2].position == Vector2(639, 0) and normal[3].position == Vector2(-1, 0) and
		normal[4].position == Vector2(639, 352) and normal[5].position == Vector2(-1, 352),
		"normal positions")
	var painted := Display.painter_records(normal)
	expect(painted[0] == normal[5] and painted[5] == normal[0],
		"layer-6 painter reverses retained submissions")
	var neutral := Display.sprite_tint(Display.NORMAL_TINT)
	var strip := Display.sprite_tint(Display.STRIP_TINT)
	var start_tint := Display.sprite_tint(Display.START_TINT)
	expect(neutral == Color(1, 1, 1, 1) and strip == Color(1, 1, 1, 20.0 / 128.0) and
		start_tint == Color(0.75, 0.75, 0.75, 1), "sprite /128 tint adapter")
	expect(Display.warmup_remaining(4, true, false, 12) == 10 and
		Display.warmup_remaining(3, false, true, 12) == 3 and
		Display.warmup_remaining(3, false, false, 12) == 2 and
		Display.warmup_remaining(10, true, false, 1500) == 0,
		"terrain reset, pending equipment floor and 1500-present escape")
	var terrain := TerrainStream.new()
	terrain.wanted = PackedInt32Array([1, 2])
	terrain.resident = {1: []}
	expect(terrain.loading_busy(), "missing wanted terrain cell is busy")
	terrain.resident[2] = []
	expect(not terrain.loading_busy(), "complete terrain window is ready")
	terrain.pending_cell = 3
	expect(terrain.loading_busy(), "pending terrain build is busy")
	terrain.free()
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
	if "--inject-failure" in OS.get_cmdline_user_args(): expect(false, "injected failure")
	print("LOADING_CASES %d/%d" % [checks - failures, checks])
	quit(1 if failures else 0)
