# Play loading readiness, round 5

STATUS: DONE. Rounds 5 and 6 add Observed arrival stages to the synthetic prefixes. Live arrival stays refused. Every gate passed. The full CTest needed one serial rerun of three exact-vector tests that had timed out under mini load (details below). Journeys match wave 37 at 80/81 steps with pickup 10→11.

Research pin: `research-loading-readiness` `56a6760` (rounds 5 and 6; audits `research-loading-readiness-r5r6-fix.md` and `research-loading-readiness-r6-fix.md`, final verdict PASS). Client base: wave 37 VERIFIED `70d2de19`. Branch `codex/play-loading-r5`.

## Merge of round 4

`git merge --no-ff 5ab36040` gave merge commit `903dc4d7`. There was one conflict, in `native/src/campaign_fight.cpp`:

- Wave 37 had ported the player's starting items (`start.grant_starting_items` calls `pickups::give_starting_items`) and deleted the VF-1 `starting_equipment: refused_unknown` refusal.
- Round 4 had only reworded that refusal's comment.

I kept the wave-37 side, with no refusal. That matches `fight_starting_equipment = "runtime_record"`, which `session_controller.cpp` already had on HEAD. Nothing else conflicted, and I did not invent any code. The round-4 report's line about `refused_unknown` describes round 4's older base.

## What was implemented

Implementation commit: `2b06905e`. Citations are `56a6760` paths. `r5:` means `docs/loading-arrival-r5.md` and `r6:` means `docs/loading-arrival-r6.md`.

The new code is opt-in through `ArrivalControl::through_round = 6`. Round 4 stays the default, so its code path and its tests are byte-for-byte unchanged. Any other round value is refused with `arrival round was not audited`. All names, quest values and the slot-19 value are synthetic caller controls. Nothing is copied from a private script. The public tests use only `authored_*` fixtures.

**Route 0, empty name table**

- Object load `0x142320` finds no object (r5:45-56).
- It then runs, in order (r5:51-56):
  - append `0x144160`
  - player count `0x143534`
  - append `0x144160`
  - quest read `0x143500`
  - quest write `0x143540`
- 17 `removePlayerItem` `0x14598c` calls follow (r6:37-44). Each is player `-1` and runs one substring search per current player. Every search returns no hit, so no bag word is stored.
  - If the search reports a hit, the stage refuses at `0x1c1cb0`.
  - If the search service is unbound, the stage refuses.
- Next come three reads, `setPlayerFlag(0..3, 4, 0)` (which also clears the three records past the one live player), and two reads (r6:50-59). The address order matches `tools/difftest_arrival_stage_r6.py:44-45`.
- There is no post action.
- The loader's `0x322e6c` then makes one `0x1eda24` pass. It promotes the queue to front 2, back 0, parity 1 and runs none of the names (r6:63-69).
- The stage refuses at `0x355490` with `dbg_printf/warm-up` (r6:76-78, 181-185). The 48 `0x145690` tests are not run, because they belong to a later pass.

**Route 0 with an AutoProp**

- Slot 18 runs only for object flag `0x10000000` with vtable `0x5133c0` (r5:45-49).
- `0x23d680` enters only when the signed compare `0 < States` holds (r5:67-74). With States 0 or -1 it stores nothing, and the entry stops at `0x14598c` with back count 2 (r5:58-61).
- With States 100 it stores zeros at `+0x178`, `+0x17c` and the 8-byte `+0x180` (r5:61-65). Then one of these happens:
  - **Null model:** refuse at `0x179550` (`fault at load address 0x10`). Nothing is enqueued (r6:101-103).
  - **Flag-clear header:** store the show mask at `+0xc8`. The zero clip at `+0x128` is not stored and `anim_push` is skipped. Slot 19 `0x23d7ac` runs to a return. Then the appends run and the entry stops at `0x14598c` with back count 2 (r6:103-111).

**Hide-mask rule**

- `arrival_hide_mask` returns the complement of the double at `+8` (r6:82-98).
- Only when flag bit `0x2` is set does it walk the 16-byte `{show, hide}` records at `+0x21 << 4`.
- A null model faults.
- The tests check that flags 0 and 1 give `0x…fa` and that flag 2 gives `0x…f9`, not the decoy `0x…f6`.

**Route 1**

- `lump_load` `0x173558` and lookup `0x17306c` run (r5:97-108).
- If no record is resident, or flag bit 0 is set, the stage refuses at `cd_findFile` `0x1724fc` before the disc lookup.
- On a hit, the entry runs the quest reads, `showMonster` and three `hideMonster` calls, in the order of `tools/difftest_arrival_stage_r5.py:35-37`.
- With one creature object per name (r6:115-125):
  - `showMonster` calls `obj_updateGridCell` and clears flag `0x8000` and `+0x231` bit 3.
  - Each `hideMonster` calls `obj_moveOffGrid` and sets both.
- The entry returns with the queue at 0. The stage stops at `0x322e6c`, because the loader call after route 1 was not executed.

**Route 2**

- The round-4 prefix runs first. Then `cut_start` runs with these fixture inputs: offline, no player object, level name from `0x41b721`, one resident `<level>.lmp` record whose single entry is `<cut>.cut` with header word 0, and a camera at `0x4f8450`.
- It applies the r5:136-151 finals. Camera `+0x34` is set to 0. Camera `+0x38`, `+0x3c` and `+0x98` are not touched, which the tests check with sentinels.
- The post action then writes the quest value, publishes the replacement and returns.
- The replacement's `deleteMonster` `0x144b44` (r6:127-136):
  - returns on an empty name table;
  - with one creature object whose property list is absent, records `GameObject_dtor` `0x14cf90`, stores vtable `0x5122a8` at `+0x48`, and lists three `0x1a1698` calls. It frees nothing, the name count is unchanged and the object can still be found.
- After that the stage stops at `0x199944` with `cut_runFrame tasks`, because the two registered tasks are not run (r5:156).
- Alternatively, with `first_frame` 0 or 1, the first `cut_runFrame` runs instead of the replacement (r6:144-149). It stores 0 at `0x4039ac` (planted `0x11111111`) and does not store setup byte `0x44fcd2`. The stage then refuses at `0x14f498` with `cut_setup`.

## Refusals kept

Each one has a precise reason and boundary address.

| Case | Reason | Boundary |
|---|---|---|
| Live route selection | `route state` (ordinal outside 0..2). The live loader applies no prefix. | — |
| Loader after the promote | `dbg_printf/warm-up` | `0x355490` |
| Nonzero clip | `anim_push with a nonzero clip` | `0x170838` |
| Slot 19 nonzero value | `slot 19 with a nonzero value` | `0x23d7ac` |
| Delete with a property list (`+0x2c` or a host list) | `delete with a property list` | `0x3b91d8` |
| `cut_setup` onward | `cut_setup` | `0x14f498` |
| The two registered cut tasks | `cut_runFrame tasks` | `0x199944` |

Other refusals, each for something that was not executed:

- A prop header with flag `0x2`, because setState's variant argument is not given: `0x179550`.
- An inventory hit: `0x1c1cb0`.
- More than one object of a name.
- A non-creature show, hide or delete target.
- A lump table with more than one record.
- A nonzero or missing cut header, or a missing resource: `0x19b4dc` / `0x17365c`.
- A network session or a player object.
- `cut_runFrame` arguments other than 0 and 1.
- Rounds other than 4 and 6.

The live `loading_readiness(..., true)` keeps round 4's five names. It also appends the seven round-6 Unknowns (r6:181-185): live route selection, dbg_printf/warm-up, nonzero clip, slot 19 nonzero, delete with a property list, `cut_setup` onward, and the level-name bytes the loader stores. The readiness booleans are unchanged. `blood1_arrival` stays `refused`.

**Not modeled.** The research names `0x1a1698` and `0x23d7ac` but does not record their stores. They are listed in `ArrivalPrefix::callees`, but their effects are not applied.

## Gates

Everything ran on the Mac mini in `~/malik-champions/open-champions/worktrees/play-loading-r5`, an rsynced copy that excludes `.git`, `build`, `work` and `.godot`. Builds used `-j 4` and CTest used `-j 3`, with standard input closed. Logs are in the ignored folder `work/play-loading-r5/`, with the mini home path replaced by `~/`. The mini was heavily shared during these runs, with load averages up to about 86.

| Gate | Result |
|---|---|
| Native Release configure and build | **PASS**, exit 0, 4 s + 596 s (`configure.log`, `build.log`). No warning in the changed files. |
| GDExtensions | **PASS**, each exit 0. Archive `template_debug` 634 s and `template_release` 555 s; level texture 117 s / 112 s; creature animation `template_debug` 414 s. `lipo -archs` reports `x86_64 arm64` for all five libraries (`build-status.txt`). |
| Focused `ctest -R ^script-commands` | **PASS**, 3/3 (`script-commands`, `-measure-usage`, `-measure-empty`). `script-commands ok`. |
| Full CTest `-j 3` | 259 registered (the same count as wave 37). First run: **256/259**, 3,120 s. Three tests failed, each on the test script's own subprocess timeout while the mini's load average was about 86–133: `messages-exact` (120 s limit in `tools/test_messages_vectors.py`), `player-exact` (180 s in `tools/test_player_vectors.py`) and `session-facing-exact` (600 s in `tools/test_session_facing_vectors.py`). None of them touches loading code. Serial rerun of exactly those three: **3/3 PASS**, 104 s, 133 s and 297 s (`ctest-rerun3.out`). That makes 259/259. No test or timeout was changed. |
| Strict Godot matrix: `tools/godot_matrix.py matrix --captures --timeout 1200` | **54/54 PASS**, exit 0, 397 s. Fresh isolated projects. The unchanged `check-log` found 0 diagnostic errors in all 105 matrix logs. Matches wave 37's 54/54 case for case. See the first-run note below. |
| FULL_SAVE_LOAD | **PASS**, 300/300. Continued and uninterrupted presentation SHA-256 are identical: `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`. |
| FULL_TRACE | **PASS, unchanged**: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. Synthetic combat: 2,645 checks, 0 failures, the same as wave 37. |

**First matrix run (not the gate).** The first run went 29/54. All 25 failures had the same error: `res://.godot/imported/open_champions_avatar.png-….ctex` was missing. My one headless import (`--editor --quit-after 120`, 44 s) ended before the scan had finished. Under a mini load average of about 86, the log reads `WARNING: Scan thread aborted`, and the matrix's own `editor` case was cut short the same way. I then ran `Godot --headless --path godot --import` (exit 0, 30 s, no errors). It imported the project's only `.import` asset, the avatar PNG. After that I reran the whole matrix with the same command, and it passed 54/54. I changed no tool, test or case, and filtered nothing. The first run's logs are kept as `work/play-loading-r5/matrix-run1-incomplete-import/`.

**Versus round 4's 58/61.** Round 4 ran on its own older base `d07f7e4e`, which predates `tools/godot_matrix.py`. It used a copied matrix driver (`work/loading-r2-tools/godot_matrix.py`) with a different, 61-case list. That list included `loading_disc_cases`, which exits 2 without a game folder. Its other two failures were `ui_layout_smoke`'s old four-argument `hud_text_glyphs` call and `native_fx_visual`'s relative capture path. Neither remains on wave 37's tree: `ui_layout_smoke` and `native_fx_visual` both pass here. So this branch should be compared with wave 37's 54-case list, and against that list it matches exactly.
| Journeys: unchanged `tools/run_loop_playthrough.py`, headless then rendered | Headless **80** steps, rendered **81**. Both exit 1, the retained full-journey refusal, as in wave 37. 0 log errors. Six refusal kinds each, the same six as wave 37. Refused rows **27 / 28**. Pickup **10→11**, drop produced, walked and picked up, 52 walk ticks, 0 diagnostic retries. Rendered captures **38**. `player_death` stays refused with `saved_level_reloaded` false. Disc files **994 before / 994 after**. 303 s total. |

**Journey differences versus wave 37.**

- **Simulation results:** none. Step counts, refused-row counts, refusal kinds, pickup, walk ticks, captures, the death refusal and the disc count all equal wave 37's recorded 80/81, 27/28, six kinds, 10→11, 52, 38 and 994.
- **Host-only:** loot-pickup `fps` is 58 headless and 2 rendered. That measures the mini's load at the time, not the simulation.
- **Not compared index by index:** wave 37's per-index step artifacts are only in the playable-integration worktree, which I was told not to touch.
- **Why these changes do not show:** `blood1_arrival` is `refused` with an empty `loading_refusal`, as before. The new readiness limit names live in `loading_readiness().refusals`, and that list never reaches the journey: `real_data_play.gd:736-757` reads only the booleans and a fixed error string. The round-5 stages are opt-in synthetic APIs, and the live loader applies no prefix.

## Files

- `native/include/champions/loading_equipment.hpp`: `ArrivalControl` round-6 controls, `ArrivalPrefix::callees` / `loader_promoted`, `arrival_hide_mask`.
- `native/src/loading_equipment.cpp`: `arrival_hide_mask` and `run_arrival_r6`. Round 4's `run_arrival_prefix` body is unchanged.
- `native/src/script_commands.cpp` and `native/include/champions/script_commands.hpp`: the seven round-6 limit names on the live readiness path.
- `gdextension/src/session_controller.cpp`: comment only.
- `native/tests/script_commands_tests.cpp`: `test_loading_r6_hide_mask` and `test_loading_r6_arrival`, with synthetic fixtures only. `test_loading_r4_*` is unchanged.

No test, baseline, fixture or tool was weakened or edited. Nothing was pushed and no PR was opened. I did not touch `~/champions-re` or playable-integration. No process was killed by name. The private disc was read only. The mini's `build/` was deleted after the gates.
