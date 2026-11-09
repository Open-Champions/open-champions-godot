# Creature AI, missions 1–3 (decision kernel)

`native/include/champions/creature_ai_m1_3.hpp`, `native/src/creature_ai_m1_3.cpp`,
test `creature-ai-m1-3` (`native/tests/creature_ai_m1_3_tests.cpp`).

Source: research branch `t3/research-campaign-combat` at `fbb2e1d`,
`docs/creature-ai-m1-3.md` (cited as `C:<line>`), `docs/monsters-2.md` (`M2:<line>`),
`tools/models_creature_ai_m1_3.py` (`MOD:<line>`). Audits
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

`set_state(Class, SetContext, request)`: the `Creature_filterState` redirects of
C:115-122 (same order as the audited `monsters_1::Model::filter`), the no-case → 0
lists (C:124-127) and the executed entry table (MOD:1164-1196): timers as
`clock + base + draw % mod` (range Executed; own-draw source Observed in M2),
last-attack fields, cflag `0x200`, death calls. SeaMonster's 0x10 writes no `+0x318`;
Droid's 0x14/0x17 call neither `dropLoot` nor `loot_generateDrops` (C:131-153).
`entry_known` is false for a state without an executed row (for example WarWraith 0x10:
C:131 says "all classes", but the entry table has no WarWraith row).

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

## Not yet wired

`blood1_actors` (Kobold/WarWraith, isolated) and `mission2_actors` (Clockwork, Droid,
IronGuardian, live in `campaign_fight`) keep their own contracts this round. Existing
tests pin refusals that this research now resolves (for example
`mission2_actors_tests.cpp:241` refuses Clockwork `set_state(9)`, `:306` refuses the
Droid fuse request, `blood1_actors_tests.cpp:355` refuses the WarWraith targeted state
7, `:419` refuses `set_state(8)`). Wiring the kernel into them requires changing those
existing assertions, which this round does not do.
