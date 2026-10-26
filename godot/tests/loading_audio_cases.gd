extends SceneTree

# Synthetic mixer boundary cases. No disc bytes are used.
const Bank = preload("res://audio/synthetic_bank.gd")
class AudioAdapter extends "res://real_data/real_audio.gd":
	func update_camera() -> void:
		pass
var checks := 0
var failures := 0

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("Loading audio: " + message)

func _initialize() -> void:
	var bridge = ClassDB.instantiate("ChampionsNativeAudio")
	check(bridge != null, "bridge available")
	if bridge == null:
		print("LOADING_AUDIO %d/%d" % [checks - failures, checks])
		quit(1)
		return
	var bankless_tick: int = bridge.get_observation().tick
	check(bridge.service_loader_sound_pair() and bridge.get_observation().tick == bankless_tick + 2,
		"loader services sound without a level bank")
	var bank: Dictionary = Bank.vat(["mx_env_test.vag", "amb_test.vag"])
	check(bridge.set_level_vat(bank.directory, bank.bytes, 0, 60), "documented entry countdown")
	var before: Dictionary = bridge.get_observation()
	check(bridge.service_loader_sound_pair(), "loader services two sound updates")
	var after: Dictionary = bridge.get_observation()
	check(after.tick == before.tick + 2 and after.draw_frame == before.draw_frame and
		after.level_countdown == 60 and after.streams[0].queued == 0 and
		after.streams[2].queued == 0, "loader has no ambient poll or draw advance")
	var session_ok := true
	for i in range(59): session_ok = bridge.sound_tick_without_cosmetic() and session_ok
	check(session_ok and bridge.get_observation().level_countdown == 1,
		"session boundary resumes countdown and draw cadence")
	before = bridge.get_observation()
	check(not bridge.sound_tick_without_cosmetic() and bridge.get_last_error() ==
		"ambient cosmetic draw unavailable" and bridge.get_observation().tick == before.tick and
		bridge.get_observation().draw_frame == before.draw_frame and
		bridge.get_observation().level_countdown == 1,
		"missing cosmetic draw refuses without mutation")
	check(bridge.sound_tick(7) and bridge.get_observation().level_countdown == 14407,
		"supplied cosmetic draw resumes documented ambient decision")
	var audio := AudioAdapter.new()
	audio.bridge = bridge
	audio.playing = true
	audio.level_key = "synthetic"
	audio.last_tick = 0
	before = bridge.get_observation()
	audio.ready_tick({"tick": 1, "level_key": "synthetic", "events": [], "cosmetic_draw": 7})
	after = bridge.get_observation()
	check(after.tick == before.tick + 1 and after.draw_frame == before.draw_frame + 1,
		"supplied no-queue tick advances service and draw counters through adapter")
	check(audio.bank_errors.is_empty() and audio.last_tick == 1 and
		after.level_countdown == before.level_countdown - 1 and
		after.streams[0].queued == before.streams[0].queued and
		after.streams[2].queued == before.streams[2].queued,
		"supplied no-queue tick has no bank error")
	audio.bridge = null
	audio.free()
	bridge = null
	print("LOADING_AUDIO %d/%d" % [checks - failures, checks])
	quit(1 if failures else 0)
