extends Control

# hud_stepFade's black rectangle (script_effects.cpp, research-fade 197f52a):
# logical 0,0..640,448 at layer 6; PS2 alpha 128 is opaque. Native ready
# ticks own the fade value. Drawing never advances the countdown or the task.
var level := 0

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

func observe(state: Dictionary) -> void:
	level = clampi(int(state.get("fight_death_fade", 0)), 0, 128) if state.get("fight_death_task", false) else 0
	visible = level > 0
	queue_redraw()

func _draw() -> void:
	if level > 0:
		draw_rect(Rect2(Vector2.ZERO, size), Color(0.0, 0.0, 0.0, float(level) / 128.0))
