# Native visual effects and lighting (Return to Arms)

This is a cosmetic, tick-driven port of the read-only Return to Arms v2.00 research
contract at `d7922ef96e9fc3b812a4f3ce441478e93f3f44ff`. It does not affect
authoritative combat or multiplayer state. It does not establish Norrath behavior.
The research describes code read from the user's executable; a live original-game
difftest has not been run.

## Evidence pin

SHA-256 of the files read in `~/Projects/champions-re-wt/research-fx`:

| File | SHA-256 |
|---|---|
| `docs/effects.md` | `307ddab2bccfc3a0087ddf8d10f659ef994ed83486bab86d598612d63312a074` |
| `docs/lighting.md` | `4849318da3cc25e941fdd82f69726c5f11fd886f6885c04282479326770dbd32` |
| `symbols/effects.tsv` | `dc3e4cc831350faa6cf1b6e818b737cc039a672f3d752e7353791fee8375e35f` |
| `tools/ref_particles.py` | `5ae0c722cf95b4f4d5b929b601bfa50d32610bfd042be2577015fa02c0feb94f` |
| `tools/ref_lighting.py` | `8e491566d762d585151cb8e28b8eabfa1416e0f894b91c6f08e564f5072813d3` |
| `tools/fx_particle_defs.py` | `b3565cf30132a829f4aa05c3ea01e13efb2afc62bfd0b2cef50f0cbf76417b42` |
| `tools/fx_callsites.py` | `f643e004baf1bbd313dee9dfa77f1f5effe966c4d879eb73150f75d38acaa72e` |
| `tools/test_ref_particles.py` | `828672bc5a58b9db3e2a94f054e1dbcb995a73c0f27ce26b64c4be01223b8aa1` |
| `tools/test_ref_lighting.py` | `1d4dfa02a445d44c3ebf11d38f10a9f84acd9dfa4ae8d144fc2420faa0c3ea0e` |

## Inputs and boundaries

The 0x50-byte particle definition **values are not in this repository**. The owner
generates JSON from their own executable with
`python3 -I tools/fx_particle_defs.py /path/to/SLUS_209.73 --json`, stores the
result outside the checkout, and loads it at runtime through
`ChampionsFxBridge.load_definitions_json(text)` or
`NativeFxRenderer.load_definition_file(path)`. The loader accepts complete
callback-free rows with finite values and rejects a partial input atomically.
Runtime emitter definitions are built from the shared 0x48-byte level chunk /
AddParticle record; they likewise carry no compiled retail definition values.

`champions-native-fx` is portable C++17 with no Godot dependency. A `System`
owns a 64 × 240 page pool, 32 resident definition slots, 16 list indices,
16 groups, a signed-16 tick clock, FIFO retirement and the original silent
capacity drops. Birth positions and velocities use truncated signed-16 fields;
positive backdate follows the research formula. A particle lives through age
11, 30, 60 or 90 (inclusive), as selected by its life class. Rendering computes
motion, size and colour from age. A draw step advances shared flipbook records
on rows 1, 2 and 4. Kind 3 is retained but never drawn.

Emitters preserve rate, timer, weather gate, mirror flags, rotations, and cosmetic
LCG draw order. Level chunks must call their emitter only while drawn, using the
rendered frame; object helpers call the same runner from their tick hook.
The provided API does not schedule either caller automatically.

The primitive store includes a 1024-glow queue, 64 trail handles and point
rings, 20 beam handles with depth and expiry, and the 30 × 30 decal grid with
64 occupied cells, 15 decals per cell and 16 growing slots. Floor decal
rectangles use the documented `floorsprite.tex` cells. Decal colour freezes
the ambient, sun and flagged point-light contribution at creation. Gore detail
0/1/2 gates decal types as documented. `spawn_fx` accepts the documented
one-shot IDs; definition addresses are keys looked up in runtime JSON.

Lighting provides the unscaled base level, per-frame sun/ambient scale, unscaled
fill, per-object tints, 90 × 90 point-light collection, two-best model lights,
four-light WORLD vertex terms, the level light chunk modes, fire radii,
`LightEffect`, and decal tint. Sun visibility is fixed at 1.0. Fog is absent.
The Godot bridge exposes per-vertex model and WORLD colours. The renderer can
apply them to an `ArrayMesh` with normals and a material that uses vertex colour.

## Godot use

Add `godot/fx/native_fx_renderer.gd` to a `Node3D`. Supply the user's particle
texture and its **measured** sheet height and, for decals, the user's
`floorsprite.tex` texture. No sheet height or unknown glow texture filename is
invented. Call `advance_tick()` once per simulated tick, and
`present(sim_frame)` for each rendered frame. Model and WORLD meshes can call
`apply_vertex_lighting(mesh_instance, true/false)` after node changes. A
nonstandard material must be adapted explicitly to consume vertex colours.

The bridge methods `spawn_particle`, `add_emitter`, `run_emitter`,
`spawn_event`, `add_decal`, `add_glow`, `create_trail` /
`add_trail_point`, and `create_beam` / `update_beam` are hooks for
combat-runtime hit/death,
creature-anim events, and skills. Call a skill's cast hook on its cast tick, its
run hook each simulated tick, and its impact hook on that event tick. The
effect-class recipes are **not** inferred from the callsite map: the caller
must supply an effect ID or a fully authored emitter/definition. No new
simulation coupling has been added to another agent's files.

The renderer uses separate MultiMeshes for blend 0 (`blend_mix`) and blend 1
(`blend_add`), with no depth write. Kind 3 is omitted. It intentionally refuses
kind-2 lines while their second-vertex alpha is unresolved. `present()`
returns explicit render refusals for lines, glows, trails and beams; their
records can be inspected through bridge snapshot methods. Its exact visual
match to the original has not been measured in an exported application.

## Explicit refusals and gaps

- `fx_modelStatusEffects` counts, disintegrate, ice-shatter and body-part
  recipes have no exact implementation.
- `fx_spawn` IDs 7, 0x14, 0x15 and 0x16 return `UnknownDirection`;
  ID 5 returns `UnsupportedRecipe` because it creates Fire objects and uses
  the shared seed. IDs needing a missing runtime definition return
  `MissingDefinition`.
- IDs 6, 0xb and 0x12 also return `UnsupportedRecipe`: the prose does not
  settle the axis term of 6, the velocity of 0xb's extra five particles, or
  the velocity of 0x12. Their IDs and definition addresses remain mapped
  without assigning invented values.
- The kind-2 second-vertex alpha, glow texture names and particle sheet height
  are unresolved. The Godot renderer returns a refusal for the sheet and
  exposes its line refusal. No fallback value is silently substituted.
- Trail and beam geometry, glow projection and skill-specific visual classes
  still need exact rendering work. Their native records and scheduling hooks
  do not constitute a visual match.
- Group page lifetime and slot behavior are implemented; the research does
  not give enough matrix detail to render a group's local particles through
  its mat34, so that transform remains unsupported.
- The research was read, not live-difftested. Private game files are unavailable
  on this machine, so there are no private acceptance counts. Exported Godot
  visual inspection is a separate gate.

## Verification

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R native-fx-synthetic --output-on-failure
ctest --test-dir build/native -j 3 --output-on-failure
```

The native synthetic assertions port the reference examples for definition
layout, records and backdate, life and capacity boundaries, emitter timing,
flipbook rows, primitive limits, light scale/tints, selection and vertex
shading, chunk modes, Fire/LightEffect and decal tint.
