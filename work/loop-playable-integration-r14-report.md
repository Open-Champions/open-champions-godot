STATUS: COMPLETE

Wave 14 of job playable-integration is done. `codex/level-spawn-2` is merged,
and the real-data sweep is now **111/111** with 0 refusals. CTest is 196/196 on
arm64 and on x86_64 under Rosetta (two Rosetta timeouts passed when rerun alone).
The Godot dev and export matrices pass.
Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch
`codex/playable-integration`, local commits only (no push, PR or issue).
`~/champions-re` was not touched. The disc copy was only read, and its counts,
logs and captures stay in the gitignored `work/private/`.

## Merge (start `037e26d`)

| Branch | Range | Merge commit |
|---|---|---|
| level-spawn-2 | 53c4883..e7ac3d5 | b7374ca (no conflicts) |

**Integration fix `ed499e2`.** The universal level-texture extension failed to
link after the merge (`objects::World` constructor, destructor and
`find_free_spot` undefined). `gdextension/level_texture/CMakeLists.txt` lists
native sources by hand and compiles `level_flow.cpp`, which now builds a World
for the mode-A search. I added `objects_lifetime.cpp`, `objects_scheduler.cpp`
and `objects_queries.cpp`. level-spawn-2 did not catch this because it reused
the old texture binaries.

## (1) Real data, 111-level sweep

`work/run-slices-r14.sh` is the wave-13 slice script with r14 output names.
It ran four headless slices, 60 ticks per level, with the light and recipe
JSON, in 3 min 52 s. All slices exited 0 with 0 failed checks.

Counts by reason:

| | wave 13 | wave 14 |
|---|---|---|
| Tried / loaded / played | 111 / 108 / 108 | **111 / 111 / 111** |
| Refused, `library-load` (no ground at the story start) | 3 | **0** |
| Refused, any other reason | 0 | 0 |
| Story ground / marker taken | 108 / 95 | 111 / 98 |
| With water / tiles shown | 45 / 9,308 | 48 / 9,451 |
| Water entries / ticks in water / observed | 2 / 126 / 2,835 | 4 / 252 / 3,024 |
| Meshes / textured | 173,142 / 173,097 | 174,456 / 174,408 |
| Light record named / default | 90 / 18 | 91 / 20 |
| Level banks found | 108 | 111 |
| Ready ticks / checks passed | 6,372 / 116 | 6,549 / 119 |

These are unchanged: HUD 34/34, fonts, pieces 177 / 174 claimed / 52 drawn
claimed, and 7 levels over the old cap. Every deterministic count equals
level-spawn-2's own sweep. Only `time.*`, `audio.frames_pushed` (pumped
per frame) and the peak static memory differ.

`champions-level-spawn-measure`, built from the integrated branch and run on the
paired archive directory, gives output byte-identical to level-spawn-2's run:
paired 111, cold 3, cold origin floor 0, ring floor 3, cold search snap 6/6,
stand-in 6/6, lattice misses 0, cold story 3, and 0 failures.

## CTest

- arm64 `ctest --test-dir build/native -j 3 --output-on-failure`: **196/196**,
  659 s (`player-exact` 118 s).
- x86_64 Rosetta, `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_OSX_ARCHITECTURES=x86_64`, then build and `ctest -j 3`:
  - 194/196 in 2,567 s. This run overlapped the sweep, the captures and the
    Godot matrices.
  - `monsters-1-exact` hit its trace subprocess's 180 s limit, and
    `skills-a-exact` hit its 120 s CTest limit.
  - Rerun alone (`ctest -R '^(monsters-1-exact|skills-a-exact)$'`), both pass:
    210 s and 116 s. So **196/196**.
  - `skills-a-exact` has little headroom under Rosetta.
- The binaries are Mach-O x86_64. `level-flow-exact` and `fx-exact` pass on both
  architectures.

## (2) Godot

- `python3 work/playable-wave14/run-builds.py`: all four universal extension
  builds pass after `ed499e2`.
- Dev matrix `run-godot.py`: **43/43**.
  - Full combat: 2,616 checks.
  - Combined FX: 1,876.
  - Wave-12 extensions: 40.
  - FX bridge refusals: 33.
  - creature-anim: 32 / 25 / 231.
  - `native_fx_visual` (GL): 4 runs, no refusals.
  - `FULL_TRACE` is `8dc349c1…aa0c`.
- Export: `Godot --headless --path . --export-release 'macOS universal'
  ../build/exports/wave14-final/OpenChampions.zip` (run from `godot/`), then
  `ditto -xk`.
- `run-export.py`: **16/16** on the first run.
  - The 36,000-tick 1280×720 Full run: 108,822 checks, 0 failures, 140 s.
  - 960×720 and 800×600: 2,742 each. Exported headless: 2,616.
  - All four `FULL_TRACE` are `8dc349c1…aa0c`.
- Exported app on real data:
  - The 12-level set: 12/12. Counts equal wave 13 except timing and audio
    pumping.
  - The three arena indices: 3/3 loaded and played, with story ground,
    0 failures.
- App: `build/exports/wave14-final/Open Champions.app`.
  - ZIP SHA-256 `8bd72823…721b`.
  - Main dylib `bfaf12dd…c0bf`. Texture dylib `0b93213e…9941`.
  - Both are packaged byte-identical, x86_64 + arm64.

## (3) Captures of the three arena levels

Graphical 1280×720 run of indices 69–71 (`--from=69 --to=72`), written to
gitignored `work/private/captures-r14/` (9 PNGs). I inspected the play and
overview images.

- Run counts: loaded 3, played 3, story ground 3, marker taken 3, checks 5,
  failures 0, ready tick 59, health full.
- All 9 PNGs are byte-identical to level-spawn-2's `captures-spawn2/`.
- Every overview shows the arena meshes, with textured terrain, the water
  patches and the outer walls. None is an empty void.
- Index 69: the camera sits low over a water patch, and the scene marks
  "in water". The figure is hard to see against the water.
- Index 70: the figure stands in a water patch among rock spires ("in water").
- Index 71: the figure is on dark rock and is small and dim.
- Correction to level-spawn-2's report: it said one figure stands in water and
  two on rock. The scene's own flags and counter (2 water entries) show two in
  water and one on rock.
- The creature close-up of index 69 is nearly black under its light record.

## Commits

- `b7374ca`: merge codex/level-spawn-2.
- `ed499e2`: link the objects sources into the level-texture extension.
- `f07bc39`: docs/REAL_DATA.md wave-14 section.
- `c23c456`: docs/INTEGRATION.md wave-14 section.
- This report.

## Open / unverified

- The live `Player1_ctor` seed stays refused. The seed-0 empty-list search is
  one of the two seeds that section 18 executed. Also still refused: the
  `findFreeSpot` class radii, the mode-A name table, `arena_pickSpawnPoint`
  with a0 = 0, and the unchecked net-peer index.
- Starting two of the three arena figures in water follows from the seed-0
  ring. Whether the original's live seed lands them there is not verified.
- The research ELF and name searches were not re-run (level-spawn-2 used the
  pinned hashes).
- Interaction was scripted, not manual play. No audio was heard. No native
  Intel, Windows or Linux hardware was used (x86_64 is Rosetta only).
