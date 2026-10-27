extends "res://tests/playable_wave_smoke_play.gd"
const Icons = preload("res://tests/playable_wave_smoke_ui_fixture.gd")

func extra_panels() -> void:
	var art = scene.components.art
	var before: Dictionary = scene.controller.get_observation()
	var names: Array[String] = []
	var images: Array[PackedByteArray] = []
	for colour in ["red", "blue"]:
		for frame in range(11):
			names.append("bar_%s%02d.tex" % [colour, frame])
			images.append(ArtFixture.fixture(100, 8, frame + 40))
	names.append("hud_statusbar00.tex")
	images.append(ArtFixture.fixture(160, 46, 17))
	var hud := directory.path_join("HUD.LMP")
	write(hud, Archives.lmp(names, images))
	scene.actions.art.pressed.emit()
	art.file_dialog.file_selected.emit(hud)
	check(art.load_hud_set() and art.loaded.size() == 23, "explicit complete native HUD art set")
	expected_art_count = 23
	check(scene.play_hud != null and scene.play_hud.textures.size() == 23, "the play-view HUD shares the checked art set")
	art.picker.hide()
	check(scene.controller.get_observation() == before, "HUD resource loading cannot mutate play state")
	var view = art.hud_view
	check(view.layout_service != null and view.dynamic_service != null, "both production native layout services")
	check(view.records.size() == 2 and view.painter_order == PackedInt32Array([1, 0]) and view.missing.is_empty(), "native order paints frame before observed health; unavailable play mana omitted")
	var health: Dictionary = view.records[0]
	check(health.x == 27 and health.y == 46 and health.clip.left == 27 and health.clip.right == 122 and health.resource == "bar_red01.tex", "independent reference position, 96/100 fill and tick-7 frame")
	check(view.records[1].x == 8 and view.records[1].y == 36 and view.records[1].resource == "hud_statusbar00.tex", "documented local status frame position")
	scene.scene_scroll.ensure_control_visible(art)
	await capture("ui-01-positioned-hud")
	# Missing mana is never fabricated for play. Exercise the mana mapping with
	# a separate real diagnostic controller, then restore the live observation.
	var recovery = ClassDB.instantiate("ChampionsSessionController")
	var recovered: Dictionary = recovery.set_recovery_test(true)
	art.update_observation(recovered)
	check(view.records.size() == 3 and view.records[1].y == 63 and view.records[1].resource.begins_with("bar_blue"), "real native mana observation reaches positioned bar")
	art.update_observation(scene.raw_observation)
	recovery = null
	# A missing family member refuses the whole explicit set, retaining art.
	write(directory.path_join("incomplete.LMP"), Archives.lmp(["bar_red10.tex"], [ArtFixture.fixture(100, 8, 9)]))
	art.open_source(directory.path_join("incomplete.LMP"))
	var retained: Image = art.loaded.bar_red.image
	check(not art.load_hud_set() and art.loaded.size() == 23 and art.loaded.bar_red.image == retained, "incomplete/mismatched HUD family cannot replace current set")
	art.open_source(hud)
	var inventory = art.inventory_panel
	var hdr := directory.path_join("PICCACHE.HDR")
	var dat := directory.path_join("PICCACHE.DAT")
	write(hdr, Icons.index_fixture())
	var payload := Icons.fixture()
	# Opaque, high-contrast synthetic palette makes the actual grid visible.
	# Packet/index geometry remains the independently checked native recipe.
	for index in range(256):
		var offset := 224 + index * 4
		payload[offset] = (40 + index) & 255
		payload[offset + 1] = 160
		payload[offset + 2] = 220
		payload[offset + 3] = 128
	payload.resize(5 * 2048)
	write(dat, payload)
	scene.actions.inventory_pictures.pressed.emit()
	check(scene.paused and inventory.visible and scene._component_modal_open(), "inventory menu is a paused real modal")
	inventory.index_dialog.file_selected.emit(hdr)
	inventory.data_dialog.file_selected.emit(dat)
	inventory.name_edit.text = "pic_sword11.tex"
	press(inventory, "Add picture")
	check(inventory.entries.size() == 1, "explicit HDR lookup and DAT range become a displayed image")
	if inventory.entries.is_empty(): return
	var icon: Dictionary = inventory.entries[0]
	check(icon.image.get_size() == Vector2i(9, 6) and icon.image.get_data().slice(0, 4) == PackedByteArray([56, 160, 220, 255]), "independent native icon pixels and explicit display alpha")
	check(icon.placement.x == 27 and icon.placement.y == 20 and icon.placement.columns == 1 and icon.placement.rows == 1, "native grid origin and footprint")
	for i in range(9): check(inventory.add_picture("pic_sword11.tex"), "additional explicit grid picture")
	check(inventory.entries[8].cell == Vector2i(0, 1) and inventory.entries[8].placement.y == 54, "authored first-fit row uses native 34-pixel placement")
	await capture("ui-02-inventory-grid")
	if DisplayServer.get_name() != "headless":
		check(inventory.position.y >= 0 and inventory.position.y + inventory.size.y <= root.size.y and inventory.get_ok_button().get_global_rect().end.y <= inventory.size.y, "inventory and Close fit compact exported window")
	var count: int = inventory.entries.size()
	check(not inventory.add_picture("missing.tex") and inventory.entries.size() == count, "missing icon retains loaded grid")
	check(not inventory.select_index(directory.path_join("absent.HDR")) and inventory.index_path == hdr, "failed HDR replacement retains selection")
	var truncated := directory.path_join("truncated.DAT")
	write(truncated, PackedByteArray()); check(not inventory.select_data(truncated), "short DAT rejected")
	payload.resize(2048)
	write(truncated, payload)
	check(inventory.select_data(truncated) and not inventory.add_picture("pic_sword11.tex") and inventory.entries.size() == count, "bounded selected DAT span refusal preserves grid")
	await capture("ui-03-inventory-refusal")
	inventory.select_data(dat)
	check(scene.controller.get_observation() == before, "inventory lookup/decoding/refusals are read-only simulation")
	scene.resume_session()
	check(scene.paused, "cannot resume behind inventory modal")
	inventory.hide()
	scene.resume_session()
	check(not scene.paused, "closing inventory permits resume")
	scene.set_paused(true)

func continuation_cases() -> void:
	await super.continuation_cases()
	check(scene.components.art.inventory_panel.entries.size() == 10, "continuation load retains read-only inventory pictures")
