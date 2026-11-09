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

## Hub2 objects, the particle reset, and the archive open

Research pin: `research-mission2-route` `d17b46f` (audit
`research-mission2-route-r3r4-fix.md` PASS), `docs/mission2-compose.md` and
`docs/mission2-round4.md`. `mission2_objects` is a direct-call contract like
the hub-return list. The session does not call it: the level load stops
before these objects exist in play (`docs/mission2-round4.md:47-69`). Public
tests are synthetic: authored rows, variables, seeds and names. Every path
the research did not execute throws `Refused`.

- hub2 dialog. A cold open (mission 0 at 3, mission 1 at 0 or 3, missions
  3..12 clear, variables absent) follows mission 2's byte: 0 or 2 is node 1
  with one choice, 1 is node 4, 3 is node 5. Node 1's choice runs `0x29bc`,
  grants nothing, and lands on node 2, or on node 3 when mission 1 is
  complete. Node 4's choices change no mission byte. Node 5's two choices
  make mission 3 available. Reopening after the node-3 landing selects
  node 3, and both of its choices make mission 2 available
  (`docs/mission2-compose.md:124-155`). Whether the node-2 landing stores
  a variable was not read back, so that reopen refuses.
- Key `0x98` opens on any mission 1..11 available and not complete, or
  mission 12 available (`docs/mission2-compose.md:159-165`).
- Action `0x54` calls `0x2210`, whose gate `0x1a84` is a mission 1..12 that
  is available, not complete, and has its own quest variable set. Gate 0
  calls `setEntrance` `evilstart` then `activateMissionMenu`; gate 1 enters
  `0x22d0` (`addHelpMessage`, `addDeferedCall`). Both return 0
  (`docs/mission2-compose.md:167-190`). The `dialog_isOpen` split by seed is
  the seeded responder stub, so the menu open is not decided.
- `particle_resetAll` after init returns 1, keeps slot 0, frees 1..31,
  clears both handles, sets both generations to 1, and releases a claimed
  definition. The init ran with `0x19e4a0` and `0x173768` stubbed to 0
  (`tools/difftest_mission2_compose.py:486-495`). Before init it refuses
  with the menu-image fault (`docs/mission2-compose.md:192-210`).
- The archive open is a read-list check: the first read is `0x800` bytes at
  offset 0 of the `.gob`, one read is `0x5000` bytes at offset 0 of the name
  in `g_levelTexName` plus `.tex`, every read hits one file, and the lump
  count goes from 0 to 3. An unset tex name refuses
  (`docs/mission2-round4.md:12-45`). The existing level loader is unchanged.
- `handle_initPool` after `pool_init` sets the free-list head. Then the
  GameObject constructor takes one shared-seed step, and the PushTrigger,
  Trigger and NPC constructors return their flags
  (`docs/mission2-round4.md:71-92`).
- `Player_interact` with zero hit points returns the player record and makes
  no calls (`docs/mission2-compose.md:228-235`). After the null-`r11`
  character init with a linked object, a PushTrigger in range is prompted
  once, the prompt returns 1, and the use function is not entered
  (`docs/mission2-round4.md:94-125`).

Refused: the PushTrigger use `0x1ecc70` (the test that skips it is Unknown),
the portal constructor (its lump is not resident), `game_loadLevel` at the
`texcache_reset` write or the `sceSifSetDma` call (the DMA result is not
invented), arrival, and NPC or Trigger prompts.
