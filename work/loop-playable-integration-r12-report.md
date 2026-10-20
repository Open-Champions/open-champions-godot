STATUS: COMPLETE

Wave 12 of job playable-integration is done. All 11 released branches are merged
and the session cap is raised with a measured memory cost. The real-data scene
wiring is done, and the 111-level sweep, both CTest runs, the dev matrix and the
export matrix all pass.
Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch
`codex/playable-integration`, local commits only (no push, PR or issue).
`~/champions-re` was not touched. The disc copy was only read. Its counts and
captures stay in the gitignored `work/private/`.
Reference-function coverage unchanged; client integration only.

## Merges (start `68e7016`)

| Branch | Tip | Merge commit |
|---|---|---|
| review-fixes | 28e6cf0 | 17be224 |
| real-data | 386b52a | 4428c6f |
| hud-fx-3 | 9a287f4 | e523927 (union of extension sources) |
| audio-2 | 46d0970 | bf1fe55 (`load_stream`: audio-2 span + review-fixes stale-entry refusal) |
| textures-2 | 2f1a547 | 90c9640 (level-draw source + real-data sources; textures-2 material with real-data shader cache) |
| creature-ai | aee703f | bd490a8 |
| combat-runtime-3 | 7f43045 | brought in by 13efdfc |
| combat-runtime-4 | faf44b6 | 13efdfc (own-class slot 27 + empty-slot return, both kept) |
| ui-fonts-2 | e8ee00e | 828346f (docs, sources and test blocks united) |
| level-lights | 2d0a58f | cf10465 |
| level-spawn | 11167ac | 371c053 |

Every tip is an ancestor of HEAD. The separate combat-runtime-3 merge was aborted
in favour of merging combat-runtime-4, which contains it. `codex/fx-trig` was not
merged (wave 13).

## Integration commits

- a0f5618: `session_control::Config::start`, an optional client start position.
  It is saved as terrain marker byte 2. Scenes without it keep the historical
  bytes, and the native-save trace digest is unchanged. New checks: replay,
  indexed snapshot and play continuation keep the start; no terrain and
  non-finite starts are refused.
- 934a7ff: Full-fixture journal operations 14/15.
  - `spawn_creature` adds one creature-ai own-machine class in slot 3.
  - `drop_potion` drops a combat-runtime-4 potion, kinds 1–3.
  - `fixture-save` proves a mid-run save continues byte-identically (76 checks).
  - Holy Shield is not offered. Absorb needs the model sample-point services, and
    `give_skill(16)` would rewrite the fixture's Holy Strike table.
- d4ecfda, controller changes:
  - `set_terrain_start_objects` makes the session start at level-spawn's
    `story_start_ground`.
  - `present_geometry` merges each chunk's batches into one surface.
  - The static mesh cap rises from 4,096 surfaces / 1 M triangles to 16,384 / 4 M.
  - New combat bindings.
- 8b1f124, integration fix: level-lights' smoke compared review-fixes'
  `shade_world` dictionary with a Vector3, so it stopped on a script error and
  never quit.
- 10ac4f6: Full-pad creature picker and Potion 1/2/3 buttons, plus the new
  `playable_wave_smoke_wave12.gd` (40 checks).
- a71f470, real-data scene:
  - per-level light records, selected when the level loads (`select_level_light`;
    `reset()` only ran when play ended, so every level got the default row);
  - the story-start objects;
  - FNT rows as an explicit HUD font choice (the original HUD font is not
    documented);
  - `--skill-recipes=` and `--level-lights=`;
  - the private not-drawn session copy and `ChampionsRealWorld.session_copy` are
    removed;
  - the sweep counts the new results.
- ade47c9 / 81b3efb, plus this commit: REAL_DATA.md, PLAYABLE_COMBAT.md,
  INTEGRATION.md.

## (a) Session cap, measured

Each `prepare_terrain_file` was measured on all 108 loaded levels with the
gitignored probe `work/scratch/session_mesh_memory.gd`.

- Peak level: 4,307 surfaces (47,277 batches) and 1,826,822 triangles. 7 levels
  are over the old cap; none are over the new one.
- Headless: +236 MB static (mean 117 B/triangle).
- Rendered GL: +125 MB static plus +66 MB video (36 B/triangle).
- Bound at the new 4 M-triangle cap: about 0.69 GB headless and about 0.42 GB
  rendered.

The private copy is no longer needed and is removed.

## (b) / (c) Real data, 111-level sweep

`work/run-slices-r12.sh` ran four headless slices, 60 ready ticks per level, with
the user's light and recipe JSON. 0 failed checks.

| | real-data round 2 | wave 12 |
|---|---|---|
| Loaded and played | 59 / 111 | **108 / 111** |
| Refused | 52 no ground at origin | **3**, all "no ground at its story start (radius-12 snap)" |
| HUD identities | 9 / 34 | **34 / 34** |
| FNT pages | 0 / 8 | **8 / 8** |
| Light records | synthetic sun | 90 named rows, 18 default row |
| Story start | origin | 108 used, 95 with a marker taken |
| Skill recipes | 1 / 1 | 1 / 1, 3 light records, Full fixture available |

- The graphical 1280×720 pass over all 111 levels gives the same 108 / 111, with
  20 captures.
- The exported app ran 12 / 12 levels.
- I inspected captures of five levels: an arena with water, a cave, a snowfield,
  a dungeon with 45 claimed pieces, and a forest with water. In each, the player
  stands on story-start ground on textured terrain. The cave renders dark under
  its own record.
- creature-ai: 7 of the 10 classes run 300 ticks beside the fight. AntQueen,
  Skeleton and Soul stop on their documented refusals.
- combat-runtime-4: potions 1–3 make their LooseFire/decal, poison-gas and
  light/decal calls. Both of these are exercised on the synthetic practice
  terrain only.

## (d) CTest

- arm64 `ctest --test-dir build/native -j 3`: **196/196**, 682 s.
- x86_64 Rosetta `ctest --test-dir build/native-x64 -j 3` (built with
  `CMAKE_OSX_ARCHITECTURES=x86_64`): **196/196**, 980 s. `player-exact` passed
  in both aggregate runs (134 s / 154 s), so nothing was rerun alone.
- Earlier, after the merges: the 65 affected tests passed.

## (e) Godot

- `python3 work/playable-wave12/run-builds.py`: 10/10.
- Dev matrix `run-godot.py`: **43/43**.
  - The three creature-anim entries first exited 1 because
    `godot/creature_anim/bin` had not been built in this worktree. After building
    `gdextension/creature_anim` they pass (32, 25 and 231 checks).
  - Full combat: 2,616 checks.
- Export:
  `Godot --headless --path godot --export-release 'macOS universal' ../build/exports/wave12-final/OpenChampions.zip`,
  then `ditto -xk`.
- `run-export.py`: **16/16** after one rerun.
  - The 36,000-tick 1280×720 run first reported 1 failure in 108,822 checks:
    the known flaky "stale clip clears cosmetic owner". Rerun alone: 108,822
    checks, 0 failures.
  - 4:3 and 800×600: 2,742 each.
  - Every `FULL_TRACE` is `8dc349c1…aa0c`.
- App: `build/exports/wave12-final/Open Champions.app`. ZIP SHA-256 `30e3ca55…0d87`.
  The main dylib `3484af97…78d0` and texture dylib `858bf674…7c51` are packaged
  byte-identical, x86_64 + arm64.

## Open / unverified

- 3 levels have no ground under the story-start snap. The live `findFreeSpot`
  seed and the real object list stay refused (level-spawn `OpenItem`), so no
  search moves the start.
- Light records replace only the sun, ambient and base; special-chunk light
  modes stay gated.
- The HUD font is a user choice; FNT header +2 is not a line height.
- The Full fixture needs flat ground at height 0 at its authored positions. It
  was not run on game-folder levels.
- Every refusal from the merged branch reports is unchanged.
- The flaky stale-clip check remains unexplained (pre-existing).
- Interaction was scripted, not manual play. No audio was heard. No
  Intel/Windows/Linux hardware was used (x86_64 is Rosetta only).
