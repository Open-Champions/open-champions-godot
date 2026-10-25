extends RefCounted

var host: Node
var row: HBoxContainer
var choice: OptionButton
var targets: Array = []

func configure(owner_scene: Node, menu: Node) -> void:
	host = owner_scene
	row = HBoxContainer.new()
	menu.add_child(row)
	choice = OptionButton.new()
	choice.tooltip_text = "Diagnostic direct use. Original proximity dispatch is not implemented."
	row.add_child(choice)
	var button: Button = host._button("Use hub service (direct)", use_selected)
	row.add_child(button)
	host.actions.hub_service = button
	row.visible = false
	choice.visible = false
	host.actions.hub_service.visible = false
	refresh()

func refresh() -> void:
	if host.controller == null: return
	var next: Array = host.controller.hub_service_targets()
	if next == targets: return
	targets = next
	choice.clear()
	for target in targets:
		choice.add_item("%s #%d" % [str(target.kind), int(target.index)], int(target.index))
		choice.set_item_tooltip(choice.item_count - 1, str(target.engagement) + " " + str(target.refusal))
	choice.visible = not targets.is_empty()
	host.actions.hub_service.visible = not targets.is_empty()
	row.visible = not targets.is_empty()

func use_selected() -> Dictionary:
	if choice.selected < 0 or choice.selected >= targets.size():
		host.status_label.text = "No hub service selected."
		return {"ok": false, "error": "No hub service selected."}
	var target: Dictionary = targets[choice.selected]
	if not target.get("direct_use", false):
		host.status_label.text = "Service refused: " + str(target.refusal)
		return {"ok": false, "error": target.refusal}
	var busy: bool = host.frontend != null and host.frontend.active()
	var result: Dictionary = host.controller.hub_service_use(int(target.index), busy)
	if result.get("request_save", false):
		if host.frontend == null:
			host.status_label.text = "Save flow unavailable; no persistence claimed."
			return {"ok": false, "error": host.status_label.text}
		var opened: Dictionary = host.frontend.open_save_screen()
		if not opened.get("ok", false):
			host.status_label.text = "Save screen refused: " + str(opened.get("error", "save host unavailable"))
			return opened
		host.status_label.text = str(result.persistence)
	else:
		host.status_label.text = str(result.get("error", "Service refused."))
	return result
