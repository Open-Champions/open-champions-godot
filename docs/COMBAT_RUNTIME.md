# Opt-in RTA multi-actor combat runtime

The isolated port described below is now consumed by the explicit synthetic
scene policy documented in [PLAYABLE_COMBAT.md](PLAYABLE_COMBAT.md). Its original
port proof and refusal boundary are preserved here; scene acceptance is separate.

`champions-combat-runtime` adds an independent native `combat_runtime::World`
and a ready-frame `SessionAdapter`. It has no Godot dependency. The existing
Session, Controller, save glue and `objects_lifetime.cpp` are unchanged. This
addition does **not** lift the old Session adapter's single-actor restriction or
make the current Godot scene a combat client. The session-play-runtime owner must
perform the wiring below.

## Immutable evidence

The implementation is authored from the documented US SLUS_209.73 v2.00 contract
at research commit **9289d564acadcdd8270b48ed9155ac6289cf17f5**:

| Research file | SHA-256 |
|---|---|
| `docs/combat-runtime.md` | `f5aa058d5e47e988dc63a6c366c4bd1de1ad2191fedbaa3c763a0f7e6643aee3` |
| `tools/ref_combat_runtime.py` | `328fbc428e26d50ac5cdb455d1614cf35148216151ba7904bb2ef92984e25ddd` |
| `tools/test_ref_combat_runtime.py` | `a462b1ce23611c246b8158eead09a8604a60d5dbe42f5f08f19a797feadce41d` |
| `tools/difftest_combat_runtime.py` | `1f5cf47aae712fa15010e26ea9eed413906a53559d9e8a6ccde0d8eb837e8a6b` |

The generator also pins all six imported reference modules; their full hashes are
in `native/tests/vectors/combat_runtime.json`. It reads immutable Git blobs, so a
research branch advancing does not change this port's evidence. The research
reports 30,988 original-instruction comparisons with zero mismatches. Those are
**research results**, not native tests or a validation run performed here. No
retail bytes, decompiler output or extracted assets are included.

Existing `SESSION_COMBAT.md`, `SESSION_CREATURE.md`, player, monster and arena
ports informed the boundary. The existing core arithmetic and handle/pool code,
creature movement, arena base damage/status/targeting and Skelight state-entry
bodies are reused. The new player helpers and queries follow the pinned runtime.
No behavior or profile is claimed for Norrath.

## State and order

One memory owns scalar actor state. The handle graph and shared random generator
are managed read-through regions of that image. The existing generic World stores
mutable scalar fields in C++ records, so it cannot become this shared-memory World
without changing files owned by another job. This new World therefore composes the
existing pool and handle primitives with memory-based lists and queries. It must
be the **sole World for its actors**, never a second shadow World beside Session's.

Pool addresses are object identity. Immediate call arguments may use addresses;
every retained target, attacker, status source, queued event and external actor
reference uses the one 6,096-node handle pool. `retain()` returns an external head;
`target(head)` resolves it. Destruction clears incoming references. Reusing a
same-size-class block does not resurrect cleared handles. Raw writes to registered
heads or nodes refuse. Callers explicitly relink grid membership after position
changes; same-cell updates go to the front without reallocating nodes.

`run_objects()` flushes the delete list, then walks active, frame-mod-15 and
frame-mod-60 lists. New links go to the front; links added during a list walk do
not run in that walk. The exclude mask and not-run bit apply. Iterators survive
mutations of the current object. `tick()` supplies visibility, executes that pass,
advances the frame and rebuilds alive players. It is a bounded composition, not
all original main-loop tasks. Pool-pressure reaping refuses.

Skelight executes pending damage, base creature tick, activity, animation,
knock-back, target pick, then its supported state switch. Inactive animation
never advances or delivers a hit event. Hits call victim slot 2 synchronously.
Creature HP changes then; player damage accumulates. Reaction belongs to the
victim's next own run. A Skelight's death queues deletion, leaving the grid and
handles live until the next delete-list flush. The bounded resource-free teardown
clears +364, +360, +2e4, +238, +234, then +8, +4, +0. Full attachment/spell/player
destructor chains refuse. A dead player remains allocated and alive-player lists
update immediately. Party-wipe continuation refuses after that state is retained.

Damage reuses the existing exact `combat::damage_apply` body. Its C++ records are
**call-local marshalling values**, not persistent actor authorities: modified
lanes are published before each virtual callback, then reloaded immediately from
the image. Only changed lanes are published, so slot-2 updates are preserved.
The shared RNG is borrowed directly, without seed import/export. The usual
successful ranged monster hit consumes seven shared draws; a player hit consumes
eight. Fixed ranges and misses have their original different counts. Creature
reactions and knock-back dust use object +44; player pending-damage rolls use the
shared stream. No randomness is supplied by Godot.

Targeting uses the existing exact base pick-target body, table-based team tests,
visibility, target/all-enemies arc helpers and cone search. Box traversal remains
row, column, newest grid link first. Arc hits deliver each hit immediately in that
order. Cone search uses the soft-double 1.4 range box and has no hard far-distance
cap. Full actor words and player record fields are compared in the exact corpus.

## Session wiring for the owning job

1. Select this policy explicitly for RTA. Construct one `World(services,
   camera_mode)` and one `SessionAdapter(world, first_ready_frame)`. Transfer
   ownership of actors to this World during setup. Do not enable legacy
   single-actor creature movement, base combat, or the old object scheduler for
   these same actors. No live migration or checkpoint conversion is provided.
2. Supply terrain raycast, slide, height and water-query services backed by the
   same selected terrain Level. Water must report dry; an actual water result
   refuses. The `flat_fixture_services()` helper is explicitly synthetic. Production
   callers must install real services, including dialog-open and jumpgate travel
   state queries for the player damage gates. Animation and skill services have no
   implicit production fallback.
3. Supply animation start and advance callbacks. Their first argument is the
   actor address, not actor +114. The animation state uses that actor's shared
   memory. Start callbacks update count/playing flags; advance callbacks update
   the same memory and return the event mask. No duplicate clock should advance
   on no-ready, frozen or presentation-only frames.
4. Invoke `adapter.ready_tick(frame)` once at the ready priority-10 simulation
   boundary. It rejects skipped or repeated frame numbers. Do not invoke both it
   and the old `runObjects` task. After an execution exception the adapter latches
   failure and retains the completed prefix; retrying the same frame refuses.
5. The creature-anim job's **mask 0x1** can be returned directly by the animation
   advance callback. Alternatively `hit_test_event(actor_handle, mask)` queues a
   handle-bound batch for the actor's next animation phase. Choose one route for
   each event. Destroyed actors cannot redirect events to reused addresses;
   inactive creature batches are discarded. Skelight tests the event in state 6.
6. Player integration exposes a **combat-only phase**, in the documented order:
   base creature tick, knock-back, record HP mirror, immunity/protection timers,
   pending damage, then an optional unarmed hit event in action state 1. The
   original action switch requires the explicitly refused `Player_move`; it is
   not run or silently replaced. Later original-player integration must call the
   exposed services at its documented sites and own attack selection/timing.
   No new controls or combo policy is invented here.
7. Keep Session saves/replays and observations versioned when adding this policy.
   This adapter does not serialize itself or claim old save compatibility.
   Presentation reads owned observations only. Keep World alive longer than
   SessionAdapter and any callbacks that borrow it.

Minimal setup uses `Spawn` values as **authored fixtures**, not original actor
constructors. All floating inputs/outputs are raw32 patterns using the portable
core FPU. Objects are not host pointers. `slot_function()` exposes the documented
Creature, Player and Skelight table words; unsupported virtual bodies refuse.

## Refusals and proof boundary

Partial paths remain explicit refusals: `moveWithCollision` touch order,
projectiles, restricted player movement, full destructor chains, `findPlayerInCone`,
and remaining hit/cooldown helpers. Walking/chase/flee that requires navigation,
combos, weapons, heavy/scaled attacks, blocking, knock-down reactions, other class
runs/death/loot, spells, PvP and network play remain unsupported. Creature movement
contacts and the second knock-back pass use the executed `Creature_moveBy` body.
Player rush touch refuses; its documented empty non-rush touch is supported.
Corpse blood decals remain refused at this pin, irrespective of later research.
Unknown engine call-outs never silently return successful effects. Explicitly
labelled empty-resource/presentation fixtures match the reference composition.

Direct helper calls preserve partial writes on exceptions. The ready-frame
adapter adds failure latching. Memory, call histories, handle capacity, retained
iterators and external handles are bounded; budget failures are errors, not
successful simulation. `clear_history()` clears observations, not gameplay state.

The public corpus is generated independently from the pinned Python reference,
then consumed without the research repository. It covers shared-image/handle
reuse, 11 executed helper families, exact seeds, both run-order hit timings,
inactive swings, multi-actor grid order and 80-tick Skelight composition. The
research's full player/Skelight duel self-hash is **not claimed**: that driver
executes observed-only player movement, which this job explicitly refuses.
Native API checks cover the opt-in adapter and additional refusal boundaries.

Reproduce:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_runtime_vectors.py \
  --research /path/to/research-combat \
  --out native/tests/vectors/combat_runtime.txt.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^combat-runtime-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

Private files were unavailable under the job rules; no private acceptance was
attempted. Godot integration, visual gameplay, target-platform runs and console
or emulator execution need separate evidence. See the local round report for the
actual build/test results and commit.
