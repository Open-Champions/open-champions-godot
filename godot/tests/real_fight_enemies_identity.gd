extends SceneTree

const Presenter = preload("res://real_data/real_fight_enemies.gd")

class Binder extends RefCounted:
	var material := StandardMaterial3D.new()
	func apply(instance: MeshInstance3D) -> bool:
		instance.set_surface_override_material(0, material)
		return true

class Fixture extends Presenter:
	var prepared := {}
	func _model(row: Dictionary) -> Dictionary:
		var identity := _identity(row)
		if row.get("unavailable", false): return {}
		if not prepared.has(identity):
			prepared[identity] = {"mesh": BoxMesh.new(), "binder": Binder.new(),
				"textured": row.get("textured", true), "foot": 0.0}
		return prepared[identity]

var failures := 0
var checks := 0

func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(label)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var host := Node3D.new()
	root.add_child(host)
	var source := Node.new()
	host.add_child(source)
	var presenter := Fixture.new()
	presenter.install(source, RefCounted.new(), host)
	var row := {"handle": 1, "model_lump": "fixture", "model_entry": "body-a",
		"model_mask": 1, "texture_entry": "texture-a", "position": Vector3.ZERO,
		"facing": 0, "textured": false}
	check(presenter.update({"fight_enemies": [row]}) == 1, "initial body drawn")
	var previous: MeshInstance3D = presenter.instances[1]
	check(previous.material_override != null, "untextured body has fallback material")
	row.textured = true
	for field in ["model_lump", "model_entry", "model_mask", "texture_entry"]:
		if field == "model_mask": row[field] = 2
		else: row[field] = str(row[field]) + "-replacement"
		check(presenter.update({"fight_enemies": [row]}) == 1, field + " replacement drawn")
		var current: MeshInstance3D = presenter.instances[1]
		check(current != previous and previous.get_parent() == null, field + " retires old body")
		check(current.mesh != previous.mesh and current.material_override == null,
			field + " binds new mesh without stale fallback")
		check(current.get_surface_override_material(0) == presenter.prepared[Presenter._identity(row)].binder.material,
			field + " binds current texture material")
		previous = current
	check(presenter.update({"fight_enemies": [row]}) == 1 and presenter.instances[1] == previous,
		"unchanged identity reuses its instance")
	check(presenter.update({"fight_enemies": []}) == 0 and presenter.instances.is_empty()
		and presenter.identities.is_empty() and previous.get_parent() == null,
		"absent actor is detached and retired, not hidden")
	row.model_entry = "other-class-body"
	check(presenter.update({"fight_enemies": [row]}) == 1 and presenter.instances[1] != previous,
		"same address after absence gets replacement class presentation")
	previous = presenter.instances[1]
	row.unavailable = true
	check(presenter.update({"fight_enemies": [row]}) == 0 and presenter.instances.is_empty()
		and previous.get_parent() == null, "unavailable model cannot leave a stale body visible")
	presenter.clear()
	host.queue_free()
	await process_frame
	print("ENEMY_IDENTITY ", checks, " checks, ", failures, " failures")
	quit(int(failures != 0))
