extends SceneTree

var failures := 0

func check(value: bool, message: String) -> void:
	if not value:
		failures += 1
		push_error(message)

func append_u64(out: PackedByteArray, value: int) -> void:
	for shift in range(0, 64, 8):
		out.append((value >> shift) & 255)

func tag(out: PackedByteArray, low: int, high: int) -> void:
	append_u64(out, low)
	append_u64(out, high)

func upload(packet: PackedByteArray, width: int, height: int, pixels: PackedByteArray) -> void:
	tag(packet, 4 | (1 << 60), 14)
	tag(packet, 1 << 48, 0x50)
	tag(packet, 0, 0x51)
	tag(packet, width | (height << 32), 0x52)
	tag(packet, 0, 0x53)
	tag(packet, (pixels.size() / 16) | (2 << 58), 0)
	packet.append_array(pixels)

func fixture() -> PackedByteArray:
	var palette := PackedByteArray()
	for i in range(256):
		palette.append_array(PackedByteArray([i, i ^ 0x55, 19, i % 129]))
	var pixels := PackedByteArray()
	pixels.resize(8192)
	pixels[0] = 8
	pixels[2] = 17
	pixels[17] = 16
	pixels[273] = 24
	var packet := PackedByteArray()
	upload(packet, 16, 16, palette)
	upload(packet, 64, 32, pixels)
	var payload := PackedByteArray()
	payload.resize(128)
	payload.encode_u16(0, 9)
	payload.encode_u16(2, 6)
	payload.encode_u16(6, packet.size() / 16)
	payload.encode_u16(8, 0x44)
	payload.encode_u32(12, 2 | (19 << 6) | (7 << 12) | (6 << 16))
	payload.encode_u32(16, 128)
	payload.append_array(packet)
	return payload

func index_fixture() -> PackedByteArray:
	var index := PackedByteArray()
	index.resize(16)
	index.encode_u32(0, 1)
	index.encode_u32(8, 16)
	index.encode_u32(12, 1)
	index.append_array(PackedByteArray([112, 105, 99, 95, 115, 119, 111, 114, 100, 0]))
	index.encode_u32(4, index.size())
	index.resize(index.size() + 10)
	index.encode_u16(26, 11)
	index.encode_u16(28, 0)
	index.encode_u16(30, 5)
	index.encode_u16(32, 9)
	index.encode_u16(34, 6)
	return index

func _initialize() -> void:
	call_deferred("run_smoke")

func run_smoke() -> void:
	var inspector = ClassDB.instantiate("ChampionsArchiveInspector")
	check(inspector != null, "Inventory extension class loaded")
	if inspector == null:
		quit(1)
		return
	var payload := fixture()
	var lookup: Dictionary = inspector.lookup_inventory_icon(index_fixture(), "pic_sword11.tex", "rta")
	check(lookup.get("ok", false) and lookup.get("found", false) and lookup.first_sector == 0 and lookup.sectors == 5,
		"Synthetic item picture name resolves through HDR")
	var dat := payload.duplicate()
	dat.resize(5 * 2048)
	if lookup.get("found", false):
		var selected := dat.slice(lookup.first_sector * 2048, (lookup.first_sector + lookup.sectors) * 2048)
		check(inspector.decode_inventory_icon(selected, "rta").get("ok", false), "HDR sector range decodes")
	check(not inspector.lookup_inventory_icon(index_fixture(), "missing.tex", "rta").found, "Missing picture name")
	var grid: Dictionary = inspector.inventory_icon_grid_placement(32, 34, 100, 2, 200.0, "rta")
	check(grid.get("ok", false) and grid.x == 195 and grid.y == 176 and grid.columns == 2,
		"Synthetic inventory grid placement")
	check(not inspector.inventory_icon_grid_placement(32, 34, 100, 2, 200.0, "norrath").ok,
		"Norrath grid profile refused")
	var raw: Dictionary = inspector.decode_inventory_icon(payload, "rta")
	check(raw.get("ok", false), "Synthetic inventory icon decoded")
	if raw.get("ok", false):
		check(raw.width == 9 and raw.height == 6 and raw.rgba.size() == 216, "Bounded dimensions and pixels")
		var expected := PackedByteArray([16, 24, 0, 0, 8, 0])
		for y in range(6):
			check(raw.rgba[y * 9 * 4] == expected[y], "Field weave and palette row " + str(y))
		var image := Image.create_from_data(9, 6, false, Image.FORMAT_RGBA8, raw.rgba)
		check(image != null and image.get_data() == raw.rgba, "Scene can create an RGBA8 image")
		var shown: Dictionary = inspector.decode_inventory_icon(payload, "rta", true)
		check(shown.get("ok", false) and shown.rgba[0] == 16 and shown.rgba[3] == 32, "Explicit display alpha")
	check(not inspector.decode_inventory_icon(payload, "norrath").ok, "Norrath refused")
	check(not inspector.decode_inventory_icon(payload.slice(0, 30), "rta").ok, "Truncated header refused")
	print("inventory-icons Godot smoke: ", "PASS" if failures == 0 else "FAIL", " (", failures, " failures)")
	quit(0 if failures == 0 else 1)
