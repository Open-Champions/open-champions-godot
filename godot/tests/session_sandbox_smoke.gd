extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var scene = load("res://scenes/session_sandbox.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	var cases = load("res://tests/session_sandbox_cases.gd").new()
	var passed: bool = await cases.run(scene)
	# Awaited case locals and queued replacement subtrees must unwind before
	# renderer shutdown. Free the real scene while RenderingServer still lives.
	cases = null
	scene.queue_free()
	scene = null
	await process_frame
	await process_frame
	quit(0 if passed else 1)
