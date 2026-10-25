extends RefCounted

# Cosmetic player body from the RTA v2.00 constructor trace. Resource names and
# all 111 ordered slots are read from the owner's executable at run time.
const MeshBuilder = preload("res://scenes/scene_animation_mesh.gd")
const RealCreature = preload("res://real_data/real_creature.gd")

var model_index := -1
var status := "Player record has no mapped class model."
var textured := false
var body_drawn := false
var binder: RefCounted
var actor: MeshInstance3D
var animation_archive := ""
var clip_slots: Array = []
var body_bytes := PackedByteArray()
var archive_reader: RefCounted
var rest_mesh: Mesh

func clear() -> void:
	model_index = -1
	textured = false
	body_drawn = false
	binder = null
	animation_archive = ""
	clip_slots.clear()
	body_bytes = PackedByteArray()
	archive_reader = null
	rest_mesh = null
	if actor != null and is_instance_valid(actor): actor.material_override = null

static func _row_index(rows: Array, name: String) -> int:
	for i in range(rows.size()):
		if rows[i] is Dictionary and str(rows[i].get("kind", "")) == "LMP" and str(rows[i].get("name", "")).to_lower() == name:
			return i
	return -1

func install(real: Node, visual: MeshInstance3D, reader: RefCounted,
		index: int, appearance: Array, player_count: int, expected_class := -1) -> bool:
	clear()
	actor = visual
	if reader == null or visual == null or index < 0 or index >= 12:
		status = "Player model refused: record or model reader unavailable."
		return false
	var executable := str(real.files.get("slus_209.73", ""))
	if executable.is_empty():
		status = "Player model refused: owner executable unavailable."
		return false
	var resources: Dictionary = reader.player_model_resources(FileAccess.get_file_as_bytes(executable), index)
	if not resources.get("ok", false) or not resources.get("clip_slots_1_to_111", []) is Array or resources.clip_slots_1_to_111.size() != 111:
		status = "Player model refused: " + str(resources.get("error", "traced slot table is incomplete"))
		return false
	if expected_class >= 0 and int(resources.class_index) != expected_class:
		status = "Player model refused: class and model index disagree."
		return false
	var code := str(resources.body_code)
	# The traced second selection condition (0x506040) is not available here.
	# A single-player SELECT choice is only a host preview of the body lookup.
	var select_name := "select2.lmp" if player_count >= 2 else "select.lmp"
	var selection := str(real.files.get(select_name, ""))
	if selection.is_empty():
		status = "Player model refused: selection archive is absent."
		return false
	var body_name := code + "_bodyv1.vif"
	var listing: Dictionary = reader.inspect_archive(selection)
	if not listing.get("ok", false):
		status = "Player model refused: selection archive could not be inspected."
		return false
	var selection_rows: Array = listing.rows
	var body_row := _row_index(selection_rows, body_name)
	if body_row < 0:
		status = "Player model refused: traced body entry is absent."
		return false
	# Weapon class to idle-slot dispatch and animation advancement are untraced.
	# Show the VIF rest pose without selecting or advancing any ANM.
	var rest: Dictionary = reader.inspect_model(selection, body_row, "rta", str(listing.fingerprint))
	var prepared: Dictionary = MeshBuilder.prepare_rest(rest)
	if not prepared.get("ok", false):
		status = "Player model refused: " + str(prepared.get("error", "rest pose unavailable"))
		return false
	var body_read: Dictionary = reader.read_player_resource(selection, body_row,
		str(listing.fingerprint), body_name)
	if not body_read.get("ok", false):
		status = "Player model refused: " + str(body_read.get("error", "body resource changed"))
		return false
	body_bytes = body_read.bytes
	visual.mesh = prepared.mesh
	rest_mesh = prepared.mesh
	var animation_name := str(resources.animation_lump).to_lower()
	animation_archive = str(real.files.get(animation_name, ""))
	clip_slots = resources.clip_slots_1_to_111.duplicate()
	archive_reader = reader
	visual.scale = Vector3.ONE # Player_ctor leaves Creature_ctor's 1.0 scale.
	visual.position = Vector3(0, -prepared.bounds.position.y, 0)
	visual.material_override = null
	model_index = index
	body_drawn = true
	status = "Class body rest pose drawn; original idle animation and appearance choices unresolved."
	if appearance.size() == 4:
		var skin := int(appearance[3])
		if skin >= 0:
			var skin_name := "%s_bodyv%d.tex" % [code, skin + 1]
			binder = RealCreature.new()
			binder.environment = real.environment
			var bound: Dictionary = binder.bind_named(selection, selection_rows, body_row, skin_name,
				reader, str(listing.fingerprint))
			if bound.get("ok", false) and binder.apply(visual):
				textured = true
				status = "Class body rest pose and record-selected skin drawn; original idle animation refused."
				if int(appearance[0]) != -1 or int(appearance[2]) != -1:
					status += " Hair/beard attachments refused: part draw path unported."
			else:
				binder = null
				status = "Class body rest pose drawn; record-selected skin refused: " + str(bound.get("error", "material binding failed"))
		else:
			status = "Class body rest pose drawn; record skin byte refused."
	if not textured:
		# The present new-character record omits +0x20..+0x23. Keep the
		# class mesh, but never invent missing or refused skin bytes.
		var neutral := StandardMaterial3D.new()
		neutral.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		neutral.cull_mode = BaseMaterial3D.CULL_DISABLED
		neutral.albedo_color = Color("dec384")
		visual.material_override = neutral
	if player_count < 2:
		status += " Selection flag 0x506040 is unknown; select.lmp is a host preview choice."
	return true

# An explicit diagnostic sample. The real level session has no observed
# weapon-animation class or Player_run state, so it must not call this as if
# it were automatic player playback. Slots are one-based and retain repeats.
func draw_selected_pose(weapon_class: int, role: int, tick: float) -> Dictionary:
	if not body_drawn or actor == null or archive_reader == null or body_bytes.is_empty():
		return {"ok": false, "error": "Player pose refused: selected body bytes are unavailable."}
	if weapon_class < 0 or weapon_class > 7 or role < 0 or role > 12:
		return {"ok": false, "error": "Player pose refused: class or role is outside the traced row."}
	if animation_archive.is_empty():
		return {"ok": false, "error": "Player pose refused: traced animation archive is absent."}
	var slot := 8 + 13 * weapon_class + role
	var clip_name := str(clip_slots[slot - 1]).to_lower()
	if not clip_name.ends_with(".anm"):
		return {"ok": false, "error": "Player pose refused: selected slot is not an animation."}
	var listing: Dictionary = archive_reader.inspect_archive(animation_archive)
	if not listing.get("ok", false):
		return {"ok": false, "error": "Player pose refused: animation archive could not be inspected."}
	var clip_row := _row_index(listing.rows, clip_name)
	if clip_row < 0:
		return {"ok": false, "error": "Player pose refused: selected clip is absent."}
	var clip_read: Dictionary = archive_reader.read_player_resource(animation_archive,
		clip_row, str(listing.fingerprint), clip_name)
	if not clip_read.get("ok", false):
		return {"ok": false, "error": "Player pose refused: " + str(clip_read.get("error", "selected clip changed"))}
	var clip: PackedByteArray = clip_read.bytes
	var sample: Dictionary = archive_reader.sample_skinned_resources(body_bytes, clip, tick)
	var prepared: Dictionary = MeshBuilder.prepare(sample)
	if not prepared.get("ok", false):
		return {"ok": false, "error": "Player pose refused: " + str(prepared.get("error", "sample unavailable"))}
	actor.mesh = prepared.mesh
	actor.position = Vector3(0, -prepared.bounds.position.y, 0)
	if binder != null: binder.apply(actor)
	return {"ok": true, "slot": slot, "duration_ticks": int(sample.duration_ticks),
		"visible_corners": int(prepared.visible_corners)}

func restore_rest_pose() -> void:
	if actor != null and rest_mesh != null:
		actor.mesh = rest_mesh
		actor.position = Vector3(0, -rest_mesh.get_aabb().position.y, 0)
		if binder != null: binder.apply(actor)
