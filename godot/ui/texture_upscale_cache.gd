extends RefCounted

const DIRECTORY := "user://texture-upscale-cache/v2"
const MODEL := "realesrgan-x4plus"

static func key(width: int, height: int, pixels: PackedByteArray, factor: int) -> String:
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(("oc-upscale-v2\n%d\n%d\nexternal-rgba\n%d\n%s\n" % [width, height, factor, MODEL]).to_utf8_buffer())
	hash.update(pixels)
	return hash.finish().hex_encode()

static func png_size(bytes: PackedByteArray) -> Vector2i:
	if bytes.size() < 24 or bytes.slice(0, 8) != PackedByteArray([137,80,78,71,13,10,26,10]) or bytes.slice(12,16).get_string_from_ascii() != "IHDR":
		return Vector2i.ZERO
	var dims := Vector2i.ZERO
	for i in range(4):
		dims.x = (dims.x << 8) | bytes[16+i]
		dims.y = (dims.y << 8) | bytes[20+i]
	return dims

static func valid_input(width: int, height: int, pixels: PackedByteArray, factor: int) -> bool:
	return width >= 1 and width <= 1024 and height >= 1 and height <= 1024 and factor >= 2 and factor <= 4 and pixels.size() == width*height*4

static func replacement(width: int, height: int, pixels: PackedByteArray, factor: int) -> Dictionary:
	if not valid_input(width,height,pixels,factor):
		return {"error": "Invalid bounded cache input or factor."}
	var path := DIRECTORY.path_join(key(width, height, pixels, factor) + ".png")
	if not FileAccess.file_exists(path):
		return {"error": "Cache miss. Export the cache input, run the local helper, then preview again."}
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return {"error": "Replacement PNG cannot be read."}
	var byte_count := file.get_length()
	if byte_count > 67108864:
		return {"error": "Replacement PNG exceeds 64 MiB."}
	if png_size(file.get_buffer(24)) != Vector2i(width*factor, height*factor):
		return {"error": "Replacement PNG has invalid dimensions or header."}
	file.seek(0)
	var image := Image.new()
	if image.load_png_from_buffer(file.get_buffer(byte_count)) != OK:
		return {"error": "Replacement PNG could not be decoded."}
	if image.get_width() != width*factor or image.get_height() != height*factor:
		return {"error": "Replacement dimensions changed during decoding."}
	image.convert(Image.FORMAT_RGBA8)
	var inspector = ClassDB.instantiate("ChampionsArchiveInspector")
	var fidelity: Dictionary = inspector.texture_fidelity(width,height,pixels,image.get_width(),image.get_height(),image.get_data(),factor)
	if not fidelity.get("accepted",false):
		return {"error": "AI replacement rejected: box error %d/%d, new palette pixels %d, flat changes %d, hard alpha changes %d." % [fidelity.get("error_numerator",0),fidelity.get("error_denominator",1),fidelity.get("new_palette_pixels",0),fidelity.get("flat_region_changes",0),fidelity.get("hard_alpha_changes",0)], "fidelity": fidelity}
	return {"image": image, "fidelity": fidelity}

static func export_input(width: int, height: int, pixels: PackedByteArray, factor: int) -> String:
	if not valid_input(width,height,pixels,factor):
		return "Invalid bounded cache input or factor."
	var directory := ProjectSettings.globalize_path(DIRECTORY)
	var ancestor := directory
	while not ancestor.is_empty():
		if FileAccess.file_exists(ancestor.path_join(".git")) or DirAccess.dir_exists_absolute(ancestor.path_join(".git")):
			return "Cache export must stay outside a repository."
		var parent := ancestor.get_base_dir()
		if parent == ancestor:
			break
		ancestor = parent
	if DirAccess.make_dir_recursive_absolute(directory) != OK:
		return "Could not create the user cache directory."
	var identity := key(width, height, pixels, factor)
	var input := Image.create_from_data(width,height,false,Image.FORMAT_RGBA8,pixels)
	var encoded := input.save_png_to_buffer()
	var file := FileAccess.open(DIRECTORY.path_join(identity + ".input.png"),FileAccess.WRITE)
	if file == null:
		return "Could not write cache input."
	file.store_buffer(encoded)
	file.close()
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(encoded)
	var manifest := {"key": identity, "width": width, "height": height, "factor": factor, "model": MODEL, "alpha_mode": "decoded-rgba", "fidelity_policy": "exact-box-palette-alpha-v1", "input_sha256": hash.finish().hex_encode()}
	file = FileAccess.open(DIRECTORY.path_join(identity + ".json"),FileAccess.WRITE)
	if file == null:
		return "Could not write cache settings."
	file.store_string(JSON.stringify(manifest))
	return "Cache input saved locally: " + directory + "/" + identity + ".json"
