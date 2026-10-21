extends SceneTree

# Streamed level textures on an authored 4 x 4 block of texture cells: the
# 3 x 3 window, nine slots, nearest-first reads, eviction, the atlas texels and
# the moving-piece tag. Synthetic fixtures only.
const TerrainFixture = preload("res://tests/session_sandbox_cases.gd")
const GeometryFixture = preload("res://tests/world_geometry_fixture.gd")
const Levels = preload("res://scenes/playable_level_textures.gd")
const TEX_FIXTURE := "res://tests/fixtures/level_texture_synthetic.bin"
var checks := 0
var failures := 0

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("run")

# One chunk per cell id, all sharing the authored packet and texture header.
func world_bytes(cells: Array, moving_cell: int) -> PackedByteArray:
	var world := GeometryFixture.world(TerrainFixture.new().flat_world())
	world.encode_u32(0xa4, 0x40)
	var packet_start := world.decode_u32(0x84)
	for i in range(6):
		world.encode_u16(packet_start + 0x90 + 2 * i, [0, 0, 640, 0, 0, 320][i])
	var record := world.slice(0x84, 0x84 + 0x3c)
	var table := (world.size() + 15) & ~15
	world.resize(table)
	for cell in cells:
		var copy := record.duplicate()
		copy.encode_u16(0x34, int(cell))
		if int(cell) == moving_cell: copy.encode_u32(0x24, copy.decode_u32(0x24) | 0x20)
		world.append_array(copy)
	world.encode_u32(0, cells.size())
	world.encode_u32(0x24, table)
	return world

# Every listed cell id points at the fixture's cell 5049 bytes.
func tex_bytes(cells: Array) -> PackedByteArray:
	var tex := FileAccess.get_file_as_bytes(TEX_FIXTURE)
	var offset := tex.decode_u32(8)
	var size := tex.decode_u32(12)
	check(tex.decode_u32(4) == 5049, "fixture TEX starts with cell 5049")
	var at := 4
	for cell in cells:
		tex.encode_u32(at, int(cell))
		tex.encode_u32(at + 4, offset)
		tex.encode_u32(at + 8, size)
		at += 12
	tex.encode_s32(at, -2)
	return tex

func write(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, path + " writable")
	file.store_buffer(bytes)
	file.close()

func run() -> void:
	var cells := []
	for row in range(49, 53):
		for column in range(49, 53): cells.append(row * 100 + column)
	var world_path := ProjectSettings.globalize_path("user://level_texture_stream.WORLD")
	var tex_path := ProjectSettings.globalize_path("user://level_texture_stream.TEX")
	write(world_path, world_bytes(cells, 5051))
	write(tex_path, tex_bytes(cells))
	if not ClassDB.class_exists("ChampionsLevelTextureStream"):
		check(false, "stream class registered")
		return finish()
	var native: RefCounted = ClassDB.instantiate("ChampionsLevelTextureStream")
	var summary: Dictionary = native.open(world_path, tex_path)
	check(summary.get("ok", false), "stream opens the synthetic pair")
	check(int(summary.get("cells", 0)) == 16 and int(summary.get("triangles", 0)) == 16, "16 drawn cells, one triangle each")
	check(float(summary.get("cell_size", 0)) == 960.0, "a zero header +0x68 is a 960-unit cell")
	check(int(native.focus_cell(0.0, 0.0)) == 5050 and int(native.focus_cell(-0.5, 959.5)) == 5049, "focus cell row follows y, column x")
	var window: PackedInt32Array = native.window(5050)
	check(window.size() == 9 and window[0] == 5050, "window holds nine cells, the focus cell first")
	check(Array(window.slice(1, 5)).all(func(c): return absi(c - 5050) == 1 or absi(c - 5050) == 100), "edge neighbours before corners")
	check(native.window(5252).size() == 4, "window keeps only cells with drawn chunks")
	var view: Dictionary = native.cell_view(5050, 128)
	check(view.get("ok", false) and view.groups.size() == 1 and bool(view.groups[0].atlas), "an opaque batch goes to the cell atlas")
	var atlas: Dictionary = view.images[0]
	var image := Image.create_from_data(int(atlas.width), int(atlas.height), false, Image.FORMAT_RGBA8, atlas.rgba)
	check(image.get_width() == 42 and image.get_height() == 22, "atlas page is the image plus a one-texel border")
	check(roundi(image.get_pixel(2, 1).r * 255.0) == 3 and image.get_pixel(0, 0) == image.get_pixel(1, 1), "atlas keeps exact texels and clamps the border")
	var uv: PackedVector2Array = view.groups[0].uv
	check(uv[1].is_equal_approx(Vector2((1.0 + 40.0) / 42.0, 1.0 / 22.0)) and uv[2].is_equal_approx(Vector2(1.0 / 42.0, 21.0 / 22.0)), "UVs map into the atlas box")
	var moving: Dictionary = native.cell_view(5051, 128)
	check(moving.groups.size() == 1 and not bool(moving.groups[0].atlas) and bool(moving.groups[0].moving) and int(moving.groups[0].chunk) == cells.find(5051), "a moving-piece chunk keeps its own tagged node")
	var order: PackedInt64Array = native.window_order(PackedInt32Array([5051, 5050]))
	check(order.size() == 6, "window order lists every resident batch")
	var display: Node3D = Levels._build_stream({"world": world_path, "tex": tex_path, "decoded": {"streamed": true}})
	check(display != null and display.has_meta("level_texture_pair"), "streamed display builds")
	var holder := Node3D.new()
	root.add_child(holder)
	var changes := [0]
	display.cells_changed.connect(func(): changes[0] += 1)
	holder.add_child(display)
	check(display.resident.size() == 9 and changes[0] == 9, "ready loads the complete first window")
	check(display.get_child_count() == 9, "one draw per resident cell here")
	var tagged := display.get_children().filter(func(n): return n.has_meta("level_draw_chunk"))
	check(tagged.size() == 1 and int(tagged[0].get_meta("level_draw_chunk")) == cells.find(5051), "moving piece node carries its chunk")
	display.show_complete(Vector2(960.0 * 2.0 + 1.0, 960.0 * 2.0 + 1.0))
	check(display.resident.size() == 9, "never more than nine slots")
	check([5151, 5152, 5251, 5252].all(func(c): return display.resident.has(c)), "the new window is resident")
	check(display.cell_evictions == 3 and display.cell_loads == 12, "three slots reused for the three new cells")
	check(display.get_child_count() == 9, "evicted cells free their nodes")
	display._retarget(Vector2(-960.0 * 2.0 + 1.0, -960.0 * 2.0 + 1.0))
	check(not display.resident.has(4949), "the cell farthest from the new focus was reused first")
	check(display._load_next() and not display._load_next() and display.cell_loads == 13, "one missing cell is read per call")
	check(display.resident.has(4949) and not display.resident.has(5252) and display.resident.size() == 9, "reading it reuses the farthest slot")
	display.set_process(false)
	display._retarget(Vector2(960.0 * 2.0 + 1.0, 960.0 * 2.0 + 1.0))
	var first_missing := -1
	for cell in display.wanted:
		if not display.resident.has(int(cell)):
			first_missing = int(cell)
			break
	var before_async: int = display.cell_loads
	display._advance_stream()
	check(first_missing >= 0 and display.pending_cell == first_missing and not display.resident.has(first_missing), "worker starts the nearest missing cell without publishing a partial cell")
	var hidden_until_complete := true
	for _i in range(120):
		if display.pending_cell < 0: break
		await process_frame
		for instance in display.pending_built:
			hidden_until_complete = hidden_until_complete and not instance.visible
		display._advance_stream()
	check(display.pending_cell < 0 and display.resident.has(first_missing) and display.cell_loads == before_async + 1, "worker publishes one complete cell before starting another")
	check(hidden_until_complete, "staged groups stay invisible until the cell is complete")
	display._drop(int(display.wanted[0])) # Exercise a focus change during a synthetic read.
	display._advance_stream()
	var stale: int = display.pending_cell
	display._retarget(Vector2(-960.0 * 2.0 + 1.0, -960.0 * 2.0 + 1.0))
	for _i in range(120):
		if display.pending_cell < 0: break
		await process_frame
		display._advance_stream()
	check(stale >= 0 and display.pending_cell < 0 and not display.resident.has(stale), "a cell outside the new focus window is discarded before publication")
	holder.queue_free()
	await process_frame
	DirAccess.remove_absolute(world_path)
	DirAccess.remove_absolute(tex_path)
	finish()

func finish() -> void:
	print("level texture stream: ", checks, " checks, ", failures, " failures")
	quit(1 if failures else 0)
