extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var scene = load("res://ui/archive_browser.tscn").instantiate()
	if scene.get_script() == null:
		push_error("Browser script failed to load")
		quit(1)
		return
	root.add_child(scene)
	var smoke = load("res://tests/smoke_cases.gd").new()
	var failures: int = smoke.run(scene.inspector, scene)
	print("Open Champions browser smoke: ", "PASS" if failures == 0 else "FAIL")
	quit(0 if failures == 0 else 1)
