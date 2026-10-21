extends SceneTree
# Synthetic-only smoke test for ChampionsNativeAudio: authored ADPCM bytes, no game data.
var checks := 0
var failures := 0

func check(value: bool, why: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(why)

func put_be32(bytes: PackedByteArray, offset: int, value: int) -> void:
	for i in 4:
		bytes[offset + i] = (value >> (24 - 8 * i)) & 0xff

func synthetic_vag(rate: int, blocks: int) -> PackedByteArray:
	var bytes := PackedByteArray()
	bytes.resize(0x30 + blocks * 16)
	put_be32(bytes, 0x0c, blocks * 16)
	put_be32(bytes, 0x10, rate)
	for b in blocks:
		var o := 0x30 + b * 16
		bytes[o] = 0x0c
		bytes[o + 1] = 1 if b == blocks - 1 else 0
		for j in range(2, 16):
			bytes[o + j] = 0x33
	return bytes

func vat_directory(names: Array, stereo: bool, size: int) -> PackedByteArray:
	var dir := PackedByteArray()
	dir.resize((names.size() + 1) * 0x50)
	for i in names.size() + 1:
		var p := i * 0x50
		if i < names.size():
			var raw: PackedByteArray = String(names[i]).to_ascii_buffer()
			for j in raw.size():
				dir[p + j] = raw[j]
			dir.encode_u16(p + 0x48, 1 if stereo else 0)
		dir.encode_u32(p + 0x40, i * 0x800 if i < names.size() else size)
		dir.encode_u32(p + 0x44, 24000)
	return dir

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var audio = ClassDB.instantiate("ChampionsNativeAudio")
	check(audio != null, "ChampionsNativeAudio is registered")
	var generator := AudioStreamGenerator.new()
	generator.mix_rate = 48000
	generator.buffer_length = 0.5
	var player := AudioStreamPlayer.new()
	player.stream = generator
	get_root().add_child(player)
	player.play()
	var playback = player.get_stream_playback()
	check(playback is AudioStreamGeneratorPlayback, "generator playback")
	audio.attach_playback(playback)
	check(audio.register_vag("Step.vag", synthetic_vag(16000, 40)), "register synthetic VAG")
	check(not audio.register_vag("step.VAG", synthetic_vag(8000, 4)), "duplicate keeps the first")
	check(audio.play_sample("step.vag", 1.0, 1.0, Vector3.ZERO, false, false) == 0, "direct sample")
	check(audio.animation_event(0x8, 1, "step.vag", Vector3(100, 0, 0)) == 1, "footstep hook")
	check(audio.animation_event(0x1, 0, "step.vag", Vector3(100, 0, 0)) == 2, "hit hook")
	check(audio.animation_event(0x4, 0, "step.vag", Vector3.ZERO) == -1, "combo strike is not a footstep")
	var vat := PackedByteArray()
	vat.resize(0x1000)
	check(audio.set_global_vat(vat_directory(["mx_maintheme_l.vag", "mx_ba_broon_l.vag"], true, 0x1000), vat, 0), "global VAT")
	check(audio.play_menu_music(false), "menu music queued")
	check(audio.get_observation().music_volume == 0x3f000000, "g_musicVolume starts at save_init's 0.5")
	check(audio.set_options_file_music(0) and audio.get_observation().music_volume == 0 and not audio.get_observation().music_gate_open and not audio.play_boss_music("FatCyclops", "Broon") and not audio.play_menu_music(false) and audio.get_observation().streams[2].queued == 0, "an options-file 0 closes the music gate")
	check(audio.set_options_file_music(0x3fc00000) and audio.get_observation().music_volume == 0x3fc00000, "the options-file word is copied with no clamp")
	check(not audio.set_options_file_music(-1) and not audio.set_options_file_music(0x100000000), "the options-file word is a 32-bit pattern")
	check(not audio.set_music_volume(1.5) and audio.set_music_volume(1.0) and audio.get_observation().music_volume == 0x3f800000, "slider-range values are 0..1")
	check(audio.set_options_file_music(0x3d4ccccc) and audio.play_boss_music("FatCyclops", "Broon"), "boss music queued at the gate pattern")
	check(audio.get_observation().music_gate_open and audio.play_menu_music(false), "menu music queues at the exact gate pattern")
	check(audio.get_gated_flag_entries() == 0, "flags 0x40 and 0x100 are applied by their callers, not counted as gated")
	check(audio.set_level_vat(vat_directory(["amb_x_l.vag"], false, 0x800), vat.slice(0, 0x800), 0, 100), "level VAT")
	var pushed := 0
	for frame in 20:
		audio.sound_tick(0)
		pushed += audio.pump_frames(1 << 20)
	check(pushed > 0, "frames pushed to the generator")
	check(audio.get_stream_time(2) > 0.0, "menu stream position")
	check(audio.get_stream_time(0) > 0.0, "level ambience on stream 0")
	check(audio.get_last_error() == "", "successful calls clear the last error")
	check(not audio.queue_global_entry(3, "x", 0) and audio.get_last_error() != "", "bad stream reports an error")
	audio.attach_playback(null)
	player.stop()
	player.free()
	playback = null
	print("native audio smoke: %d checks, %d failures, %d frames" % [checks, failures, pushed])
	quit(1 if failures else 0)
