STATUS: PASS — 2026-11-09. The round-6 jump-gate portal is carried in the native client, end to end through the real `Loader` and `SessionAdapter`. That covers menu selection with the refused current level, the 181 travel frames and the name/`0xff` write, cancel, the handoff, arrival at the hub's own gate, and the 0x1e4 destination block. State 6 and later, the net and fixed-destination paths, and main-loop steps 2/4/5 stay refused or unclaimed. Gates on the mini: Release `-j 4` plus five universal extensions; focused 12/12 (`mission2-route` 1,215 checks); CTest 267/267 with no timeouts; strict Godot 55/55; combat 2,645/0; FULL_SAVE_LOAD 300/300; FULL_TRACE unchanged (36,402 bytes, `9c7922b2…`); journeys equal to wave 41 at every step/result pair (80/81, refused rows 27/28, pickup 10→11); probe 234/241, identical to wave 41.

# Play mission 2 portal, round 1 — 2026-11-09

Branch `codex/play-mission2-portal`, MacBook worktree
`~/Projects/open-champions/worktrees/play-mission2-portal`, from wave 41
VERIFIED `6fcda7de`. Research: `t3/research-mission2-route` `3c10c07`,
`docs/mission2-round6.md` (audits `research-mission2-route-r6.md` and
`-r6-fix.md`, PASS, three LOW R6F-1..3 that touch only the research's own
test strength and doc wording). Line numbers `:N` below are in
`docs/mission2-round6.md` at `3c10c07` unless another file is named.

Edits and commits are on the MacBook worktree. Builds, tests, Godot and
journeys ran only on the Mac mini, in
`~/malik-champions/open-champions/worktrees/play-mission2-portal`. That copy
was seeded locally from the mini's `playable-integration-w42` (the renamed
w41 tree) without `build/` and `.godot`, then made identical to this
worktree with `rsync -rlp --checksum --delete` (no `-t`; 32 files rewritten
or added, 10 deleted, `work/play-mission2-portal-r1/rsync.txt`), and the
rewritten files were touched. The mini copy has no `.git`, and the mini's
bare repo does not hold wave 41, so the commits were made directly on the
MacBook branch. There was nothing to fast-forward.

## What the client already did

- **Wave 38, mission2-route** (`native/src/mission2_route.cpp`): the
  hub-return list (`reset_destinations`, `add_current_level`) and the direct
  call `portal_row0`. It drives row 0 through `level_flow::GateTravel` and
  stops before the loader. Other rows, the loader body and arrival refuse.
- **Wave 39, exits-travel** (`native/src/level_flow.cpp`,
  `native/src/session_level_flow.cpp`): `begin_change`
  (`mainloop_loadRequestedWorld` preparation: departure store, prev, split
  at `_`, orientation copy, `0xfe`) and `SessionAdapter::change` (loader,
  object build, placement). When travel is set and there is no fixed
  destination, a `Portal`/`PortalHurt` record runs one free-spot search per
  player, with spread 36.0 and radius 24.0 and any-height set. Facing comes
  from the shared draw, and rank is set to 3, which skips the loader's own
  search. `place_players` then snaps to terrain z and constructs each slot.
- **`GateTravel`** (`level_flow.cpp:838-872`) models confirm, then 181
  counter calls, then the request. It has no fade-in, selection, fade-out or
  begin-travel frames.
- **The +0x134 save row**: `ps2_save::pack_jump` (`native/src/ps2_save.cpp:396-403`)
  and `load_impl` (`:792-793`) already pack and restore the 0x1e4 block.
- **The live Godot host** has no gate discovery and refuses gate travel and
  portal arrival (`gdextension/src/session_controller.cpp:2346`, `:2357`,
  `:2376`, `:2878`; `session_controller_hub.cpp:21`).

## What changed

New `native/include/champions/mission2_portal.hpp` and
`native/src/mission2_portal.cpp`, in the existing `champions-mission2-route`
library. The library now also links `champions-session-level-flow`. The live
session and the GDExtension do not link it, so no Godot binary changes.

- **Menu selection.** `portal::frame` runs one call of `jumpgate_menuRun`
  per frame.
  - State 0 finds the current level and stores its index. It clears the
    selection and the previous selection, sets `0x41b4fc`/`0x41b500`, and
    pauses offline (`:133-142`, executed `:212-215`).
  - State 1 fades in by 8 until `0x80`, then holds `0x7f`: 16 frames
    (`:144-145`, `:216`).
  - State 2 handles input (`:147-163`). Pressed `& 0x120`, or an inactive
    slot, cancels with the accept sound. Pressed `& 0x40` confirms unless
    the selection is the current level, which is refused as at `0x27dfa0`
    (`:154-156`, executed `:222-224`). Otherwise `+0x10 & 0x101000` moves up
    when the selection is above 0, and `& 0x204000` moves down while it is
    below count − 1 (executed `:218-220`).
- **What the gate writes.**
  - Confirm stores travel 1 (`0x27e00c`, `:155-156`, `:228`).
  - Fade-out takes 8 frames, then calls `pauseMenu_close` and clears both
    pause words (`:165-169`, `:229-230`).
  - State 4 begins travel (`:174-175`, `:231`).
  - State 5 runs 181 frames. The first 180 write nothing for the next world.
    The last writes the selected name to `g_pendingLevel` `0x41b6d0` and
    `0xff` to `0x41b720`, and nothing else (`:177-184`, `:232-240`).
  - Cancel fades for 8 frames, then calls `net_lockLocalInput(chooser, 5)`,
    which unlocks input, and removes both tasks. It leaves the pending name
    and the orientation byte unchanged, and travel stays 0 (`:170-172`,
    `:245-254`).
  - The travel word `0x500ae8` is `level_flow::State::travel.ticks`, the
    value the client's loader path reads when it builds a gate.
- **Handoff.** `portal::handoff` accepts only the request state 5 wrote
  (orientation `0xff`, travel 181) and calls the client's
  `SessionAdapter::change`.
  - The departure store gets the level being left. The loader gets `hub`.
  - The name splits with an empty subname, `g_prevLevelName` is the level
    left, and the pending name is empty.
  - `0x41b7c1` holds `0xff` and `0x41b720` holds `0xfe` (`:302-318`).
- **Arrival.** This uses the existing adapter path unchanged. The new test
  shows that it is the behaviour round 6 executed at the destination's own
  gate (`:334-363`):
  - the gate is built once and sees travel 181;
  - one free-spot search runs near that gate. The call at `0x27c9d8` passes
    `0x24` and `0x18` in the third and fourth argument registers, the same
    order as the client's 36.0 and 24.0;
  - `g_randSeed` steps once, and the facing is the seed's high half;
  - rank 3 is stored;
  - player 0 is constructed once, at the free spot beside the gate, on the
    terrain height, with that facing.
- **Destination list save/load.** `portal::save_destinations` writes the
  sign-extended count byte as a word, then the 480 bytes, and returns
  `0x1e4`. `portal::load_destinations` keeps the word's low byte, copies the
  480 bytes back, and returns `0x1e4` (`:258-285`). The test checks that the
  block equals `ps2_save::pack_jump`'s +0x134 row.
- **Refused.** These all throw `Refused`:
  - a net game (`:142`, `:148-150`) and a fixed destination (`:141`,
    `:180`): both read, not executed;
  - up and down held in the same frame;
  - an unlisted current level;
  - travel already nonzero at open;
  - more than 15 names;
  - a name that does not fit its 32-byte slot;
  - menu state 6 and later (see below).

Tests: the existing `mission2-route` test gains four functions on seeds
20973 and 7: `test_portal_menu`, `test_portal_cancel`, `test_portal_arrival`
and `test_destinations`. It goes from 1,019 to 1,215 checks. All names are
authored (`ord-p`, `zm…`), and the level images are authored lumps run
through the real `Loader` and `SessionAdapter`. No existing assertion was
changed. `docs/MISSION2_ROUTE.md` gains a "Jump-gate portal (round 6)"
section.

## Kept refused (Inferred or Unknown, or not run)

- **Menu state 6 and later.** Round 6 executed state 6 → 7, and state 7's
  travel clear and task removal (`:186-189`, `:241-243`). Which main-loop
  pass takes the request against state 6 is Inferred (`:292-296`). As
  instructed, the client does not run state 6: the menu is left there and a
  further frame refuses. After the load, the adapter's existing
  exits-travel clear (`GateTravel::arrived` on the first ready tick,
  `session_level_flow.cpp:46-48`, `:172`) still clears travel. That is
  older behaviour; the round-6 state 7 is not ported.
- **Main-loop steps 2, 4 and 5** were not run (`:465-471`). The new code
  does not add or claim them. `begin_change` still sets `sim_frame = 0` and
  the ambient countdown 60 (`native/src/level_flow.cpp:1014-1016`). Those
  come from the earlier mainloop/exits research, not from round 6. They are
  left unchanged, for decision.
- **Loader internals** (VU1, DMA, timer, boot and `engine_init` fixtures,
  sections 1, 5, 6) are scaffolding and are not ported. The client keeps
  its own `Loader`.
- **The free-spot geometry and what it draws from `g_randSeed`** are
  Inferred (`:370-371`). `FindSpot` stays the caller's hook. Round 6's
  spawn and heading numbers are disc-derived and are not used.

## Existing refusals superseded by round 6 (not flipped, for decision)

| Refusal | Superseded by |
|---|---|
| `native/src/mission2_route.cpp:95` `portal_row0` row ≠ 0, "pad movement is Unknown"; asserted at `native/tests/mission2_route_tests.cpp:243` | `:218-224` (seeded up/down selection executed) |
| `native/src/mission2_route.cpp:155` `refuse_arrival`; asserted at `mission2_route_tests.cpp:252`; `docs/MISSION2_ROUTE.md:130` | `:351-363` (gate placement and `Player_ctor` executed) |
| `native/src/mission2_route.cpp:151` `refuse_loader_body`; asserted at `mission2_route_tests.cpp:251`; `docs/MISSION2_ROUTE.md:129` | `:302-312` and `:373-379` (`game_loadLevel` runs and returns under fixtures). The internals stay scaffolding, so only the stop's claim is superseded |
| `docs/MISSION2_ROUTE.md:132` "List save/load … stay unexecuted" | `:278-285` (round trip executed) |
| `gdextension/src/session_controller.cpp:2878` "Portal arrival refused: destination constructors, post-constructor seed, blocking-object grid and live actor facing are unavailable." | constructor, seed step and facing: `:334-363`. The blocking grid (free-spot geometry) is still Inferred (`:370-371`) |
| `session_controller.cpp:2357` `script_gate_confirm` "…paused menu task, and destination constructor seed…" and `:2346` status text | menu task: `:195-254`; constructor seed: `:342-345`. The campaign GateList lifecycle in the live host is still not established (`docs/mission2-compose.md:48-51`) |
| `gdextension/src/session_controller_hub.cpp:21` "…travel and arrival are not integrated in this host." | travel `:226-243`, arrival `:351-363`. Discovery in the live host still needs the live object-grid query |
| `native/include/champions/level_flow.hpp:149`, `native/src/level_flow.cpp:669` `OpenItem::GateAnyHeightOnDisc` | partly: `:353-363` ran `JumpGate_ctor`'s search on the hub under fixtures. Its geometry inputs are still Inferred |

Not a refusal, but a timing difference to decide: `GateTravel`
(`level_flow.cpp:844-871`, used by `level_play` and the live controller)
writes the request 181 counter calls after confirm. Round 6 has 8 fade-out
frames and 1 begin-travel frame before state 5's 181 (`:229-232`).
`GateTravel` and its tests are unchanged.

## Gates (Mac mini)

Evidence is under ignored `work/play-mission2-portal-r1/`, copied back with
the mini home path rewritten to `~`. Script: `gates.sh` there, which is wave
41's `gates.sh` with the `W` and `E` paths changed and one `focused` stage
added (`diff` shows only those lines). Each build step checks for at least
30 GB free: 35 GB at the first check, 44 GB before the final rebuild.
Parallelism was `-j 4` and `ctest -j 3`. CTest and the matrix ran together,
then the journeys and the probe in sequence. The mini load average was
about 30–100 from other agents. PIDs started: build+ext 32854, CTest 79399,
matrix 79400, journeys+probe 8809. All exited on their own, and none was
killed. Standard input was closed for CTest, Godot and the probe.

1. **Release build + GDExtensions: PASS.** Fresh configure. First the
   `champions-mission2-route-tests` target alone, which found two test-only
   compile errors (`flow::Raw`) and a missing authored `msel.lmp`. Both were
   fixed before the first commit. Then the full native build: exit 0, 227 s.
   The comment-only citation commit `ed5b2f67` was synced, touched and
   rebuilt: two objects, exit 0. No compiler warnings. The 11 `ignoring
   duplicate libraries` linker notes are identical in text to wave 41
   (`diff` of the warning lines is empty). Extensions: exit 0, 372 s.
   Archive and level-texture, each `template_debug` and `template_release`,
   and creature-anim `template_debug` all report `x86_64 arm64`, the same as
   wave 41. Warning counts per log are the same as wave 41: 6, 6, 4, 4 and
   0. The GDExtension does not link `champions-mission2-route`.
2. **Focused: PASS, 12/12**, 194 s: `mission2-route` (1,215 checks; was
   1,019), `level-flow-exact`, `level-play`, `story-route`,
   `script-commands`, `quests`, `mission2-actors`, `ps2-save`,
   `native-save`, `native-save-access`, `native-save-partition` and
   `native-save-indexed`.
3. **Full CTest `-j 3`: PASS, 267/267**, exit 0, 1,299 s. The count is the
   same as wave 41: no test was added; `mission2-route` grew in place. **No
   test timed out, so none was rerun.** Longest under load: `netplay` 223 s,
   `session-facing-exact` 212 s, `native-save-indexed` 164 s.
4. **Strict Godot matrix: 55/55 PASS**, exit 0, 571 s, run after one
   headless import (exit 0, 0 error lines; the class cache includes
   `NativeFxRenderer`). The unchanged `check-log` reports `logs=107
   failing=0`.
5. **Combat: 2,645 checks, 0 failures**
   (`matrix/playable_wave_smoke_full_combat.log`).
6. **FULL_SAVE_LOAD: PASS, 300/300.** The continued and uninterrupted
   presentation hashes are both
   `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`.
7. **FULL_TRACE: unchanged.** 36,402 bytes, SHA-256
   `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
   headless renderer. No baseline was edited.
8. **Journeys.** Unchanged `tools/run_loop_playthrough.py`, headless then
   rendered, read-only disc, 110 s. The wrapper exits 1, which is the
   retained `player_death` refusal. Disc files: 994 before, 994 after.
   Compared with wave 41 by wave 41's own `compare.py`:

   | Variant | Steps | Step/result pairs differing | Refused rows | Refusal kinds | Pickup | Captures |
   |---|---|---|---|---|---|---|
   | Headless | 80 | 0 | 27 (= w41) | 8 (= w41) | 10→11, 52 walk ticks, 0 retries | 0 |
   | Rendered | 81 | 0 | 28 (= w41) | 8 (= w41) | 10→11, 52 walk ticks, 0 retries | 38 |

   Every difference is listed here:
   - **`portal_arrival`: no change.** It is `blocked_no_real_source` (38
     candidate levels) in both variants. It was the same in wave 41 and in
     wave 41's rerun. The step needs a loadable level with `Portal`
     records, then calls the live controller's `script_gate_confirm`. Its
     refusals (`session_controller.cpp:2357`, `:2878`) are untouched, as
     instructed for superseded refusals. The new module is native-only, and
     the live host still has no gate discovery. So the round-6 port cannot
     reach this row. Wiring it into the live host is the decision listed
     above.
   - **Hub-action retry count, rendered only:** `… hub action beyond
     documented <path> offers is Unknown count=1` became `count=3`. Headless
     stays at `count=2`. This count follows the frame rate of the dialog
     steps (wave 41 report, difference 1). Rendered `dialog_prompt` /
     `dialog_choice_close` ran at 157/149 fps here. Wave 41 ran at 93/77
     with count 1, and its rerun at 107/122 with count 2. No dialog, hub or
     `setMissionAvailable` code changed.
   - **Host only:** `fps`, `memory_bytes`, and `enemy_fight.swing_wall_msec`
     (headless 39,592→29,041, rendered 41,961→31,203). Fight HP, drops and
     awards are identical.
9. **Story-route probe: 234/241**, exit 1. Apart from the disc-count lines,
   the output is line for line wave 41's `probe.log` (`diff` empty). The
   seven failures are the same disclosed `m3-gnome-kill` #1-6 and
   `m5-start` #1. Disc files: 994 before, 994 after.

Privacy: no file this branch changed contains a macOS absolute home path (the slash, `Users`, slash prefix; written split here so this report adds no match). The mini
`build/` (6.5 GB) was deleted afterwards; 34 GB was free and the tree is
1.2 GB. No push, PR, install, `/tmp` use on the mini, private-disc write,
research or `playable-integration` edit, or process-name kill.
