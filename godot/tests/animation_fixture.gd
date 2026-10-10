extends RefCounted

# Original synthetic .anm bytes. Fields are MSB-first within little-endian
# 16-bit words, independently written from the native parser implementation.
static func _field(bits: Array[int], value: int, width: int) -> void:
	for shift in range(width - 1, -1, -1):
		bits.append((value >> shift) & 1)

static func _clip(depths: Array, positions: Array, rotations: Array, keys: Array, duration: int, masked_joint := -1) -> PackedByteArray:
	var bind_offset: int = (0x70 + depths.size() + 3) & ~3
	var key_offset := bind_offset + depths.size() * 8
	var data := PackedByteArray()
	data.resize(key_offset)
	data.encode_u32(0, depths.size())
	data.encode_u32(4, duration)
	data.encode_u32(8, key_offset)
	data.encode_u32(12, bind_offset)
	data.encode_u32(16, 0x70)
	if masked_joint >= 0:
		# Byte assignment avoids signed int64 ambiguity for bit63.
		data[0x48 + masked_joint / 8] = 1 << (masked_joint % 8)
	var bits: Array[int] = []
	for joint in range(depths.size()):
		data[0x70 + joint] = depths[joint]
		_field(bits, 15, 4)
		for value in positions[joint]:
			_field(bits, value, 16)
		_field(bits, 15, 4)
		for value in rotations[joint]:
			_field(bits, value, 16)
	for key in keys:
		_field(bits, key[0], 8)
		_field(bits, key[1], 1)
		_field(bits, key[2], 6)
		_field(bits, 15, 4)
		for value in key[3]:
			_field(bits, value, 16)
	_field(bits, 255, 8)
	while bits.size() % 32 != 0:
		bits.append(0)
	var stream := PackedByteArray()
	stream.resize(bits.size() / 8)
	for index in range(bits.size()):
		var shift := 15 - index % 16
		stream[(index / 16) * 2 + shift / 8] |= bits[index] << (shift % 8)
	data.append_array(stream)
	return data

# Root Z90, child X90, independent root. At tick1.5 joint origins are
# (2.5,2,3), (2.5,4,4), (-2,1,-1); at tick4 root is (5,2,3).
# Two keys at tick4 deliberately replace velocity in stored order.
static func hierarchy() -> PackedByteArray:
	return _clip([0, 1, 0], [[64,128,192], [128,0,64], [-128,64,-64]],
		[[2896,0,0,2896], [2896,2896,0,0], [4096,0,0,0]],
		[[0,1,0,[256,0,0]], [4,1,0,[0,256,0]], [0,1,0,[-256,0,0]]], 8, 0)

static func invalid_ancestor() -> PackedByteArray:
	return _clip([0,1,2,0], [[0,0,0], [64,0,0], [0,64,0], [192,-128,64]],
		[[0,0,0,0], [4096,0,0,0], [4096,0,0,0], [4096,0,0,0]], [], 0, 1)

static func mask63() -> PackedByteArray:
	var depths: Array = []
	var positions: Array = []
	var rotations: Array = []
	for index in range(64):
		depths.append(0)
		positions.append([index * 64,0,0])
		rotations.append([4096,0,0,0])
	return _clip(depths, positions, rotations, [], 0, 63)

static func empty() -> PackedByteArray:
	return _clip([], [], [], [], 0)

static func all_invalid() -> PackedByteArray:
	return _clip([0], [[64,128,192]], [[0,0,0,0]], [], 0)

# This valid duration/velocity produces a model origin above 5e11 on all axes.
# It exercises rendering precision without mutable Dictionary-only input.
static func large_coordinate() -> PackedByteArray:
	return _clip([0], [[0,0,0]], [[4096,0,0,0]],
		[[0,1,0,[32767,32767,32767]]], 4294967295)
