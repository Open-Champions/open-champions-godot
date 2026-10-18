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
Reset, mode replacement and continuation load clear cosmetic pools; FX is not
part of the gameplay save format. Ordinary and level play use lighting but do
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
