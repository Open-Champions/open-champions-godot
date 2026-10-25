# RTA creature water services

The portable `champions-creature-water` target implements the supplied-grid
`query`, `water_test`, and `water_disturb` contract. It is independent of
Godot. The owner supplies a relocated spatial tree, chunk table, payloads,
allocated ripple grids, and the existing sparse `creature_hp::Memory`. The
service neither loads a WORLD file nor creates a grid.

`Service::query` returns water chunks in traversal order. Its capacity includes
the null terminator, so capacity 2 admits one hit and capacity 16 admits 15.
It preserves duplicate leaf indices, visits the right child first when both
split sides qualify, uses inclusive chunk bounds, and writes optional output
pointer slots in original order. `water_test` uses its additional XY padding,
returns presence, and writes the first hit's minimum Z only when requested.
`water_disturb` checks cut-skip before the query, counts hits even without a
grid, and applies signed16 ripple writes in X-outer/Y-inner order. It does not
read payload masks or neighbor indices.

The explicit `install_creature_callbacks` adapter binds these operations to
the existing `creature_movement::Model` callback names only when the service
shares the creature model's `Memory`. The service must outlive those callbacks.
The existing creature bookkeeping, splash sound, and object RNG order remain
in `creature_movement.cpp`. No Session/Controller policy is installed: that
owner must supply relocated runtime chunks and valid allocated grids before
enabling water. Static WORLD geometry still refuses water and the dry Session
gate is unchanged.

Refusals cover capacity below two, tree work beyond 65,536 nodes, invalid split
axes, more than 16 pending nodes, nonpositive disturbance radius, nonpositive
rounded X width for a selected grid, raw32 address overflow, and exhausted
write history. Abnormal pointers and overlapping/aliased memory are outside
the research contract. Completed writes remain visible if a later refusal
occurs. Grid allocation, water simulation, neighbor exchange, rendering,
Norrath behavior, and original live-game execution remain unverified.

## Evidence

Read-only research branch `t3/research-anim` at
`2502e2bfac58203ac0174fccd7e0953b36d06439`:

| Source | SHA-256 |
|---|---|
| `docs/spec/water-services.md` | `7747301090f3f09e482c5bf173e0fe3381979999f475d7a275e2a366521dcd58` |
| `docs/spec/research-anim-handoff.md` | `192189db8e4339411ac5e7b41c7f8d368acb7afe8f534a1532ce0c7b2a0ddd82` |
| `tools/ref_water_services.py` | `b822e671af336c2f73f75aacf02a3fbd0ab33128a70058f4985ec67599b0bdf1` |
| `tools/test_ref_water_services.py` | `2895bbe1470b8ecbe6c16057353d1a9e41a8bcedf2fbbf21a2fcad73ce87f0c8` |
| `tools/difftest_water_services.py` | `c85fe61e15d2f0dfec9fe3b876fd73111df29b2af1ba56f83137bccdaefe3a66` |

The research handoff reports 576 synthetic original/reference calls in four
arithmetic modes. Native tests port the nine public synthetic cases, including
boundary, duplicate, capacity and two-child multi-chunk traversal, and check
the opt-in creature adapter. Passing these tests is not a live-water claim.
