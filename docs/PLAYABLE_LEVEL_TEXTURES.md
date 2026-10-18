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
positions, normalized UVs, decoded RGBA pixels, bilinear clamp-to-edge sampling,
GS alpha conversion, reverse/additive blending, cutout and Godot layer priority.
The texture shader optionally multiplies RGB by native-FX vertex colours. Texture
alpha and cutout/blend tests are unchanged. Isolated texture previews retain
white/unlit colours unless this play-only option is selected.

For dynamically lit WORLD chunks, wave 7's renderer supplies derived geometry
normals and calls the existing native `shade_world` implementation. The texture
adapter introduces no point-light arithmetic or new lighting model. Current
level/sun/point inputs remain the labelled synthetic lighting fixture from wave
7; original WORLD light entities are not loaded. Native returned RGB is retained
in vertex colours and consumed by the shader. Static-light caches still avoid
repeated multiplication, and model/skinned-pose lighting remains independent.

The released material metadata explicitly disables dynamic lighting on some
chunk flags. Those chunks retain their released decoded base-texture pass;
the signed constant-colour modes are not interpreted as new point lights or
guessed colours. The scene labels special chunk lighting as gated. This is not
console final-colour parity.

## Explicit gates

- Environment mapping remains disabled: only the decoded base texture is drawn.
  Original environment images and VU1 camera-normal coordinates are absent.
- Console depth ordering remains unverified/disabled as a parity claim. The
  released Godot layer priority remains intact; it is not a console depth model.
- Automap tile-code mapping remains disabled; no map tile is inferred.
- Non-dynamic special constant-colour composition retains the base pass only.
- Texture flag 0x0001, unsupported WORLD collision modes and full model 0x46
  decoding retain their existing refusals. The two model-row helpers remain
  explicit caller options; play loading does not pick one implicitly.

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
