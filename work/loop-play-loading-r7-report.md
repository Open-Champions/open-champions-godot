# Play loading readiness, round 7

STATUS: DONE. The arrival stage moves from the r5/r6 pins to round 7 at `f8c780b` through the opt-in `through_round = 7`. The delete frees, the request selects its route, setState writes the clip entry, and the loader runs to `loadscreen_stop`. Every refusal the research keeps stays refused. Every gate passed. CTest was 259/259 in one run, with no rerun. The matrix was 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, and FULL_TRACE was unchanged. Live arrival stays refused, and the live loader applies no prefix.

Research pin: `research-loading-readiness` `f8c780b34e772d71ed9a3b9fd2350226739dc6ca` (`docs/loading-arrival-r7.md` with its Round-8 and Round-9 fixes sections, and `tools/difftest_arrival_stage_r7.py`, `_r8.py`, `_r9.py`). Audits: `research-loading-readiness-r7.md`, `-r7-fix.md`, and `-r7-fix2.md`, which is PASS with LOWs R10-1 to R10-3 and the carried R8-10. Client base: the PASS tip of `codex/play-loading-r5`, `9cdead61` (audit `play-loading-r5-fix.md`, PASS, with one follow-up item). Branch `codex/play-loading-r7`.

Citations below are `r7:<line>`, meaning `f8c780b:docs/loading-arrival-r7.md`. Difftest functions are named where the note defers a layout to them.

## What was implemented

Implementation commit: `3b77dc8b`. The new code is opt-in through `ArrivalControl::through_round = 7`. Rounds 4 and 6 keep their code paths. The existing tests for them are unchanged and still pass. Round 6 stays as its own pin. Its comment now says round 7 withdraws its "does not free" claim. Any other round is refused with `arrival round was not audited`. Every name and value in the tests is a synthetic `authored_*` control. The level names (`hub`, `hub2`, `blood1`, `strife`, `intro`) are names the client code already uses (`frontend.cpp`, `level_flow.cpp`, `script_commands_objects.cpp`). They are not disc text, and no cut data is used.

### Delete with frees (the follow-up from `play-loading-r5-fix`)

- **No property list** (r7:355-371; `difftest_arrival_stage_r8.py` `free_head`). After the round-6 checks (creature flag, GameObject table `0x5122a8`), the delete stores the table word, lists `0x14cf90` and the three `handle_clear` calls (still unmodeled, r6:132), then `pool_free` `0x150470`. It applies exactly the two recorded stores:
  - `0x1504bc`: the block goes into the head word `0x403a1c` + 16 × bucket;
  - `0x1504c4`: the previous head goes into the block's first word.
  The name table is not touched, so the name still finds the freed object (r7:373).
- **Bucket.** Only bucket 1 was executed, with a non-zero previous head, for a 0x250-byte GameObject. The client accepts only addresses in the class-1 range of its pool layout (`core_state.hpp` `Pool`: the 0x500-byte class, `0x6288c0`–`0x6c88c0`, on block boundaries). Only the address layout is shared with the client's pool; the allocator itself is not told of the free (fix round, PLR7-3, below). Anything else is refused at `0x150470`, before any dtor store: `pool_free of a block outside pool class 1 was not executed`. An empty bucket-1 list is refused the same way: `pool_free onto an empty free list was not executed`.
- **Property list** (r7:375-384). The executed list is `kind=1`, then one `name=` entry whose value names this object (fix round, PLR7-1: any other first entry is refused). That list is accepted in either of two forms:
  - the research layout, `+0x2c` → a null-terminated array of string pointers;
  - the host `props` list, which the client's object load already registers by `name=`.

  The delete lists six `strncmp` `0x3b91d8` calls and `objNames_remove` `0x14be58`. It removes the value through the client's name-table model, so the count falls by one and the name then finds nothing. The three handle clears and the same two free-list stores follow. Any other list shape is refused at `0x3b91d8` before any store. That covers one entry, three entries, a `name=` entry first, a value naming another object, and memory and host lists together. Several `name=` entries stay Unknown (r7:386-387).

### Route selection (r7:131-197, 492-500)

New public functions in `loading_equipment`:

- `new_game_destination` stores `hub` at `0x4fdc30`. It copies it to the request `0x41b6d0` and stores orientation `0xff` at `0x41b720`. It replaces the request with `hub2` only when three bits are all set at the current difficulty `0x4f719c`:
  - decision `+0x4c1b`, bit `difficulty`;
  - choice `+0x4c1a`, bit `difficulty`;
  - mission-1 completion bit `0x2` of `+0x4bbf` + 13 × difficulty + 1.

  The bit layout is from `difftest_arrival_stage_r7.py` `producer` A2. Difficulties outside 1–5 are refused. So is a set select flag `0x4f70a0` or PvP mode B `0x4f7120` (r7:137-139).
- `mission_one_start` reads mission row 0 `+8` of `0x4fe540`. It refuses a non-null evil alternative `+0x0c` (r7:181-184). It does not write the request, because the menu copy is not executed (r7:193-197).
- `boot_request(offline = true)` writes the front end's `intro` (r7:169-172, 187-188). Online is refused.
- `split_request` copies the request into `g_levelName` `0x41b721` and `0x44fe18`, clears its first byte, and returns the route (r7:160-163, 177-185): `hub` 0, `strife` 1, `blood1` 2. Every other name is refused with no store, including `hub2` (whose ordinal was not executed) and `intro`.

### setState with a clip, and slot 19 (r7:270-351)

At state 0 (r5:61-65), on the AutoProp path after the hide mask:

- **Mode and capacity.** The mode is 3 when `+0x12c` bit `0x2` is set, and 6 otherwise. Only capacity bytes `+0x9d` 0 and 4 were executed. Others are refused at `0x170838`.
- **Clip length.** Every executed push used clip `+4` = 5. `0x170bc8` stores 5.0 at entry `+0x18`, and the research does not record where that value comes from. A push with another clip length is refused at `0x170bc8`, rather than given a constant.
- **Depth clear.** The depth byte `+0x9c` is cleared before the push. **R10-2 is respected.** This clear is cited as Observed static only (`sb zero` at `0x23d6f8`, checked by field), and its effect is inferred from the executed second push (r7:340-344). The code comment says so. Nothing in the client calls it Executed.
- **Capacity 4.** All 32 bytes of the entry at `+0xa0` are written, each as credited in round 8 (r7:311-325):
  - `+0` clip (`0x170878`);
  - `+4` link: when it is non-zero, 0 at the link's `+0x14` (`0x17088c`) and 0 at `+4` (`0x170890`);
  - `+8` 1.0 (`0x1708a8`), `+0xc` 0 (`0x1708a4`), `+0x10` 1.0 (`0x1708ac`), `+0x14` 0 (`0x1708b0`);
  - `+0x18`: 0 (`0x170880`), then 5.0 (`0x170c1c`);
  - `+0x1c` mode (`0x1708a0`), `+0x1e` 0 (`0x170888`).

  The depth becomes 1. `+0x9e` becomes the old halfword **OR** the mode (`0x17087c`, r7:326-339), so a second push ORs in a second mode.
- **Capacity 0.** Nothing is pushed and no entry byte is stored. That result ran only with clip `+4` = 5, so since the fix round (PLR7-2) any other length at capacity 0 is refused at `0x170838`. Its `+0x9e` stores were not checked (R10-2), so none are applied and `0x170838` is listed in `ArrivalPrefix::unmodeled`.
- **`+0x17c`** is **set** to clip `+4` + 1 (`0x23d764`, r7:279-299), never added to an older value.
- **Slot 19.** The value is `+0x12c` bit 0 (r7:275-276). Round 7 refuses the old `slot19_value` control. With value 1 and `+0x11c` bit 0 clear, `+0xc` bit 0 is set (`0x23d878`, r7:345-346). The entry then stops before `0x14598c` as in round 6.
- **Slot 19 = 1 with `+0x11c` bit 0 is refused.** With the world word `0x44fdc8` = 0, the refusal is at `0x182b08` (`slot 19 collision: 0x182b00 faults on a null world 0x44fdc8`), before the `+0xc` store, as executed (r7:347-349). With a non-zero world word, the refusal is at `0x182b00`, because a real collision world is Unknown (r7:351).

  The brief said "Slot 19 = 1 … refuse it". I read that as this collision case, and kept the executed value-1 case without the collision bit (r7:345-346, 510-511) as a port.
- **Clip and value 1 together.** These ran only separately in the research, so a clip with value 1 is refused at `0x170838`.

### Loader stops (r7:199-263)

Route 0 with an empty name table no longer stops at `dbg_printf/warm-up`. In order:

1. `mainloop_startWarmup` stores 10 at `g_warmupFrames` `0x506048`.
2. `dbg_printf`, the one promote (front 2, back 0, parity 1, no native run), `dbg_printf`, and the stack check `0x19d9f8`.
3. Ten passes of the six calls, in order (`0x1e72f8`, `0x1ebc18`, `0x172ca8`, `0x17d830`, `0x17fce0`, `0x17fe88`). Each pass stores the counter one lower, 9 down to 0 (the `0x322f30` stores). The six calls record no stores, so they are listed in `unmodeled`.
4. The second stack check, `dbg_printf`, and the shop test.

The two stops, both kept refused with the current reasons:

- **Empty level name.** `dbg_printf`, then `loadscreen_stop` `0x3224d8` stores 1 at `0x5060a0`. The stage refuses at `0x3528e4` with `loadscreen_stop: ReferThreadStatus on an unstarted thread`. The thread is the one `game_loadLevel` starts, which this path has not run (r7:265-267, 504-505).
- **Level `hub`.** `shop_openOrStock` `0x1c7af8` → `lump_load` `0x173558`, then the stage refuses before the disc lookup at `0x1724fc` with `cd_findFile`, as route 1 does. `0x5060a0` is not stored.

The other refusals and limits on this path:

- Any other level name is refused at the shop test `0x323074`.
- A camera object or an active cut byte is refused before the warm-up starts, because `Camera_update` and `cut_runFrame` were not executed in the loop.
- The timer-0 store at `0x322f04` is a hardware register in the research's fixture map (r7:44-46). It is not applied to client memory.
- Streaming idle (`0x17fe88` returning 0) is the research fixture's result. The client has no streaming model to test it against.

### Unchanged and still refused

`game_loadLevel` and its loading-screen thread, `ReferThreadStatus`, the shop's disc lookup, `0x182b00` with a collision world, the cut's `.world`/`.ob`/`.scr`, what the actors draw, and the mission-menu copy (r7:519-529). The first `cut_runFrame` still refuses at `cut_setup` `0x14f498`. Round 7 runs the cut further only with the real level lump, which is not ported. Route 1 still stops at `0x322e6c`. The live `loading_readiness(..., true)` keeps every round-4 and round-6 name. It adds six round-7 Unknowns, and its booleans are unchanged. The live loader applies no prefix.

### Stale wording

`work/loop-play-loading-r5-report.md` no longer calls round 7 "`4487a88`, still under audit". The three places now carry a dated note that round 7 passed at `f8c780b` and is ported here. The code comment at the delete now cites `f8c780b` (r7:355-360, 512-513).

## Tests

`native/tests/script_commands_tests.cpp` adds `test_loading_r7_request` and `test_loading_r7_arrival`, plus two `#include` lines. No existing line is changed or removed (`git diff 9cdead61 -- native/tests` gives only `+` lines). No tool, vector, baseline or Godot test is touched.

**Mutations** (`work/plr7/mutations.txt`). Each is applied to `loading_equipment.cpp`, rebuilt, run, then restored with `git checkout`. After restoring, `git diff` is empty and the test passes.

| Mutation | Checks that fail |
|---|---|
| M1: no free-list stores | 3 |
| M2: `+0x9e` = mode, not OR | 3 |
| M3b: no `+1` | 4 |
| M4: no name removal | 2 |
| M5: `hub2` on any bit | 2 |
| M6: no countdown | 2 |
| M7: no `+0xc` bit | 1 |
| M3c: no entry zero of `+0x17c` | 1 (the round-6 check) |

**M3 survives** (`+0x17c` added instead of set). It is an equivalent mutant: the round-5 entry store has just zeroed `+0x17c` (r5:61-65, r7:279-280), so adding to it gives the same value. That is the research's own reading: "the sum's base is the word itself, which `setState` has just zeroed" (r7:297-298).

## Gates

Everything ran on the Mac mini in `~/malik-champions/open-champions/worktrees/play-loading-r7`. That copy was seeded by copying the PASS `play-loading-r5` copy (a standalone repo with `alternates` into `repo.git`), leaving out `build/` and `.godot`. Branch `codex/play-loading-r7` starts at `9cdead61`. The seven changed files were then synced with `rsync -rlp --checksum` and touched. Their sha256 sums match the MacBook's. There was one `build/` root. Builds used `-j 4` and CTest `-j 3`, with standard input closed. Before the build, `df -h ~/malik-champions` showed 39 GiB free. Logs are in the ignored `work/plr7/`, with the home path replaced by `~/`. Every step logs `uptime` before and after it (`load.txt`, the status files). The gate scripts are `work/plr7/gates-*.sh`, adapted from round 5's.

| Gate | Result | Load before → after |
|---|---|---|
| Native Release configure and build, `-j 4`, clean `build/native` | **PASS**, exit 0, 1 s + 114 s. 0 warnings in the changed files (10 in the whole build, all in other files, as in round 5) | 11.9 → 20.5 |
| GDExtensions | **PASS**, each exit 0. Archive `template_debug` 110 s, `template_release` 100 s; level texture 21 s / 20 s; creature animation `template_debug` 80 s. `lipo -archs` gives `x86_64 arm64` for all five libraries, each rebuilt at 07:17–07:20 that day (`ext-status.txt`) | 39.9 → 73.3 |
| Focused `ctest -R ^script-commands` | **PASS**, 3/3 (`script-commands`, `-measure-usage`, `-measure-empty`), 1 s (`focused.log`). The first focused run, before the test fix below, failed one check | 20.5 → 20.5 |
| Full CTest `-j 3` | **PASS, 259/259** (259 registered, as in round 5), exit 0, 954 s, one run. No test timed out, so no test was rerun. Round 5's slow trio passed inside the parallel run: `messages-exact` 7 s, `player-exact` 62 s, `session-facing-exact` 196 s (`ctest.log`, `ctest-status.txt`) | 64.5 → 39.1 |

| Strict Godot matrix: `tools/godot_matrix.py matrix --captures --timeout 1200`, after `Godot --headless --path godot --import` (4 s, exit 0) | **54/54 PASS**, exit 0, 156 s. The unchanged `check-log` finds 0 diagnostic errors in all 105 matrix logs. Same case list as round 5 | 33.8 → 17.5 |
| Combat (`playable_wave_smoke_full_combat`) | **2,645 checks, 0 failures** | (in matrix) |
| FULL_SAVE_LOAD | **PASS, 300/300**. Continued and uninterrupted presentation SHA-256 are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, as in round 5 | (in matrix) |
| FULL_TRACE | **PASS, unchanged**: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` | (in matrix) |
| Journeys: unchanged `tools/run_loop_playthrough.py`, headless then rendered | Headless **80** steps, rendered **81**. Both exit 1, the retained full-journey refusal, as in round 5. 0 log errors. 6 refusal kinds each, and the refusal lists are identical to round 5's. Pickup **10→11**, drop produced, walked and picked up, 52 walk ticks, 0 diagnostic retries. Rendered captures **38**. Disc files **994 before / 994 after** (also 994 after the rerun and the control). 110 s | 16.8 → 15.1 |
| Journeys, rerun (same build, same command, `playthrough-rerun/`) | The same counts. Identical to the first run at every step except `swing_wall_msec`. 123 s | 16.4 → 40.6 |

**The one failing first run.** `a host property list removes the name the same way` failed once. The test had made that object with the name both as a `new_object` name and as a `name=` host prop. `new_object` registers `name=` props itself (`script_builtins.cpp:327-329`), so the name was in the table twice, and the delete correctly refused it as naming two objects. The fix was to the test only: the object now gets its name from its prop alone, as loaded objects do. The code was not changed. That fix is in `3b77dc8b`.

**Journeys, step by step against round 5.** Round 5's own `compare_journey.py` compares every key of every step, the step order, the refusal list and the capture count. It ignores only the host-only `fps` and `memory_bytes`, and prints indices and key names only. The round-5 results are the mini's `play-loading-r5/work/plr5-fix/` `baseline-r5` (round 5), `playthrough` (fix round, first run) and `playthrough-rerun` (fix round, rerun). Outputs are `journey-compare.txt`, `journey-compare-rerun.txt`, `journey-compare-control.txt` and `journey-choices.txt`.

- **Against every round-5 run:** the step counts (80/81), step order, refusal lists and captures (0/38) are identical. The only differing steps are these three:
  - step 18 `choices_taken`;
  - step 27 `script_executed_commands`;
  - headless step 65 / rendered step 66 `swing_wall_msec`, a wall-clock time of the enemy-fight swing.

  Against the round-5 fix's first run, headless differs only at step 65.
- **Round 7 first run against its own rerun:** identical except `swing_wall_msec`.
- **The choice count, step 18 / 27.** The journey driver presses Enter every 8 process frames until the dialog closes (`godot/tests/loop_playthrough.gd:784-792`), so the number of choices taken depends on frame timing. Step 27 follows it exactly: in all nine runs, `addQuest` = choices − 2 and `setMissionAvailable` = choices − 1.

  | Run | headless choices | rendered choices |
  |---|---|---|
  | round 5 baseline | 4 | 3 |
  | round 5 fix, first | 5 | 3 |
  | round 5 fix, rerun | 4 | 3 |
  | round 7 | 5 | 6 |
  | round 7, rerun | 5 | 6 |
  | **control: round 5's own build, run now** | **5** | **4** |

  The rendered 6 is new, so I ran a control (`gates-journey-control.sh`). It uses round 5's tree and its own GDExtension libraries, built at 02:43–02:53 that day, with the same command, at round 7's load. The control's headless run is identical to round 7's at every step except `swing_wall_msec`. Its rendered run took 4 choices, a value no earlier run produced either. So the same round-5 build gives 3 or 4 rendered choices depending on the run, and the count is timing, not this change.

  The code also cannot reach it. Nothing outside `loading_equipment.*` and the tests calls `run_arrival_prefix`, `through_round` or the four request functions (`git grep`). The live path's only change is six more names in `loading_readiness(…, true).refusals`. `session_controller.cpp:1990-1995` joins those into the `error` string and leaves `ok` and the booleans unchanged, and the refusal lists are identical to round 5's.

  The control wrote only to round 7's `work/plr7/control-r5-now/`. `git status` of the round-5 copy is unchanged.

The mini's `build/` was deleted after the gates.

## Files

- `native/include/champions/loading_equipment.hpp`: the round-7 `ArrivalControl` comment, `ArrivalPrefix::warmup_passes`, the `unmodeled` note, `ArrivalRequest` and the four request functions.
- `native/src/loading_equipment.cpp`: `run_arrival_r6` becomes `run_arrival_staged(…, round)`. Round-6 behaviour is unchanged. The new parts are `set_state_clip`, `delete_name_entry`, the loader tail, the free and the request functions.
- `native/src/script_commands.cpp`, `script_commands.hpp`, `gdextension/src/session_controller.cpp`: the six round-7 limit names and their comments.
- `native/tests/script_commands_tests.cpp`: the two tests above.
- `work/loop-play-loading-r5-report.md`: the stale-wording fix.

Nothing was pushed and no PR was opened. No process was killed by name. Nothing was installed. On the mini, work stayed inside `~/malik-champions`, with no `/tmp`. The private disc was read only, by the journeys.

## Fix round (audit `play-loading-r7.md`: FIX, 1 MEDIUM, 2 LOW)

Base `09501cbe`. Fix commit `f9a30114`. The research pin is still `f8c780b`.

### PLR7-1 (MEDIUM): only `kind=1` as the first property entry

The research ran one list, `{"kind=1", "name=<that name>", 0}` (r7:375-384; `difftest_arrival_stage_r7.py:450-458`). It does not record which keys the six `strncmp` `0x3b91d8` calls compare against. `delete_name_entry` used to accept any first entry that did not start with `name=`. It now accepts only the exact string `kind=1`. Any other first entry is refused at `0x3b91d8`, before the `+0x48` store, the name removal and the free stores, with the existing reason `delete with this property list was not executed`. The comment above `delete_name_entry` and the report text above now say "`kind=1`, then one `name=` entry".

New refusal checks:
- host lists `{"authored_key=1", …}`, `{"kind=2", …}` and `{"kind=1 ", …}`, each followed by `name=authored_odd`;
- the memory layout at `+0x2c` with `authored_key=1` first.

Each check asserts boundary `0x3b91d8`, no store at `+0x48`, at the head word or at the block's first word, an unchanged name count, and that the name still finds the object.

### PLR7-2 (LOW): capacity 0 with a clip length other than 5 is refused

The capacity-0 result "no store touches `obj+0xa0`–`+0xbf`" ran only with clip `+4` = 5 (r7:303-306; `difftest_arrival_stage_r7.py:380-388`). The r8 `props` runs at 9 and `0x40` (`difftest_arrival_stage_r8.py:414-419`) plant no entry sentinel and assert only `+0x17c`. No research shows the entry untouched at 9 or `0x40`, so the stage now refuses capacity 0 with any length other than 5 at `0x170838`, with `anim_push at capacity 0 with a clip length other than 5 was not executed`. The refusal comes before the depth clear, the push and the `+0x17c` set.

**One existing test line changed.** The capacity-0 loop asserted acceptance for lengths 5, 9 and `0x40`. Lengths 9 and `0x40` are now refused, so that loop now runs length 5 only (its assertions are unchanged). A new loop checks that 9 and `0x40` refuse: boundary `0x170838`, no store in the entry, at `+0x9c` or at `+0x9e`, `+0x17c` not set to length + 1, and an empty queue. This is the only changed or removed line in the tests. The executed `+0x17c` result at 9 and `0x40` is no longer reached by the client. That is the cost of refusing.

### PLR7-3 (LOW): the free is not connected to the client's allocator

The stage applies only `pool_free`'s two recorded memory stores: `0x1504bc` puts the block in the bucket-1 head word `0x403a2c` (`0x403a1c` + 16), and `0x1504c4` puts the old head in the block's first word. **The client's allocator is not told of the free.**

- The client's pool is `World::pool` (`objects.hpp:236`), a host `Pool` (`core_state.hpp:48-63`, "No host memory is exposed"). It keeps `head` and `link` as members, and `Pool::free` (`core_state.cpp:62-66`) updates only them.
- No client code writes the class table `0x403a10` to memory. `git grep` for `403a10`, `403a1c` and `403a2c` outside `work/` finds only this stage, its header comment and its test (the other hits are float bit patterns in `tests/vectors/*.tsv`).
- So outside a fixture that plants the head word, `0x403a2c` is 0 and the stage refuses with `pool_free onto an empty free list was not executed`. The accepted path needs a planted free-list head.

**Not connected, because connecting would mean guessing.** The memory head word and the host `head[1]` are two separate states, and nothing recorded maps one to the other. Also, the round-7 test's objects are setup objects (`Variables::new_object`, `script_builtins.cpp:293-306`), whose addresses come from `next_address` or the caller, not from `Pool::allocate` (loaded objects do use it, `script_commands_objects.cpp:257`). Those class-1 test addresses are still on the host pool's free list, so also calling `Pool::free` would link them twice. This is said in the comment at the free (`loading_equipment.cpp`, round-7 block) and in the `ArrivalControl` header comment.

### Fix-round gates

All gates ran on the Mac mini in `~/malik-champions/open-champions/worktrees/play-loading-r7`, at `f9a30114`. The three changed files were synced with `rsync -rlp --checksum` and touched, and their sha256 sums matched the MacBook's. Before the build, `df -h ~/malik-champions` showed 23 GiB free at first, so the build waited until 33 GiB were free (34 GiB at the start of the gate script). There was one `build/` root, with `-j 4` and CTest `-j 3`, and standard input closed. Logs are in the ignored `work/plr7-fix/`, with the home path replaced by `~/`. The scripts are round 7's `work/plr7/gates-*.sh` with `W=work/plr7-fix`, run in order by `gates-all.sh`. Every step logs `uptime` before and after it.

| Gate | Result | Load (1-min) before → after |
|---|---|---|
| Native Release configure and build, `-j 4`, clean `build/native` | **PASS**, exit 0, 2 s + 121 s. 0 warnings in the changed files (10 in the whole build, all in other files, as in round 7) | 15.9 → 22.2 |
| Focused `ctest -R ^script-commands` | **PASS**, 3/3, 1 s | 22.2 → 22.2 |
| GDExtensions | **PASS**, each exit 0. Archive `template_debug` 96 s, `template_release` 94 s; level texture 20 s / 19 s; creature animation `template_debug` 62 s. `lipo -archs` gives `x86_64 arm64` for all five libraries | 22.2 → 11.5 |
| Full CTest `-j 3` | **PASS, 259/259** (259 registered, as in round 7), exit 0, 802 s, one run, no rerun, no timeout. `messages-exact` 7 s, `player-exact` 56 s, `session-facing-exact` 138 s | 11.5 → 6.2 |
