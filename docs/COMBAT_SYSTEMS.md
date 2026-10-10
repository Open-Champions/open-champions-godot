# Native combat and statistics

`champions-combat` implements the published Return to Arms combat reference model
as portable C++17. It depends only on `champions-core` and the standard library.
`champions-combat-items` explicitly bridges the item port into combat records.
These systems are not yet connected to a playable scene.

## Contract and measured coverage

The contract is [tools/ref_combat.py at 7212ecf](https://github.com/Open-Champions/champions-return-to-arms-re/blob/7212ecfecbeae8dd2015f6c82897226f49a94695/tools/ref_combat.py),
SHA-256 `b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781`.
The [combat specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/7212ecfecbeae8dd2015f6c82897226f49a94695/docs/spec/combat.md)
provides its behavior and evidence limits. Its MIT license and contributor
copyright apply to this authored translation. Tests supply made-up records,
tables and callback returns; no executable, original tables or raw decompilation
is loaded or published.

| Component | Contract | Exact generated cases |
|---|---|---:|
| Common | Single arithmetic, library math, soft doubles, records, random histories and world callbacks | 284,050 |
| Damage | Hit rolls, block/critical/status/resistance/reflection and attached spell helpers | 44,704 |
| Statistics | Monsters, experience, player equipment/statistics and Unicode names | 38,829 |
| Item bridge | Record conversion and combat-derived statistics | 8,192 |

Common tests include every 16-bit sine/cosine angle and signed16 value, all
sixteen arithmetic configurations and a targeted double-exponent-gap sweep.
Damage traces compare mutated objects, spells and player fields, ordered effects
and callbacks, every shared and cosmetic draw, errors and callback exceptions.
Statistics traces include level-up callbacks, experience allocation, figurines,
resistances, overwritten HP/mana bonuses and callback-mutated weapon branches.
Unicode16 lowercase tables are generated from public Python behavior, including
expansions and contextual final sigma; they contain no game text.

## Gameplay arithmetic and a reported model difference

Combat honors the caller's selected FPU configuration. It uses integer bit
patterns throughout; no host floating-point types, libm, viewer transforms or
Godot physics enter its calculations. Combat supports the half-exponent power
branch that the item model leaves unsupported. Fraction-style reference doubles
normalize signed zero and the reference NaN sentinel explicitly.

The two published models differ at extreme opposite-sign double addition.
For operands `0x3bf8f82720000000` and `0xc26cf51fa0000000`, the pinned core
`dadd` returns `0xc26cf51fa0000000`; the combat Fraction-add/chop contract returns
`0xc26cf51f9fffffff`. Consequently `base_roll(0xffffffff, 0, 65535)` produces
`0xfffffffe` in the combat model, versus `0xffffffff` when rewritten using core
helpers. The combat adapter retains the Fraction contract's borrow instead of
changing the verified core. This is a demonstrated model discrepancy, not proof
that either original instruction implementation is wrong. Research files remain
unchanged; this note reports the discrepancy for its owner.

The public soft-double representation supports finite normal binary64 patterns,
canonical zero and the reference NaN sentinel. An unrepresentable result fails
explicitly rather than pretending to support arbitrary Python Fraction ranges.
Gameplay intermediates formed from finite raw singles fit this range. Unwrapped
integer fields have explicit signed64 representation limits; overflow refuses
rather than invoking C++ undefined behavior.

## Records and callbacks

Typed records retain copied list values and borrowed record identities, matching
the model's shallow record-copy behavior. Callers must keep active pointees alive
and stable. World player records use stable deque storage so callback append does
not invalidate active records; erasing or reordering active records is outside
this lifetime contract. Negative list indices and short-list failures follow the
model where an operation actually reaches the entry.

Skill statistics, level adjustment, allocation, enemy checks and take-damage are
injected callbacks. The default take-damage callback logs its inputs; it does not
invent creature HP processing. Real skill data, melee-hit logic, regeneration,
undead-shield strikes and engine integration remain outside the reference.
Recursive damage stops with `Unsupported` at depth 128, preserving partial state
and restoring the depth counter after exceptions. Nonterminating level-up
callbacks likewise fail after 4,096 transitions. These safety limits are explicit
refusals, not successful gameplay or a console stack model.

`ItemArena` translates Latin1 resource-key bytes to Unicode codepoints and copies
count, flags and all four modifier slots. Distinct definitions sharing an address
remain distinct, while repeated conversions share stable definition identity.
The arena retains its sources; converted items borrow its definition pointers.
Keep it alive through use and call `refresh_definitions()` after changing source
fields. Item slots are snapshots; definition refresh updates aliases explicitly.
No fixed-Ps2 item calculation is substituted for selected-context combat math.
The bridge corpus changes its results across contexts for 236 of 512 authored
records, and verifies that conversion/statistics retain caller settings even
when a missing definition causes an error.

## Reproduce and evidence boundary

The ordinary native CMake build runs these tests without game files or a research
checkout. To regenerate fixtures, select the external research checkout at the
pin above. Statistics regeneration additionally requires Python with Unicode16
(normally Python3.14); other Unicode revisions fail explicitly. The scratch
Unicode output path below avoids overwriting the committed generated header:

```sh
python3 tools/generate_combat_common_vectors.py --research-dir /path/to/champions-re --output work/combat_common.tsv
python3 tools/generate_combat_damage_vectors.py --research-dir /path/to/champions-re --output work/combat_damage.tsv
python3 tools/generate_combat_stats_vectors.py --research-dir /path/to/champions-re --output work/combat_stats.tsv --unicode-output work/combat_unicode_data.hpp
python3 tools/generate_combat_items_vectors.py --research-dir /path/to/champions-re --output work/combat_items.tsv
```

Manifests record reference, generator and plain-vector hashes. Deterministic gzip
fixtures are public authored data. Native runners produce actual output/state
records; the wrapper derives expected traces independently from Python results
and removes old reports before each attempt. Missing, changed or truncated
fixtures fail. CI requires all thirteen core/item/combat/AMX suites on all five native
hosts, rejects equal-but-wrong traces, and records the exact tested source commit.

All four optimized suites and ASan/UBSan suites pass locally on Apple Silicon.
The integrated native build passes 27 CTest cases. Target-platform
execution and regression evidence is supplied by CI for the implementation commit.
This establishes pinned Python-model parity with synthetic inputs. Original-data
loading, console and Norrath fidelity, engine integration and original-network
compatibility require separate evidence. The [AMX port](SCRIPT_MACHINE.md) follows
this milestone; network-wire rules come next,
before the remaining reflection/shell/facing-point model coverage.
