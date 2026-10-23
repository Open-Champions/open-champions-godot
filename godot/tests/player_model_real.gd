extends SceneTree

# Private opt-in acceptance: twelve documented class bodies in both selection
# archives, one authored skin-zero appearance, and the body rest pose. Counts only.
const RealData = preload("res://real_data/real_data.gd")
const RealPlayer = preload("res://real_data/real_player.gd")

class ReaderScene extends Control:
	var inspector: RefCounted

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var folder := ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--game-folder="):
			folder = argument.trim_prefix("--game-folder=")
	if folder.is_empty() or not DirAccess.dir_exists_absolute(folder):
		push_error("Choose an absolute read-only game folder.")
		quit(2)
		return
	var real := RealData.new()
	root.add_child(real)
	real._scan(folder, 0)
	var inspector = ClassDB.instantiate("ChampionsArchiveInspector")
	var reader_scene := ReaderScene.new()
	reader_scene.inspector = inspector
	real.scene = reader_scene
	var actor := MeshInstance3D.new()
	root.add_child(actor)
	var player := RealPlayer.new()
	var drawn := 0
	var textured := 0
	for player_count in [1, 2]:
		for index in range(12):
			if player.install(real, actor, inspector, index, [-1, 0, -1, 0], player_count) and player.body_drawn and actor.mesh is ArrayMesh:
				drawn += 1
				if player.textured: textured += 1
			else:
				print("PLAYER_MODEL_REFUSAL index=%d count=%d reason=%s" % [index, player_count, player.status])
	print("PLAYER_MODEL_COUNTS " + JSON.stringify({"tried": 24, "drawn": drawn, "textured": textured}))
	player.clear()
	actor.free()
	reader_scene.free()
	real.free()
	quit(0 if drawn == 24 and textured == 24 else 1)
