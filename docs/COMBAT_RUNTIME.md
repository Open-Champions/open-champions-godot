# Opt-in RTA multi-actor combat runtime

The isolated port described below is now consumed by the explicit synthetic
scene policy documented in [PLAYABLE_COMBAT.md](PLAYABLE_COMBAT.md). Its original
port proof and refusal boundary are preserved here; scene acceptance is separate.

Round 2 adds the `Composition::Full` world described under *Round 2* below, with movement, the real player tick, armed melee, defence, projectiles, three spells and the monster helpers. The round-1 composition stays the default.

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
knock-back, target pick, then its supported state switch. The other nine
own-machine classes (Critter, Soul, NPC, Natasla, Mermaid, GnomeNavigator,
Skeleton, Gnaw, AntQueen) run the decision switch from the own-state-machine
contract when spawned with `spawn_class`. Their vtables are allocated on first
spawn, so a world that never spawns them keeps the Skelight image. Sight cones,
`GameObject_rand`, rear-cone and ally tests, Critter species clips and
`deadUpdate` hold/size stay explicit refusals except Gnaw's recorded slot-25
force 0 and its two sight arcs. Round-1 `moveToTarget` is still the walking-chase
refusal. Skelight's existing run is unchanged. Inactive animation
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

This section describes `Composition::Round1`, the default. Since round 2 the
helper bodies the two compositions share are the executed round-2 bodies. On round
1's domain they behave identically, and round 1's pinned corpus still passes. The
round-1 composition keeps its set-up, its combat-only player phase and its
refusals of the blocked melee line of sight (type names are a Full-composition
fixture), corpse blood decals, PvP, walking and spells. The executed armed, scaled,
block-intake and knock-down branches now run in both compositions.
`Composition::Full` and its boundary are described under *Round 2* below.

In round 1, partial paths remained explicit refusals: `moveWithCollision` touch order,
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

## Round 2: the Full composition

`World(services, camera_mode, Composition::Full)` is the native port of the research's composed round-2 runtime (`ref_combat_full.FullRuntime`).
It holds the six round-2 models' executed bodies in one world, and its player slot 4 is the real player tick.
`Composition::Round1` stays the default constructor argument, and its set-up is byte-for-byte round 1's.

### Evidence

Research branch `t3/research-combat`, commits `d7251e7..8b68a93`, pinned at head **8b68a93658b840c34dffddfb662930c99ee931a2**.
The parent of `d7251e7` is round 1's pin `9289d56`. The shared modules (`ref_core`, `ref_objects`, `ref_creature`, `ref_combat`, `ref_arena_monsters`, `ref_player`, `ref_combat_runtime`, `ref_skills_a/b/c`, `ref_items`) are unchanged in the range.
All 39 pinned blobs and their SHA-256 are in `native/tests/vectors/combat_runtime_2.json` and `tools/generate_combat_runtime_2_vectors.py`. The round-2 files are:

| Research file | SHA-256 |
|---|---|
| `docs/combat-runtime.md` | `4eaa5b510c8e56ca91b2c0753de8a06397d4f909e1a91da28d90d1313111fd63` |
| `docs/spec/combat-movement.md` | `30464bb083c90d92dbb27651476ed9049746752b1644dfd7accfad7538ba1740` |
| `docs/spec/combat-player-melee.md` | `9d1f82d2b0e64f5731912d0327e7d628412f7363c852aa9a188f8814ad9b4e29` |
| `docs/spec/combat-player-defence.md` | `94f6b07b4e139ce3d2967e33c68b360569c2c84a3e56e1ae5db5b423d46cdf0b` |
| `docs/spec/combat-projectiles.md` | `176304258f18fd5daaf584471d3fa9d02dea0c4cd64ac3d6d7db46a1bd930d11` |
| `docs/spec/combat-spells.md` | `f311616b967398f58fdfd8e298733f1ac67588db232229e61280d4c67ec0c999` |
| `docs/spec/combat-monster-death.md` | `c86fbad22eb9f40d6dea32138f5fa796efdf8df29d3094afac59c07aaa9ec8e1` |
| `tools/ref_combat_move.py` | `28fa523d3c1b4768ac1b147a0fb095dd4cfde8192a9f7db59b0c3d9a39f7cba0` |
| `tools/ref_combat_melee.py` | `3995b40c39c7c09478e2263b2b43adcfaee6dd30f75198238e5eabc7787c1888` |
| `tools/ref_combat_ptick.py` | `b90cc565e1651c659248945d5e98695449e1f6de20138928b18ade69568fb79c` |
| `tools/ref_combat_ranged.py` | `7755ff203383b92d39a190475b5704fb3fc512abcc107924e09189913d42bda5` |
| `tools/ref_combat_spells.py` | `b2cb2574f83f9c056a5cae729f37d29bc817ea18cd6566ddc626e0932aaaca3a` |
| `tools/ref_combat_monsters.py` | `1c9628ba39a4f6eda0db02f914e03e473f931a1850f6367291efdcf47e208d29` |
| `tools/ref_combat_full.py` | `57f3a3f2e19a9997b8c024a1f25f03ecfc2b1dfdc53190040682096006c3bfc4` |
| `tools/test_ref_combat_move.py` | `8c5bfbd0f11e0b0f079a1da296790630dc6dbbdfeebf464a3404c99e7d08a148` |
| `tools/test_ref_combat_melee.py` | `a971d7347563cba2c9b530fc48bf6fad47351359e6f8c92e1831723aa5147980` |
| `tools/test_ref_combat_ptick.py` | `03103d8874fb646fd10ec79cba00d4967b5d83a867d54a370cc9c1c495c76a20` |
| `tools/test_ref_combat_ranged.py` | `b21ce57a225ec0e6038e00e0b966cba60b82e23e3c5656a6512ae2a202060482` |
| `tools/test_ref_combat_spells.py` | `291a37a449726424987cd78a85c0d9b595de2420aad707132ba3675f6e1e6a17` |
| `tools/test_ref_combat_monsters.py` | `ce0bb83df608d9db9f84a6d3f0259bc52f5f0bc4ad79ba320888b1ed6539c16b` |
| `tools/test_ref_combat_full.py` | `cac7ede6e89919c7d1eed9ba218bc4e899005ab05c6c30d0d259c54e9abbba64` |
| `tools/difftest_combat_move.py` | `bcbaa829a929e17af9fccfb849bb210df67e846bd5cf57be44aa79b0ca8967a1` |
| `tools/difftest_combat_melee.py` | `fa80442ea1d05efad2a624b908dd1304b604f9bf8b593a6fe9804aa868c5d5ed` |
| `tools/difftest_combat_ptick.py` | `36aa9c6a410e9f7281a008bf40881a5594eef388a1f798d33f093f3a42bcc2ee` |
| `tools/difftest_combat_ranged.py` | `add303919a467162c0b8fbdd894b2e7e653c21dba901847f805865bdfeb288ed` |
| `tools/difftest_combat_spells.py` | `9ac399fca630c7683d0240db14feb2fcc8539173f4d47a8242c6936cfb0b5788` |
| `tools/difftest_combat_monsters.py` | `d8094414af90cd88610d9738ff93fbc7da2a8c445e8f49c2e59bf97121588cf4` |

The research reports 157,137 original-instruction cases with 0 mismatches for round 2, plus 12,000 executed combo ticks.
Those are **research results**, not runs made here.
No retail bytes, decompiler output or extracted assets are included.

### What runs natively

| Area | Native file | Executed bodies |
|---|---|---|
| Movement | `combat_runtime_move.cpp` | `obj_moveWithCollision` 0x14d4b8: one box query; the other's slot 6, then the mover's own; the prediction-copy guard; the grid relink is the world's. `Player_move` 0x1b4020 in every mode, with the 40 % facing turn, the swing window, slow factor, water, landing, the screen clamp and its second half. `Player_animStep` 0x1b9520 root motion and `Player_clampToScreen` 0x1b3d90. |
| Player tick | `combat_runtime_tick.cpp` | `Player_run` 0x1a5030 is the existing exact `player::Tick` port on the shared memory. Every call-out resolves as in FullRuntime, so the idle-fidget and idle-voice shared draws are made. Queued casts are made before the tick tail. |
| Offence | `combat_runtime_melee.cpp` | `Player_pickAttackAnim` 0x1bba88 (one unused shared draw); every branch of `Player_meleeHit` 0x1b6a98 (dual wield, bow/thrown as a fist, heavy range raised in place, scale, AutoProp line of sight, trail material); `Player_hitSparks` up to its sample-point refusal. |
| Defence | `combat_runtime_defence.cpp` | `Player_takeDamage` with the block branch; `Player_blockSparks` with silent 0 (at least 150 own draws); `Player_processDamage` with knock-downs by class and bearing, PvP A halving, blood, the death path and the held spell; `Player_hasAnim`; `Player_interact` with nothing in reach. |
| Projectiles | `combat_runtime_ranged.cpp` | `obj_moveAndCollide`, `Projectile_ctor`, `MissileWeapon` ctor/run (kinds 0, 3)/onTouch (one victim per frame, pierced list, dodge roll)/slot 17/dtor, and `playerProjectile` ctor/run (kinds 0, 4..)/dtor. A projectile destroys itself in its own run. |
| Monsters | `combat_runtime_monsters.cpp` | `canSeePoint`, `canSeeObjEdge`, `findPlayerInCone` (hard `far` cap; strict arc), `hitTargetAtReach`, `hitTargetInArcEdge`, `hitTargetOrPlayerInArc`, `knockbackTarget`, both cooldown setters, `world_notifyObjectsNear`, `Creature_deadUpdate` with its blood decal (two own draws), `Creature_dropLoot`, and the Goblin death branch. |
| Spells | `combat_runtime_spells.cpp` | `world_collectObjectsInArc` (partnerless, nearest first, the rest in reverse acceptance), `world_collectInSphere`, `world_damageInRadius`, and the five effect destructors. Holy Strike (ref_skills_a), Shock of Fire (ref_skills_b, including its stack frame of scratch vectors) and Wave of Flame (ref_skills_c), with the LightEffect simulation part. |

The arena port's Skelight run is unchanged, except that the Full composition also runs its chase (`Creature_moveToTarget`), its away-from-home wander entry and `Creature_moveToDest`, as the composed reference does.

### Corrections of 15.6, applied

- The player's movement is the executed `Player_move`. Every call makes `dialog_isOpen`, the target-angle `net_getCamYaw` and `net_getCamFlag`. It stores the static velocity 0x8657a0 and the -0.0 track weights (soft-double sign of zero), always stores the +0x482 countdown, and passes a height output to `water_test`.
- The mover's grid relink is the world's `obj_updateGridCell`, as the composition uses it. The executed inline copy stores the same words.
- The stagger roll is skipped in states 3 and 7, with 0x400000 and on a heavy attack. A front block zeroes or halves the hit and walks the player's own seed.
- `Creature_setAttackCooldown` does nothing without a target and skips slot 27 in state 0x14. `Creature_setAttackCooldownOn` does nothing for a boss or a null object. `hitTargetAtReach` is 3-D with no arc, ray or enemy test.
- `findPlayerInCone` caps the distance at `far` and starts its ray at full height.
- Arc lists keep only their first entry nearest. Radius damage raises to the minimum, then caps at the maximum.
- Projectiles and effects destroy themselves in their own run. Creatures die through `obj_delete` and the next flush.
- The Resurrect save (`g_numPlayers == 1`) and the held spell follow the executed `_death`.

### Services the Full composition needs

Beyond round 1's, the Full composition needs: `camera_yaw` and `camera_flag` (per slot); `screen_planes` (the camera frustum, needed when two or more players are alive offline); `animation_root` (the root motion of the last advance); `attach_point`; `sample_point_count`; `resource` (res_find, with text lumps marked); and `is_class` / `loot_generate` / `spawn_named_item` for `Creature_dropLoot`.
A missing service refuses rather than answering zero.
`flat_fixture_services()` fills the synthetic ones (yaw 0, follow camera, no root motion, attach point at the object's position, no sample points, resource 0). Production must supply real answers.
Animation start/advance keep round 1's signatures. The research's `PlayerAnim` (four base cells and a fifth swing track) is a test fixture, not part of the World.

The Full composition's set-up writes the research's authored fixtures:
- the missile, thrown, HolyStrike, Sparks, LightEffect and Wave-of-Flame table words;
- the type-info descriptors' name words in mangled form (`rtti_typeInfoEquals` compares names);
- the stack-list head 0x8000;
- the empty timed-modifier list 0x537c08.

Players get the four base animation cells, the idle-voice limit and the record speed multiplier. Class tables are written at spawn.
`give_weapon`, `spawn_missile`, `spawn_thrown`, `give_skill`, `queue_cast`, `stick`, `press_attack`, `bind_button` and `pad` are the research's authored call sites and inputs. The constructors they call are the executed bodies.

### Session adapter

`SessionAdapter` keeps its round-1 API and adds:
- `pad_input(slot, {stick, held, pressed})`;
- `bind_button` (button masks are the caller's controller mapping; none is invented);
- `attack` and `block` (the bound masks);
- `spawn_missile` and `spawn_thrown` (from an owner handle; they return a retained projectile handle that reads 0 once the projectile destroys itself);
- `cast` (Holy Strike 16, Shock of Fire 57 or Wave of Flame 95, given with `World::give_skill`).

The tick clears the pads' pressed and +0x10 edge words afterwards, as the reference's tick does.
In the Full composition the player's melee hit is the tick's frame count, not an animation event. `hit_test_event` masks still reach `anim_advance` for the actor.

### Refusals of the Full composition

The Full composition refuses wherever the composed reference refuses or has no defined result:
- **Movement and water:** entering water; the screen clamp without camera planes; SpellCharge contact.
- **Projectiles and offence:** missile kinds 1, 2, 5 and 6 at run; thrown potions; deflected missiles; Penetrating Arrows and Dodge at a nonzero rank; a projectile as its own attacker; `Player_hitSparks` or a stuck arrow needing model sample points.
- **Defence:** silent block sparks; a resisted knock-down of the real player (`SkillStrongStance_trigger`); GutChunk blood objects; Holy Shield absorption; the Resurrect save; interactable objects; PvP B arena lives and the PvP A kill credit.
- **Spells:** unknown spell classes; a creature destroyed with attached spells; skills other than 16, 57 and 95.
- **Monsters:** Skelight flee; Goblin states other than death; `Creature_dropLoot` without loot and type services.
- **Domain:** `Player_dtor`; network, cutscene, PvP, damage numbers and decal options at the tick.

The 201 refused fixture cases pin these refusals.

### Effects that are not cosmetic

The effects research (`t3/research-fx` at `19f80a6`, `docs/effects.md` 15.2, 15.5 and 17) shows that skill effect classes draw from the object (+0x44), spell (+0x10) and shared generators. Every `LightEffect` constructor draws the shared seed. `fx_spawn` id 5 and the ice shatter draw the shared seed too, and `DiseaseCloud` and `StaticFire` deal damage.

The three composed spells were checked against that research's recipe rows (`ref_fx_skills.RECIPES`) and need no draw this port does not make:

| Spell | Recipe-row draws | What the native port makes |
|---|---|---|
| Holy Strike | ctor SHARED; run SHARED (LightEffect) and own | `Projectile_ctor`'s shared draw, the LightEffect object with its shared draw, and the own-generator puffs and hit rolls. `HolyStrike_draw` is a glow with no draw. |
| Wave of Flame | ctor SHARED and SHARED (LightEffect); run spell | The Spell base's shared draw, the LightEffect's shared draw, and the spell-generator particle draws of every ring. |
| Shock of Fire | Sparks lies outside the effects research's skill-file range (0x28f880..) | The executed `ref_skills_b` model: own-generator particle balls, its LightEffect on contact, and the radius damage. |

`LightEffect_run` draws nothing (`ref_lighting.LightEffect`); the native run keeps its end test. Every one of these generator words is compared byte for byte by the fixtures.

The simulation side of the other effects belongs to the FX API job (native-fx-2). Here they are explicit refusals, never presentation:
- `fx_spawn` id 5 (reached only by missile kinds and LooseFire, which are refused already);
- `fx_shatterToLooseIce`;
- the run bodies of `DiseaseCloud` (0x2ce6e0) and `StaticFire` (0x2c595c).

Every other `fx_spawn` id the composed bodies reach (0, 2, 3, 7, 8, 9, 10, 11, 0x10, 0x11, 0x12) is presentation in both the reference composition and this port.

### Findings

- **Shock of Fire presentation names.** The composed reference builds its presentation table before `ref_combat_spells` adds `voice_setPosition` and `fx_ringBurst`, so it refuses Shock of Fire's first run, while the spells runtime treats both as presentation. The native Full composition treats them as presentation. The generator applies the same one-line adjustment, recorded in the manifest.
- **Unbounded re-dispatch.** A Shock of Fire burst that damages a non-creature object (for example a LightEffect) reaches the GameObject default take-damage slot, which the reference re-dispatches without end (`RecursionError`). That has no defined result, so native refuses it with an explicit re-dispatch guard.
- **Status-source re-link.** The reference re-links the status-source handle on every status assignment, even for the same source, which reorders the source's back list. The native damage glue does the same.

### Proof and reproduction

`native/tests/vectors/combat_runtime_2.txt.gz` holds 2,858 synthetic cases and 56,035 operations.
The generator's own count is 4,954,834 checks; the native replay counts 5,085,275 exact comparisons.
The cases cover:
- the six composed scenarios of `test_ref_combat_full.py`;
- seeded direct-call families for every body above, in the difftests' domains, on the composed world;
- world-level projectiles, spells, real-tick combos, blocking ticks and walking home.

After every operation the native replay compares the trace events (run, damage, deletion, destruction, casts, player ticks with their shared-draw counts, pool and light allocations, experience, loot calls), every changed byte, a digest of all non-zero memory, and the handle words. The handle node pool is compared at the end of each case.
`combat-runtime-2-host` adds native checks of the touch order, the idle fidget, the extended adapter, the composition guard and the refusals.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_runtime_2_vectors.py \
  --research /path/to/research-combat \
  --out native/tests/vectors/combat_runtime_2.txt.gz
ctest --test-dir build/native -R '^combat-runtime' --output-on-failure
```

Private files were unavailable under the job rules; no private acceptance was
attempted. Godot integration, visual gameplay, target-platform runs and console
or emulator execution need separate evidence. See the local round report for the
actual build/test results and commit.
