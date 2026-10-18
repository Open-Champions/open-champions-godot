extends RefCounted
# Authored test tones only. Names used by native selectors identify synthetic
# replacements; no retail PCM, music data, or inferred ANM sound associations.
static func be32(bytes: PackedByteArray, at: int, value: int) -> void:
	for i in 4: bytes[at+i] = (value >> (24-8*i)) & 255

static func vag() -> PackedByteArray:
	var bytes := PackedByteArray()
	bytes.resize(0x30 + 160*16)
	be32(bytes,12,160*16)
	be32(bytes,16,16000)
	for b in 160:
		bytes[0x30+b*16] = 8
		bytes[0x31+b*16] = 1 if b == 159 else 0
		for j in range(2,16): bytes[0x30+b*16+j] = 0x33 if b%8 < 4 else 0xdd
	return bytes

static func vat(names: Array) -> Dictionary:
	var directory := PackedByteArray()
	directory.resize((names.size()+1)*0x50)
	var bytes := PackedByteArray()
	bytes.resize(names.size()*0x800)
	for i in names.size()+1:
		var at := i*0x50
		directory.encode_u32(at+0x40,i*0x800)
		directory.encode_u32(at+0x44,24000)
		if i == names.size(): break
		var name: PackedByteArray = String(names[i]).to_ascii_buffer()
		for j in name.size(): directory[at+j] = name[j]
		for b in 128:
			bytes[i*0x800+b*16] = 9
			for j in range(2,16): bytes[i*0x800+b*16+j] = 0x22 if b%16 < 8 else 0xee
	return {"directory":directory,"bytes":bytes}
