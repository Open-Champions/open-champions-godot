# Native visual effects and lighting (Return to Arms)

A Godot-independent, tick-driven port of the Return to Arms v2.00 effect and lighting
contract, and a Godot presentation of it. Round 2 (native-fx-2) follows the research round
that **ran the original code**: the effect functions in `tools/ee_emu.py`, and what they
send to the vector unit in `tools/vu1_emu.py`, which runs the VU1 microprograms of the
user's own executable (7 difftests, 0 mismatches). Every exact part here is a port of a
research reference written from, and difftested against, that execution. RTA only; nothing
here is claimed for Norrath.

## Evidence pin

Research branch `t3/research-fx`, worktree `<research-checkouts>/research-fx`, commit
`19f80a6e92e07a77ee75c589fa11a71330eee082` (round 1 was `d7922ef`). Read: `docs/effects.md`
sections 12–18, `docs/lighting.md` section 9, and the references below with their tests and
difftests. SHA-256 of every pinned file is in `tools/fx2_research.py` (`PINS`); the vector
generators read the files only through it (`git show 19f80a6:path`, hash-checked), and every
vector manifest records the commit and the hashes. The ones the code follows:

| File | SHA-256 |
|---|---|
| `docs/effects.md` | `2ffa72fdc0d786be192d4a184cf04bf5b2b1cbb42fd3d94a53a6770977084caa` |
| `docs/lighting.md` | `4b6622824a6c0f8aa72a2c2879d9412e5f7785343a2132541ccb9b06d7c5d76b` |
| `tools/vu1_emu.py` | `3fee1b1f2f6a7139c4776a10d933607996393c42ef9eb53783d9cc6b35d4d868` |
| `tools/ref_fx_render.py` | `96fd28118a3024f43dbb23405b7d9c1f9fafde59362f00167a231ceb6f4be592` |
| `tools/ref_fx_recipes.py` | `b83c050201bb374ef04b2a880c0f3eeb031c1e6a4c75d9b8d6438e38f215d371` |
| `tools/ref_fx_skills.py` | `1b54848a26fd533fff00279820cec023d487709882ddd334dc78caab32f642f7` |
| `tools/ref_decals.py` | `aaadcac4a01b114a4906042df9795e9ce9a2259ad3f473642a8c9a24d33b9d46` |
| `tools/ref_fx_props.py` | `db9de4c4b07b0b438b8b6a272b083837dbdad82b3a66c72bfb392f8dcef0b139` |
| `tools/ref_particles.py` | `5d7c133aa2ee3f9cc8f57af32d136df57de412f73ab145b1553f1e6eef7a394c` |
| `tools/ref_lighting.py` | `2060ca9932bb79907ed21fbbd83e87a60fdd26996a02b78bc7550bd92131ef62` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `tools/fx_particle_defs.py` | `b3565cf30132a829f4aa05c3ea01e13efb2afc62bfd0b2cef50f0cbf76417b42` |
| `tools/fx_skill_recipes.py` | `01f5fddf855b9b49dd86f1849ca8f95e3305974152f0c784151b6b8473c8313e` |
| `tools/fx_callsites.py` | `f643e004baf1bbd313dee9dfa77f1f5effe966c4d879eb73150f75d38acaa72e` |

`ref_core.py` is the same file the native core (`champions-core`) ports, so the raw float
arithmetic is shared.

## Inputs: no retail values in the repository

- **Particle definitions**: `python3 -I tools/fx_particle_defs.py <your SLUS_209.73> --json`
  (research repository), kept outside the checkout, loaded at run time
  (`EffectWorld::load_definitions`, `ChampionsFxBridge.load_definitions_json`). Definition
  *addresses* appear in code as keys, as in the references; their *values* never do.
- **Skill-recipe constants**: `python3 -I tools/fx_skill_recipes.py <your SLUS_209.73> --json`,
  loaded by `skills::RecipeConstants::from_json`. Every constant effect argument of the 21
  round-2 effect functions (definitions, glow colours/alphas/flags/bias, trail rows, widths,
  flags, colours and lifetimes, decal types, fx_spawn ids, back-dates) is read from it. The
  loader validates the file against the pinned recipe table and refuses a misfit. The light
  records that the StaticFire and SpellPlanarSteelEffect constructors write are the
  `"light_records"` object `fx_skill_recipes.py --json` emits at research `t3/research-fx`
  `b6618dd634bf368acb5ab79fc84ff092553de25b` (docs/effects.md §12.2). The rest of this
  module stays on the `19f80a6` pin above. Omitting `light_records`, or a StaticFire
  falloff, still makes those constructors refuse.
- **Textures**: read by lump and name from the user's own GOB/LMP files
  (`ChampionsFxBridge.load_effect_textures`): `engine.lmp`: `particletex.tex`, `trail.tex`,
  `floorsprite.tex`, `halo1.tex`; `fx.lmp`: `halo2`–`halo5`, `halo7`, `halo8`, `wisphalo.tex`
  (docs/effects.md 12.2, 13.9, 14.2).
- Tests use synthetic values only. The skill vectors replace all 133 constant argument
  positions with stand-ins, and a scan checks that no reference constant appears.

## Parts (native/fx2_targets.cmake)

| Library | Header | Reference | What it is |
|---|---|---|---|
| `champions-fx-exact` | `fx_exact.hpp`, `fx_decals.hpp`, `fx_props.hpp`, `fx_rz.hpp` | `ref_particles.py`, `ref_decals.py`, `ref_fx_props.py` | definitions, spawn records, pools and groups, emitters, flipbook; floor decals; torches, candles, lantern, lamp |
| `champions-native-fx` (round 1, corrected) | `fx.hpp`, `lighting.hpp` | `ref_lighting.py` | round-1 API now delegating to the exact pools; lighting with the round-2 corrections |
| `champions-fx-render` | `fx_render.hpp` | `ref_fx_render.py` | the GS output of particles, groups, kind-2 lines, trails, beams and glows; trail/beam/glow stores; the frame in GS order |
| `champions-fx-recipes` | `fx_recipes.hpp` | `ref_fx_recipes.py` | every `fx_spawn` id, disintegrate, ice shatter, the eight lose-a-body-part functions, model-vertex helpers, `fx_modelStatusEffects` |
| `champions-fx-skills` | `fx_skills.hpp` | `ref_fx_skills.py`, `fx_skill_recipes.py` | call-out → primitive mapping for all skill models, the 161-row recipe table (91 classes), the 21 round-2 functions, the recipe-constants loader |
| `champions-fx-sim` | `fx_simulation.hpp` | (wiring) | `EffectSimulation` (every machine) and `EffectWorld` (per machine), the presentation runs |

### 1. Glows, trails, beams (docs/effects.md 13.4, 13.5, 13.7, 13.8)

Exact per `ref_fx_render`: glow projection and A+D packets, including the turned quad and
the sign spill of an off-screen X. Single-point and paired trail strips, with chunks
starting at the oldest index rounded down to even and a width of W/w with x doubled. The
trail builders. Beams: a node tree from six cosmetic draws per node plus a **libc `rand()`
& 7** counter (`core::LibcRand`, the newlib 64-bit generator); drift with the live cosmetic
seed once per drawn frame (unless frozen). Points rebuilt every draw with the cosmetic
seed set to **`g_simFrame × 0x229 + index × 0x3f3`** and put back afterwards, so the shape
changes once per simulation frame. The polyline holds only the midpoints. Even/odd draw
frames use the two trails. **GS order** (13.3): `glow_draw(0)` packets at key 3 come first,
then at key 4 the particle packet, the `glow_draw(1)` packets and the trail packet. That
is the reverse of the call order.

### 2. Kind-2 lines (13.1)

Gouraud lines (PRIM 0x49). The tail carries the head's RGB with **alpha 0**. The head's
clip test sets ADC on the tail. ALPHA_1 is 0x48, as the kind-1 lists leave it.

### 3. Skill effect functions (15)

`skills::recipes()` lists all 161 effect-calling functions of the skill files (91
classes), with phase, executed model, static primitive counts and generators.
`must_run_headless()` names those that draw from own, spell or shared generators.

- **140 functions:** their models already exist as the native skill ports (skills_a, b, c,
  arena_monsters), which emit the call-outs. `skills::from_lanes` / `from_typed` /
  `value_from_result` adapt each port's call log, and `to_primitive` decodes it field by
  field.
- **The 21 round-2 functions** are `skills::Model`, call for call over a memory image:
  - draw-slot glows: ShockProjectile, FireArrow, HolyStrike, SummonBadger, Resurrect, SmokeGen
  - MultiFireProjectile
  - FireStormFlare
  - Pustule
  - DiseaseCloud
  - SpellArena
  - HornOmen
  - PlanarSteel
  - StaticFire, with its helpers

**Site mapping (inferred).** A model call takes its JSON constant by (function, primitive,
ordinal in ascending site address). The evidence:
- `fx_callsites.scan_calls` walks the code in address order.
- The recipe table's static counts equal the mapped calls for all 21 functions.
- Where one constant fills two sites, the loader requires them to agree.

Weakest cases: the order of MultiFireProjectile_end's two `fx_spawn` sites, SpellArena_run's
two particle sites, and HornOmen's two sites. HornOmen is modelled as one call, so its two
sites must agree.

### 4. Groups (13.2)

Each live group is drawn after the global list set in index order 0..15 through
`mat4_mulMat34(mat34_mul(groupMat, view), P)`. After the last group the global matrix is
uploaded again. Group lists belong to the group slot (16.1).
`particleGroup_setMatrix` with a matrix the call-out does not carry (the `'stack'` token
still produced for skills_c's zero matrix) is marked unplaced and not drawn.
SpellHornOmenEffect now passes the 12 lanes of `mat34_setPosRotZ` on the owner position
`+0x10` and facing `+0x38` (docs/effects.md §12.2, research `b6618dd`).

### 5. One-shots, deaths, status (14)

Every `fx_spawn` id is exact:
- 24 made-on-first-use definitions.
- The camera gate first, except for id 5.
- Id 5's shared-seed Fire objects.
- 0x15 a **flat ring**: the integer `abs` of a float gives 0.
- 0x16 always rising.
- Ids 6, 0xb, 0x12 as executed.

Also exact: disintegrate (back-dated cloud), ice shatter (1 + n shared draws), the eight
lose-a-body-part functions (two have no caller), `fx_burnModel`, `fx_shockModel`,
`fx_drawModelGlowNearCamera`, `fx_emitFromModelVerts`, and `fx_modelStatusEffects`. For the
last: the swing trail, sparks, glow, all five elements and ribbons. The amount is the
caller's, and poison/disease measure the motion against the same frame.

### 6. Textures (13.9, 14.2)

The particle sheet is 512 × 160: 16 × 5 cells. A tile at 0x50 or above samples GS memory
below the sheet (U), so presentation refuses it and reports it. The glow textures are
`fx.lmp:halo2.tex` (0x4f848c) and `engine.lmp:halo1.tex` (0x44fe04), plus the others in
14.2. A glow's texture word is the global's address: each global holds itself as a token.

### 7. Round-1 corrections (17; lighting 9)

- **31 usable particle slots**: slot 0 is held for good by 0x7e1d00.
- **Toward-zero single precision** throughout: definitions, back-dating (k/60 s gives
  k − 1 ticks), colours ending one short when the rate is inexact, and the emitter timer.
- **Fill** trunc(0.79999995 × ambient).
- **LightEffect** keeps colour fractions; its node gets trunc().
- **16 shared light-chunk slots**: a takeover starts from the previous owner's radius
  (`lighting::LightSlots`).
- Decal growth: 0.19999999 / 0.049999997 with overshoot, an unsigned type gate, and a block
  z range starting at 0.
- **Not cosmetic**: effect classes use object (+0x44), spell (+0x10) and shared generators.
  Every LightEffect draws the shared seed in its constructor. `fx_spawn` id 5 and the ice
  shatter draw the shared seed. DiseaseCloud (with no attacker, every 30th frame) and
  StaticFire (every 12th frame while `isNearView(941)`) **deal damage**.

`fx::sim::EffectSimulation` is the simulation-side API for this, separate from the
renderer:
- It runs on every machine, with or without an `EffectWorld` attached.
- It makes all those draws.
- It hands damage, world queries, constructors and path blocking to the combat runtime
  through `GameHooks`. A missing gameplay answer **refuses**; it never silently returns a
  default.
- Skill models' `LightEffect_ctor` call-outs have already counted their shared draw (the
  models do it), so the light itself is per machine.
- `answer(callout)` is the hook the existing skill ports' callbacks can call.
- `classes()` runs the 21 functions with this routing. StaticFire's damage gate
  `isNearView` is a per-machine answer: it is the documented D6/D7 hazard, reproduced as
  the original has it.

### 8. The `MULq | WAITQ` question (13.6)

Both readings are implemented exactly behind `render::WaitQ`: `WaitFirst` (the pair waits
for the divider) and `Vu1CppReading` (the upper half reads the old Q, as `vu1.cpp` does).
The default is **`WaitFirst`**, the research's inferred reading: under the other reading
the trail program would ignore its width word and its RSQRT. It has not been checked on a
console. It changes kind-2 line tails and single-point trail widths. The setting is
`EffectWorld::waitq` / `ChampionsFxBridge.set_wait_first`.

## Godot

`godot/fx/native_fx_renderer.gd` (`NativeFxRenderer`) holds no game state.

1. `load_definition_file(path)` and `load_textures([gob or lmp paths])` read the user's
   files.
2. Call `advance_tick(sim_frame)` once per simulated tick.
3. Call `present(camera)` once per rendered frame. `camera` is the game's own view mat34,
   `g_projScale`, `g_cameraMatrix` and `g_viewProj` (`{"view", "proj_scale", "camera",
   "viewproj", "sim_frame", "net_frame"}`); how the game derives them is outside this
   contract.

The bridge returns the frame's GS output as runs in GS order. Each run has normalised
coordinates of the 1280 × 448 frame centred on 0x8000 (I: the centre; shown at 4:3), view
depth from the clip w, the ALPHA_1 blend (0x44 mix, 0x48 add, 0x09 premultiplied
inverse), and the texture by name. The script draws each run in clip space with GS
modulation (0x80 = 1.0), depth-tested against the 3D scene without depth writes. Later
runs draw over earlier ones (`render_priority`).

Floor decals are drawn through the same camera from their stored records. That is
presentation only: the decal task's GS output was not part of the executions.
`apply_vertex_lighting` (model and WORLD vertex colours) is unchanged from round 1, on the
corrected lighting.

The bridge is presentation. It refuses `fx_spawn` id 5 (gameplay) and leaves the shared
seed to the combat runtime's `EffectSimulation`.

No C++ exception crosses `ChampionsFxBridge` (review 2026-10-20, finding 1). Every
mutator and lighting query answers a Dictionary `{ok, ...}`; a refusal is
`{ok: false, refusal}`:

- `set_detail` (0..2), `spawn_particle`, `fx_spawn` (`calls`), `run_emitter` (`count`),
  `add_glow`, `add_trail_point`, `update_beam`, `add_decal` (`added`), `tick`,
  `set_level_light`, `add_light_node`, `model_light`;
- `add_emitter`, `create_trail`, `create_beam` answer `{ok, handle}` (finding 4);
- `shade_model`, `shade_world`, `decal_tint` answer `{ok, rgb}`.

Non-finite inputs are refused at the boundary. A native exception part-way through a
world update (`tick`, `draw`, a spawn) latches the bridge, and later calls refuse with
the stored reason. Particles of definitions missing from the user's JSON are counted by
address (`EffectWorld::missing_by_definition`) and named in the refusal (finding 5).
`draw` / `NativeFxRenderer.present` answer `ok: false` whenever the frame carries
refusals, and still return the runs they could make (finding 6). `save_state` /
`load_state` hold the bridge's exact presentation state for the fixture continuation
(`fx_world_save.hpp`).

## Explicit refusals and open points

- **U in the research (18):**
  - The console's last bit is not checked: every interpreter assumes round toward zero.
  - What GS memory holds below the sheet (tiles ≥ 0x50 are refused).
  - Who sets 0x44fc98, which is exposed as `hold_trails`.
  - What 0x5038c8 counts (SpellArena reads it from memory).
  - Effect classes outside the skill files.
- **Emitter angles: `math_cos16` / `math_sin16`** (2026-10-20). The original routines
  are the half-angle polynomial in the FX research `docs/spec/trig16.md`, not host libm
  and not a correctly rounded IEEE cosine. `sin16(0)` is +2⁻²². The port
  (`champions/fx_trig.hpp`) uses the default ps2 adder in integer arithmetic, so arm64
  and x86_64 agree. The generator replaces `ref_particles._cs` with `ref_core.cos16` /
  `sin16` and promotes each float32 result to binary64 for the multiplies the reference
  still does in Python floats. 16 `trig` rows in `fx_exact.tsv` check every angle.
  Trails, glows and the combat cosine already call the same polynomial through
  `core::cos16`. Beam builders do not call either routine. The haze circle is computed
  by `haze_circle`; it is not a committed table.
- **Still refused from that contract:** the angle passed into `Player_blockSparks`,
  the four formulas in `Creature_processDamage`, whether `particle_drawHaze` runs only
  for kind 3, and which screen axis receives the haze sine word. `draw_haze` refuses
  the last two. The silent-0 block-spark body remains the earlier executed defence
  model; this job does not treat that as a newly recovered angle.
- **Native choices** (undocumented; marked NATIVE in the code; all per machine):
  - Trail/beam handle generation: lowest free slot; the 8-bit counter advances per create;
    stale handles are ignored.
  - Which beam trail is created first.
  - The split of `beam_create`'s colour word: red in the low byte.
  - Flag-1 trails skip the update entirely.
- **Refused as undocumented:**
  - Trail rows outside 0..8; odd paired chunks; negative beam depth.
  - Builders over the ring's 126 points; a trail clock past 0x7fff.
  - Ice shatter with `(int)count == 0`; model-vertex emission from a model with no
    sample points.
  - Orc parts 6 and 7.
  - Light records missing from the recipe JSON.
  - A recipe JSON that does not fit the pinned table.
- **Not modelled in the exact pools:** the level reset 0x18ebb8. `decal_reset` follows the
  docs' reading (I), and its effect on the decal total is left to the caller.
- **Python 3.12 `sum()`:** the lighting reference's dot products use compensated summation
  (Python 3.12 `sum`), and `lighting.cpp` reproduces it. The reference itself matches the
  original only within 2.5 × 10⁻⁷ for model-light directions.
- **No private acceptance:** private game files are not on this machine, so no real JSON or
  texture was loaded.

## Verification

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(fx-|native-fx-synthetic)' --output-on-failure
ctest --test-dir build/native -j 3 --output-on-failure
```

Vectors (plain TSV with JSON manifests in `native/tests/vectors/fx_*.{tsv,json}`) are
regenerated from the pinned references with `python3 -I tools/generate_fx_{exact,render,recipes,skills}_vectors.py`
(each reads `<research-checkouts>/research-fx` through `tools/fx2_research.py`):

| Test | Rows / checks |
|---|---|
| `fx-exact` | 14,690 rows (pools, definitions, spawn records, emitters, flipbook, decals, props, lighting, light slots, light effects, every 16-bit angle), plus the synthetic assertions |
| `fx-render` | 8,914 rows (VU arithmetic, matrices, particles incl. groups and lines, trails, beams, glows, builders; both FPU models; both WaitQ readings) |
| `fx-recipes` | 23,800 rows over 42 properties, 3,357 unit checks |
| `fx-skills` | 2,319 cases over the 25 methods, 14,799 call-outs decoded |
| `fx-sim` | 74 wiring checks: headless vs drawn generator equality, damage and constructors through the hooks, refusals, GS order, groups, lines, light effects, beam seeds, weapon handles, presentation |
| `native-fx-synthetic` | round-1 assertions updated to the corrected values |

Godot (GDExtension built with `-DGODOTCPP_TARGET=template_debug` / `template_release`):
`--headless --script res://tests/native_fx_smoke.gd` and
`--rendering-method gl_compatibility --script res://tests/native_fx_visual.gd`, which
writes `work/native-fx-2-visual.png`. `--headless --script res://tests/level_lights_smoke.gd`
loads a synthetic per-level record.

## Per-level light record

Research `t3/research-fx` commit `8e66747` (`docs/lighting.md` section 10,
`tools/fx_level_lights.py`) is the contract for this loader only. The rest of this
document stays pinned at `19f80a6`. The executable has no per-level sun array. Each
table row is a name and a flag word; marker objects supply ambient, sun and direction.
`champions::rta::lighting::load_level_lights` reads the extractor's JSON. Lookup follows
`game_loadLevel` (exact `name_sub`, then a row name that starts with the plain name,
then a row name inside it). A miss uses the null-name row. Colours are stored as the
low 8 bits, alpha 0, and the direction is normalised by `Level::make`. Fill is
`Object::reset`. World vertices still use the base level plus point lights; the sun
lights models.

Play keeps the labelled synthetic sun unless `--level-lights=<json>` is supplied
and the scene that loaded the level passes that level's own name (and sub-name) to
`select_level_light`; the real-data scene passes the GOB stem with an empty sub, as
`split_request` gives for a bare level name. The command line names the JSON only,
never the row. With no loaded level name the record is refused ("level light
refused: loaded level name unknown") and the synthetic sun stays; a known name that
misses every pass takes the null-name row, as `game_loadLevel` does. The synthetic
point node stays either way. A file that does not load leaves the synthetic sun in
place.
