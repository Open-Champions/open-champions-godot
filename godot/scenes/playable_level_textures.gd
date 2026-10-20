extends RefCounted

const Terrain = preload("res://scenes/level_texture_terrain.gd")
# Client resource ownership only; native saves and lighting remain unchanged.
const MAX_PAIRS := 128
const MAX_SAVES := 8
var scene: Control
var world_path := ""
var pairs: Dictionary = {}
var saves: Dictionary = {}
var restore: Dictionary = {}
# Optional WORLD path the session controller loads instead of the displayed
# one (a presentation-thinned copy with identical collision bytes).
var session_worlds: Dictionary = {}

func configure(owner: Control) -> void:
	scene = owner

func load_pair(world: String, tex: String) -> bool:
	if scene.controller == null or not ClassDB.class_exists("ChampionsLevelTexturePreview"):
		return false
	var loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
	var result: Dictionary = loader.load_world_tex(world, tex)
	loader.free()
	if not result.get("ok", false):
		scene.status_label.text = str(result.get("error", "WORLD/TEX pair refused."))
		return false
	var data := {"world": world, "tex": tex, "decoded": result}
	var candidate := build(data)
	if candidate == null:
		scene.status_label.text = "Textured ground could not be prepared."
		return false
	var prepared: Dictionary = scene.controller.prepare_terrain_file(str(session_worlds.get(world, world)))
	if not prepared.get("ok", false):
		candidate.free()
		scene.status_label.text = str(prepared.get("error", "WORLD refused."))
		return false
	var committed: Dictionary = scene.controller.commit_terrain_file(prepared.token)
	if not committed.get("ok", false):
		candidate.free()
		scene.status_label.text = str(committed.get("error", "WORLD commit refused."))
		return false
	scene._publish_terrain_root(candidate)
	scene._last_clock_usec = 0
	scene.held_commands = 0
	scene.pending_damage = false
	scene._observe(committed)
	world_path = world
	pairs.erase(world)
	pairs[world] = tex
	while pairs.size() > MAX_PAIRS: pairs.erase(pairs.keys()[0])
	return true

static func build(data: Dictionary) -> Node3D:
	var candidate := Terrain.build(data.decoded, true)
	if candidate != null: candidate.set_meta("level_texture_pair", data)
	return candidate

func remember_save(path: String) -> void:
	saves.erase(path)
	if scene.terrain_root.has_meta("level_texture_pair"):
		saves[path] = {"hash": FileAccess.get_sha256(path), "data": scene.terrain_root.get_meta("level_texture_pair")}
	while saves.size() > MAX_SAVES: saves.erase(saves.keys()[0])

func prepare_restore(path: String) -> void:
	restore = {}
	if saves.has(path) and FileAccess.file_exists(path) and FileAccess.get_sha256(path) == saves[path].hash:
		restore = saves[path].data

func published(candidate: Node3D) -> void:
	world_path = str(candidate.get_meta("level_texture_pair").world) if candidate.has_meta("level_texture_pair") else ""

func label() -> String:
	if not scene.terrain_root.has_meta("level_texture_pair"): return ""
	var pair: Dictionary = scene.terrain_root.get_meta("level_texture_pair")
	var special := false
	for surface in pair.decoded.surfaces: special = special or not surface.get("dynamic_light", true)
	var note := " · special chunk lighting gated (base texture only)" if special else ""
	return ("%s + %s · decoded textures · native FX lighting in play · environment / console depth / automap mapping gated" % [str(pair.world).get_file(), str(pair.tex).get_file()]) + note
