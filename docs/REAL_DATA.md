# Real game-folder measurement and play (2026-10-20)

First run of the merged wave 1–11 modules against the owner's own read-only
RTA US v2.00 disc copy (994 files). Only aggregate counts are recorded here.
No bytes, file names or decoded content are in this repository or its exports.
Earlier owner baselines in [PRIVATE_DATA_VALIDATION.md](PRIVATE_DATA_VALIDATION.md)
came from a different extraction and counting scope. Do not compare them directly.

## Tools

- `build/native/champions-real-data-measure --game rta DIR [--fx-defs F] [--fx-recipes F]`
  runs the level-texture, WORLD, level-flow manifest/objects, inventory-icon,
  audio, font, animation-decoder, skinning and FX JSON modules over a folder.
  It prints TSV counts and grouped refusal reasons only. The reasons are cut
  before any `(` or `:`, so quoted resource names never appear. Public CTests
  check only its refusals (bad title, empty folder).
- `res://scenes/real_data_play.tscn` is the play flow plus **Game folder…**,
  a level list, a creature-archive list and **Skill recipe JSON…**. It reads
  the chosen folder at run time:
  - Levels load through the real level library with their `<LEVEL>.TEX`.
  - The released mixer uses `<LEVEL>.VAT` with its `ntsc_dialog.bin`, plus the
    global bank.
  - Effect sheets come from engine/fx lumps.
  - The creature preview is an explicit model/clip choice.
  - The only copy is the level's WORLD entry. It goes to `user://real-data-cache`
    and is deleted on exit.
- `res://tests/real_data_scene.gd` drives that scene (opt-in, never CI). Pass
  `--game-folder= --cache=` and optionally `--summary= --capture-dir=
  --fx-defs= --fx-recipes= --levels=N --all --ticks=N`.

## Results (occurrence counts)

| Module | Accepted / total | Main refusal reasons |
|---|---|---|
| Validator animations | 4,510 / 4,510 | none |
| Validator models | 2,322 / 2,323 | 1 needs an external row transform |
| Validator textures (lump decoder) | 3,222 / 3,347 | 110 level TEX files sent to the lump decoder; 15 lump format refusals |
| Validator terrain | 120 / 120 | none |
| Validator WORLD geometry | 52 / 120 | 49 water, 19 moving pieces |
| Validator containers | 1,061 / 1,061 | none |
| Level TEX files (level decoder) | 111 / 111 files, 4,808 / 4,808 cells, 157,820 / 157,820 textures | none |
| Level WORLD ↔ TEX binding | 49 / 111 levels, 440,168 surfaces | 44 water, 18 moving pieces |
| Level manifest steps (disc listing + GOB + lump only) | 111 / 111 | model-lump step not supplied |
| Level objects | 111 / 111, 17,398 records | none |
| Runtime GOB walk vs stored offsets | 111 / 111 same entry count | none |
| Inventory pictures (picture cache) | 11,511 / 11,511, all sizes match the index | the other 3 caches are not picture caches |
| Audio level/global banks | 117 / 117 directories, 5,351 / 5,559 entries | 208 entries in 22 banks: invalid ADPCM block header |
| Audio nested VAG | 4,933 / 4,933 | none |
| Audio loose VAG/VA1 | 1 / 2 | non-ASCII audio name |
| Fonts | 8 / 8 read, 8 / 8 metrics | embedded page: 0 / 8 through the lump decoder (no documented page decoder) |
| Animation decoder | 4,510 parsed, 4,510 decoded at t=0 | none |
| Model/clip skinning, same lump | 21,156 / 22,939 pairs; 1,026 / 1,029 models have a skinnable clip | 1,783 pairs: influence joint exceeds transform count |
| Particle definitions JSON | 1 / 1, 362 definitions | none |
| Skill recipe JSON | 0 / 1 | the model needs a constant the tool reports as null |
| HUD art catalogue | 9 / 34 identities | 25 have no row: real used sizes differ from catalogue page sizes |
| Scene levels (library + TEX + play) | 20 / 111 | 44 water, 18 moving, 18 no ground at the start point, 11 over the mesh limit |
| Scene effect sheets | 11 / 11 textures, 0 missing | none |
| Scene creature previews | 4 / 5 archives tried | 1 archive with no accepted model/clip |

Model/clip pairs counted here only skin without errors. They are not retail
pairing evidence.

The scene was captured on 4 levels at 1280×720 (12 captures, kept only in the
gitignored `work/private/`). These rendered:
- decoded, lit textured ground
- real skinned creature models (untextured, position-only preview)
- the real level bank, with no bank errors

Lighting remains the released labelled synthetic sun/node set. One level's
special-chunk lighting is still gated, so those surfaces draw dark.

## Round 2 (2026-10-20): water, moving pieces, large levels, creature textures

Counts only, same disc copy. Evidence: research `t3/research-levels` 36036da
(`docs/formats/world.md`, `docs/level-flow.md` 11, 12, 16.4),
`t3/research-anim` (`docs/spec/water-services.md`, `docs/formats/model-texture.md`,
`docs/monsters-2.md` AutoProp section 3).

What changed:
- `world_geometry::decode` keeps water tiles (payload, documented mask bit
  order, no packet) and moving pieces (rest-pose geometry plus the piece
  table) by default. `STATIC_ONLY` keeps the v1 policy for the vector corpus.
- `champions-real-world` gives the water services a relocated read-only WORLD
  image. Ripple grids are not allocated: they are cosmetic and only exist for
  drawn tiles, so `water_test` and the `water_disturb` count stay exact.
  Pieces go through `level_flow::Pieces`: `AutoDoor`/`DoorSwing` claims in
  record order, the documented AutoDoor start state, then unclaimed pieces
  lose collision. That collision lives in the adapter's own terrain copy.
- The textured loader merges consecutive same-material batches of a chunk and
  uses explicit budgets (16,384 merged surfaces, 4 M triangles, 1 GiB RGBA).
  The session controller kept its 4,096-surface / 1 M-triangle display cap
  (`session_controller.cpp`, owned by another job). For larger levels it
  loaded a private cache copy whose extra chunks carried the documented
  not-drawn cell 0. Wave 12 raised the cap and removed that copy (below).
- The creature preview binds the model's same-base `.tex` with per-corner UVs,
  header +0x18 blending (1 and 9 only), TEST_1 alpha test and per-packet PRIM.

| Measure | Round 1 | Round 2 |
|---|---|---|
| Validator WORLD geometry | 52 / 120 | 120 / 120 |
| WORLD geometry decode, levels with TEX | 49 / 111 | 111 / 111 |
| WORLD ↔ TEX binding | 49 / 111 | 111 / 111 levels, 1,115,102 batches |
| Water tiles accepted | 0 | 9,451 tiles in 48 levels (66 use the second constant set); 9,451 / 9,451 tile centres test as water |
| Moving pieces | refused | 177 in 18 levels; 174 claimed, 3 unclaimed lose collision, 0 start open; 54 drawn chunks |
| Model ↔ same-base texture | not attempted | 2,323 / 2,323 have one; 2,312 bind; 11 refused (8 texture size, 2 IMAGE destination, 1 model row transform) |
| Model blend header +0x18 | | 1: 1,787; 9: 514; 11 other values drawn opaque (undocumented) |

Scene sweep, all 111 levels (headless, four slices, 60 ready ticks each):
- **59 / 111 levels load and play** (20 in round 1), 0 failed checks.
- Of the 39 newly loading: 23 have water, 7 have moving pieces and 31 needed
  the session copy (the kinds overlap).
- **52 are refused, all for "no ground at the current starting point".** That
  check runs after the geometry decode, so 34 water/piece levels that used to
  stop at the decode now reach it. That spawn rule is with a research job.
- Water: 6,038 tiles shown. The player is in water on 207 of 1,449 observed
  ticks, with 5 water entries.
- Display peaks:
  - 5,068 merged surfaces from 47,277 batches.
  - 1,826,822 triangles.
  - 369 MB RGBA.
  - 2.17 GB static memory in one headless process.
  - The session copies hid 42,141 chunks in total.
- Graphical run at 1280×720 (14 levels): 11 captures.
  - Creature previews 3 / 3 textured.
  - User particle definitions and skill recipe JSON both loaded.
  - Peaks: 1.06 GB static, 0.76 GB video.

Not documented at this round, so not done then:
- Water drawing constants were one labelled neutral material. Colour, FIX,
  slope UVs, the wave and layer 3 are now [WATER_DRAW.md](WATER_DRAW.md).
  The claim-frame seed and the drawer splash point stay refused.
- Model header +0x18 values other than 1 and 9, and the context-2
  environment pass.

## Wave 12 (2026-10-20): story start, level lights, fonts, raised session cap

Integration of review-fixes, real-data, level-spawn, level-lights,
ui-fonts-2, hud-fx-3, textures-2, audio-2, creature-ai and combat-runtime-3/4
on `codex/playable-integration`. Counts only, same disc copy.

What changed in the real-data scene:
- **Story start.** The level's `objects.ob` goes to
  `ChampionsSessionController.set_terrain_start_objects`. The session then
  starts at `level_flow::story_start_ground` (cold story, one player, marker
  identity search, `player_ctor_z` at radius 12; docs/LEVEL_FLOW.md section
  17) instead of the origin, and refuses a level only when that snap has no
  ground. `session_control::Config::start` carries the position; saves keep it
  (terrain marker byte 2). Terrain loaded without objects keeps the historical
  origin start and probes.
- **Session mesh cap.** `present_geometry` merges each chunk's batches into
  one surface (the neutral session display has one material) and the caps are
  now 16,384 surfaces / 4 M triangles, the textured loader's budgets. The
  private not-drawn copy and `ChampionsRealWorld.session_copy` are gone; the
  session reads the level's WORLD entry directly.
- **Level lights.** **Level lights JSON…** (or `--level-lights=`) takes the
  user's `tools/fx_level_lights.py --json` output. Each loaded level selects
  its record by its level name through `game_loadLevel`'s lookup; play then
  replaces the labelled synthetic sun, ambient and base with it.
- **Skill recipes.** **Skill recipe JSON…** (or `--skill-recipes=`) loads the
  user's recipe constants including `light_records`.
- **HUD and fonts.** The original-art catalogue uses the TEX used sizes
  (ui-fonts-2), and the folder's FNT rows are listed as an explicit HUD font
  choice. Which font the original HUD uses is not documented, so none is
  picked automatically.

| Measure | Round 2 | Wave 12 |
|---|---|---|
| Scene levels loaded and played | 59 / 111 | **108 / 111** |
| Refused: no ground at the start | 52 (origin probe) | **3** (story start, radius-12 snap) |
| Levels over the old session cap | 31 via the private copy | 7 drawn directly; 0 over the new cap |
| Story start used / marker taken | — | 108 / 95 |
| Level light record | synthetic sun on 111 | 90 named rows, 18 default row (user JSON: 90 rows) |
| HUD art identities | 9 / 34 | **34 / 34** |
| FNT pages decoded | 0 / 8 | **8 / 8** |
| Skill recipe JSON | accepted 1 / 1 | accepted 1 / 1, 3 light records; Full fixture available |

Sweep: `work/run-slices-r12.sh`, four headless slices, 60 ready ticks per
level, 0 failed checks. 9,308 water tiles shown; the player was in water on
126 of 2,835 observed ticks (2 entries). 177 pieces, 174 claimed, 52 drawn
claimed. 108 level banks found, 0 bank errors.

Session mesh cost (`prepare_terrain_file` per level, all 108 loaded levels,
gitignored probe `work/scratch/session_mesh_memory.gd`):
- Peak level: 4,307 surfaces (47,277 batches), 1,826,822 triangles.
- Headless (dummy renderer keeps the vertex copy): peak +236 MB static,
  mean 117 bytes per triangle.
- Rendered (GL compatibility): peak +125 MB static plus +66 MB video, i.e.
  36 bytes per triangle of vertex buffer.
- Bound at the new cap (4 M triangles, worst observed bytes per triangle):
  about 0.69 GB headless, about 0.42 GB rendered (static plus video).
- Whole headless scene process peak: 2.51 GB static (textured level plus
  session mesh plus audio and FX).

Captures (gitignored `work/private/captures-r12/`, 1280×720, inspected): five
levels (an arena with water, a cave, a snowfield, a dungeon with 45 claimed
pieces, a forest with water). The player figure stands on the story-start
ground in each. The cave is dark under its own light record.

Still refused or not done:
- 3 levels have no ground under the radius-12 story-start snap. level-spawn's
  measure skipped the 62 water/piece levels, so these are new evidence; the
  live `findFreeSpot` seed and object list stay refused (level-spawn
  `OpenItem`), so no search moves the start.
- Light records only replace the sun, ambient and base; the special-chunk
  light modes stay gated (docs/NATIVE_FX.md).
- The HUD font row is a user choice; FNT header +2 is still not a line height.

## Water draw (2026-10-20)

The neutral tile is replaced by the documented layer-3 surface
([WATER_DRAW.md](WATER_DRAW.md)). The wave steps on `ready_ticks`. A new
block is a zero grid because the claim-frame weights stay refused, so a real
tile stays flat and samples one texel of the water texture.

Private disc, read-only, counts only. The scene loaded the first 3 levels
that contain water (2 earlier levels had none and were not played). 223
tiles were drawn. All 3 water textures decoded at 64×64. The run had 0
failed checks. Nine captures are in gitignored
`work/private/water-draw-captures/` (play, creature, and overview for each
level). The pools are flat dark teal sheets blended over the ground. No
ripples, which is the refused claim seed. On the third level the sheet
draws over nearer rock: the documented test does not reject a pixel for
being behind existing geometry.

