STATUS: PARTIAL — the native player record, script binding, level text, and native player save work; the requested full real-level HUD smoke remains refused where the research does not supply the draw inputs.

# Player state, round 1 — 2026-10-21

## Branch and milestones

- Worktree: `~/Projects/open-champions/worktrees/player-state`, branch `codex/player-state`.
- Base: wave-20 `ded5aab` because `work/loop-playable-integration-r20-report.md` exists. Merged `codex/menus` (`815a8f3`) at local merge `6ad2613`. `codex/hud-overlay` (`fbffecb`) was already an ancestor of wave 20. `codex/save-import` was already present.
- Local commits: `3a819a6` native record and executable tables; `e1af13f` session/HUD binding; `5492c02` save replay correction and evidence refusals; `579e008` imported effective stats and native save verification; `2060608` updated the stale script music test expectation for the merged audio boot default. No push or PR.

## Implemented

- A typed player record carries name, class/model, base and effective stats, acknowledged and earned level, XP, gold, and raw current/maximum health and mana words. Its offsets come from research-menus `docs/player.md` and research-saves `docs/save-format.md`. New-character level 1, zero XP, 500 gold and class start stats come from the documented creation flow and the selected executable. No retail data bytes are embedded.
- `player_recalcStats` maximum health/mana formulas use the six class float immediates walked from the selected executable's documented `0x1b6740–0x1b694c` chain. XP thresholds are the executable's documented 81-word table at `0x41b558`. Invalid or missing data refuses the record. Created current health/mana remains unknown; imported saves carry their saved raw current/max fields. Native `.player` companions version 02 preserve the record alongside native session saves and require the selected executable at load.
- The session exposes record fields and thresholds to the HUD and writes the documented live record offsets into the level script host before `preStart`. The script host's gold and the observed record use +`0xe4`, the same field credited by `Gold_onTouch` in research-combat-ctors-4 `gold-run.md` (`6baf4cd`). A level script's later changes to level, XP, gold, stats and maxima are read from that live record.
- The HUD draws level text from +`0x59` at the documented anchor when the small font is available. Its pending-level glow uses the native pulse, fade and sprite placement. The XP layout receives acknowledged level, XP and executable thresholds, but its green texture remains unavailable. A real player name reaches the HUD but its colour inputs remain refused. Gold reaches the record and menus, but the HUD gold counter's documented floater/alpha gate is not produced by play.
- The original no-game-folder diagnostic front end still runs without claiming a real player record. Player companions are removed when overwriting a save with a playerless session, avoiding stale record adoption.

Changed source: `native/include/champions/player_state.hpp`, `native/src/player_state.cpp`, `native/player_state_targets.cmake`, `native/tests/player_state_tests.cpp`, `native/tests/script_commands_tests.cpp`, `native/CMakeLists.txt`, `native/src/script_commands.cpp`; `gdextension/CMakeLists.txt`, `gdextension/src/session_controller.hpp/.cpp`, `gdextension/src/ps2_save_import.cpp`; `godot/scenes/play_flow.gd`, `godot/ui/frontend_menus.gd`, `godot/ui/original_art/hud_view.gd`, `godot/ui/play_hud.gd`, and the two Godot HUD/player-state smokes.

## Verification

- Release native build and arm64 binary: passed. Initial full `ctest --test-dir build/native --output-on-failure -j 3`: **220/221**, with the sole `script-commands` failure caused by an old test expectation that music volume was unknown before a slider call. The merged audio model supplies the documented save-init 0.5 default. That test was corrected and passed alone **1/1**; final full rerun: **221/221 passed**, 853.61 seconds.
- Godot 4.5.2 dev matrix: **51/51 passed** after correcting the playerless save observation. Front-end smoke **85 checks**, menu smoke **36 checks**, targeted play HUD and UI layout smokes passed. Native player CTest passed. This checkout registers **221** native tests, including the newly added player-state test and the merged menu/front-end targets; wave 20's earlier report counted 216 at its tested snapshot.
- `FULL_TRACE`: **21,990 bytes**, SHA-256 `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, matching wave 20.
- Rendered an authored synthetic HUD at 1280×720 and inspected its centered 4:3 placement, health/mana fills, and pending-level glow through the full fade. This uses placeholder art for visual inspection; it is not a private-disc screenshot or retail pixel-parity claim.
- Real-data sweep, scripts off: **111 tried / 111 loaded / 111 played**, **230 passed / 0 failed checks**, empty refusal reasons. Scripts on: the same **111/111**, **230 passed / 0 failed**, all 111 scripts attached, **17,128 executed / 695 refused calls** and **16 refusal stops**. These deterministic counts match wave 20; no sweep count changed. All 111 levels retained the existing health bar and status frame.
- The opt-in real-character smoke created a character from the selected executable, entered a real level, and reported `player_ready=1`, `level_text=1`, `max_mana_known=1`, `gold_record_known=1`. The requested four-element HUD acceptance check failed: `experience=0`, `mana_bar=0`, `gold=0`. The test exits 1 to preserve that proof boundary. Its native player companion saved and read back; a plain native session save/load with that companion restored gold and maximum mana (`native_roundtrip=1`). Real-level native scene restore refused the existing multi-surface collision display boundary; the player companion itself read successfully.

## Commands

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 3
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/mac-template_debug -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DGODOT_CPP_DIR=<pinned godot-cpp> -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-template_debug --target champions-archive-extension -j 3
python3 work/playable-wave20/run-godot.py
bash work/run-slices-player-state.sh psr1n
bash work/run-slices-player-state.sh psr1 --scripts
<Godot 4.5.2> --headless --path godot --script res://tests/player_state_real_smoke.gd -- --disc=<owner-selected read-only folder> --cache=<ignored private cache>
```

The two copied runner scripts and all private acceptance logs are ignored `work/` files. The Godot matrix runner is a local copy of wave 20's 51-case script.

## Explicit refusals and limits

1. New-character current HP/mana initialization is not stated in the cited research. The record therefore retains an unknown current value; no real mana fill is drawn for a created character. An imported PS2 record can supply the actual current values.
2. Research-ui identifies green XP bars and their 100×6 used rect, but does not provide a measured upload/decode pairing among the 34 pinned HUD art identities. The XP layout is computed, while the texture draw is refused.
3. Name drawing needs the observed arena/player colour and blink mask. The session has no source for those inputs. The name is present in the record and script host, but no invented tint is used.
4. The HUD gold counter requires a gold floater and positive counter alpha. The created character's documented 500 gold alone does not open that gate. The live +`0xe4` field is bound; the real Gold object touch/floater is not integrated.
5. The glow is implemented for a pending level, and an authored synthetic pending-level smoke passed. A new level-1 character has no pending level, so no glow is expected in its real-level smoke.
6. Full real-level native scene restore is refused by the existing one-plain-surface presentation limit. The player companion roundtrip passes on a supported plain native session. This limit is separate from the player record serialization.

The private disc was read-only. Only aggregate counts from it are recorded here. Generated logs, temporary caches and the opt-in smoke's save files are kept outside tracked source or cleaned up. No retail assets or address guesses were added.
