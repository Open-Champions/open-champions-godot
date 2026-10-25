# Original creature animation, bounded RTA implementation

`champions-creature-anim` implements the documented Return to Arms US v2.00
pose, blend, root/yaw and SmallSpider animation route. It is a separate portable
C++17 library. Existing `animation_runtime`, Session, Controller, save and
`scene_animation_*` code is unchanged. This is the future scene preview's API;
this job does not connect that scene to the new implementation.

## Immutable evidence

Research checkout (read-only): `<research-checkouts>/research-anim`.
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
A service whose Callable's object was freed is removed and refused by name before the
next operation, and every stored call checks `is_valid()` immediately before
`Callable::call`; the bindings release their Callables when destroyed (review
2026-10-20, finding 8; `godot/creature_anim/freed_callable_smoke.gd`,
`run_creature_anim_smoke.py --freed`). Configuration values that the engine uses as raw
integers must be integer Variants.
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
compare complete sparse engine memory and ordered writes in addition to complete
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
searched for. The full-body path above remains SmallSpider-only; the bounded
creature/player dispatch surface below extends animation selection. Equipment
assembly, original lighting and actual sound playback remain integration work.

## Round 3: creature tables and player animation dispatch

The extension also registers **ChampionsAnimationDispatch**. This surface applies
animation dispatch and produces poses for a host that already owns the filtered AI
state or player action. The original `ChampionsCreatureAnimation` SmallSpider
constructor/run/touch route above remains unchanged.

Research is pinned separately to **5694ae3c63e7ddff4b2177adf2ae783b23dd6a9b** in
[`creature_dispatch_sources.json`](../native/tests/vectors/creature_dispatch_sources.json)
(audit CAI-8; it was 4f9b057, and between the two only documents and a
`ref_creature_dispatch.py` docstring changed, so the numeric table is
identical). That manifest hashes all three new dispatch documents, the generated numeric table,
reference model/tests, the movement/player contracts needed for exact constants,
and `ref_skill_anim.py`, `ref_field_precedence.py`, `ref_own_machines.py`,
`test_ref_own_machines.py` and `own-state-machines.md`. The exact dispatch
vectors now include the skill states (`ref_skill_anim`: the seven state-6
constructors, charge, shadow step, and the class-4/6 and flag-0 refusals),
the two-field rows and helpers (`ref_field_precedence`), and one explicit
refusal for every row whose entry is a runtime clip pointer (`None`; Critter,
Skeleton) instead of skipping it; an unknown op kind stops the generator.
`tools/generate_own_machine_vectors.py` reads the two own-machine files from
the same pin and records the commit and hashes in `own_machines.txt`.
The previous round's source pins and vectors remain unchanged. The numeric C++ data
is reproducibly compiled from the pinned table by `tools/generate_creature_dispatch.py`.
No executable/decompiler/retail data is part of this process.

### Native table contract

`champions/creature_anim/dispatch.hpp` exposes:

- `select_dispatch(class, filtered_state, Choice)` returns ordered operations and
  the recorded final state. All **47 root-motion, 10 steered and 10 special
  classes** are admitted. The special classes are the ten own-machine runs
  (Critter, Soul, NPC, Natasla, Mermaid, GnomeNavigator, Skeleton, Skelight,
  Gnaw, AntQueen). The **6 no-dispatch classes**, unknown states, unknown
  conditions, and unobserved nonperiodic random choices refuse. Agony state
  0x12 reads word +0x2dc as a mask: any bit of 0x380000 selects the knockback
  clip and wins over 0x40000; otherwise 0x40000 selects the hit reaction.
  The preserved SmallSpider full-body path is still available independently.
- `Choice.random` is the original **clip-choosing draw's result**, supplied after the
  host's original RNG ordering. It is not a request to consume a new draw. A single
  recorded field/value or all-stubs-return condition may be selected explicitly.
  Unrecorded combinations of fields, or nonzero random plus condition, refuse.
  The table does not establish arbitrary live-memory predicates or state-filter
  behavior; the caller must not pass a baseline choice for unknown live conditions.
- `apply_dispatch(context, selection)` invokes the exact start/blend/push/stop/sync
  operations on an owned Runtime. Definition entries resolve at each call. Steered
  walk/run uses the table's flags with **no ROOT bit**. No generic locomotion flags
  are substituted. The host records `selection.stored` only after successful calls.
- Virtual/holder operations require an explicit synchronous handler and retain their
  original position in the sequence. In particular, death's slot 28 is called after
  starting its end-held clip. Slot 27 redispatch goes to the host; we do not silently
  expand it with zero conditions. Constructor ops are inspectable numeric data;
  they are not a complete constructor/loot/path/RNG implementation.
- `install_dispatch_events` installs the four slots on shared clip identities.
  Goblin variants `26a624/v1` and `26a624/v2` are explicit. Null resources are skipped
  as in the Python installer; missing entry mappings refuse. Later writers remain
  visible to every actor sharing that clip identity.
- `dispatch_events(class,state,word)` returns observations `{kind,consumer,state,
  mask,word}`. Kinds include `hit_test`, `footstep`, `corpse`, and other `consumer`
  records. The complete sign-extended word is retained. A corpse consumer receives
  **zero too**; mask 0 means any nonzero word. Null stub target `000000` is excluded.
  Masks and names are the actual per-state rows; no universal bit-1 damage rule or
  bit-4 footstep is fabricated. These are dispatch observations, not executed combat,
  RNG, stores, targeting or effect creation. Conditional consumers still require
  their engine service's original predicates. Order between separately probed masks
  is not evidence of original run-call order; output is stable ascending mask order.
- `dispatch_transition_probe` exposes only the executed zero/all-set flag probes.
  It refuses intermediate flag words. Some rows have multiple transitions or extra
  unknown predicates; the API does not turn the probes into an invented AI state
  machine. The host consumes ACT/HIT union bits and supplies its next filtered state.

The contract itself still labels out-of-probe and multi-field conditions, complete
RNG draw ordering, and consumers behind unexplored branches as unknown. Support for
57 classes means their recorded **animation dispatch** is executable, not that their
complete combat AI, constructors or run bodies have been reconstructed.

### Player contract

Player calls use `player_action(action, PlayerInput, NamedClip)` and
`apply_dispatch`. Explicit named actions distinguish cases sharing a state number:
`shot`, `first_swing`, `heavy`, `close_attack`, `combo`, `combo_close`, `script_idle`,
`script_walk`, `stagger`, `knockdown`, `flinch`, `death`, `cast_loop`, `cast_oneshot`,
`block_pose`, `block`, `block_reaction`, `ground_pound`, `evade`, `warcry`, `ride`.
The host calls them when the documented predicate is true; these functions select
and apply animation calls, not input handling, damage resistance, immunity, cooldowns,
track preparatory stores from the surrounding player body, or release scheduling.
`script_walk` includes frame resync. Death selects the class/model-specific header
entry, block reactions use the documented draw boundary, knockdown names use signed
relative-hit boundaries, and dynamic resources resolve by exact documented names.
Missing named resources refuse except the documented seated-to-idle riding fallback.

`reset_player_idle` writes the four persistent cells (flags 0x1205) and preserves
unwritten rates/times before cache release. Call only after the host's object/HP
checks; display-only idle names are explicit. `player_locomotion` receives the already
scaled/capped/slow-adjusted stick magnitude plus blend and previous-speed state.
It preserves operation order, the 0x3edc28f5/0x3f333333 rate knees, the acceleration
subtraction comparison, and software-binary64 weights (including the exact
0x3fdccccccccccccc constant for 0.45). It steps before assigning the new weights. Its standalone step calls raw
`Runtime::advance`; any additional Player_animStep speed modifiers must be handled
by the integration owner before using this bounded animation-only route. It is not
a substitute for the full movement/Player_animStep call chain.
Movement modes 1/4/6 zero the blend/locomotion weights without stepping; 2/3/5 leave
animation unchanged. World velocity/collision remains the movement owner's work.

`player_fidget` consumes the supplied shared-draw service only at the documented
four-track branch; moving cancels/fades without drawing. `player_combo_variant` is a
pure lookup using the frame hash, model, weapon class and old variant. The caller
must still consume the original shared draw; the Godot `player_combo` method requires
and invokes that service exactly once. The model-9/class-1 and model-11/class-3 combo rules are absent from
the document's table and refuse. No guessed clip is substituted.

`install_player_events` writes only slots 0/1 on rows 0..5, preserving slots 2/3
and leaving dual wield row 7 untouched. `install_ground_pound_events` is separate
for the skill's four writes. Player emissions include footsteps and ground pound
(copy players get the sound/voice observation but not area damage/effect). **Melee,
arrow/throw and cast releases are not animation events**; their frame/weight timing
remains with the existing player/combat owner. State **7** still refuses: no
direct store of it was recorded. State **6** is Cyclone (`cyclone1.anm`, and a
lookup of `spel_bw_cyclone.anm` that is not a second player clip), Bash and
Shield Bash (`bash.anm`), and the class-0 shouts Ancestral Call, roar, battle
cry and blood lust. Ancestral Call stores 6 on the selection the host applies
after the blend. All seven ran only on a weapon-class-0 player whose object
flag word was 0x40, so any other weapon class or flag word refuses ("skill
constructor not executed for this weapon class/flags"). State
**9** is Charge: it stores 9 only when object flag 0x40 is set and starts no
clip. State **0xe** is Shadow Step (`spel_de_shadowstep.anm`); its destructor
returns the player to state 2 and restores flag 0x40. Only the flag pair
0x40 → 0x45 was executed. Bow/thrown warcry's unwritten stack buffer also refuses. Marker-exit observations
are exposed only where a marker alone establishes the documented next state;
state 0/5/0xb exits require extra timer/phase/input conditions.

### Godot table surface

Create `ChampionsAnimationDispatch` and call `configure` with:

- `profile: "rta-us-v2.00"`, `class: <documented creature> | "Player"`.
- `clips`: resolved resource integer → PackedByteArray; `entries`: original
  definition index → resolved resource integer; `base_resource` identifies a clip.
- `model_resource`, `texture_resource`, `model_bytes`, optional model `variant`
  and positive `units`. Creature entries 1/2 must match model/texture identities.
  Player model/equipment assembly is supplied by its owner; this API does not infer it.
- Optional `named_clips`: exact player clip name → registered resource integer.
- Optional `shared_with`: another configured **ChampionsAnimationDispatch**. Both
  actors share the Runtime and clip/event identity. Do not mix this surface with a
  separate Runtime's original `ChampionsCreatureAnimation` event-memory bridge.

All results use `{ok,error?}`. `select_state(state, choice, frame)` accepts `{random}`
plus either `{field,value}` or `{return}`. It does not advance time.
`set_virtual_handler(Callable)` receives `(kind, arguments)`. For holder calls and
virtual slots other than 27 it must return `true` after executing the original host
service. Slot 27 instead requires `{filtered_state, choice}` from the host filter/RNG
service: the adapter synchronously applies that nested selection before the outer
state store, with a depth cap of 16. A boolean acknowledgment of slot 27 refuses.
Missing/false handlers refuse. A handler whose object was freed is removed and refused
by name. Public callback reentrancy is refused; failures preserve
any executed prefix and are not rollback.

`install_events(variant)` initializes class slots; pass `""` for baseline/player.
`player_action(name,input)` selects a documented player action. Its `ground_pound`
action uses the SkillGroundPound path and installs the four event slots; native
callers can apply the older action without that separate installer. `player_reset(input)`
sets the four base cells and neutral observation state. PlayerInput fields are
`weapon_class`, `model`, `variant`, signed16 `relative_hit`, `draw`,
`no_combat_level`, `display_only`. `player_fidget(stick_raw,weapon_class,shared_draw)`
and `player_combo(model,weapon_class,old_variant,frame,shared_draw)` expose their
separate services. `transition_probe(flags)` returns `observed_targets` only and
never automatically changes state.

`tick(frame,speed_raw,copy,scripted_walk)` advances exactly once and returns owned
`positions`, `normals`, `pose_rows`, `joint_count`, union `flags`, `state`, raw `events`,
`emissions`, selected model/texture resources and `root_x_raw/root_y_raw/root_yaw`.
Positions/normals use the same Godot basis conversion as the original binding.
**Root deltas here are in animation-local coordinates**, explicitly marked by
`root_space`; the host must apply the existing Creature/Player movement transform,
scale, slow factors and collision exactly once. This lower-level surface does not
pretend to have run Creature_moveBy. Duplicate/stale ticks refuse; a failed tick is
consumed. `player_move(frame,speed_raw,mode)` performs the documented locomotion
animation step and returns the same pose plus `rate_raw` and `blend_raw`.

Run the additional synthetic adapter/render smoke with:

```sh
python3 tools/run_creature_anim_smoke.py --godot /path/to/Godot --dispatch
python3 tools/run_creature_anim_smoke.py --godot /path/to/Godot --dispatch --render
```

Current validation results are in the job's latest `work/loop-creature-anim-rN-report.md`.
Neither these synthetic fixtures nor table parity establishes original-asset visual
acceptance, cross-platform traces, a complete player tick, or complete creature AI.
