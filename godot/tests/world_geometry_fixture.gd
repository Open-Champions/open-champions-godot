extends RefCounted

# Same authored VIF46 packet as generate_world_geometry_vectors.packet(3,0).
# Signed local vertices (-12,4,-8),(20,-16,12),(8,24,0), one triangle.
# Translation (16,-8,4) happens before quarter scale and a +90 degree XY turn.
# Expected native vertices: (1,1,-1),(6,9,4),(-4,6,1).
const PACKET_HEX := "000000000000000000000000030000040100000500000369f4ff0400f8ff1400f0ff0c000800180000000000000000050000036a810000007fff03fc7e0000000040056903000000030003000000030000800000000003800000000006000000000000000040016d00000000000000000040016c038000000000ae3013040000000000004600001403010001004003650000ffff7100eeffe200ddff0101000100000011000000000000000000000000"

static func world(collision: PackedByteArray, flags := 0) -> PackedByteArray:
	var data := collision.duplicate()
	var offset := (data.size() + 15) & ~15
	data.resize(offset)
	var packet := PACKET_HEX.hex_decode()
	data.append_array(packet)
	data.encode_u32(0x84, offset)
	data.encode_u32(0x88, packet.size() / 16)
	for i in range(6):
		data.encode_float(0x8c + i * 4, -128.0 if i < 3 else 128.0)
	data.encode_u32(0xa8, flags)
	data.encode_u32(0xac, 16)
	data.encode_u32(0xb0, 0xfffffff8)
	data.encode_u32(0xb4, 4)
	data.encode_u16(0xb8, 1)
	data.encode_u16(0xba, 0)
	data.encode_u16(0xbc, 32767)
	data.encode_u16(0xbe, 0)
	return data
