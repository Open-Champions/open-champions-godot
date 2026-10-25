# RTA Minotaur class and shared-World adapter

Evidence: `research-minotaur` at `27eb6ae309c72e15a31fc4b04d012b833f15031f`,
`docs/spec/minotaur.md`, `tools/ref_minotaur.py`, and its two executed
differential probes. `audits/research-minotaur-fix.md` reports PASS. Executed
means bounded EE execution with recorded service call-outs, not a PS2 fight.
This implementation is RTA US v2.00 only.

## Implemented

- Factory allocation `0x470`, constructor `0x288fdc` and definition `0x501200`;
  the existing base Creature constructor runs before the class-local body.
- Vtable `0x5165f0`, embedded initialized holder at `+0x3c8`, radius 25,
  flags, frame stores, ordinary/boss own-RNG variant selection, model masks
  and all eight animation event registrations.
- Filtered state entries, ordered common tick, attack states 6–11, first-charge
  overlay and cooldown, reaction, guard deadline, death animation selection,
  held-part drop requests and the original loot arguments. Gold chance is
  raw `0x3f199999`, not a rounded replacement.
- State completion reads the native animation flags; charge/guard deadlines
  use the recorded strict signed comparisons. State 0 has no invented idle
  deadline: its observed decisions are sight and distance from home.
- Existing exact monster statistics, including unsigned percentage lookup,
  own HP roll and level/difficulty scaling; 8,250 synthetic statistics vectors
  cover both seeds, levels 1–160 and the five Valor-scaled levels.
- Disc-time definition identifiers, clip binding and event advancement. No
  Minotaur archive, model, texture or animation identifiers are compiled in.
- One authority object, memory body, HP and RNG identity. Player damage reaches
  the inherited damage slot. The session fight uses the player-position activity
  mode rather than an unset origin-camera focus.
- Actor registry entries are retired by World membership, pointer and serial,
  preventing stale entries or dangling insertions when a pool address is reused.
  Synthetic tests cover standalone reconstruction and shared Minotaur adoption.
- Godot presents the selected class model, visibility mask, scale and position
  from the shared observation. This is a rest pose, not verified animated play.

## Explicit boundaries

The factory host still restricts construction to one player at difficulty 1;
statistics at other difficulties are tested independently, not enabled campaign
sessions. Minotaur placement properties other than `name` refuse because their
init services are not bound.

Live descriptor contents and delivered Minotaur damage, the target-attacking-me
predicate, bounds-derived initialization, animated appearance/root trajectory,
held-part attachment and PhProp publication are not established by the pinned
class research. They stay counted refusals. Gold scatter/allocation, ground-item
publication, pickup and credit remain unported. The existing unarmed player is
a labelled stand-in, not the campaign's original equipment.

A diagnostic low-HP probe can exercise death, XP and loot call ordering, but is
not a natural kill. The corpse movement path can still refuse when the inherited
death transition removes its grid node. The ordinary playthrough currently kills
a boar, not a Minotaur, and still reports `blocked_unported_combat` for loot
publication/pickup/Gold. None of these results establishes PS2 combat parity.

## Checks

`minotaur-class` checks 1,502 authored cases / 19,557 assertions over seeds
20973 and 7. `minotaur-stats-exact` replays 8,250 reference-generated cases.
`campaign-actors` and `campaign-fight-adopt` check address reuse and shared HP.
The acceptance report is `work/loop-minotaur-report.md`; private logs and
screenshots remain under ignored `work/minotaur-final/`.
