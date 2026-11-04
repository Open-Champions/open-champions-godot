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
flags and player records. Same-act entry preserves that progress and leaves
classified live bags untouched. A cross-act entry removes unkept flag-39
cells through `inventory_removeItem` (`research-inventory-reset` `ce2e086`,
`docs/inventory-reset.md`). The bridge supports one local player. Imported
inventory, a live cell with no definition, a campaign-model bag, and an
unbound source act still refuse before selection or loading. It does not
silently retain old-act quests.

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
It labels the four-level sequence Inferred. The client therefore supplies
no guessed exit, automatic ordering, portal destination, boss completion
or item reward. Existing movement in a loaded World remains available.

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
