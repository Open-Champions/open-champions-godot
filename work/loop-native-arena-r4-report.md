STATUS: IN PROGRESS — pending full CTest.

# Native arena round 4

Research pins: research-arena `17810bb`; research-arena-match `d4f58da`.
Preserved and finished the two interrupted source edits after checking pinned references.

- NA-R3-1: restored last-attacker handle-presence routing in World player death.
  A present handle calls on_player_killed with its object; an absent handle calls
  lose_life with the player slot and prediction exclusion. Evidence:
  d4f58da tools/ref_combat_ptick.py:437-442, tools/ref_arena_modes.py:113-168.
- NA-R3-2: flags now short-circuit both memory callbacks before creature-state
  access. Saved-object classification remains unchanged. Evidence:
  d4f58da tools/ref_arena_match_core.py:45-67.
- NA-R3-3: round-3 commands use caller-supplied $RESEARCH_ARENA_MATCH,
  $GODOT_CPP_DIR, $GODOT and $GAME_FOLDER instead of host/private roots.

Regression tests and gates pending. No live-match support is implied.
