# Drawn water

Water tiles use the colour, slope UVs, vertices, claim weights, drawer splash
and per-level appearance documented in `t3/research-textures` at `1528708`
(`docs/formats/water-draw.md`, `tools/ref_waterdraw.py`). The native
implementation is `champions::presentation::rta::water_draw`. It is
presentation. Creature `water_test` / `water_disturb` still return the service
counts. The service grid pointer stays 0, so that call writes nothing there.
When the count is not zero, the same call is applied to the drawn field. The
field takes its chunks from the same WORLD query (`0x189240`, the
`WaterImage::disturb_chunks` box and capacity), in query order.

The drawer and the height step do not call `cos16` or `sin16`. Those functions
are not used here.

## What is drawn

Layer 3, one pass, after ordinary WORLD (layer 0) and before a WORLD chunk
with flag `0x200` (layer 4). Flag `0x200` on the water chunk itself does not
move it off layer 3. There is no second packet and no context-2 environment
pass.

| | Set 0 | Set 1 (chunk flag `0x1000`) |
|---|---|---|
| FIX | 102 | 98 |
| RGBAQ | `0x006D6B5F` (R 95, G 107, B 109, A 0) | `0x00808080` |
| Height scale | bits `0x3B999999` (0.0046875) | bits `0x3CC00000` (five times set 0) |

Those two rows are the executable tables. On a load, the reset runs first and
writes set 1's fix, colour and scale as 0, then `water_setup` replaces set 0
only. A `customVars` object (case-insensitive class match, `docs/objects.md`
of the research) can replace either set. The scene applies those lines after
setup (`replacing`), as section 7 step 2 of the research note says a level
property replaces the RGB and the fix. `before_setup` is tested and is not
what the scene uses. `refuse_property_order` records that the original
factory-versus-setup order was not executed.

Keys match exactly. Values must be the executed shapes: four comma integers
for a colour, one integer for the timer, one decimal (optional minus, digits,
at most one point) for the floats. A closing carriage return is accepted:
every water line on the disc ends in one, and `sscanf` stops at the first
character a conversion cannot take. Anything else is refused, stores nothing,
and is counted. A colour whose fix one byte cannot hold (|fix| > 255) is
refused too: the drawer writes the fix as the high word of `ALPHA_1`, and the
pin does not say which bits of that word the GS keeps. Colour parts keep their
low byte, as `colour_word` in the reference does; a negative part was not an
executed vector. Float properties use the IEEE value of the decimal (decimal to
double to float, as the difftest deposited it), because `sscanf` `%f` did not
return in the interpreter. `waterWavyness` and `waterWavyness2` then multiply
by the static set-1 scale `0x3CC00000`. Bump strength, radius and the timer
word are stored. Nothing documented reads the timer.

A negative fix selects `ALPHA` `0x68` and `Cs × F + Cd`. The play shader keeps
`blend_mix` for a positive fix. A negative fix uses `blend_add` with
`ALBEDO = min(texel × tint, 1) × abs(fix)`. The executable's sets are
positive; level lines on the disc include negative fixes.

Alpha channel 0 is not the transparency. Positive FIX uses `ALPHA` `0x64`:
`(Cs − Cd) × F + Cd` with F = FIX/128. The texture function is modulate:
`final = texel × colour / 128`. `TEX1` is `0x61` (bilinear, no mips). `TEST`
`0x00071001` fails the alpha test for every pixel, so colour is written and
depth is neither updated nor tested.

The play shader uses that blend (`blend_mix`, `ALPHA = FIX/128`), modulate
(`ALBEDO = min(texel.rgb × tint, 1)`, with `tint` the set colour divided by
128 as a uniform: a vertex colour channel stops at 1.0 and level colours reach
255; the clamp is `COLCLAMP` 1 from the research render pipeline),
`depth_draw_never` and `depth_test_disabled`. `render_priority` is 3.
The texture is the first `.tex` of `waterenv.lmp`. UV is in texels; the
shader divides by the texture size. The texture is the `waterenv` image
`water_setup` was given (water-draw.md sections 1 and 7.1); no substitute is
documented. When it does not decode, that level's water is not drawn, the
scene says "texture refused, not drawn" and the real-data run counts the tiles
under `water.tiles_not_drawn`.

## Grid, wave and vertices

33 rows × 32 signed int16 heights. `step = floor((maxX − minX) + 0.5) ×` the
float at bits `0x3D088888`. Origin is `floor(minX + 0.5)`, `floor(minY + 0.5)`,
`chunkZ × 0.25` (`chunk+0x30`, not bounds z). Floor is toward −infinity. CPU
adds use the PS2 model; the VU pack uses chop. Before the view:

```
x = originX + rowIndex × step
y = originY + columnIndex × step
z = originZ + (height / 16) × scale
```

Records 1..32 are the uploaded rows. Calls 0 and 1 are not kicked. Thirty
strips cover records 2..32 at column positions 0..30. The height under column
i is column i+1. Slope UV, 12.4 masked to 14 bits, uses bits `0x3D4C78EA`. A
flat sample is 512 (32 texels). A sample 160 above its neighbour is 504.
There is no UV scroll, and the frame number is not a matrix input.

The play mesh uses those world positions (Godot `(x, z, −y)`), not the GS
screen coordinates. `project_strips` still tests the documented screen clip
(outside ±1000, sticky for the rest of the strip, exactly 1000 kept, z not
tested). The play camera uses its own frustum.

The wave steps once per simulation tick (`ready_ticks`), and a paused tick
does not step or drop a block. Heights and velocities saturate as signed
16-bit. Shifts are arithmetic. Velocity row r−1 drives height row r. Row 0 is
replaced by the damped gap velocity. A clear mask bit stores velocity 0 and
leaves the height, except row 0. Damping is `vel − (vel >> 7)`. A zero grid
stays zero for every mask, so that step is skipped; the result is the same
grid.

After the step, slot 0 copies the column seam and slot 2 copies the row seam,
both from the post-step snapshot. Where both slots write `height[1][0]` or
`velocity[0][0]`, slot 2 is applied second. That overlap was not a separate
executed case. Slots 1 and 3 are not read. Neighbour −1 is absent.

A block is kept while the signed `(frame − stamp) < 200`. At 200 it drops and
is not stepped. The next tick claims it again.

The claim runs after the step and the two seams, in tile order, so an earlier
tile claimed this tick is live for a later one. Every tile counts as drawn
every tick: the view cull and the world draw order are not modelled, and tile
order stands in for that order. A neighbour that is not live
contributes nothing. The weight is `0x3DCCCCCC`. Slots 0 and 2 are the edges
the per-frame exchange also copies. Slots 1 and 3 are read by the claim and
are still not read by that exchange. Height row 0 stays zero. Every
disturbance sample is cleared. Ten weights stay strictly under 1, so one edge
is truncated against 1.

`filled_mask()` is the level flag value `0x8` ("bit 8" in the research note;
its difftest writes `0x8`). The flag word at `0x44fe08` is row `+0x20` of the
level table, which the level-lights JSON already carries (`flags`). When the
user chooses that JSON the scene passes the row's word to the field. On the
disc copy, 0 of the 90 rows set `0x8` and 0 set `0x10`.

## Splashes

The drawer splash runs only when flag `0x10` is set and the tick is not
frozen. It is inside the per-tile draw, so the field rolls the generator once
per live tile after the claim. A hit consumes three seeds, disturbs at
strength 1500 and radius 6 through the chunk query, and leaves the write in
the velocity grid until the next step. The start value of the word at
`0x404460` is not in the pin, so `set_level_flags` refuses `0x10` unless a
seed was supplied (tests supply one). No disc row sets `0x10`.

A wading player would disturb the drawn water through `real_world::Wader`
(`Player_move` step (f), t3/research-wading 942a6ba; see
[PLAYER_WADING.md](PLAYER_WADING.md)): 1000 or 3000 at radius 24 on entry and
300 at radius 12 while wading, at `(x - 0.0, y, z - 12)`. The creature water body
(300/12, 1500/24) is the creature `water()` at `0x113d78`, not the player.
The `--wade-probe` harness stimulus (one labelled 3000/24 `water_disturb`) is
kept for the captures. The scenes do not run `Wader` by default (the control
walk has no frame counter or stick speed); `--wade-diagnostic` opts in.

In the captures the player stands under the water blend. Players are layer 1
and water is layer 3 with no depth test (research section 2, an inference
from `TEST_1`), so the tile covers them at F.

## Refused

- The simulation splash when the countdown at `0x44f200` is negative. A
  countdown of 3 still skips. Strength 41 and radius 10 are stored and that
  body does not run.
- Any reader of `waterBumpTimer` at `0x44f06c`. Setup stores 4, the reset
  stores 6, and a level line stores the integer.
- Neighbour slots 1 and 3 in the per-frame exchange. The claim does read them.
- Whether `customVars` runs before or after `water_setup`.
- `Player_move`'s `water_disturb` immediates. The published player water calls
  are the jump and the block move, and those constants are not used for walking.
- Splash particles. Both definition words are zero in the executable and the
  names are not in the image. `second_particle_z` is tested and nothing is spawned.
- The start value of the drawer splash seed at `0x404460`.
- A level colour fix outside ±255, and any level line outside the executed
  value shapes.

## Pins

`t3/research-textures` `1528708c99ab48c03339dad614f6c32c3892f905`.

| Source | SHA-256 |
|---|---|
| `docs/formats/water-draw.md` | `b762ef9462465336c165e6355c8d4ef6d56bbfb0e9fcc2d252757e432550e292` |
| `tools/ref_waterdraw.py` | `f6830db4cb0a3f10d7aafe1052223de0f44808597ca677c43c8da2044756c839` |
| `tools/test_ref_waterdraw.py` | `97a8076f94f09d2ca1bea276f4fdf8632b8b7d7b3496c20b722b523845fbb5de` |
| `tools/difftest_waterdraw.py` | `707d2ab2d592e97822ece1cbe8e530c56c6e9f639314e7469bc7c03e4042a0f9` |

`champions-water-draw-tests` ports the reference vectors. The difftest's
original-executable run is research evidence; this repository does not run it.
