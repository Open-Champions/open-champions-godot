extends RefCounted

const Terrain = preload("res://scenes/level_texture_terrain.gd")
const Stream = preload("res://scenes/level_texture_stream.gd")
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
# Opt-in (the game-folder scene): keep only the 3 x 3 texture cells around the
# camera focus resident instead of decoding the whole level.
var stream_cells := false

func configure(owner: Control) -> void:
	scene = owner

func load_pair(world: String, tex: String) -> bool:
	if scene.controller == null or not ClassDB.class_exists("ChampionsLevelTexturePreview"):
		return false
	var data: Dictionary
	if stream_cells and ClassDB.class_exists("ChampionsLevelTextureStream"):
		data = {"world": world, "tex": tex, "decoded": {"streamed": true}}
	else:
		var loader = ClassDB.instantiate("ChampionsLevelTexturePreview")
		var result: Dictionary = loader.load_world_tex(world, tex)
		loader.free()
		if not result.get("ok", false):
			scene.status_label.text = str(result.get("error", "WORLD/TEX pair refused."))
			return false
		data = {"world": world, "tex": tex, "decoded": result}
	var candidate := build(data)
	if candidate == null:
		scene.status_label.text = "Textured ground could not be prepared."
		return false
	# The streamed display owns the visible meshes. Its session prepare still
	# validates every chunk and keeps the same collision and summary counts.
	var prepared: Dictionary = scene.controller.prepare_terrain_file(str(session_worlds.get(world, world)), not stream_cells)
	if not prepared.get("ok", false):
		candidate.free()
		scene.status_label.text = str(prepared.get("error", "WORLD refused."))
		return false
	var committed: Dictionary = scene.controller.commit_terrain_file(prepared.token)
	if not committed.get("ok", false):
		candidate.free()
		scene.status_label.text = str(committed.get("error", "WORLD commit refused."))
		return false
	if candidate.has_method("setup_focus"): candidate.setup_focus(committed)
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
	if data.decoded.get("streamed", false): return _build_stream(data)
	var candidate := Terrain.build(data.decoded, true)
	if candidate != null: candidate.set_meta("level_texture_pair", data)
	return candidate

# The streamed display keeps the WORLD geometry and the compressed TEX only.
# Its pair records the level totals; surfaces stay empty because none are
# decoded up front. A restore reopens the same files.
static func _build_stream(data: Dictionary) -> Node3D:
	var native: RefCounted = ClassDB.instantiate("ChampionsLevelTextureStream")
	var summary: Dictionary = native.open(data.world, data.tex)
	if not summary.get("ok", false): return null
	var candidate = Stream.new()
	candidate.streamer = native
	candidate.set_meta("level_texture_summary", summary)
	candidate.set_meta("level_texture_pair", {"world": data.world, "tex": data.tex, "decoded": {
		"streamed": true, "surfaces": [], "images": [],
		"triangles": int(summary.triangles), "batches": int(summary.batches), "cells": int(summary.cells), "rgba_bytes": 0}})
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
	var note := " · constant chunk light is the documented constant on the level's base level (gated, base texture only, without a level light record)" if special else ""
	return ("%s + %s · decoded textures · native FX lighting · environment rows, layer order and automap tiles" % [str(pair.world).get_file(), str(pair.tex).get_file()]) + note
