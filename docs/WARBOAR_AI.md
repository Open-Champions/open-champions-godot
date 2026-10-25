# WarBoar AI in the shared campaign World

RTA US v2.00 only. `campaign_actors::Runtime` runs the WarBoar family over
the level factory's existing object and memory. `campaign_fight` supplies the
same combat World, Player, handles, terrain, clip runtime and damage pipeline
used by the player's melee. No replacement enemies or periodic damage timer.

Evidence: `research-boar-ai` at `3f81c89215af3a59337709f98c75385370b7cea0`,
`docs/issues/warboar-ai-handoff.md`, `tools/difftest_warboar_ai.py`, and the
PASS audit `cursor-runs/audits/research-boar-ai-fix.md`. Inherited contracts
are `research-campaign-combat` at `328b072`, `docs/spec/creature-tick.md`
sections 3, 6, 9, 11 and 12, and the already ported animation/combat models.

The class's executed prefix is pending damage, base run (with early return),
activity (with early return), speed-1 animation, knockback at raw
`0x3e4ccccc`, and force-0 acquisition, followed by the class switch. Queries
read current memory after these calls. The filter's returned state is used,
including root, stun and attack-cooldown redirects and idle target clearing.
The campaign host selects the existing per-player visibility-input mode;
activity follows the current player positions rather than a fixture's stale
spawn-time focus. The factory object's three handle heads are shared with the
combat view, so movement retains its original grid node and run-list identity.

## Implemented contract

- Idle/wander call the pointblank-first sight wrapper with force 0. It keeps
  radius-adjusted near/far and strict far/angular bounds, the ray-mask mode,
  and the requirement for a terrain answer outside the inclusive [0,1] range.
- Acquisition reuses staggered grid scans, enemy/team gates, attacker/current
  biases, distance ties and the conditional 480-frame home leash. Chase does
  not add a new sight test on every tick.
- A constructed boar's clock and deadline remain zero. Entering idle writes
  the creature clock plus 180; expiry is strict. Wander uses the existing
  sparse own-draw home-point helper. Chase and retreat use the existing
  avoidance, waypoint, turning, collision and stuck-recovery bodies.
- States 3 and 5 retain ordered target and alternative-enemy cones, the
  per-object LCG and its short-circuit draw counts. State 7 is supported when
  supplied, but the class never chooses it.
- Attack entry resets the hit latch and records the last attack. Events come
  from native clip advancement, including constructor-registered absolute
  event times. State 9's continuation is raw `0x3f000000`, mask 2. Event 1
  dispatches every time it is supplied; the hit latch only gates turning.
  Event 2 can enter state 5 even if animation flag 8 has just cleared.
- Hit dispatch uses signed damage fields and the existing multi-victim arc
  and damage pipeline, preserving large/small attack flag draw differences.
  State 18 is rejected while old state is 6..15. Reaction, stun recovery and
  state-16 cooldown retreat use the researched conditions.

## Executed state-entry callbacks

`tools/check_warboar_ai_research.py` reuses the pinned probe with synthetic
memory and recording services. It checks all original 530 cases plus these
12 complete entry signatures and a finished-animation continuation case:
543 cases on each of seeds 20973 and 7. It imports no original translation.
Offsets are definition offsets, flags are raw words, and blend values are
raw f32. Base filter/path/animation callbacks remain stubbed in that probe.

| State | Offset | Flags | Blend in / out | Sync |
|---|---:|---:|---|---|
| 0 | 0x84 | 3 | 3dcccccc / 0 | no |
| 1 | 0x94 | 0x803 | 3dcccccc / 0 | yes |
| 3 | 0x4c | 0x803 | 3e4ccccc / 0 | yes |
| 5 | 0x84 | 3 | 3e4ccccc / 0 | yes |
| 6 | 0x24 | 8 | 3e999999 / 3e999999 | no |
| 7 | 0x54 | 0x808 | 3e999999 / 3e999999 | no |
| 8 | 0x2c | 0x4008 | 3e999999 / 3e999999 | no |
| 9 | 0x3c | 0x4008 | 3e999999 / 3e999999 | no |
| 10 | 0x34 | 0x808 | 3e999999 / 3e999999 | no |
| 16 | 0x44 | 0x803 | 3dcccccc / 0 | yes |
| 18 | 0x6c | 0x10 | 3dcccccc / 3dcccccc | no |
| 19 | 0x84 | 3 | 3dcccccc / 0 | no |

States 1, 3 and 16 clear the path; state 1 also picks a wander destination.
The public native tests use authored resource payloads and scripted service
answers, plus synthetic shared-World sight/leash/filter/multi-hit fixtures.

## Refused and unverified

Exact live event spacing and root trajectories under a real campaign's
blend/activity/status history are Unknown. The client computes displacement
and yaw from its native clip runtime; it does not certify retail trajectories
or synthesize missing root motion. The current Godot enemy view still draws
the class model's rest pose at the native position and facing.

Normal-route outgoing damage fields, victim equipment and delivered player
HP loss remain Unknown. The campaign host refuses boar-to-player damage
before shared RNG draws or HP writes, counting the reason. Synthetic inputs
exercise the full dispatch through the established combat pipeline.

A runtime path-search service is not bound. A move needing one refuses that
move with its exact partial mutation prefix; sensing and reactions continue.
There is no invented path or fabricated successful search. Wet movement,
unported status services, rank/tint loot, death animation and ground-item
limits retain their existing refusals. Script/event-driven group activation
is Unknown; no pack scheduler or low-HP fear behavior is added.

The optional `champions-campaign-fight-probe --ai-check` reads the selected
disc and reports aggregate counts. Its player placement after 120 ticks is
diagnostic; enemy state, targeting and stats are untouched. This is native
integration evidence, not an unassisted campaign or PCSX2 capture.
