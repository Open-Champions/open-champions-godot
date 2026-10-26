STATUS: MORE — integration gates verified; the scripted journey retains bounded physical-exit and reward/pickup refusals.

# Playable integration wave 30 — 2026-10-24 to 2026-10-25

## Scope and merges

Resumed the clean `codex/playable-integration` worktree at `7647ffb`, after the earlier full-history merges of `codex/play-spells` (`947c002`), `codex/native-arena` (`b598e5b`), and `codex/native-horsemen` (`7f2f4dd`). The earlier `e7f2ef3` inventory tip had been skipped because its then-current audit was `FIX`. The spell, arena, and Horsemen fix audits were `PASS`; their merged head was verified before continuing.

The requested additional merges used `git merge --no-ff --no-edit` in this order, with all ancestors:

| Branch | Audited tip | Audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/valor-fight` | `252f34a` | `valor-fight-r2-fix.md`: `PASS` | `0698dc5f` | 11 paths |
| `codex/play-inventory` | `20a04f7` | `native-inventory-r1c.md`: `PASS` | `8d29601e` | none |

The Valor resolution retained the existing hub mission route, physical walk probe, Horsemen services, arena ray query, and HUD composition while adding the incoming shared World fight, resource and player services, WarBoar construction, and fight display. The conflicted CMake files kept the union of required targets and libraries. The player maxima conflict used the common helper with the same documented formula. The ray query kept the newer audited arena implementation. No unrelated branch was merged.

The first merged journey exposed a duplicate local declaration in the combined playthrough script; the declaration was removed and the save snapshot now reads experience and level at the actual save step. It also exposed a shared player identity fault in the hub's `onePlayerHasItem` script path. The combat player is a live `WorldAuthority` actor, while that command only needs its documented object flag word. The integration fix checks the authority's live identity, reads the flag from shared memory, and preserves the standalone host's registry check. A native test covers a live shared actor, a dead actor, and a stale identity. A focused guarded route run again opened the mission menu, selected the first story request, arrived, and carried the campaign state. The GDExtension exposes the underlying script fault on a refused exit for diagnosis. Godot generated the UID for the incoming fight display script.

## Before the additional merges (`7647ffb`)

- arm64 Release build and four universal macOS extension builds: PASS. Full `ctest --test-dir build/native --output-on-failure -j 3`: **238/238 PASS**, 1,289.50 seconds; no timeout retry.
- Godot dev matrix: **54/54** commands exited zero. Full synthetic combat: **2,640 checks**, zero failures; `FULL_SAVE_LOAD` **300/300**.
- Scripts-on and scripts-off private sweeps: each **111/111 tried, loaded, played**, **230 checks**, zero failures and zero top-level refusal kinds. The game folder remained read-only.
- `FULL_TRACE`: **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- Scripted playthrough: **31 steps** headless and **31 steps** rendered; rendered produced **17 captures**. The scripted hub/story route and save/load passed. The physical walk-in exit and Valor fight remained refused at this pre-Valor head. Both wrappers exited 1 for those bounded refusals; the disc file count was **994 before and after**.

## Final verification

The arm64 Release rebuild passed. The focused `script-builtins-exact` CTest passed **1/1** in 321.01 seconds after the shared-identity guard and its test were added. Full Release `ctest --test-dir build/native --output-on-failure -j 3` completed in **3,158.07 seconds**: **242/244 passed** in the parallel run. The only failures were Python subprocess timeouts under machine load: `session-facing-exact` at 600 seconds and `player-exact` at 180 seconds. Each was rerun alone with `-j 1` and passed: **1/1** in **364.98 seconds** and **1/1** in **314.60 seconds**, respectively. Thus all **244 distinct cases** passed across the full run and the two required serial retries; the initial parallel run itself remains recorded as 242/244.

The final guarded Godot dev matrix passed **54/54 commands**. Its full synthetic combat run passed **2,640 checks**, zero failures, with `FULL_SAVE_LOAD` **300/300**. `FULL_TRACE` stayed **36,402 bytes** at SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from the requested baseline. The new real-level fight and inventory screen are covered by the separate journey and rendered menu checks; this synthetic trace alone does not certify them.

The final guarded scripts-on and scripts-off sweeps each passed **111/111 tried, loaded, and played**, with **230 checks**, zero failures, and zero top-level refusal kinds. Each used four 60-tick slices against the read-only game folder. The rendered inventory/menu capture passed **15/15 images**. Both inventory pages and the taller-item refusal frame were inspected in the actual viewport; the refusal marker stayed inside the first grid cell. These captures are host renders, not original-client pixel comparisons.

The guarded scripted journey completed **31 steps headless** and **31 rendered**, with **19 rendered captures**. Its hub dialog, mission menu, story request and carried arrival passed. The physical exit step tried **6** eligible walk attempts with **311** trigger samples and **0** dispatches. In the separate fight diagnostic, a live enemy was damaged and died, awarding **2,100 XP** and one loot item roll; Gold stayed **500 → 500**, and the item did not become a ground object or pickup. The fight used a labelled diagnostic placement and an unarmed stand-in because the campaign's starting equipment remains unverified. Save, load, and cold load passed. The physical exit, Gold source, ground-item spawn, and pickup remain refused. Both wrappers exit 1 for that incomplete journey, with **994** disc files before and after.

The rendered mission panel, fight, loaded scene, and inventory captures were inspected. The diagnostic fight placement can expose empty background beyond the authored surface; it does not establish ordinary walking reachability or original-client visual parity.

## Guarded journey steps

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
| 18 | `mission_1_story_request` | story request queued | story request queued |
| 19 | first story arrival | arrived | arrived |
| 20 | `level_change` | changed | changed |
| 21 | `walk_in_exit_trigger` | refused | refused |
| 22 | `enemy_level` | entered | entered |
| 23 | `enemy_fight` | blocked_unported_combat | blocked_unported_combat |
| 24 | `multi_surface_level` | loaded | loaded |
| 25 | `pause` | pause | pause |
| 26 | `pause_save` | routed | routed |
| 27 | `in_game_save` | saved | saved |
| 28 | `quit_to_main` | main | main |
| 29 | `load_and_carry` | restored | restored |
| 30 | `cold_load` | restored | restored |
| 31 | `mismatched_display` | refused_as_expected | refused_as_expected |

## Commands and limits

Run from `worktrees/playable-integration`. The local matrix driver is an ignored copy of the prior 54-command driver with only the output directory changed. Raw logs, sweep summaries, disc-derived saves, and screenshots stay under ignored `work/` paths. The private folder argument below is deliberately redacted from this tracked report.

```text
git status --short --branch
git log -5 --oneline
git merge --no-ff --no-edit 252f34a
git merge --no-ff --no-edit 20a04f7
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --parallel 3
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^script-builtins-exact$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^session-facing-exact$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'
python3 -u work/playable-wave30-post/run-godot-guard.py
bash work/run-slices-r21.sh w30guard-on --scripts
bash work/run-slices-r21.sh w30guard-off
python3 -u tools/run_loop_playthrough.py --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder <read-only disc> --out work/playable-wave30-post/playthrough-guard
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --path godot --script res://tests/menu_capture.gd -- --game-folder=<read-only disc> --capture-dir=<ignored work directory> --cache=<ignored work directory>
```

An intermediate post-merge CTest was interrupted by its own exact PID after the source fix, and an initial final CTest was superseded by the additional live-identity guard. Neither is counted as a complete gate. The authoritative final full run and serial retries are the guarded CTest results described above. The game folder was read only throughout. This validation does not establish physical exit reachability, ordinary campaign equipment, Gold sourcing, spawned/picked-up loot, world-object save continuation, original-client parity, or execution on other platforms.
