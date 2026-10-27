STATUS: MORE — the bounded mission-2 class port is implemented and integrated; loaded animation/AI, boss spawning and original mission completion remain refused.

# Mission 2 hostile implementation

Worktree `port-mission2-hostiles`, branch `codex/port-mission2-hostiles`, base `252f34a` on `codex/valor-fight`. No sibling worktree edits, push, issue or PR.

Local implementation commits: `35521d6b` (actor contracts and synthetic tests), `bb905a5c` (factory/shared-World integration, Godot observations and boundary documentation). This report is committed separately.

Evidence: `research-mission2` at `ffff9a7`, `docs/mission2-enemies.md`, its authored reference/difftest, and the addressed Clockwork/Droid/IronGuardian notes in `docs/monsters-2.md`. Both the hostile audit and `research-mission2-fix2.md` read PASS; the hostile verdict was checked again after implementation.

## Changes

- Added portable C++17 Clockwork cutter/hammer, Droid and IronGuardian actors over the existing shared World and memory. The host runs the base constructor, selected balance row and stat service once; class adoption adds no allocation, shared draw or second stat roll.
- Definitions resolve the executable's names at runtime. No extracted resource-entry names or game bytes are in the patch. Constructor fields, event-ID handoffs, 16 measured tick paths, melee flags, inherited damage, Droid explosion dispatch and death/drop arguments retain their research boundaries.
- Wired the factory, campaign World, native probe and Godot observations. Class identities stay distinct; the boar presenter skips the new classes. Missing loaded constructor services and non-death state entry remain explicit refusals.
- Added synthetic tests and an opt-in, counts-only real-data Godot test. The synthetic services are authored answers, not an original loaded-level oracle. Actor save/load and original-client parity are not established.

Files: `native/{include/champions,src,tests}/mission2_actors*`, the campaign target file, script host request/base-constructor path, campaign fight and probe, session bridge, boar presenter, `godot/tests/mission2_hostiles_real.gd`, `docs/MISSION2_HOSTILES.md` and its roadmap link.

## Verified acceptance

- New actor test: **457 checks passed**. Four independent balance-factor rows, 16 ticks / 162 ordered calls, three melee dispatches, four death routes, one Droid explosion, inherited damage and actual shared-World damage to authored targets. Inactive/expiry/budget, the unconditional Droid unlisted-state reset and contained-refusal cases are covered. The expanded actor check was rerun after that reset correction.
- Focused native CTest: **5/5 passed**, including the expanded mission-2 test after the final Release rebuild.
- Native counts-only probe: four levels, **120 ticks each**, zero start/runtime failures. Godot bridge probe: four levels, **60 ticks each**, zero failures; no new class drew a boar instance.

| Level index | Cutters | Hammers | Droids | IronGuardian |
|---|---:|---:|---:|---:|
| 0 | 23 | 29 | 0 | 0 |
| 1 | 23 | 23 | 30 | 0 |
| 2 | 20 | 14 | 0 | 0 |
| 3 | 0 | 0 | 0 | 1 |
| Total | 66 | 66 | 30 | 1 |

The final native census matched the earlier run, with 6–7 recorded refusal kinds per level. Zero runtime failures does not mean the loaded services were executed.

- Final Godot development matrix: **54/54 commands exited zero** in **286.13 s**, using isolated test storage after the final rebuild. Initial run: 53/54; `session_facing` failed its recovery assertion. Its unchanged isolated retry passed **62/62**, and the final full matrix passed. Initial failure and retry logs are retained; no facing/recovery implementation was changed. The temporary project override and task-owned user data were removed.
- Matrix log review: the unchanged scene-play-flow fixture emits two missing-`loaded` script diagnostics while reporting 65 checks / zero failures. The UI-layout missing-tint error is an intentional required-argument check. Neither diagnostic comes from the new actor path; their logs are retained. The final real-data Godot census also reports an ObjectDB exit-leak warning; it exits zero with 163 actors / zero failed checks, without establishing leak-free scene teardown.
- `FULL_TRACE`: **36,402 bytes**, SHA-256 **9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659**, unchanged. `FULL_SAVE_LOAD`: **300/300**. The Full fixture does not instantiate these mission actors.
- Scripted level sweep: **111/111 tried, loaded and played**, **230 checks**, zero failures/refusal kinds; 111 scripts attached, 76 started. Four slice exits: 0/0/0/0.
- Journey: **24 steps each**, **13 rendered captures**, headless/rendered exit **1** because the journey remains incomplete. Both reproduce one kill, XP/level gain and a loot roll. Ground loot construction/pickup and Gold remain refused. Fight fields agree apart from timing/presentation metrics. Rendered output was inspected. The original game folder remained **994 files before and after**.

## Commands

Command inputs: `GODOT_BIN=../integration/work/runtime/Godot.app/Contents/MacOS/Godot`, `GODOT_CPP_DIR=../integration/work/deps/godot-cpp`, `SWEEP_SCRIPT=../play-enemies/work/play-enemies/run-sweep.sh`, `RTA_DISC=../../private-validation/rta-disc`, and `CENSUS_CACHE=work/mission2-hostiles/real-godot-final-cache`. Paths are shown relative to the worktree and were resolved to absolute paths for execution.

`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`

`cmake --build build/native -j 4`

`ctest --test-dir build/native --output-on-failure -j 3`

The three Godot extensions were configured and built with the pinned local godot-cpp checkout:

`cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64`

`cmake -S gdextension/level_texture -B build/level-texture-debug -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64`

`cmake -S gdextension/creature_anim -B build/creature-anim-debug -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64`

Each build directory was built with `cmake --build <directory> -j 4`. The main extension was rebuilt after the final actor correction.

`python3 -u work/mission2-hostiles/run-godot.py`, then `python3 -u work/mission2-hostiles/run-godot-final.py` — the existing 54-command matrix with worktree-local output directories; the final run used isolated test storage.

`bash "$SWEEP_SCRIPT"` — the existing four sequential scripts-on, 60-tick slices.

`python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/mission2-hostiles/playthrough`

`"$GODOT_BIN" --headless --path godot --script res://tests/mission2_hostiles_real.gd -- --game-folder="$RTA_DISC" --cache="$CENSUS_CACHE"`

The counts-only native probe used `--ticks 120 --counts-only` on each of the four documented census inputs. Paths select owner inputs and local tooling; no input content is recorded here. Detailed logs remain ignored under `work/mission2-hostiles/` and `work/play-enemies/sweep/`.

## Remaining evidence gates

Loaded holder/model/animation results and complete event parameters are missing. Live target AI, non-death state entry, missiles/junk, hammer steam/corpse events, Droid fuse entry/props and IronGuardian's live Droid handles/timing remain unexecuted. The live scene refuses before producing attack events from unknown animation state. Synthetic hits are not proof of original-level landed attacks.

Part shedding, loaded death clips, ranked/tinted boss loot, named quest drop, loot landing/pickup, Gold source, save/load and portal completion remain refused. The native numeric actor census is not mission completion. See `docs/MISSION2_HOSTILES.md` for exact service boundaries.

Arm64 Release configure/build passed. An earlier build used the generated graph from before the new library was added and failed at the probe link; CMake regeneration and the full build resolved that link. Full `ctest -j 3`: **240/242 passed** in **3533.61 s**. `player-exact` hit its 240-second CTest timeout; `session-facing-exact` hit its subprocess 600-second timeout. No state/trace mismatch was reported by either timed probe. Sequential `-j 1` retries: facing passed in **454.87 s**; player again timed out at **240.07 s**. The unchanged player vector command then passed **274,304 exact complete-state cases and transport refusals** in **316.17 s**, measured separately with a 900-second wall bound. The CTest limit remains unchanged; the full CTest gate is not clean.

Retry commands: `ctest --test-dir build/native --output-on-failure -j 1 -R '^(player-exact|session-facing-exact)$'`, then `python3 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz`.
