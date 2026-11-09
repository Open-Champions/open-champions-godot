STATUS: IN PROGRESS — gates through FULL_TRACE recorded; journeys pending.

# givePlayerItem — round 1 — 2026-11-09

Branch `codex/play-give-item`, from wave 37 VERIFIED `70d2de19`. Research pin `t3/research-items` `5a683b4`, `docs/give-player-item.md` (audits `cursor-runs/audits/research-give-item.md` and `research-give-item-fix.md`, PASS). Edited and committed on the MacBook; every build, test, Godot run and journey ran on the mini in an rsynced copy.

## Gates (mini, Release, stdin closed)

| Gate | Result |
| --- | --- |
| Release configure + `cmake --build build/native -j 4` | PASS, exit 0, about 165 s |
| GDExtensions, universal | PASS, exit 0. Archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`; `lipo -archs` `x86_64 arm64` on all five, built fresh this run |
| Focused `give-player-item` | PASS, 87 checks, 0 failures |
| Full CTest `-j 3` | PASS, **260/260**, 874.74 s (259 + `give-player-item`) |
| Strict Godot matrix `--captures`, timeout 1200 | PASS, **54/54**; editor import exit 0; `check-log` 0 flagged of 105 logs |
| Full combat | 2,645 checks, 0 failures |
| FULL_SAVE_LOAD | 300/300, continued = uninterrupted `cb2309ab…d8573` |
| FULL_TRACE | unchanged, 36,402 bytes, `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` |

Journeys: pending.
