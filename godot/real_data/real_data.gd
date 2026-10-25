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
const RealWorld = preload("res://real_data/real_world.gd")
const RealCreature = preload("res://real_data/real_creature.gd")
const MAX_SCAN_DEPTH := 3
const MAX_FILES := 4096
const MAX_ENTRY_BYTES := 64 * 1024 * 1024

var scene: Control
var disc_dir := ""
var cache_dir := ""
var files: Dictionary = {} # lower-case file name -> absolute path
var levels: Array[Dictionary] = [] # {stem, gob, tex, vat}
var current_qualified := ""
var current := -1
var world: RefCounted = null # RealWorld presenter of the current level
var level_lights_path := "" # user's fx_level_lights.py --json output, if chosen
var fonts: Array[Dictionary] = [] # {path, row_index, fingerprint} FNT rows found
var creature: RefCounted = null # RealCreature texture of the current preview
var arena_tables: Dictionary = {} # counts from the executable's arena tables, if read
var environment: ImageTexture # first .etex in the current level's env.lmp, if it decodes
var last_error := ""
# Level textures stream by cell around the camera focus, as the original does
# (scenes/level_texture_stream.gd). False decodes every cell up front.
var stream_textures := true

func configure(owner: Control, cache: String) -> void:
	scene = owner
	cache_dir = cache

func open_disc(path: String) -> bool:
	files.clear()
	_dialog_font = PackedByteArray()
	_dialog_text = PackedByteArray()
	_dialog_font_searched = false
	levels.clear()
	current = -1
	current_qualified = ""
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
	# The arena name tables, read from the folder's executable at run time
	# (docs/ARENA_MODES.md). They select the mode-A start for a level that is
	# listed there and has no story start. Without an executable that start
	# stays refused.
	arena_tables = {}
	if files.has("slus_209.73"):
		arena_tables = scene.controller.load_arena_tables(files["slus_209.73"])
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

# Every entry of `lump` whose name ends with `extension`, by its exact name.
func read_entries(archive: String, lump: String, extension: String) -> Dictionary:
	var out := {}
	var file: FileAccess = null
	for row in _rows(archive):
		if typeof(row) != TYPE_DICTIONARY or not _matches(row, lump, extension): continue
		var size := int(row.get("size", 0))
		if size <= 0 or size > MAX_ENTRY_BYTES: continue
		if file == null: file = FileAccess.open(archive, FileAccess.READ)
		if file == null: return out
		file.seek(int(row.get("offset", 0)))
		var bytes := file.get_buffer(size)
		if bytes.size() == size and not out.has(str(row.name)): out[str(row.name)] = bytes
	return out

# The NPC dialog's inputs for level `index` (docs/DIALOG.md), read from the
# game folder at run time and handed to the native host; nothing is kept.
var _dialog_font := PackedByteArray()
var _dialog_text := PackedByteArray()
var _dialog_font_searched := false
func dialog_data(index: int) -> Dictionary:
	if index < 0 or index >= levels.size(): return {}
	var level: Dictionary = levels[index]
	var lump: String = str(level.stem) + ".lmp"
	if _dialog_text.is_empty() and files.has("gtext.lmp"):
		_dialog_text = read_entry(files["gtext.lmp"], "", "gtext.uni")
	var font := dialog_font()
	var vat_size := 0
	if not str(level.vat).is_empty():
		var vat := FileAccess.open(level.vat, FileAccess.READ)
		if vat != null: vat_size = vat.get_length()
	return {
		"level": str(level.stem),
		"dbin": read_entries(level.gob, lump, ".dbin"),
		"stream_directory": read_entry(level.gob, lump, "ntsc_dialog.bin"),
		"vat_size": vat_size,
		"text": _dialog_text,
		"font": font,
		# dialog_loadBox's res_find("dlgbox.lmp", "box.tex"): the loose lump is on the disc.
		"box_available": files.has("dlgbox.lmp"),
	}

# gamefontmed.fnt, the dialog's font word 0x4f7100 (docs/dialog.md 7).
func dialog_font() -> PackedByteArray:
	if _dialog_font_searched: return _dialog_font
	_dialog_font_searched = true
	var names := files.keys()
	names.sort()
	for name in names:
		var extension: String = str(name).get_extension()
		if extension != "lmp" and extension != "gob": continue
		var bytes := read_entry(files[name], "", "gamefontmed.fnt")
		if not bytes.is_empty():
			_dialog_font = bytes
			break
	return _dialog_font

# box.tex of dlgbox.lmp (dialog_loadBox, docs/dialog.md 7), decoded.
func dialog_box() -> Dictionary:
	if not files.has("dlgbox.lmp"): return {"ok": false}
	var path: String = files["dlgbox.lmp"]
	var listing: Dictionary = scene.inspector.inspect_archive(path)
	if not listing.get("ok", false): return {"ok": false}
	var rows: Array = listing.get("rows", [])
	for i in range(rows.size()):
		if str(rows[i].get("name", "")).to_lower() == "box.tex":
			return scene.inspector.inspect_texture(path, i, "rta", str(listing.get("fingerprint", "")))
	return {"ok": false}

# Load level `index` through the real level library, then its documented TEX.
func load_level(index: int, previous := "", entrance := "", qualified := "") -> Dictionary:
	if index < 0 or index >= levels.size(): return {"ok": false, "stage": "index"}
	var level: Dictionary = levels[index]
	var bytes := read_entry(level.gob, level.stem + ".lmp", ".world")
	if bytes.is_empty(): return {"ok": false, "stage": "world-entry"}
	DirAccess.make_dir_recursive_absolute(cache_dir)
	var world_path := ProjectSettings.globalize_path(cache_dir.path_join("level-%03d.world" % index)).simplify_path()
	var out := FileAccess.open(world_path, FileAccess.WRITE)
	if out == null: return {"ok": false, "stage": "cache-write"}
	out.store_buffer(bytes)
	out.close()
	world = null
	var session_path := world_path
	# Level-spawn story start (docs/LEVEL_FLOW.md section 17) from this level's
	# objects.ob. Without it the session keeps the historical origin start.
	var objects := read_entry(level.gob, level.stem + ".lmp", "objects.ob")
	scene.controller.set_terrain_start_objects(world_path, objects, str(level.stem), previous, entrance)
	var library = scene.components.get("library")
	if library == null: return {"ok": false, "stage": "library"}
	# The streamed path commits the session once with a summary-only neutral
	# display; the textured root is the only mesh it publishes. Keep the library
	# candidate and its history without making it load the WORLD a second time.
	var hold: bool = scene.fx != null and "world_lighting_held" in scene.fx
	if hold: scene.fx.world_lighting_held = true
	library._add_candidate(session_path)
	if not stream_textures:
		library._load_selected()
		if scene.level_textures.world_path != session_path:
			if hold: scene.fx.world_lighting_held = false
			return {"ok": false, "stage": "library-load", "error": library._native_error()}
	scene.level_textures.session_worlds.clear()
	scene.level_textures.stream_cells = stream_textures
	var textured: bool = scene.load_textured_terrain(world_path, level.tex)
	if hold: scene.fx.world_lighting_held = false
	if not textured:
		return {"ok": false, "stage": "textures", "error": scene.status_label.text}
	if stream_textures: library.record_loaded(session_path)
	current = index
	current_qualified = qualified if not qualified.is_empty() else str(level.stem)
	# Per-level light record from the user's executable JSON (level-lights):
	# game_loadLevel's lookup by this level's name. Applied when play starts;
	# without a JSON the labelled synthetic sun stays.
	if scene.fx != null and scene.fx.has_method("select_level_light"):
		scene.fx.select_level_light(level_lights_path, str(level.stem))
	scene.audio.level_bank = bank(level.vat, level.gob, level.stem + ".lmp")
	world = RealWorld.new()
	var parts: Dictionary = world.open(world_path, read_entry(level.gob, level.stem + ".lmp", "objects.ob"), scene.terrain_root)
	if not parts.get("ok", false):
		world = null
		return {"ok": false, "stage": "world-parts", "error": str(parts.get("error", ""))}
	# The level flag word is row +0x20 of the same level-lights record.
	var flags_result := {}
	if scene.fx != null and bool(scene.fx.get("level_light_active")) and scene.fx.level_light_record.has("flags"):
		flags_result = world.set_level_flags(int(scene.fx.level_light_record.flags))
	var water_image := read_entry(level.gob, "waterenv.lmp", ".tex")
	var water_texture: Dictionary = world.set_water_texture(water_image)
	_load_environment(level)
	if creature != null: creature.set_environment(environment)
	var terrain: Dictionary = scene.controller.get_terrain()
	return {"ok": true, "stage": "loaded", "world": world_path, "start": terrain.get("start", {}), "session_surfaces": terrain.get("surfaces", []).size(), "session_triangles": int(terrain.get("triangles", 0)), "water_tiles": parts.water.size(), "pieces": parts.pieces.size(), "claimed": int(parts.get("claimed", 0)), "drawn_claimed": world.drawn_claimed_pieces(), "water_texture_ok": bool(water_texture.get("ok", false)), "water_texture_width": int(water_texture.get("width", 0)), "water_texture_height": int(water_texture.get("height", 0)), "water_lines": int(parts.get("water_lines", 0)), "water_lines_refused": int(parts.get("water_lines_refused", 0)), "water_negative_fix": bool(parts.get("water_negative_fix", false)), "level_flags_loaded": bool(flags_result.get("ok", false)), "level_flags_error": str(flags_result.get("error", "")), "water_tiles_refused": int(water_texture.get("tiles_refused", 0)), "environment": environment != null}

func _load_environment(level: Dictionary) -> void:
	environment = null
	if not ClassDB.class_exists("ChampionsRealWorld"): return
	var bytes := read_entry(level.gob, "env.lmp", ".etex")
	if bytes.is_empty(): return
	var decoded: Dictionary = ClassDB.class_call_static("ChampionsRealWorld", "decode_texture", bytes)
	if not decoded.get("ok", false): return
	var image := Image.create_from_data(int(decoded.width), int(decoded.height), false, Image.FORMAT_RGBA8, decoded.rgba)
	if image == null: return
	environment = ImageTexture.create_from_image(image)

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
				creature = RealCreature.new()
				creature.environment = environment
				var textured: Dictionary = creature.bind(archive, preview.rows, m)
				if not textured.get("ok", false) or not creature.apply(preview.mesh_instance): creature = null
				return {"ok": true, "attempts": attempts, "models": models.size(), "clips": clips.size(), "textured": creature != null, "texture_error": str(textured.get("error", "")), "alpha1_documented": bool(textured.get("alpha1_documented", false)), "two_pass": bool(textured.get("two_pass", false)), "environment": creature != null and creature.environment != null}
	return {"ok": false, "attempts": attempts, "models": models.size(), "clips": clips.size()}

# FNT rows in the folder's archives (ui-fonts-2 decodes their embedded page).
# Any row can be an explicit choice for the original-art panel. The play HUD
# takes only the documented one (load_hud_font).
func find_fonts() -> Array[Dictionary]:
	fonts.clear()
	var names := files.keys()
	names.sort()
	for name in names:
		var extension: String = str(name).get_extension()
		if extension != "lmp" and extension != "gob" and extension != "fnt": continue
		var listing: Dictionary = scene.inspector.inspect_ui_candidates(files[name], "rta")
		if not listing.get("ok", false): continue
		for row in listing.get("rows", []):
			if row is Dictionary and row.get("kind", "") == "font":
				fonts.append({"path": files[name], "row_index": int(row.row_index), "fingerprint": str(listing.get("fingerprint", "")), "name": str(row.get("name", "")), "parent": str(row.get("parent", ""))})
	return fonts

func load_font(index: int) -> bool:
	if index < 0 or index >= fonts.size(): return false
	var art = scene.components.get("art")
	if art == null or art.hud_view == null: return false
	var entry: Dictionary = fonts[index]
	var read: Dictionary = scene.inspector.read_fnt_entry(entry.path, entry.row_index, "rta", entry.fingerprint)
	if not read.get("ok", false): return false
	if not art.hud_view.set_font(read.bytes): return false
	art.hud_view.observe(art.observation)
	return true

# The play HUD's art: every documented identity of the original-art catalogue
# from its named lump in this folder (hud-layout.md: HUD.LMP for the bars and
# the status frame), through the panel's checked path. Returns counts.
func load_hud_art() -> Dictionary:
	var out := {"loaded": 0, "refused": 0, "missing": 0}
	var art = scene.components.get("art")
	if art == null: return out
	for id in art.catalogue:
		var source: String = files.get(str(art.catalogue[id].lump).to_lower(), "")
		if source.is_empty(): out.missing += 1
		elif art.load_documented(str(id), source): out.loaded += 1
		else: out.refused += 1
	return out

# research-ui 3e39392 docs/formats/ui-fonts.md: hud_drawPlayerStatus reads the
# small-font pointer 0x4f7104, which startup fills from FONT.LMP
# gamefontsmall.fnt. Only that row, and only when exactly one matches.
func hud_font_bytes() -> PackedByteArray:
	var found: Array[Dictionary] = []
	for entry in fonts:
		var container := str(entry.parent).get_file() if not str(entry.parent).is_empty() else str(entry.path).get_file()
		if container.to_lower() == "font.lmp" and str(entry.name).to_lower() == "gamefontsmall.fnt": found.append(entry)
	if found.size() != 1: return PackedByteArray()
	var read: Dictionary = scene.inspector.read_fnt_entry(found[0].path, found[0].row_index, "rta", found[0].fingerprint)
	return read.get("bytes", PackedByteArray()) if read.get("ok", false) else PackedByteArray()

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
