extends Control

# Play-scene overlay for the round-3 HUD contracts (research-ui 1ed740a,
# docs/hud-dynamic.md sections 7 and 9). The native functions behind
# ChampionsUiHudRound3Service hold the documented arithmetic. Every element
# below still lacks a documented input that play supplies, so this overlay
# draws nothing and records why in `refusals`. It must not invent a trigger,
# a projection, a placement or a colour (audit HUD-1..HUD-5).
const REFUSALS := {
	# damage_apply creates FlyingText (skipped for player victims) with the
	# damage number, crit, kind and a shared-seed constructor draw
	# (research-ui docs/gameplay.md step 30). The combat runtime refuses that
	# 0x98 allocation (combat_runtime_damage.cpp), so there is no event.
	"flying_numbers": "FlyingText comes only from damage_apply; the combat runtime refuses that allocation and play supplies no g_viewProj 0x44f140 or world anchor",
	"compass_label": "the label string 0x86aaf0 is unidentified and the gates 0x4f8424, 0x4f841c, dialog and 0x4f66d5 are not supplied",
	"minimap": "the projector 0x168cac is undocumented and play supplies no object flag words",
	"help": "the level script's help box is drawn by ui/text_hud.gd (docs/STRINGS.md); this overlay has no producer",
	"notice": "frame art 0x17fbb8 is not implemented here, and nothing produces hud_notice",
	"medal": "hud_drawMessageBox and menuList_draw are not implemented here, and nothing produces hud_medal",
	"net_cam": "player_slotColor is not implemented here, and nothing produces hud_net_cam",
	"bust": "the level script's bust panel without a maximum is drawn by ui/text_hud.gd (docs/STRINGS.md); the sprite RGB and the maximum branch's second wrap stay undocumented",
}
var service: Object
var play_mode := false
var refusals: Dictionary = {}

func _ready() -> void:
	mouse_filter = MOUSE_FILTER_IGNORE
	if ClassDB.class_exists("ChampionsUiHudRound3Service"):
		service = ClassDB.instantiate("ChampionsUiHudRound3Service")
	queue_redraw()

func observe(_observation: Dictionary, playing: bool) -> void:
	play_mode = playing
	refusals = REFUSALS.duplicate() if service != null and playing else {}
	queue_redraw()
