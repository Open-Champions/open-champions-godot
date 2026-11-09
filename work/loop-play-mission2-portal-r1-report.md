STATUS: PENDING

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

GATES_PENDING
