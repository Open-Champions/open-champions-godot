extends "res://creature_anim/dispatch_smoke.gd"

# Review finding 8: Godot Callables stored by the creature-animation bindings
# (set_service, set_virtual_handler) outlive the object they call. Free that
# object, then run the stored callback: the binding must refuse with an explicit
# error (and drop the dead callback) instead of calling into a freed instance.
# Synthetic fixtures only (the same ones as smoke.gd / dispatch_smoke.gd).

const HOLDER := """extends Node
func zero_service(_args: Array) -> int:
	return 0
func acknowledge(_kind: String, _args: Array) -> bool:
	return true
"""

var failures := 0
# smoke.gd's check() quits with 1 but a later quit(0) wins; count failures here.
func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures += 1
		printerr("FAIL: ", message)

func holder() -> Node:
	var script := GDScript.new()
	script.source_code = HOLDER
	check(script.reload() == OK, "holder script compiles")
	var node := Node.new()
	node.set_script(script)
	return node

func creature_config() -> Dictionary:
	var fixture = load("res://tests/skinning_fixture.gd")
	var memory: Array = [[0x1024,4,0x502818], [0x502824,4,0x9000], [0x50282c,4,0xa000],
		[0x502854,4,0x6000], [0x502864,4,0x6100], [0x1048,4,0x50fd40],
		[0x50fd40+27*8+4,4,0x2ab704], [0x50fd40+28*8+4,4,0x10beb8],
		[0x10dc,4,0x3f800000], [0x13c4,2,600], [0x403a08,1,1], [0x1228,4,0x42c80000]]
	var clips := {}
	for i in range(6):
		clips[0x6000+0x100*i] = fixture.animation(64+64*i)
	for i in range(4):
		memory.append([0x502834+8*i,4,0x6200+0x100*i])
	return {"profile":"rta-us-v2.00", "class":"SmallSpider", "address":0x1000,
		"definition":0x502818, "model_resource":0x9000, "texture_resource":0xa000,
		"base_resource":0x6000, "clips":clips, "memory":memory, "model_bytes":fixture.model(), "units":1.0}

func run() -> void:
	# --- ChampionsCreatureAnimation.set_service ---------------------------------
	var creature: RefCounted = ClassDB.instantiate("ChampionsCreatureAnimation")
	check(creature.configure(creature_config()).ok, "creature configured")
	var node := holder()
	for name in ["handle_clear", "handle_link", "handle_removeNode", "Creature.v17", "Creature_applyKnockback"]:
		check(creature.set_service(name, Callable(node, "zero_service")).ok, "service installed: " + name)
	check(creature.change_state(0).ok, "services answer while their object lives")
	node.free()
	var after: Dictionary = creature.tick(1, 10)
	var error := str(after.get("error", ""))
	print("FREED_SERVICE tick: ", after.get("ok"), " ", error)
	check(not after.ok and error.contains("freed"), "a service whose object was freed refuses explicitly: " + error)
	# The dead callbacks were dropped: installing a live one makes the binding usable again.
	var live := holder()
	for name in ["handle_clear", "handle_link", "handle_removeNode", "Creature.v17", "Creature_applyKnockback"]:
		check(creature.set_service(name, Callable(live, "zero_service")).ok, "replacement service: " + name)
	check(creature.change_state(0).ok, "a live replacement service works")
	live.free()
	check(not creature.change_state(0).ok, "freed again: refused again")
	# Dropping the binding while it still holds callbacks tears them down first.
	creature = null

	# --- ChampionsAnimationDispatch.set_virtual_handler -------------------------
	var dispatch: RefCounted = ClassDB.instantiate("ChampionsAnimationDispatch")
	check(dispatch.configure(config_for("Agony")).ok, "Agony configured")
	check(dispatch.install_events("").ok, "Agony events installed")
	var handler_node := holder()
	check(dispatch.set_virtual_handler(Callable(handler_node, "acknowledge")).ok, "virtual handler installed")
	handler_node.free()
	var death: Dictionary = dispatch.select_state(20, {}, 38)
	error = str(death.get("error", ""))
	print("FREED_HANDLER select_state: ", death.get("ok"), " ", error)
	check(not death.ok and error.contains("freed"), "a virtual handler whose object was freed refuses explicitly: " + error)
	var again: Dictionary = dispatch.select_state(20, {}, 38)
	check(not again.ok and not str(again.error).contains("freed"),
		"after the refusal the dead handler is gone (no call into it): " + str(again.get("error")))
	# Non-integer Variants where raw32 integers are required (binding audit U2).
	var bad := creature_config()
	bad["address"] = 4096.5
	var other: RefCounted = ClassDB.instantiate("ChampionsCreatureAnimation")
	var refused: Dictionary = other.configure(bad)
	check(not refused.ok and str(refused.error).contains("integer"), "float address refused: " + str(refused.get("error")))
	bad = creature_config()
	bad["memory"] = [[NAN, 4, 0]]
	refused = other.configure(bad)
	check(not refused.ok and str(refused.error).contains("integer"), "NaN memory address refused: " + str(refused.get("error")))
	print("CREATURE_FREED_CALLABLE_SMOKE: ", checks, " checks, ", failures, " failures")
	quit(1 if failures else 0)
