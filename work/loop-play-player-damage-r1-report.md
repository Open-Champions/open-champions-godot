STATUS: IN PROGRESS — gates being recorded.

# Player damage — round 1 — 2026-11-09

Branch `codex/play-player-damage`, from wave 37 VERIFIED `70d2de19`. Research pin `t3/research-spells-r3` `1e764d4` (audits `cursor-runs/audits/research-spells-r5.md` and `research-spells-r5-fix.md`, both PASS). Contract `research-spells-r3/docs/spec/player-spells-numeric-r3.md:121-200`, executed by `tools/difftest_spell_numeric_r3.py:440-631`.

## Gates

Mini, Release, stdin closed. Godot `~/malik-champions/runtime/Godot.app`. Tree rsynced from the MacBook, excluding `.git`, `build`, `work`, `.godot` and the extension `bin/` directories.

| Gate | Result |
| --- | --- |
| Release `cmake --build build/native -j 4` | PASS, exit 0, 140 s |
| GDExtensions, universal | PASS, exit 0, 1072 s on a loaded mini. Archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`. `lipo -archs` is `x86_64 arm64` on all five |
| Focused `player-damage` | PASS, 118 checks. Neighbours `player-death-flow`, `campaign-fight-boundaries`, `combat-runtime-host`, `combat-runtime-2-host`, `simulation-boundary` PASS |
