extends RefCounted

const WorldGeometryFixture = preload("res://tests/world_geometry_fixture.gd")
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

# Two authored, disjoint flat elements sharing a height field. The second is
# inside the same query grid but outside the first element's bounds.
func flat_world_pair() -> PackedByteArray:
	var data := flat_world()
	var field := data.slice(0x100, 0x100 + 0x14 + 129 * 129 * 2)
	data.resize(0x10000 + field.size())
	for i in field.size(): data[0x10000 + i] = field[i]
	data.encode_u32(0xcc, 0x10000)
	var element := data.slice(0xc0, 0xe4)
	for i in element.size(): data[0xe4 + i] = element[i]
	data.encode_u16(0xe4, 132)
	data.encode_u16(0xe8, 156)
	data.encode_u32(0x1c, 2)
	data.encode_u16(0x8412, 1)
	data.encode_u16(0x8414, 0xffff)
	return data

# Return only weak references after each local owner has gone out of scope.
# This proves actual resource destruction before process/renderer shutdown.
func terrain_resource_lifetimes(path: String) -> Array:
	var temporary = ClassDB.instantiate("ChampionsSessionController")
	var first: WeakRef = weakref(temporary.get_terrain().surfaces[0].mesh)
	var loaded: Dictionary = temporary.load_terrain_file(path)
	check(loaded.get("ok", false), "resource lifetime candidate loads")
	var current: WeakRef = weakref(temporary.get_terrain().surfaces[0].mesh)
	var candidate: Dictionary = temporary.prepare_terrain_file(path)
	check(candidate.get("ok", false), "resource lifetime candidate prepares")
	var cancelled: WeakRef = weakref(candidate.terrain.surfaces[0].mesh)
	check(temporary.cancel_terrain_file(candidate.token), "resource lifetime candidate cancels")
	candidate.clear()
	temporary = null
	return [first, current, cancelled]

func run(scene) -> bool:
	scene.automatic_steps = false
	DirAccess.make_dir_recursive_absolute(DIRECTORY)
	check(scene.controller != null, "real native controller is installed")
	if scene.controller == null:
		return false
	# Let the newly attached scene complete its initial viewport/input frame.
	await scene.get_tree().process_frame
	# Real binding and real UI callbacks, authored Unicode file paths only.
	scene.reset_session()
	for mask in [17, 0, 4, 16]:
		scene.controller.advance(mask)
	var checkpoint: Dictionary = scene.controller.get_observation()
	check(scene.save_format.get_selected_id() == 0, "scene keeps replay as the default save format")
	var save_path := DIRECTORY.path_join("Save 测试.ocnsave")
	files.append(save_path)
	check(scene.save_session_file(save_path), "native save writes user-selected Unicode path")
	var saved_bytes := FileAccess.get_file_as_bytes(save_path)
	check(saved_bytes.slice(0, 8) == PackedByteArray([79, 67, 78, 83, 65, 86, 69, 0]) and saved_bytes.decode_u32(8) == 1, "file preserves replay v1 bytes")
	var snapshot_path := DIRECTORY.path_join("Snapshot 测试.ocnsave")
	files.append(snapshot_path)
	scene.save_format.select(1)
	scene.save_dialog.file_selected.emit(snapshot_path)
	check(FileAccess.file_exists(snapshot_path), "real save picker callback writes selected snapshot format to Unicode path")
	check(scene.controller.get_observation() == checkpoint, "snapshot save action does not mutate native state")
	var snapshot_bytes := FileAccess.get_file_as_bytes(snapshot_path)
	check(snapshot_bytes.slice(0, 8) == PackedByteArray([79, 67, 78, 83, 78, 65, 80, 0]) and snapshot_bytes.decode_u32(8) == 3, "indexed direct snapshot has distinct magic and version")
	var snapshot_before_write := snapshot_bytes.duplicate()
	check(scene.save_session_file(snapshot_path) and FileAccess.get_file_as_bytes(snapshot_path) == snapshot_before_write, "snapshot UI overwrite preserves exact bytes")
	var snapshot_blocker := write_file("Snapshot 测试.ocnsave.ocnsave.tmp", PackedByteArray([7]))
	check(not scene.save_session_file(snapshot_path) and FileAccess.get_file_as_bytes(snapshot_path) == snapshot_before_write and FileAccess.get_file_as_bytes(snapshot_blocker) == PackedByteArray([7]), "snapshot UI temporary collision preserves both files")
	DirAccess.remove_absolute(snapshot_blocker)
	check(not scene.save_session_file(DIRECTORY.path_join("missing/snapshot.ocnsave")) and scene.controller.get_observation() == checkpoint, "snapshot UI write failure preserves native state")
	scene.save_format.select(0)
	scene.save_dialog.file_selected.emit(save_path)
	check(FileAccess.get_file_as_bytes(save_path) == saved_bytes, "switching back through save picker preserves unchanged replay v1 bytes")
	scene.controller.advance(8)
	check(scene.load_session_file(snapshot_path), "real scene load action accepts direct snapshot")
	var snapshot_observation: Dictionary = scene.controller.get_observation()
	var checkpoint_authority: Dictionary = checkpoint.duplicate()
	check(snapshot_observation.get("save_history_pending", false), "binding retains deferred save records after load")
	check(scene.status_label.text.contains("Historical save data not yet checked"), "scene displays deferred validation state")
	for presentation_field in ["terrain_name", "terrain_note", "terrain_mode", "save_history_pending"]:
		snapshot_observation.erase(presentation_field)
		checkpoint_authority.erase(presentation_field)
	check(snapshot_observation == checkpoint_authority, "direct snapshot binding restores pending state")
	var baseline = ClassDB.instantiate("ChampionsSessionController")
	var save_prepared: Dictionary = baseline.prepare_save_file(save_path)
	check(save_prepared.get("ok", false), "fresh owner prepares checkpoint")
	if save_prepared.get("ok", false):
		baseline.commit_terrain_file(save_prepared.token)
	for mask in [0, 17, 0, 4, 16]:
		var resumed = scene.controller.advance(mask)
		var reference = baseline.advance(mask)
		check(resumed.get("save_history_pending", false), "binding continues without historical reads")
		resumed.erase("save_history_pending")
		reference.erase("save_history_pending")
		check(resumed == reference, "indexed pending movement/health continuation matches replay owner")
	check(scene.controller.get_observation().get("save_history_pending", false), "binding retains deferred records through continuation")
	# Return baseline to the same checkpoint for the original replay checks.
	save_prepared = baseline.prepare_save_file(save_path)
	if save_prepared.get("ok", false):
		baseline.commit_terrain_file(save_prepared.token)
	scene.controller.advance(8)
	check(scene.load_session_file(save_path), "scene loads and publishes prepared checkpoint")
	for field in ["position_raw", "hp_raw", "vz_raw", "frame", "submissions", "ready_ticks", "damage_events", "paused", "stopped", "failed"]:
		check(scene.raw_observation[field] == checkpoint[field], "checkpoint restores " + field)
	for mask in [0, 17, 0, 4, 4, 16, 0]:
		check(scene.controller.advance(mask) == baseline.advance(mask), "delayed movement and health continuation matches independent load")
	baseline = null
	var before_load: Dictionary = scene.controller.get_observation()
	var ground_before: Node3D = scene.terrain_root
	var broken := write_file("broken.ocnsave", PackedByteArray([1, 2, 3]))
	check(not scene.load_session_file(broken) and scene.controller.get_observation() == before_load and scene.terrain_root == ground_before, "invalid save preserves authority and visible ground")
	check(not scene.save_session_file(DIRECTORY.path_join("missing/scene.ocnsave")) and scene.controller.get_observation() == before_load, "write failure preserves authority")
	check(scene.save_session_file(save_path), "existing save can be atomically replaced")
	var blocker := write_file("Save 测试.ocnsave.ocnsave.tmp", PackedByteArray([9]))
	var before_write := FileAccess.get_file_as_bytes(save_path)
	check(not scene.save_session_file(save_path) and FileAccess.get_file_as_bytes(save_path) == before_write and FileAccess.get_file_as_bytes(blocker) == PackedByteArray([9]), "temporary collision preserves existing files")
	DirAccess.remove_absolute(blocker)
	scene.save_dialog.popup_centered()
	check(not scene.handle_input_event(key(KEY_D)), "save picker blocks movement")
	scene.save_dialog.hide()
	scene.load_dialog.popup_centered()
	check(not scene.handle_input_event(key(KEY_ESCAPE)), "load picker blocks pause toggle")
	scene.load_dialog.hide()
	scene.reset_session()
	Input.parse_input_event(key(KEY_W))
	Input.flush_buffered_events()
	await scene.get_tree().process_frame
	check(scene.held_commands == 4, "Godot input dispatch reaches actual scene controls")
	scene.advance_step()
	var routed: Dictionary = scene.advance_step()
	check(routed.position_raw == PackedInt64Array([0, 0x3f7fffff, 0]) and routed.ready_ticks == 1, "routed Godot key executes an actual ready creature step")
	Input.parse_input_event(key(KEY_W, false))
	Input.flush_buffered_events()
	await scene.get_tree().process_frame
	check(scene.held_commands == 0, "routed key release clears held controls")
	scene.reset_session()
	check(scene.raw_observation.get("movement_policy", "") == "dry_creature" and scene.raw_observation.get("ready_tick_limit", 0) == 512, "active native controller uses bounded creature movement")
	check(scene.terrain_root.get_child_count() > 0, "nonempty terrain is rendered")
	check(scene.actor_visual.mesh != null and scene.actor_visual.mesh.get_surface_count() > 0, "default actor has real visible geometry")
	check(scene.raw_observation.position_raw == PackedInt64Array([0, 0, 0]) and scene.raw_observation.hp_raw == 0x42c80000, "initial native raw position and100HP")
	check(not scene.handle_input_event(key(KEY_D, true, true)), "keyboard echo is ignored")
	check(scene.handle_input_event(key(KEY_D)) and scene.handle_input_event(key(KEY_H)), "movement and health key presses are accepted")
	var first: Dictionary = scene.advance_step()
	check(first.ready_ticks == 0 and first.damage_events == 0 and first.position_raw[0] == 0, "startup input waits for actual ready frame")
	var second: Dictionary = scene.advance_step()
	check(second.position_raw[0] == 0x3f7fffff and second.hp_raw == 0x42c40000 and second.damage_events == 1, "actual movement plus100to98 health")
	check(second.vz_raw == 0 and second.position_raw[2] == 0, "original creature movement lands on owned ground without duplicate gravity")
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
	var mesh_lifetimes := terrain_resource_lifetimes(floor_path)
	check(mesh_lifetimes[0].get_ref() == null, "replaced native mesh releases its final owner")
	check(mesh_lifetimes[1].get_ref() == null, "controller destruction releases its active native mesh")
	check(mesh_lifetimes[2].get_ref() == null, "cancelled candidate mesh releases its final owner")
	check(scene.load_terrain_file(floor_path), "authored coherent flat WORLD loads through real decoder and candidate swap")
	check(scene.raw_observation.submissions == 0 and scene.raw_observation.hp_raw == 0x42c80000 and scene.terrain_root.get_child_count() == 1, "successful terrain replacement starts clean authority and nonempty ground")
	check(scene.raw_observation.get("movement_policy", "") == "dry_creature" and scene.raw_observation.get("ready_tick_limit", 0) == 512, "terrain replacement retains active creature policy and bounded run")
	var pair_path := write_file("Two plain elements.WORLD", flat_world_pair())
	check(scene.load_terrain_file(pair_path) and scene.controller.get_play_capabilities().terrain_elements == 2 and scene.terrain_root.get_child_count() == 2,
		"two documented flat terrain elements load and display")
	var pair_save := DIRECTORY.path_join("Two plain elements.ocplay")
	var pair_saved: Dictionary = scene.controller.save_file(pair_save)
	var pair_reload: Dictionary = scene.controller.prepare_save_file(pair_save) if pair_saved.get("ok", false) else {}
	check(pair_saved.get("ok", false) and pair_reload.get("ok", false) and pair_reload.get("terrain", {}).get("surfaces", []).size() == 2,
		"multi-element collision preview prepares from a native continuation")
	if pair_reload.get("ok", false): scene.controller.cancel_terrain_file(pair_reload.token)
	var hole := flat_world_pair()
	hole.encode_u16(0x8416, 0)
	hole.encode_u16(0x8418, 0xffff)
	hole.encode_u32(0x8404, 6)
	var hole_path := write_file("Missing element coverage.WORLD", hole)
	var hole_result: Dictionary = scene.controller.prepare_terrain_file(hole_path)
	check(not hole_result.get("ok", false) and "partial surface coverage" in str(hole_result.get("error", "")),
		"multi-element preview refuses a collision-grid hole")
	check(scene.load_terrain_file(floor_path), "single-element fixture returns after multi-element save probe")
	scene.handle_input_event(key(KEY_D))
	scene.advance_step()
	scene.advance_step()
	check(scene.raw_observation.position_raw[0] == 0x3f7fffff, "decoded terrain participates in actual movement")
	check(scene.raw_observation.get("terrain_mode", "") == "practice", "legacy collision-only fixture is explicitly an illustration")
	check(scene.raw_observation.terrain_note == "Practice ground · collision-plane illustration", "practice terrain note preserves UTF-8 punctuation")
	var world_path := write_file("Static geometry.WORLD", WorldGeometryFixture.world(flat_world()))
	var before_prepare: Dictionary = scene.controller.get_observation()
	var prepared: Dictionary = scene.controller.prepare_terrain_file(world_path)
	check(prepared.get("ok", false) and scene.controller.get_observation() == before_prepare, "preparation leaves current authority and queues untouched")
	if prepared.get("ok", false):
		check(prepared.terrain.mode == "static_world" and prepared.terrain.triangles == 1 and prepared.terrain.surfaces[0].mesh is ArrayMesh, "actual source triangle and mesh are prepared before publication")
		var expected_native := PackedVector3Array([Vector3(1, 1, -1), Vector3(6, 9, 4), Vector3(-4, 6, 1)])
		check(prepared.terrain.surfaces[0].positions == expected_native, "source translation then rotation is independently anchored")
		var rendered: PackedVector3Array = prepared.terrain.surfaces[0].mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
		check(rendered == PackedVector3Array([Vector3(1, -1, -1), Vector3(6, 4, -9), Vector3(-4, 1, -6)]), "nonflat source triangles map native axes exactly once")
		check(not scene.controller.commit_terrain_file(prepared.token + 1).ok and scene.controller.get_observation() == before_prepare, "wrong candidate token leaves authority unchanged")
		check(scene.controller.cancel_terrain_file(prepared.token) and not scene.controller.commit_terrain_file(prepared.token).ok, "cancelled candidate cannot publish")
	var stale: Dictionary = scene.controller.prepare_terrain_file(world_path)
	scene.reset_session()
	check(stale.get("ok", false) and not scene.controller.commit_terrain_file(stale.token).ok, "reset invalidates prepared candidates")
	check(scene.load_terrain_file(world_path) and scene.raw_observation.terrain_mode == "static_world", "real static WORLD geometry replaces the illustration")
	var static_mesh: Mesh = scene.terrain_root.get_child(0).mesh
	var static_material: StandardMaterial3D = scene.terrain_root.get_child(0).material_override
	check(static_material.cull_mode == BaseMaterial3D.CULL_DISABLED and static_material.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED, "static ground uses neutral two-sided unlit display")
	check("models, lights and effects omitted" in scene.terrain_label.text, "omitted scene content is stated")
	check(scene.raw_observation.terrain_note == "Static WORLD ground · neutral display · models, lights and effects omitted", "static terrain note preserves UTF-8 punctuation")
	scene.handle_input_event(key(KEY_D))
	scene.handle_input_event(key(KEY_H))
	scene.advance_step()
	var world_ready: Dictionary = scene.advance_step()
	check(world_ready.position_raw[0] == 0x3f7fffff and world_ready.hp_raw == 0x42c40000, "same WORLD bytes supply actual ready movement and health")
	scene.set_paused(true)
	var preserve: Dictionary = scene.controller.get_observation()
	# A WORLD whose only chunk is a water tile has no visible static ground.
	var water_only := write_file("blocked128.WORLD", WorldGeometryFixture.world(flat_world(), 0x80))
	check(not scene.load_terrain_file(water_only) and scene.controller.get_observation() == preserve and scene.terrain_root.get_child(0).mesh == static_mesh and scene.paused, "a water-only WORLD refuses without replacing paused scene")
	# A moving-piece chunk decodes as geometry at its rest pose; prepare and
	# cancel so the paused scene is not replaced.
	var moving_chunk := write_file("moving32.WORLD", WorldGeometryFixture.world(flat_world(), 0x20))
	var moving_prepared: Dictionary = scene.controller.prepare_terrain_file(moving_chunk)
	check(moving_prepared.get("ok", false) and scene.controller.cancel_terrain_file(moving_prepared.token) and scene.controller.get_observation() == preserve and scene.terrain_root.get_child(0).mesh == static_mesh, "a moving-piece chunk prepares as static ground without replacing the scene")
	var no_floor := flat_world()
	for cell in range(4):
		no_floor.encode_u32(0x8400 + cell * 4, 2)
	var no_floor_path := write_file("No starting floor.WORLD", WorldGeometryFixture.world(no_floor))
	check(not scene.load_terrain_file(no_floor_path) and scene.controller.get_observation() == preserve and scene.terrain_root.get_child(0).mesh == static_mesh, "no supported starting floor refuses atomically")
	scene.reset_session()
	check(scene.terrain_root.get_child(0).mesh == static_mesh and scene.raw_observation.terrain_mode == "static_world", "reset retains loaded source geometry")
	# Keep the visible scene ready for the remaining appearance-only checks.
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
	check(scene.raw_observation.get("movement_policy", "") == "dry_creature" and scene.raw_observation.get("ready_tick_limit", 0) == 512, "reset retains active creature policy and cap")
	scene.handle_input_event(key(KEY_W))
	scene.handle_input_event(key(KEY_H))
	scene.advance_step()
	var reset_ready: Dictionary = scene.advance_step()
	check(reset_ready.position_raw == PackedInt64Array([0, 0x3f7fffff, 0]) and reset_ready.hp_raw == 0x42c40000, "reset on loaded terrain still runs original creature movement and shared health")
	var supplied_checkpoint: Dictionary = scene.controller.get_observation()
	check(scene.save_session_file(save_path), "supplied terrain checkpoint saves")
	check(scene.load_session_file(save_path), "supplied collision terrain restores through scene")
	check(scene.raw_observation.position_raw == supplied_checkpoint.position_raw and scene.raw_observation.hp_raw == supplied_checkpoint.hp_raw and scene.raw_observation.ready_ticks == supplied_checkpoint.ready_ticks, "supplied terrain load retains current movement and health")
	check(scene.raw_observation.terrain_mode == "practice" and scene.raw_observation.terrain_name == "Restored collision ground", "load labels collision illustration rather than claiming source geometry")
	scene.release_controls()
	for path in files:
		DirAccess.remove_absolute(path)
	print("Open Champions session sandbox: ", checks, " checks, ", failures, " failures")
	if failures == 0:
		print("SESSION_SANDBOX_SMOKE_PASS")
	return failures == 0
