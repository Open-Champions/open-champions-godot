STATUS: MORE — wave 25 is merged, the full arm64 CTest and HUD/level gates pass, but the play session still has no faithful potion, selector, gold-floater or minimap input producer. No undocumented input was substituted.

# Native HUD round 3 — 2026-10-22

Branch `codex/native-hud`; merged wave 25 `6ba8afe` locally. Nothing pushed and no PR.

## Producer audit

- The live player record exposes level, acknowledged level, XP, XP thresholds and direct gold. The HUD already consumes those for the XP bar. Record HP/mana are deliberately separate from the simulated actor until the original `Player_run` mirror is ported.
- `Player_usePotion` needs a live player object, the bag-2 cells and names, pending currents, a pressed potion button, inventory removal/drop and regen writes. The current play session has no `Player_usePotion` execution or bag-2 player-runtime bridge. Script item commands can update binder inventory memory, but they do not create the player action/event. No potion notice was claimed from a merely present record.
- The selector needs raw local D-pad held/pressed words, all nine gates, configured A/B/Use masks, a skill record and the quick-slot handler. The current scene has authored movement/dialog keys and no original raw-pad/configuration/skill runtime. A Godot key was not substituted for the configured mask.
- Gold floaters begin only at `Gold_onTouch` or the two chest interactions. Neither touch/interaction path is constructed in this play session. Script gold and the new-character 500 are direct record writers and must leave the counter hidden.
- The minimap projector needs the local camera position/yaw, world automap grid, texture gate, zoom and bounds in one observation. The scene has level geometry and an authored view, but no corresponding original local camera/automap producer. VU1 tile arithmetic, the atlas layout and draw-script icon effects remain Unknown and refused.

Sources: `t3/research-hud-gaps` `f7c3964` (`docs/spec/hud-quickslots.md`, `hud-gold.md`, `hud-minimap.md`), `t3/research-player-state`, `t3/research-items`, `t3/research-ui-text-gaps`, and the ported session, item, message, player, HUD and WORLD services. The specific absent edge is runtime production, not a missing arithmetic bridge.

## Change

The read-only real-level HUD smoke now asserts the potion, minimap-tile and minimap-icon refusal labels as well as its existing XP, health, direct-gold, quick-slot and projector checks. This guards the evidence boundary after the wave 25 merge. No new gameplay producer is represented as working.

The first full CTest exposed a reproducible `script-commands` teardown crash. AddressSanitizer identified a heap use-after-free: `Host` destroyed its target handle cells before `Variables` removed their paired world-list nodes. `Host::~Host` now clears those pairs while the cells and world are alive. The existing charm-live-target test passes under AddressSanitizer and in Release after this fix.

## Verification

- arm64 Release native configure/build: passed. First full CTest: 233/234 in 899.46 seconds, with `script-commands` crashing; `player-exact` and `skills-b-exact` passed and there were no timeouts. Serial `script-commands` reproduced the crash, and the AddressSanitizer run located the use-after-free. After the fix, focused `script-commands`, `hud-extras` and `player-state` passed 3/3. Final full CTest: **234/234 passed in 661.99 seconds**. No timeout serial rerun was needed.
- Godot arm64 `template_debug` extension and archive extension builds: passed. The post-fix dev matrix passed 54/54. Full headless combat trace: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`; `FULL_SAVE_LOAD` presentation matched at 300/300 presents.
- Both post-fix read-only four-slice sweeps, scripts on and off: each 111/111 tried, loaded and played; 230 checks, zero failures, no top-level refusals.
- Post-fix real-level HUD smoke: headless and GL rendered 15/15 checks. The 1120×780 rendered frame was inspected: the original HUD frame, health and XP appear over the real level, while unsupported elements remain absent. The practice figure is still explicitly labelled as such.

Counts-only validation logs and the rendered frame are under ignored `work/native-hud-r3/` and `work/private/`. The owner's game folder was read only; no game bytes, extracted assets or decoded content were committed.

## Commands

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^script-commands$'
cmake -S native -B build/native-hud-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address'
cmake --build build/native-hud-asan --target champions-script-commands-tests -j 4
ASAN_OPTIONS=symbolize=0:abort_on_error=1 build/native-hud-asan/champions-script-commands-tests native/tests/vectors/script_commands.tsv native/tests/vectors/script_host.txt
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 1 -R '^script-commands$|^hud-extras$|^player-state$'
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/mac-template_debug -DGODOT_CPP_DIR=../integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DCHAMPIONS_LEVEL_FLOW_WATER_ROOT=$PWD -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-template_debug --parallel 2
cmake --build build/mac-template_debug --target champions-archive-extension --parallel 4
python3 work/native-hud-r3/run-godot.py
bash work/native-hud-r3/run-slices.sh hudon-r3 --scripts
bash work/native-hud-r3/run-slices.sh hudoff-r3
cmake --build build/mac-template_debug --parallel 2
python3 work/native-hud-r3/run-godot-after-fix.py
bash work/native-hud-r3/run-slices.sh hudon-r3-fix --scripts
bash work/native-hud-r3/run-slices.sh hudoff-r3-fix
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/native_hud_play_real_smoke.gd -- --disc=/Users/syndicatemike/Projects/open-champions/private-validation/rta-disc --cache=/Users/syndicatemike/Projects/open-champions/worktrees/native-hud/work/private/hud-r3-fix-cache
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --path godot --rendering-method gl_compatibility --script res://tests/native_hud_play_real_smoke.gd -- --disc=/Users/syndicatemike/Projects/open-champions/private-validation/rta-disc --cache=/Users/syndicatemike/Projects/open-champions/worktrees/native-hud/work/private/hud-r3-fix-cache --capture=/Users/syndicatemike/Projects/open-champions/worktrees/native-hud/work/private/hud-r3-fix-frame.png
```

Each long command wrote its counts-only log under `work/native-hud-r3/`.

## Remaining

Implement the native player action/inventory and quick-slot handler, world gold touch/chest event chain, and original local camera/automap state before submitting those HUD inputs. Keep the VU1 tiles, atlas layout and scripted icons refused pending the named research evidence. Original-client fidelity, Intel/Windows/Linux execution and physical interaction remain unverified.
