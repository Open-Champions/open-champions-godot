extends SceneTree

var checks := 0
var failures := 0

func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures += 1
		printerr(message)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	check(ClassDB.class_exists("ChampionsSessionController"), "native extension must be imported before the death UI check")
	if failures:
		quit(1)
		return
	# Public synthetic UI checks. Real Player/World death is covered in native
	# tests and in the private playthrough, never replaced by this draw fixture.
	root.size = Vector2i(640, 448)
	var background := ColorRect.new()
	background.color = Color.WHITE
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_child(background)
	var fade = load("res://ui/death_fade.gd").new()
	root.add_child(fade)
	await process_frame
	fade.observe({})
	check(not fade.visible, "a live player has no death overlay")
	fade.observe({"fight_death_task": true, "fight_death_fade": 64})
	check(fade.visible and fade.level == 64, "native half fade reaches the renderer")
	for frame in range(3): await process_frame
	check(fade.level == 64, "displayed frames do not advance native death time")
	if DisplayServer.get_name() != "headless":
		RenderingServer.force_draw()
		var image := root.get_texture().get_image()
		var pixel := image.get_pixel(320, 224)
		check(absf(pixel.r - 0.5) < 0.02 and absf(pixel.g - 0.5) < 0.02 and absf(pixel.b - 0.5) < 0.02,
			"recorded black fade is visible at half alpha")
	fade.observe({"fight_death_task": true, "fight_death_fade": 128})
	check(fade.visible and fade.level == 128, "completed native fade stays opaque while reload refuses")
	await process_frame
	if DisplayServer.get_name() != "headless":
		RenderingServer.force_draw()
		check(root.get_texture().get_image().get_pixel(320, 224).r < 0.01, "full fade paints black")
	fade.observe({"fight_death_task": false, "fight_death_fade": 128})
	check(not fade.visible, "an absent task cannot leave a stale death overlay")
	var controller: RefCounted = ClassDB.instantiate("ChampionsSessionController")
	check(not controller.fight_diagnostic_lethal_hit().get("ok", true), "diagnostic refuses without a live shared-World fight")
	background.queue_free()
	fade.queue_free()
	controller = null
	await process_frame
	print("PLAYER_DEATH_UI ", checks, " checks, ", failures, " failures")
	quit(1 if failures else 0)
