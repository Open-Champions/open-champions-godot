extends RefCounted

# Authored inputs only. Three corners have independently chosen 2/3/4-joint
# influences. Source positions are (0,0,0), (2,0,0), (0,1,2); normal agrees
# with that winding. Helpers reuse this project's authored VIF writer.
static func model(mode := "weighted") -> PackedByteArray:
	var writer = load("res://tests/model_fixture.gd")
	var weights: Array = [[0,64,4,191], [0,100,4,100], [8,55,255,0], [0,0,4,55], [8,100,12,100]]
	if mode == "rigid63":
		weights = [[252,0,255,3]]
	elif mode == "zero_invalid":
		weights = [[0,0,12,255], [0,0,12,255], [0,0,12,255]]
	var packet := PackedByteArray()
	writer._word(packet, 0x04000003)
	writer._unpack(packet, 0x69, 0, false, [[0,0,0], [32,0,0], [0,16,32]])
	writer._unpack(packet, 0x6a, 3, false, [[0,-114,57], [0,-114,57], [0,-114,57]])
	writer._unpack(packet, 0x69, 6, true, [[3,0,3], [3,0,3], [0,0,0], [3,0,0], [6,0,0]])
	writer._unpack(packet, 0x6e, 11, true, weights)
	var copies := 11 + weights.size()
	writer._unpack(packet, 0x6d, copies, true, [[0,0,0,0]])
	var tag := 3 | 0x8000 | (1 << 46) | (0x15c << 47) | (3 << 60)
	writer._unpack(packet, 0x6c, copies + 1, false, [[tag & 0xffffffff,tag >> 32,0x413,0]])
	writer._word(packet, 0x14000042)
	writer._word(packet, 0x01000103)
	writer._unpack(packet, 0x65, 0x81, true, [[0,0], [64,0], [0,32]])
	writer._word(packet, 0x01000101)
	while packet.size() % 16 != 0:
		packet.append(0)
	var data := PackedByteArray()
	data.resize(128)
	data.encode_u64(0, 1)
	data[0x12] = 1
	data[0x13] = 0xff
	data[0x25] = 4
	data[0x26] = 1
	data.encode_u32(0x28, 128)
	data.encode_u32(0x2c, 128 + packet.size())
	data.append_array(packet)
	return data

# Four independent roots: Z90, X90, identity, Y180. All inverse-bind values
# are zero. Root0 translates at one unit/tick, reversing at tick4; duplicate
# tick4 records exercise stream order. No profile/rig semantic match is implied.
static func animation(root_x := 64) -> PackedByteArray:
	var writer = load("res://tests/animation_fixture.gd")
	return writer._clip([0,0,0,0], [[root_x,128,192], [-64,256,128], [320,-128,64], [-256,64,-128]],
		[[2896,0,0,2896], [2896,2896,0,0], [4096,0,0,0], [0,0,4096,0]],
		[[0,1,0,[256,0,0]], [4,1,0,[0,256,0]], [0,1,0,[-256,0,0]]], 8)

static func invalid_ancestor() -> PackedByteArray:
	var writer = load("res://tests/animation_fixture.gd")
	var data: PackedByteArray = writer.invalid_ancestor()
	for byte in range(0x48, 0x50):
		data[byte] = 0
	return data

static func full63() -> PackedByteArray:
	var writer = load("res://tests/animation_fixture.gd")
	var data: PackedByteArray = writer.mask63()
	for byte in range(0x48, 0x50):
		data[byte] = 0
	return data
