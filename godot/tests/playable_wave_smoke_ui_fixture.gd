extends RefCounted

# Authored packet/index recipe from inventory_icons_smoke; no retail bytes.
static func append_u64(out: PackedByteArray, value: int) -> void:
	for shift in range(0, 64, 8):
		out.append((value >> shift) & 255)

static func tag(out: PackedByteArray, low: int, high: int) -> void:
	append_u64(out, low)
	append_u64(out, high)

static func upload(packet: PackedByteArray, width: int, height: int, pixels: PackedByteArray) -> void:
	tag(packet, 4 | (1 << 60), 14)
	tag(packet, 1 << 48, 0x50)
	tag(packet, 0, 0x51)
	tag(packet, width | (height << 32), 0x52)
	tag(packet, 0, 0x53)
	tag(packet, (pixels.size() / 16) | (2 << 58), 0)
	packet.append_array(pixels)

static func fixture() -> PackedByteArray:
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

static func index_fixture() -> PackedByteArray:
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

