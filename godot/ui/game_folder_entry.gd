extends Node

# On the authored play scene, offer the real-data scene. That scene reads a
# folder the user picks at run time. The export does not contain game data.

func _ready() -> void:
	get_tree().node_added.connect(_consider)


func _consider(node: Node) -> void:
	if node.scene_file_path != "res://scenes/play_flow.tscn":
		return
	if node.ready.is_connected(_add_button):
		return
	node.ready.connect(_add_button.bind(node), CONNECT_ONE_SHOT)


func _add_button(scene: Node) -> void:
	if not is_instance_valid(scene):
		return
	var actions = scene.get("menu_actions")
	if actions == null or not is_instance_valid(actions):
		return
	if actions.get_node_or_null("GameFolderEntry") != null:
		return
	var button := Button.new()
	button.name = "GameFolderEntry"
	button.text = "Your game folder…"
	button.pressed.connect(func() -> void:
		get_tree().change_scene_to_file("res://scenes/real_data_play.tscn"))
	actions.add_child(button)
