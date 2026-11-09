# Mission 2 objects, round 1

STATUS: IN PROGRESS. Gates are recorded below as they finish.

Branch `codex/play-mission2-objects`, from wave 37 VERIFIED `70d2de19`. First commit
`baf36119` merges `codex/play-mission2-route` `cbcb264e` (audited PASS, merging in wave 38)
with `--no-ff`; it merged without conflicts. Code: `4b8d3110`.

Research pin: `~/Projects/champions-re-wt/research-mission2-route` `d17b46f`. Audit
`research-mission2-route-r3r4-fix.md` is PASS (0 HIGH, 0 MEDIUM, 3 LOW). Contracts:
`docs/mission2-compose.md` and `docs/mission2-round4.md`.

## Gate results

| Gate | Result |
|---|---|
| Release native `cmake --build build/native -j 4` | PASS. Fresh configure 14.1 s, build 11 min 43 s under shared load, exit 0. No warning names a new file. |
| GDExtensions, Release / `template_debug`, `--parallel 4` | PASS. Archive, level-texture and creature-animation extensions exit 0, built against the mini's pinned godot-cpp `e83fd090`. |
| Focused `mission2-objects` and `mission2-route` | PASS. 2/2. `mission2-objects` is #246 (236 checks); `mission2-route` is #245. |
