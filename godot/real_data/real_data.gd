extends Node

# Run-time adapter for an explicitly chosen, read-only RTA game folder. It only
# reads that folder. The level's WORLD entry is copied out of its GOB into the
# explicitly chosen cache folder because the released loaders take file paths;
# nothing is written under res://, into saves or into exported packages.
# Pairing rules used here are the documented ones: <LEVEL>.TEX and <LEVEL>.VAT
# beside <LEVEL>.GOB, the first .world of <level>.lmp in directory order, and
# the ntsc_dialog.bin VAT directory of that lump. Creature model/clip pairs are
# explicit user choices; arithmetic success never claims retail pairing.
const RealAudio = preload("res://real_data/real_audio.gd")
const RealFx = preload("res://real_data/real_fx.gd")
const MAX_SCAN_DEPTH := 3
const MAX_FILES := 4096
const MAX_ENTRY_BYTES := 64 * 1024 * 1024

var scene: Control
var disc_dir := ""
var cache_dir := ""
var files: Dictionary = {} # lower-case file name -> absolute path
var levels: Array[Dictionary] = [] # {stem, gob, tex, vat}
var current := -1
var last_error := ""

func configure(owner: Control, cache: String) -> void:
	scene = owner
	cache_dir = cache

func open_disc(path: String) -> bool:
	files.clear()
	levels.clear()
	current = -1
	disc_dir = ""
	if not DirAccess.dir_exists_absolute(path):
		return _fail("Game folder unavailable.")
	_scan(path, 0)
	for name in files:
		if name.get_extension() != "gob": continue
		var stem: String = name.get_basename()
		if files.has(stem + ".tex"):
			levels.append({"stem": stem, "gob": files[name], "tex": files[stem + ".tex"], "vat": files.get(stem + ".vat", "")})
	levels.sort_custom(func(a, b): return a.stem < b.stem)
	if levels.is_empty():
		return _fail("No GOB with a matching level TEX was found.")
	disc_dir = path
	_install_presentation()
	return true

func _scan(path: String, depth: int) -> void:
	var dir := DirAccess.open(path)
	if dir == null or depth > MAX_SCAN_DEPTH: return
	dir.list_dir_begin()
	var name := dir.get_next()
	while not name.is_empty() and files.size() < MAX_FILES:
		if not dir.is_link(name):
			if dir.current_is_dir(): _scan(path.path_join(name), depth + 1)
			else: files[name.to_lower()] = path.path_join(name)
		name = dir.get_next()
	dir.list_dir_end()

# Replace the scene's synthetic audio/FX presenters with subclasses that read
# the chosen folder. Signals are moved from the old node to the new one.
func _install_presentation() -> void:
	if scene.audio != null and not scene.audio is RealAudio:
		var status: Label = scene.audio.label
		if scene.controller.play_audio_tick.is_connected(scene.audio.ready_tick):
			scene.controller.play_audio_tick.disconnect(scene.audio.ready_tick)
		scene.audio.release()
		scene.audio.queue_free()
		var audio := RealAudio.new()
		audio.global_bank = bank(files.get("mouth.vat", ""), files.get("mouth.lmp", ""), "")
		scene.add_child(audio)
		audio.configure(scene, status)
		scene.controller.play_audio_tick.connect(audio.ready_tick)
		scene.audio = audio
	if scene.fx != null and not scene.fx is RealFx:
		var parent: Node = scene.fx.get_parent()
		if scene.controller.combat_visual_tick.is_connected(scene.fx.combat_tick):
			scene.controller.combat_visual_tick.disconnect(scene.fx.combat_tick)
		var json: String = scene.fx.definition_json
		var source: String = scene.fx.definition_source
		parent.remove_child(scene.fx)
		scene.fx.queue_free()
		var fx := RealFx.new()
		fx.sheet_archives = PackedStringArray([files.get("engine.lmp", ""), files.get("fx.lmp", "")])
		parent.add_child(fx)
		fx.configure(scene)
		fx.definition_json = json
		fx.definition_source = source
		fx.reset()
		scene.controller.combat_visual_tick.connect(fx.combat_tick)
		scene.fx = fx

# Directory bytes + VAT path for a bank. `holder` is the LMP/GOB holding the
# ntsc_dialog.bin directory; `lump` restricts the search to one nested LMP.
func bank(vat: String, holder: String, lump: String) -> Dictionary:
	if vat.is_empty() or holder.is_empty(): return {}
	var directory := read_entry(holder, lump, "ntsc_dialog.bin")
	if directory.is_empty(): return {}
	return {"vat": vat, "directory": directory}

func _rows(archive: String) -> Array:
	var listing: Dictionary = scene.inspector.inspect_archive(archive)
	if not listing.get("ok", false): return []
	return listing.get("rows", [])

func _matches(row: Dictionary, lump: String, name_or_extension: String) -> bool:
	var name := str(row.get("name", "")).to_lower()
	var parent := str(row.get("parent", "")).get_file().to_lower()
	if not lump.is_empty() and parent != lump: return false
	return name == name_or_extension if not name_or_extension.begins_with(".") else name.ends_with(name_or_extension)

# First matching row in directory order (lump_findNthByExtension order).
func read_entry(archive: String, lump: String, name_or_extension: String) -> PackedByteArray:
	for row in _rows(archive):
		if typeof(row) != TYPE_DICTIONARY or not _matches(row, lump, name_or_extension): continue
		var size := int(row.get("size", 0))
		if size <= 0 or size > MAX_ENTRY_BYTES: return PackedByteArray()
		var file := FileAccess.open(archive, FileAccess.READ)
		if file == null: return PackedByteArray()
		file.seek(int(row.get("offset", 0)))
		var bytes := file.get_buffer(size)
		return bytes if bytes.size() == size else PackedByteArray()
	return PackedByteArray()

# Load level `index` through the real level library, then its documented TEX.
func load_level(index: int) -> Dictionary:
	if index < 0 or index >= levels.size(): return {"ok": false, "stage": "index"}
	var level: Dictionary = levels[index]
	var world := read_entry(level.gob, level.stem + ".lmp", ".world")
	if world.is_empty(): return {"ok": false, "stage": "world-entry"}
	DirAccess.make_dir_recursive_absolute(cache_dir)
	var world_path := cache_dir.path_join("level-%03d.world" % index)
	var out := FileAccess.open(world_path, FileAccess.WRITE)
	if out == null: return {"ok": false, "stage": "cache-write"}
	out.store_buffer(world)
	out.close()
	var library = scene.components.get("library")
	if library == null: return {"ok": false, "stage": "library"}
	library._add_candidate(world_path)
	library._load_selected()
	if scene.level_textures.world_path != ProjectSettings.globalize_path(world_path).simplify_path() and scene.level_textures.world_path != world_path:
		return {"ok": false, "stage": "library-load", "error": library._native_error()}
	if not scene.load_textured_terrain(scene.level_textures.world_path, level.tex):
		return {"ok": false, "stage": "textures", "error": scene.status_label.text}
	current = index
	scene.audio.level_bank = bank(level.vat, level.gob, level.stem + ".lmp")
	return {"ok": true, "stage": "loaded", "world": world_path}

# Explicit model/clip choice from a chosen LMP. Returns the first model row and
# clip row the native sampler accepts; this is a cosmetic user selection only.
func load_creature(archive: String, max_attempts := 64) -> Dictionary:
	var preview = scene.components.get("animation")
	if preview == null or not preview.open_archive(archive): return {"ok": false, "attempts": 0}
	var models: Array[int] = []
	var clips: Array[int] = []
	for i in range(preview.rows.size()):
		var name := str(preview.rows[i].get("name", "")).to_lower()
		if name.ends_with(".vif"): models.append(i)
		elif name.ends_with(".anm"): clips.append(i)
	var attempts := 0
	for m in models:
		for c in clips:
			if attempts >= max_attempts: return {"ok": false, "attempts": attempts}
			attempts += 1
			if preview.select_pair(m, c):
				preview.play()
				return {"ok": true, "attempts": attempts, "models": models.size(), "clips": clips.size()}
	return {"ok": false, "attempts": attempts, "models": models.size(), "clips": clips.size()}

func clear_cache() -> void:
	var dir := DirAccess.open(cache_dir)
	if dir == null: return
	for name in dir.get_files():
		if name.begins_with("level-") and name.ends_with(".world"): dir.remove(name)

func _fail(message: String) -> bool:
	last_error = message
	return false

# Extracted WORLD copies live only as long as the scene.
func _exit_tree() -> void:
	clear_cache()
