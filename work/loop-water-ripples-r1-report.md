STATUS: COMPLETE

# water-ripples, round 1 (Claude takeover of the Cursor Grok run)

Branch `codex/water-ripples`, from `codex/playable-integration`. Evidence:
`t3/research-textures` `1528708` (`docs/formats/water-draw.md`,
`tools/ref_waterdraw.py`, `tools/test_ref_waterdraw.py`,
`tools/difftest_waterdraw.py`; the SHA-256 pins in `docs/WATER_DRAW.md` match),
plus `docs/data-tables.md` 7.1, `docs/objects.md`, `docs/spec/creature-tick.md`,
`docs/spec/movement-camera.md` and `tools/ref_player.py` on the same branch.
Nothing was pushed, and nothing was written in `~/champions-re` or on the disc.

## Review of the two Grok commits (1f8cf0c, 83cf3d9)

These trace and are kept: `claim_grid` (weights `0x3dcccccc`, the replacement with 1, slot
order), `splash_point`, `second_particle_z`, `disturb_cells`, the reset and setup words,
`colour_word`, `wavyness_scale`, the order inside the tick (step, then seams, then claim,
then splash), and the additive form for a negative fix. The ported vectors match
`test_ref_waterdraw.py`.

Fixed because they did not trace:

1. **The player's wade disturb used the creature water body** (300/12 and 1500/24, plus
   a stick stand-in for `+0xb8`). Those constants belong to the creature `water()` at
   0x113d78. `Player_move`'s `water_disturb` arguments are not published. I removed
   `creature_standing_disturb`, the `standing_disturb` binding and the play-scene call.
   A wading player does not disturb the water, and that refusal is explicit.
2. **`Field::disturb` used a hand-written bounds test** with a cap of 16. It now takes
   its chunks from the documented query (`WaterImage::disturb_chunks`, which reuses
   `Service::query` with the same box and capacity). That means the tree order, at most
   15 chunks, and the cut byte.
3. **The drawer splash seed started at 0** (the retail word 0x404460 is unknown). Now
   `set_level_flags(0x10)` refuses unless a seed is supplied.
4. **The level flag word "not loaded".** It is row +0x20 of `g_levelTable`
   (`data-tables.md` 7.1). The scene now reads it from the user's level-lights JSON.
   On the disc, 0 of 90 rows set 0x8 or 0x10, so the mask fill and the drawer splash
   are off as a measurement.
5. **Level-line parsing threw on any odd value**, which failed the whole level load.
   Values now have to be the executed shapes. Anything else is refused and counted. A
   closing `\r` is accepted: every disc line has one, and `sscanf` stops before it. A
   fix with |fix| > 255 is refused, because the pin does not say which bits of the
   `ALPHA_1` high word the GS keeps. Floats now go decimal → double → float, as the
   difftest deposit does.
6. **The tint was a vertex colour.** A vertex colour channel stops at 1.0, so level
   colours above 128 were clipped. The tint is now a shader uniform, clamped after
   modulate (`COLCLAMP` 1).

## Tests and runs

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4`: pass.
- `ctest -j 3` (full): **200/200 passed**. `water-draw` has 77 checks; `real-world-adapter`
  has 20 (3 new for `disturb_chunks`).
- GDExtension `build/level-texture-template_debug` rebuilt. Godot matrix
  (`work/water-ripples-smokes/run-godot.py`): **43/43 exit 0**.
- Disc sweep (headless, `--all --ticks=30`, level-lights JSON): 111/111 levels loaded and
  played, 0 failed checks. 48 water levels, 9,451 tiles, flag word loaded on 48. 46
  levels have water lines: 157 applied, 6 refused (fix outside ±255). 1 level draws
  additive.
- Captures in `work/private/water-ripples-r2/`, 3 water levels, `--wade --wade-probe`.
  The player reached water in 3/3 levels (169 ticks in water).
  - The wade shots show flat water with the level's own colour (one level is tinted
    dark red by its line). The player is under the layer-3 blend.
  - The probe shots show rings spreading through the drawn grid.
  - The probe is a labelled harness stimulus. It uses the Player jump-state strength
    and radius (3000/24, `ref_player.py`) at the player position. The scene does not
    run that state.

## Commits

- `5d4ea71` Drop untraced water stand-ins and route disturbs through the WORLD query.
- `7619f08` Accept the closing carriage return on level water lines; wade until in water.
- `ede9c33` Document the traced water disturb path, level lines and refusals.
- (this report)

## Still refused or unverified

- The negative-countdown simulation splash; any reader of `waterBumpTimer`; slots 1 and 3
  in the per-frame exchange; the factory-versus-setup order (the scene follows section 7
  step 2: lines replace); splash particles; the splash seed start; `Player_move`'s water
  immediates.
- Negative colour parts (3 disc lines) follow the reference's low-byte mask. No executed
  vector covers them.
- Draw order: every tile counts as drawn every tick, in tile order. The view cull and the
  world draw order are not modelled.
- In Godot, a fix of 129..255 gives `ALPHA` > 1 in `blend_mix`. That extrapolation was not
  checked against the GS.
- Water overdraws nearer geometry and the player. That follows the research's inference
  from `TEST_1` (marked I), and it is visible in the captures. A research recheck would
  settle it.
