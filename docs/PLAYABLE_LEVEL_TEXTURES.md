# Textured play levels

Wave 8 composes the released level decoder/material adapter with wave 7's
native-FX WORLD lighting. Reference-function coverage unchanged; client
integration only. Decoder details, source pins, explicit model-row options and
remaining port limitations are in [LEVEL_TEXTURES.md](LEVEL_TEXTURES.md).

## Selection and resource ownership

Choose a supported dry static WORLD through the level library or existing WORLD
picker. **Level textures…** then accepts an explicitly selected matching TEX.
No filenames, directories or retail associations are inferred. A later library
visit can reuse that previously selected pair, re-reading both selected files.
The client retains at most 128 such path associations in memory. Unpaired levels
remain on the existing neutral display path; they never inherit a previous
level's textures.

The real `ChampionsLevelTexturePreview` decodes the texture directory/cell,
PSMT8 palette data and WORLD surface metadata. The scene prepares all owned
images, shaders, mesh UVs and the native collision candidate before committing
and publishing the new ground synchronously. Invalid WORLD/TEX input preserves
the prior native state and textured root. Starting/resetting ordinary play
retains that selected ground and its textures. Legacy diagnostic and isolated
preview routes remain available.

Native save bytes are unchanged and contain no asset paths or pixels. Within
this running scene, up to eight successful textured saves retain their decoded
resource owner keyed by the explicitly chosen save path and SHA-256 of the
written bytes. Loading that exact save prepares the matching textured subtree
before native commit. A changed file, unknown copy, evicted association or new
application run restores native collision with neutral ground; select a WORLD /
TEX pair explicitly to recover the presentation. No association is guessed from
collision geometry. These are bounded client resource policies, not original
game save behavior.

## Material and lighting composition

A shared terrain builder preserves the released native Z-up mapping `(x,z,-y)`,
positions, normalized UVs, decoded RGBA pixels, bilinear clamp-to-edge sampling
on level textures, GS alpha conversion, the documented ALPHA 2/1/9/0x48
equations, the 0x2f cutout and console layer priority. The texture shader
multiplies RGB by native-FX vertex colours when the chunk is dynamic. Isolated
texture previews retain white/unlit colours unless this play-only option is
selected. Environment images repeat and are bilinear; the base level texture
stays clamp-to-edge.

For dynamically lit WORLD chunks, the existing renderer supplies derived
geometry normals and calls native `shade_world`. Stored environment normals
stay in the mesh custom channel and are not substituted for those lighting
normals. The texture adapter adds no point-light arithmetic. Current
level/sun/point inputs remain the labelled synthetic lighting fixture;
original WORLD light entities are not loaded. Model and skinned-pose lighting
remains independent.

Chunks whose flags select flash (−230), full (+230) or flat (0) skip
`shade_world` and store `world_constant_channel` on the **loaded level's base
level**: 0x80, or 0x40 / 0x20 by the level's flag word (render-pipeline
section 5.1; research-fx `8e66747` lighting.md section 10.2). That base comes
from the level light record resolved for the loaded level; without one the
constant colour is refused and the chunk draws its base texture only
(labelled gated). Flag 0x100 wins over 0x2000, and 0x2000 wins over 0x200.
Negative sums wrap as `(2 × sum) mod 256`; other sums clamp at 255. The chunk
is presented as `texel × channel / 128`, clamped (render-pipeline section 9
item 3): the vertex colour stores `channel / 255` and the shader multiplies by
255 / 128. Dynamically lit chunks keep native-fx's existing `rgb / 255`
presentation, which halves a base-level texel; that convention predates this
adapter and is not changed here.

Environment flag 0x10 rebuilds its material when the caller supplies an image.
The camera adapter uploads the documented rows each frame. It reads
`g_cameraMatrix` as three rows that are the camera-space images of the world
x, y and z axes (render-pipeline section 3), so row i is
(right[i], down[i], forward[i]). The two GS passes
are one opaque shader: the environment sample replaces the destination, then
the chunk equation runs against that sample. A cutout keeps the environment
sample. The environment sample is modulated by the same vertex colour
(render-pipeline section 9 item 6). Its coordinates are computed per vertex,
as program 0x48 does, with the screen term in pixels of the 1280 × 448 buffer
from its centre, y down: `sx = F cam.x / cam.z`, `sy = F × 0.4666667 cam.y /
cam.z`, `F = 477.5 / tan(fov / 2)` taken from the Godot camera's vertical
field of view (render-pipeline sections 3 and 4.6). With no image the ALPHA 9
second pass is not composited on its own: the chunk's base pass draws opaque,
as before this adapter, and the environment stays gated.
Automap quads are built only when the caller supplies a decoded atlas, and
that mesh stays hidden on the play camera because the HUD rectangle is
unknown.

## Explicit gates

- The environment image is caller-supplied. No lump name is inferred. The
  per-vertex UV formula was not re-difftested against VU program 0x48. Scroll
  stays 0 unless the caller sets level-option bit 2.
- Within one layer, later transparent packets use a nearer sort offset. Opaque
  equal-depth order can still follow Godot's opaque pass. The depth test
  remains greater-or-equal: nearer fragments win and equal depth passes.
- The WORLD header +0x6c automap tile texture has no documented pixel format
  (world.md: U), so nothing decodes it; `champions-level-draw-measure`
  reports `atlas=undocumented`. Automap quads need a caller-supplied atlas. The HUD window and which way screen Y points
  relative to grid row 0 are not invented.
- Dynamic light slot colours of call 0x149b90 are unchanged; those chunks keep
  `shade_world`.
- Texture flag 0x0001, unsupported WORLD collision modes and full model 0x46
  decoding retain their existing refusals. `dome.vif` is unused on the retail
  disc, so the two model-row helpers remain explicit caller options.

No private level/TEX input was available or searched for. No retail values,
textures or level names were added. The sole bundled fixture is a byte-identical
copy of the released authored native texture test file, used only by explicit
acceptance scripts. Production selection never chooses it automatically.

## Checks and artifacts

`playable_wave_smoke_textures.gd` extends the entire wave-7 journey. It checks
explicit selection, real native decoded pixels/UVs, exact native lighting colours,
atomic bad-TEX refusal, reset, library revisit, same-save restoration, unknown
copy and changed-save isolation, and the non-dynamic lighting gate. Its graphical
leg compares the real decoded textured surface with native vertex colours and
a temporary white-colour reference, then restores native colours. This proves
actual shader modulation as well as checking mesh data. Camera/actor visibility
changes are inspection-only; geometry and decoded textures remain unchanged.

The owner preview/session tests use the same authored fixture inside the export.
The owner blend/cutout/priority pixel check also runs in the actual exported app.
Combined captures cover 1280×720, 960×720 and 800×600. Final counts, hashes and
capture paths are recorded in the wave-8 integration report.
