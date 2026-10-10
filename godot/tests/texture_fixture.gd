extends RefCounted

# Authored 4x4 transfer; no retail bytes. Pixel order deliberately varies by row.
static func rgba_texture() -> PackedByteArray:
	var data := PackedByteArray()
	data.resize(288)
	data.encode_u16(0, 4)
	data.encode_u16(2, 4)
	data.encode_u16(4, 1)
	data.encode_u16(6, 10)
	data.encode_u16(8, 0x20)
	data.encode_u32(12, 0x22001)
	data.encode_u32(16, 128)
	data.encode_u64(128, 4 | (1 << 60))
	data.encode_u64(136, 0x0e)
	data.encode_u64(144, 1 << 48)
	data.encode_u64(152, 0x50)
	data.encode_u64(168, 0x51)
	data.encode_u64(176, 4 | (4 << 32))
	data.encode_u64(184, 0x52)
	data.encode_u64(200, 0x53)
	data.encode_u64(208, 4 | 0x8000 | (2 << 58))
	for i in range(16):
		data[224 + i * 4] = i * 13 + 1
		data[225 + i * 4] = 255 - i * 11
		data[226 + i * 4] = i * 7
		data[227 + i * 4] = [0, 64, 128, 255][i % 4]
	return data

# Authored 16-color plain-row fixture. Palette is a separate 8x2 RGBA upload;
# index bytes 0x10, 0x32, ... prove low-nibble-first ordering across rows.
static func plain_four_texture() -> PackedByteArray:
	var data := rgba_texture()
	data.resize(400)
	data.encode_u16(6, 17)
	data.encode_u16(8, 0x2000)
	data.encode_u32(12, 1 | (0x14 << 6) | (2 << 12) | (2 << 16))
	data.encode_u64(176, 8 | (2 << 32))
	data.encode_u64(208, 4 | (2 << 58))
	data.encode_u64(288, 4 | (1 << 60))
	data.encode_u64(296, 0x0e)
	data.encode_u64(304, (1 << 48) | (0x14 << 56))
	data.encode_u64(312, 0x50)
	data.encode_u64(328, 0x51)
	data.encode_u64(336, 4 | (4 << 32))
	data.encode_u64(344, 0x52)
	data.encode_u64(360, 0x53)
	data.encode_u64(368, 1 | 0x8000 | (2 << 58))
	for i in range(8):
		data[384 + i] = (2 * i) | ((2 * i + 1) << 4)
	return data

# Authored 256-color texture: PSMCT32 8x8 and PSMT8 16x16 share one GS
# block. Uniform indices make expected colors independent of address mapping.
static func eight_bit_texture(index: int) -> PackedByteArray:
	var data := rgba_texture()
	data.resize(1600)
	data.encode_u16(0, 16)
	data.encode_u16(2, 16)
	data.encode_u16(4, 5)
	data.encode_u16(6, 92)
	data.encode_u16(8, 4)
	data.encode_u32(12, 2 | (0x13 << 6) | (4 << 12) | (4 << 16))
	data.encode_u64(176, 16 | (16 << 32))
	data.encode_u64(208, 64 | (2 << 58))
	for i in range(256):
		data[224 + i * 4] = i
		data[225 + i * 4] = 255 - i
		data[226 + i * 4] = (i * 3) & 255
		data[227 + i * 4] = 64 if i == 8 else 128
	data.encode_u64(1248, 4 | (1 << 60))
	data.encode_u64(1256, 0x0e)
	data.encode_u64(1264, 1 << 48)
	data.encode_u64(1272, 0x50)
	data.encode_u64(1288, 0x51)
	data.encode_u64(1296, 8 | (8 << 32))
	data.encode_u64(1304, 0x52)
	data.encode_u64(1320, 0x53)
	data.encode_u64(1328, 16 | 0x8000 | (2 << 58))
	for i in range(256):
		data[1344 + i] = index
	return data

static func lmp(names: Array[String], payloads: Array[PackedByteArray]) -> PackedByteArray:
	var data := PackedByteArray()
	data.resize(4 + names.size() * 12)
	data.encode_u32(0, names.size())
	for i in range(names.size()):
		data.encode_u32(4 + i * 12, data.size())
		data.append_array(names[i].to_utf8_buffer())
		data.append(0)
	for i in range(payloads.size()):
		data.encode_u32(8 + i * 12, data.size())
		data.encode_u32(12 + i * 12, payloads[i].size())
		data.append_array(payloads[i])
	return data

static func two_lmps(first: PackedByteArray, second: PackedByteArray) -> PackedByteArray:
	var data := PackedByteArray()
	data.resize(120)
	for i in range(2):
		var name := ("first.LMP" if i == 0 else "second.LMP").to_ascii_buffer()
		for j in range(name.size()):
			data[i * 40 + j] = name[j]
		var child := first if i == 0 else second
		data.encode_u32(i * 40 + 32, data.size())
		data.encode_u32(i * 40 + 36, child.size())
		data.append_array(child)
	return data
