STATUS: MORE — the merged player record now drives the real-level XP bar and documented starting currents, and the HUD has strict input bridges for quick slots and the minimap projector; actual potion, selector, gold-floater and minimap producers remain absent. The full arm64 CTest run could not finish under severe shared-machine load; its HUD and player-state tests passed.

# Native HUD extras, round 2 — 2026-10-22

Branch `codex/native-hud`, worktree `~/Projects/open-champions/worktrees/native-hud`. Merged `codex/playable-integration` commit `2e57376` locally as `f02cdec`; implementation `c890634`, CTest timeout adjustment `510e51b`. Nothing pushed; no PR.

## Wiring and evidence

- Resolved the merge conflicts in `gdextension/CMakeLists.txt` and `godot/ui/play_hud.gd`. The merged player record supplies acknowledged level, experience, gold and executable-backed XP thresholds. The play HUD uses the native `hud_extras` XP draw and the checked real `bar_green` art. An older layout path also submitted XP after the merge; that duplicate was removed.
- For a newly created record, `set_player_character` applies `hud_extras::firstLevelCurrent` to the executable-backed maxima and marks currents known. Imported and loaded currents are preserved. The play bars still use the separate simulated actor; record mana is not substituted for its missing runtime mana. This preserves the player-state audit's PS2-2 boundary.
- Pending health and mana rectangles keep the documented local-slot/dead-bit gate without a health-positive shortcut.
- `ChampionsHudExtrasService` now exposes the tested `usePotion`, selector gates/axes/button mask/message path to complete, caller-supplied bag-2 and raw-pad observations. `play_hud.gd` handles unique potion event IDs and computes selector input when those observations exist. The local-slot and positive-HP selector gates are enforced before a raw-pad read.
- The play HUD calls the minimap projector only with complete camera/grid/world-texture inputs. The current session supplies none. Its VU1 tiles and draw-script icons are explicitly labelled Unknown and refused. The gold coin size, cosmetic seed path and unexecuted notice starters are also explicitly refused. A record gold change alone never starts the gold counter.

Research: `t3/research-hud-gaps` `f7c3964`, audit PASS, especially `docs/spec/hud-status.md`, `hud-xp.md`, `hud-gold.md`, `hud-quickslots.md`, `hud-minimap.md`. No retail bytes or decoded content were committed.

Files changed after the merge: `gdextension/src/session_controller.cpp`, `gdextension/src/hud_extras_service.cpp`, `godot/ui/play_hud.gd`, `godot/tests/play_hud_smoke.gd`, new `godot/tests/native_hud_play_real_smoke.gd` and its UID, `docs/HUD_EXTRAS.md`, `native/CMakeLists.txt`, and this report.

## Verification

- Arm64 Release native build succeeded; `file` confirms the HUD test binary is arm64. Focused `ctest -R 'hud-extras|player-state'` passed 2/2.
- Godot 4.5.2 arm64 `template_debug` extension built. `play_hud_smoke.gd` passed after fixing the duplicate XP. Final dev matrix: 53/54 on the first pass; `creature-blend-gl` equation-9 pixel check failed once, then passed all 22 checks alone. Initial matrix log and isolated rerun are retained.
- Real-level HUD smoke passed all 12 counts: player/current/thresholds present, XP and health drawn, pending health gated correctly, and mana, direct gold, quick slots and minimap refused for missing play inputs. Rendered GL frame inspected in the play viewport. Separate real-art smoke decoded 11/11 green frames, drew XP with all 11 frames, and drew gold text for 133/133 visible frames with a caller-supplied pickup.
- Both read-only 111-level sweeps: scripts on and off each tried, loaded and played 111/111, with 230 checks, zero failures and zero refusals. Both loaded 44 green frames across four slices with zero art refusals.
- Headless Full: 2,620 checks, zero failures, `FULL_TRACE` 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, matching the merged combat round-3/4 report.
- Native full CTest: **not complete**. The initial concurrent run timed out at `player-exact`'s 240-second property and was interrupted; the first isolated retry also reached 240 seconds. A direct no-matrix run passed all 274,304 exact complete-state cases and transport refusals in about eight minutes. `native/CMakeLists.txt` raises only that test's CTest timeout to 900 seconds; no assertions changed. In the subsequent full `ctest -j 3`, `player-exact` passed in 422.79 seconds and 35 of the first 37 tests passed. `skills-b-exact` timed out at its pre-existing 150-second limit; `control_anim_cache-exact` hit its own runner's 600-second subprocess timeout. The run was interrupted after those two timeouts with machine load about 382 on 18 CPUs. Isolated `skills-b-exact` timed out again at 150 seconds, and a direct invocation reached its own 120-second native subprocess timeout. Isolated `control_anim_cache-exact` again reached its runner's 600-second native subprocess timeout (605.47 seconds total). Machine load was about 414 at that point. No HUD/player-state assertion failure was reported. The full suite needs a quiet-machine rerun; see `work/native-hud-r2/ctest-arm64-final.log` and the two isolated logs.

## Commands

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -R 'hud-extras|player-state'
ctest --test-dir build/native --output-on-failure -R '^player-exact$'
python3 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -R '^skills-b-exact$'
python3 tools/test_skills_b_vectors.py --exe build/native/champions-skills-b-tests --vectors native/tests/vectors/skills_b.tsv.gz
ctest --test-dir build/native --output-on-failure -R '^control_anim_cache-exact$'
cmake -S gdextension -B build/mac-template_debug -DGODOT_CPP_DIR=../integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DCHAMPIONS_LEVEL_FLOW_WATER_ROOT=$PWD -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-template_debug --parallel 2
cmake --build build/mac-template_debug --target champions-archive-extension --parallel 4
python3 work/native-hud-r2/run-godot-final.py
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/play_hud_smoke.gd
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --path godot --rendering-method gl_compatibility --script res://tests/creature_blend_smoke.gd
bash work/native-hud-r2/run-slices.sh hudon-r2 --scripts
bash work/native-hud-r2/run-slices.sh hudoff-r2
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/native_hud_play_real_smoke.gd -- --disc=/Users/syndicatemike/Projects/open-champions/private-validation/rta-disc --cache=/Users/syndicatemike/Projects/open-champions/worktrees/native-hud/work/private/hud-r2-cache
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --path godot --rendering-method gl_compatibility --script res://tests/native_hud_play_real_smoke.gd -- --disc=/Users/syndicatemike/Projects/open-champions/private-validation/rta-disc --cache=/Users/syndicatemike/Projects/open-champions/worktrees/native-hud/work/private/hud-r2-cache --capture=/Users/syndicatemike/Projects/open-champions/worktrees/native-hud/work/private/hud-r2-frame.png
```

## Remaining

The play session has no bag-2 inventory/`Player_usePotion` event, raw D-pad/configured masks or quick-slot handler, gold touch/chest floater, or local-camera/automap texture inputs. The record-to-simulated-player mana link is also absent. The native arithmetic and optional HUD bridges do not claim those runtime producers. Unknown minimap tile arithmetic and script-gated icons remain refused. Full CTest needs a quiet-machine rerun. No Rosetta, export matrix or Windows/Linux build was run in this round.
