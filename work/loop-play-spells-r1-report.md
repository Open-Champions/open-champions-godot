STATUS: PARTIAL — rank-one Disease Bolt is selectable and castable in the opt-in Full combat play fixture, with exact mana debit, regeneration, projectile spawn and fixture damage; the original HUD selector, campaign enemy hits, higher ranks and projectile expiry remain refused or unverified.

# Player spell casting, round 1 — 2026-10-23

## Scope and evidence

Worktree `codex/play-spells` starts at wave-25 `6ba8afe`. The port follows the PASS audit at `~/Projects/champions-re-wt/cursor-runs/audits/research-spells.md` and the pinned `t3/research-spells` `2225004` contract `docs/spec/player-spell-pipeline.md`. It uses the existing executed `Player_run`, cast helper and mana-regeneration code. It adds Disease Bolt skill dispatch, projectile construction/run/touch, and a call through the existing combat damage handler.

The skill and projectile table pointer values were checked read-only against the US v2.00 executable. This establishes addresses only; unmodelled destructor and drawing bodies remain refused. No executable bytes were copied into the repo or report.

The Full fixture assigns Disease Bolt to ability A and exposes a select button plus U cast input. The fixture's 100/100 mana and matching ability-name marker are explicitly synthetic test inputs; they are not retail starting stats or literal game text. Rank-one cost is the observed 9 mana, recast is 30 frames, and the existing player state machine controls request admission, animation threshold and mana regeneration. The smoke observes mana at 91 when the projectile spawns and a later increase toward the cap. The projectile hit invokes the reviewed damage path and reduces a Full-fixture enemy's HP.

The Full fixture submits ability A's sampled pressed and held state to `Player_run`. Releasing it clears the held state immediately; a press and release between ready ticks cannot admit an ordinary kind-0 cast on a later neutral tick. The native and scene smokes hold the button until the cast event. This is separate from the original local HUD selector's three-frame input mask, which is not implemented here.

## Verification

- Native arm64 Release build: pass. Focused `ctest --test-dir build/native -R '^play-spells$' --output-on-failure`: 1/1 pass. The first full `ctest --test-dir build/native --output-on-failure -j 3` aggregate was **232/234** in 1,328.11 s: `combat-runtime-2-exact` found the newly installed table in the default fixture's memory digest, and `script-commands` crashed without output. I moved the new table into opt-in selection. The second full aggregate was **233/234** in 1,043.06 s: only `script-commands` crashed. AddressSanitizer traced that test's crash to `Host` freeing its target handle cells before `Variables` unregisters linked objects. The member order is now corrected, so the cells outlive variables teardown. `script-commands` passed 20 consecutive Release runs and its AddressSanitizer run. The final rebuilt-tree aggregate passed **234/234** in 966.58 s.
- Godot GDExtension, level-texture and creature-animation template-debug builds: pass. `godot/tests/play_spells_smoke.gd`: pass for selection, mana decrease, projectile spawn and Full-fixture hit. Full scene smoke uses the visible select button and U key; selection survives fixture save/load, and the rendered run passed with spawn, hit and HP loss. I inspected the rendered Full fixture capture and mana readout.
- Godot development matrix after the native teardown fix: **54/54** exit zero after building this worktree's separate creature-animation extension. `FULL_TRACE`: `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, 36,402 bytes, unchanged from wave 25. The new spell scenario runs after that trace and is opt-in, so it does not change the established scenario. Its scene record confirms selection, spawn, mana debit, hit, and fixture HP loss.
- Read-only disc sweeps at 60 ready ticks per level after the script-host teardown fix: scripts on **111/111** tried, loaded and played, 230 passed checks, zero failures and zero top-level refusals; scripts off **111/111**, 230 checks, zero failures and zero top-level refusals. The scripts-on internal stub-refusal count remains 163, with 15,472 unbuilt objects. The disc had 994 files before and after the final runs; no private data was written or committed.

## Audit fixes

PSP-1 (`play-spells-r1.md`): removed the Full fixture's three-ready-tick post-release hold. `press_disease_bolt(held)` now submits the supplied held level and a pressed edge; `release_disease_bolt()` clears the held level immediately. The native and Full scene smokes first release before a ready tick and check that no cast starts on later neutral ticks. They then hold ability A through state-5 admission and projectile dispatch, releasing after spawn. This follows the pinned `Player_run` pad contract; the separate local HUD selector mask is not used as cast input.

Post-fix checks: arm64 Release configure/build and focused `ctest --test-dir build/native -R '^play-spells$' --output-on-failure` passed (1/1); full `ctest --test-dir build/native --output-on-failure -j 3` passed **234/234** in 858.56 seconds. The rebuilt GDExtension and `play_spells_smoke.gd` passed; Full scene smoke passed, including `FULL_TRACE` `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` (36,402 bytes). `python3 work/play-spells-matrix-r5/run-godot.py` passed **54/54**. Read-only `bash work/run-slices-r21.sh spells-audit-on --scripts` and `bash work/run-slices-r21.sh spells-audit-off` each passed **111/111** levels tried, loaded and played, 230 checks, zero failures and zero top-level refusals. Scripts-on retained 163 internal stub refusals; disc file count remained 994.

## Limits

This is an authored Full combat fixture path. The original D-pad/HUD quick-slot message producer is a separate reviewed contract and is not in this wave-25 base. The campaign currently has no live enemies in this branch, so campaign spell hits and end-to-end gameplay are unverified. The audited research did not establish Disease Bolt's destructor or higher-rank cost float bits; the native port refuses expiry and ranks above one instead of supplying those paths. The rendered hit capture shows the resulting explicit stopped-session refusal; the scene smoke asserts the fixture enemy's HP loss. Original visual projectile effects are also not established by this fixture hit; the selected recipe set reports missing particle definitions. The skill difftest verified the outgoing damage call with stubbed world services; the new native fixture hit is local integration evidence, not original-client parity.

An additional rendered `scene_play_flow_smoke.gd` probe at 800×600 failed its narrow-window status-visibility assertion (the status was below the viewport). The required headless matrix case and the rendered Full-spell scenario passed. This extra layout issue was outside the Full-spell view and remains open.

Counts-only logs and rendered captures are under ignored `work/`. Commands used:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 2
cmake -S gdextension/level_texture -B build/level-texture -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/level-texture --parallel 2
cmake -S gdextension/creature_anim -B build/creature-anim -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/creature-anim --parallel 2
/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/play_spells_smoke.gd
/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/playable_wave_smoke_full_combat.gd -- --ready-ticks=640
python3 work/play-spells-matrix-r5/run-godot.py
bash work/run-slices-r21.sh spells-final-on --scripts
bash work/run-slices-r21.sh spells-final-off
```

The Godot runner and sweep driver are ignored local copies of established repository workflows; no sibling worktree was edited.
