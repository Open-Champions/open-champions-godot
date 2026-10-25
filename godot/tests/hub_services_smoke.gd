extends SceneTree

var failures := 0
var checks := 0

func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1
		printerr(label)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var scene = load("res://scenes/real_data_play.tscn").instantiate()
	scene.automatic_steps = false
	root.add_child(scene)
	await process_frame
	check(scene.controller.hub_service_targets().is_empty(), "no invented hub records without a level")
	for index in [-1, 0, 2147483647]:
		var result: Dictionary = scene.controller.hub_service_use(index, false)
		check(not result.get("ok", true), "missing hub/player refuses direct use")
		check(not result.get("request_save", true), "refusal requests no save")
		check(not str(result.get("error", "")).is_empty(), "refusal includes a diagnostic")
	check(not scene.hub_services.choice.visible, "service selector hidden outside a hub")
	check(not scene.actions.hub_service.visible, "service action hidden outside a hub")
	check(not scene.hub_services.use_selected().get("ok", true), "empty selector refuses")
	print("HUB_SERVICES_SMOKE ", checks, " checks, ", failures, " failures")
	scene.queue_free()
	await process_frame
	quit(1 if failures else 0)
