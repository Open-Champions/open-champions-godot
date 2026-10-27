STATUS: MORE — the existing Full combat Player now has a live HUD data bridge. The ordinary level session still lacks the Player/inventory producers, and the required campaign playthrough retains the supplied baseline's failures.

Branch `codex/native-hud`, base `78e0d2f`; implementation `21b5e8c`, lifecycle-fixture correction `0acabd3`, negative-current/XP-order fix `a84ab1f`, test-compilation fix `95e2edb`. Local commits only; no push, PR, issue, sibling-worktree edits or private-data commit.

The new portable `hud_player.hpp` / `hud_player.cpp` read the attached Player's owned memory, checking object membership, real-player/copy flags and slot. Full combat publishes a separate `hud_player` observation: HP, mana, pending values, level, acknowledged level, XP, gold, cooldown, quick-slot indices, ability IDs, potion counts and timer words. The HUD selects that source exclusively; a missing/stale source refuses instead of falling back to the character companion or isolated actor. XP thresholds still require the selected executable, and absent positive mana maxima remain refused.

Live gold adopts the native World's existing floater/payout state. It never replays a pickup, runs a second payout, or consumes another seed. Local fades and motion advance per displayed draw, independently of observations. Leaving play, changing source or rewinding the native frame discards local display history. The legacy synthetic pickup path remains separate. Native tests cover the existing `Gold_onTouch` producer through payout, source isolation, signed widths, refusals and read-only observation/drawing; Godot tests cover the production bridge and HUD selection.

The adapter now admits pending overlays at negative observed currents with a positive maximum and clear dead bit, as the executed contract requires. XP also remains before the glow/frame in submission order when mana is absent. Focused synthetic checks and rendered real-art compositions cover both cases; the final Full combat trace remains unchanged.

Evidence: read-only research `t3/research-hud-gaps` `f7c3964` (`hud-status.md`, `hud-xp.md`, `hud-gold.md`, `hud-quickslots.md`) and `t3/research-hud-inputs` `44c6720` (`docs/spec/hud-input-producers.md`), with the supplied PASS audits. Only addressed Observed/audited fields and existing executed arithmetic are used. No original tables, bytes, art or decoded private content were added.

Files: the two new native HUD files and `native/tests/hud_player_tests.cpp`; minimal native/GDExtension CMake additions; `session_controller.cpp`, `hud_extras_service.cpp`, the two HUD views, their existing Godot smoke tests, `docs/HUD_EXTRAS.md`, and this report.

Remaining: ordinary campaign Player construction/recalculation and record-to-object mirror; authoritative bag-2 inventory and potion/regen edge; raw selector pad/configured masks/messages and icon/timer draw ownership; campaign gold/chest producers; damage-flash clear/acknowledgement. Quick-slot/count/timer observations are metadata, not an implemented belt. Minimap VU1 tiles, atlas layout, fractional reveal, scripted icon effects, undocumented coin dimensions, console timing and raster parity remain refused/unverified. The Full fixture remains an authored synthetic host, not campaign acceptance. Intel/Windows/Linux execution and physical controller interaction were not tested.

| Gate | Result |
|---|---|
| arm64 Release configure/build | PASS; final build 79.45 seconds after the test-only compilation fix below |
| Focused native CTest | 4/4 PASS; direct `hud-player-live` executable: 645 checks PASS |
| Full `ctest -j 3` plus required isolated retry | Initial 234/235 PASS in 3,345.66 seconds; `player-exact` hit its 180-second child-process timeout (350.06 seconds total). Isolated `-j 1` retry: 1/1 PASS in 307.31 seconds. All 235 cases passed across these runs; the initial full run was not clean. |
| Godot arm64 `template_debug` build | PASS, including final incremental rebuild |
| Godot dev matrix on final code | 54/54 PASS; no script errors; lifecycle case has 65 checks, 0 failures |
| Scripts-on level sweep | 111 tried / 111 loaded / 111 played; 230 checks, 0 failures, 0 top-level refusals |
| Scripts-off level sweep | 111 tried / 111 loaded / 111 played; 230 checks, 0 failures, 0 top-level refusals |
| `FULL_TRACE` | Unchanged: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` |
| `FULL_SAVE_LOAD` | 300/300 presents; continued/uninterrupted presentation hashes match |
| Real-level HUD smoke | Headless 16/16 and GL rendered 16/16 PASS |
| Requested loop playthrough | FAIL: headless exit 1 and rendered exit 1; 21 steps each, 0/11 screenshots; private file count 994 before / 994 after |

The playthrough's refused enemy fight, absent pause Save, refused load/carry and subsequent level change are already recorded in `work/loop-playable-integration-r25-report.md`. The load reaches the unchanged `present_terrain` single-plain-surface guard, present at the supplied `78e0d2f` baseline. The rendered variant also retains its unmapped player-mesh refusal. These are not reported as successful campaign progression; no save/enemy-owner changes were made in this HUD round.

Initial matrix log review found two missing-property script errors in the existing lifecycle spy despite its zero exit code. Adding its required empty `loaded`/`catalogue` fields resolves both; the affected case and the final complete 54-command matrix pass without script errors. The UI-layout smoke intentionally rejects a call without its required tint argument. Existing ObjectDB shutdown warnings and duplicate-static-library linker warnings remain; no warning-free run is claimed. Both 111-level sweeps and the playthrough were repeated on the final code.

The Release build exposed a compiler bottleneck in the pre-existing generated level-flow assertions. The original translation unit was still optimizing after 24:35 wall / 7:47.37 CPU; sampling showed Clang SROA/PromoteMemToReg. Factoring cold assertion-message construction and grouping the unchanged 2,298 complete cases into 36 functions allowed the final Release build to finish in 79.45 seconds. `-O3 -DNDEBUG` remains unchanged. The original vector file and the payload recovered from the generated wrapper both hash to `4534b382289c405bbc331773f6b0d872d179c7bd6eb92bd2c74cb3d556cd9278`; all 36 batches are called once in their original order, with original failure-line references. These changes affect only the native test harness (`level_flow_targets.cmake`, `level_flow_tests.cpp`, new `level_flow_check.cpp`, and `tools/batch_level_flow_vectors.py`), not game code. The tradeoff is one build-time wrapper-generation step using the test suite's existing Python dependency. Interrupted preliminary runs are not counted as passing gates; only verified owned PIDs were stopped.

Inspected the actual GL HUD capture and separately labelled authored live-field compositions over the same real art, including negative currents and XP without mana: health, mana, XP, pending fills, level text and gold glyphs occupy the documented layout. These compositions are not evidence that the ordinary campaign has those producers. Images and all counts-only logs remain under ignored `work/native-hud-r5/` and `work/private/`.

Commands used (with `HUD_GODOT` denoting the pinned local Godot 4.5.2 executable, `GODOT_CPP_DIR` its pinned godot-cpp checkout, and `RTA_DISC` the approved read-only disc):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 1 -R '^hud-extras$|^hud-player-live$|^player-state$|^script-commands$'
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'
build/native/champions-hud-player-tests
cmake -S gdextension -B build/mac-template_debug -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-template_debug --parallel 2
"$HUD_GODOT" --headless --path godot --script res://tests/play_hud_smoke.gd
python3 work/native-hud-r5/run-godot-head.py
"$HUD_GODOT" --headless --path godot --script res://tests/scene_play_flow_smoke.gd
RTA_DISC="$RTA_DISC" bash work/native-hud-r1/run-slices.sh hudon-r5-head --scripts
RTA_DISC="$RTA_DISC" bash work/native-hud-r1/run-slices.sh hudoff-r5-head
"$HUD_GODOT" --headless --path godot --script res://tests/native_hud_play_real_smoke.gd -- --disc="$RTA_DISC" --cache="$PWD/work/private/hud-r5-cache"
"$HUD_GODOT" --path godot --rendering-method gl_compatibility --script "$PWD/work/native-hud-r5/visual-live-final.gd" -- --disc="$RTA_DISC" --cache="$PWD/work/private/hud-r5-cache" --capture="$PWD/work/private/hud-r5-actual-final.png" --capture-live="$PWD/work/private/hud-r5-authored-live-final.png" --capture-xp-only="$PWD/work/private/hud-r5-xp-only.png" --capture-negative="$PWD/work/private/hud-r5-negative.png"
python3 tools/run_loop_playthrough.py --godot "$HUD_GODOT" --game-folder "$RTA_DISC" --out work/native-hud-r5/playthrough-head
```
