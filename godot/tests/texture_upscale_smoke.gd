extends SceneTree
const Fixture = preload("res://tests/texture_fixture.gd")
const Cache = preload("res://ui/texture_upscale_cache.gd")
var failures := 0
func check(value: bool, text: String) -> void:
	if not value:
		failures += 1
		push_error(text)
func _initialize() -> void:
	call_deferred("_run")
func _run() -> void:
	var browser = load("res://ui/archive_browser.tscn").instantiate()
	root.add_child(browser)
	var path := "user://upscale-synthetic.LMP"
	var file := FileAccess.open(path,FileAccess.WRITE)
	file.store_buffer(Fixture.lmp(["art.tex"],[Fixture.rgba_texture()]))
	file.close()
	browser.open_archive(path)
	check(browser.inspector != null and browser.rows.size()==1,"Real extension and fixture loaded")
	if browser.rows.is_empty():
		quit(1)
		return
	browser.preview_row_index = 0
	browser._preview_selected()
	check(browser.upscale_filter.selected == 0 and browser.preview_after.texture == null,"Off by default")
	check(browser.upscale_factor.selected==0 and "faithful default" in browser.upscale_filter.get_item_text(1),"Measured nearest 2x is first enabled default")
	var original: PackedByteArray = browser.preview_rgba.duplicate()
	for filter in range(1,4):
		for factor in range(2,5):
			browser.upscale_filter.select(filter)
			browser.upscale_factor.select(factor-2)
			browser._update_upscale()
			check(browser.preview_after.texture != null,"Before/after texture")
			check(browser.upscale_rgba.size() == 4*4*factor*factor*4,"Scaled byte count")
			check(browser.preview_rgba == original,"Original retained")
	browser.preview_alpha.set_pressed(true)
	browser._render_preview()
	var alpha_display: PackedByteArray = browser.preview_after.texture.get_image().get_data()
	check(alpha_display[0]==0 and alpha_display[1]==0 and alpha_display[2]==0 and alpha_display[3]==255,"After alpha channel display regenerates ordinary alpha")
	var zero_alpha := Fixture.rgba_texture()
	for i in range(16):
		zero_alpha[224+i*4] = 90
		zero_alpha[225+i*4] = 60
		zero_alpha[226+i*4] = 30
		zero_alpha[227+i*4] = 0
	var alpha_path := "user://upscale-synthetic-alpha.LMP"
	var alpha_file := FileAccess.open(alpha_path,FileAccess.WRITE)
	alpha_file.store_buffer(Fixture.lmp(["alpha.tex"],[zero_alpha]))
	alpha_file.close()
	var alpha_listing: Dictionary = browser.inspector.inspect_archive(alpha_path)
	var raw: Dictionary = browser.inspector.inspect_texture_upscaled(alpha_path,0,"rta",alpha_listing.fingerprint,"bicubic",2)
	var opaque: Dictionary = browser.inspector.inspect_texture_upscaled(alpha_path,0,"rta",alpha_listing.fingerprint,"bicubic",2,true)
	check(raw.ok and raw.upscaled_rgba[0]==90 and raw.upscaled_rgba[3]==0,"Uniform hidden RGB preserved")
	check(opaque.ok and opaque.rgba[3]==0 and opaque.upscaled_rgba[0]==90 and opaque.upscaled_rgba[1]==60 and opaque.upscaled_rgba[2]==30 and opaque.upscaled_rgba[3]==255,"Opaque RGB resampling retains model colors and original alpha")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(alpha_path))
	check(Cache.replacement(2147483647,4,original,4).has("error"),"Cache bounds before PNG load")
	check(Cache.export_input(4,4,original,5)=="Invalid bounded cache input or factor.","Cache export factor refused")
	var identity: String = Cache.key(4,4,original,2)
	check(identity != Cache.key(4,4,original,3),"Factor changes identity")
	var altered := original.duplicate()
	altered[0] ^= 1
	check(identity != Cache.key(4,4,altered,2),"Pixels change identity")
	browser.upscale_filter.select(4)
	browser.upscale_factor.select(0)
	browser._update_upscale()
	check(browser.preview_after.texture == null,"Cache miss clears after")
	var replacement := Image.create(8,8,false,Image.FORMAT_RGBA8)
	replacement.fill(Color(1,0,0,1))
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(Cache.DIRECTORY))
	var cached_path: String = Cache.DIRECTORY.path_join(identity+".png")
	replacement.save_png(cached_path)
	browser._update_upscale()
	check(browser.preview_after.texture == null,"Invented cache colors rejected")
	var exact := Image.create_from_data(4,4,false,Image.FORMAT_RGBA8,original)
	exact.resize(8,8,Image.INTERPOLATE_NEAREST)
	exact.save_png(cached_path)
	browser._update_upscale()
	check(browser.preview_after.texture != null,"Faithful cache hit displayed")
	var bad_alpha := exact.get_data()
	bad_alpha[3] ^= 1
	Image.create_from_data(8,8,false,Image.FORMAT_RGBA8,bad_alpha).save_png(cached_path)
	check(Cache.replacement(4,4,original,2).has("error"),"Alpha mutation rejected")
	var metrics: Dictionary = browser.inspector.texture_fidelity(4,4,original,8,8,exact.get_data(),2)
	check(metrics.accepted and metrics.error_numerator==0 and metrics.new_palette_pixels==0,"Same native fidelity gate")
	check(not browser.inspector.texture_fidelity(2147483647,4,original,8,8,exact.get_data(),2).accepted,"Binding refuses overflowing source dimensions")
	var exported: String = Cache.export_input(4,4,original,2)
	check("Cache input saved locally" in exported,"Explicit cache input export")
	var manifest_path: String = Cache.DIRECTORY.path_join(identity+".json")
	var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(manifest_path))
	check(manifest.key == identity and manifest.factor == 2 and manifest.model == Cache.MODEL,"Export settings")
	for suffix in [".json", ".input.png"]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(Cache.DIRECTORY.path_join(identity+suffix)))
	var wrong := Image.create(1,1,false,Image.FORMAT_RGBA8)
	wrong.save_png(cached_path)
	browser._update_upscale()
	check(browser.preview_after.texture == null,"Wrong cache dimensions refused")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(cached_path))
	var rejected: Dictionary = browser.inspector.inspect_texture_upscaled(path,0,"rta",browser.current_fingerprint,"nearest",999999)
	check(not rejected.ok,"Factor refusal through binding")
	rejected = browser.inspector.inspect_texture_upscaled(path,0,"norrath",browser.current_fingerprint,"nearest",2)
	check(not rejected.ok,"Norrath refusal")
	var mutation := FileAccess.open(path,FileAccess.WRITE)
	var changed := Fixture.rgba_texture()
	changed[224] ^= 1
	mutation.store_buffer(Fixture.lmp(["art.tex"],[changed]))
	mutation.close()
	browser._update_upscale()
	check(browser.preview_rgba.is_empty() and browser.preview_after.texture == null,"External cache revalidates stale archive")
	browser.open_archive(path)
	browser.preview_row_index = 0
	browser.upscale_filter.select(0)
	browser._preview_selected()
	browser.upscale_filter.select(1)
	browser._update_upscale()
	if "--visual" in OS.get_cmdline_user_args():
		browser.preview_alpha.set_pressed(false)
		browser.upscale_filter.select(3)
		browser.upscale_factor.select(2)
		browser._update_upscale()
		await process_frame
		(browser.get_child(1) as ScrollContainer).scroll_vertical = 10000
		await process_frame
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("user://texture-upscale-synthetic-preview.png")
		print("Synthetic screenshot: ",ProjectSettings.globalize_path("user://texture-upscale-synthetic-preview.png"))
	browser._clear()
	check(browser.preview_image.texture==null and browser.preview_after.texture==null,"Clear both views")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	print("Texture upscale smoke: ","PASS" if failures==0 else "FAIL")
	quit(0 if failures==0 else 1)
