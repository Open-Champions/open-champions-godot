# Play story route, round 1

STATUS: IN PROGRESS. Gates are recorded below as they finish.

Branch `codex/play-story-route` from wave 38 VERIFIED `16b66176`. Code commit
`5f5b4ad6`. Research pin `t3/research-story-route-r3` `3c7987c`,
`docs/story-route.md` sections 6-8 (audits `research-story-route-r4.md`,
`research-story-route-r5r6.md`, `research-story-route-r6-fix.md`, PASS).
Contract note: `docs/STORY_ROUTE.md`.

## What the client had

- Mission menu: `quests::menu_tick` / `Campaign::tick_menu` already held the
  state machine, the highest-available opening row, down that will not step
  onto a locked row, and the evil column copy. Row 0 did not confirm with
  mission 1's bit clear. The scene offered missions 1 and 2 only.
- Progression: wave 38's run-task admission and the mission 2 route
  (`mission2_route`, direct call). `require_complete_commands` was on for the
  four mission 2 levels only.
- Missions 3-5: nothing. Their levels loaded under the legacy policy, so any
  entry the client could run would run.

## What changed

All research references are `t3/research-story-route-r3` `3c7987c`
`docs/story-route.md` unless named otherwise.

Mission menu (section 6):

- `story_route::menu_entries` builds the twelve rows from the executable's
  mission table and act tables, read at run time. It refuses any other
  shape: fewer than twelve rows, an empty story pointer, an evil pointer
  outside missions 6, 10 and 12 (or missing on one of them), an evil level in
  another act, or story acts other than 0..11 in row order (`:237-268`; SR4-1
  checked the twelve tuples against the image).
- `story_route::story_request`: column 0 requests the story level, or the
  evil level when `game_getEvil` is set and the row has one; orientation
  `0xff`; at difficulty 1 bit 0 keeps the story level (`:257-262`). Row 0
  confirms with mission 1's bit clear and is marked "not an offer"
  (`:260-262`, `:338-340`, `tools/ref_story_route.py:112-117`). Later locked
  rows refuse.
- `Campaign::tick_menu` now applies that row-0 rule (`MenuWorld::row0_open`,
  set only by the campaign wrapper, so the older field-machine vectors are
  untouched). This is the one behaviour change to existing code paths in the
  menu.
- Cursor rules were already in `quests::menu_tick` and are now tested:
  highest available row on open, completion does not move it, a new bit does
  not move it until down or the reopen tick, down will not step onto a
  locked row, up sits on one, reopen keeps row and request until the state-0
  tick (`:228-236`, `:303-311`).
- Scene: `mission_menu_select_story` accepts missions 3-12 from their table
  row. Missions 3-5 preflight the act transfer like mission 2
  (`story_refusal` per row, and the cross on rows 2-4). Missions 6-12 are
  requested and `script_level_request` then refuses the level with
  `in_level_refusal` (`:322-326`, `:349-354`).

Missions 3-5 (sections 7-8):

- `story_route::gate_for(level)` gives a host `EntryGate` for the nineteen
  levels sections 7-8 name (with the three travel destinations no entry was
  run in). The scene turns on `require_complete_commands` and the gate there.
- The host checks the gate at every entry start: `run`, `run_start` /
  `run_prestart`, post actions, deferred calls and `callScript`
  (`Variables::before_entry`). 75 entries may start; any other entry is
  refused before it runs and stops the script (`Counts::entry_refusals`).
  Thirteen named entries keep their own reason: `brokeFinalWall` x3
  (`:728-737`), `holeCheck` x3 (`:738-743`), side-level `start` x6
  (`:676-681`, `:723-725`, `:945-948`), `startDia7a53` (`:769-771`). Entries
  started from an object record are refused (`:549-552`, `:945-947`).
  Dialog opening is refused (`:412-421`, `:442-450`, `:465-475`, `:782-802`).
- The listed entries run through the existing commands. In these levels
  `newWorld(0)` reads the data-segment string, because the research executed
  those travel entries and recorded their level (`:638-670`, `:832-848`);
  the private probe confirms the requested names. The mission 2
  null-destination refusal is unchanged for ungated levels.
- `playerHasItem` is the client's `inventory_countItemByName` over the bag
  (the item count is a real client service; `:469-474`, `:686-706`).
  `cutScene`, `activateStore`, `preloadResource`, `autoPropSetState` and,
  without a bound sound resource, `soundSequence` stay refused as before.
- `story_route::availability_entries` lists the 17 hub/hub2 entries for
  missions 4-6 (`:504-510`, `:861-880`). The hubs keep their existing policy.

## Tests and private acceptance

`story-route` (new CTest, authored fixtures: synthetic table rows, act lists
and AMX images carrying the research's entry identifiers and authored
strings): 635 checks. It covers the table refusals, story/evil/row-0
requests, the cursor and reopen rules, missions 6-12 refusal, the entry
lists, the gate on direct, nested, object-use and offset-0 calls, the
observed byte/quest/travel shapes, the item-count condition and the 17 hub
entries.

`champions-story-route-probe <disc>` on the mini's read-only disc, counts only
(`work/play-story-route-r1/probe.log`): **212/213**. Menu table 5/5, all 75
listed and 13 refused entries are publics of their shipped levels, and the
executed mission 3, 4 and 5 cases match: starts, conditions, the five islc1,
six wat1, six return and eight tormenta travels, gnome entrances, the
25-call `shardCheck` count with the client's empty bag on all six side
levels, completions on four records, and the 17 hub/hub2 availability writes.
Quest titles are compared with each other at run time and never printed.

Explained differences in that run:

- `return10c8` returns 2 with evil set: `game_getEvil` returns the masked
  value (`ref_natives.py:1191-1194`), and the client runs difficulty 1 where
  the research ran difficulty 0.
- After the observed writes the client stops at a command it still refuses:
  `soundSequence` after the `islc2`, `endtor` and `callScri8c36` grants;
  `preloadResource` after `tormenta` `preStart` writes the entrance;
  `activateStore` in hub `setMissidce3` and `setMissic4` (section 7 does not
  mention their store calls). The written bytes match the research.
- **The one failure:** `tormenta` `start` stops at the client's refused
  `autoPropSetState` before its `setMissionAvailable(5, 1)`. The research ran
  that native's original body and reported no stub. The probe records this
  as `m5-start-client-stop` 1/1. In play, mission 5 therefore cannot become
  available from its level start; porting `autoPropSetState` needs its own
  evidence and is not in this round.

## Kept refused

| Item | Reason |
|---|---|
| Menu draw `0x232300` | Faults through the display-list cursor at `0x44fe10`, pc `0x17b968` (`:289-301`). |
| Input-ready word `0x8e2f10` restorer | Unknown (`:289-301`). The client keeps reading the pad on each state-2 tick, as before. |
| Loader body, arrival | Not run (`:322-326`, `:549-552`, `:945-947`). |
| Dialog selection in mission 3-5 levels | Selector not run; reachability Inferred at best. |
| Object use / trigger dispatch in those levels | Not run. |
| Missions 6-12 in-level beats | Not established; the level is refused after the request. |
| Mission 1 | No change. Only `research-mission1-complete-fix2.md` closed claims were consulted (M1-1..M1-13); the open M1-14..M1-18 items were not used. The existing mission 1 contract stays. |

## Gates

| Gate | Result |
|---|---|
| Release `cmake --build build/native -j 4` | PASS, exit 0 (`work/play-story-route-r1/build.log`). New targets `champions-story-route-tests`, `champions-story-route-probe`. |
| GDExtensions, `template_debug`, Release, `-j 4` | PASS, exit 0 for the archive extension, level-texture and creature-animation (`ext-build.log`, `tex-build.log`, `anim-build.log`). |
| Focused CTest `story-route`, `quests`, `script-commands`, `dialog`, `mission2-route`, `mission1-scripts`, `script-items` | PASS 7/7 (`ctest-focused.log`). `story-route` 635 checks. |
| Strict Godot matrix `python3 tools/godot_matrix.py matrix --godot <mini Godot 4.5.2> --captures --timeout 1200`, stdin closed | PASS 54/54, 0 failed, including the graphical cases (`matrix.out`, `matrix/results.json`). The unchanged `check-log` classifier found 0 errors across all 105 case and engine logs. |
| Full synthetic combat | PASS, 2,645 checks, 0 failures (`matrix/playable_wave_smoke_full_combat-engine.log`). Same as wave 38. |
| FULL_SAVE_LOAD | PASS 300/300. Uninterrupted and continued presentation both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`. Same as wave 38. |
| FULL_TRACE | PASS, unchanged. 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless renderer. |
| Journeys, unchanged `tools/run_loop_playthrough.py`, headless then rendered, stdin closed, read-only disc | Headless 80 steps, 27 refused rows, rendered 81 steps, 28 refused rows, 38 captures. Pickup passed on both, inventory 10 to 11, 52 walk ticks, 0 retry kills. 8 refusal kinds on each. Godot exit 1 with 0 log errors (the retained full-journey refusal). Disc files 994 before and 994 after (`playthrough/`, `playthrough.out`). |

### Journey differences

Compared step by step with the retained mission 2 round 1 journey
(`play-mission2-route` `work/playthrough/*/result.json`). Wave 38's report
records that its step name and result pairs equal that wave-37 run at every
index. Here every pair is identical in both variants, and the refused-row
counts are the same, 27 and 28.

Refusal kinds are 8, as in wave 38. The two kinds beyond the six in the
wave-37 run are the progression hub-action refusal
(`native/src/script_dialog.cpp`, "hub action beyond documented first/second
offers is Unknown"), once from the dialog confirm and once from the tick, as
wave 38 records. No other kind's count changed. The node-3 retry count on
that kind is headless 2 and rendered 1; wave 38 recorded 2 and 3. Progression
round 3 explains this counter as the close-fade retry under the display-frame
schedule, not a second grant (`work/loop-play-progression-r3-report.md:34`),
and wave 38 gives 1-4 as the expected range.

Host-only fields moved under the shared mini (load average above 130 during
the run): headless loot `fps` 88 and rendered `fps` 2. `rendered_pickup_verified`
stays false, `player_death` stays refused and `saved_level_reloaded` stays
false, as in wave 38.

The journeys never open the mission menu past mission 2, so the new
missions 3-12 selection and the mission 3-5 gate do not change them.
