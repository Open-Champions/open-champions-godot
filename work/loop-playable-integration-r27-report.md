STATUS: MORE — the three pinned Wave 27 histories are merged and the real-level save path passes; the scripted enemy fight remains refused.

# Playable integration wave 27 — 2026-10-23

## Merge

Started from clean Wave 26 `514b6b3`. Merged exactly these pinned commits, with
their ancestors, in the requested order using `git merge --no-ff --no-edit`:

| Branch | Pinned tip | Merge commit |
| --- | --- | --- |
| `codex/native-hud` | `78e0d2f` | `60462ec` |
| `codex/play-save` | `170fb63` | `281cf91` |
| `codex/menus` | `7f6dbc2` | `7e860c2` |

The HUD merge had two conflicts. `native/CMakeLists.txt` retains both the
existing UI text tests and the incoming HUD extras targets. The script Host
destructor first clears paired handle nodes from its host-owned heads, then
releases the machine and Variables while those heads still exist. The latter
combines the two audited teardown safeguards. The other merges were clean.
All three pinned tips are ancestors of the merged head. No other branch was
merged. The named final HUD, play-save and save-screen audits say PASS; their
read-only findings were checked against this merge, not treated as test runs.

## Verification

- arm64 Release native configure and build: PASS. Full CTest: **236/236 PASS**
  in 1,076.95 seconds. `player-exact` passed in 189.97 seconds; no test
  timed out, so no serial timeout retry was needed.
- Four universal macOS GDExtension configure/build pairs: PASS.
- Godot dev matrix: **54/54** commands exited zero. Its `scene_play_flow`
  missing-`loaded` fixture diagnostic and `ui_layout_smoke` wrong-arity
  diagnostic remain visible despite zero exit, as in Wave 26.
- Read-only scripts-on sweep: **111/111 tried, loaded and played**, 230 checks,
  zero top-level failures or refusal kinds. Scripts-off: the same **111/111**,
  230 checks and zero top-level failures or refusal kinds. Each used four
  60-tick slices.
- `FULL_TRACE`: **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  exactly Wave 26. The merged HUD, save and front-end paths do not alter this
  synthetic full-combat fixture.
- Integrated HUD real-level smoke: 16/16 reported gates true, exit zero.
  The rendered HUD capture was inspected: the status bars sit over the real
  level and player model; gold, mana, quick-slot and minimap inputs remain
  explicitly refused where their producers are unavailable.
- Rendered front-end capture: **24** PNGs, exit zero. Inspected the Save frame
  and empty-Save capture: real tile art is present, five numbered markers are
  visible, and only row 1 says `New Save`. The caller card cache, localized
  preview wording and native session action are labelled as host boundaries.

## Scripted playthrough

`tools/run_loop_playthrough.py` ran headless and rendered on the owner's
read-only disc. It wrote **24 steps** in each mode, zero and **11** screenshots
respectively. Both variants returned 1 because `enemy_fight` is refused: this
route has zero live combat actors and the attack input was not handled. Two
recorded diagnostic refusal kinds say world-object/in-flight script save
continuation is unsupported and one candidate exit script refused; a later
documented action did change levels. The disc contained **994 files before
and after**. The rendered level, dialog, Save and loaded-level captures were
inspected; the class mesh, dialog frame and restored multi-surface terrain are
visible.

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
| `player_visual` | not applicable | class mesh |
| `multi_surface_level` | loaded, 221 terrain elements | loaded, 221 terrain elements |
| `level_script` | enabled | enabled |
| `walk_and_talk` | dialog open | dialog open |
| `dialog_visual` | not applicable | drawn |
| `dialog_prompt` | drawn | drawn |
| `dialog_choice_close` | closed | closed |
| `enemy_fight` | refused | refused |
| `pause` | pause | pause |
| `pause_save` | routed | routed |
| `in_game_save` | saved | saved |
| `quit_to_main` | main | main |
| `load_and_carry` | restored, 221 terrain elements | restored, 221 terrain elements |
| `level_change` | changed | changed |
| `cold_load` | restored, 221 terrain elements | restored, 221 terrain elements |
| `mismatched_display` | refused as expected | refused as expected |

Warm and cold load both reloaded the selected level's WORLD terrain and TEX
display and restored player position, HP, gold and campaign fields. The
different-level collision check refused reuse of the saved image as expected.

## Commands and limits

Run from `worktrees/playable-integration`:

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
python3 -u work/playable-wave27/run-builds.py
python3 -u work/playable-wave27/run-godot.py
bash work/run-slices-r21.sh w27on --scripts
bash work/run-slices-r21.sh w27off
python3 tools/run_loop_playthrough.py --godot <local Godot 4.5.2> --game-folder <read-only disc> --out work/playable-wave27/playthrough
<local Godot 4.5.2> --path godot --script res://tests/frontend_capture.gd -- --game-folder=<read-only disc> --capture-dir=<ignored capture dir> --cache=<ignored cache dir>
<local Godot 4.5.2> --path godot --rendering-method gl_compatibility --script res://tests/native_hud_play_real_smoke.gd -- --disc=<read-only disc> --cache=<ignored cache dir> --capture=<ignored HUD PNG>
```

Logs and captures are under ignored `work/playable-wave27/` and
`work/private/`; no private bytes, proprietary content names or decoded assets
are tracked. This verifies the bounded native scene, not original-client
fidelity, campaign combat, world-object continuation, PS2 memory-card parity,
physical Intel/Windows/Linux execution or original-client netplay.
