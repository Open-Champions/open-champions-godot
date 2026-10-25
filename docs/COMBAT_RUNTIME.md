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
knock-back, target pick, then its supported state switch. Of the other nine
own-machine classes only Gnaw can be built: `spawn_class("Gnaw")` is the
authored creature placement (the one Skelight uses, standing in for
`Creature_ctor`; `Creature_init` stays a recorded call-out) plus
`Gnaw_ctor`'s class words (research-anim `5694ae3` `ref_monsters_1.gnaw_ctor`,
`docs/spec/monsters-1.md` 4.2: table `0x515038` with run, draw and setState
over the base slots, kind 1, turn rate 1.2, radius 6, speed 0, the timers and
creature flags, the one-own-draw scale). `Gnaw_run` runs the executed prologue
(pending damage and `Creature_run` with the slot-16 model, `updateActive`
whose 0 ends the tick, `stepAnim(1.0)`, knock-back 1.0, slot 23) and feeds the
step's event word to the own-machine switch, so state 6 bites on bit 0.
`Gnaw_setState` is `ref_monsters_1.gnaw_set_state`: `Creature_filterState`
first, then the clip as a class-definition word (`+0x1c + 8k`) and its side
effects (path clear, wander pick, the cooldown 60, the bite draws). Its death
refuses at `PhProp_initPhysics` for the first GutChunk, and a hit with flags
`& 0xd` refuses at `GutChunk_ctor` (a Gnaw bleeds). Flee, and walking home,
wander or chase outside the Full composition, refuse as Skelight's do. The
Critter, NPC, Natasla, Mermaid and GnomeNavigator constructors, run prologues
and setState side effects were not executed (`own-state-machines.md` section 1
runs only the switch, with the prologue stubbed), so `spawn_class` refuses them
("constructor is not executed"); the round-3 dispatch rows are not used as a
stand-in setState. AntQueen, Skeleton and Soul are executed since combat-ctors
(section "AntQueen, Skeleton and Soul" below). Round-1 `moveToTarget` is still the walking-chase
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
   same selected terrain Level. `water_test` and `water_disturb` run the executed
   wading step (docs/PLAYER_WADING.md); a wader without a `water_disturb`
   service refuses. The `flat_fixture_services()` helper is explicitly synthetic. Production
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
- **Movement and water:** the screen clamp without camera planes; SpellCharge contact.
- **Projectiles and offence:** deflected missiles; Penetrating Arrows and Dodge at a nonzero rank; a projectile as its own attacker; `Player_hitSparks` or a stuck arrow needing model sample points. Thrown potion kinds 1, 2 and 3 are the round-4 bodies. Arrow kinds 1, 2, 5 and 6 are the round-3 bodies. `retain_round2_refusals()` puts the pinned round-2 corpus back on the refusals it recorded for potions, those arrow kinds, and a disease allocation.
- **Defence:** the Resurrect save; interactable objects; PvP B arena lives and the PvP A kill credit. Silent block sparks are the round-4 body. Holy Shield absorption is the round-4 stub when the caster's pose word at `+0x114` is 0, and the round-5 sample points when that word is set. The same corpus flag still refuses both. A resisted knock-down of the real player still refuses `SkillStrongStance_trigger`. That trigger's light and particles are not this round. The phantom constructor refuses at `Creature_ctor` (below). `GutChunk_ctor` at the blood call stays a refusal; the constructor itself refuses at `PhProp_ctor` and its run was not executed.
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
- `fx_shatterToLooseIce`;
- the run body of `StaticFire` (0x2c595c).

`DiseaseCloud_run` (0x2ce6e0) is the round-3 body below. A kind-1 ground hit refuses at `fx_spawn` id 5: that id builds 3 or 4 `Fire` objects on the shared seed (research-fx `docs/effects.md` row 5 and section 17) and no Fire body is executed. The playable Effects observer refuses the same id. `SpellDiseaseEffect_run` stays a refused skill path.

Every other `fx_spawn` id the composed bodies reach (0, 2, 3, 7, 8, 9, 10, 11, 0x10, 0x11, 0x12) is presentation in both the reference composition and this port.

### Round 3

Research pin `22649cd` (`tools/ref_combat_round3.py`, `docs/spec/combat-round3.md`). The bodies are in `combat_runtime_round3.cpp`. Round 1 and round 2 APIs and their pinned vectors are unchanged.

- **DiseaseCloud.** The radius grows by one half while it is strictly below the integer cap. Three streams emit `cap/10` particles from the object generator. On a frame divisible by 30, `damage_apply` runs with no attacker, attack 100000 and flags `0x1000040`, descriptor type `0x26`. An empty owner handle deals nothing. The owner is skipped, and so is a victim when both it and the owner have flag `0x40`. The cloud is not skipped: `GameObject_ctor` links it into grid A (radius 24), so its own box returns it, and the research run hands every returned object to `damage_apply`. For a non-creature that is one shared draw and slot 2 of table `0x5186f0`, which is not documented, so the World throws `Unsupported` when the walk reaches the cloud (after the objects before it). An ownerless cloud deals nothing and does not refuse. The damage path builds one `SpellDiseaseEffect` (`0x1b0`) from the shared draw `damage_apply` already took, and does not draw again. A second hit sees the attached effect and does not allocate another. At the end frame slot 1 with 3 reaches `DiseaseCloud_dtor` `0x3cd480`; the research has only that symbol (`symbols/runtime.tsv`), so ending the cloud throws `Unsupported`.
- **GutChunk.** Constructor `0x201958`, reached through `spawn_gut_chunk`, refuses at its first call, `PhProp_ctor` `0x155a0c`. The research executes the constructor with that call as a callback; the prop chain (`0x153e54`, `GameObject_ctor` with its shared draw and grid link) is not read, so the World does not answer it. The executed tail after it (table `0x50f3b8`, the gore bit, `+0x250`, `+0x25c`, `+0x260`) is therefore not reached. The run was not executed. The player-tick blood call still refuses `GutChunk_ctor`, which is how the round-2 composition records it.
- **Strong Stance phantom.** Constructor `0x1618d4`, reached through `spawn_phantom`, refuses at its first call, `Creature_ctor` `0x109d68` with the target's class definition. The research executes that body (`ref_arena_monsters.creature_ctor`), but it starts with `GameObject_ctor` on the definition, which the combat World does not compose, so no phantom is built and the run `0x161a54` (step, knockback, move, timer) is not reachable from the API. The player tick still refuses `SkillStrongStance_trigger`; that call also builds a light and 25 particles, which are not this round. The Holy Shield light is round 4.
- **Arrows.** Kinds 1, 2, 5 and 6 share the flight. A hit stores 0 at `+0xec` and destroys the arrow on that frame after `light_relink`. Kinds 0 and 3 keep the stone hit and do not relink. Kind 2 bursts a ring and damages in a radius of 24 to 72. Kind 5 emits 32 particles and effect 10. Kind 1 emits 24 particle pairs every frame. Its hit refuses: at `fx_spawn` id 5 on a ground hit (result 1), otherwise at the first `ParticleProp_ctor` `0x1549e8` block (a 0x148-byte Prop whose constructor draws the shared seed; the body is not read). Kind 6 emits 100 particles and, when the owner is a player, asks `player_getPassiveLevel`.

`native/tests/vectors/combat_runtime_3.txt.gz` holds eight synthetic cases. The native replay compares the particle words, the grown radius, the expiry word, the kind-2 ring and the light colours. `cloud-tick` is a damage frame whose research box holds a creature and the cloud: the native World matches the creature's `damage_apply` and refuses at the cloud. `arrow1` (a ground hit) is replayed as the `fx_spawn` 5 refusal; `arrow1-flight` compares kind 1's particle pairs. `disease-attach` runs the research `disease_attach` and compares the shared seed after its draw, `+0x10`, the table, `+0x1a0`, `+0x1a4`, `+0x1a8`, `+0x1ac` and all 96 swarm words. GutChunk and the phantom have no vectors: both constructors now refuse at their first call-out. Those words come from the research model at `22649cd` with velocity 0, so the real move leaves the arrow on the planted point.

The round-2 corpus still records a refusal where a hit now allocates a DiseaseEffect, and where an arrow of kind 1, 2, 5 or 6 now runs. `retain_round2_refusals()` restores that surface for the corpus replay. The Full composition the client uses leaves the round-3 bodies on. An empty take-damage slot (`0x3c7e88`) returns on that composition; the corpus replay still records the old re-dispatch refusal.

Deflection, the bow release in state 0, and the remaining skill paths stay refused. Potion kinds 1–3, the Holy Shield light, and block sparks with a nonzero silent argument are round 4.

### Round 4

Research pin `95c17ae` (`tools/ref_combat_round4.py`, `docs/spec/combat-round4.md`). The bodies are in `combat_runtime_round4.cpp`. The live Full composition runs them. `retain_round2_refusals()` clears that flag together with the round-3 flag, so the pinned round-2 corpus still refuses a thrown potion and a silent spark.

- **Thrown potions.** `playerProjectile_run` `0x1ba940` for kinds 1, 2 and 3. Kinds 0 and 4 and above stay in `thrown_run`. A potion keeps the mover's result whole: bit 1 is not cleared and `+0xcc` is not folded in. Kind 1 emits three flight particles from the shared seed before the spin. When the run ends it refuses at the first `pool_new(0x54)` block: `LooseFire_ctor` `0x202b70` is a stubbed call-out of the research (a GameObject that runs `fx_spawn` 5), so the five fires and their decals are not produced. Kind 2 refuses when `pool_new(0x70)` returns a block: `poisonGasSpell_ctor` `0x202de8` (300 frames, radius 20, damage 5) is read but not executed. A zero pool return still skips either constructor (and, for kind 2, the gas sound). Kind 3 raises a ring by `0x416b3333`, calls `world_damageInRadius` unless the level name at `0x44fe18` matches `0x535ed8` (`innpit`, ASCII case ignored), lays a decal when the ground is strictly within 4 of z in the soft double arithmetic, and builds a light when `pool_new(0xbc)` returns a block. A zero pool return skips that constructor and, for kind 2, the sound. The native pool uses the next larger class when the smallest fitting class is empty, so filling only the small class does not produce that zero inside the 1MB sparse budget.
- **Holy Shield.** `SpellHolyShield_absorb` `0x2e8534`, reached from `take_damage` when `find_spell_by_type` sees getter `0x3cf124` and the record's mana at `+0xc0` is above 0. The fraction is slot 10 of the skill times `0x3c23d70a`. The paid part leaves the damage and the mana, and each result is floored at 0. 241 particles use the spell's own generator, with each coordinate `add(unit, unit) - 1`. A cooldown at `+0x1c` that is at or below 0 stores `0x3e86a832` and, with a pool block, a light 60 above the caster. `attach_holy_shield` plants the words that function reads in a `0x28`-byte block on table `0x519ce0` (skills-35-68 section 54), writing that table's slots 0–2 (getter `0x3cf124`, destructor `0x3cf0c4`, run `0x2e8428`); slots 3 and 4 are the undocumented base entries and stay 0. The spell generator word `+0x10` is an authored fixture value of 1, because the constructor's shared draw is not run. `SpellHolyShield_ctor` stays refused, as does the spell's run.
- **Silent block sparks.** `Player_blockSparks` `0x1b9658` when silent is not 0. The two hand points are read and discarded. A sample count at or below 0 returns. Each point emits five shared-seed sparks: silent 1 uses definition `0x865810` and silent 2 uses `0x865860`. Any other nonzero silent draws the balls and spawns nothing. Silent 0 stays the round-2 body and its 50 own-seed sparks. Both executed callers pass 0. Who passes a nonzero silent is not established. `Services::sample_point` is `model_getSamplePoint`; a count of 0 never calls it.

`native/tests/vectors/combat_runtime_4.txt.gz` holds nine synthetic cases. The native replay compares the particle words, the blast ring and light, the shield cooldown and result, and the shared seed after silent sparks. `potion-fire` and `potion-gas` still record the research call-out arguments; the replay checks that the World refuses at that call and compares the particles before it. Position-sensitive cases plant a z velocity equal to the gravity term, so the real move leaves the projectile where it was planted. The blast case plants `innpit`, so the decal angle is the first shared draw of that run.

These stay refused, because round 4 does not execute them: deflection (the chance stays 0; a nonzero Dodge rank still throws before that roll); the bow release in state 0 (the player tick is unchanged); monster death states beyond the bodies already composed; skill paths other than 16, 57 and 95; `SpellHolyShield_ctor` and `SpellHolyShield_run`; `SpellDiseaseEffect_run`; `SkillStrongStance_trigger` and its light and 25 particles; `GutChunk`'s run; Fire construction from `fx_spawn` id 5; `LooseFire_ctor` and `poisonGasSpell_ctor`. (Since combat-ctors the two constructors, and `fx_spawn` id 5 inside `LooseFire_ctor`, run; see below.)

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_runtime_4_vectors.py
ctest --test-dir build/native -R 'combat-runtime-4' --output-on-failure
```

### Round 5

Research pin `2980af1` (`tools/ref_combat_round5.py`, `docs/spec/combat-round5.md`, commits `4cf2902` and `2980af1`). The new bodies are in `combat_runtime_round5.cpp`. Round 1 through round 4 APIs, pins and the round-4 `holy_shield_absorb` stub are unchanged. `take_damage` calls the sample body only when the pose word at the caster's `+0x114` is nonzero.

The four `Creature_processDamage` pairs and the silent-0 `Player_blockSparks` angle use `champions::rta::fx::trig::cos16` and `sin16`. The live Full fight goes through `arena_monsters`. The monsters-1 module already passed the same registers to its own `cos16`; that corpus is unchanged.

- **Hit-effect angle** (`0x110e60` / `0x110e80`). The sign-extended hit halfword at `+0x2e0` plus `0x7fff`. It runs when the radius at `+0x1c` is at least 25 and the pending flags have a bit of `0xd`.
- **Metal sparks** (`0x11110c` / `0x11111c`). The sign-extended facing at `+0x38`, with no add. The velocity scale is 10.
- **Gib.** The third of the three own draws is the angle. Two chunks use bias `0xe0c0` and span 16000; four chunks use bias `0xb1e0` and span 40000. The angle is the sign-extended hit plus the sign-extended sum of that bias and `draw % span`. The sine call receives the same register.
- **Silent-0 block sparks** (`0x1b97dc` / `0x1b9800`). The same sign-extended facing. Silent other than 0 stays the round-4 body and does not call these cosines. Both executed callers pass 0.
- **AntQueen.** `fade_bank_tracks` (`0x2385dc`, the call the session named `aq_ground`) writes rate `0xbd088888` on a track whose clip equals any of the class-def words at `+0xb4`, `+0x44`, `+0xbc` and `+0x4c`. A signed count at `+0x118` of 0 or below, including `0xff`, writes nothing. `update_bank` (`0x238448`, `aq_air`) springs the weight from the signed halfword at `+0x110` and stores the angle halfword at `+0x3e4` before the track loop. States 0 and 1 are the executed part of `AntQueen_run`. State 0 fades, then calls slot 25 with 0; a nonzero return and a signed track count below 4 request state 3 instead of the second sight. State 1 banks, then calls slot 25 once with 0. `Creature_updateActive` returning 0 does not stop the queen. Slot 25 past state 1 stays refused, and state 3's sight cone is still the Gnaw-only contract.
- **Skeleton.** States 0 and 1 of `Skeleton_run`. The first slot-25 call passes 0. When that returns 0 and the kind word at `+0x3d0` is 4, the second call passes 1. A nonzero first return requests state 3 and skips the second. `updateActive` returning 0 leaves before the switch. Slot 25 past state 1, and the arrow release, stay refused. `Creature_updateActive` is called only by these three runs; other own-machine classes are unchanged.
- **Soul.** The phase gate calls `is_demon_near` when `(g_simFrame XOR the byte at +0x230) & 15` is 0. The word at `+0x44` is the object generator. The box is the position ± 300 on each axis, capacity 128. The first entry whose type equals Demon's and whose object flag 8 is clear returns 1. Demon's descriptor address is not in the research, so the native compare is on the slot-0 getter: each typeinfo getter returns its own class's descriptor, so an entry is a Demon exactly when its getter is `Demon_typeinfo` `0x3ca81c`. `updateActive` returning 0 leaves before the phase test. Thrown-state hits stay refused.
- **Holy Shield sample points.** `holy_shield_sample` follows `model_getSamplePoint` at scale 1: the caster's position and facing, the pose already stored at `+0x114`, and `Spell_rand` modulo the sample count. A count of 0 still refuses. A missing pose, or a pose with no joints, refuses; building a pose is not this body. The player's slot 16 (`0x3c3b38`) is the model at `+0x544`. The light, the group and the slot-10 result stay the round-4 absorb. `SpellHolyShield_ctor` stays refused. Creature slot 17 calls spell slot 2 every tick, so the live shield uses the skills-b `SpellHolyShield_run` body `0x2e8428`: the clock at `+0x18` and the flash at `+0x1c` step by `0x3c888888`, and a duration at `+0x20` below the clock ends through slot 1 with argument 3. Otherwise the run notes `particleGroup_setMatrix` and tints the owner's eight halfwords. The fixture plants the clock at 0 and the duration from skill stat index 1 after `attach_holy_shield`, and relies on the round-4 plant, which points the spell at table `0x519ce0` with its first three slots written: the getter `0x3cf124`, the destructor `0x3cf0c4` and the run. skills-35-68 section 54 records `0x3cf0c4` as the base Spell destructor; its steps are the base tail of the executed `SpellWaveOfFlame_dtor` (table `0x515ba0`, clear the back list at `+4`, `pool_free` on bit 0). Slots 3 and 4 stay 0 and refuse.
- **MultiFireProjectile_run** (`0x2da2bc`). Once `+0x60` is set the run returns. Otherwise it records one trail point, adds 1 to the age at `+0x6c`, and moves with the velocity at `+0x54`. Bit 0 of that result ends with reason 1. A signed age of 601 or more ends with reason 0. Otherwise ten particles of definition `0x8e5910` are drawn from the object's own generator. `MultiFireProjectile_end`'s body and `onHit` are not re-executed in round 5; round 6 executes both, and the run now calls the end body.

`native/tests/vectors/combat_runtime_5.txt.gz` holds eighteen synthetic cases: the angle registers, five bank springs, three fades, the zero sample count, two shield samples (identity, and two joints with two points under a turned facing), the demon box with six entry lists, nine Soul phase rows, and four volley runs. The native replay compares the stored words and the particle rows with the research model at `2980af1`; the demon lists run through the live world (Gnaw objects carry the planted Demon getter), and the phase rows run through the own-machine decision module.

Audit 2026-10-21 (CAI-4): `spawn_class` refused AntQueen, Skeleton and Soul because their constructors were not executed. Since combat-ctors (below) they are built by their executed constructors again, and the bodies above run in their states 0 and 1. `fade_bank_tracks`, `update_bank` and `is_demon_near` stay callable on a creature object, and the round-5 replay uses an authored Skelight placement as that object; the nine Soul phase rows replay against the own-machine decision module. The Full fixture's slots 4-6 stay empty. Before that audit, the Full fixture added an AntQueen at (0, 100), a Skeleton at (60, 100) and a Soul at (−60, 100), each facing `0xc000` with 120 HP. It still arms Holy Shield on the Holy Strike skill already in the table. `give_skill(16)` is not called again. The shield's model is one sample point and its pose is one joint. Those actors and the absorb change the Full trace hash; the reason is recorded with the hash in the round report. A mid-fight save still replays the journal, so the continued run matches the uninterrupted run of the same binary. `fixture-save` traces each tick's demon tests and sample points: over its 300 ticks the shield absorbs 9 hits (2,169 sample points), identically after the load. With slots 4-6 empty there are no demon tests (the round-5 fixture had 19).

These stayed refused after round 5, which does not execute them (round 6 below executes the first six): `DiseaseCorpse_run`, `SpellPlanarSteelEffect_run`, `SpellHornOmenEffect_run`, `SpellArena_run`, `Pustule_run`, `SmokeGen_run`, deflection, the bow release composed into the player tick, monster death states beyond the bodies already composed, a nonzero silent passed to block sparks, haze kind 3 and the haze screen axis.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_runtime_5_vectors.py
ctest --test-dir build/native -R 'combat-runtime-5' --output-on-failure
```

### AntQueen, Skeleton and Soul; thrown-potion endings (combat-ctors)

Research `t3/research-combat-ctors` at **e9777eb25a150123e48452f25968e8ee263d0003** (commits `98c081a`, `22408a1`, `e9777eb`; each difftest 0 mismatches on seeds 20973 and 7). Those are **research results**, not runs made here.

| Research file | SHA-256 |
|---|---|
| `docs/spec/combat-ctors.md` | `4ad864cf79408a82f4b84794d9f097c7b61818c4f8f8dffa4755e4bf57fb4040` |
| `tools/ref_combat_ctors.py` | `118fb30578b872b871b49b62038941739fd32ff869dc9cf14ed381e8a5e793cc` |
| `docs/spec/potion-ctors.md` | `db27aa470106689a9f073f3beadc7c37a6bb8d6a49896c67a5efea25e52d9364` |
| `tools/ref_potion_ctors.py` | `4b7a2520c01f7acf831ec0041384d5af9aec4a4466c671018ad63de0f249af42` |

The other pinned blobs (tests, difftests, `ref_core`, `ref_creature`, `ref_arena_monsters`, `ref_monsters_1`, `ref_skills_a`) are in `native/tests/vectors/combat_ctors.json` and `potion_ctors.json`. The music volume of the same research is in [NATIVE_AUDIO.md](NATIVE_AUDIO.md).

**The three classes.** `combat_ctors.cpp` holds `AntQueen_ctor` `0x236c70`, `Skeleton_ctor` `0x2a5af4`, `Soul_ctor` `0x2a9478`, the set-states `0x237918`, `0x2a7768` and `0x2aac38` (every state number), the run functions up to their switch, and `Soul_getModel` (slot 16), on the arena model:
- AntQueen and Soul store state 0 directly with `+0xf4` = 0, so their state-0 entry draw is not made. The Skeleton constructor calls `Skeleton_setState(0)`.
- Skeleton and Soul return before the switch when `Creature_updateActive` says inactive; AntQueen does not test it.
- Soul's 0x21 burst is flat (the copy's z word is zeroed before the ×15 scale). On a level named `airhub` (case ignored) every Soul request becomes 0x22 before `Creature_filterState`.
- `combat-ctors-exact` replays 480 reference cases (4,404 operations, 20,976 call-outs answered and recorded): call names and arguments, every stored byte, and the shared draw count.

In the World (`World::spawn_class`, Full composition only) the constructors run on the authored placement that stands in for `Creature_ctor`, as for Skelight and Gnaw; the hit-point draw before it is not made. The placement's active bit is cleared, as `Creature_ctor` leaves it, so the AntQueen's and Skeleton's every-60 list behaves as the original's. The class table is the base plus every documented override (symbols `monsters.tsv`, `runtime.tsv`, `creatures.tsv`; `docs/monsters-2.md`; combat-ctors.md section 7 lists every slot that differs). The anim push calls, `Creature_init` and the class-definition event times are recorded call-outs (the World's animation service is the authored fixture, as for Gnaw). `model_buildHideMask` is a required service; the flat fixture answers all ones. `Creature_setCharm` is the Observed body of skills-35-68.md. The Skeleton's holder slots 1 and 21 are recorded (slot 21 only feeds `fx_modelStatusEffects`, FINDINGS D37). The type name `Skeleton`, the prefix texts, the class-table bit 0 on `A`..`Z`, the airhub text and authored skelcom clip tokens enter the image with the first creature that reads them. Slot 4 runs the prologue, then the round-5 switch for states 0 and 1.

Explicit refusals in round 1 (round 2, below, executes `AntQueen_run` past state 1, `AntQueen_pickFlyDest`, the take-damage slots, `Skeleton_isEnemy` and the destructors): a state past 1; `Soul_takeDamage`, `Skeleton_takeDamage`, `Skeleton_isEnemy`, Skeleton slot 12, `Skeleton_shatter` and `AntQueen_pickFlyDest` (recording stubs of the research); `anim_findTrack`; the holder's element slots 2–4 (`creature_buildElementDamage` draws the shared seed) and slot 11; and the inline destructors `AntQueen_dtor` `0x3c3c10`, `Skeleton_dtor` `0x3cc5b8` and `Soul_dtor` `0x3cc72c` (named, bodies not read). AntQueen's slot 2 is a plain jump to `Creature_takeDamage` (`docs/monsters-2.md`). The Round-1 composition refuses the three classes.

The Full fixture places the Soul in slot 6 (-60, 100). It is on team 0 and wanders for the whole fight: 19 demon tests in `fixture-save`'s 300 ticks, and the mid-fight save still continues byte for byte. Slots 4 and 5 stay empty, because on the round-5 placement the Skeleton's first target scan (ready tick 32) calls `Skeleton_isEnemy`, and the AntQueen sees the player and requests state 3 (tick 33); `combat-runtime-5-host` checks both stops. Putting them in the default fight needs `Skeleton_isEnemy` and the AntQueen switch past state 1 executed.

**Thrown potions 1 and 2** (`combat_runtime_potions.cpp`). `LooseFire_ctor` `0x202b70`: GameObject (one shared draw), table `0x50e2b8`, owner handle `+0x4c`, end frame `+0x50` = frame + 120, `fx_spawn` id 5, the every-frame list, flags `|= 0x420c`, radius 10. `fx_spawn` id 5: one shared draw for 3 + (d & 1) fires, three per point, then `pool_new(0x50)` and `Fire_ctor` (one more shared draw); the 24 particle definitions it builds on first use are presentation. `Fire_run` draws only the fire's own generator (glow and one particle) and calls slot 1 when its life of 100 runs out. `LooseFire_run` calls slot 1 once `+0x50` is below the frame (frame F + 121). `LooseFire_onTouch` acts on frames divisible by 20; a player's or creature's descriptor type 0x15 stays refused (with its value); any other toucher gets `damage_apply` with an empty descriptor. `poisonGasSpell_ctor` `0x202de8` is a plain GameObject with one shared draw. Still refused: `Fire_dtor` `0x3c5050`, `LooseFire_dtor` `0x3c6778` and `poisonGasSpell_dtor` `0x3c7878` (named in `runtime.tsv`, bodies not read; runtime.md 4.3 gives only their general shape) and `poisonGasSpell_run` `0x1fd0f0` (combat round 6). `potion-ctors-exact` replays 126 reference cases through the World.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_ctors_vectors.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_potion_ctors_vectors.py
ctest --test-dir build/native -R 'combat-ctors|potion-ctors|combat-runtime-[45]|fixture-save' --output-on-failure
```

### AntQueen_run, Skeleton_isEnemy, the take-damage slots, six destructors and the fire entry (combat-ctors round 2)

Research `t3/research-combat-ctors-2` at **5b928373a4bc31e5566312de598f861cc9f58071** (commits `6eeb4d8`, `36bb63b`, `14f282c`, `e274824`, `5b92837`; every difftest 0 mismatches on seeds 20973 and 7). Those are **research results**, not runs made here.

| Research file | SHA-256 |
|---|---|
| `docs/spec/combat-ctors-2.md` | `d3324aa50d9cd5c0c44121aa98141fb44763e9f25f57ef9a7421710eb7e5a296` |
| `docs/spec/antqueen-run.md` | `e2065673ef4d7e5cd96e0b6254e9d38e06db0e939680a0f431b9a334295a7791` |
| `docs/spec/fire-touch.md` | `11feb9072f6d3c17d5b4bc816cd94f2854b70922e9699530d0296dcc73802426` |
| `docs/spec/full-placement.md` | `3c2cac34c429d0424c002dc08e1e6a5e5754a2eec12cf2284f8f214559a2b2b8` |
| `tools/ref_antqueen_run.py` | `cbe6f5c983e9cf8b38c64406506522b0703c66ede7b26304fb6d5c0c632dd562` |
| `tools/ref_combat_ctors2.py` | `c0dc0ee6ea659ee7ecd2085c99c83b6ebf9bf62a285c9461f69ee3b8dda89fc5` |
| `tools/ref_fire_touch.py` | `82ac8c6ff727c90aef86a1f1c96f9fc6c86528d7bc66d17b71da1a15d04e73f4` |

The other pinned blobs are in `native/tests/vectors/combat_ctors_2.json` and `fire_touch.json`.

**The bodies** (`combat_ctors_2.cpp`, on the arena model):
- `AntQueen_run` `0x236f78`: the whole switch, all 26 states (antqueen-run.md section 3), with the bank helpers `0x2385dc` and `0x238448` and `AntQueen_pickFlyDest` `0x238150`, which `AntQueen_setState` state 0x17 now runs. A model that answers `AntQueen_pickFlyDest` as a call-out (the round-1 vectors pin `e9777eb`, where it was a recording stub) gets that answer instead. States 7, 9 and 10 pass the stack descriptor `(0x17, v, v, 0)`, v the low 16 bits of the `SkillPoisonWeapons` slot 11 at `g_skillTable[26]`.
- The eight type-info getters of combat-ctors-2.md section 3 (descriptor built on first use: `+0` the name, `+4` the `__si_type_info` or `__user_type_info` table, `+8` the base descriptor) and `rtti_typeInfoEquals` (the same descriptor, or equal name strings).
- `Skeleton_isEnemy` `0x2a7e04`: `Creature_isEnemy` + 1 when that is non-zero (2 or 5); otherwise 1 for a live, exact-class Skeleton on the opposite side (`+0x3d4` 1 against 2) while the skeleton's charm timer `+0x312` is 0; else 0.
- `Skeleton_takeDamage` `0x2a774c` and `AntQueen_takeDamage` `0x2378fc`: `Creature_takeDamage` with the four arguments.
- `Soul_takeDamage` `0x2aa908`: only a Demon attacker (exact class) does anything: `Creature_takeDamage`, then, with a non-zero sample count, exactly six flat particles (one own draw for the sample index, own-draw rejection for the unit ball, z zeroed, ×15, definition `0x8e4600`). A null attacker makes the `cxx_throwBadTypeid` call-out and refuses: what catches the throw is Unknown (section 8).
- The six inline destructors (section 6): `AntQueen_dtor` `0x3c3c10`, `Skeleton_dtor` `0x3cc5b8`, `Soul_dtor` `0x3cc72c` over `Creature_dtor` `0x117128`; `Fire_dtor` `0x3c5050`, `LooseFire_dtor` `0x3c6778`, `poisonGasSpell_dtor` `0x3c7878` over `GameObject_dtor` `0x14cf90` (with its `name=` scan). Each stores its table, clears its handles in order, calls the base with 0 and frees on bit 0.

`combat-ctors-2-exact` replays 480 reference cases (4,342 operations, 26,791 recorded call-outs) from the pinned models on authored images: call names and arguments, every stored byte, the shared draw count and the result. Every AntQueen state and out-of-range numbers are run; `isEnemy` returns 0, 1, 2 and 5; `Soul_takeDamage` refuses its null attacker on both sides. As a check, a state-6 reach of 40, a probe distance of 151 and `isEnemy` returning r instead of r + 1 gave 328 failures.

**In the World.** `CT::install_round2` replaces the round-1 refusals of `Skeleton_isEnemy`, `Skeleton_takeDamage` and `Soul_takeDamage`. Slot 4 of an AntQueen runs `CT::antqueen_run` whole. The Skeleton and the Soul keep their round-1 switch for states 0 and 1. The destructors run `CT::class_dtor` on the World's model: `handle_clear`, `iter_*`, `Spell.v1` and `pool_free` are the World's services, `sub_202500` and `AnimState_dtor` are recorded, and `objNames_remove` refuses (no World object has a property list, so the scan never reaches it). `Creature_hitTargetAtReach` and `Creature_hitTargetOrPlayerInArc` take the poison descriptor into `damage_apply`. `terrain_height` uses the Full World's `terrain_height` service when it has one (height_below before). `AntWave_ctor` `0x238654` refuses (state 0x19 has no requester; antqueen-run.md section 7). The three potion tables get the GameObject defaults of slots 7, 10, 11 and 12 (`0x3c7eb0`, `0x3c7ec8`, `0x3c7ef0`, `0x3c7ef8`), which all 156 classes outside the Creature branch hold (research `docs/runtime.md` 4.4, Observed): a creature hit near a fire or a gas cloud notifies it through slot 12.

**The fire entry** (fire-touch.md). `LooseFire_onTouch` now passes `(0x15, v, v, 0)` for a player or creature toucher to the real `damage_apply` (rating 10,000,000, flags `0x0100c040`, range 0..0). It always hits, stores the burn `+0x340 = 2 − 2 × fire resistance`, and the burn tick in `Creature_run` (already ported) hits on `frame & 31 == 0`. `fire-touch-exact` runs 306 rows through the native `combat-damage` harness: the touch (creature, player and plain touchers; the fire, a creature and a player as the attacker), the burn tick (monster and player; the player's range draw made first) and a re-touch of a burning victim, with resistances, armour, PvP mode A and Frost Armor. The rows are `ref_combat.damage_apply` with the arguments `ref_fire_touch.loosefire_touch` and `burn_tick` pass (captured from those functions), on the same pinned `ref_combat.py` as the `combat-damage` corpus.

**Frost Armor's endless self-hit.** A burning Frost Armor carrier is its own attacker in the burn tick; its strike-back re-enters `damage_apply` while the roll (0..99) is below the chance statistic, so with a chance of 100 or more the original recurses until its stack runs out (fire-touch.md section 3). `frost_armor_on_hit` now refuses that case after the original's roll and chance lookup, instead of reaching the 128-deep recursion limit. Below 100 the chain ends and runs.

**The Full fight.** The AntQueen is back in slot 4, placed before the Soul in slot 6, at (-110, 110). That placement is authored, and chosen: the round-5 placement (0, 140) is off the synthetic level's flat ground (±128; `99f22ef` moved it to (0, 100)), and at (0, 100) she stands in the walk-and-combo lane. The player's swings hit her there, and her blood stops the session at `GutChunk_ctor` (its `PhProp_ctor` is not executed), so `playable-combat-fx` and the Godot full-combat smoke could not finish their combos; their outcomes also move because her placement makes one more shared draw. At (-110, 110) she sees the player at ready tick 33 and runs states 3, 8, 9, 0x17 (`pickFlyDest`) and 0x18 as the research probe's queen does, and the authored combat scenarios end before she reaches the player. Slot 5 stays empty: a Skeleton gets past `Skeleton_isEnemy` now and stops at its first sighting, in `Skeleton_run` state 3 (and `creature_allyInLineOfFire` beside it), which full-placement.md section 4 ranks as not executed; a spawned Skeleton stops there at ready tick 66 in `fixture-save`. The idle Full fight now ends at ready tick 539 on the existing `party wipe: deathTask` refusal; her placement's shared draw shifts the Skelights' rolls. Only the two Skelights damage the player: the AntQueen's clips have no events in the fixture's animation, so her bites and stings never land. Potion 1 runs to ready tick 725 and potion 2 to 787, both to the same party wipe. The Godot `FULL_TRACE` changes from `41e06391…fee75` (26,856 bytes) to `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9` (31,622 bytes); with slot 4 left empty this branch still gives `41e06391…`, so the AntQueen is the only cause (her placement's shared draw, her trace lines and her states).

Still refused, as the research ranks them not executed: `Skeleton_run` past states 0 and 1, `creature_allyInLineOfFire`, `Skeleton_onNearbyEvent` (Skeleton slot 12), `Creature_setCharm` beyond its Observed prose, `GutChunk_ctor`, the death path (`Skeleton_shatter`, loot, experience), `AntWave_ctor`, and the Soul's model sample points. The Soul's states 0 and 1 keep the round-1 switch: full-placement.md section 4 ranks them executed only to the demon check, because its probe counts the combat-round tables; research-anim's own-state-machines.md section 11 executed their decisions with the prologue stubbed.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_ctors_2_vectors.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_fire_touch_vectors.py
ctest --test-dir build/native -R 'combat-ctors|fire-touch|combat-damage|potion-ctors|combat-runtime-[56]|fixture-save' --output-on-failure
```

### world_findFreeSpot (audit-fixes)

An idle Full fight used to stop with 310 ready ticks done, with `missing arena handler: world_findFreeSpot`. The Skelight at x = 80 never moves in that fixture (no root motion). Its `Creature_moveTo` stuck recovery (`0x114510`, research-combat `docs/creatures.md`) then searches for a free spot once the stand reaches 300 clock ticks, and no handler answered. The round-5 actors play no part in it.

The Full composition now answers with `World::find_free_spot`: `world_findFreeSpot` `0x14acf8` as executed in research-levels `13fafa4` (`ref_objects.World.find_free_spot`; `docs/level-flow.md` 17.2 and 18.3; `docs/objects.md`). It makes up to 50 tries. Each try draws a shared-seed point in the unit ball (three draws per sample) and offsets the centre by that point's x and y times the spread. A try fails when:
- `terrain_testFlag1` is nonzero;
- the floor from `terrain_height` is not below centre z + 6, unless anyHeight is set;
- the box query (result ± radius, floor ± 100000, limit `0x80`) holds an object without flag `0x1` where dx² + dy² ≤ radius² + r².

When all 50 tries fail, ok is 0 and the point is the last one that passed the terrain tests, or the centre. Two new services carry the terrain tests: `terrain_blocked` and `terrain_height`. The Full fixture supplies both from its own ground. A world without them refuses. Round 1 is unchanged.

`native/tests/vectors/combat_free_spot.txt.gz` holds 11 cases from the reference. They cover open ground, the stuck fight's geometry, a ring of objects, blocked terrain, a floor at exactly 6 (with and without anyHeight), a floor far below, a non-solid and a solid covering object, mixed terrain, and a wide search. Each case compares the point, ok and the shared seed after. `combat-free-spot-host` runs the idle Full fight for 400 ready ticks. The Skelight searches in the 311th from (80, 0) with spread 72 and radius 12, and moves to the spot it finds. Left idle, the fight reaches the second search at 621. At 786 the player has died, and the fight stops on the existing `party wipe: deathTask` refusal.

A walking player beside that Skelight reaches the same search (`combat-free-spot-host`, `walk_beside_skelight`: 1,500 ready ticks walking ±x past it, 8 searches with spread 72, radius 12 and anyHeight 0, no refusal). Research `t3/research-combat-walk` `difftest_combat_walk.py` (`free_spot`, seeds 20973 and 7) executed `0x14acf8` with these arguments on the original object world, including retried and failed searches. The netplay session supplies the two terrain services from each peer's level copy (docs/NETPLAY.md).

The same host test pins walking after an attack (`attack_then_walk`). A held press while walking starts state 1. The swing ends 40 ticks after it starts (`+0x448`), because the stick is past 0.8 (`0x3f4ccccc`) and 39.3 (`0x421d3332`) ticks have passed. The player does not move during the swing and walks on every tick after it. A press every 12 ticks chains the swings and keeps state 1 (research `walk.once`, `walk.mash.12`). The neutral state starts a swing from the held word (pad `+8`, `ref_player.Tick.state_neutral`), so a pressed edge without the held bit (`press_attack(false)`) starts none. `pad_apply` never makes such a pad.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_free_spot_vectors.py
ctest --test-dir build/native -R 'combat-free-spot' --output-on-failure
```

### Round 6

Research pin `faa7033` (`tools/ref_combat_round6.py`, `docs/spec/combat-round6.md`, `docs/combat-runtime.md` section 19). The bodies are in `combat_runtime_round6.cpp`. Round 1 through round 5 APIs, pins and vectors are unchanged. Every constant is an immediate the reference names, or an address `ref_creature` already names (`g_cullByCamera` `0x4039c1`, the camera focus `0x44f048`, the record's object word `+0x43d8`, 2^-16).

- **Poison gas** (`poisonGasSpell_run` `0x1fd0f0`). While the frame is below the signed end `+0x5c`, the float radius `+0x64` grows by one half when it is below the integer target `+0x60`, and `target / 10` (toward zero) particles of `0x871088` spawn: a unit-ball point times the radius, a second point times 5. On a frame divisible by 30, with the owner handle `+0x4c` set, each object in the box position ± `trunc(radius)` (capacity `0x40`) whose owner slot 20 returns nonzero takes `damage_apply` with flags `0x01000040` and entry `(0x17, low 16 of +0x6c, the same, 0)`, both amounts 0.
- **Disease corpses** (`DiseaseCorpse_run` `0x2ce4a4`). One box, position ± the integer `+0x54`, capacity `0x40`. Half is `+0x50 / 2` toward zero. A candidate is `(flags & 0x28) == 0x28` with the owner handle set; it counts even when `pool_new(0x64)` returns 0. A block is the round-3 `DiseaseCloud_ctor` at the victim's position with the word `+0x50` and the owner. The walk stops at the end of the list or once the count reaches half.
- **Multi-fire hits** (`MultiFireProjectile_onHit` `0x2da610`, `MultiFireProjectile_end` `0x2da928`). An empty owner handle returns. A player owner skips a victim whose flags meet `0x01000040`, and a creature (flag `0x20`) whose `+0xa8` has bit `0x04000000`; otherwise the range is slot 13 (stat 0, the level `+0x68`, the handle, cap 10) times `0x3c23d70a` divided by slot 10 (stat 2), applied to the record's weapon words `+0xd0`/`+0xd4` and rounded half away from zero. Any other owner returns when its slot 20 is 0; otherwise the minimum is slot 13 and the maximum adds a `0.2` spread from two draws of the bolt's generator. The damage is `damage_apply(other, owner, 100000, 0, 0, 0x40000, no entry, 0; min, max)`, then the end with reason 2. There is no already-ended test in the hit. The end returns when `+0x60` is set; otherwise it releases the trail `+0x74`, and for a nonzero reason draws once, selects format `0x54f598` and `fx_spawn` 8 under level flag `0x40000`, else `0x54f5b0` and `fx_spawn` 2, and plays the `res_find(0x54f560, …)` sound on the bolt. It deletes the bolt and stores 1. The format text is not in this repository, so the printed name is recorded as (format, `(draw & 3) + 1`) and the resource service sees the lump with an empty entry. `Skill_getStatFor` `0x3c87f0` and `Skill_getStatForHandle` `0x3c8864` (slots 12 and 13, `ref_skills_a`) are now dispatched for the hit's slot-13 call. The round-5 `MultiFireProjectile_run` still records the reason it passes, and now runs this end body.
- **Smoke** (`SmokeGen_run` `0x31c898`). `isNearView(position, 1000)` (`0x1e77c0`, `ref_creature.is_near_view`) returning 0 does nothing else. Otherwise `SmokeGen_stepParticles` is a recorded call (the research leaves it a stub). A signed `+0x48c` ≤ 0, or a frame whose low three bits are not clear, stops there. Otherwise every object in the box position ± (`+0x478` × 3) except the generator, except one with flag bit 0, and except one without flag `0x20`, takes `damage_apply` from the generator with flags `0x0100c040`, entry `(0x26, 1, 1, 0)`, both amounts the integer `+0x48c`.
- **Pus Storm** (`Pustule_run` `0x30bde0`). A burst whose timer float exceeds `0x420eccfb`, or a timer of 301 or more, ends the blob. Until the burst the blob falls (`+0x60` gains `0xbe4ccab3`), and three particles of `0x8e7548` are laid back along the step. Reaching the floor `+0x54` clears the timer, sets the burst, plays `res_find(0x552328, 0x552388)`, and plants ten drops from `terrain_height` (the `height_below` service, argument 8.0) plus 12. From the burst tick on, each drop moves, gains `0xbee1c9f7`, and spawns a particle of `0x8e74f8`. The only generator drawn is the blob's own.
- **Arena** (`SpellArena_run` `0x305bbc`). The signed counter `0x5038c8` clears creature flag 2 below 61 and sets it otherwise. From 30, `int(23 f)` particles with `f = 1 - (counter - 30) × 0x3bda740d`; every fourth uses `0x8e71e0`, the rest `0x8e7190`. Each is a sample point of the creature's model moved back along the angle of a second draw. Both tints become `(0x56, 0x35, 0x21, 0x3d)`.
- **Horn omen** (`SpellHornOmenEffect_ctor` `0x2fc574`, `_run` `0x2fc684`). The constructor stores table `0x51ae58`, one shared draw at `+0x10`, `particleGroup_create` at `+0x1c` (a new `particle_group` service) and the end frame plus 600. The run tints the creature `int(a × 0x3bea0ea0 × (225, 20, 0, 100))` below 140 ticks and `(225, 20, 0, 100)` after, and lays up to 150 sample-point particles of `0x8e6c40` from the creature's model at the origin with facing 0.
- **Planar steel** (`SpellPlanarSteelEffect_ctor` `0x2fb728`, `_run` `0x2fb9b4`). The constructor stores table `0x51ae28`, fills the light at `+0x30` (the creature's position, colour `(0x4b, 0x4b, 200, 0xff)` twice, 1 divided by a zero radius), the end frame plus 240, the countdown `int(creature +0x20 × 3)`, the orbit angles 0 and `0x7fff`, and plays `res_find(0x551320, 0x551328)`. The run moves the light, swings its radius with `sin16(frame × 1000)`, tints the creature, counts down, fires the burst once (60 particles of `0x8e6ba0` and `res_find(0x551320, 0x551340)`), and lays the two orbit glows and sparks, ten falling sparks and ten sparks 5 above `terrain_heightAtPoint` (a new `height_at_point` service).

**What refuses.** The research does not have the destructors of these classes: section 8 leaves every spell destructor but two unmodelled, and no object destructor of the gas cloud, the corpse or the blob is read. Each "slot 1 with 3" is recorded (`delete_self` / `delete_spell`) and then refuses unless the slot holds a destructor the runtime already models. So the gas at its end frame, every corpse after its walk, the blob at its limits, and the two spells at their end frames refuse there. (Since combat-ctors round 2, a cloud built by `poisonGasSpell_ctor`, as potion 2 builds it, carries table `0x50d500`, whose slot 1 `poisonGasSpell_dtor` `0x3c7878` is executed in research-combat-ctors-2, so that cloud ends; this round's fixture objects still refuse.) The tables `0x51ae58` and `0x51ae28` are stored as the constructors store them; their words were not read, so nothing is planted in them, and creature slot 17 does not reach these runs. The spell block (`World::spell_block`, 0x80 bytes) is the difftest's size: the script built-ins `hornOmen` `0x146628` and `planarSteel` `0x1466d4` that allocate these spells were not read. `World::spawn_effect_fixture` builds a fixture object for the four object runs: a pool block, a fixture table whose slot 4 is the run and whose other slots are 0, and the owner handle. The classes' constructors are not in the research. A negative sample count, and a sample count of 0 with particles due, refuse (the reference raises at the same point). Nothing in the live Full fight creates these objects or spells.

Still refused, because this round does not execute them: `StaticFire_run` and its constructors, `SmokeGen_stepParticles`, the destructors of these classes, deflection, the bow release composed into the player tick, monster death states beyond the bodies already composed, Skeleton and AntQueen past state 1, Soul's thrown-state hits, a nonzero silent passed to block sparks, and haze kind 3.

`native/tests/vectors/combat_runtime_6.txt.gz` holds 46 cases from the pinned reference: five gas runs, five corpse runs, seven hits (player and monster owners, both skips, a friend, an already-ended bolt), four ends, five smoke runs (camera and player near-view), five blob runs, five arena runs, four horn runs and its constructor, and four planar runs and its constructor. The reference's call answers are chosen so that the native world's own bodies give the same answers on the matching fixture; the replay checks the enemy relation and the box order before it compares. It compares particle rows, the damage arguments (by victim), box arguments, cloud constructor arguments, stored words, sounds, effects, the printed name, the trail, glows, sample-point arguments, the shared seed after a constructor, and each refusal (6,830 checks). `combat-runtime-6-host` checks the slot-4 dispatch, the composed volley end and the refusals.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_runtime_6_vectors.py
ctest --test-dir build/native -R 'combat-runtime-6' --output-on-failure
```

### Findings

- **Shock of Fire presentation names.** The composed reference builds its presentation table before `ref_combat_spells` adds `voice_setPosition` and `fx_ringBurst`, so it refuses Shock of Fire's first run, while the spells runtime treats both as presentation. The native Full composition treats them as presentation. The generator applies the same one-line adjustment, recorded in the manifest.
- **Unbounded re-dispatch.** A Shock of Fire burst that damages a non-creature object (for example a LightEffect) reaches the GameObject default take-damage slot, which the reference re-dispatches without end (`RecursionError`). The round-2 corpus replay still refuses that. On the round-3 Full composition the empty slot `0x3c7e88` returns, which is what lets an arrow's light sit inside a radius damage without looping.
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

## Full fixture decisions: the Skeleton's placement (combat-ctors round 4)

The Full fixture's slot-5 Skeleton at (70, 100) (`combat_play.cpp`) is an **authored fixture choice**, like the AntQueen's (-110, 110). It is not a research value. The research's round-5 probe placement is (70, 140) (`research-combat-ctors-4` `6baf4cd`, `full-placement.md` 6.4); y = 140 is off the synthetic level's flat ground (±128), which the fixture's placement check demands. (70, 100) keeps x, lies on the flat ground and is on the far side of the player from the AntQueen (slot 4) and the Soul (slot 6). In this fixture the Skeleton first enters state 3 at frame 282; the research's frame 123 belongs to (70, 140) and is not comparable. Placing it makes the Skeleton's constructor and equipped-holder draws on the shared seed, so it changes the seed-driven rolls of every later actor and the Godot `FULL_TRACE` (a new actor pair per line plus shifted seeds). That is a fixture consequence, not a traced event. The earlier sentences above that say slots 4 and 5 stay empty describe rounds 1 and 2.

**Loaded-data services (round 4).** `model_getMeshJoint` (`0x179358`, the u8 at +6 of the 10-byte mesh record, `model-texture.md`) and `resref_get` (`0x17377c`, the cached data pointer, `engine.md`) are call-outs with no modelled result in the research (`ref_phprop.py`, `gut-chunk.md` section 9). The World no longer answers them with 0 and "the first word at the pointer": `Services::mesh_joint` and `Services::resref` supply loaded data, and a missing function refuses (`Skeleton_shatter` → `PhProp_ctor2`, `Gold_ctor2`). The synthetic Full fixture supplies neither.

**Soul_isDemonNear, standalone.** `combat_ctors::soul` takes `typeid(obj)` of every box object, as `soul-run.md` and `ref_soul_run.py` do; it no longer skips objects with an unset slot 0 or an unknown getter and refuses instead. The World answers `Soul_isDemonNear` through its own call-out and does not take this path.

## Combat constructors, round 5

Research `t3/research-combat-5` `f33d1a3` executes `rigidbody_init`,
`rigidbody_updateDerived`, `quat_fromMat3` and `quat_normalize`. The native
PhProp constructor now stores their complete body output and spends exactly one
cosmetic draw for `rigidbody_init`; the shared seed is untouched. Full World
objects now enter the already executed `PhProp_run` and `GutChunk_run` bodies.
The synthetic Full fixture has no loaded model hull table, so its AntQueen
blood chunk stops at `model_getHullPoints returned null` on ready tick 269.
It does not substitute a hull or claim that a live bone or gold pile settles.

The cold-arrow timer and exploding-arrow `LightEffect_ctor` now have exact
standalone bodies. The World composes the latter for its kind 1/2/5/6 arrow
light calls, recording the full light record and its one GameObject shared draw.
The elemental arrow family remains a separate `skills_b` runtime, with no
Full World spawning route. The round-5 direct `StaticFire_ctor`, shared init and
particle push bodies execute with supplied host services; its World path
refuses when no path-grid service is supplied, and its run, draw and destructor
remain unexecuted. `Minion_run`'s executed prefix places a held Soul after its
animation step; states below `0x18` still refuse at the unexecuted dispatch.
