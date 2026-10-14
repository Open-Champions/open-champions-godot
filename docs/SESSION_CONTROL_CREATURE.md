# Dry creature movement in the controllable scene

The Godot movement and health test now explicitly selects
`session_control::MovementPolicy::DryCreature`. Its integer keyboard commands run
the original modeled `Creature_moveBy` through the existing
[Session creature adapter](SESSION_CREATURE.md). Both initial scene construction
and successful terrain-file replacement select this policy with a **512-ready-tick
limit**. The native Controller's default remains `Isolated`, preserving the older
control contract and its v1 fixture bytes.

This is an authored control policy, `rta-session-control-creature-v1`, around an
original movement body. It is not original `Player_move`, a playable Champions
game or evidence of original-console network compatibility.

The optional [recovery test](SESSION_RECOVERY.md) adds explicit health/mana
recovery after movement and the health test, with its own 256-ready-tick cap.
Recovery remains disabled in the legacy policies described here.

## Input, ownership and execution

The controller still owns exactly one actor in an actual one-peer network-mode
Session. No fabricated ready packets or direct presentation-position updates are
used. The first command submission waits; the next ready tick consumes the earlier
imported `Pad.held` state. WASD supplies fixed raw velocity, opposite directions
cancel and diagonal axes retain their full independent speed. The presentation
clock chooses when to submit a command; elapsed time and Godot physics do not
enter gameplay arithmetic.

The actor is explicitly a base creature with flag `0x20`; its HP backing carries
dry flag `0x100000`. Movement uses that same HP Memory, World, handle pool and
terrain Level. The caller-owned policy supplies authored dispatch storage at
`0x10000000`; it does not discover or claim an original retail vtable address.
There is no isolated-mover binding in this policy. The ready task invokes
`move_creature` and then consumes any matching health-test edge, applying the
existing amount-two HP operation with null attacker.

H rising edges remain queued by submitted input-frame identity and apply once on
the matching ready frame. Pause, focus loss and dialogs release inputs and discard
pending edges. Buffered commands older than the release cutoff drain neutrally;
resume does not apply queued movement catchup. Stop preserves state. Reset creates
a fresh Session with the same policy, limits and initial terrain. Invalid command
bits refuse before mutation, including while paused. Operational failure retains
the completed prefix, stops the controller and requires explicit reset.

Movement publishes original root/timer, position and vertical-velocity results,
then HP observes the updated position. Shared World, object-local and combat random
states remain separate; dry movement does not duplicate or advance them. Native
observations expose the **actual active policy and configured limit**, rather than
having the bridge infer them from a desired configuration.

## Bounds and complete observations

`DryCreature` accepts an explicit ready-tick limit from 1 through 512; the legacy
2,000-tick default is refused for this policy. The fixed controller's conservative
history allowance is 96 entries per ready tick: at most 42 movement entries,
23 HP entries including its combat event, and two scheduler events fit within it.
The sole local peer has no remote packets, error logs, scripts or arbitrary
callbacks. Setup and scalar projection use unlogged pokes. At 512 ticks the bound
leaves space for movement preflight and the existing Session bookkeeping reserve.
No additional history clearing is introduced. A whole-budget API test exercises
512 ready ticks and 256 health edges. It passed optimized and under sanitizers
while retaining every log and stopping exactly at the limit.

The new complete trace retains every Controller field, including movement policy,
active cap, command queue and release cutoff; full Session, World, network, HP,
terrain and creature snapshots; and typed callback arguments/results. Shared raw
backing and its store log appear once under HP. Full initial states and lossless
per-action deltas reconstruct every later observation. Older complete control
formatters refuse the new policy, and older Session formatters refuse an active
creature extension. The unchanged legacy 101-case control and 281-case Session
creature fixtures remain separate regression gates.

## Terrain, presentation and remaining scope

The existing terrain-file loader and bounded flat-ground presentation are retained.
The bridge constructs both replacement terrain and a new DryCreature Controller
before publishing either; unsupported files leave the prior scene intact. Reset
retains selected terrain. Optional cosmetic model selection still changes only
appearance. No viewer extension, new asset decoder or physics replacement is part
of this change.

Original player controls, animation-generated root motion, water services,
derived creatures, other-actor dynamic contacts and Norrath remain unvalidated.
The original standalone water/settle bodies are not installed as scene services.
Windows visual behavior needs its own executed evidence; macOS results do not
establish it.

## Evidence

The independent corpus contains **151 cases**, with movement, health edges and
supplied-terrain recipes covering all **16 arithmetic configurations**. The oracle
combines immutable SHA-verified original models through pinned authored controller
and Session-creature helpers. It executes the real single-peer machine and
original movement services; no expected state comes from native output. Negative
command cases prove refusal leaves state unchanged. Native-only workload and
identity failures are tested separately from Python parity.

Expected exact trace SHA-256:
`1563f50296e9e0eff04cbe8d897cd3e9a80ef8b0b9d90c71d9f48321f53a3db7`.
Three separate final generator runs produced identical gzip and manifest files;
all seven atomic-publication failure tests passed. The Python suite passed **397
tests**, including **213 explicit native-path/platform skips**.

The macOS source and exported application passed **46 scene checks** and **283
archive-browser checks**. Physical UI inspection on the default ground verified
held-D movement across the displayed grid, H changing health 100→98, pause and
reset restoring 100. This verifies the inspected local application and ground,
not every supplied terrain or another operating system.

All **81 local CTest cases** passed. The API executable reported **1,834 checks**,
including the whole 512-tick run; all 151 exact cases and transport checks passed
optimized and under ASAN/UBSAN. All **27 linked runtime source units and both
entrypoints** were independently confirmed instrumented; leak detection was
disabled. LLVM20 formatting passed all tracked native source and owned C++
files; GCC15 syntax and the simulation boundary checks passed. Three independent
reviews found no unresolved implementation, provenance or observation blocker.

The preceding 78-case Intel Mac PR job took 43.5 minutes overall, including
33.6 minutes of CTest. The expanded controller suite therefore gives native
jobs a 60-minute ceiling while retaining every test and artifact gate. This is
time allowance for the larger workload, not a performance improvement claim.

Final target-platform CI remains a separate gate and must identify the tested
head and actual artifacts. No broader execution claim is implied by the
generated corpus or local visual checks.

The optional [facing test](SESSION_FACING.md) composes the existing turning
adapter before movement, with its own explicit 256-ready-tick policy.
