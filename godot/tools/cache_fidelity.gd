extends SceneTree
# Used by the optional CLI helper before atomic cache publication. No writes.
func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 3 or not args[2].is_valid_int():
		quit(2)
		return
	var images: Array[Image] = []
	for path in [args[0],args[1]]:
		var file := FileAccess.open(path,FileAccess.READ)
		if file == null or file.get_length()>67108864:
			quit(2)
			return
		var encoded := file.get_buffer(file.get_length())
		var dims: Vector2i = load("res://ui/texture_upscale_cache.gd").png_size(encoded)
		if dims.x<1 or dims.y<1 or dims.x>4096 or dims.y>4096:
			quit(2)
			return
		var image := Image.new()
		if image.load_png_from_buffer(encoded)!=OK:
			quit(2)
			return
		image.convert(Image.FORMAT_RGBA8)
		images.append(image)
	var inspector = ClassDB.instantiate("ChampionsArchiveInspector")
	var result: Dictionary = inspector.texture_fidelity(images[0].get_width(),images[0].get_height(),images[0].get_data(),images[1].get_width(),images[1].get_height(),images[1].get_data(),args[2].to_int())
	print("Fidelity: ",JSON.stringify(result))
	quit(0 if result.get("accepted",false) else 1)
