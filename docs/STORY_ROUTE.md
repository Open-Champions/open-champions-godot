# Story route after the first menu request

RTA US v2.00 only. Research pin: `t3/research-story-route-r3` `3c7987c`,
`docs/story-route.md` sections 6-8. Audits: `research-story-route-r4.md`;
`research-story-route-r5r6.md` was VERDICT FIX (SR6-1), and
`research-story-route-r6-fix.md` closed it with PASS.
Mission 1's completion writers stay on `t3/research-mission-unlocks`
`7b924e1` (`docs/spec/mission-unlocks.md:279-523`); this branch uses only the
claims that `research-mission1-complete-fix2.md` marked closed and adds no
mission 1 behaviour. This is a bounded client contract, not a playable route
past mission 2.

Code: `native/include/champions/story_route.hpp`, `native/src/story_route.cpp`
(part of `champions-script-commands`), the host's `EntryGate`
(`script_commands.hpp`), the menu row-0 rule in `quests.cpp`, and the scene
wiring in `gdextension/src/session_controller.cpp` and
`godot/scenes/real_data_play.gd`. Tests: CTest `story-route`
(`native/tests/story_route_tests.cpp`, authored fixtures only). Private
acceptance: `champions-story-route-probe DIR` (counts only).

## Mission menu (section 6)

`story_route::menu_entries` turns the executable's mission table (read at run
time by `quests::read_mission_rows`) and act tables into twelve rows, missions
1..12. Mission 0 is not a row. Only missions 6, 10 and 12 carry an evil level,
each in its story level's act, and the story acts from current act -1 are
0..11 in row order. Any other table shape is refused
(`docs/story-route.md:228-268`; SR4-1 asserted the twelve tuples).

Confirm, column 0, requests the row's story level, or its evil level when
`game_getEvil` is set and the row has one, with orientation `0xff`. Mission
bytes are unchanged. At difficulty 1 the evil bit is bit 1; bit 0 keeps the
story level (`:257-262`). Row 0 confirms even when mission 1's available bit
is clear (`0x231078(0)` returns 1); `Campaign::tick_menu` now applies that
rule through `MenuWorld::row0_open`. That request is not a hub offer
(`:336-338`). Later rows confirm only when their mission is available.

Cursor rules, already in `quests::menu_tick` and now covered by tests: the
state-0 tick selects the highest available mission in 1..12; completing a
mission does not move the row; a newly available bit does not move it until
down or the state-0 tick after reopen; down will not step onto a locked row;
up sits on one. Reopen keeps the row, the old request, orientation and pause
until the state-0 tick (`:228-236`, `:303-309`).

Missions 3-5 story confirm preflights the act transfer the same way mission 2
does. Missions 6-12 may be requested; the scene then refuses the level
(`:322-326`, `:349-354`). The medal and bonus columns (menu columns 1 and 2)
were not re-run (`:268`), so the scene also refuses a medal or bonus level of
missions 3-12 (`story_route::load_refusal`). Missions 1 and 2 medal and bonus
keep their existing policy.

## Missions 3-5 (sections 7-8)

`story_route::gate_for(level)` returns a host `EntryGate` for the nineteen
mission 3-5 levels sections 7-8 name. The scene turns on
`require_complete_commands` and the gate for those levels. In them:

- Only the 75 listed entries may start, whether directly, at level start, as
  a post action, as a deferred call or through `callScript`. Research ran 71
  of them (`failQuest`, `endMission` and the two `steppedOnTrigger` entries
  only inside their callers). The other 4, the `islc1` racer wrappers
  `postActifb1b`, `postActi6592`, `postActi928c` and `postActifd9a`, are a
  static parse of the dialog file and the wrapper entries, not a run
  (`:442-450`). Any other entry
  is refused before it runs and stops the level script (`entry_refusals`).
- Thirteen named entries are refused with their own reasons: `brokeFinalWall`
  on the three `wate2` levels (faults at pc `0x146ecc`, `:728-736`),
  `holeCheck` on the three `watg` levels (distance Unknown, `:738-743`),
  `start` on the six side levels (what it stores into the compared
  `shardCheck` variable is Unknown, `:676-681`, `:723-725`, `:945-948`), and
  `startDia7a53` (`:769-771`). `watmedal` and `tormedal` are the mission 4
  and 5 medal levels. Research did not run them (`:562-563`), and they are not
  among the nineteen gated levels. Their load is refused as a mission 4 or 5
  medal-column level (see the mission menu above).
- An entry started from an object record (physical use or trigger) is
  refused: that dispatch was not run (`:549-552`, `:945-947`).
- Dialog opening is refused: the selector was not run and node reachability
  is Inferred at best (`:412-421`, `:442-450`, `:465-474`, `:782-802`).

The listed entries then run through the existing commands:

- Starts make the mission available on record 0 only and store
  `startedMissionN` and `currentMission`; `wat1` also adds its one quest,
  chosen by the evil byte (`:391-398`, `:599-611`, `:752-760`).
- `cutScene` stays refused, so a posted intro that would request a cut
  (`islc1` `absorb`, `wat1` `absorb`/`waterIntro`, `tormenta` `if87a0` and
  `if3f82`) stops the script there. The research recorded the request, not a
  played cut (`:400-408`, `:613-621`, `:773-780`).
- Completions grant on all four records (`0xA0` to `0xA2`, record 0 `0xA1`
  to `0xA3`) and do not make the next mission available (`:476-494`,
  `:623-634`, `:686-712`, `:815-830`).
- `playerHasItem` is the client's `inventory_countItemByName` over the real
  bag, the one stub-dependent service that is a client service
  (`:469-474`, `:686-706`). `addHelpMessage` keeps its text-table and chime
  requirements. `activateStore` stays refused without an attached dialog.
- `autoPropSetState`, `killMonster`, `soundSequence` and `preloadResource`
  run the level-commands port in these levels (`docs/LEVEL_COMMANDS.md`,
  research `t3/research-level-commands` `8efa13d`).
- Travel entries write their level, entrance and orientation `0xff`
  (`:430-440`, `:454-463`, `:638-670`, `:832-848`). Research ran each listed
  travel entry through the original binder and recorded the pending level it
  wrote. On the disc, several of those shipped entries pass data offset 0 as
  the level string. That is a client observation, not a research line: the
  private probe reads offset 0 and reproduces every recorded level. In a
  gated level the host therefore reads offset 0 as the binder does; only
  listed entries can start there. The mission 2 `newWorld(0)` refusal stays
  for ungated levels.
- **Mission 3:** `islgnome` `entrance1`-`entrance3` with `startedQuest` 1
  and `finishedQuest` absent or 0 make eight `killMonster` calls and still
  request `islc2` with their door (`:454-463`). The client now runs those
  calls (`docs/LEVEL_COMMANDS.md`). **Remaining play gap:** no Gnome
  constructor is ported, so with the level's records adopted, the gnome
  targets are stubs and the first `killMonster` refuses before any store.
  The probe checks the research outcome (`m3-gnome-kill`) and this play stop
  (`m3-gnome-kill-stub-refusal`).
- **Mission 5:** `tormenta` `start` runs its five `autoPropSetState` calls
  and then `setMissionAvailable(5, 1)`. **Remaining play gap:** no AutoProp
  constructor is ported, so with the records adopted, the first call finds a
  stub and refuses before the availability store (`m5-start-stub-refusal`).
  `preloadResource` in `preStart` is a recorded no-op when the game folder
  has the lump. The play session does not supply that listing yet, so in play
  it still stops there.

The hub and hub2 entries that make missions 4, 5 and 6 available are listed
in `story_route::availability_entries` (`:504-510`, `:861-878`). Research says
which mission 5 and 6 entries have a `changeSwoop(0)`/`activateStore` pair and
that the others have neither (`:868-876`). Section 7 does not mention store
calls for the four mission 4 entries, so their store calls are Unknown
(`store_known` false). On the disc, `setMissidce3` and `setMissic4` stop at the
client's refused `activateStore` after the availability write. The list
records that as a client observation (`client_store_stop`), not as research.
The hubs keep their existing host policy. The tests check each entry against
a research-cited table and replay the listed shapes.

## Kept refused

| Item | Reason |
|---|---|
| Menu draw `0x232300` | Faults on an 8-byte store through the display-list cursor at `0x44fe10` (pc `0x17b968`) with its stubs installed (`:289-301`). The Godot view is a host layout, not this draw. |
| Input-ready word `0x8e2f10` | Who restores it to 1.0 during play is Unknown (`:289-301`). The menu keeps reading the pad each state-2 tick, as before. |
| Loader body and arrival | Not run (`:322-326`, `:549-552`, `:945-947`). The existing loader still carries the player and campaign; arrival placement is not claimed. |
| Dialog selection in mission 3-5 levels | Not run (see above). |
| Object use and triggers in those levels | Not run (see above). |
| Missions 6-12 in-level beats | Not established (`:322-326`, `:349-354`). |
| Medal and bonus levels of missions 3-12 | Columns 1 and 2 not re-run (`:268`); `watmedal` and `tormedal` not run (`:562-563`). |
| `killMonster`, `autoPropSetState` on stub targets | No Gnome or AutoProp constructor is ported; the fields the ports read were set by hand in the research (`docs/LEVEL_COMMANDS.md`). |

## Private acceptance

`champions-story-route-probe DIR` loads each level's `script.scr` from the
selected read-only disc, checks that every listed and refused entry is a public
of its level, and replays the executed cases with the gate on, one player,
mission bytes 3..6 planted at `0xA0` and the pending level, entrance and
orientation at the research's sentinels. Quest titles are compared with each
other at run time and never printed. Output is group counts and case numbers.
`m5-start` and `m3-gnome-kill` check the research outcomes with no object
adopted. The `-stub-refusal` groups adopt the level's `objects.ob` as stubs
and check the remaining play stop.
