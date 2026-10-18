extends "res://scenes/session_sandbox.gd"
const LevelTextureMaterial = preload("res://scenes/level_texture_material.gd")

# Optional textured RTA base layer over the existing Session terrain controller.
# No data is bundled; the caller must select a matching WORLD/TEX pair.
func _ready() -> void:
	super._ready()
	var args := OS.get_cmdline_user_args()
	var world_path := ""
	var tex_path := ""
	for i in range(args.size() - 1):
		if args[i] == "--world": world_path = args[i + 1]
		if args[i] == "--tex": tex_path = args[i + 1]
	if not world_path.is_empty() and not tex_path.is_empty():
		call_deferred("load_textured_terrain", world_path, tex_path)

func load_textured_terrain(world_path: String, tex_path: String) -> bool:
	if controller == null or not ClassDB.class_exists("ChampionsLevelTexturePreview"):
		return false
	var loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
	var result: Dictionary = loader.load_world_tex(world_path, tex_path)
	loader.free()
	if not result.get("ok", false):
		status_label.text = str(result.get("error", "The WORLD/TEX pair was refused."))
		return false
	var candidate := _prepare_textured_root(result)
	if candidate == null:
		status_label.text = "The textured ground could not be prepared."
		return false
	# Prepare both the native collision state and the textured subtree before
	# publishing either. This mirrors the inherited candidate/commit sequence.
	var prepared: Dictionary = controller.prepare_terrain_file(world_path)
	if not prepared.get("ok", false):
		candidate.free()
		status_label.text = str(prepared.get("error", "The WORLD could not be loaded."))
		return false
	var committed: Dictionary = controller.commit_terrain_file(prepared.token)
	if not committed.get("ok", false):
		candidate.free()
		status_label.text = str(committed.get("error", "The WORLD could not be committed."))
		return false
	_publish_terrain_root(candidate)
	_last_clock_usec = 0
	held_commands = 0
	pending_damage = false
	_observe(committed)
	terrain_label.text = "Textured RTA ground"
	return true

func _prepare_textured_root(result: Dictionary) -> Node3D:
	return preload("res://scenes/level_texture_terrain.gd").build(result)
