extends Control
# Lifecycle spy only. Never a production path, parser, controller or acceptance
# substitute for the real sibling modules. Inject through component_factory.
signal level_loaded(path: String)
signal preview_changed(active: bool)
signal art_ready(ready: bool)
var configured: Array = []
var clears := 0
var suspended := false
var observations: Array = []
# The shell's HUD and menu adapters also consume the art component's state.
# Empty art is a valid unloaded component; omitting the fields violates that
# contract when clear_art emits art_ready during a session replacement.
var loaded: Dictionary = {}
var catalogue: Dictionary = {}
var hud_view = null
func configure(first, second = null) -> void:
	configured = [first, second]
func show_library() -> void:
	show()
func show_picker() -> void:
	show()
func observe(value: Dictionary) -> void:
	observations.assign([value.duplicate(true)])
func update_observation(value: Dictionary) -> void:
	observations.assign([value.duplicate(true)])
func set_suspended(value: bool) -> void:
	suspended = value
func clear_preview() -> void:
	clears += 1
	preview_changed.emit(false)
func clear_art() -> void:
	clears += 1
	art_ready.emit(false)
