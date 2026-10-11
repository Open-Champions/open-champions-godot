extends RefCounted

const ModelFixture = preload("res://tests/model_fixture.gd")
const ArchiveFixture = preload("res://tests/texture_fixture.gd")
const DIRECTORY := "user://session-sandbox-smoke"
var checks := 0
var failures := 0
var files: Array[String] = []

func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error("Session sandbox: " + label)

func key(code: int, pressed := true, echo := false) -> InputEventKey:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	event.echo = echo
	return event

func write_file(name: String, data: PackedByteArray) -> String:
	var path := DIRECTORY.path_join(name)
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		check(false, "authored test file opens")
		return ""
	file.store_buffer(data)
	file.close()
	files.append(path)
	return path

# Entirely authored one-surface WORLD. No original asset bytes are used.
func flat_world(special := false) -> PackedByteArray:
	var data := PackedByteArray()
	var grid := 0x8400
	var tail := grid + 16
	data.resize(tail + 12)
	data.encode_u32(0, 1)
	data.encode_u32(8, 0xffffffff)
	data.encode_u32(12, 0xffffffff)
	data.encode_u32(0x10, 2)
	data.encode_u32(0x14, 2)
	data.encode_u32(0x18, grid)
	data.encode_u32(0x1c, 1)
	data.encode_u32(0x20, 0xc0)
	data.encode_u32(0x24, 0x84)
	data.encode_u32(0x64, tail + 4)
	data.encode_u32(0x78, tail)
	data.encode_u16(0xc0, 0xff80)
	data.encode_u16(0xc2, 0xff80)
	data.encode_u16(0xc4, 128)
	data.encode_u16(0xc6, 128)
	data.encode_u32(0xcc, 0x100)
	data.encode_u16(0xe0, 32767)
	data.encode_u32(0x100, 0xffffffc0)
	data.encode_u32(0x104, 0xffffffc0)
	data.encode_u32(0x108, 129)
	data.encode_u32(0x10c, 129)
	data.encode_u16(tail, 0)
	data.encode_u16(tail + 2, 65535)
	if special:
		data.encode_u16(0x114, 1)
	return data

func run(scene) -> bool:
	scene.automatic_steps = false
	DirAccess.make_dir_recursive_absolute(DIRECTORY)
	check(scene.controller != null, "real native controller is installed")
	if scene.controller == null:
		return false
	# Let the newly attached scene complete its initial viewport/input frame.
	await scene.get_tree().process_frame
	scene.reset_session()
	Input.parse_input_event(key(KEY_W))
	Input.flush_buffered_events()
	await scene.get_tree().process_frame
	check(scene.held_commands == 4, "Godot input dispatch reaches actual scene controls")
	Input.parse_input_event(key(KEY_W, false))
	Input.flush_buffered_events()
	await scene.get_tree().process_frame
	check(scene.held_commands == 0, "routed key release clears held controls")
	scene.reset_session()
	check(scene.terrain_root.get_child_count() > 0, "nonempty terrain is rendered")
	check(scene.actor_visual.mesh != null and scene.actor_visual.mesh.get_surface_count() > 0, "default actor has real visible geometry")
	check(scene.raw_observation.position_raw == PackedInt64Array([0, 0, 0]) and scene.raw_observation.hp_raw == 0x42c80000, "initial native raw position and100HP")
	check(not scene.handle_input_event(key(KEY_D, true, true)), "keyboard echo is ignored")
	check(scene.handle_input_event(key(KEY_D)) and scene.handle_input_event(key(KEY_H)), "movement and health key presses are accepted")
	var first: Dictionary = scene.advance_step()
	check(first.ready_ticks == 0 and first.damage_events == 0 and first.position_raw[0] == 0, "startup input waits for actual ready frame")
	var second: Dictionary = scene.advance_step()
	check(second.position_raw[0] == 0x3f7fffff and second.hp_raw == 0x42c40000 and second.damage_events == 1, "actual movement plus100to98 health")
	check(scene.actor.position.is_equal_approx(Vector3(second.position.x, second.position.z, -second.position.y)), "presentation follows native Z-up to Godot mapping")
	check(scene.health_bar.value == 98 and "98" in scene.health_label.text, "health display follows actual native result")
	scene.handle_input_event(key(KEY_H, false))
	scene.handle_input_event(key(KEY_D, false))
	scene.advance_step()
	check(scene.raw_observation.damage_events == 1, "held health key does not repeat")
	scene.handle_input_event(key(KEY_H))
	scene.handle_input_event(key(KEY_H, false))
	scene.advance_step()
	scene.advance_step()
	check(scene.raw_observation.hp_raw == 0x42c00000 and scene.raw_observation.damage_events == 2, "quick tap between frames persists and98to96 occurs once")
	var owned: Dictionary = scene.raw_observation.duplicate(true)
	var mutated: Dictionary = scene.controller.get_observation()
	mutated.position_raw[0] = 999
	check(scene.controller.get_observation().position_raw == owned.position_raw, "bridge arrays are owned observations")
	scene.reset_session()
	scene.handle_input_event(key(KEY_D))
	scene.handle_input_event(key(KEY_A))
	scene.handle_input_event(key(KEY_W))
	scene.handle_input_event(key(KEY_S))
	scene.advance_step()
	scene.advance_step()
	check(scene.raw_observation.position_raw == PackedInt64Array([0, 0, 0]), "opposing direction pairs cancel")
	scene.reset_session()
	scene.handle_input_event(key(KEY_D))
	scene.handle_input_event(key(KEY_S))
	scene.advance_step()
	scene.advance_step()
	check(scene.raw_observation.position_raw[0] == 0x3f7fffff and scene.raw_observation.position_raw[1] == 0xbf7fffff, "diagonal input keeps explicit full axes")
	check(scene.actor.position.z > 0, "negative native Y maps to positive display Z")
	scene.reset_session()
	scene.handle_input_event(key(KEY_D))
	scene.handle_input_event(key(KEY_H))
	scene.advance_step()
	scene.notification(Control.NOTIFICATION_APPLICATION_FOCUS_OUT)
	check(scene.paused and scene.held_commands == 0 and not scene.pending_damage, "focus loss pauses and clears presentation controls")
	var paused: Dictionary = scene.controller.get_observation()
	scene.advance_step()
	check(scene.controller.get_observation() == paused, "paused step cannot advance native authority")
	scene.set_paused(false)
	scene.advance_step()
	check(scene.raw_observation.position_raw == PackedInt64Array([0, 0, 0]) and scene.raw_observation.hp_raw == 0x42c80000, "resume discards buffered input before cutoff")
	scene.terrain_dialog.popup_centered()
	check(not scene.handle_input_event(key(KEY_ESCAPE)) and not scene.handle_input_event(key(KEY_D)), "modal picker blocks pause toggle and movement input")
	scene.terrain_dialog.hide()
	var before_bad: Dictionary = scene.controller.get_observation()
	check(not scene.controller.advance(32).ok and scene.controller.get_observation() == before_bad, "invalid bridge command preserves current state")
	var bad := write_file("broken.WORLD", PackedByteArray([1, 2, 3]))
	var mesh_before: Mesh = scene.terrain_root.get_child(0).mesh
	check(not scene.load_terrain_file(bad) and scene.controller.get_observation() == before_bad, "bad terrain candidate preserves live controller")
	check(scene.terrain_root.get_child(0).mesh == mesh_before, "bad terrain preserves visible ground")
	var special := write_file("unsupported.WORLD", flat_world(true))
	check(not scene.load_terrain_file(special) and scene.controller.get_observation() == before_bad, "parser-valid unsupported display terrain fails atomically")
	var floor_path := write_file("Flät 测试.WORLD", flat_world())
	check(scene.load_terrain_file(floor_path), "authored coherent flat WORLD loads through real decoder and candidate swap")
	check(scene.raw_observation.submissions == 0 and scene.raw_observation.hp_raw == 0x42c80000 and scene.terrain_root.get_child_count() == 1, "successful terrain replacement starts clean authority and nonempty ground")
	scene.handle_input_event(key(KEY_D))
	scene.advance_step()
	scene.advance_step()
	check(scene.raw_observation.position_raw[0] == 0x3f7fffff, "decoded terrain participates in actual movement")
	var names: Array[String] = ["note", "same.vif", "same.vif"]
	var payloads: Array[PackedByteArray] = [PackedByteArray([1]), ModelFixture.model(), ModelFixture.model(0x42, 3)]
	var archive := write_file("Appearance 测试.LMP", ArchiveFixture.lmp(names, payloads))
	var selection: Dictionary = scene.select_asset_archive(archive)
	check(selection.get("ok", false) and scene.model_list.item_count == 2, "authored archive models are listed without name deduplication")
	check(scene.model_list.get_item_metadata(0) == 1 and scene.model_list.get_item_metadata(1) == 2, "filtered model UI retains original row identities")
	var before_asset: Dictionary = scene.controller.get_observation()
	check(scene.apply_asset_model(1), "selected visible model produces actual ArrayMesh")
	check(scene.actor_visual.mesh is ArrayMesh and scene.actor_visual.mesh.get_surface_count() == 1, "only visible source mesh is displayed")
	check(scene.controller.get_observation() == before_asset and scene.raw_observation == before_asset, "appearance selection never resets movement or HP authority")
	var appearance: Mesh = scene.actor_visual.mesh
	check(not scene.apply_asset_model(2) and scene.actor_visual.mesh == appearance, "all-hidden duplicate-name row fails without destroying prior appearance")
	payloads[0] = PackedByteArray([2])
	write_file("Appearance 测试.LMP", ArchiveFixture.lmp(names, payloads))
	check(not scene.apply_asset_model(1) and scene.actor_visual.mesh == appearance, "same-path changed archive fingerprint refuses stale model selection")
	check(scene.controller.get_observation() == before_asset, "failed appearance selection leaves authority unchanged")
	scene.restore_default_actor()
	check(scene.actor_visual.mesh is CapsuleMesh and scene.controller.get_observation() == before_asset, "restoring practice appearance is cosmetic")
	scene.stop_session()
	var stopped: Dictionary = scene.controller.get_observation()
	scene.advance_step()
	check(scene.stopped and scene.controller.get_observation() == stopped, "stop prevents new native steps")
	scene.reset_session()
	check(not scene.stopped and scene.raw_observation.hp_raw == 0x42c80000 and scene.raw_observation.submissions == 0, "reset starts fresh run on selected terrain")
	for path in files:
		DirAccess.remove_absolute(path)
	print("Open Champions session sandbox: ", checks, " checks, ", failures, " failures")
	if failures == 0:
		print("SESSION_SANDBOX_SMOKE_PASS")
	return failures == 0
