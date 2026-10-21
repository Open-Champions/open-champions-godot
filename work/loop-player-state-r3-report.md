STATUS: FIXED — all six round-2 findings (PS2-1..PS2-6) are resolved. Arm64 CTest 227/227, Godot dev matrix 53/53, both 111-level sweeps 111/111 with zero failures, FULL_TRACE unchanged. The opt-in real-character smoke keeps its documented exit 1 for unsupported HUD elements.

# Player state round 3 — 2026-10-21

Worktree `~/Projects/open-champions/worktrees/player-state`, branch `codex/player-state`. Local commits only; nothing pushed, no PR. Audit: `~/Projects/champions-re-wt/cursor-runs/audits/player-state-r2.md`.

## Commits

| Commit | Content |
|---|---|
| `d728736` | PS2-1 provenance, PS2-3 import caps, PS2-4 `playGotItemSound` narrowing (native, written by the earlier run) |
| `2720a57` | PS2-2: record values kept apart from the simulated player's values |
| `0df43b5` | PS2-5: glow alpha stepped per displayed draw |
| this report | PS2-6 and verification record |

## Finding → resolution

| Finding | Resolution |
|---|---|
| PS2-1 MEDIUM | Resolved by an executed trace, not by refusing. Research worktree `~/Projects/champions-re-wt/research-player-state` (branch `t3/research-player-state`, commits `716f827`, `9c23686`): `tools/difftest_player_init.py` runs `player_initNewCharacter` 0x1b1d70 and the first `player_recalcStats` through `tools/ee_emu.py` in 60 cases (12 models × class start stats and four caller-supplied sets); note `docs/issues/player-init-recalc.md`. Findings: init zeroes the 0x4c30-byte record (experience 0), stores level 1 / acknowledged 1 / gold 500, copies base to effective stats; init alone leaves max HP 0 and max mana 10 (its own `recalcStats` call skips the maxima while the player object is null); the first recalc with an object present gives effective = base (starting gear has no stat modifier 7–10, figurine bonus zero) and maxima equal to the `ref_combat` formula bit for bit in 60/60 cases; it also matches `live-rig.md` 4b (46/37/24/28 → HP 32.8, mana 36.7). `create()` therefore returns that documented state; the code comment in `player_state.cpp` cites it. Current HP/mana stay unknown (`current_known` false). Not covered by the trace, and stated as such: which caller runs the first object-present recalc in play. **Not re-run in this session:** the retail executable path was blocked by the sandbox here, so the 60/60 figure is the earlier run's executed result, taken from the committed research note. |
| PS2-2 MEDIUM | `present_observation` no longer overlays record values on `hp`, `max_hp`, `mana`, `max_mana`; those stay the simulation's own values (the ones the simulation uses and the bars draw). Record values are under `record_hp`, `record_max_hp`, `record_mana`, `record_max_mana` (plus `_raw`) and `player_record_currents_known`; current ones appear only when known. No mixed-source bars remain. The glow's HP gate uses the simulation HP. Tests: `play_hud_smoke.gd` checks a `record_hp`-only observation draws no health bar; `player_state_real_smoke.gd` reads the `record_*` keys. |
| PS2-3 LOW | `import` refuses level or acknowledged level > 50 and experience > table[50] (`player_importCharacter` clamp, data-tables.md 4.4). Tests: three refusal/accept cases plus a record that levels past 50 in native play still saving (`player-state` 16 checks). |
| PS2-4 LOW | `needs_player_current` takes the player count; `playGotItemSound` is refused for unknown currents only with two or more players (script.md row 61). Each route's research source is cited in the comment. Test `test_got_item_sound_unknown_currents`: one player not refused, two players refused. |
| PS2-5 LOW | `observe()` only decides the gates, reserves the glow's painter slot and resets alpha on the menu-open event; `_draw()` calls `advance_glow()`, which steps the alpha by 4 once per displayed draw (research-ui hud-dynamic.md §2). The smoke test now drives draws explicitly and checks that repeated observations between draws do not move the alpha. |
| PS2-6 LOW | The digest in the round-2 report was malformed. Full value: `FULL_TRACE {"bytes":31622,"renderer":"headless","sha256":"4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9"}`, identical to wave 22 (`loop-playable-integration-r22-report.md` line 89). |

## Verification

- Native Release build on arm64 and full CTest (`-j 3`): **227/227 passed** (886.91 s, `work/player-state-r3-ctest.log`), run on `d728736`. Nothing under `native/` changed afterwards (`git diff d728736 HEAD -- native` is empty).
- Godot 4.5.2 arm64 dev extension rebuilt from the PS2-2 source: passed. Dev matrix `work/playable-wave22/run-godot.py`: **53/53 exit 0** (includes `play-hud`, `menu`, `frontend`, `ps2_save_import`, `script-commands`).
- FULL_TRACE from the full-combat smoke: 31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, equal to wave 22.
- Real-data 111-level sweeps (read-only disc, counts only): scripts off **111 tried / 111 loaded / 111 played**, 230 checks passed, 0 failures, 0 refusals; scripts on the same **111/111**, 230 passed, 0 failures, 0 refusals, 111 scripts attached, 0 faults. All eight slices exited 0.
- Opt-in real-character smoke (`player_state_real_smoke.gd`): folder, player, level, level text, companion read/write, plain native save/load, native roundtrip, two carried level transitions with three gold steps and three counted unknown-current refusals passed. It exits **1 by design**: `experience=0`, `mana_bar=0`, `gold=0` (XP texture, created-character mana fill and gold floater producer remain unsupported), and `load_native_stage_failed` is the existing multi-surface collision display refusal. Same as round 2; none of it is counted as passed.

## Remains

- Current HP/mana of a created character are still unknown natively (the trace shows the trimmed values equal the maxima, but the in-play caller is untraced).
- The simulated player is not linked to the character record; the original mirrors record HP into the object inside `Player_run`. Needs a documented native equivalent before bars can use record values.
- Real-character HUD elements listed above stay refused.

Private game files remained read-only. This report holds aggregate counts only; no retail bytes, names or decoded content were added to tracked files.
