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

func clear() -> void:
	model_index = -1
	textured = false
	body_drawn = false
	binder = null
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
	visual.mesh = prepared.mesh
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
			var bound: Dictionary = binder.bind_named(selection, selection_rows, body_row, skin_name)
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
