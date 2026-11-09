STATUS: DONE — play-levelup-hud round 1 (2026-11-09). Observed level-up, skills_screen sender and HUD producers ported from research-levelup 98a8a47 and research-hud-inputs fb24f5e. CTest 260/260, matrix 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged, journeys 80/81 with pickup 10→11.

# play-levelup-hud round 1

Branch `codex/play-levelup-hud` from wave 37 `70d2de19`. Edited and committed on the MacBook; every build, test, Godot run and journey ran on the mini in an rsynced copy (excluding `.git`, `build`, `work`, keeping `godot/.godot`, which was created by one headless editor import). Mini Godot 4.5.2, read-only disc, counts only. Logs are in ignored `work/r1/` with the mini home path replaced by `~/`.

## Review of the 17 uncommitted files left by the interrupted agent

Kept, after checking each rule against the specs:

- The `player_giveExp` loop (now `levelup_hud::give_experience`, driven by `Ledger::give_xp`): ceiling `table[80]-1`, exact words, skill word +1..5 by old level, stat byte +3 with 254→1, refill only when HP > 0, `+0x58` untouched (levelup.md:28-62; hud-live-producers.md:86-112). The level-up notice (360 frames, force 0, kept while the local menu is open; hud-live-producers.md:120-122, 174-176).
- The `skills_screen` sender (levelup.md:92-164) and its Godot binding.
- `barClipRight` zero-max saturation without a clamp, the green bar's 32-bit wrapped differences (hud-live-producers.md:48-55, 113-118), and the signed-halfword message alpha (hud-live-producers.md:188-193).
- `reveal_world` (hud-live-producers.md:146-165; tools/ref_hud_inputs.py:77-132).
- Cite comments on the existing `Tick::regen_health` / `regen_mana`, which already match hud-live-producers.md:60-82.

Changed or discarded, and why:

- **Caller-supplied `alt_skill`** replaced by the nine executed alternatives embedded by class, row kind and ident (levelup.md:121-131). A free alternative from the caller could unlock rows the original never unlocks; with the table, class 4 ident 95 / skill 18 and class 6 ident 90 / skill 86 are what they are, and skill 18 does not unlock ident 95 on class 6.
- **Order of recalc and the message-7 handler** swapped: the screen calls `player_recalcStats` at 0x1d5950, and `inventory_onSkillPoint` does not recalculate (levelup.md:160-173). The draft applied the handler first, so recalc would have seen the new passive rank.
- **Recalc without executable rules** now refuses, the same as `spend_stat`; the draft recalculated from zero class formulas.
- **`snapshot_player_record` / `restore_player_loop`** discarded: a byte toy of `save_snapshotPlayer` that set only the first of the 17 equipment pointers, called by nothing. The client path is `Ledger::save` / `load`, tested for the pending level.
- **Duplicate bar, regen, `show_message`, `tick_message` and alpha functions** in the new library discarded: `ui_layout`, `Tick`, `strings::Hud` and `hud_dynamic` already implement them; the tests now check those real implementations against the notes.
- **HUD view guard relaxation** (`hud_view.gd`, draw zero-max and negative bars) tried and reverted; see the matrix gate.
- **Build wiring** fixed: `champions-levelup-hud` is included from `menus_targets.cmake` with an include guard, so the GDExtension (which includes only `menus_targets.cmake`) links it.

## Still refused

Per-skill virtual bodies (levelup.md:155-158); inclusive cursor-box edges (levelup.md:101-103); `save_load` header, camera, quest and memory-card steps (levelup.md:195-196); VU1 tiles `0x403e20` and the `world+0x6c` atlas; scripted map icons; ammunition and thrown notices; every other `hud_showMessage` caller; the `+0x58` writer (hud-live-producers.md:235-244). Also not executed: level bytes outside 0..80 and a negative bar maximum. The play skills page does not yet read the class trees from the executable, so it does not call the sender (gap `skill-confirm-input`). The level-up message text is not known, so the notice is reported (`level_message_*`) but not drawn.

## Gates (mini, synced copy)

- Native Release configure/build `-j 4`: **PASS**, exit 0 (`work/r1/build.log`). GDExtensions: **PASS**, exit 0; archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`, all `lipo -archs` = `x86_64 arm64` (`work/r1/gdext.log`).
- Focused CTest (`levelup-hud|menus|ui-layout|strings|hud`): **PASS 7/7** (`work/r1/focused-ctest.log`). New `levelup-hud` test: 117 synthetic checks; `menus-exact` 317 checks including `observed_level_vectors`.
- Full CTest `-j 3`: **PASS 260/260**, 848.27 s, exit 0 (`work/r1/ctest.out`). Wave 37 was 259; the added test is `levelup-hud`.
- Strict Godot matrix `--captures --timeout 1200`: **PASS 54/54**, exit 0 (`work/r1/matrix/results.json`), checker 0 diagnostics. `menu_smoke` 94 checks, 0 failures. A first run (`work/r1/matrix-try1*`) failed 9 `playable_wave_smoke_*` cases: the round's HUD-view change drew zero-maximum bars, but the play scene uses a zero maximum to mean "value not available" (`unavailable play mana omitted`), and those draws produced negative `Rect2` sizes. That view change was reverted (`godot/ui/original_art/hud_view.gd` back to wave 37). The native `barClipRight` saturation stays. No test or checker was changed.
- Full synthetic combat: **2,645 checks, 0 failures** (`work/r1/matrix/playable_wave_smoke_full_combat.log`).
- FULL_SAVE_LOAD: **PASS 300/300**; continued and uninterrupted presentation SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical to wave 37.
- FULL_TRACE: **PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. No baseline edited. No change was needed to explain: the trace is the simulation path, and this round touches the menu ledger, the HUD layout helpers and a new library, not the simulation tick (`player.cpp` gained comments only).
- Journeys (unchanged `tools/run_loop_playthrough.py`, headless then rendered, stdin closed): headless **80** steps, rendered **81**, pickup **10→11** (produced, walked, picked up, 52 walk ticks), 0 log errors, 38 rendered captures, 27/28 refused rows, 6 refusal kinds, ending `player_death` refused. Disc 994 files before and after. Wrapper exit 1 is the retained full-journey refusal, as in wave 37 (`work/r1/playthrough.out`).
- Mini `build/` deleted afterwards.

No push, PR, private-disc write, edit to `~/champions-re` or `playable-integration`, or process-name kill. One stalled rsync of mine was stopped by its PID.
