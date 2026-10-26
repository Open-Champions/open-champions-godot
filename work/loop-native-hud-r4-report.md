STATUS: PARTIAL — HUDXF-1 and HUDXF-2 are fixed; the real-session HUD input producers remain refused because their owning runtime state and event edges are absent.

## Change and evidence

Audit `native-hud-r3-fix.md` identified the missing observed dead-bit gate and a stale gold counter during a busy floater. The play HUD now refuses pending rectangles at every HP unless `dead` is an observed Boolean, and a busy gold counter refuses an independently changed record value. Gold tick and draw timing continue during that refusal, so an idle record update can synchronize. Synthetic Godot checks cover positive and zero HP, invalid/missing/clear/set dead bits, and a direct record write during pending credit. The real-level smoke checks that a positive-HP pending rectangle is refused when the dead bit is absent.

Research `t3/research-hud-inputs` `b7374c9`, `44c6720`, `docs/spec/hud-input-producers.md` passed read-only audit with zero findings (`cursor-runs/audits/research-hud-inputs.md`). Its ordered port begins at an original per-slot `Player_run` tick with a player object, bag-2 cells and configured pad masks. This real play session owns only an isolated movement/health simulation and a separate `player_state::Record` without bag cells or a live original player object (`session_controller.cpp`, `player_state.hpp`). The menu ledger owns a separate inventory model, not the session's authoritative player inventory. Connecting a host key, empty bag, or the simulation HP to `Player_usePotion` would substitute untraced data. No potion producer is wired.

The selector requires raw local held/pressed words, nine gates, configured A/B/Use/accept masks and message dispatch against live skill records. The current play controller does not own those inputs. Gold touch and chest interactions require real object events and world positions; none are emitted by this session. The minimap requires the world texture/grid load, draw-time player reveal, original local camera, spectated object and object query/ghost list; the static WORLD renderer is not that producer. The existing pure HUD arithmetic remains available for complete observed inputs, but the real play path keeps these four producer chains refused and labelled. VU1 tiles, atlas layout, fractional reveal and scripted icon effects remain unverified and refused.

## Verification

| Check | Result |
|---|---|
| `cmake --build build/mac-template_debug --parallel 2` | PASS; archive extension rebuilt |
| Godot `res://tests/play_hud_smoke.gd` | PASS |
| arm64 Release focused CTest `-R '^hud-extras$|^player-state$|^script-commands$'` | 3/3 PASS |
| arm64 Release full CTest `-j 3` | 234/234 PASS in 817.09 seconds; no timeout rerun |
| Godot dev matrix `python3 work/native-hud-r4/run-godot.py` | 54/54 PASS |
| 111-level scripts on, `RTA_DISC=… bash work/native-hud-r1/run-slices.sh hudon-r4 --scripts` | 111 tried / 111 loaded / 111 played; 230 checks, 0 failures, 0 top-level refusals; all four slices exit 0 |
| 111-level scripts off, `RTA_DISC=… bash work/native-hud-r1/run-slices.sh hudoff-r4` | 111 tried / 111 loaded / 111 played; 230 checks, 0 failures, 0 top-level refusals; all four slices exit 0 |
| `FULL_TRACE` from `godot-combined-full-combat.log` | 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to round 3 |
| real-level HUD smoke, headless | 16/16 PASS; counts only |
| real-level HUD smoke, GL rendered | 16/16 PASS; captured frame inspected |

The GL frame shows the original status frame, health and XP over the real level. Its unsupported fields remain absent. The disc was read-only and no game bytes, extracted assets, names or decoded content were committed. Intel, Windows, original hardware timing and physical controller interaction remain unverified.

Commands executed for the checks above: `bash work/native-hud-r3/audit-fixes/run-native.sh` (`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j 4`, focused `ctest --test-dir build/native --output-on-failure -j 1 -R '^hud-extras$|^player-state$|^script-commands$'`, then full `ctest --test-dir build/native --output-on-failure -j 3`); `cmake --build build/mac-template_debug --parallel 2`; `python3 work/native-hud-r4/run-godot.py`; `../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/play_hud_smoke.gd`; `RTA_DISC=<approved read-only disc> bash work/native-hud-r1/run-slices.sh hudon-r4 --scripts`; the same command with `hudoff-r4` and no `--scripts`; and headless then GL compatibility invocations of `res://tests/native_hud_play_real_smoke.gd` with the approved read-only disc and ignored local cache/capture paths. Counts-only logs are under ignored `work/native-hud-r4/`; the image remains under ignored `work/private/`.

## Commit

`029887b` — `Refuse unobserved HUD pending and busy gold composites` (audit fixes only).
