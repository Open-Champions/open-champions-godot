# Play-mode FX composition

Wave 7 connects the released `ChampionsFxBridge` and `NativeFxRenderer` to the
existing opt-in play policies. Native FX algorithms and their explicit refusals
remain as documented in [NATIVE_FX.md](NATIVE_FX.md). This is synthetic client
integration, not a claim of retail effects, original level lighting or new
reference-function coverage.

## Definitions and ownership

No user definitions JSON was supplied. The default is the committed native-FX
smoke test's authored particle row, keyed to the documented spawn-0 definition
symbol `0x8704a8`. Its values are synthetic. Particle/floor sheets are generated
white/red test images with measured dimensions, not extracted textures. The
play view labels these resources as synthetic, above the viewport.

The pause menu's **FX definitions JSON…** action opens an explicit file picker.
Only that selection reads external definitions; there is no search, automatic
retail discovery or committed retail value. A temporary native bridge validates
the full JSON before replacing the cosmetic owner. Invalid JSON leaves the
current resources intact. A valid replacement clears cosmetic history, preserves
the current ready-tick boundary and remains paused. Test sheets remain synthetic;
this picker does not claim to supply the original particle atlas. The acceptance
check selects another authored synthetic JSON file solely to test this route.

## Native lighting in the scene

The play adapter calls native `shade_model` for player, creature and manually
selected cosmetic meshes, and `shade_world` for each static terrain mesh. It
uses an explicitly authored sun/ambient/point-light fixture; WORLD light chunks
are not inferred. Native XY/Z-up positions and normals convert to Godot XZ/Y-up
only at the renderer boundary. Normals use the inverse-transpose transform.
Position-only triangle fixtures receive derived geometric normals for display;
these are not reconstructed original model normals.

Vertex colours multiply the original colours and preserve mesh vertices, UVs,
indices and textures. Standard materials consume those colours unshaded to
avoid a second Godot lighting pass. The known diagnostic grid shader explicitly
opts into this same path; unknown custom materials still refuse. Static WORLD
lighting publishes with the terrain observation, so rendering cannot change its
mesh resource after a failed load. Moving meshes rebake only after a pose or
transform change. A new native skinned mesh replaces the cached source instead
of freezing the first pose. Reset restores original resources and frees owners.

## Combat events and clocks

`combat_play::Session::visual_events()` returns owned observations from the
latest ready tick: nonzero `damage_result` entries are hits; `obj_delete` is the
death queue. Actor addresses are resolved to fixture slots and copied native
positions while handles remain valid. This does not alter native combat, RNG,
hit timing, death timing or next-tick removal.

`ChampionsSessionController.combat_visual_tick(packet)` emits once for every
successful combat ready tick, including every tick within an `advance_time`
catch-up batch. Its packet contains `tick` and `events` (`kind`, `slot`, native
`position`). No-ready input, pause, observations and rendering do not deliver
new events. The cosmetic adapter advances the released FX pool once per packet,
then routes hits/deaths through `spawn_event` with an independent cosmetic RNG.
Draw lifetime follows the released renderer's separate frame/frozen semantics.
Reset, mode replacement and a dry continuation load clear cosmetic pools; FX is not
part of the dry `.ocplay` format (an accepted presentation reset, see
[REVIEW_FIXES_2026_10_20.md](REVIEW_FIXES_2026_10_20.md) finding 9). A Full combat
fixture continuation carries the drawing bridge's exact state instead. Ordinary and level play use lighting but do
not invent combat events or damage effects for diagnostic damage input.

## Authored blood policy and retained refusals

The synthetic fight explicitly assigns the red-material policy. This is not a
claim about Skelight or Agony's original body material. Each accepted hit calls
the documented spawn-0 recipe (300 particles) and type-1 red splat. The observed
death queue calls the same documented burst and a type-0 red pool, with authored
sizes 4/8 and zero growth/rotation. These are a composed fixture policy, not a
reconstruction of an unknown original death recipe.

| Menu setting | Burst | Hit splat | Death pool |
|---|---|---|---|
| Blood off (0) | None | None | None |
| Blood splats (1) | 300 per hook | Yes | No |
| Blood and pools (2) | 300 per hook | Yes | Yes |

The native gates implement these differences. Body parts, disintegration and
ice-death recipes remain unsupported. Glow, trail and beam primitives stay
disabled; kind-2 per-vertex alpha and original texture/projection gaps retain
the owner's refusals. Native audio is still absent. No fallback draws guessed
geometry or mutates shared gameplay RNG.

## Acceptance

`combat-play` checks hit/death observations before removal. The combined
`godot/tests/playable_wave_smoke_fx.gd` inherits the complete long-play,
continuation, HUD, inventory, level-gate and animation journey. It proves:

- Two native hits give HP 84 and 600 particles; floor splats appear in the real
  MultiMesh. Gore 0/1/2 changes the burst/splat/pool counts as documented above.
- Player retaliation produces a hit before death, then one death hook and
  next-tick removal without duplicate FX. Combat RNG retains its exact seed.
- The real generated animation event gives damage and FX on tick 37, never 36.
- Native pose vertices remain current after lighting at tick 38; static WORLD
  colours are not multiplied repeatedly. Pause and no-ready calls are inert.
- Invalid explicit JSON is atomic; reset/load clears cosmetic owners; glow,
  trail and beam queues remain empty.

All inputs are synthetic fixtures. The animated carrier and bounded combat /
level policies retain the limitations documented in their separate contracts.

## Wave 11: native-fx-2 output and the Full combat effects

native-fx-2 removed the round-1 approximate primitives (`spawn_event`, the
MultiMesh presentation and its snapshots). The scene now draws only the bridge's
exact GS-order runs through `NativeFxRenderer`: decals first, then glow pass 0,
particles, glow pass 1 and trails, later runs over earlier ones, depth-tested
against the 3D scene without depth writes. The game camera is a client
derivation, outside the effect contract: the scene camera's right / down /
forward axes in native XY / Z-up form the row-vector view mat34 (also used as
`g_cameraMatrix`), `proj_scale` = 480 / tan(fov_y / 2) for the 480-line half
height at 4:3, and `ChampionsFxBridge.game_camera` builds `g_viewProj` with the
same `mat4_set_projection` inputs as the native fx-sim tests. Runs whose
primitives were all culled are skipped (Godot refuses empty surfaces).

**Full combat fixture.** `combat_play::Effects` (native) holds one
`EffectSimulation` per session and runs it inside every ready tick, with or
without a renderer. Call-outs the combat World records with their arguments
(`particle_spawn` with a definition, `fx_spawn` with an id and position,
`decal_add`) go to `EffectSimulation::answer`; effect-class gameplay call-outs go
to the World (`damage_apply` → `World::damage_descriptor`, box queries →
`collect_box`); shared draws use the World's `g_randSeed`. Presentation call-outs
the trace records without the arguments a GS output needs (trail points, sounds,
block sparks without a definition, LightEffect colours) are counted by name in
the observation's `effects.unrouted`, never drawn from guesses. The scene's
bridge world is attached with `attach_effect_bridge`; its priority-15 update runs
natively after the World pass, and `draw_effect_slots` runs `HolyStrike_draw`
once per rendered frame. Detaching it (headless) changes no seed or health.

**Other fixtures.** The authored blood policy above keeps its hooks but now calls
the exact `fx_spawn` 0 recipe (300 particles per hit; it returns at gore 0
natively) and `decal_add` with the native detail gate, on the same bridge.

**Synthetic inputs.** Particle definitions are the smoke test's authored row keyed
to the spawn-0 burst and Holy Strike's three puff keys. Sheets are generated at
the documented sizes (particle 512 × 160, trail 128 × 448, floor 256 × 128, eight
32 × 32 radial glows registered with `set_glow_texture_size`). Recipe constants
are the native test fixture `fx_skill_recipes_synthetic.json`, exported as
`res://fx/` and labelled in the UI. The explicit definitions picker still
replaces only definitions; user GOB/LMP textures and a user recipe JSON remain
caller-supplied and were not available here.
