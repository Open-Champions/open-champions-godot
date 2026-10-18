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

Combat advertises **no save formats**. Both bridge save methods refuse before
creating any file; no legacy snapshot or continuation can omit combat actors.
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
