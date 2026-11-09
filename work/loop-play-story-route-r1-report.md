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
