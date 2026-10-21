extends "res://audio/playable_audio.gd"

# Feeds the user's own banks to the released native mixer: the global MOUTH.VAT
# and the selected level's <LEVEL>.VAT, each with its ntsc_dialog.bin directory.
# Native policy selects the level music/ambient entries. Animation-event
# samples have no documented bank association, so events are counted, not voiced.
var global_bank: Dictionary = {}
var installed_global_bank: Dictionary = {}
var level_bank: Dictionary = {}
var skipped_events := 0
var bank_errors: Array[String] = []

func _vat(bank: Dictionary) -> PackedByteArray:
	return FileAccess.get_file_as_bytes(bank.vat) if bank.has("vat") else PackedByteArray()

func install_banks() -> void:
	pass # the user's banks are installed per session below

func reset(play: bool, tick := 0, key := "") -> void:
	if bridge == null: return
	bridge.attach_playback(null)
	player.stop()
	bridge.end_session()
	playing = play
	last_tick = tick
	level_key = key
	menu_accumulator = 0
	events.clear()
	# end_session keeps the process-lifetime global VAT and mixer state. The
	# disc bank is read-only, so reopening the same bank only makes a second
	# 108 MB native copy overlap the first during a level reset.
	if global_bank.is_empty():
		installed_global_bank = {}
	elif global_bank != installed_global_bank:
		if bridge.set_global_vat(global_bank.directory, _vat(global_bank), 0):
			installed_global_bank = global_bank.duplicate(true)
		else:
			bank_errors.append(bridge.get_last_error())
	if play:
		start_level(key)
	elif not global_bank.is_empty() and not bridge.play_menu_music(false) and bridge.get_observation().music_gate_open:
		bank_errors.append(bridge.get_last_error())
	player.play()
	bridge.attach_playback(player.get_stream_playback())
	update_label()

func start_level(key: String) -> void:
	level_key = key
	if level_bank.is_empty(): return
	# Initial countdown 1 is the existing explicit demo policy (research leaves it unspecified).
	if not bridge.set_level_vat(level_bank.directory, _vat(level_bank), 0, 1):
		bank_errors.append(bridge.get_last_error())

func animation_event(_event: Dictionary, _tick: int) -> void:
	skipped_events += 1

func update_label() -> void:
	if label == null: return
	label.text = "Audio: your game folder (%s level bank) · %s · ready tick %d" % ["with" if not level_bank.is_empty() else "no", "play" if playing else "menu", last_tick]
