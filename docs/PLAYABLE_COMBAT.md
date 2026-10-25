# Opt-in scene combat fixture

The wave-4 integration exposes `start_play_session(combat_fixture = false)`.
The default retains `rta-play-session-v1` and its bounded continuation saves.
Passing `true`, or selecting the scene's **Combat fixture** checkbox before
**Start play session**, selects `rta-combat-fixture-v1` instead.

This is an authored synthetic encounter over the released, exact combat runtime.
It is not the original encounter loader, player action switch, attack controls,
character progression, original animation pairing or full game. The supplied
runtime contract remains [COMBAT_RUNTIME.md](COMBAT_RUNTIME.md).

## Ownership and ready frames

`combat_play::Session` owns one combat World, one `SessionAdapter`, a one-local-peer
native wire machine and a native task list containing the priority-10 combat task.
No legacy actor World, creature movement binding, old base-combat task or old
`runObjects` scheduler is active for these actors. The bridge destroys its previous
`native_save::Scene` when publishing the new combat owner. Switching back creates
or loads a new owner; it never migrates an actor checkpoint between policies.

The native wire machine invokes the adapter once per ready task boundary. Wire
frame 1 maps to combat frame 0; successful execution publishes the advanced combat
frame and shared seed to the wire machine. A submission without a ready frame does
not animate or run actors. Pause, stop and presentation getters never tick. Ready
execution exceptions retain the completed prefix and latch the session failed and
stopped, in addition to the adapter's own latch. No retry rewinds that state.

Histories are cleared at the next active ready boundary using the runtime's
`clear_history()`; this does not clear actor state, handles, scheduler or RNG.
The fixture has a separate 4,096-ready-tick budget and retains only the most recent
combat pass history. It does not inherit or promise the normal play session's
continuation format or its 36,000-tick acceptance. Observations identify
`session_mode=play`, `combat_enabled=true`, `combat_policy=rta-combat-fixture-v1`,
`history_policy=latest-tick`, `history_complete=false` and `movement_policy=unsupported`.
The old per-module history arrays are not combat retention measurements.

## Explicit fixture inputs

All spawn values are authored fixtures, not original constructors or encounter
statistics. There is one player (slot 0, team 0, HP 100, strength 20, level 1,
position zero) and two **Skelight** instances (team -1, HP 1, attack 100000,
positions +30 and -30 on X, facing toward the player). Remaining `Spawn` values
come from the documented runtime defaults. The player's fixture action state is
1, with its target handle linked to Skelight 1. Retained external actor identity
always uses the runtime's handle pool. No host pointer is an actor identity.

The policy explicitly selects `flat_fixture_services()`: empty-resource skill,
travel, water and sight fixtures are visible assumptions, never production
fallbacks. The selected collision Level backs height and slide services. Start
rejects nonzero height samples, rotated/modified surfaces, pieces or absent ground
at either creature spawn. Only flat, dry, unmodified ground is accepted. This does
not claim arbitrary WORLD raycast/visibility or scene-geometry collision parity.
For production encounters, real animation, skill, water, sight and travel services
must replace this explicitly named policy.

Animation start/advance uses the runtime owner's authored fixture: set actor
count/playing fields, emit mask 0x1 at synthetic swing tick 10, end a swing at tick
30 and a non-swing at tick 20. The clock advances only during that actor's native
animation phase. Skelights consume this route only. The scene's **Fixture: player
hit event** button calls `combat_hit_event(0, 1)` and queues the separate player
animation-event route. It does not select an attack or change the player's action
state; states other than 1 refuse. The pending event is delivered at the next ready
boundary, and pause/release discards pending presentation input. No input-to-combo
or stagger-recovery policy is invented.

The scene draws clearly labelled authored capsule markers from owned observations.
The original-art panel reads actual player record HP; no duplicate Godot health
state or Godot randomness is involved. Cosmetic model/clip preview and inventory
picture inspection remain independent presentation tools.

## Refusals and saves

Nonzero legacy movement/damage commands, masks with bits outside 0x1, unsupported actor slots,
creature event injection (which would duplicate the animation route), optional
recovery/facing/cache diagnostics and cache commands refuse. Runtime refusals for
walking, restricted player movement, weapons, scaled/heavy attacks, blocking,
knock-downs, spells, other classes, full destructor chains, PvP, network and other
unimplemented call-outs remain intact. This integration does not turn them into
successful no-ops. The supplied native runtime tests cover those lower-level paths.

Combat saves as a **fixture continuation** (`fixture-continuation-v1`,
[NATIVE_SAVE.md](NATIVE_SAVE.md#fixture-continuation-v1)): the selected ground, the
journal of every combat operation and a redundant witness; a Full save made while a
bridge draws also carries the bridge's exact effect state. Loading replays it into a
fresh fixture and must match the witness byte for byte. No legacy snapshot or dry
continuation can contain combat actors, and none is converted.
A valid ordinary continuation can explicitly replace the combat session after
normal atomic preparation. Invalid loads retain the current owner. Reset creates
a fresh combat fixture on the same selected ground. Starting without the checkbox
creates normal play again. The checkbox is off by default.

## Fight acceptance

`native/tests/combat_play_tests.cpp` and
`godot/tests/playable_wave_smoke_combat.gd` prove the integrated order:

1. Player and both documented Skelights are present and alive.
2. First neutral submission produces no ready tick.
3. At ready tick 12, two creature animation hits have reduced player record HP to
   exactly **84** (`1118306304` raw32), with the player still in action state 1.
4. A player mask-1 event at tick 13 immediately makes Skelight 1's HP nonpositive;
   the creature remains allocated and has not processed its own damage yet.
5. At tick 14, the victim's own run enters state 20 and queues deletion. Its
   external handle still resolves and the marker remains visible.
6. Tick 15 flushes deletion: its handle is null, its marker disappears and the
   second Skelight remains alive. Shared seed is exactly **1140036074**.

The combined Godot check first runs the existing real level library, animation,
original-art/HUD, inventory and long-play/save/load acceptance. It then opts into
combat, proves the fight and refusals, resets, and loads the ordinary continuation.
The graphical run captures all six fight states. These are scripted UI signals
and fixed native steps in the actual export, not a claim of manual playthrough.

Fresh commands, outcomes, source/asset hashes and capture paths are recorded in
the wave-4 section of [INTEGRATION.md](INTEGRATION.md) and local
`work/loop-playable-integration-r4-report.md`.

## Full composition fixture (wave 11)

`combat_play::Fixture::Full` (policy `rta-full-combat-fixture-v1`) is a third,
explicit opt-in. It runs the released `combat_runtime::World` in
`Composition::Full` ([COMBAT_RUNTIME.md](COMBAT_RUNTIME.md), *Round 2*): the
executed `Player_move`, the real `Player_run` tick, armed melee and combos,
blocking, projectiles and Holy Strike. The ordinary combat and animated fixtures
are unchanged and remain the defaults of their own selections.

Set-up is the research composed runtime's walk-and-combo scenario: a strength-40
player with a 30–40 / attack 50 / type 8 weapon, a 120-HP Skelight 80 ahead
facing the player, plus a second 120-HP Skelight 40 behind for blocking and Holy
Strike (skill 16, rank 1). Round 5 added three more authored actors (an AntQueen, a Skeleton and a Soul in slots 4-6); the 2026-10-21 audit (CAI-4) removed them because their constructors were not executed. Since combat-ctors (research-combat-ctors e9777eb) the three constructors, set-states and run prologues are executed, and the Soul is back in slot 6 at (-60, 100): `Creature_setCharm(-1, 0)` puts it on team 0 with a permanent charm, and it wanders in states 0 and 1 for the whole fight. Since combat-ctors round 2 (research-combat-ctors-2 5b92837) `AntQueen_run` runs in every state, and the AntQueen is back in slot 4, ahead of the Soul; the research probe ran her 3,000 idle ticks without meeting an unexecuted body. She stands at (-110, 110), an authored corner of the flat ground: at the round-5 (0, 100) the player's combo swings hit her and her blood stops the session at `GutChunk_ctor`. She still sees the player at ready tick 33 and runs her chase, flight, stings and landing. Slot 5 stays empty: a Skeleton gets past `Skeleton_isEnemy` `0x2a7e04` now and stops at its first sighting of the player, in `Skeleton_run` state 3 and `creature_allyInLineOfFire`, which are not executed. The same set-up arms Holy Shield on the Holy Strike skill already in the table, with a one-point model at the player's `+0x544` and a one-joint pose at `+0x114`. `give_skill(16)` is not called again. These are authored fixtures, not an original encounter.
Services are `flat_fixture_services()` with the session's terrain height,
`dialog_isOpen` = paused, no loot class. Animation is the research `PlayerAnim`
fixture (player clips 20 ticks; creature swings 30 ticks with mask 0x1 on tick
10), with one labelled addition: a started player track is at full weight at
once. `PlayerAnim` never writes a track weight, so `Player_run` state 8 (which
releases a block only when the top weight is ≥ 1.0) could never end a block.
Only block release and the state-5 cast phases read that word, and this fixture
casts through `World::queue_cast`.

Input: WASD become the stick on the next ready tick (one axis ±1.0, a diagonal
±0.70710677 per axis; a client mapping). J presses and holds attack (research
`press_attack`, held by default), K holds block, L queues Holy Strike for the
player's next run. Button masks 0x10 / 0x20 are the research fixture's words; no
original layout is claimed. The round-1 hit-event button and diagnostic damage
command refuse. The Full fixture saves and loads as a fixture continuation; loading a
Full save needs the recipe constants it was made with (length + CRC32 identity).

Every ready tick runs native-fx-2's `EffectSimulation` as the session's tick
observer, after the priority-10 World pass, on every machine, drawing or not
(see [PLAYABLE_FX.md](PLAYABLE_FX.md), *Wave 11*). Effect-class damage goes through
`World::damage_descriptor`, the same executed descriptor glue the composed spells
use. Combat-runtime-2's refusals stay: DiseaseCloud damage always allocates a
DiseaseEffect (0x1b0), which the World refuses, so the session fails on that
tick, identically headless and drawn; `fx_spawn` id 5 Fire, ice-shatter and
GutChunk constructors meet the same refusals.

Acceptance (`playable-combat-fx`, 39 native checks; `playable_wave_smoke_full_combat.gd`):
the stick walks the player >40 units; held presses chain swings with
`P+0x460` counting up by one and ≥ 2 swing variants; the first armed hit lands
on swing start + 16 (combat-player-melee.md 3); the killed Skelight queues
`obj_delete` in its own run the next tick and is destroyed at the next flush; a
held front block takes no health and releasing it does; Holy Strike cast on
frame 1 damages on frame 9 (speed 450, radii 12 + 12, 80 apart: 8 frames) with
own-generator puffs, a glow and its LightEffect; headless and drawing runs of a
300-tick scenario give byte-identical seeds and health.

## Full fixture extensions (wave 12)

Two authored set-up operations reach merged executed bodies. Both are
fixture-continuation journal operations (14 and 15), so a mid-run save
continues byte-identically (`fixture-save`, `wave12_extensions`).

- **Own-machine creature** (`combat_spawn_creature(index)`, picker beside the
  Full pad). One `World::spawn_class` creature (creature-ai, the ten
  own-state-machine classes in `combat_play::OWN_CLASSES`) in slot 3, at the
  fixture's authored (0, 80) facing the player, on the fixture's 120-HP
  authored placement. One per session. The placement is the fixture's, not
  original spawn data. Skelight, Gnaw, AntQueen, Skeleton and Soul can be
  built (Gnaw_ctor and the three research-combat-ctors constructors); Critter,
  GnomeNavigator, Mermaid, NPC and Natasla refuse ("constructor is not
  executed") before the World is touched. Over 300 ready ticks with the
  player attacking (`playable_wave_smoke_wave12.gd`) Gnaw, Skelight and Soul
  run; Gnaw ends in its chase state 3. Since combat-ctors round 2 the Skeleton
  gets past `Skeleton_isEnemy` and stops the session at its first sighting of
  the player, in `Skeleton_run` state 3 (ready tick 33; not executed). The
  AntQueen chases the player until his hit bleeds her into the `GutChunk_ctor`
  call, which has no World handler (tick 52; as a Gnaw's blood). A Skelight
  placed at (0, 80) is nearer the fixture's Soul than the player and attacks
  it; `Soul_takeDamage` ignores an attacker that is not a Demon, and the
  Skelight runs the whole 299 ticks.
  A hit that bleeds a Gnaw, its death and a missing flat ground still stop the
  session.
- **Thrown potions 1–3** (`combat_drop_potion(kind)`, buttons Potion 1/2/3).
  `World::spawn_thrown` at the player with the `Thrown` defaults and zero
  velocity, since the throw call site and its arc are not contracted. The
  combat-runtime-4 `playerProjectile_run` bodies then run. Kind 3 lays the
  ring's LightEffect and floor decal and the session keeps running. Kind 1
  builds five `LooseFire_ctor` `0x202b70` objects, each with 3 or 4 `Fire`
  objects through `fx_spawn` id 5 (research-combat-ctors 98c081a); the fires
  burn for 100 runs and end through `Fire_dtor` `0x3c5050`, the LooseFire
  through `LooseFire_dtor` (research-combat-ctors-2). A fire touching a player
  or a creature on a frame divisible by 20 applies its entry `(0x15, v, v, 0)`
  through `damage_apply` and leaves the burn the creature tick then hits with.
  Kind 2 builds its `poisonGasSpell_ctor` `0x202de8` cloud, which runs
  `poisonGasSpell_run` `0x1fd0f0` (combat round 6, merged from
  combat-runtime-6) and ends through `poisonGasSpell_dtor`. In `fixture-save`
  both run until the idle fight's own party wipe (ready ticks 725 and 787).
- **Holy Shield.** The Full fixture plants the sample model and the pose, then
  `arm_fixture_holy_shield` attaches a shield on skill 16. A player hit with
  mana and a pose follows `model_getSamplePoint` at scale 1 (241 particles).
  The round-4 stub remains the path when the pose word is 0. The shield's
  executed run (`0x2e8428`) steps its clock every tick and ends it through the
  base Spell destructor when the duration (skill statistic 1) passes.
  `SpellHolyShield_ctor`, deflection, the state-0 bow release and a nonzero
  silent block-spark caller stay refused.

The Full fixture needs flat ground at height 0 at its authored positions
(`terrain_height_below` at radius 4); a terrain without it refuses with
"combat fixture actor has no flat ground". It is exercised on the synthetic
practice terrain; it was not run on game-folder levels.
