# Creature AI, missions 1–3 (decision kernel)

`native/include/champions/creature_ai_m1_3.hpp`, `native/src/creature_ai_m1_3.cpp`,
test `creature-ai-m1-3` (`native/tests/creature_ai_m1_3_tests.cpp`).

Source: research branch `t3/research-campaign-combat` at `fbb2e1d`,
`docs/creature-ai-m1-3.md` (cited as `C:<line>`), `docs/monsters-2.md` (`M2:<line>`),
`tools/models_creature_ai_m1_3.py` (`MOD:<line>`), `tools/ref_creature_ai_m1_3.py`
(`REF:<line>`), `tools/difftest_creature_ai_m1_3.py` (`DT:<line>`). Audits
`research-creature-ai-m1-3`, `-fix`, `-fix2`: PASS with four LOWs, respected here
(below). Only claims the research labels Executed or Observed are ported.

## What it does

`tick(Class, Context, Answer)` runs one class run function's per-tick decision order
for the twelve classes placed in missions 1–3 (Kobold, WarWraith, Orc, Goblin, Ogre,
Warrior, Clockwork, Droid, IronGuardian, Froglock, Amphian, SeaMonster). The caller
answers each query (`sight`, `see`, `cone`, counts, `hitAtReach`, …) and executes each
emitted action. The result is the ordered event list and the object's own generator
state (`+0x44`, `GameObject_rand`).

- Entry and both tick exits (C:66-73): `Creature_run` nonzero, `updateActive` zero.
- Head order and knock-back scale per class (C:73-82); WarWraith picks before
  knock-back and skips the pick when dead.
- State machines, early returns, thresholds and clock-compared fields of C:253-479:
  ally counts (`> 2`, `> 1`, `> 0`, `>= 1`, `< 2`, `== 0`), box count `< 3`,
  deadline `+0xf4` strictly against the clock, WarWraith `+0x3cc`, Orc shaman `+0x47c`,
  goblin shaman `+0x3d8`, Droid `+0x3cc < clock`, IronGuardian `+0x3cc` and its
  counter `+ 1 > 120`.
- Amphian blocks only when HP `+0x228` < 0.5 × max `+0x22c` (strict, relative), and
  makes that draw only below half (C:459-465). No other class decision reads HP on any
  walked path (C:135-146).
- WarWraith fade byte `+0x3c8` read signed: ≥ 99 turns, 100–127 delete in 0x14,
  128–255 act as negative (C:279-298, M2:6128, M2:6277-6279).
- Orc shaman 0x17 extra `see(0, 6000, 0x7fff)` turn (C:329-332, M2:5492-5494).
- IronGuardian 7/8 turn only when the alive-player count `0x4f8420` is exactly 1
  (C:432-435, M2:1392).
- Rare own-generator branches (`% 30`, `% 60`, `% 5`, `% 3`, `& 3`, `& 1`).
- Froglock water (`Context::water`, required; Observed, M2:4139-4160, never walked):
  a blowgun in reach requests 8 instead of 7 while the standing-in-water cflag is set
  (C:447-448, M2:4158-4159); helper `0x265754` (water just entered, or water ahead just
  ended) re-requests the current state after `moveToDest` in 1, after `moveToTarget`
  in 3, and before anything else in 0x10 (M2:4143-4160, M2:4178). The `+0x460`,
  `+0x461` and `+0x462` bytes accept only 0 and 1.
- Clockwork object flags (`Context::object_flags`, required): flag 2 gates the dead
  burst (M2:3711-3716) and the hammer puff velocities (M2:3721-3723, MOD:738-742); the
  burst itself sets flag 2 for the rest of that tick.
- Variant values outside the walked set refuse (Kobold `+0x3cc` 0–2, Orc `+0x478` and
  Goblin `+0x3d4` 0–5, Clockwork `+0x3c8` 0/1, Froglock holder 0x12/0x10).
- Event arguments are integers × `ARG_SCALE` (10000): the research compares its float
  arguments to four decimals, so the knock-back scales (0.4, 0.1, 0.2, …) are held to
  that precision, not as guessed IEEE bits.

`set_state(Class, SetContext, request)`: the `Creature_filterState` redirects of
C:115-122 (same order as the audited `monsters_1::Model::filter`), the no-case → 0
lists (C:124-127) and the executed entry table (MOD:1164-1196): timers as
`clock + base + draw % mod` (range Executed; own-draw source Observed in M2),
last-attack fields, cflag `0x200`, death calls. SeaMonster's 0x10 writes no `+0x318`;
Droid's 0x14/0x17 call neither `dropLoot` nor `loot_generateDrops` (C:131-153).
A filtered state with no executed landing throws `Unsupported`: only `SET_ENTRY` rows
(MOD:1163-1197) and the `check_filter` landings 0/3/5/6/0x10/0x13/0x14 (DT:442-461) were
executed, so for example WarWraith 8, Droid 9, Goblin 7/8, SeaMonster 7–9/0x17/0x18,
Orc 7 and Warrior 9/10 refuse (C:124: a class without a case stores 0). For a filter
landing without a row `entry_known` is false: the stored state was executed, the other
entry writes were not (for example WarWraith 0x10: C:131 says "all classes", but the
entry table has no WarWraith row). Every row ran with the class variant field at 0
(`SetContext::variant`); any other variant refuses.

`hit_at_reach(ReachProbe)`: Euclidean distance from attacker + facing × reach strictly
below 60 on x, y and z; radii ignored; object flag 8 skips (C:166-179). It uses the
ported `cos16`/`sin16`; no offset is added.

Level tables: the creature record counts per level, the factory pick by type name,
and the four property-key counts (C:28-60), by name and count only.

## Respected LOWs

- The ±60 asymmetry offset and its "0.00001" size are **Inferred** (CAIF2-2). The
  kernel adds no offset; the test asserts only the two executed rows (+60.0 hits,
  −60.0 misses at facing 0).
- WarWraith's HP load is scoped to walked paths (CAIF2-3); the drain heal write is not
  ported (not compared).
- monsters-2 line citations are the shifted ones at `fbb2e1d` (CAIF2-4): `M2:6128`
  fade range, `M2:6202` WarWraith HomeRange, `M2:6178` one wraith per victim,
  `M2:5492-5494` and `M2:6277-6279` the dated corrections.

## Still refused (Unknown)

Kobold 9 body and entry, Goblin 7/8 and Froglock 7/8 bodies, any state the research did
not walk, unplaced level types. Not in the kernel at all: Cazic, Critter and Catapult
runs, IronGuardian Droid spawning, cutter/IronGuardian missile cadence, slot 20/21
overrides by execution, HP rules inside the stubbed base `Creature_run` /
`processDamage`, field writes the research did not compare (fade, drain heal), live
trajectories and animation-event spacing.

## Vectors

`tools/generate_creature_ai_m1_3_vectors.py` runs the research models (not the
executable) on a sample of the research's own synthetic context grid, enumerates every
answer path, and writes `native/tests/vectors/creature_ai_m1_3.tsv.gz` with its `.json` manifest (24,206 paths,
seed 20973, all twelve classes). `tools/test_creature_ai_m1_3_vectors.py` checks the hashes and expands it into the build directory; the C++ test compares every event and the final
generator state.

## Wiring (round 2)

- `mission2_actors`: the Droid fuse is decided by `droid_fuse_due` and requested through
  slot 27; `set_state` accepts only the executed entries Clockwork (cutter) and
  IronGuardian 9 and Droid 0x17 through `set_state` here. It applies the filter service,
  the entry writes and the landing, then refuses the clip start and the rest of the class
  setState, whose order was not compared. Every other non-death request refuses as before,
  with no service call. Post-death requests refuse.
- `blood1_actors`: targeted state 7 runs the kernel's state body (from `Tick::body`) for
  the Kobold variants (event bits 0 and 1) and the WarWraith drain. The drain asks a
  ColdWeapons service and `hitTargetAtReach(0x404)`; a landed drain refuses, because
  the max HP / 15 heal (M2:6165-6167, Observed) was not compared. `set_state(8)` is
  accepted for the plain Kobold only (`+0x3c8 = 8`), then refuses the clip start.
- The kernel is in the `check_simulation_boundary.py` census (84 files).
