STATUS: COMPLETE — seven audit fixes and all requested focused checks pass; the broader full CTest is incomplete under machine load.

2026-10-23 | `codex/mission-menu` from `7ed3f21` | local implementation; no push

## Evidence and implementation

- Source: `t3/research-story-route` `76d4447`, `docs/spec/mission-menu-art.md`, and the seven-finding native-mission-menu FIX audit. The US v2.00 executable table and MSEL art are read from the owner-selected disc at run time. No retail bytes or extracted assets enter tracked files.
- Replaced the host panel with a 640×448 Godot canvas. It validates and decodes the documented 32 MSEL textures, loads `gamefontsmall.fnt`, reads the 12 display rows and player mission state through the native controller, and paints the evidenced viewport endpoints with available/locked node and link tints, row-zero marks, side strips and a local prompt.
- The existing campaign menu state machine handles opening, up/down and column selection, confirm/cancel, and the `blood1` pending-level write. The integrated input accepts the executed alternate down/right pad bits; down returns to the story column. The view submits keyboard input only in ready state and closes after the documented cancel fade. A live confirm waits in state 6 because the external fade source is not ported.
- The route probe now checks all 12 table image/position pairs, all 32 decoded art entries and the small font, locked movement, cancel fade/close, and the first-story route. Its rendered check saves locked and synthetic mission-2-available screenshots under ignored `work/` and verifies a pixel change in that node's documented rectangle.

## Audit fixes

- MM-1: removed guessed backdrop quarters and sparkle position. MSEL resources remain validated, but their unresolved draw geometry is not submitted.
- MM-2: removed the invented detail alpha and fixed selection glow tint. Detail text and glows await the original blend and pulse evidence.
- MM-3: removed key-triggered catch-up ticks and the forced fade. Input is accepted only in state 2; opening, cancel and state-5 transitions advance at one tick per frame, then stop at state 6 without a fade source. The route harness alone supplies a controlled fade to prove the native state 6→7→8 path; live transition remains refused.
- MM-4: network prompts now require explicit first-active and pause conditions. Tests cover client text, paused silence, first-active `AcceptBack`, and missing-condition refusal.
- MM-5: row-zero draw presence is independent of the mission availability bit. The original availability flag remains separate for selection; the probe checks the missing-bit render case.
- MM-6: invalid MSEL/font assets hide the panel and consume menu keys without advancing the native state. The probe captures this refusal.
- MM-7: the command example takes a caller-supplied `DISC` path instead of recording the owner's private path.

The screenshot probe captures each visual refusal or prompt branch under ignored `work/` and checks submitted sprite/text keys and tints against those captures. Its controlled fade is test input, not evidence of a live fade service.

## Audit verification

- Release arm64 focused CTest: `ctest --test-dir build/native --output-on-failure -j 1 -R '^(quests|script-commands)$'`: 2/2 passed. The modified GDExtension rebuilt successfully.
- Godot headless route and cancel probes: exit 0. All 13 new mission-menu checks passed; the controlled fade produced the documented pending request and level arrival. The missing-font and unavailable-live-fade refusals are intentional. Cancel reached counter -1 and closed the menu.
- Godot dev matrix: 54/54 commands exited 0. `scene_play_flow` and `ui_layout_smoke` still log their pre-existing internal script/API errors, so their exit codes do not establish those checks as clean.
- Full-combat smoke from that matrix: 2,620 checks, 0 failures; `FULL_TRACE` 36,402 bytes with SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to the baseline.
- Godot rendered route: exit 0. The second node changed in 164 sampled pixels. I inspected the menu, network client prompt and invalid-font refusal captures. An initial rendered check exposed a stale refusal label on recovery; the fixed rerun clears it. The unrelated dialog visual check still refuses, as in the baseline.
- 111-level scripts-on sweep: exit 0; 111 tried / 111 loaded / 111 played, 224 checks passed, 0 failures, 0 refusal kinds, 111 scripts attached, 0 script faults. Read-only disc file count stayed 994 before and after.
- The all-target Release build was stopped after about 17 minutes of active `level_flow_tests.cpp` optimization amid concurrent worktree builds and heavy swapping. The subsequent full `ctest -j 3` was stopped after 72/231 cases passed, with no observed failure or timeout; `champions-level-flow-tests` was unavailable. Full-suite status remains unverified. The requested focused arm64 CTest above completed independently.

### Audit commands

`DISC` is a caller-supplied path to the read-only owner disc; `GODOT` is the pinned 4.5.2 executable identified in the command block below. All output directories are ignored `work/` paths.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target champions-quests-tests champions-script-commands-tests -j 2
ctest --test-dir build/native --output-on-failure -j 1 -R '^(quests|script-commands)$'
cmake --build build/godot-extension --config Release --parallel 2
nice -n 10 $GODOT --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$PWD/work/mission-menu-audit-headless-final" --route-only
nice -n 10 $GODOT --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$PWD/work/mission-menu-audit-cancel" --menu-cancel-only
nice -n 10 $GODOT --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$PWD/work/mission-menu-audit-rendered-final" --route-only --rendered
nice -n 10 python3 work/play-route/run-godot.py
nice -n 10 $GODOT --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder="$DISC" --cache="$PWD/work/mission-menu-audit-sweep-cache" --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary="$PWD/work/mission-menu-audit-sweep-summary.json"
cmake --build build/native -j 4
nice -n 15 ctest --test-dir build/native --output-on-failure -j 3
```

## Verification before audit fixes (retained baseline)

- macOS arm64 Release CTest `^(quests|script-commands)$`: **2/2 passed** after updating five older synthetic vector expectations for the newly executed down-column rule. `champions-quests-tests` is Mach-O arm64.
- Godot 4.5.2 headless `--route-only`: exit 0; **12/12** row images/positions, **32/32** textures, small font present, locked down stayed at row 0/column 0, story request `blood1`, and campaign/record carry into the requested level.
- Godot 4.5.2 headless `--menu-cancel-only`: exit 0; Escape was handled, state 4 counter reached -1 after the documented fade ticks, and the menu closed.
- Godot 4.5.2 rendered `--route-only`: exit 0; the final captured menu was inspected in the play viewport. The locked versus available image check found **161 changed samples** in the second node. Both PNGs are only in ignored `work/mission-menu-route-rendered-final/`. The separate dialog visual check reports a refusal while this bounded route exits 0.
- Godot 4.5.2 dev matrix (`python3 work/play-route/run-godot.py`): **54/54 commands exited 0**. `scene_play_flow` and `ui_layout_smoke` retain the same internal script/API errors recorded in the play-route baseline; an exit code alone does not clear them.
- 111-level scripts-on sweep: **111 tried / 111 loaded / 111 played**, 224 checks passed, 0 failures, 0 refusal kinds, 111 scripts attached and 0 script faults. The read-only disc file count was 994 before and after.
- Full-combat headless smoke: **2,620 checks, 0 failures**; `FULL_TRACE` **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to the `7ed3f21` play-route baseline.

## Remaining evidence boundary

The executed research establishes static centered scroll endpoints, lookup calls and controlled tints, not interpolated scroll frames, moving backdrop rectangles, localized glyph raster layout, or GPU packet/blend parity. The Godot view refuses unresolved backdrop rectangles, sparkle placement, detail glyph alpha, glow pulse tint, and network prompts without both first-active and pause state. Other-row icon/link positions and the detail frame's raster style remain unverified. The rendered capture is visual evidence of this client, not a retail parity claim. No Norrath behavior is claimed.

## Commands

From this worktree; `DISC` is the read-only owner disc folder and `GODOT` is the pinned Godot 4.5.2 executable:

```sh
DISC="${DISC:?Set DISC to the read-only owner disc folder}"
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
