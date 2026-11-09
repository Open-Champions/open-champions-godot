# Mission 2 entry and refusal boundary

RTA US v2.00 only. This is a partial route implementation, not a completed
playable mission. Evidence: research-mission2 `8b25047..ffff9a7`, particularly
`docs/mission2.md` at `047adfe` and its PASS audit
`research-mission2-fix2.md`; research-story-route `76d4447` and
research-quest-log `03c2304`. The implementation follows play-route
`e141628`'s bridge without merging or editing that branch.

The real-data scene now connects the hub's `activateMissionMenu` to the
native mission menu. The two story buttons use the campaign's availability
bits. Completing mission 1 alone does not unlock mission 2. The selected
owner executable supplies the 13 table rows at `0x4fe540`, stride `0x34`;
row 1's story pointer is checked against the documented `innovat1` entry.
No route destinations, table payloads or mission text are extracted into
the repository. The host layout shows the availability gate explicitly.
`native/include/champions/mission_menu_table.hpp` owns the portable table
reader; the Godot bridge supplies its result to the native campaign.

The menu advances once per displayed frame, freezes player input, and
uses the existing `hud_stepFade(4)` port while state 6 waits for 128.
It does not supply a forced finished fade. The bridge retains play-route's
explicit fade-input seam for controlled tests; the real scene passes -1
to select the actual fade service. State 8 produces the pending-world
request, and the existing level loader carries player and campaign state.
Button chords refuse: the research executed one button per tick.

The executable's documented act tables bind the campaign and level-flow
models (`research-quests` `7efd3ce`). `campaign_entry.hpp` invokes the native
act-change contract before the destination script: a cross-act entry clears
the old journal and quest-variable values, while retaining mission/medal
flags and player records. Same-act entry preserves that progress. The
bridge supports one local player with a known empty inventory. Imported
inventory, a nonempty live or campaign bag, and an unbound source act refuse
before selection or loading; live bag carry and flag-39 migration are
unported. It does not silently retain old-act quests or discard inventory.

`Host::require_complete_commands` is enabled for all four documented
mission-2 story level identities. An unsupported command stops the script
even when its result is discarded. Deferred actions and later entry calls
then remain stopped. Earlier supported writes are retained; this is a
stop boundary, not transaction rollback. `newWorld(0)` is refused before
interpreting address zero as a string. The inferred boss-music route also
refuses in this mode. The default policy for other levels is unchanged.
The scene shows the stopped command and suppresses pending level changes
from a stopped mission script.

Mission-2 exit actions are refused with a diagnostic even in levels whose
start script did not stop. The research audit explicitly leaves each
record-to-entry pairing and addressed call's entry containment Unknown.
It labels the four-level sequence Inferred. The scene therefore supplies
no guessed exit, automatic ordering, boss completion or item reward. The
hub-return list below is a separate direct call and is not this scene's
exit. Existing movement in a loaded World remains available.

## Evidence still required

- Assert each NPC, PushTrigger and Trigger link per record, and each claimed
  objective/exit call inside parsed public-entry bounds.
- Execute start, both NPC branches, turn-in and deferred grant, boss and
  portal wrappers, both gate states, and resolve both null exit calls.
- Port the missing cutscene service and the selected object constructors,
  physical interactions, boss death/drop and quest item pickup. Enemy
  classes remain owned by port-mission2-hostiles.
- Verify the real hub offer, original arrival placement and consecutive
  physical exits with the same Player, World, shared seed and save state.
  The existing loader's omitted construction draws still prevent
  original-seed parity claims.

## Verification entry points

The `script-commands` CTest includes authored fixtures for the separate
mission-2 offer, row selection and actual fade, campaign replacement,
ignored-command/deferred-grant refusal, null exit refusal and restored
selected-object context. Authored act fixtures verify same-act preservation,
cross-act journal/variable reset, retained mission/player records and
refusal of unknown inventory or source-act state.

`tools/run_mission2_route.py` runs headless and rendered private acceptance.
It requires `--godot`, `--game-folder` and `--research-dir`; output must
stay under ignored `work/`. The existing fixture generator verifies the
research `9649e6b` reference hashes before building two synthetic hub menu
scripts. The native `champions-script-commands-tests` fixture generator
also supplies an authored nonempty-inventory campaign; its path can be
set with `--fixture-generator`. This deliberately controlled offer is
labelled in the result. The probe checks the old-act authored quest is
cleared at entry and verifies inventory refusal without changing the menu.
The executable table, entry archive, scripts, Player and World are real.
The other three loads are labelled direct diagnostics. Aggregate counts,
ready ticks and expected refusals are measured; the result explicitly
leaves `playable_route_verified` false. Captures and original data remain
local and untracked.

## Hub-return list

Research pin: `research-mission2-route` `b8e2d11`, `docs/mission2-compose.md`.
`mission2_route` is the direct-call contract. Public tests use synthetic
ordinary names. The session does not call it, because the object that calls
the two list writers during play was not re-established
(`docs/mission2-compose.md:48-51`).

Latch 0 on reset stores the one name from `game_getHubName`. A nonzero latch
leaves the existing entries and does not call the resolver
(`docs/mission2-compose.md:16-24`, `tools/ref_mission2_compose.py:10-21`).
A new current name is appended, then the first hub entry other than the
resolver result is removed. A second copy of that other hub name stays. A
name that is already listed returns with the resolver uncalled
(`docs/mission2-compose.md:28-40`, `tools/ref_mission2_compose.py:24-41`).
Latch-clear reset followed by four ordinary insertions leaves the resolver
hub and then those four names (`docs/mission2-compose.md:42-46`).

Portal gate 0 refuses before discovery and registers no travel task. Gate 1,
with the current level already listed, writes row 0 through the existing
travel counter. The pre-load stop accepts that pending hub and does not enter
the loader. Player bytes and the quest table are copied through unchanged
(`docs/mission2-compose.md:54-70,92-96`, `tools/ref_mission2_travel.py:54-58`).

Mission-menu run-task admission, including counter 0, stays on
`codex/play-progression` round 3, code `d5889ae9`, report `c583578a`. It is
not copied. This portal uses the existing jump-gate travel counter.

These stay refused:

| Call | Reason |
|---|---|
| Hub2 dialog choices | The dialog was not opened (`docs/mission2-compose.md:124-130`). |
| Helper `0x18ebb8` | The compared menu path stubs it. Its other effects are Unknown (`docs/mission2-compose.md:133-138`). |
| Physical NPC, trigger, and portal use | Unknown (`docs/mission2-compose.md:141-145`). |
| Loader body | The stop is before any loader instruction at `0x1e55c0` (`docs/mission2-compose.md:65-70`). |
| Arrival | Unknown (`docs/mission2-compose.md:72-74`). |

List save/load and a nonzero act-change on this boundary stay unexecuted.
The older destination-list walk in level flow is unchanged.

## Jump-gate portal (round 6)

Research pin: `research-mission2-route` `3c10c07`, `docs/mission2-round6.md`
(audits `research-mission2-route-r6.md` and `-r6-fix.md`, PASS). Code:
`native/include/champions/mission2_portal.hpp`, in the same
`champions-mission2-route` library. The live session does not call it.
Only Observed or Executed behaviour is carried. Line numbers below are in
`docs/mission2-round6.md`.

- **Menu** (`portal::open`, `portal::frame`, one call per frame, `:204-207`).
  State 0 finds the current level in the list, clears the selection, sets
  `0x41b4fc`/`0x41b500` and pauses offline (`:133-142`). State 1 fades in by
  8 until `0x80`, then holds `0x7f`: 16 frames (`:144-145`, `:216`). State 2
  cancels on pressed `& 0x120` or an inactive slot, confirms on pressed
  `& 0x40` unless the selection is the current level (`0x27dfa0`), and
  otherwise moves up on `+0x10 & 0x101000` above 0 and down on
  `& 0x204000` below count − 1 (`:147-163`, executed `:218-228`). State 3
  fades out by `0x10`: 8 frames, then `pauseMenu_close` and state 4 with
  travel, or `net_lockLocalInput(chooser, 5)` and removal of both tasks
  without it (`:165-172`, `:229-230`, `:245-254`). State 4 begins travel
  (`:174-175`). State 5 counts travel to 181, then writes the selected name
  to `g_pendingLevel` and `0xff` to the orientation byte, and nothing else
  (`:177-184`, `:232-240`). The travel word is
  `level_flow::State::travel.ticks`, which the client's loader path reads.
- **Handoff** (`portal::handoff`). The request goes through
  `SessionAdapter::change`, the client's `mainloop_loadRequestedWorld`
  path: the level left is stored, the name splits with an empty subname,
  `g_prevLevelName` is the level left, `0x41b7c1` keeps `0xff`, `0x41b720`
  becomes `0xfe`, and the loader gets `hub` (`:302-318`). The handoff
  refuses any request that is not the gate's (orientation `0xff`, travel
  181).
- **Arrival.** The existing adapter path builds the destination's own gate
  with travel set: one free-spot search per player near it (the call passes
  `0x24` and `0x18`), one `g_randSeed` step, facing from the seed's high
  half, then rank 3 so the loader's own search is skipped, the terrain
  height, and player 0 constructed there (`:334-363`). The free-spot
  geometry stays the caller's hook.
- **Destination block** (`portal::save_destinations`,
  `portal::load_destinations`). The sign-extended count byte as a word, then
  the 480 bytes; the load keeps the word's low byte. Both return `0x1e4`.
  This is the `+0x134` save row that `ps2_save::pack_jump` packs
  (`:258-285`).

Refused: a net game and a fixed destination (read, not executed: `:141`,
`:148-150`, `:180`), up and down in one frame, and menu state 6 and later.
State 6 → 7 and state 7's clear were executed (`:186-189`, `:241-243`), but
which main-loop pass takes the request against state 6 is Inferred
(`:292-296`), so the client leaves the menu in state 6. Main-loop steps 2,
4 and 5 were not run (`:465-471`). The loader internals round 6 runs (VU1,
DMA, boot and `engine_init` fixtures) are research scaffolding and are not
ported. The older `portal_row0` direct call and the refusal rows above are
unchanged.
