STATUS: MORE — Wave 29 merges and bounded checks pass; the integrated physical walk-in exit and Valor fight still refuse in the real-level playthrough.

# Playable integration wave 29 — 2026-10-23

## Merge and gate

Started from clean Wave 28 `1f44b580c0bd994a049651b4914051cab4cce1e8`. Used `git merge --no-ff --no-edit` on exactly the two eligible pinned tips, in order:

| Branch | Pinned tip | Merge commit |
| --- | --- | --- |
| `codex/play-exits` | `8511500` | `2677bb7cfc637c6ea27f2b43cd1ba36fd9806ac9` |
| `codex/play-visuals` | `7215b9d` | `480c5bd8ccf60ac1fa7e68ff810dc7b66dcfd441` |

The exit conflict retained the Wave 28 hub dialog, mission menu, first-story transition and save/load journey, then placed the incoming physical Trigger walk in a separate step. The audit-passed `play-exits-r2-fix.md` and `play-visuals-r4-fix.md` both say PASS. At the third merge step `play-spells-r1-fix.md` was absent, so `947c002` was skipped. No other branch was merged.

The first integrated journey run caught a missing helper in the resolved playthrough script; the three navigation helpers were restored directly from `8511500` in local commit `e13ffbb`. The physical step now records its own result and screenshot. Its incoming `walk_targets.size() < 4` gate skipped every source on this head: each examined source exposed one eligible live walk-in Trigger. The gate now tries any nonempty eligible list. The accepted walk still did not reach a Trigger; no level request was queued, and the step remains a refusal. The rendered journey now saves the player viewport for the idle and walk pose samples so they can be inspected despite the host controls scroll position. These are local harness changes only; no game value was invented.

## Verification

- Native arm64 Release configure/build: PASS. Full arm64 CTest: **236/236 PASS** in **2,797.01 seconds**. `player-exact` passed in 547.43 seconds under its 900-second limit; no test timed out, so no serial timeout retry was needed.
- Four universal macOS GDExtension configure/build pairs: PASS. Godot dev matrix: **54/54 commands exited zero**. The Full synthetic combat smoke passed **2,624 checks, zero failures**, and `FULL_SAVE_LOAD` was **300/300**. `scene_play_flow` and `ui_layout_smoke` retained their pre-existing diagnostics despite exit zero.
- Read-only scripts-on sweep: **111/111 tried, loaded and played**, 230 checks, zero failures or top-level refusal kinds. Scripts-off: the same **111/111**, 230 checks, zero failures or refusal kinds. Both used four 60-tick slices. The read-only disc stayed **994 files before and after** the journey.
- `FULL_TRACE`: **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from the requested `9c7922b2…` baseline. The synthetic Full fixture does not construct the campaign route or physical exit, so unchanged trace does not certify those paths.
- Scripted journey: **31 steps** headless and **31 steps** rendered; both wrappers exit 1 for the physical walk-in exit and Valor combat refusals. Rendered mode saved **17 PNGs**. The rendered class model contributed 2,046 sampled viewport pixels; explicit idle and walk poses differed in 33,084 vertices and 2,008 sampled viewport pixels. The actual idle/walk viewport, mission panel, route arrival, save and loaded-level captures were inspected. The pose sampling selects class-0 clips explicitly; it does not prove automatic action selection.
- Physical Trigger step: six eligible walk attempts across six examined sources, 263 trigger samples, five navigation stalls, zero Trigger dispatches, no queued level change. Neither mode claimed entry or carry-over from that step. The earlier scripted hub transition did carry the player record and campaign image to the first story level.
- Valor remains a separate diagnostic: zero combat actors before/after, attack input unhandled, no drops. Save/load, cold load, and expected different-level collision refusal passed. One logged refusal kind in each journey says world-object and in-flight script continuation are unsupported.

## Journey steps

| # | Step | Headless | Rendered |
| --- | --- | --- | --- |
| 1 | `game_folder` | opened | opened |
| 2 | `boot_card` | boot-card | boot-card |
| 3 | `logo` | soe-logo | soe-logo |
| 4 | `intro` | intro-movie | intro-movie |
| 5 | `main_menu` | main | main |
| 6 | `character_class` | char-class | char-class |
| 7 | `character_created` | playing | playing |
| 8 | `first_level` | entered | entered |
| 9 | `player_visual` | not_applicable_headless | class_mesh |
| 10 | `player_pose_sample` | sampled | sampled |
| 11 | `level_script` | enabled | enabled |
| 12 | `walk_and_talk` | dialog_open | dialog_open |
| 13 | `dialog_visual` | not_applicable_headless | drawn |
| 14 | `dialog_prompt` | drawn | drawn |
| 15 | `dialog_choice_close` | closed | closed |
| 16 | `mission_1_unlock` | available | available |
| 17 | `hub_trigger_mission_menu` | opened | opened |
| 18 | `mission_1_story_request` | blood1 | blood1 |
| 19 | `blood1_arrival` | arrived | arrived |
| 20 | `level_change` | changed | changed |
| 21 | `walk_in_exit_trigger` | refused | refused |
| 22 | `valor_fight_setup` | loaded | loaded |
| 23 | `valor_fight` | refused | refused |
| 24 | `multi_surface_level` | loaded | loaded |
| 25 | `pause` | pause | pause |
| 26 | `pause_save` | routed | routed |
| 27 | `in_game_save` | saved | saved |
| 28 | `quit_to_main` | main | main |
| 29 | `load_and_carry` | restored | restored |
| 30 | `cold_load` | restored | restored |
| 31 | `mismatched_display` | refused_as_expected | refused_as_expected |

## Commands and limits

Run from `worktrees/playable-integration` with the existing local Godot 4.5.2 binary. Build/matrix drivers were local ignored copies of the Wave 28 scripts with only the output directory changed to `work/playable-wave29`; raw logs and private captures remain ignored.

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
python3 -u work/playable-wave29/run-builds.py
python3 -u work/playable-wave29/run-godot.py
bash work/run-slices-r21.sh w29on --scripts
bash work/run-slices-r21.sh w29off
python3 -u tools/run_loop_playthrough.py --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder <read-only disc> --out work/playable-wave29/playthrough
```

Only aggregate private-disc counts are committed. This verifies a bounded native/Godot client, not original-client fidelity, physical exit reachability, live campaign combat, PS2 card parity, mounted Horsemen combat, original-client netplay, or physical Intel/Windows/Linux execution.
