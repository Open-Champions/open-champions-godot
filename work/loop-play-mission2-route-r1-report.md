# Mission 2 hub-return route, round 1

STATUS: PASS. The direct-call hub-return list stores the resolver hub, appends a current level and removes one other hub entry, and the row-0 portal is accepted by the existing pre-load stop. Player bytes and the quest table are copied through that stop. The scene does not call this module. CTest 260/260, strict Godot 54/54, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged, journeys 80/81 with pickup 10 to 11.

Code: `676d1af5` on `70d2de19`. This report is a later commit on `codex/play-mission2-route`.

Research pin: `research-mission2-route` `b8e2d11`. Audits `research-mission2-route-r2.md` and `research-mission2-route-r2-fix.md` are PASS for the fix range. Contract: `docs/mission2-compose.md`.

## What changed

`mission2_route` is a direct call. The session does not link it. `docs/mission2-compose.md:48-51` does not establish which object calls the two list writers during play.

Observed and bound:

- Latch 0 on reset stores the one name from `game_getHubName`. A nonzero latch leaves the existing entries and does not call the resolver. Mission 2's completion byte is not an input. `docs/mission2-compose.md:16-24`. `tools/ref_mission2_compose.py:10-21`. `tools/difftest_mission2_compose.py:42-52` stores the difficulty bit into the campaign bytes; the client calls the existing `quests::hub_name`.
- A name that is already listed returns with the resolver uncalled. Otherwise the name is appended, then the first hub entry other than the resolver result is removed. A second copy of that other hub name stays. `docs/mission2-compose.md:28-40`. `tools/ref_mission2_compose.py:24-41`.
- Latch-clear reset followed by four synthetic ordinary names leaves the resolver hub and then those four names. `docs/mission2-compose.md:42-46`. `tools/ref_mission2_compose.py:53-58`.
- Portal gate 0 refuses before discovery and registers no travel task. Gate 1, with the current level already listed, writes row 0 through the existing jump-gate travel counter (181 accepting ticks, orientation 255). The pre-load stop then clears the pending name, makes the hub current, saves the level being left, stores orientation 254, and keeps the portal orientation. The loader is not entered. Player bytes (`4 * 0x4C30`) and the quest table (`0x2000`, including its first byte) come back unchanged. `docs/mission2-compose.md:54-70,92-96`. `tools/ref_mission2_travel.py:36-40,54-58`. `tools/difftest_mission2_compose.py:191-249`.

The synthetic test is `mission2-route`: 1019 checks. It uses authored names (`ord-a` and the same shape as the compose samples). No retail level names were added.

## Not copied

Mission-menu run-task admission, including a still-registered counter 0 and removal on the next negative step, is `codex/play-progression` round 3, code `d5889ae9`, report `c583578a`. This branch does not contain that code, and this route does not need it. The portal uses the existing jump-gate travel counter, not `missionMenu_run`.

The older `GateList::add_current` substring walk in level flow is unchanged.

## Refusals kept

| Call | Reason |
|---|---|
| Hub2 dialog choices | The dialog was not opened. `docs/mission2-compose.md:124-130`. |
| Helper `0x18ebb8` | The compared menu path stubs it. Its other effects are Unknown. `docs/mission2-compose.md:133-138`. |
| Physical NPC, trigger, and portal use | Unknown. `docs/mission2-compose.md:141-145`. |
| Loader body | The stop is before any loader instruction at `0x1e55c0`. `docs/mission2-compose.md:65-70`. |
| Arrival | Unknown. `docs/mission2-compose.md:72-74`. |

List save/load and a nonzero `save_actChanged` on this boundary stay unexecuted. `docs/mission2-compose.md:72-74`.

## Gate results

| Gate | Result |
|---|---|
| Release `cmake --build build/native -j 4` | PASS. Fresh configure, exit 0. |
| Focused `mission2-route` | PASS. 1019 checks, then 0.01 s inside full CTest. |
| Full `ctest --test-dir build/native --output-on-failure -j 3` | PASS. 260/260. Real time 818.66 s. No failures. The new test is #245. The previous wave was 259/259. |
| Three development GDExtensions, Release / `template_debug`, `-j 4` | PASS. Exit 0 for the archive extension, the level-texture extension, and the creature-animation extension. |
| Strict Godot matrix with captures | PASS. 54/54, 0 failed, including the six graphical cases. `ui_layout_smoke` passed. |
| FULL_SAVE_LOAD | PASS. 300/300. Both presentation hashes `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`. |
| FULL_TRACE | PASS, unchanged. 36,402 bytes. SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. Headless renderer. |
| Journeys | Headless 80 steps, 27 refused. Rendered 81 steps, 28 refused, 38 screenshots. Pickup passed, inventory 10 to 11, 52 walk ticks, 0 retry kills. Six refusal kinds on each. Godot exit 1 with 0 log errors. `player_death` refused and `saved_level_reloaded` false. Disc files 994 before and 994 after. |

Commands ran on the Mac mini in `~/malik-champions/open-champions/worktrees/play-mission2-route`, synced from this worktree without `.git`, `build/`, `work/`, or `.godot`. The mini Godot binary and the read-only disc were used. Standard input was closed for CTest and Godot. No baseline, fixture, or harness edit. No push and no pull request.

## Differences

The step name and result pairs match wave 37: 80/81, refused rows 27/28, pickup 10 to 11, 52 walk ticks, and the same six refusal kinds. `rendered_pickup_verified` remains false, as it was on wave 37.

Host timings moved under shared load. Another journey was running on the mini during this one. Headless `fps` is 92 and `enemy_fight` `swing_wall_msec` is 33599. Rendered `fps` is 2 and `swing_wall_msec` is 30371. Wave 37 recorded headless fps 134, headless swing 25024, and rendered swing 27173. Those fields are wall clock around the swing loop, not simulation results.

The first journey launch stopped before the first mark. The synced tree has no `.godot` import cache, so `NativeFxRenderer` was not in the global class cache and `scene.real` was nil. A headless editor import on the mini wrote that cache, which is not committed. The rerun above is the journey gate.

`template_release` extension binaries were not rebuilt. The dev matrix and the journeys load `template_debug`.
