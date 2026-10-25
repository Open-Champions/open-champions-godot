extends SceneTree

# Read-only operator acceptance: selects art for every discovered level and
# checks the selected archive's shot header. Emits counts only.
const Display = preload("res://real_data/loading_display.gd")
var folder := ""

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="):
			folder = argument.trim_prefix("--game-folder=")
	call_deferred("run")

func run() -> void:
	if folder.is_empty():
		quit(2)
		return
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	if not scene.open_game_folder(folder):
		quit(2)
		return
	var checked := 0
	var fallback := 0
	var refused := 0
	for level in scene.real.levels:
		var wanted: String = Display.candidate(str(level.stem))
		var selected: String = Display.select_lump(str(level.stem), scene.real.files)
		if selected == "lmiss.lmp" and selected != wanted:
			fallback += 1
		var source: String = scene.real.files.get(selected, "")
		if source.is_empty():
			refused += 1
			continue
		var listing: Dictionary = scene.inspector.inspect_archive(source)
		if not listing.get("ok", false):
			refused += 1
			continue
		var found := false
		var rows: Array = listing.get("rows", [])
		for i in range(rows.size()):
			if str(rows[i].get("name", "")).to_lower() != "shot.tex": continue
			var decoded: Dictionary = scene.inspector.inspect_texture(source, i, "rta", str(listing.get("fingerprint", "")))
			found = bool(decoded.get("ok", false)) and int(decoded.get("width", 0)) == 640 and int(decoded.get("height", 0)) == 320
			break
		if found:
			checked += 1
		else:
			refused += 1
	print("LOADING_DISC levels=%d selected=%d fallback=%d refused=%d" % [scene.real.levels.size(), checked, fallback, refused])
	quit(0 if refused == 0 and checked == scene.real.levels.size() else 1)
