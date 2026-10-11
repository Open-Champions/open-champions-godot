extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	var cases = load("res://tests/session_sandbox_cases.gd").new()
	var passed: bool = await cases.run(scene)
	quit(0 if passed else 1)
