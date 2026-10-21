STATUS: COMPLETE

Wave 18 of job playable-integration is done. All five branches are merged, in the order given, and none was backed out: `codex/perf`, `codex/combat-runtime-6`, `codex/water-ripples`, `codex/save-import` and `codex/exports`. The three low exports audit findings are fixed.

Results, all on the first run:
- Real-data sweep: **111/111**, with scripts on and with scripts off.
- CTest: **211/211** on arm64 and on x86_64 (Rosetta).
- Godot: dev matrix **48/48**, export matrix **18/18**.
- **FULL_TRACE is unchanged at `374e40c9…2035`.**

Setup:
- Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch `codex/playable-integration`, start `c42ad2d`.
- Local commits only. Nothing was pushed, and no PR or issue was opened.
- `~/champions-re` was not touched. The disc copy was only read. Its counts and logs stay in the gitignored `work/private/`.
- netplay, creature-blend, combat-ctors, dialog and script-commands-2 were not merged.

## Merges

| Branch | Tip | Merge | Conflicts |
|---|---|---|---|
| perf (STATUS PARTIAL) | 65974a0 | 2daf360 | `real_audio.gd`, `playable_fx.gd`, plus one merge fix |
| combat-runtime-6 | 396a112 | ef6d289 | `COMBAT_RUNTIME.md` only |
| water-ripples | 14ae473 | fe01847 | `real_data.gd`, `real_data_scene.gd` |
| save-import | 7948736 | 99cb6cf | none |
| exports | a157fcc | b6a2621 | none; EXP-1..3 fixed in b5dab4c |

Each merge was checked before its commit:
- **Native build:** Release build, plus the ctests for the area the merge touched.
- **Extensions:** the extensions were rebuilt.
- **Smokes:** the affected Godot smokes were run.
- **Real data:** perf and water-ripples were also run on 12 real levels.

## How the wave-17 audit rules were kept

- **Refused commands still stop the script (SC-6).** No merged branch touches the script host. With scripts on, `scripts.refusal_stops` is 39 and `stub_refusals` is 102, the same as wave 17. Executed / refused command calls are 4,192 / 669, also unchanged.
- **AntQueen, Skeleton and Soul stay refused (CAI-4).**
  - combat-runtime-6 does not touch `spawn_class`.
  - `wave12-combat-extensions` reports "AntQueen constructor is not executed" in both the dev run and the exported run.
  - Round 6's `delete_self` passes a `DiseaseCloud_dtor` slot to `destroy()`, which still throws the audit's `DISEASE_CLOUD_DTOR_REFUSAL` (CR3).
  - In `COMBAT_RUNTIME.md`, the Holy Shield paragraph keeps the audit text: the round-4 plant, with no fixture table.
- **The per-run `user://` fixture folders are kept.** No branch touches `playable_wave_smoke*.gd`. The stale-clip flake did not fire in any run this wave.
- **AUD-4, one mixer per process, is kept.**
  - perf rebuilt `ChampionsNativeAudio` on every reset, so that the old ~100 MB copy of the global bank was freed before the next one was made.
  - That would put the process-lifetime words back to their start images on every level, which AUD-4 forbids.
  - The merge keeps `end_session()`.
- **TEX-3, constant light on the level's base level, needed a fix (2daf360).**
  - perf's streamed display called `cell_view(cell, LevelTextureMaterial.PLAY_LIGHT_BASE)`. That is the fixed base 128, and the audit deleted the constant, so the stream script would not have parsed at all.
  - `cell_view` now leaves constant-light groups uncoloured when the base is negative. The stream passes -1 and tags each group `level_constant_light` / `level_constant_base -1`.
  - `playable_fx.light_constant` then applies the level's own base and the texel × colour / 128 presentation, as it does for the full display.
  - Real data: 130 of 130 streamed constant chunks sit on their level base. Wave 17 had 1,244 of 1,244 on the full display.
- **WAT-1, no water without a decoded waterenv image, is kept.** The water-ripples result carries both `water_tiles_refused` and its new line and flag fields.
- **A wading player stays refused.** water-ripples itself removed the standing-disturb binding, and none exists after the merge.

## Exports audit lows (b5dab4c)

- **EXP-1.** `skills_b.hpp` now uses `push_macro` / `#undef` / `pop_macro` around the `Model::alloca` declaration.
  - `skills_b_tests.cpp` needed its own translation-unit `#undef`. macOS `<alloca.h>` also defines `alloca`, and the test had compiled only because of the header's leaked `#undef`. The first build after the change failed with 7 errors.
  - `zig c++ -target x86_64-windows-gnu` compiles three files: `skills_b.cpp`, the test, and a file that calls `alloca()` after including the header.
- **EXP-2.** The global autoload is removed. `play_flow.gd` adds "Your game folder…" with `_add_action`. `scene_play_flow_smoke` has a new check for it: 64 → 65 checks.
- **EXP-3.** The `BUILDS.md` wording is fixed.

## (1) Real data, 111-level sweep (`work/run-slices-r18.sh`, 60 ticks)

| Run | Wall time | Tried / loaded / played | Refused | Failed checks | Refusal list |
|---|---|---|---|---|---|
| `r18 --scripts` | 28 s | 111 / 111 / 111 | 0 | 0 | empty |
| `r18n` (no scripts) | 25 s | 111 / 111 / 111 | 0 | 0 | empty |

Wave 17 took 5 min 28 s and 5 min 41 s.

These counts equal wave 17 (`r17b` / `r17bn`):
- levels and starts: 111 story ground, 98 markers, 98 pre-search, 3 mode A;
- 6,549 ready ticks and 119 checks;
- water: 48 levels, 9,451 tiles, 48 textures, 0 tiles not drawn, 252 ticks in water;
- 177 pieces, 174 claimed;
- arena tables, lights, fonts, HUD and audio banks;
- scripts and campaign counts.

Every count that changed, and why:

| Count | Wave 17 → 18 | Cause |
|---|---|---|
| `pieces.drawn_claimed` | 52 → 14 | perf: only resident cells draw. **Expected, not a regression.** |
| `levels.meshes` / `surfaces` / `textured_meshes` | 174,456 / 174,457 / 174,408 → 6,133 / 6,134 / 6,085 | perf: cell streaming and per-cell batches. These equal perf's own sweep. |
| `lights.constant_chunks` (and `_on_level_base`) | 1,244 → 130 (all on base) | only resident cells' constant groups exist, and they are batched |
| `fx.lighting_world_calls` | 4,774,659 → 100,557 | fewer world nodes; the neutral display is not lit |
| `display.max_level_surfaces` / `max_level_rgba_bytes` | 18,826 / 1.1 GB → 0 | a streamed display has no full decode to measure |
| `water.lines_applied` / `lines_refused` / `levels_with_lines` / `levels_additive` / `level_flags_loaded` | new: 157 / 6 / 46 / 1 / 48 | water-ripples; equals its own report |
| `time.*`, `memory.static_peak_bytes`, `audio.frames_pushed` | load 815 s → 38 s summed; peak 7.6 → 2.2 GB summed | timing and memory |

Measure tools (quests, arena, level-spawn) were not rerun. No merged branch changes their inputs.

## (2) CTest

| Arch | Result | Time | Slowest tests |
|---|---|---|---|
| arm64 | **211/211**, first run | 677 s | session-facing-exact 160 s, player-exact 134 s |
| x86_64 (Rosetta, `build/native-x64`, Mach-O x86_64) | **211/211**, first run | 1,282 s | session-facing-exact 269 s, player-exact 180 s, native-save-indexed 175 s |

- There were no timeouts.
- The new tests since wave 17 are `combat-runtime-6-host`, `combat-runtime-6-exact` (6,830 checks) and `ps2-save` (328 checks).
- There were 0 compiler warnings on either arch. The existing ld duplicate-library note still appears.
- Machine load was about 15–22 from other worktrees' Godot runs.

## (3) Godot

- All four universal extension builds pass. They were rebuilt after every merge.
- **Dev matrix** `work/playable-wave18/run-godot.py`: **48/48** on the first run.
  - That is wave 17's 45 plus `level_texture_stream_smoke` (27), `fx_shade_batch_smoke` (12) and `ps2_save_import_smoke` (20 PASS).
  - Check counts equal wave 17: full combat 2,620, FX 1,876, animation 1,809, play-short 1,341, level 1,643, textures 1,909, audio 2,578, UI 1,377, combat 1,417, wave12 22, creature-anim 32 / 25 / 231.
  - The one difference is `scene_play_flow`, 64 → 65, from the EXP-2 check.
  - `native_fx_visual` had no refusals.
- **FULL_TRACE** is `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes, unchanged. combat-runtime-6 records why: no round-6 object or spell exists in the live Full fight.
- **Export:** `--export-release 'macOS universal'` → `build/exports/wave18/` (`OpenChampions.zip`, `Open Champions.app`, x86_64 + arm64).
  - ZIP SHA-256 begins `390247e4`.
  - Archive dylib `8e2ec03e…` and level-texture dylib `a0c00434…`. Both are packaged byte-identical, x86_64 + arm64.
  - No `.gob`, `.lmp` or `.iso` member is in the zip, and no `rta-disc` or `private-validation` string is in the app.
- **Export matrix** `run-export.py`: **18/18** on the first run.
  - The 36,000-tick 1280×720 Full run passed **108,826 checks with 0 failures** and 63 captures, in 126 s.
  - Exported headless passed 2,620. 960×720 and 800×600 passed 2,746 each.
  - Every FULL_TRACE is `374e40c9…2035`.
- **Exported app on real data** (60 ticks): 12/12, 12/12 with `--scripts`, and 3/3 arena levels (69–71) with `--scripts`. There were 0 failures and 0 refusals. Against wave 17 the only changes are the streamed display counts and the new water-line and flag counts.

## Regressions kept

1. **The wave-17 audit refusals are unchanged:**
   - no AntQueen, Skeleton or Soul in the Full fight;
   - own classes other than Gnaw and Skelight refuse to spawn;
   - thrown potions 1 and 2 stop a Full session;
   - there is no level or boss music without a music volume;
   - SC-4, SC-5 and SC-6 still apply;
   - the HUD overlay draws nothing.
2. **A wading player does not disturb water.** `Player_move`'s water immediates are not published.
3. **Round-6 objects and spells refuse at their destructors.** Nothing in a live session creates them.
4. **The PS2 import fills native script and item state, but no live session uses it.**

## Commits

- 2daf360, ef6d289, fe01847, 99cb6cf, b6a2621: the five merges. The perf commit includes the constant-light merge fix.
- b5dab4c: EXP-1, EXP-2 and EXP-3.
- The final commit holds `INTEGRATION.md` (wave-18 section), `REAL_DATA.md` (wave-18 section), the `LEVEL_TEXTURES.md` constant-light note, and this report.

## Open / unverified

- **perf is still PARTIAL.**
  - Peak footprint is above 1 GB on 5 of its 10 heavy levels.
  - Walk frames spike when a new cell is read.
  - The slot-reuse order and the "nearest" metric are labelled client choices.
  - Only the game-folder scene streams.
- **Not done for perf:** no exported-app frame-time run and no manual play.
- **Windows and Linux exports** were not rebuilt or executed this wave. Only the zig syntax/compile check for EXP-1 was run.
- **save-import has no real-file acceptance count**, because the disc has no saves.
- **The wave-17 open items still stand:**
  - no planted player count in the session binder;
  - no act tables;
  - no arena `Match` in the scene;
  - the `combined-level` flake that shared the old directory was not reproduced.
- **No native Intel, Windows or Linux run, and no manual play.**
