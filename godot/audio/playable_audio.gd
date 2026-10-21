extends Node
# Presentation policy: fresh mixer on menu/play/load/reset; not serialized.
# During play only native ready-tick signals advance sound. Device pulls never do.
const Bank = preload("res://audio/synthetic_bank.gd")
var bridge: RefCounted
var player: AudioStreamPlayer
var scene: Control
var playing := false
var level_key := ""
var last_tick := 0
var menu_accumulator := 0.0
var pushed_frames := 0
var events: Array[Dictionary] = []
# The player's music option, supplied by the owner. The executable's stored
# g_musicVolume is not documented, so native music is refused while this is null.
var music_volume = null
var errors: Array[String] = []
var label: Label

func configure(owner_scene: Control, status: Label) -> void:
	scene = owner_scene
	label = status
	player = AudioStreamPlayer.new()
	var generator := AudioStreamGenerator.new()
	generator.mix_rate = 48000
	generator.buffer_length = 0.1
	player.stream = generator
	player.volume_db = -18.0 # Authored preview volume, not a reconstructed mix gain.
	add_child(player)
	reset(false)

func reset(play: bool, tick := 0, key := "") -> void:
	if bridge != null: bridge.attach_playback(null)
	player.stop()
	bridge = ClassDB.instantiate("ChampionsNativeAudio")
	playing = play
	last_tick = tick
	level_key = key
	menu_accumulator = 0
	events.clear()
	apply_music_volume()
	var bank := Bank.vat(["mx_maintheme_l.vag","mx_ba_broon_l.vag"])
	if not bridge.set_global_vat(bank.directory,bank.bytes,0): errors.append(bridge.get_last_error())
	if not bridge.register_vag("synthetic-step.vag",Bank.vag()): errors.append(bridge.get_last_error())
	if not bridge.register_vag("synthetic-hit.vag",Bank.vag()): errors.append(bridge.get_last_error())
	if play:
		start_level(key)
	else:
		if not bridge.play_menu_music(false): errors.append(bridge.get_last_error())
	player.play()
	bridge.attach_playback(player.get_stream_playback())
	update_label()

func set_music_volume(volume: float) -> bool:
	if bridge == null or not bridge.set_music_volume(volume): return false
	music_volume = volume
	return true

func apply_music_volume() -> void:
	if music_volume != null and not bridge.set_music_volume(music_volume): errors.append(bridge.get_last_error())

func start_level(key: String) -> void:
	level_key = key
	var bank := Bank.vat(["mx_theme_synthetic.vag","amb_synthetic.vag"])
	# Process image of the countdown is 0x2a30. The level-load store was not
	# re-executed, so this fixture still passes 1.
	if not bridge.set_level_vat(bank.directory,bank.bytes,0,1): errors.append(bridge.get_last_error())

func observe(raw: Dictionary) -> void:
	if bridge == null: return
	var play: bool = raw.get("session_mode", "") == "play"
	var tick: int = raw.get("ready_ticks",0)
	if play != playing or (play and tick < last_tick): reset(play,tick)

func animation_event(event: Dictionary, tick: int) -> void:
	var role: int = event.get("clip_role",2) # ClipRole, never derived from mask.
	var sample := "synthetic-hit.vag" if role == 0 else "synthetic-step.vag"
	var voice: int = bridge.animation_event(event.mask,role,sample,event.get("position",Vector3.ZERO))
	if voice >= 0:
		events.append({"tick":tick,"mask":event.mask,"role":role,"sample":sample,"voice":voice})
		if events.size() > 64: events.pop_front()

func ready_tick(packet: Dictionary) -> void:
	if bridge == null: return
	if not playing: reset(true,packet.tick-1,packet.level_key)
	if packet.tick != last_tick+1:
		errors.append("Audio requires consecutive native ready ticks")
		return
	if packet.level_key != level_key: start_level(packet.level_key)
	update_camera()
	for event in packet.events: animation_event(event,packet.tick)
	bridge.sound_tick(0) # Constant cosmetic draw; never consumes gameplay RNG.
	last_tick = packet.tick
	pushed_frames += bridge.pump_frames(800)
	update_label()

func boss_enter(creature_type: String, object_name: String) -> bool:
	# Caller must supply documented identity. Existing synthetic combat carriers
	# are not bosses, so this is an explicit scene fixture control/service hook.
	return playing and bridge.play_boss_music(creature_type,object_name)

func boss_leave() -> bool:
	return playing and bridge.stop_boss_music()

func update_camera() -> void:
	if scene.camera == null: return
	var position: Vector3 = scene.camera.global_position
	var right: Vector3 = scene.camera.global_basis.x.normalized()
	bridge.set_camera(Vector3(position.x,-position.z,position.y),Vector3(right.x,-right.z,right.y))

func _process(delta: float) -> void:
	if bridge == null: return
	# Entry-menu clock is cosmetic only. A selected/paused play session never
	# advances here, including returning to selection with that session retained.
	if not playing and not scene.session_started:
		menu_accumulator += minf(delta,0.1)
		while menu_accumulator >= 1.0/60.0:
			bridge.sound_tick(0)
			menu_accumulator -= 1.0/60.0
	pushed_frames += bridge.pump_frames(1600)

func update_label() -> void:
	label.text = "Audio: synthetic bank · %s · ready tick %d" % ["play" if playing else "menu",last_tick]

func release() -> void:
	if bridge != null: bridge.attach_playback(null)
	if player != null: player.stop()
	bridge = null
	set_process(false)
