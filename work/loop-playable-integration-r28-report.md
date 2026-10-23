STATUS: MORE — the Wave 28 campaign route reaches blood1 and the save/load probes pass; the separate Valor fight remains refused because there are no live combat actors.

# Playable integration wave 28 — 2026-10-23

## Merge and integration

Started from clean Wave 27 `3313467492f868e1c895d402ba4edd476ae084ce`. Used `git merge --no-ff --no-edit` on exactly the pinned tips, in order:

| Branch | Pinned tip | Merge commit |
| --- | --- | --- |
| `codex/play-route` | `7ed3f21437d74074b8a2eb26e81bd82f8a83eb69` | `880376b` |
| `codex/native-horsemen` | `2811b3c27fc905ca2342a42a76194d37d1a0e812` | `19c7691ecb7ffb5e301b5dc76d50102a23e17e6a` |

Both pinned tips are ancestors of the merged head; no other branch was merged. The final route and Horsemen audits say PASS. The route merge combined the incoming mission-row binding with Wave 27's level statistics, WORLD and first-level item setup. The playthrough conflict retained Wave 27's drawable dialog, visual, save/load and cold-load checks while placing the audited hub dialog → record-11 PushTrigger action → mission menu → first story request → blood1 arrival first. It then probes Valor separately and selects a multi-surface level for save/load. The Horsemen test conflict uses the audited 13 shared draws and expects all four on-foot rows to build. A separate `level-objects` test still expected an on-foot refusal; local fix commit `4220e4b` now checks that the documented row builds, while the untraced row still refuses. No game value was invented.

## Verification

- arm64 Release configure and native build: PASS. First full CTest: **235/236**, with only `level-objects` failing on the stale on-foot refusal assertion. After the expectation correction, focused `level-objects`: **1/1 PASS**. Final full CTest: **236/236 PASS** in 869.36 seconds, including `level-objects`, `script-commands` and `horsemen-mount-contract`. No test timed out in either full run; no serial timeout retry was needed.
- Four universal macOS GDExtension configure/build pairs: PASS. Godot dev matrix: **54/54 commands exited zero**. `scene_play_flow` still logs its missing `loaded` fixture property and `ui_layout_smoke` its wrong-arity method call, despite exit zero; these diagnostics predate Wave 28.
- Read-only scripts-on sweep: **111/111 tried, loaded and played**, 230 checks, zero failures or top-level refusal kinds. Scripts-off: the same **111/111**, 230 checks and zero failures or refusal kinds. Scripts-on built 437 script objects and left 15,464 unbuilt at their documented boundaries. Both used four 60-tick slices.
- Full-combat Godot fixture: **2,620 checks, zero failures**; `FULL_SAVE_LOAD` 300/300. `FULL_TRACE`: **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from Wave 27 and the requested `9c7922b2…` baseline. This synthetic fixture does not create the hub route or on-foot Horsemen records, so the two merges do not change it.
- Scripted playthrough on the owner's read-only disc: headless **29 steps**, rendered **29 steps** and 12 PNGs. Both processes exit 1 only because `valor_fight` is refused: zero actors before/after, attack input unhandled, no drops. Save/load, cold load, and the expected different-level collision refusal pass. The single recorded refusal kind in each run says world-object and in-flight script continuation are unsupported. Disc file count: **994 before and after**.
- Rendered dialog prompt, mission menu, blood1 arrival, Valor diagnostic, save and loaded-level captures were inspected. The host mission panel is visible on the hub; the carried character appears on the destination terrain. The save confirmation is labelled as a host layout.

## Scripted playthrough, every step

| Step | Headless | Rendered |
| --- | --- | --- |
| `game_folder` | opened | opened |
| `boot_card` | boot-card | boot-card |
| `logo` | soe-logo | soe-logo |
| `intro` | intro-movie | intro-movie |
| `main_menu` | main | main |
| `character_class` | char-class | char-class |
| `character_created` | playing | playing |
| `first_level` | entered | entered |
| `player_visual` | not_applicable_headless | class_mesh |
| `level_script` | enabled | enabled |
| `walk_and_talk` | dialog_open | dialog_open |
| `dialog_visual` | not_applicable_headless | drawn |
| `dialog_prompt` | drawn | drawn |
| `dialog_choice_close` | closed | closed |
| `mission_1_unlock` | available | available |
| `hub_trigger_mission_menu` | opened | opened |
| `mission_1_story_request` | blood1 | blood1 |
| `blood1_arrival` | arrived | arrived |
| `level_change` | changed | changed |
| `valor_fight_setup` | loaded | loaded |
| `valor_fight` | refused | refused |
| `multi_surface_level` | loaded | loaded |
| `pause` | pause | pause |
| `pause_save` | routed | routed |
| `in_game_save` | saved | saved |
| `quit_to_main` | main | main |
| `load_and_carry` | restored | restored |
| `cold_load` | restored | restored |
| `mismatched_display` | refused_as_expected | refused_as_expected |

The route checks are more specific than the row names alone: the NPC choice closed with positive native choices taken; the hub record-11 action started from a closed menu, returned `ok` and opened it; the first story request queued `blood1`; the arrival preserved the player name, class, level, XP, gold, raw maximum HP/mana, campaign image and Mission 1 availability. The mission action is an explicit selected-object host call. Physical proximity/use, exact arrival marker and live campaign combat remain unverified.

## Commands and limits

Run from `worktrees/playable-integration` (Godot 4.5.2 at the existing local runtime; the owner's disc path is omitted):

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
cmake --build build/native --target champions-level-objects-tests -j 2
ctest --test-dir build/native --output-on-failure -R '^level-objects$'
ctest --test-dir build/native --output-on-failure -j 3
python3 -u work/playable-wave28/run-builds.py
python3 -u work/playable-wave28/run-godot.py
bash work/run-slices-r21.sh w28on --scripts
bash work/run-slices-r21.sh w28off
python3 tools/run_loop_playthrough.py --godot <local Godot> --game-folder <read-only disc> --out work/playable-wave28/playthrough
```

Only aggregate private-disc counts are tracked here. Logs and captures stay under ignored `work/playable-wave28/` and `work/private/`. This verifies the bounded native/Godot client, not original-client fidelity, mounted Horsemen combat, physical Intel/Windows/Linux execution, PS2 memory-card parity or original-client netplay.
