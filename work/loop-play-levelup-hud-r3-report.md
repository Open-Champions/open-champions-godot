STATUS: DONE — play-levelup-hud round 3 (2026-11-09), FIX of audit play-levelup-hud-r2 (MEDIUM LU2-1, LU2-2; LOW LU2-3..LU2-5), all five fixed, plus the optional cursor-box edge lift. CTest 259/260 under load with the one load timeout passing alone, matrix 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged, journeys 80/81 with pickup 10→11.

# play-levelup-hud round 3

Branch `codex/play-levelup-hud`, from round-2 tip `61aa59e3` (wave 37 `70d2de19`). Edited and committed on the MacBook; every build and test ran on the mini in the synced copy (`rsync -rlp --checksum`, no timestamps, then `touch` of the 14 changed files). Logs are in ignored `work/r3/` on the mini.

Re-pinned to the r3 research: `research-levelup` `8d38a0b` (`docs/spec/levelup.md`, `tools/difftest_levelup.py`, `tools/difftest_progression.py`) and `research-hud-inputs` `8f06b2d` (`docs/spec/hud-live-producers.md`, `docs/issues/claims-audit.md`, `docs/issues/native-hud.md`, `FINDINGS.md`). Every `levelup.md:` and `hud-live-producers.md:` citation in the branch was moved to the r3 line numbers with a line map built from `git diff` of the two pins; the ones that fell in changed passages (virtual bodies, cursor edges, `save_load`, the `+0x58` writer, the "Still refused" list) were rewritten by hand against the r3 text.

## Fixes

- **LU2-1 (MEDIUM)** `godot/ui/game_menus.gd` sets `g_localMenuOpen` only for the host screens that stand for the documented writers, `menu_openInventory` `0x1c15c8` and `shop_openOrStock` (claims-audit.md:103, :287; FINDINGS K2; `menu_openInventory` sets it to 1 on the owner, hud-live-producers.md:273): inventory, shop, shop bag, skills, stats, level-up. `close_menu` clears it only when closing one of those (`inventory_close` / `shop_close`). Pause and Save index do not write it. Switching from an inventory or shop screen straight to Pause or Save has no original path, so the flag becomes unknown there (new `Ledger::forget_local_menu` / `ChampionsMenuService.forget_local_menu`) and the level message stays undecided until the next inventory or shop open and close. The `_ready` comment and LEVELUP_HUD.md now name the writers; the host-screen-to-opener mapping and the closed start are labelled Inferred.
- **LU2-2 (MEDIUM)** `levelup_hud::skill_screen`: the missing-resource exit is handled like the cursor and confirm exits. When the page-5 latch would change the stored `0x4f4878` word (`latches && (held & 2) != gate`) the call refuses; otherwise it leaves the word. The levelup_hud test that locked in "missing resource leaves before the latch store" now expects the refusal, and adds the unchanged-word case (gate 2, held 2), the held-0/gate-0 case of the only probe (difftest_levelup.py:244) and page 0.
- **LU2-3** `Ledger::skill_screen` runs on the ledger's page 0 as well as page 5: difftest_levelup.py:240-243 and levelup.md:155-159 execute page 0 (no latch; a preset word suppresses). Any other page still refuses ("runs on page 0 or the level-up page 5 only").
- **LU2-4** A prerequisite whose low word is 0 and whose high word is not refuses, like the `-1` low-word case (levelup.md:162-164 give no compare width for either). A full 0 still passes on skill 60 or 61.
- **LU2-5** Row kinds other than 0 and 1 refuse, in `levelup_hud::skill_screen` and in the Godot binding (levelup.md:198-204 give only the kind-0 and kind-1 sends).
- **Cursor-box edges (optional, executed at r3)** All four edges are inside (levelup.md:139-151; tools/difftest_progression.py:362-391). `Cursor::Edge` now sends like `Cursor::Inside`. New `levelup_hud::cursor_box(panel_x, scroll, row_x, row_y, x, y)` is the 0x1d4aac-0x1d4af4 test on the raw cursor floats (`cvt.w.s` truncation toward zero, the interpreter's model). Its test runs the probe's 14 points for each of the three `(panel+0, panel+0x28)` pairs on an authored row, plus the panel-x and scroll shifts. The `skill-cursor-edges` gap is removed.

### Existing tests changed (not only added to)

Each change is the one a fix requires; nothing else was edited.

- `native/tests/levelup_hud_tests.cpp` "missing resource leaves before the latch store": replaced by "missing resource that would change the latch is refused" plus three added cases (LU2-2, as the audit asks).
- `native/tests/levelup_hud_tests.cpp` "inclusive cursor-box edges stay refused", `native/tests/menus_tests.cpp` "cursor-box edge stays refused", `godot/tests/menu_smoke.gd` "cursor-box edges stay refused": now expect the edge to send (edge lift). The menus_tests case moved to its own block so the following balance checks are unchanged.
- `godot/tests/menu_smoke.gd` gap list: `skill-cursor-edges` removed from the expected refusals and a check added that it is no longer a refusal, as round 1 did for `skill-spend-sender`.

These three refusals and the gap were added by this branch in round 1; no pre-branch test or baseline was changed.

## Gates (mini, synced copy)

- Native Release configure/build `-j 4`: **PASS**, exit 0 (`work/r3/build.log`; an incremental rebuild after the last sync recompiled `levelup_hud.cpp` only, exit 0, `work/r3/build2.log`). GDExtensions: **PASS**, exit 0; archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`, all `lipo -archs` = `x86_64 arm64` (`work/r3/gdext.log`).
- Focused CTest (`levelup-hud|menus|ui-layout|strings|hud`): **PASS 7/7** (`work/r3/focused-ctest.log`). `levelup-hud` 190 checks (137 in round 2), `menus-exact` 338 (331).
- Full CTest `-j 3`: **259/260**, 1,991.38 s (`work/r3/ctest.out`), on the final code, run alongside the matrix and the journeys. The one failure, `messages-exact`, stopped at its own 120 s subprocess timeout (`TimeoutExpired` in `tools/test_messages_vectors.py`, no assertion), as in round 2's first run. **Rerun alone: PASS**, 40.89 s (`work/r3/retimed-ctest.log`). No timeout was changed.
- Strict Godot matrix `--captures --timeout 1200` (exit status and log diagnostics as gates): **PASS 54/54**, exit 0, first run (`work/r3/matrix/results.json`). `menu_smoke` 135 checks, 0 failures (115 in round 2). It ran alongside the full CTest.
- Full synthetic combat: **2,645 checks, 0 failures** (`work/r3/matrix/playable_wave_smoke_full_combat.log`).
- FULL_SAVE_LOAD: **PASS 300/300**; continued and uninterrupted presentation SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical to rounds 1 and 2 and wave 37.
- FULL_TRACE: **PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. No baseline edited; this round changes the skill sender, the menu ledger, the binding and the host menu flag, not the simulation tick.
- Journeys (unchanged `tools/run_loop_playthrough.py`, headless then rendered, stdin closed; ran alongside the full CTest): headless **80** steps, rendered **81**, pickup **10→11** (produced, walked, picked up, 52 walk ticks), 0 log errors, 38 rendered captures, 6 refusal kinds, ending `player_death` refused. The rendered 81-step status breakdown is identical to round 2 (`work/r2/playthrough/rendered/result.json` against `work/r3/...`). Disc 994 files before and after. Wrapper exit 1 is the retained full-journey refusal, as in rounds 1 and 2 and wave 37 (`work/r3/playthrough.out`).
- Mini `build/` deleted afterwards.

## Still refused or stated as limits

Unchanged from round 2: per-skill mana-cost bodies (a skill row needs a planted float; r3 executes them for rows 1–2 but they are not ported), the `save_load` steps, VU1 tiles, scripted map icons, ammunition and thrown notices, other `hud_showMessage` callers (r3 executes several; not ported this round), alternatives off their executed chain, prerequisite kinds other than 0/1, a `-1` low word with a nonzero high word. Added this round: a 0 low word with a nonzero high word; row kinds other than 0/1; skill-screen calls on pages other than 0 and 5; resource, cursor and confirm exits that would change the latch word. Removed: the cursor-box edge refusal and the `skill-cursor-edges` gap. Stated limits: the host-screen-to-opener mapping for `g_localMenuOpen` and its closed start are Inferred, and a switch from an inventory or shop screen straight to Pause or Save leaves it undecided; the play skills page still does not read the class trees, so nothing in play calls the sender or `cursor_box` (gap `skill-confirm-input`).

No push, PR, private-disc write, edit to the research worktrees or `playable-integration`, or process-name kill. No disc text and no mini home paths in committed files.
