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
  ground. When a marker took, that start is the marker before
  `Player1_ctor`'s `findFreeSpot` (`start.pre_search` is true, up to 36 units
  from the original) and no `start.facing` is published. `session_control::Config::start` carries the position; saves keep it
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


## Wave 14 (2026-10-20): mode-A arena start, 111 / 111

Integration of level-spawn-2 on `codex/playable-integration`. Counts only, same
disc copy.

What changed: a cold story start whose radius-12 snap has no floor, with no
Player1 that took and with an `ArenaPlayerStart`, now uses the mode-A wave
start (`level_flow::story_start_ground`; docs/LEVEL_FLOW.md, research
`docs/level-flow.md` section 18): ring radius 180, slot 0, count 1,
`findFreeSpot` spread 36 then 240 with an empty object list and seed 0, then
`player_ctor_z` at radius 12. Levels that already had story ground keep it.

| Measure | Wave 13 | Wave 14 |
|---|---|---|
| Scene levels loaded and played | 108 / 111 | **111 / 111** |
| Refused (all reasons) | 3 (`library-load`: no ground at the story start) | **0** |
| Story ground / marker taken | 108 / 95 | 111 / 98 |
| Levels with water / water tiles shown | 45 / 9,308 | 48 / 9,451 |
| Water entries / ticks in water (observed) | 2 / 126 (2,835) | 4 / 252 (3,024) |
| Meshes / textured | 173,142 / 173,097 | 174,456 / 174,408 |
| Level light record | 90 named, 18 default | 91 named, 20 default |
| Level banks found | 108 | 111 |
| Ready ticks / checks passed | 6,372 / 116 | 6,549 / 119 |

Sweep: `work/run-slices-r14.sh`, four headless slices, 60 ready ticks per
level, 0 failed checks, 3 min 52 s. Every deterministic count equals
level-spawn-2's own sweep; only `time.*`, `audio.frames_pushed` (pumped once per
process frame) and peak static memory differ. HUD 34 / 34, pieces 177 / 174
claimed / 52 drawn claimed, unchanged.

`champions-level-spawn-measure` on the paired archive directory prints the same
lines as level-spawn-2's run: paired 111, cold 3, cold origin floor 0, start
floor 3, ring floor 3, cold search snap 6 / 6, stand-in 6 / 6, lattice misses 0,
cold story 3, decode and stand-in failures 0.

Exported app (`build/exports/wave14-final`): the 12-level run matches wave 13
except timing; the three arena indices load and play 3 / 3 with story ground and
0 failures.

Captures (gitignored `work/private/captures-r14/`, 1280×720, inspected): the
three arena levels (indices 69–71). They are byte-identical to level-spawn-2's
captures. Each overview shows the arena meshes; health is full at ready tick 59.
Two figures start in a water patch (the scene's in-water label is set, 2 water
entries) and one on rock. level-spawn-2's report described one in water and
two on rock; the in-water count here is from the scene's own counter.

Still refused or not done:
- The live `Player1_ctor` seed, the `findFreeSpot` class radii, the
  mode-A name table and `arena_pickSpawnPoint` with a0 = 0 stay refused; the
  seed-0 empty-list search is one of the two seeds section 18 executed.
- Light records only replace the sun, ambient and base; the special-chunk light
  modes stay gated (docs/NATIVE_FX.md).

## Wave 15 (2026-10-20): level scripts on real levels, 111 / 111

Integration of script-commands on `codex/playable-integration`. Counts only,
same disc copy. The level-script host (docs/SCRIPT_COMMANDS.md) is opt-in: the
real-data scene's "Level script (opt in)" checkbox or `--level-script`, and
`godot/tests/real_data_scene.gd --scripts` for the sweep. Without it no host is
created and no script runs.

Sweep: `work/run-slices-r15.sh r15 --scripts`, four headless slices, 60 ready
ticks per level, light and recipe JSON as in wave 14. Two slices were cut short
by a harness time limit while the machine was heavily loaded by other
worktrees. Those two were rerun whole. All four final slices exited 0 with 0
failed checks.

| Measure | Wave 14 | Wave 15 (scripts on) |
|---|---|---|
| Scene levels loaded and played | 111 / 111 | **111 / 111** |
| Refused (all reasons) | 0 | 0 |
| Story ground / marker taken, water, meshes, lights, banks, HUD, pieces | as in wave 14 | equal |
| Ready ticks / checks passed | 6,549 / 119 | 6,549 / 119 |

Every count outside the host equals wave 14. Only `time.*`,
`audio.frames_pushed` (pumped per process frame, and the machine was loaded)
and peak static memory (+0.4 MB) differ.

Script counts over the 111 levels:

| | Count |
|---|---|
| Scripts attached | 111 |
| Started (`preStart` or `start` defined) | 76 |
| Reached a wait / reached completion | 41 / 37 |
| Faults / missing entries | 0 / 0 |
| Objects registered / refused | 17,398 / 0 |
| Commands executed | 22,371 across 38 names |
| Commands refused | 1,375 across 28 names |

Started, wait, completion and object counts equal
`champions-script-commands-measure --game rta`, which the integrated build
reproduces exactly: 1,681 executed across 36 names, 529 refused across 27.
The call counts differ because the play session resumes deferred names on every
one of the 60 ready ticks, while the measure tool stops when the deferred name
table repeats. Polling scripts therefore repeat the same commands. Three names
appear only in play: `findDistance` and `closestPlayerDist` (executed) and
`stopFollowingPath` (refused).

Executed (levels / calls): `addParticleOnObject` 9 / 5,960, `gamePaused`
23 / 3,222, `addDeferedCall` 41 / 2,597, `cos1000` 5 / 2,552, `sin1000`
5 / 2,552, `getRand` 10 / 2,161, `getv` 53 / 1,222, `countPlayersAlive`
10 / 561, `getScaledTime` 7 / 533, `callScript` 24 / 263, `setv` 50 / 175,
`hideMonster` 41 / 138, `zeroPlayerMana` 2 / 118, `getHealthUsed` 1 / 60,
`postAction` 27 / 39, `setSaveInhibit` 27 / 27, `getNumPlayers` 14 / 25,
`showOnMap` 4 / 24, `setMissionAvailable` 23 / 23, `getEvil` 16 / 19,
`findDistance` 4 / 15, `showMonster` 15 / 15, `closestPlayerDist` 1 / 14,
`getMissionComplete` 10 / 13, `setPlayerFlag` 3 / 9, `addQuest` 8 / 8,
`friendly` 1 / 6, `grantMission` 4 / 4, `getMadeDecision` 3 / 3,
`increaseEnemyRank` 1 / 3, `clearAllSkills` 2 / 2, `getMissionAvailable` 1 / 2,
and 1 / 1 each for `explodePotionsOnly`, `getPlayerFlag`, `moveMonster`,
`removeQuest`, `setEntrance` and `startWithSwoopDone`.

Refused (levels / calls): `showBust` 9 / 502, `countQuestItems` 12 / 306,
`setMonsterVal` 13 / 94, `playerHasItem` 16 / 91, `onePlayerHasItem` 2 / 72,
`removePlayerItem` 4 / 47, `setCreatureParams` 11 / 45, `rootMonster` 4 / 30,
`setCFlag` 3 / 23, `cutScene` 17 / 18, `followPath` 5 / 16,
`autoPropSetState` 6 / 15, `setParticle` 13 / 14, `deleteMonster` 2 / 12,
`generateMonster` 5 / 12, `addHelpMessage` 9 / 10, `stopFollowingPath` 1 / 10,
`savePlayer` 9 / 9, `setAlternateDeath` 9 / 9, `saveCheckPoint` 8 / 8,
`setWeaponType` 5 / 8, `givePlayerItem` 1 / 7, `preloadResource` 7 / 7,
`charmMonster` 3 / 4, `revealMap` 3 / 3, and 1 / 1 each for `removeAllItems`,
`removeAllWeapons` and `startDialog`.

The scene's own path was checked separately (untracked
`work/playable-wave15/scene_path_check.gd`, three level indices). With
`--level-script`, `load_real_level` attached the script on 3 / 3 levels, and
2 of them started it, with 0 faults. Without the flag, 0 / 3 attached and no
counts were available. Both runs had 0 failures.

Still refused or not done:
- Every partly documented or unknown command returns 0 (the refusals above:
  combat, creature AI, audio, HUD text, items, cutscenes, paths).
- Script effects stay inside the host. `hideMonster`, `showMonster` and
  `moveMonster` move the host's own object world. `setEntrance` sets the host's
  own level-flow state. `addParticleOnObject` spawns on the host's headless
  effect world. None of these reaches the drawn level, the Godot effect
  presenter or the play observation.
- The post action runs once at level load, not again after a cutscene, dialog
  or help screen.

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


## Wave 16 (2026-10-21): drawn water on real levels, 111 / 111

Integration of water-draw on `codex/playable-integration`. Counts only, same
disc copy. Water tiles now use the documented layer-3 surface
([WATER_DRAW.md](WATER_DRAW.md)) in place of the neutral tile.

Sweep: `work/run-slices-r16.sh`, four headless slices, 60 ready ticks per
level, light and recipe JSON as in wave 14. It ran twice: `r16 --scripts`
(3 min 39 s) and `r16n` with no level script (3 min 44 s). All eight slices
exited 0 with 0 failed checks.

| Measure | Wave 15 | Wave 16, scripts on | Wave 16, scripts off |
|---|---|---|---|
| Scene levels loaded and played | 111 / 111 | **111 / 111** | **111 / 111** |
| Refused (all reasons) | 0 | 0 | 0 |
| Levels with water / water tiles shown | 48 / 9,451 | 48 / 9,451 | 48 / 9,451 |
| Water textures decoded / refused | not drawn | 48 / 0 | 48 / 0 |
| Meshes / textured | 174,456 / 174,408 | 174,456 / 174,408 | 174,456 / 174,408 |
| Mesh surfaces | 174,456 | 174,457 | 174,457 |
| Ready ticks / checks passed | 6,549 / 119 | 6,549 / 119 | 6,549 / 119 |

Each decoded water texture is 64×64. The one extra surface is a level whose
water uses both documented sets (set 1 is chunk flag `0x1000`). Its one water
mesh now has a surface per set, where the neutral mesh had one. The mesh count
is unchanged. Every other deterministic count equals wave 15 with scripts on
and wave 14 with scripts off, including every script count. Only `time.*`,
`audio.frames_pushed` and peak static memory (+26 MB summed over the four slices) differ.

Water entries and ticks in water stay 4 and 252 of 3,024 observed. Drawing
does not touch the creature `water_test` grid.

The level whose water uses both sets is index 20. It was found with one
headless run per level over indices 0–27, comparing each level's mesh count
with its surface count.

Captures (gitignored `work/private/captures-r16/`, 1280×720, windowed,
inspected): index 20, plus the first 3 water levels from index 28 on
(indices 28, 29 and 30). water-draw's own captures covered the first 3 water
levels from index 0. Play, creature and overview for each: 12 PNGs, 0 failed
checks. The windowed run from index 28 went on to play all 35 water levels in
28–110, with 0 failures and 35 textures decoded.

| Index | Tiles | What the captures show |
|---|---|---|
| 20 | 48 (both sets) | Bright orange sheets over a round pit and one side patch. Texture detail is visible up close. Where two tiles overlap, the overlap is brighter, because each tile blends on its own. |
| 28 | 37 | Small pale grey-green patches in the overview. None is in the play camera's view. |
| 29 | 11 | One small grey-green patch in the overview. Not in the play view. |
| 30 | 772 | Many orange square sheets across the level. Up close they are flat with faint texture and hard square edges. One sheet draws over nearer rock. |

None shows a ripple. Every tile is flat, as expected while the claim seed is
refused.

Exported app (`build/exports/wave16-final`, headless): the 12-level set, with
and without `--scripts`, and the three arena indices with `--scripts`. Each
equals wave 15 apart from timing, plus the new water-texture counts (5, 5 and
3 decoded at 64×64, 0 refused).

Still refused or not done:
- No ripples. The claim-frame seed is refused, so every block starts as a zero
  grid and stays flat. A real tile samples one texel of the water texture.
- The drawer's splash point and the simulation splash stay refused, so nothing
  disturbs the drawn grid.
- Water draws without a depth test, as documented, so a pool can draw over
  nearer rock.

## Audit fixes, level spawn (2026-10-21): 108 / 111

Audit findings LS-1, LS-2, LS2-1..3 (docs/LEVEL_FLOW.md). Counts only, same
disc copy.

What changed:
- A story start whose `Player1` took is the marker before `Player1_ctor`'s
  `findFreeSpot` (`start.pre_search`), and no `start.facing` is published:
  the live `g_randSeed` there is not documented.
- `story_start_ground` no longer infers the mode-A arena start from the
  records (no floor, no Player1, an `ArenaPlayerStart`). The original reaches
  those three archives only through the mode-A table, which the session does
  not read and does not select, so they are refused again. The explicit
  `level_flow::arena_mode_a_start_ground` (pre-search ring centre) is not
  called by the session.

Sweep: four headless slices, no level script (the wave-16 control recipe),
60 ready ticks. Compared with wave 16's control sweep:

| Measure | Wave 16 control | Audit fixes |
|---|---|---|
| Scene levels loaded and played | 111 / 111 | **108 / 111** |
| Refused (all reasons) | 0 | 3 (`library-load`: no ground at the story start) |
| Story ground / marker taken / pre-search | 111 / 98 / — | 108 / 95 / 95 |
| Levels with water / water tiles shown | 48 / 9,451 | 45 / 9,308 |
| Checks passed / failed | 119 / 0 | 116 / 0 |

Every other difference is the three refused levels' meshes, lights, water
and ticks; the wave-13 108-level counts are back.

`champions-level-spawn-measure`: paired 111, origin-test 18 with story
ground 18, cold 3, cold story 0 (was 3), cold mode-A pre-search ground 3;
every other line unchanged.

## Wave 17 (2026-10-21): audit fixes, quests and arena merged, 111 / 111

`codex/audit-fixes`, `codex/quests` and `codex/arena` are merged. Counts
only, same disc copy, same four-slice recipe (`work/run-slices-r17.sh`, 60
ready ticks, light and recipe JSON).

What changed for real data:
- The three cold arena archives load again, now through the documented path
  rather than the structural guess the audit removed (LS2-2). The scene reads
  the arena name tables from the folder's executable at run time
  (`arena.tables_read`; 13 and 7 entries; nothing is stored). A level whose
  story snap has no ground starts with `level_flow::arena_mode_a_start_ground`
  only when the mode-A table lists it (research-levels 13fafa4 section 18.1).
  On this disc that is the same three indices (`start.arena_mode_a` 3). Their
  start is the pre-search ring centre with no facing.
- Every taken story marker is pre-search (`start.pre_search` 98, LS-1), so
  no start facing is published.
- Constant-light chunks are lit on the level's base level (TEX-2/3): 1,244
  chunks, base 0x80 on 108 levels and 0x40 on 3.
- With scripts on, the audit's script-host fixes (SC-1..7) and the quests
  merge apply: an entry enters preStart, objects, start; a used refused
  answer stops the entry; `getRand` is refused because the play session binds
  no seed; every objects.ob record is a stub, so a command that reads a
  creature flag, hit points or a type name on one is refused (`setCFlag` now
  too). The quest campaign is reported under `campaign.*`.

| Measure | Wave 16 (`r16` / `r16n`) | Wave 17 (`r17 --scripts` / `r17n`) |
|---|---|---|
| Tried / loaded / played | 111 / 111 / 111 both | **111 / 111 / 111 both** |
| Refused levels / failed checks | 0 / 0 | 0 / 0 |
| Story ground / marker taken / pre-search / mode A | 111 / 98 / — / — | 111 / 98 / 98 / 3 |
| Meshes / surfaces / textured | 174,456 / 174,457 / 174,408 | same |
| Water levels / tiles / textures decoded | 48 / 9,451 / 48 | same |
| Ready ticks / checks passed | 6,549 / 119 | same |
| Scripts attached / started / faults / objects | 111 / 76 / 0 / 17,398 | same |
| Scripts wait / complete | 41 / 37 | 14 / 28 |
| Refusal stops / stub refusals | — | 39 / 102 |
| Executed / refused command calls | 22,371 / 1,375 | 4,192 / 669 |
| `getRand` executed / refused | 2,161 / 0 | 0 / 5 |
| Campaign: quest variables (non-zero) | — | 158 (56) |

The table is the final tree (`r17b --scripts` 5 min 28 s, `r17bn` 5 min 41 s).
Those runs equal the first wave-17 sweep on every count apart from timing.
The scene's refusal list is now empty: the first sweep listed "level light
refused: loaded level name unknown" for every level, a stale entry from
`fx.reset()` that is cleared now; all 111 levels applied their light record.

Other wave-17 counts: `campaign.quest_lines`, the mission bits and the menu,
medal and experience events are 0 in 60 ticks. The session's binder has no
planted player count, and the measure tool plants one (QUESTS.md).

`champions-quests-measure`, `champions-arena-measure` (identical to the arena
branch's run) and `champions-level-spawn-measure` (paired 111, cold 3, cold
start floor 3) were rerun on this tree. Their counts are in QUESTS.md,
ARENA_MODES.md and above.

Exported app (`build/exports/wave17-final`, headless, 60 ticks): the 12-level
set with and without `--scripts`, and indices 69–71 with `--scripts`: 12 / 12,
12 / 12 and 3 / 3, 0 failures, 0 refusals. Against wave 16 the only changes
are the arena-table read, `start.pre_search`, the constant-light counts, the
three mode-A starts and, with scripts, the script-host refusals above.

## Wave 18 (2026-10-21): perf, combat round 6, water ripples, PS2 import and exports merged, 111 / 111

`codex/perf`, `codex/combat-runtime-6`, `codex/water-ripples`,
`codex/save-import` and `codex/exports` are merged. Counts only, same disc
copy, same four-slice recipe (`work/run-slices-r18.sh`, 60 ready ticks,
light and recipe JSON).

What changed for real data:
- **Streamed level display (perf).** The game-folder scene keeps only the
  3 × 3 texture cells around the camera focus, batches order-independent
  chunks per cell, and holds world lighting while the neutral session display
  is briefly published. Display counts fall; every deterministic count
  (levels, starts, ready ticks, checks, water tiles, pieces, scripts) equals
  wave 17. Constant-light chunks on a streamed display take the loaded
  level's base level, as on the full display (fixed in the merge; see
  INTEGRATION.md wave 18).
- **Drawn claimed pieces 52 → 14** is expected: only resident cells draw.
  The claimed count is still 174.
- **Water lines and level flags (water-ripples).** The level flag word is read
  from the level-lights row (48 water levels). 46 water levels carry level
  water lines: 157 applied, 6 refused (a fix outside ±255, whose kept
  `ALPHA_1` bits the pin does not give). 1 level draws additive (negative
  fix). On this disc no row sets flag 0x8 or 0x10, so the mask fill and the
  drawer splash stay off as a measurement. A wading player does not disturb
  the water (refused: `Player_move`'s immediates are not published).
- combat-runtime-6 and save-import add nothing the sweep reaches: no
  round-6 object or spell exists in a level session, and the disc holds no
  memory-card saves.

| Measure | Wave 17 (`r17b --scripts` / `r17bn`) | Wave 18 (`r18 --scripts` / `r18n`) |
|---|---|---|
| Tried / loaded / played | 111 / 111 / 111 both | **111 / 111 / 111 both** |
| Refused levels / failed checks / refusal list | 0 / 0 / empty | 0 / 0 / empty |
| Story ground / marker taken / pre-search / mode A | 111 / 98 / 98 / 3 | same |
| Ready ticks / checks passed | 6,549 / 119 | same |
| Water levels / tiles / textures decoded / tiles not drawn | 48 / 9,451 / 48 / 0 | same |
| Pieces / claimed / drawn claimed | 177 / 174 / 52 | 177 / 174 / **14** |
| Meshes / surfaces / textured | 174,456 / 174,457 / 174,408 | 6,133 / 6,134 / 6,085 |
| Constant-light chunks / on level base | 1,244 / 1,244 | 130 / 130 |
| World lighting calls | 4,774,659 | 100,557 |
| Water lines applied / refused; levels with lines / additive; flag word loaded | — | 157 / 6; 46 / 1; 48 |
| Scripts attached / started / faults / objects | 111 / 76 / 0 / 17,398 | same |
| Scripts wait / complete / refusal stops / stub refusals | 14 / 28 / 39 / 102 | same |
| Executed / refused command calls | 4,192 / 669 | same |
| Load time summed over slices (scripts on / off) | 815 s / 906 s | 38 s / 35 s |
| Wall time (scripts on / off) | 5 min 28 s / 5 min 41 s | 28 s / 25 s |

`display.max_level_surfaces` and `display.max_level_rgba_bytes` read 0: a
streamed display has no full decode to measure. The summed static peak fell
from 7.6 GB to 2.2 GB. `audio.frames_pushed` follows the shorter run time.

Exported app (`build/exports/wave18`, headless, 60 ticks): the 12-level set
with and without `--scripts`, and indices 69–71 with `--scripts`: 12 / 12,
12 / 12 and 3 / 3, 0 failures, 0 refusals. Against wave 17 the only changes
are the streamed display counts above and the new water-line and flag
counts.

## Creature blend audit fixes (2026-10-21)

Audit BLEND-1..6 (`codex/creature-blend`), checked against research
`t3/research-textures` 1528708: `docs/spec/render-pipeline.md` 4.7, 6 and
9.6, and `docs/formats/model-texture.md`.

- The environment UV is computed per vertex, as program 0x44 does (4.7, V):
  `(192 + 64 c.x, 192 + 64 c.y) / 128` with `c` the camera-space unit normal
  × 0.99, interpolated across the triangle. 0.99 is the figure 4.7 states;
  the research gives no further digits.
- The environment pass is drawn only with header flag 0x10, every packet of
  the mesh carrying the second tag, and header `+0x20` at most 0x80. A FIX
  above 0x80 in `+0x18` is gated like an unlisted equation.
- **Assumption (I, 9.6):** the preview's environment pass covers every pixel
  of the mesh and the composite writes depth once, so a texel that fails
  `TEST_1` shows the environment sample. The `TEST_2` and `ZBUF` the
  original's context-2 model pass runs with are not documented (model setup
  writes `TEST_1` only).

Private disc, read-only, counts only. `champions-real-data-measure`:

| Measure | Count |
|---|---|
| Models bound / refused | 2,312 / 11 (unchanged) |
| Two-pass (flag 0x10) / documented second equation | 524 / 514 |
| Meshes with the second tag on every packet / drawn with the environment pass | 1,782 / 1,782 |
| `two_pass_mesh_without_env_tag` (models) | 149, single pass |
| `env_tag_without_two_pass` | 0 (the model decoder already refuses a second tag without the flag) |
| `fix2_above_0x80` | 0 |

Every other measure line equals the pre-fix run.

Scene sweep, four headless slices, 60 ready ticks, baseline
`codex/playable-integration` ac1b0d9 built the same way: 111 / 111 / 111
with and without `--scripts`, 0 failed checks, 0 refusals. Every
deterministic count equals the baseline; only `time.*`,
`audio.frames_pushed` and the peak static memory differ (+0.5 MB summed over
four slices: the 128 × 128 environment image is now loaded per level). This
sweep runs with `--levels=0` and loads no creature preview.

A second sweep loads one creature preview per level (`--levels=28` per
slice, headless, no captures): 111 previews, 107 textured, the same 4
texture-size refusals as the baseline, 76 two-pass, 107 with the level's
environment image. It also reports `fx.runtime | material must consume
vertex colors` on 99 level runs, which the baseline does not. That
refusal comes from the per-surface materials a mixed environment/single
pass model gets; it is present at the pre-fix commit too and stays open.
