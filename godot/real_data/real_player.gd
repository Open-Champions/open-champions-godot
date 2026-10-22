extends RefCounted

# Cosmetic player body from the RTA v2.00 constructor trace. Resource names and
# all 111 ordered slots are read from the owner's executable at run time.
const MeshBuilder = preload("res://scenes/scene_animation_mesh.gd")
const RealCreature = preload("res://real_data/real_creature.gd")

var model_index := -1
var status := "Player record has no mapped class model."
var textured := false
var body_drawn := false
var clip_slots: Array = []
var model_bytes := PackedByteArray()
var clip_bytes := PackedByteArray()
var clip_duration := 0
var clip_name := ""
var binder: RefCounted
var actor: MeshInstance3D
var inspector: RefCounted

func clear() -> void:
	model_index = -1
	textured = false
	body_drawn = false
	clip_slots.clear()
	model_bytes.clear()
	clip_bytes.clear()
	clip_duration = 0
	clip_name = ""
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
	inspector = reader
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
	clip_slots = resources.clip_slots_1_to_111
	var code := str(resources.body_code)
	var select_name := "select2.lmp" if player_count >= 2 else "select.lmp"
	var selection := str(real.files.get(select_name, ""))
	var animation := str(real.files.get(str(resources.animation_lump).to_lower(), ""))
	if selection.is_empty() or animation.is_empty():
		status = "Player model refused: traced selection or animation archive is absent."
		return false
	var body_name := code + "_bodyv1.vif"
	var selection_rows: Array = real._rows(selection)
	var body_row := _row_index(selection_rows, body_name)
	if body_row < 0:
		status = "Player model refused: traced body entry is absent."
		return false
	model_bytes = real.read_entry(selection, "", body_name)
	# Manifest slot 73 is an idle-named full-body clip for all twelve models.
	# Original initial-slot dispatch is still unported, so this is labelled
	# as a cosmetic preview rather than the game's selected track.
	clip_name = str(clip_slots[72])
	if not clip_name.to_lower().ends_with(".anm"):
		status = "Player model refused: no documented full-body idle preview clip."
		return false
	clip_bytes = real.read_entry(animation, "", clip_name)
	if model_bytes.is_empty() or clip_bytes.is_empty():
		status = "Player model refused: traced body or clip entry is unreadable."
		return false
	var sample: Dictionary = reader.sample_skinned_resources(model_bytes, clip_bytes, 0.0)
	var prepared: Dictionary = MeshBuilder.prepare(sample)
	if not prepared.get("ok", false):
		status = "Player model refused: " + str(prepared.get("error", sample.get("error", "pose unavailable")))
		return false
	visual.mesh = prepared.mesh
	visual.scale = Vector3.ONE # Player_ctor leaves Creature_ctor's 1.0 scale.
	visual.position = Vector3(0, -prepared.bounds.position.y, 0)
	visual.material_override = null
	clip_duration = int(sample.duration_ticks)
	model_index = index
	body_drawn = true
	status = "Class body drawn; original idle slot and appearance choices unresolved."
	if appearance.size() == 4:
		var skin := int(appearance[3])
		if skin >= 0:
			var skin_name := "%s_bodyv%d.tex" % [code, skin + 1]
			binder = RealCreature.new()
			binder.environment = real.environment
			var bound: Dictionary = binder.bind_named(selection, selection_rows, body_row, skin_name)
			if bound.get("ok", false) and binder.apply(visual):
				textured = true
				status = "Class body and record-selected skin drawn; original idle dispatch unresolved."
				if int(appearance[0]) != -1 or int(appearance[2]) != -1:
					status += " Hair/beard attachments refused: part draw path unported."
			else:
				binder = null
				status = "Class body drawn; record-selected skin refused: " + str(bound.get("error", "material binding failed"))
		else:
			status = "Class body drawn; record skin byte refused."
	if not textured:
		# The present new-character record omits +0x20..+0x23. Keep the
		# class mesh, but never invent missing or refused skin bytes.
		var neutral := StandardMaterial3D.new()
		neutral.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		neutral.cull_mode = BaseMaterial3D.CULL_DISABLED
		neutral.albedo_color = Color("dec384")
		visual.material_override = neutral
	return true

func observe(ready_ticks: int) -> void:
	if not body_drawn or clip_duration <= 0 or inspector == null: return
	var tick := float(posmod(ready_ticks, clip_duration))
	var sample: Dictionary = inspector.sample_skinned_resources(model_bytes, clip_bytes, tick)
	var prepared: Dictionary = MeshBuilder.prepare(sample)
	if not prepared.get("ok", false):
		status = "Class body pose refused: " + str(prepared.get("error", sample.get("error", "pose unavailable")))
		return
	actor.mesh = prepared.mesh
	if binder != null and not binder.apply(actor):
		textured = false
		status = "Class body drawn; texture rebinding refused."
