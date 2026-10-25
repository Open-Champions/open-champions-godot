extends RefCounted

# Run-time text sources from the user's read-only game folder (docs/STRINGS.md):
# every .uni table of GTEXT.LMP, the level lump's .uni tables, the help chime's
# sound group (byte +0x3c of fx.lmp/butn_message.anm, research-audio
# docs/formats/anim-audio.md 2.1) and the two HUD fonts of FONT.LMP. Nothing is
# copied into the project; the bytes live only in memory.
const CHIME_LUMP := "fx.lmp"
const CHIME_ENTRY := "butn_message.anm"
const TEXT_LUMP := "gtext.lmp"
const FONT_LUMP := "font.lmp"
# research-ui docs/formats/ui-fonts.md: startup loads gamefontmed and
# gamefontsmall; the medium font is also installed as the default.
const FONTS := ["gamefontmed.fnt", "gamefontsmall.fnt"]

static func entries(real: Node, archive: String, lump: String, extension: String) -> Array[PackedByteArray]:
	var out: Array[PackedByteArray] = []
	for row in real._rows(archive):
		if typeof(row) != TYPE_DICTIONARY: continue
		var name := str(row.get("name", "")).to_lower()
		var parent := str(row.get("parent", "")).get_file().to_lower()
		if not name.ends_with(extension): continue
		if not lump.is_empty() and parent != lump: continue
		var size := int(row.get("size", 0))
		if size <= 0 or size > real.MAX_ENTRY_BYTES: continue
		var file := FileAccess.open(archive, FileAccess.READ)
		if file == null: continue
		file.seek(int(row.get("offset", 0)))
		var bytes := file.get_buffer(size)
		if bytes.size() == size: out.append(bytes)
	return out

# Installs the tables and the chime on the controller. Returns counts only.
static func install(real: Node, controller: Object) -> Dictionary:
	var result := {"ok": false, "tables": 0, "help_chime": false}
	if not real.files.has(TEXT_LUMP): return result
	var tables := entries(real, real.files[TEXT_LUMP], "", ".uni")
	var chime := -1
	if real.files.has(CHIME_LUMP):
		var anm: PackedByteArray = real.read_entry(real.files[CHIME_LUMP], "", CHIME_ENTRY)
		if anm.size() > 0x3c: chime = anm[0x3c]
	var installed: Dictionary = controller.set_text_sources(tables, chime)
	result.ok = installed.get("ok", false)
	result.tables = int(installed.get("tables", 0))
	result.entries = int(installed.get("entries", 0))
	result.help_chime = installed.get("help_chime", false)
	return result

static func level_tables(real: Node, level: Dictionary) -> Array[PackedByteArray]:
	return entries(real, level.gob, str(level.stem) + ".lmp", ".uni")

static func fonts(real: Node) -> Dictionary:
	var out := {}
	if not real.files.has(FONT_LUMP): return out
	for name in FONTS:
		var bytes: PackedByteArray = real.read_entry(real.files[FONT_LUMP], "", name)
		if not bytes.is_empty(): out[name] = bytes
	return out
