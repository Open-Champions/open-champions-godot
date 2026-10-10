extends RefCounted

# Entirely authored VIF data: two identical triangles, second translated +100 X
# and hidden by default. The part mask must not hide the first mesh.
static func model(program := 0x42, hidden_mask := 2, texture_uvs := false) -> PackedByteArray:
	var first := _packet(0, program, texture_uvs)
	var second := _packet(1600, program, texture_uvs)
	var data := PackedByteArray()
	data.resize(160)
	data.encode_u64(0, 1)
	data.encode_u64(8, hidden_mask)
	data[0x12] = 2
	data[0x13] = 0xff
	data[0x25] = 5
	data[0x26] = 1
	data.encode_u32(0x28, 160)
	data.encode_u32(0x2c, 160 + first.size())
	data.encode_u32(0x30, 160 + first.size() + second.size())
	data.append_array(first)
	data.append_array(second)
	return data

static func _word(data: PackedByteArray, value: int) -> void:
	var offset := data.size()
	data.resize(offset + 4)
	data.encode_u32(offset, value & 0xffffffff)

static func _unpack(data: PackedByteArray, command: int, address: int, unsigned_values: bool, rows: Array) -> void:
	_word(data, (command << 24) | (rows.size() << 16) | 0x8000 | (0x4000 if unsigned_values else 0) | address)
	var width := 4 >> (command & 3)
	for row: Array in rows:
		for value: int in row:
			for byte in range(width):
				data.append((value >> (8 * byte)) & 255)
	while data.size() % 4 != 0:
		data.append(0)

static func _packet(shift: int, program: int, texture_uvs: bool) -> PackedByteArray:
	var data := PackedByteArray()
	_word(data, 0x04000003)
	_unpack(data, 0x69, 0, false, [[shift, 0, 0], [shift + 32, 0, 0], [shift, 16, 32]])
	_unpack(data, 0x6a, 3, false, [[0, -114, 57], [0, -114, 57], [0, -114, 57]])
	_unpack(data, 0x69, 6, true, [[3, 0, 3], [3, 0, 3], [0, 0, 0], [3, 0, 0], [6, 0, 0]])
	_unpack(data, 0x6e, 11, true, [[0, 0, 255, 3]])
	_unpack(data, 0x6d, 12, true, [[0, 0, 0, 0]])
	var tag := 3 | 0x8000 | (1 << 46) | (0x15c << 47) | (3 << 60)
	_unpack(data, 0x6c, 13, false, [[tag & 0xffffffff, tag >> 32, 0x413, 0]])
	_word(data, 0x14000000 | program)
	_word(data, 0x01000103)
	_unpack(data, 0x65, 0x81, true, [[8, 8], [56, 8], [8, 56]] if texture_uvs else [[0, 0], [64, 0], [0, 32]])
	_word(data, 0x01000101)
	while data.size() % 16 != 0:
		data.append(0)
	return data
