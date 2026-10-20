extends SceneTree

# Review finding 1 (review-playable-integration.md, High 1) and the audit that
# followed it: no C++ exception may cross the ChampionsFxBridge boundary, and
# every refusal is a {ok: false, refusal} dictionary. Each case drives one
# bound method into a native refusal (or a non-finite input that would reach
# float-to-int conversion). Synthetic values only.
#
# --case=<name> runs one case (a crash ends only that process); without it
# every case runs in order.

# Definition 256 and the address of fx_spawn's red blood (0x8704a8 = 8848552),
# both with made-up values.
const SYNTHETIC := '[{"site":1,"call":"particleDef_ctor","def":256,"complete":true,"size_start":2,"size_end":3,"gravity":0,"start_rgba":[128,64,32,128],"end_rgba":[0,64,255,0],"life_class":1,"kind":1,"tile":18,"callback":0},{"site":2,"call":"particleDef_ctor","def":8848552,"complete":true,"size_start":2,"size_end":2,"gravity":0,"start_rgba":[128,0,0,128],"end_rgba":[64,0,0,0],"life_class":1,"kind":0,"tile":17,"callback":0}]'

var failures := 0
var checks := 0

func check(condition: bool, label: String) -> void:
	checks += 1
	if condition:
		print("PASS ", label)
	else:
		failures += 1
		print("FAIL ", label)

func refused(result, label: String) -> void:
	var ok: bool = result is Dictionary and result.has("ok") and not result["ok"] \
		and String(result.get("refusal", "")) != ""
	check(ok, label + " -> " + str(result))

func accepted(result, label: String) -> void:
	check(result is Dictionary and result.get("ok", false), label + " -> " + str(result))

func bridge() -> Object:
	var b: Object = ClassDB.instantiate("ChampionsFxBridge")
	assert(b.load_definitions_json(SYNTHETIC)["ok"])
	return b

const NAN3 := Vector3(NAN, 0, 200)

func case_spawn_particle() -> void:
	refused(bridge().spawn_particle(256, NAN3, Vector3.ZERO, 0.0), "spawn_particle non-finite position")
	refused(bridge().spawn_particle(256, Vector3(0, 0, 200), Vector3.ZERO, INF), "spawn_particle non-finite backdate")

func case_fx_spawn() -> void:
	# id 0 spawns the (loaded) red blood around the position: a non-finite
	# position reaches exact::trunc (domain_error) inside particle_spawn.
	refused(bridge().fx_spawn(0, NAN3, true), "fx_spawn non-finite position")
	# Ids outside 0 ... 0x16 do nothing in the original (recipes.cpp step 3):
	# an accepted no-op, not a refusal.
	accepted(bridge().fx_spawn(0x7fff, Vector3(0, 0, 200), true), "fx_spawn id outside the table is a no-op")

func case_tick_paired_trail() -> void:
	# A paired trail (flag 2) with an odd number of points ages down to one
	# entry: TrailStore::update refuses (render.cpp, not documented).
	var b := bridge()
	var made = b.create_trail(1.0 / 60.0, 0, 64, 2, Color.WHITE)
	var trail: int = made["handle"] if made is Dictionary else made
	for k in 3:
		b.add_trail_point(trail, Vector3(k, 0, 200))
	var last = null
	for t in range(1, 6):
		last = b.call("tick", t, false)
		if last is Dictionary and not last.get("ok", true):
			break
	refused(last, "tick: paired trail with a single entry left")

func case_tick_clock() -> void:
	# A live, never-aged-out trail whose clock reaches 0x7fff.
	var b := bridge()
	var made = b.create_trail(500.0, 0, 64, 0, Color.WHITE)   # lifetime 30000 ticks
	var trail: int = made["handle"] if made is Dictionary else made
	var last = null
	for t in range(1, 0x8002):
		if t % 1000 == 1:
			b.add_trail_point(trail, Vector3(0, 0, 200))
		last = b.call("tick", t, false)
		if last is Dictionary and not last.get("ok", true):
			break
	refused(last, "tick: trail clock would pass 0x7fff")

func emitter_record(on_time: int) -> PackedByteArray:
	# Synthetic 0x48-byte record (EmitterRecord::parse offsets): grey, size 1,
	# rate 1 (+0x11), on time +0x2c.
	var record := PackedByteArray()
	record.resize(0x48)
	for i in 4:
		record[i] = 0x80
	record[8] = 1
	record[10] = 1
	record[14] = 1
	record[17] = 1
	record.encode_s32(44, on_time)
	return record

func case_run_emitter() -> void:
	var b := bridge()
	var made = b.add_emitter(300, emitter_record(0))
	var handle: int = made["handle"] if made is Dictionary else made
	check(handle != 0, "synthetic emitter accepted (%s)" % str(made))
	# One emission at a non-finite position reaches particles.spawn -> exact::trunc.
	refused(b.run_emitter(handle, NAN3, 1, 0, false, false, false, 0, false), "run_emitter non-finite position")
	made = b.add_emitter(301, emitter_record(10))
	handle = made["handle"] if made is Dictionary else made
	# A frame within 20 of 2^63: emitter_run's next-time trunc() is out of range (fx_exact.hpp).
	refused(b.run_emitter(handle, Vector3(0, 0, 200), 9223372036854775787, 0, false, false, false, 0, false),
		"run_emitter frame outside trunc's range")

func case_add_trail_point() -> void:
	var b := bridge()
	var made = b.create_trail(1.0, 0, 64, 0, Color.WHITE)
	var trail: int = made["handle"] if made is Dictionary else made
	refused(b.add_trail_point(trail, NAN3), "add_trail_point non-finite position")
	refused(b.add_trail_point(0x7777, Vector3(0, 0, 200)), "add_trail_point unknown handle")

func case_update_beam() -> void:
	var b := bridge()
	var made = b.create_beam(2.0, 1.0, Vector3(0, 0, 200), Vector3(20, 0, 200), 64, 2, Color.WHITE)
	var beam: int = made["handle"] if made is Dictionary else made
	refused(b.call("update_beam", beam, NAN3, Vector3(0, 20, 200), 64, -1), "update_beam non-finite position")
	refused(b.call("update_beam", 0x7777, Vector3.ZERO, Vector3.ONE, 64, -1), "update_beam unknown handle")

func case_add_decal() -> void:
	refused(bridge().call("add_decal", 12.0, NAN3, 0, 9, 0), "add_decal non-finite position")
	refused(bridge().call("add_decal", INF, Vector3(0, 0, 197), 0, 9, 0), "add_decal non-finite size")

func case_set_detail() -> void:
	refused(bridge().call("set_detail", 7), "set_detail outside 0..2")
	accepted(bridge().call("set_detail", 1), "set_detail 1")

func case_lighting() -> void:
	var b := bridge()
	b.add_light_node(Vector3(0, 0, 200), Vector3(255, 0, 0), Vector3(255, 0, 0), 100.0, 100.0, 0.0, 0, true)
	refused(b.shade_model(NAN3, Vector3.UP), "shade_model non-finite position")
	refused(b.shade_world(NAN3, Vector3.UP), "shade_world non-finite position")
	refused(b.decal_tint(NAN3), "decal_tint non-finite position")
	refused(b.model_light(NAN3), "model_light non-finite position")
	refused(b.call("add_light_node", NAN3, Vector3.ONE, Vector3.ONE, 1.0, 1.0, 0.0, 0, false), "add_light_node non-finite")
	refused(b.set_level_light(Vector3(NAN, 0, 1), Color.WHITE, Color.WHITE, 128, 1.0, 0), "set_level_light non-finite direction")
	var shaded = b.shade_world(Vector3(0, 0, 200), Vector3.UP)
	accepted(shaded, "shade_world finite")
	check(shaded is Dictionary and shaded.get("rgb") is Vector3, "shade_world returns rgb")

func case_create_failures() -> void:
	# Review finding 4: create failures are refusals, not a bare handle 0.
	var b := bridge()
	refused(b.add_emitter(0, PackedByteArray()), "add_emitter id 0")
	var short := PackedByteArray()
	short.resize(0x10)
	refused(b.add_emitter(301, short), "add_emitter short record")
	refused(b.create_trail(1.0, 99, 64, 0, Color.WHITE), "create_trail row outside the table")
	refused(b.create_beam(2.0, 1.0, Vector3.ZERO, Vector3.ONE, 64, -1, Color.WHITE), "create_beam negative depth")
	refused(b.create_beam(2.0, 1.0, NAN3, Vector3.ONE, 64, 2, Color.WHITE), "create_beam non-finite end")
	accepted(b.create_trail(1.0, 0, 64, 0, Color.WHITE), "create_trail valid")

func case_load_json() -> void:
	var b: Object = ClassDB.instantiate("ChampionsFxBridge")
	var r: Dictionary = b.load_definitions_json("[{\"def\": ")
	check(not r.get("ok", true), "malformed definitions JSON refused -> " + str(r))

func case_draw_ok() -> void:
	# Review finding 6: a frame that carries refusals is not ok.
	var b := bridge()
	b.spawn_particle(999, Vector3(0, 0, 200), Vector3.ZERO, 0.0)   # missing definition
	var view := PackedFloat32Array([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0])
	var cam: Dictionary = b.game_camera(view, 477.5)
	var frame: Dictionary = b.draw(cam, true)
	check(not frame.get("ok", true) and not frame.get("refusals", []).is_empty() and frame.has("runs"),
		"draw with refusals is not ok and still returns its runs -> " + str(frame.get("refusals")))

const CASES := ["spawn_particle", "fx_spawn", "tick_paired_trail", "tick_clock", "run_emitter",
	"add_trail_point", "update_beam", "add_decal", "set_detail", "lighting", "create_failures",
	"load_json", "draw_ok"]

func _initialize() -> void:
	var only := ""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--case="):
			only = arg.substr(7)
	for name in CASES:
		if only == "" or only == name:
			print("CASE ", name)
			call("case_" + name)
	print("FX_BRIDGE_REFUSALS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
