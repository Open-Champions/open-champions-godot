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
