STATUS: MORE — seven pinned branch histories merged and validated; scripted enemy fight remains refused.

# Playable integration wave 26 — 2026-10-23

## Merge and conflict resolution

Started from clean wave 25 `6ba8afe`. Merged the requested commits with
`git merge --no-ff --no-edit` in order, retaining each branch's ancestors:

| Branch | Pinned tip | Local merge |
| --- | --- | --- |
| play-save | `98a3516` | `67f6944` |
| play-exits | `d554959` | `fb68c63` |
| play-visuals | `fc82db9` | `691f6cb` |
| menus | `33fbf20` | `78adace` |
| native-ui-text | `f4f770f` | `b7c6931` |
| native-horsemen | `8c2ae9f` | `b4c8481` |
| combat-ctors | `d135ddc` | `20bc67e` |

The seven named audit reports in `~/Projects/champions-re-wt/cursor-runs/audits/`
each say PASS. Conflict resolutions combined the save manifest and class-model
installation with the exit carry path; kept the newer authored menu painter;
retained both the level statistics/Player1 gates and mounted constructor; and
used the already integrated `arena_modes` executable reader for combat's
duplicate reader. `native-hud` and `play-enemies` tips are not ancestors.

Three local follow-up commits reconcile merged synthetic test APIs (`28c99b6`),
release the script host's external object references while its handle-list
heads are still alive (`350588a`), and align two stale test expectations with
the audited mounted constructor (`70b3607`). The host fix addresses a
reproducible Godot abort when a scripted sweep loaded a second level in the
same process. A two-level reproducer and the full final sweep passed after
rebuilding the extension.

## Validated results

- arm64 Release native configure/build: pass after test API reconciliation;
  final native rebuild after host teardown fix: pass.
- Four universal macOS GDExtension configure/build pairs: pass, including
  final rebuild of both session extension variants.
- First full arm64 CTest: **232/235** passed under heavy machine contention.
  `player-exact` hit its 240-second timeout; `level-objects` and
  `script-commands` had stale assertions after the merged mounted constructor
  began building and the separate on-foot row retained its Holder refusal.
  The two assertions were corrected and both tests passed in a serial retry.
  `player-exact` also passed alone in **235.98 seconds**. The final full
  arm64 aggregate on the committed updates passed **235/235** (1,286.63
  seconds); `player-exact` completed in 226.31 seconds.
- x86_64 Rosetta CTest: skipped because machine load remained above 38 on
  18 logical CPUs after arm64 completed. No x86_64 result is claimed.
- Godot dev matrix: **54/54** exit zero on the final extension. Its two
  diagnostic log entries in `scene_play_flow` and `ui_layout_smoke` are
  identical to wave 25; their exit codes are zero.
- Read-only scripts-on sweep: **111/111 tried, loaded, played** in four
  60-tick slices; 230 checks passed, zero failures and zero refusal kinds.
  It started 76 scripts; 433 objects built and 15,468 unbuilt.
- Read-only scripts-off sweep: **111/111 tried, loaded, played**; 230 checks
  passed, zero failures and zero refusal kinds.
- `FULL_TRACE`: SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  36,402 bytes, exactly wave 25. The new save, level, visual, UI and mounted
  object paths do not change this synthetic combat fixture's trace.

## Scripted playthrough

`python3 tools/run_loop_playthrough.py` ran both variants against the selected
read-only disc. The script returned 1 because `enemy_fight` remains refused;
both variants wrote all 23 step records. The disc contained **994 files before
and after**. Headless wrote zero screenshots; rendered wrote **11**. I
inspected the rendered level, dialog prompt, and changed-level captures: a
class body mesh is visible in both levels, and the dialog box is visible over
the level. The original idle animation and appearance selections remain open.

| Step | Headless | Rendered |
| --- | --- | --- |
| game_folder | opened | opened |
| boot_card | boot-card | boot-card |
| logo | soe-logo | soe-logo |
| intro | intro-movie | intro-movie |
| main_menu | main | main |
| character_class | char-class | char-class |
| character_created | playing | playing |
| first_level | entered | entered |
| player_visual | not applicable headless | class mesh |
| level_script | enabled | enabled |
| walk_and_talk | dialog open | dialog open |
| dialog_visual | not applicable headless | drawn |
| dialog_prompt | drawn | drawn |
| dialog_choice_close | closed | closed |
| enemy_fight | refused | refused |
| pause | pause | pause |
| pause_save | routed | routed |
| in_game_save | saved | saved |
| quit_to_main | main | main |
| load_and_carry | restored | restored |
| level_change | changed | changed |
| cold_load | restored | restored |
| mismatched_display | refused as expected | refused as expected |

The pause menu retains its five original entries; F5 routes to the documented
in-game Save screen. Load restored the saved level, player state, position and
campaign values, including in a fresh scene. The level change followed a
documented script action and carried the player record and campaign image.
The fight still has no observed live enemy actor or handled attack on this
route, so it is not described as combat success.

## Commands and limits

Commands run from `worktrees/playable-integration`:

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native -R '^(script-commands|level-objects)$' --output-on-failure -j 1
ctest --test-dir build/native -R '^player-exact$' --output-on-failure -j 1
python3 work/playable-wave26/run-builds.py
python3 work/playable-wave26/run-godot.py
work/run-slices-r21.sh w26on-final --scripts
work/run-slices-r21.sh w26off-final
python3 tools/run_loop_playthrough.py --godot <local Godot 4.5.2> --game-folder <read-only disc> --out work/playable-wave26/playthrough-final
```

The first merged build exposed an old arena-reader test call and an ambiguous
synthetic helper overload. Both were corrected. The first arm64 CTest run was
stopped after 28 passing cases so the teardown fix could be built; its result
is not counted as a full pass. The first scripts-on sweep aborted in one slice
while advancing to the next scripted level; the same slice and a two-level
reproducer passed after the teardown fix. Final counts above use only the
post-fix runs. Logs and private acceptance summaries are in ignored
`work/playable-wave26/` and `work/private/`.

The game folder was used read-only for acceptance; no retail bytes, private
names or decoded assets entered this report or tracked files. These checks do
not establish original-client fidelity, a campaign enemy fight, physical
Intel/Windows/Linux execution, PS2 memory-card save compatibility, or
original-client netplay interoperability.
