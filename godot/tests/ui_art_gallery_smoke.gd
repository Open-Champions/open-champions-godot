extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var browser = load("res://ui/archive_browser.tscn").instantiate()
	root.add_child(browser)
	var cases = load("res://tests/ui_art_gallery_cases.gd").new()
	var failures: int = await cases.run(browser)
	quit(0 if failures == 0 else 1)
