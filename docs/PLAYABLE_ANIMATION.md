# Animated play fixture

`ChampionsSessionController.start_play_session(true, false, true)` explicitly
selects `rta-animated-combat-fixture-v1`. The scene selection menu exposes the
same policy. Default play, the Skelight fight, and the level-flow fixture retain
their existing contracts. Invalid combinations refuse before replacing owners.

This composition uses the released creature-animation Runtime, generated
Agony dispatch table, player `script_idle` action, pose builder and skinning
arithmetic. Independently authored two-joint clips and diamond meshes are
synthetic evidence, not extracted art. Each actor owns its animation state and
clip resources. Native integer skinning numerators cross into Godot only at the
presentation boundary. Rendering never advances animation or emits hits.

## Explicit cross-module boundary

Agony supports generated root-motion dispatch; Skelight is deliberately in the
animation table's unsupported `special` group. Combat World currently supports
Player and Skelight bodies, not Agony AI or its constructor. This fixture pairs
Agony state-six animation with two **synthetic combat carriers** running the
released Skelight state-six shared `Creature_hitTargetInArc` body. The UI names
that pairing. It does not claim Skelight animation or original Agony gameplay.
No unknown state filter, transition, resource lookup, or RNG draw is invented.
The original modules and their refusal behavior are unchanged.

The carriers start already facing and targeting the player, with the existing
fixture's team, damage and attack statistics. A positive attack counter keeps
this explicitly bounded fixture in state six. It stops at 90 ready ticks,
before its 120-tick synthetic clip ends. Uncontracted action transitions refuse.
Movement, player-hit injection and disk saves are unavailable in this policy.
The prior wave-four fight still proves player retaliation, death and removal.
The ordinary bounded play policy still supports 36,000+ ticks and continuation
saves; this fixture neither replaces nor converts that save format.

## Clock and hit delivery

A no-ready network submission does not advance poses. On each ready tick the
player idle advances once; each carrier advances inside combat's real
`step_anim` callback. `dispatch_events` classifies the returned animation word.
A typed `hit_test` with mask `0x1` enters `SessionAdapter.hit_test_event` there;
World consumes its queued event immediately after the callback returns. The
callback returns zero, preventing duplicate delivery through a second path.
Other hit masks refuse. There is no scene timer or hard-coded hit frame.

The generated Agony entry-three event at raw time `0x3f199999` (0.6 seconds)
lands on ready tick 37. Both carriers hit on that tick: health 100 → 84, two
damage events. The next event at 1.1 seconds lands on tick 67: health 84 → 74,
four cumulative damage events under the retained shared RNG. Ticks 38–66 and
68–90 do not repeat damage. Pause freezes network, poses and combat; reset
recreates all owners. The cosmetic preview remains a separate manual tool and
can temporarily replace the player's presentation without becoming combat's
clock or event source.

## Footsteps and audio

Wave 9 now integrates [native audio](PLAYABLE_AUDIO.md) through owned ready-tick
packets and explicit ClipRole. `play_animation::Actors` retains its optional
`set_footstep_hook(slot, DispatchEvent)` callback for source masks 0x4/0x8.
The stationary fixture has no walk emissions; its attack mask 0x1 drives synthetic
hit sound on the same ready tick as damage. A separate real FatCyclops walk
composition proves footsteps on ticks 13/49. Neither path consumes gameplay RNG.
Other observations, including corpse handoff, retain their prior gates.

## Checks

`animated-combat-play` checks exact tick-37 and tick-67 damage, native skinned
pose changes, no-ready/pause freezes, no duplicate hits, explicit refusals and
the 90-tick budget. `godot/tests/playable_wave_smoke_animation.gd` extends all
prior combined checks: ordinary long play/save/load, HUD/inventory, wave-four
fight, level gates, then native poses and exact-frame animation combat. It also
checks manual preview ownership, reset, and ordinary continuation replacement.
These checks use only committed or independently authored synthetic fixtures.
