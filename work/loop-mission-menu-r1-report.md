STATUS: COMPLETE — the documented static mission map and input path run; arm64 CTest, Godot matrix, 111-level sweep and bounded route checks pass.

2026-10-23 | `codex/mission-menu` from `7ed3f21` | local implementation; no push

## Evidence and implementation

- Source: `t3/research-story-route` `76d4447`, `docs/spec/mission-menu-art.md`, with the supplied PASS audits. The US v2.00 executable table and MSEL art are read from the owner-selected disc at run time. No retail bytes or extracted assets enter tracked files.
- Replaced the host panel with a 640×448 Godot canvas. It validates and decodes the documented 32 MSEL textures, loads `gamefontsmall.fnt`, reads the 12 display rows and player mission state through the native controller, and paints the controlled viewport endpoints with available/locked node and link tints, row-zero marks and glow, side strips, backdrop and localized detail keys.
- The existing campaign menu state machine handles opening, up/down and column selection, confirm/cancel, and the `blood1` pending-level write. The integrated input accepts the executed alternate down/right pad bits; down returns to the story column. The view submits keyboard input to this state machine and closes after the documented cancel fade.
- The route probe now checks all 12 table image/position pairs, all 32 decoded art entries and the small font, locked movement, cancel fade/close, and the first-story route. Its rendered check saves locked and synthetic mission-2-available screenshots under ignored `work/` and verifies a pixel change in that node's documented rectangle.

## Verification

- macOS arm64 Release CTest `^(quests|script-commands)$`: **2/2 passed** after updating five older synthetic vector expectations for the newly executed down-column rule. `champions-quests-tests` is Mach-O arm64.
- Godot 4.5.2 headless `--route-only`: exit 0; **12/12** row images/positions, **32/32** textures, small font present, locked down stayed at row 0/column 0, story request `blood1`, and campaign/record carry into the requested level.
- Godot 4.5.2 headless `--menu-cancel-only`: exit 0; Escape was handled, state 4 counter reached -1 after the documented fade ticks, and the menu closed.
- Godot 4.5.2 rendered `--route-only`: exit 0; the final captured menu was inspected in the play viewport. The locked versus available image check found **161 changed samples** in the second node. Both PNGs are only in ignored `work/mission-menu-route-rendered-final/`. The separate dialog visual check reports a refusal while this bounded route exits 0.
- Godot 4.5.2 dev matrix (`python3 work/play-route/run-godot.py`): **54/54 commands exited 0**. `scene_play_flow` and `ui_layout_smoke` retain the same internal script/API errors recorded in the play-route baseline; an exit code alone does not clear them.
- 111-level scripts-on sweep: **111 tried / 111 loaded / 111 played**, 224 checks passed, 0 failures, 0 refusal kinds, 111 scripts attached and 0 script faults. The read-only disc file count was 994 before and after.
- Full-combat headless smoke: **2,620 checks, 0 failures**; `FULL_TRACE` **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to the `7ed3f21` play-route baseline.

## Remaining evidence boundary

The executed research establishes static centered scroll endpoints, lookup calls and controlled tints, not interpolated scroll frames, moving backdrop rectangles, localized glyph raster layout, or GPU packet/blend parity. The Godot view uses static backdrop quarters and a visible alpha for the zero-high-byte detail colour; these are labelled presentation choices. Other-row icon/link positions, the detail frame's raster style, and the network-only prompt lack a captured rendered/layout contract and remain unverified. The rendered capture is visual evidence of this client, not a retail parity claim. No Norrath behavior is claimed.

## Commands

From this worktree; `DISC` is the read-only owner disc folder and `GODOT` is the pinned Godot 4.5.2 executable:

```sh
DISC=/Users/syndicatemike/Projects/open-champions/private-validation/rta-disc
GODOT=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target champions-quests-tests champions-script-commands-tests -j 2
ctest --test-dir build/native --output-on-failure -j 1 -R '^(quests|script-commands)$'
cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 2
cmake -S gdextension/level_texture -B build/level-texture -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DGODOTCPP_TARGET=template_debug
cmake --build build/level-texture --config Release --parallel 2
cmake -S gdextension/creature_anim -B build/creature-anim -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DGODOTCPP_TARGET=template_debug
cmake --build build/creature-anim --config Release --parallel 2
$GODOT --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$PWD/work/mission-menu-route-headless-final" --route-only
$GODOT --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$PWD/work/mission-menu-cancel-headless" --menu-cancel-only
$GODOT --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$PWD/work/mission-menu-route-rendered-final" --route-only --rendered
python3 work/play-route/run-godot.py
nice -n 10 $GODOT --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder="$DISC" --cache="$PWD/work/mission-menu-sweep-cache" --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary="$PWD/work/mission-menu-sweep-summary.json"
```
