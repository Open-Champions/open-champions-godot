extends SceneTree

# The level-script opt-in is off until a caller passes an image. An empty
# image is refused and does not change the play observation.
var failures := 0

func check(ok: bool, why: String) -> void:
	if not ok:
		failures += 1
		push_error(why)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	check(ClassDB.class_has_method("ChampionsSessionController", "enable_level_script"), "level script method is bound")
	var controller = ClassDB.instantiate("ChampionsSessionController")
	check(controller != null, "session controller exists")
	if controller == null:
		print("script commands smoke FAIL")
		quit(1)
		return
	var caps: Dictionary = controller.get_play_capabilities()
	check(caps.get("level_script_available", false) == true, "level script is available")
	check(caps.get("level_script_enabled", true) == false, "level script starts disabled")
	var before: Dictionary = controller.get_observation()
	var refused: Dictionary = controller.enable_level_script(PackedByteArray(), PackedByteArray())
	check(refused.get("ok", true) == false, "an empty script image is refused")
	check(controller.get_observation() == before, "a refused script leaves the observation unchanged")
	var started: Dictionary = controller.start_play_session()
	check(started.get("ok", false), "play still starts")
	var playing: Dictionary = controller.get_observation()
	var again: Dictionary = controller.enable_level_script(PackedByteArray(), PackedByteArray())
	check(again.get("ok", true) == false, "an empty script is refused during play")
	check(controller.get_observation() == playing, "the refusal does not advance play")
	controller.advance(0)
	check(controller.get_observation().get("ok", false), "a tick without a script still advances")
	check(controller.script_command_counts().get("ok", true) == false, "counts stay absent until a script is enabled")
	print("script commands smoke ", "PASS" if failures == 0 else "FAIL")
	quit(0 if failures == 0 else 1)
