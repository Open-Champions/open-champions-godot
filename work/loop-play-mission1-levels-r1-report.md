# Mission 1 levels, client round 1

STATUS: PASS with one recorded journey difference. The Observed mission 1 level rules are ported or confirmed with synthetic cases: cutscene actor binding, `.cut` keys and the slot-3 door order, record facts and drops, the 20 death cases, the strife/drundar travel sites, loader names, the 6 marker cases and the 4 hub cases. The Unknowns stay refused with cited reasons. CTest 260/260 (one load timeout rerun alone), strict Godot 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged. Journeys: every step name and result equals wave 38 in both variants. The rendered run lacks the two hub-action refusal kinds (see Differences).

Branch `codex/play-mission1-levels` on wave 38 VERIFIED `16b66176`. Code `b592712b` and `735c9d90`; this report is the next commit.

Research pin: `t3/research-strife-drundar` `de4aafe`, `docs/mission1-levels.md` (cited as doc:LINE). Audits `research-mission1-levels.md`, `-fix.md`, `-fix2.md` (PASS, 5 LOW). The LOWs are kept in mind: F2-2 adds constructor/init-script objects to the uncounted search objects, and F2-3 means the search constants are Observed listing facts.

## What the client already did

| Area | Before this round | This round |
|---|---|---|
| Cutscene actor binding | None. `cutScene` hands the file to the scene's `cut_start` (`script_builtins.cpp`), and the Mission 1 contract hard-refuses `cutScene`. | New `mission1_levels` module (below). Not called by the session yet. |
| Death cases | `Creature_onDeath` 0x10beb8 is ported as `arena_monsters::Model::slot28_on_death`: death script with the creature selected, then `dropitem` unless quiet. | Confirmed on the ten research record shapes, quiet 0 and 1. No code change. |
| Record facts and drops | The factory census for strife/drundar only (`Contract::validate_objects`). | `record_facts`, `named_value`, `names_entry`. |
| Travel sites | `newWorld` writes the pending level 0x41b6d0. The Mission 1 contract admitted no `newWorld` site. | The contract reads its six strife/drundar sites; four are admitted, the two demo-branch sites are refused. |
| Loader names | `preStart`, objects, `start` (`session_level_flow.cpp`); `find_public` is a case-sensitive binary search. | Confirmed, including the `prestart` spelling never running. |
| Markers, hubs | `level_flow::run_markers`, `quests::Campaign::hub`. | Confirmed on the 6 marker and 4 hub cases. |

## Ported (Observed)

- **`.cut` layout.** Header flags, key and actor tables; 0x2c-byte actors (class +8, position +0x1c, create byte +0x2b); 20-byte keys (time, actor, type). doc:338-343; offsets of the tables and key fields from `tools/difftest_mission1_levels.py:1024-1041`. Bounds-checked, with host limits of 4096 keys and 256 actors.
- **Actor search** (`cut_setup` 0x19b098-0x19b324). Box ±240 x/y, ±100000 z, each bound in f32. Capacity 0x40: only the first 63 results in walk order are kept. Candidates match by factory class string or by C++ type name with leading digits skipped. Objects with flag 0x80000 are skipped. The nearest in 2-D strictly below 600 is taken, and a held class-string match makes later type-only matches ineligible. Created actors are not searched. doc:345-379.
- **Door key order.** On a bound actor, type-3 then type-4 keys call slot 3 with 1 then 0, in key order (doc:313-317, 341-343, 380-382). Keys out of ascending time are refused, because only ascending order was observed. Show keys (type 8) are listed per actor (doc:384-394).
- **Record facts.** Drops per `dropitem` value, property-less records per class, and key sets per class (doc:169-184, 223-228, 271-277). A unique-name lookup. The `<empty>` placeholder names no entry (doc:178), and running it as an entry is `Missing`.
- **Travel sites.** The contract validates `newWorld` at strife 0x490, 0x1ae8, 0x1b94 and drundar 0x1a18, 0x1ac4, 0x1c20, reading destinations from the image and checking their owning entries (`tools/ref_mission1_levels.py:227-237`, doc:404-428). `Host::mission1_travel` runs an admitted site through the existing `newWorld` binder. The GDExtension exposes `travel_sites`, `travel_demo_refused` and a `travel` action. drunarm/drunare sites are not in a client contract. The drunare `levelChange` → `hub2` was already in `ArenaScript`.

## Refused (Unknown)

Each reason is a string in `mission1_levels::open_reason`:

| Unknown | Reason cited |
|---|---|
| Run-time spawns, moved creatures and constructor/init-script objects in the capacity count | doc:376-379, 543-545; audit F2-2. A binding whose kept 63 contain one is refused. One beyond the 63 takes no slot and is not refused. |
| Door state after `doors`, `cazic`, `bossdefeated` | doc:318-321, 547-548. The slot-3 calls are given; the resulting state is not. |
| Whether a show key clears `hideMonster`'s 0x8000 / bit 3 (knights, avatar) | doc:296-303, 546-547 |
| Cutscene playback, length, camera | doc:400-402, 522 |
| Unrequested cutscene file | doc:396-399, 549-550 |
| `exitzone`'s demoRunning branch | doc:425-428, 530. Recorded, kept out of the admitted sites, and refused by `mission1_travel`. |
| Property-less creatures live from load | doc:202-205, 500. Inferred only. |

Unchanged: strife `preStart` still refuses attachment, so strife travel cannot run on the disc. Bridges, `autoPropSetState`, `cutScene` and `callScript` stay hard-refused.

## Tests (synthetic, added to existing files)

`mission1-scripts` (`native/tests/mission1_scripts_tests.cpp`): 262 checks, 0 failures. There were 43 `check(` call sites before and 92 now. All names are authored; no disc text key, record or level name was added. The new checks cover:

- travel: three sites per level, one demo branch refused with no request, and the pending level written for the other two;
- `.cut` parsing and truncation;
- slot-3 order 1 then 0, plus the out-of-order refusal;
- the box;
- type-name skipping;
- nearest/bound/600/type-precedence rules;
- the 63/64 capacity edge;
- refusals for each uncounted origin;
- the 20 death cases;
- record facts;
- loader case sensitivity;
- the 6 marker cases (rank, last taker, 4 or 8 searches);
- the 4 hub cases.

One check failed during development: the hub case. The cause was the fixture's `Campaign` having zero players. Setting record 0 fixed it, with no client change.

The private probe `godot/tests/mission1_real.gd` (counts only, one added check) returned **2 levels, 49 checks, 0 failures**. That is 47 before, plus the travel count on both levels. The new contract reads pass on the real strife and drundar images.

## Gate results (Mac mini, `~/malik-champions/open-champions/worktrees/play-mission1-levels`)

| Gate | Result |
|---|---|
| Release configure + `cmake --build build/native -j 4` | PASS. Exit 0, 410 s; incremental after the record-facts edit, 35 s. |
| GDExtensions, `template_debug`, `--parallel 4` | PASS. Exit 0, 810 s. Archive, level-texture and creature-animation are each `x86_64 arm64`. `template_release` was not rebuilt; the matrix and journeys load `template_debug`. |
| Focused CTest | PASS, 6/6: `mission1-scripts`, `script-commands`, `level-objects`, `level-flow-exact`, `arena-monsters-exact`, `quests`. |
| Full `ctest --test-dir build/native --output-on-failure -j 3` | 259/260 in 1362 s under load average ~90. `messages-exact` failed only because its inner 120 s subprocess timeout expired. Rerun on its own: **Passed, 6.90 s**. So 260/260 with that one rerun. The test count is unchanged from wave 38. |
| Strict Godot matrix with captures | PASS. 54/54, 200 s. `check-log`: 105 logs, 0 failing. |
| Combat | 2,645 checks, 0 failures. |
| FULL_SAVE_LOAD | 300/300. Both presentation hashes `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`. |
| FULL_TRACE | Unchanged. 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. |
| Journeys | Headless 80 steps, rendered 81, 38 captures. Pickup 10→11, 52 walk ticks, 0 retries. Godot exit 1 at the retained `player_death` refusal, 0 log errors. Disc 994 before and 994 after. Run twice. |

## Differences from wave 38

Compared step by step with wave 38's retained `playthrough/*/result.json`, ignoring the host fields `fps` and `memory_bytes`:

- Both variants, both runs: every step name/result pair is equal. Every non-host step field is equal. Refused rows are 27 headless and 28 rendered, as in wave 38. `blood1_destination` and `player_death` are refused as before.
- Headless refusal tally: equal (8 kinds).
- Rendered refusal tally: 6 kinds against 8, in both runs. The missing two are the hub-action kinds from `script_dialog.cpp:320`: `dialog: confirm: … count=3` and `tick: dialog confirm: …`. Rendered recorded no such refusal this time. That is the post-grant node-3 close-fade retry with zero retries. Progression r3 recorded counts of 1, 2, 3 and 4 for the same path and tied them to rendered frame timing (`work/loop-play-progression-r3-report.md:34`). The hub dialog step fields (choices, grant, first story available) are equal to wave 38. This round changed no dialog or hub code. The contract changes apply only to scripts matching the strife/drundar census, which the hub is not. I am attributing this to timing, but I did not prove it: no run with an older build was made on this mini load.

## Notes

The session does not call `mission1_levels` yet: it is a library with tests, like `mission2_route` in its first round. No baseline, fixture or harness was edited, and no mini home path is committed. Scratch evidence is under ignored `work/m1/` on the mini and the MacBook, including both journey runs and the wave 38 copy used for comparison. No push and no PR. The mini's `build/` was deleted after the gates.
