extends "res://tests/playable_wave_smoke_textures.gd"

func step_play() -> void:
	super.step_play()
	check(scene.audio != null and scene.audio.last_tick == scene.raw_observation.ready_ticks and scene.audio.errors.is_empty(), "audio follows every native ready tick in long play")

func continuation_cases() -> void:
	await super.continuation_cases()
	check(scene.audio != null and scene.audio.errors.is_empty(), "audio survives all combined flows without timing errors")
	if scene.audio == null: return
	choose_level(directory.path_join("first.WORLD"))
	scene.combat_choice.button_pressed = false
	scene.animated_choice.button_pressed = false
	var audio: Node = scene.audio
	# g_soundFrame, g_drawFrame and the duck words are process-lifetime words
	# (research docs/audio-playback.md 260-265): one native mixer per process.
	var process_tick: int = audio.bridge.get_observation().tick
	scene.actions.play.pressed.emit()
	# No options file: g_musicVolume holds save_init's 0.5 (research docs/spec/music-volume.md).
	check(audio.music_volume_bits == 0x3f000000 and audio.bridge.get_observation().music_volume == 0x3f000000, "music volume is save_init's 0.5 with no options file")
	var before: Dictionary = scene.controller.get_observation()
	check(audio.playing and audio.bridge.get_observation().tick == process_tick, "play keeps the process mixer and its g_soundFrame")
	scene.advance_step()
	check(audio.bridge.get_observation().tick == process_tick, "no-ready network warm-up does not tick audio")
	for i in range(4): scene.advance_step()
	var sound: Dictionary = audio.bridge.get_observation()
	var level_music: bool = sound.streams[2].active and sound.streams[2].name == "mx_theme_synthetic.vag"
	var level_waiting: bool = not sound.streams[2].active and sound.streams[2].queued == 1
	check(sound.tick == process_tick + 4 and (level_music or level_waiting), "the first ready tick's poll queues the level music (its fade bit waits for the kept stream-2 volume word)")
	check(sound.failed_stream_entries == 0 and audio.pushed_frames > 0, "synthetic VAT decoded and generator receives PCM")
	await capture("audio-01-level-music")
	scene.set_paused(true)
	before = scene.controller.get_observation()
	sound = audio.bridge.get_observation()
	for i in range(4):
		scene.advance_step()
		await process_frame
	var after: Dictionary = audio.bridge.get_observation()
	check(before == scene.controller.get_observation() and sound.tick == after.tick and sound.streams == after.streams, "pause freezes native audio clock and queues while device drains only")
	check(audio.boss_enter("FatCyclops","Broon") and audio.boss_enter("FatCyclops","Broon"), "a second boss replaces the queued span")
	scene.resume_session()
	for i in range(260): scene.advance_step()
	sound = audio.bridge.get_observation()
	check(sound.streams[2].active and sound.streams[2].name == "mx_ba_broon_l.vag" and sound.streams[2].flags == 0x123, "native boss fade/queue starts documented music")
	check(audio.bridge.get_gated_flag_entries() == 0 and sound.streams[2].flags == 0x123, "boss keep bit 0x100 is stored on the entry, not counted as gated")
	await capture("audio-02-boss-music")
	check(audio.boss_leave(), "boss exit queues native stop")
	scene.set_paused(true)
	var save := directory.path_join("audio.ocplay")
	check(scene.save_session_file(save), "native continuation save with audio present")
	var saved: Dictionary = play_state(scene.controller.get_observation())
	var before_load: int = audio.bridge.get_observation().tick
	check(scene.load_session_file(save) and same_state(saved,play_state(scene.controller.get_observation())), "audio leaves continuation bytes/state authoritative")
	check(audio.bridge.get_observation().tick == before_load and audio.last_tick == scene.raw_observation.ready_ticks, "load keeps g_soundFrame; the saved ready tick becomes the client origin")
	scene.resume_session()
	for i in range(5): scene.advance_step()
	check(audio.bridge.get_observation().tick == before_load + 5 and audio.errors.is_empty(), "loaded session continues sound on exactly five ready ticks")
	await capture("audio-03-loaded-continuation")

	choose_level(directory.path_join("first.WORLD"))
	scene.animated_choice.button_pressed = true
	scene.actions.play.pressed.emit()
	var origin: int = audio.bridge.get_observation().tick
	scene.advance_step()
	for tick in range(1,37): scene.advance_step()
	check(audio.events.is_empty() and audio.bridge.get_observation().tick == origin + 36, "attack voices absent before real dispatch event")
	scene.advance_step()
	check(scene.raw_observation.hp == 84 and audio.events.size() == 2, "same native attack tick damages player and allocates two sound voices")
	for event in audio.events:
		check(event.tick == 37 and event.mask == 1 and event.role == 0 and event.sample == "synthetic-hit.vag", "ClipRole attack event routed once on tick 37")
	check(audio.bridge.get_observation().voices.size() == 2, "actual native mixer holds both hit voices")
	await capture("audio-04-tick37-hit-voices")
	# Typed consumer boundary: the native playable-audio CTest separately drives
	# real FatCyclops walk dispatch at ticks 13/49. This checks this scene receiver.
	var count: int = audio.events.size()
	audio.animation_event({"mask":4,"clip_role":0},37)
	check(audio.events.size() == count, "attack combo 0x4 cannot become a step")
	audio.animation_event({"mask":4,"clip_role":1},37)
	check(audio.events.size() == count+1 and audio.events.back().sample == "synthetic-step.vag", "explicit locomotion ClipRole reaches step voice")
	check(audio.errors.is_empty() and audio.bridge.get_observation().buffered_frames <= 12800, "bounded audio output and no failures")
	print("PLAYABLE_AUDIO_RESULT ",JSON.stringify({"native_tick":audio.last_tick,"audio_tick":audio.bridge.get_observation().tick,"events":audio.events,"pushed_frames":audio.pushed_frames,"errors":audio.errors,"banks":"synthetic","heard":false}))

	scene.animated_choice.button_pressed = false
	scene.combat_choice.button_pressed = false
	check(scene.load_session_file(directory.path_join("combined.ocplay")), "restore ordinary play for inherited refusal checks")
	scene.resume_session()
	var prior: int = scene.raw_observation.ready_ticks
	var prior_audio: int = audio.bridge.get_observation().tick
	scene._observe(scene.controller.advance_time(100000,0))
	var delta: int = scene.raw_observation.ready_ticks-prior
	check(delta > 1 and audio.bridge.get_observation().tick-prior_audio == delta and audio.last_tick == scene.raw_observation.ready_ticks, "catch-up batch emits every ready tick to audio")

func capture(name: String) -> void:
	if name == "01-entry":
		check(scene.audio != null and not scene.audio.playing, "entry menu has its own presentation mixer")
		for i in range(4): scene.audio.bridge.sound_tick(0)
		check(scene.audio.bridge.get_observation().streams[2].name == "mx_maintheme_l.vag", "entry selects documented menu music from synthetic global VAT")
	await super.capture(name)
