extends SceneTree
# Synthetic options image through the native front-end parser, then direct
# game-folder script entry's music preparation. No retail files are read.
var checks := 0
var failures := 0

func _initialize() -> void:
	call_deferred("run")

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error(label)

func save(path: String, data: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_buffer(data)
	file.close()

func run() -> void:
	var service = ClassDB.instantiate("ChampionsFrontendService")
	check(service != null and service.boot(PackedByteArray()).get("ok", false), "native front end boots without a file")
	if service == null:
		quit(1)
		return
	var options: PackedByteArray = service.options_image()
	var path := "user://session-music-smoke-%d/options.img" % OS.get_process_id()
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	root.add_child(scene)
	scene.frontend.options_path = path
	check(scene._sync_saved_music() and scene.audio.music_volume_bits == 0x3f000000, "missing file keeps the documented default")
	options.encode_u32(8, 0)
	save(path, options)
	check(scene._sync_saved_music() and scene.audio.music_volume_bits == 0, "saved zero reaches the session audio word")
	options.encode_u32(8, 0x3fc00000)
	save(path, options)
	check(scene._sync_saved_music() and scene.audio.music_volume_bits == 0x3fc00000, "saved value is copied without clamping")
	# A closed first-theme gate is an expected no-op in the real-data reset.
	# Mark the global bank as already installed so this checks the menu route
	# without requiring a private or synthetic bank payload.
	var real_audio = load("res://real_data/real_audio.gd").new()
	root.add_child(real_audio)
	real_audio.configure(scene, null)
	real_audio.global_bank = {"directory": PackedByteArray()}
	real_audio.installed_global_bank = real_audio.global_bank.duplicate(true)
	real_audio.bank_errors.clear()
	check(real_audio.load_options_file_music(0), "zero options word reaches the real-data mixer")
	real_audio.reset(false)
	check(real_audio.bank_errors.is_empty(), "closed first-theme gate is not a bank refusal")
	real_audio.free()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	scene.free()
	print("session music smoke: %d checks, %d failures" % [checks, failures])
	quit(1 if failures else 0)
