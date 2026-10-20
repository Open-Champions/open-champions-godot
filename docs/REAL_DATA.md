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
