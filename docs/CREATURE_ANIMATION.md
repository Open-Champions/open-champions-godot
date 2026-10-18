# Original creature animation, bounded RTA implementation

`champions-creature-anim` implements the documented Return to Arms US v2.00
pose, blend, root/yaw and SmallSpider animation route. It is a separate portable
C++17 library. Existing `animation_runtime`, Session, Controller, save and
`scene_animation_*` code is unchanged. This is the future scene preview's API;
this job does not connect that scene to the new implementation.

## Immutable evidence

Research checkout (read-only): `~/Projects/champions-re-wt/research-anim`.
Commit: **2502e2bfac58203ac0174fccd7e0953b36d06439**. Every source used by the
oracle is checked against its Git blob and SHA-256 before import.
The full source and test pin list is
[`creature_anim_sources.json`](../native/tests/vectors/creature_anim_sources.json).

| Contract | SHA-256 |
|---|---|
| `docs/spec/rendered-creature-animation.md` | `ec303b56dbd2019070edada2050fb401a454525948a017079458dc5f9d045abd` |
| `docs/spec/research-anim-handoff.md` | `192189db8e4339411ac5e7b41c7f8d368acb7afe8f534a1532ce0c7b2a0ddd82` |
| `tools/ref_anim.py` | `6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `tools/ref_creature_animation.py` | `3d55ce6558f398fa287ca09d977a75795a65107365a0e0c6a0485e414e658344` |

The base native commit is `60def02`, with the Session/cache results documented
in [ANIMATION_RUNTIME](ANIMATION_RUNTIME.md), [SESSION_ANIM_CACHE](SESSION_ANIM_CACHE.md)
and [CONTROL_ANIM_CACHE](CONTROL_ANIM_CACHE.md). The new library reuses the
existing native raw32 decoder, arithmetic and monster class bodies. Its lifecycle
code extends an isolated copy of the native runtime, preserving the original
runtime's explicit ROOT/YAW/pose refusals and checkpoint ownership.

## Native ownership and order

Headers: `champions/creature_anim/runtime.hpp` and
`champions/creature_anim/creature.hpp`.

- `Runtime` owns clip identities, canonical track cells, decoder slots, quantized
  pose buffers, persistent VU stack memory, frame counters and cosmetic seed.
  No host floating-point arithmetic enters this library.
- `Environment` owns the shared Runtime, sparse engine Memory and class resource registry.
  `Creature` holds that Environment and an existing monster Model; construct actors
  with the same shared Environment to retain class clip/event identity across instances.
  `Environment::register_clip(resource, bytes)` reuses the identity and refuses
  changed bytes. Rebinding another actor never resets a shared event table.
  Supply the real engine services through `model.callbacks` / structured callback
  interfaces. Missing services refuse; production never supplies no-op services.
- `bind(Pairing, Mesh, Dispatch)` associates a class definition's resolved model,
  texture and clip resources. Every registered clip must share the base clip's
  ordered depth/bind skeleton, and all weighted vertices must address that
  skeleton. Count similarity and filenames are not pairing evidence. The default
  dispatch is SmallSpider; further classes require explicit contracted Dispatch
  functions. This API makes no Norrath or player compatibility claim.
- `construct`, `change_state`, `touch` and `tick` invoke the existing SmallSpider
  bodies. State filtering, own RNG, cosmetic constructor phase, path/decal/death
  effects and callback ordering remain there. Native animation callbacks map
  resolved resources to shared clip identities and publish state flags at +0x11a.
  Constructor cosmetic draws share engine memory `0x404460`. Four event entries
  are reread from shared clip memory before advance, so constructor event writes
  are visible to every track using that identity.
- `tick(simulation_frame, draw_frame)` invokes the original run body once, then
  returns an owned Frame. Duplicate or stale simulation frames refuse (unsigned wrap is supported);
  a failed call consumes its frame to prevent replay of a partial side effect.
  The consumer supplies ready frames; this is not a scheduler or network fence.
  `draw(draw_frame)` only obtains/skins the current pose and never advances time.

`Frame` contains signed16 pose rows, exact rational skinned positions and normals,
the raw32 object mat34, selected model/texture resources, event bits and the
stored world root delta/yaw. The existing Creature run/movement bodies consume
motion through their collision services. Do not add returned root motion to the
Godot actor a second time. Object height uses +0x3c, not simulation Z at +0x18.

Low-level `Runtime::advance` uses `speed*0x3f7fffff`, preserves loop subtraction
order, updated second-pass root weights and unweighted bottom-up yaw. Its
`creature_step` helper takes speed after external player/Drowsy multipliers and
preserves the inactive retransform and second slow factor. The full class path
uses the existing monster Model's own Creature_stepAnim wrapper instead; never
apply both wrappers to the same tick.

Pose construction uses the full-weight base search, mask exclusions, bottom-up
component blending, quaternion sign selection and original w unwrap, followed by
one normalization. The top clip supplies depth/bind entries even when waiting or
weight zero. Hierarchy composition stores uncorrected W on the depth stack and
applies bind correction only to quantized output. FTOI truncation and low16 wrap
are preserved. Start/clear alone retain a cached pose; advance, cache release,
jump and snap invalidate it. Only draw stamps pooled poses. Attachment queries
use `joint_matrix` without advancing time. Low-level Runtime retains stale depth
behavior for reference parity; Creature's host admission requires preorder depth.

Skinning consumes already decoded joint indices (the model parser divides VU
addresses by four). A rigid run becomes weight 255 per vertex. One through four
weights must sum to 255, with no renormalization. Positions are exact numerators
over **16711680** (`255*4096*16`); normals are over **132648960**
(`255*4096*127`). No inverse bind is applied again. This reproduces the Fraction
geometric oracle, not the VU1 instruction rounding/lighting/material pipeline.

Decoder/state/loop budgets are inherited from the native runtime. Pose pools
admit 2..1024 entries; mesh input admits at most 600,000 corners. Engine histories
and sound range records are bounded. A long-lived native consumer must consume
and clear `memory`/`model` histories and `runtime.sound_log` at its chosen observation
boundary. Sound ranges are recorded only; audio playback and its cosmetic RNG
consumption remain host services. The caller supplies any sound-group limit table.

## Godot API

The independent extension is `gdextension/creature_anim`. It registers
`ChampionsCreatureAnimation` through its own entry point. Its opt-in descriptor
is staged at `gdextension/creature_anim/creature_animation.gdextension`. Copy it
to `res://creature_anim/` when enabling this extension; the built libraries go
under `godot/creature_anim/bin/`. No existing registration or project.godot is edited.
Build against the repository's pinned godot-cpp commit:

```sh
cmake -S gdextension/creature_anim -B build/creature-godot \
  -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=/path/to/pinned/godot-cpp
cmake --build build/creature-godot --target champions-creature-animation -j 4
```

The descriptor uses distinct `template_debug` and `template_release` library names.
A release export must build `-DGODOTCPP_TARGET=template_release` and package that library;
do not mix debug/release ABI libraries. Other desktop architectures require their
own build and descriptor entry. Only the local Apple Silicon build is verified.

Every method returns `{ok: bool, error?: String, ...}`. Failure preserves executed
native prefixes; it is not a rollback or permission to repeat a failed tick.

`configure(config)` replaces the binding owner after validating the supplied configuration.
With `shared_with`, shared resource/memory initialization retains failure prefixes;
there is no world rollback. Configuration keys:

| Key | Type / meaning |
|---|---|
| `profile`, `class` | Exactly `"rta-us-v2.00"`, `"SmallSpider"` |
| `shared_with` | Optional other configured ChampionsCreatureAnimation; actors in one world must share this environment |
| `address`, `definition` | Raw32 object and class-definition identities |
| `model_resource`, `texture_resource` | Resolved class resource identities |
| `model_bytes` | Selected model's PackedByteArray, decoded by the existing native parser |
| `clips` | Dictionary of resolved resource integers to `.anm` PackedByteArrays |
| `base_resource` | Registered base clip's resolved identity |
| `memory` | Array of `[address, width, unsigned-value-bits]` setup records |
| `variant` | Optional explicit model variant, default -1 |
| `units` | Positive finite Godot units per game unit, default 1 |

Configuration does not synthesize engine tables or a Creature constructor.
Provide real class entries, vtable entries and engine memory/services through the
integration owner. `set_service(name, Callable)` registers a scalar engine
service: one Array of raw integer/vector/structured arguments, integer result.
Animation services cannot be replaced. Missing/noninteger callbacks refuse.
Complex services can use the native Model's richer interfaces when embedding the
library directly; the Godot scalar bridge does not invent structured results.
Callbacks may use `read_memory(address,width)` / `write_memory(address,width,value)`;
recursive tick, reconfigure or dispatch calls refuse. `clear_history()` explicitly
discards diagnostic histories and sound ranges, returns discarded counts and
leaves simulation untouched. In a shared Environment, memory/sound histories are
world-wide; consume them at the world observation boundary. Native consumers can
read the complete histories before clearing them.

`construct(type_name, position_raw: PackedInt64Array, facing, properties)` invokes
the class constructor. `change_state(state)` and `touch(other)` invoke original
class dispatch. `tick(simulation_frame, draw_frame)` and `draw(draw_frame)` return:

- `positions`, `normals`: owned PackedVector3Arrays in Godot model coordinates.
- `object_transform`: Transform3D to apply once to the scene actor.
- `pose_rows`, `joint_count`: original quantized rows for diagnostics or a shader.
- `root_motion`, `root_yaw`, `events`: world-space motion observation and class event bits.
- `model_resource`, `texture_resource`: selected class identities.

The explicit basis policy is `B(x,y,z)=(x,z,-y)` and `B*M*B^-1` for transforms.
Units scale vertices, root displacement and object translation once. Normals use
B with no translation or unit factor. Rendering does not use Godot's default
skeletal interpolation. Mesh visibility, textures/materials and special draw
variants beyond the configured subset remain the consumer's responsibility.

Run the isolated smoke project after building:

```sh
python3 tools/run_creature_anim_smoke.py --godot /path/to/Godot
python3 tools/run_creature_anim_smoke.py --godot /path/to/Godot --render
```

The second command saves `work/godot-smoke/render-smoke.png`.
The synthetic `godot/creature_anim/smoke.gd` shows this API in an isolated fixture
context. Its named zero services are test doubles, never an original game engine.
It checks class pairing, missing-service refusal, dispatch, advancing geometry,
inactive retention, duplicate-tick refusal and ArrayMesh construction.

## Validation and remaining boundaries

The committed corpus has **37 exact cases**, including blend/root scenarios under
all 16 core FPU settings, sound/event order, wrap/copy/rebase, top skeleton,
quantized wrap, stale depth, pooled/default/fallback poses, and integrated
SmallSpider constructor/active/inactive/wander/death/touch scenarios, including
active wander and visible death completion. These scenarios
compares complete sparse engine memory and ordered writes in addition to complete
animation state. Expectations come only from pinned Python, never native output.
The source pin manifest includes the existing Python test suites (54 checks pass
when loaded from the pinned blobs). Independent native API tests cover 42
admission, geometry, lifecycle, shared-resource and callback checks. On local
Apple Silicon, the 37-case corpus and API suite also pass ASan/UBSan; leak
detection is disabled on macOS, so no leak result is claimed. Godot 4.5.2 passes
32 headless API checks and 33 checks with the synthetic rendered capture, which
was visually inspected. The native and Godot builds use Release and `-j 4`.

```sh
python3 tools/generate_creature_anim_vectors.py --research /path/to/research-anim
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -R '^creature-anim-'
ctest --test-dir build/native --output-on-failure -j 3
```

Current run evidence and exact commands are recorded in the job's ignored
`work/loop-creature-anim-r<N>-report.md`. Synthetic correctness does not establish
original-asset rendering, private acceptance, VU1 rounding or cross-platform
trace parity. Private inputs are unavailable under this job's rules and were not
searched for. Other creatures, player/equipment dispatch, original lighting and
actual sound playback remain explicit unsupported integration work.
