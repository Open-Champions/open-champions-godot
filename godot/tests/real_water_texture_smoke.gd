extends SceneTree

# WAT-1: water is drawn only with the waterenv image water_setup was given
# (water-draw.md sections 1 and 7.1 at research-textures a9486c7). A texture
# that does not decode leaves the tiles undrawn and counted; no texel is made up.
const RealWorld = preload("res://real_data/real_world.gd")
const TextureFixture = preload("res://tests/texture_fixture.gd")
var failed := 0

func check(ok: bool, message: String) -> void:
	if not ok:
		failed += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	check(ClassDB.class_exists("ChampionsRealWorld"), "real-world adapter loaded")
	var world = RealWorld.new()
	world.native = ClassDB.instantiate("ChampionsRealWorld")
	world.info = {"water": [{}, {}, {}]}
	var instance := MeshInstance3D.new()
	world.water_instance = instance
	for bytes in [PackedByteArray(), PackedByteArray([1, 2, 3, 4])]:
		var result: Dictionary = world.set_water_texture(bytes)
		check(not result.get("ok", true), "an undecodable water texture is refused")
		check(world.water_texture == null, "no substitute water texel is created")
		check(not instance.visible, "water whose texture is refused is not drawn")
		check(world.get("water_tiles_refused") == 3, "the undrawn water tiles are counted")
		check("texture refused" in world.label() and "not drawn" in world.label(), "the label says the water is not drawn")
	var decoded: Dictionary = world.set_water_texture(TextureFixture.rgba_texture())
	check(decoded.get("ok", false) and world.water_texture != null and instance.visible, "a decoded water texture is drawn")
	check(world.get("water_tiles_refused") == 0, "no tiles are refused once the texture decodes")
	instance.free()
	if failed:
		print("real water texture: ", failed, " checks failed")
		quit(1)
		return
	print("real water texture smoke passed")
	quit(0)
