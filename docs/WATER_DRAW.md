# Drawn water

Water tiles use the colour, slope UVs, vertices and layer documented in
`t3/research-textures` at `a9486c7` (`docs/formats/water-draw.md`,
`tools/ref_waterdraw.py`). The native implementation is
`champions::presentation::rta::water_draw`. It is presentation. Creature
`water_test` / `water_disturb` stay on their own grids and are not written by
this drawer.

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

Alpha channel 0 is not the transparency. Positive FIX uses `ALPHA` `0x64`:
`(Cs − Cd) × F + Cd` with F = FIX/128. The texture function is modulate:
`final = texel × colour / 128`. `TEX1` is `0x61` (bilinear, no mips). `TEST`
`0x00071001` fails the alpha test for every pixel, so colour is written and
depth is neither updated nor tested.

The play shader uses that blend (`blend_mix`, `ALPHA = FIX/128`), modulate
(`ALBEDO = texel.rgb × COLOR.rgb` with the vertex colour already divided by
128), `depth_draw_never` and `depth_test_disabled`. `render_priority` is 3.
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
is not stepped. The next draw reclaims it.

`filled_mask()` is the level-option bit 8 fill. The scene does not turn that
option on.

## Refused

These stay explicit refusals. A reclaimed or newly claimed block is a zero
grid because the claim weights were not executed. That zero is not a
measurement that the original seed is zero. With the seed refused, a real
level stays flat: every UV is 32, so the tile samples one texel, tinted by
the set colour and blended at F.

- The numeric weights `water_init` uses on the frame a block is claimed.
- The splash point the drawer picks inside the chunk bounds.
- The simulation splash taken when the countdown at `0x44f200` is negative.
  The constants (strength 41, radius 10, splash 1500/6, the LCG) are stored.
  The scene does not splash.
- The integer 4 at `0x44f06c`. Nothing documented reads it.
- Neighbour slots 1 and 3 beyond `water_init`.

## Pins

`t3/research-textures` `a9486c7f0052e533d3cc5c88f3d98d8f07bffdce`.

| Source | SHA-256 |
|---|---|
| `docs/formats/water-draw.md` | `a6d827ef663f5783512b8f2a1a72f874b543f96ffead241346dc73ad59f619aa` |
| `tools/ref_waterdraw.py` | `7c2a005a39784d65b2e65ff8f321e1bd0a580820ccf202c2f77f0f6d27cb77b7` |
| `tools/test_ref_waterdraw.py` | `d8a28cf81b05234a26eeb4041227e300cf7488972da7f285ac812cb5e082dea9` |
| `tools/difftest_waterdraw.py` | `2fea3ba76add0440f4ccdce7ce4a7d81558608705a32986ad82ef2fd37b1333c` |

`champions-water-draw-tests` ports the reference vectors. The difftest's
original-executable run is research evidence; this repository does not run it.
