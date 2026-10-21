extends SceneTree

# Synthetic record only. The same rows as native/tests/level_lights_tests.cpp.
const Playable = preload("res://fx/playable_fx.gd")

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	assert(ClassDB.class_exists("ChampionsLevelLights"))
	var service = ClassDB.instantiate("ChampionsLevelLights")
	var text := FileAccess.get_file_as_string("res://tests/fixtures/level_lights_synthetic.json")
	var loaded: Dictionary = service.load_json(text)
	assert(loaded.ok and loaded.rows == 3)
	var alpha: Dictionary = service.resolve("alpha", "")
	assert(alpha.ok and alpha.index == 0 and alpha.base == 64)
	assert(alpha.ambient == PackedInt32Array([50, 30, 20, 0]))
	assert(alpha.sun == PackedInt32Array([150, 100, 50, 0]))
	assert(alpha.fill[3] == 0 and alpha.direction == Vector3(0, -1, 1))
	assert(alpha.marker_ambient and alpha.marker_sun and alpha.marker_direction and not alpha.default_row)
	var extra: Dictionary = service.resolve("alpha", "extra")
	assert(extra.index == 1 and extra.base == 128 and extra.ambient[0] == 32 and extra.sun[0] == 68)
	assert(not extra.marker_ambient)
	var missing: Dictionary = service.resolve("missing", "")
	assert(missing.default_row and missing.index == 3 and missing.base == 64 and missing.ambient[0] == 32)
	var bridge = ClassDB.instantiate("ChampionsFxBridge")
	var applied: Dictionary = bridge.set_level_light(alpha.direction, Color8(alpha.sun[0], alpha.sun[1], alpha.sun[2], alpha.sun[3]), Color8(alpha.ambient[0], alpha.ambient[1], alpha.ambient[2], alpha.ambient[3]), alpha.base, 1.0, 0)
	assert(applied.ok)
	# No point nodes: world shade is the record's base, not the synthetic 128.
	assert(bridge.shade_world(Vector3(0, 0, 400), Vector3(0, 0, 1))["rgb"] == Vector3(64, 64, 64))
	var fx = Playable.new()
	root.add_child(fx)
	fx.level_light_path = ProjectSettings.globalize_path("res://tests/fixtures/level_lights_synthetic.json")
	fx.level_light_name = "alpha"
	fx.configure(null)
	assert(fx.level_light_active)
	assert("level light record" in fx.status_text())
	assert(fx.renderer.bridge.shade_world(Vector3(0, 0, 400), Vector3(0, 0, 1))["rgb"] == Vector3(64, 64, 64))
	# LL-1: the row comes from the loaded level's own name (lighting.md 10.1).
	# A JSON with no loaded level name is refused; the synthetic sun stays.
	fx.level_light_name = ""
	fx.reset()
	if fx.level_light_active or not "level light refused: loaded level name unknown" in fx.refusals:
		push_error("a level light JSON without the loaded level's name must not apply the default row")
		quit(1)
		return
	if fx.renderer.bridge.shade_world(Vector3(0, 0, 400), Vector3(0, 0, 1))["rgb"] != Vector3(128, 128, 128):
		push_error("the refused record must leave the synthetic base level")
		quit(1)
		return
	# A loaded level whose name misses every pass takes the null-name row.
	fx.select_level_light(fx.level_light_path, "missing")
	if not fx.level_light_active or not fx.level_light_record.get("default_row", false) or int(fx.level_light_record.base) != 64:
		push_error("a loaded level that misses the table takes the default row")
		quit(1)
		return
	fx.level_light_path = ""
	fx.reset()
	assert(not fx.level_light_active)
	assert("synthetic lighting" in fx.status_text())
	assert(fx.renderer.bridge.shade_world(Vector3(0, 0, 400), Vector3(0, 0, 1))["rgb"] == Vector3(128, 128, 128))
	fx.level_light_path = "user://missing-level-lights.json"
	fx.reset()
	assert(not fx.level_light_active and "level light JSON refused" in fx.refusals)
	assert(fx.renderer.bridge.shade_world(Vector3(0, 0, 400), Vector3(0, 0, 1))["rgb"] == Vector3(128, 128, 128))
	print("level lights smoke passed")
	quit(0)
