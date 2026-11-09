extends SceneTree

# Synthetic checks for created-character appearance wiring
# (research-char-appearance 7931517 docs/spec/created-character-appearance.md).
# No owner file is read; the request binding is exercised on refusals only.
const RealPlayer = preload("res://real_data/real_player.gd")
const Fixture = preload("res://tests/skinning_fixture.gd")
const Archives = preload("res://tests/texture_fixture.gd")

var failures := 0
var checks := 0

func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("_run")

func _requests(hair: bool, beard: bool) -> Array:
	var out: Array = []
	if hair:
		out.append({"layer": "hair", "kind": "mesh", "name": "P_H1.VIF"})
		out.append({"layer": "hair", "kind": "texture", "name": "p_h1a.tex"})
	if beard:
		out.append({"layer": "beard", "kind": "mesh", "name": "p_b1.vif"})
		out.append({"layer": "beard", "kind": "texture", "name": "p_b1a.tex"})
	out.append({"layer": "body", "kind": "mesh", "name": "p_body.vif"})
	out.append({"layer": "body", "kind": "texture", "name": "p_body1.tex"})
	return out

func _run() -> void:
	var inspector: RefCounted = ClassDB.instantiate("ChampionsArchiveInspector")
	check(inspector != null, "inspector available")
	# :193-196 gates through the native binding.
	check(inspector.player_appearance_gates(0, 0, false, false, false) == [true, true], "uncovered layers pass")
	check(inspector.player_appearance_gates(0, 0, true, false, false) == [false, false], "covered layers fail")
	check(inspector.player_appearance_gates(11, 0, true, false, false) == [true, true], "model 11 passes covered")
	check(inspector.player_appearance_gates(10, 4, true, false, false) == [true, false], "model 10 skin 4 hair")
	check(inspector.player_appearance_gates(6, 0, true, true, false) == [false, true], "model 6 head beard")
	check(inspector.player_appearance_gates(3, 0, true, true, true) == [false, true], "head exception beard")
	# Request binding refuses without an owner executable or with bad choices.
	var refused: Dictionary = inspector.player_appearance_requests(PackedByteArray(), 0, [0, 0, 0, 0])
	check(not refused.ok and not str(refused.error).is_empty() and refused.requests.is_empty(), "empty executable refused")
	refused = inspector.player_appearance_requests(PackedByteArray([1, 2, 3]), 0, [0, 0, 0])
	check(not refused.ok, "three choices refused")
	refused = inspector.player_appearance_requests(PackedByteArray([1, 2, 3]), 12, [0, 0, 0, 0])
	check(not refused.ok, "model 12 refused")
	refused = inspector.player_appearance_requests(PackedByteArray([1, 2, 3]), 0, [0, 0, 0, 300])
	check(not refused.ok, "out-of-byte choice refused")

	var rows := {"p_h1.vif": 3, "p_h1a.tex": 4, "p_b1.vif": 5, "p_b1a.tex": 6}
	var gates := func(model: int, skin: int) -> Callable:
		return func(covered: bool, head: bool, exception: bool) -> Array:
			return inspector.player_appearance_gates(model, skin, covered, head, exception)
	var plan: Dictionary = RealPlayer.plan_optional_layers(_requests(true, true), rows, gates.call(0, 0))
	check(plan.hair.pair and plan.hair.mesh_row == 3 and plan.hair.texture_row == 4, "case-folded pair rows")
	check(plan.hair.draw == "refused_cover" and plan.beard.draw == "refused_cover", "cover unknown refuses model 0")
	plan = RealPlayer.plan_optional_layers(_requests(true, true), rows, gates.call(11, 2))
	check(plan.hair.draw == "pending" and plan.beard.draw == "pending", "model 11 decided for both")
	plan = RealPlayer.plan_optional_layers(_requests(true, true), rows, gates.call(10, 4))
	check(plan.hair.draw == "pending" and plan.beard.draw == "refused_cover", "model 10 skin 4 hair only")
	plan = RealPlayer.plan_optional_layers(_requests(true, true), rows, gates.call(10, 3))
	check(plan.hair.draw == "refused_cover", "model 10 other skin needs cover")
	plan = RealPlayer.plan_optional_layers(_requests(false, true), {"p_b1.vif": 5}, gates.call(11, 0))
	check(plan.hair.draw == "absent" and not plan.hair.requested, "style -1 layer absent")
	check(plan.beard.draw == "null_pair" and plan.beard.mesh_row == 5 and plan.beard.texture_row == -1,
		"missing texture keeps a null pair")
	plan = RealPlayer.plan_optional_layers(_requests(true, false), {}, gates.call(11, 0))
	check(plan.hair.draw == "null_pair" and plan.beard.draw == "absent", "missing mesh keeps a null pair")

	# Header +0x13 is reported for the attachment rule (:210-215).
	var attached: PackedByteArray = Fixture.model()
	attached[0x13] = 2
	var names: Array[String] = ["plain.vif", "attached.vif"]
	var payloads: Array[PackedByteArray] = [Fixture.model(), attached]
	var path := ProjectSettings.globalize_path("user://char_appearance_test.lmp")
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, "synthetic archive written")
	if file != null:
		file.store_buffer(Archives.lmp(names, payloads))
		file.close()
		var listing: Dictionary = inspector.inspect_archive(path)
		check(listing.get("ok", false), "synthetic archive inspected")
		if listing.get("ok", false):
			var plain: Dictionary = inspector.inspect_model(path, 0, "rta", listing.fingerprint)
			var with_slot: Dictionary = inspector.inspect_model(path, 1, "rta", listing.fingerprint)
			check(plain.ok and int(plain.attachment_slot) == -1, "header -1 reported")
			check(with_slot.ok and int(with_slot.attachment_slot) == 2, "header attachment index reported")
		DirAccess.remove_absolute(path)
	print("CHAR_APPEARANCE_SMOKE_PASS checks=", checks, " failures=", failures)
	quit(0 if failures == 0 else 1)
