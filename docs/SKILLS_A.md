# Return to Arms Skill foundation, weapon buffs and Undead Shield

PROGRESS: 49 of 196 reference functions ported

`champions-skills-a` is an independent portable C++17 **subset** of the skills
1–34 reference. It implements shared Skill arithmetic/dispatch, the Dodge and
Regeneration numeric helpers, and the complete Lightning/Cold/Poison Weapons
family (IDs 12/14/26): can-use, cast, spell constructors, opening effects, expiry,
weapon enchantment and player auras. It also implements the complete Undead Shield
and Secondary Undead Shield family (ID 15), including aura and damage reflection. Engine services and presentation are explicit
callbacks. Session, scene integration and other active abilities remain work.
It depends on `champions-core` and the existing `creature_hp::Memory` through
`champions-creature-hp`; no other native module was changed. The namespace is explicitly `simulation::rta::skills_a`.

## Evidence and pin

The read-only research checkout is `../../champions-return-to-arms-re`, commit
`356add27d50e50972b2a671b31e8f4ec93e6778c`. The authored translation follows:

- [ref_skills_a.py](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_skills_a.py), SHA-256 `6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae`.
- [skills-01-34.md](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/skills-01-34.md), SHA-256 `3e7ea2c519e301d5a70633d04763a9fec2c198ace5969f86b6ef12a9280f80aa`: shared machinery §0.1–0.2, Dodge §2, Regeneration §19, weapon buffs §12/14/26.
- [skills-01-34-executed.md](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/skills-01-34-executed.md), SHA-256 `ad12fac09365575437c123d2d0c78ef364a7358a97d37fe5895fdf865a4bd60c`.
- The associated [differential harness](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/difftest_skills_a.py), SHA-256 `d91f55a4d6ad75dc878e3fad279f865abb679cbc34d3d28b25aab29da8728b30`.

The vector manifest additionally pins every imported reference dependency
(`ref_core.py`, `ref_creature.py`, `ref_arena_monsters.py`, `ref_items.py`). The
generator verifies both the checkout files and their content at the recorded
commit before importing anything; Python bytecode writes are disabled. No research
file is edited or vendored. The original differential harness's reported execution
is research evidence; it was **not rerun** here. Local verification drives the
Python model with authored synthetic memory and callbacks, without game files,
original instruction bytes, original statistic tables or extracted content.
The MIT copyright is TrustlessMike and contributors (2026), as in this repository's
[LICENSE](../LICENSE).

## Implemented contract

| Entry | Behavior |
|---|---|
| `skill_by_id` | Read caller-supplied table at `0x41b170 + 4*id` |
| Base slots 1, 2, 5 | Can-use 1, cast 0, mana raw `1.0` |
| Base slot 9 | Initialize only five tier-1 entries, preserving tier 2 and all padding; first/base/step writes in reference order |
| Base slots 10, 11 | Signed16 first/base, raw32 step, level-zero exit, signed cap and negative tier-2 fallback, single precision rounding |
| Base slots 12, 13 | Player flag `0x40` selects tier 1 with cap ignored; other/null owners select tier 2; handle path reads node then object exactly |
| Base slot 14 | Preserve level |
| Base slot 15 | Dispatch slot 14, then slot 10 statistic 4, convert signed32 result to raw single |
| Base slot 8 | Plain or randomized range, independently rounded and clamped to at least 1; randomized case consumes four shared draws |
| `mana_cost_at` | Recognize the reference's three supplied-code shapes: immediate step/base; slot 15 call; slot 14 call followed by zero |
| `dodge_fraction` (id 2, alias 83) | Rank zero skips callbacks; otherwise slot 14 then slot 10, raw multiply by `0x3c23d70a` |
| `regeneration_stat0` (id 19) | Slot 10 statistic 0, preserving callback result |
| `passive_cast` (ids 9/19, alias 82) | Return zero; registered passive bodies dispatch identically to base cast |

## Ported family: Lightning, Cold and Poison Weapons (12/14/26)

All three reference entry-point wrappers and their Skill virtual bodies now dispatch
through `weaponbuff_can_use`, `weaponbuff_cast`, `weaponbuff_ctor` and
`weaponbuff_run`. Spell virtual run bodies dispatch through `spell_call` or
`table_call("Spell", ..., 0x14, ...)`. Signed16 virtual adjustments wrap adjusted
`this` only. Can-use compares caller-supplied getter identities using the attached
spell list, skips empty nodes, and returns the first exact match. Spell type lookup
intentionally calls `Spell.v0` with the original spell, without a table adjustment.

The family includes these supporting contracts in the existing module:

- Spell base writes, own RNG, type lookup, attached-list search and `add_spell`.
- Signed active-set selection and both equipped weapon reads.
- Four-slot timed enchantment: read the first matching **minimum damage**, then
  write the first empty-or-matching slot; thrown and null items skip. Existing
  minimum damage must be nonnegative, below the duration limit and no higher than
  the new value. High damage uses the pinned raw `0x3e4ccccc` multiplier and
  half-away rounding; all four fields store low16.
- Full-register first duration read followed by signed-byte level reads,
  Poison-only slot 14 adjustment, integer versus raw-float aura radii, original
  opening-effect deadlines, exact flash/light/sound arguments and expiry ordering.
- Lightning shock callback and all eight Cold particles, each with five ordered
  own draws. Shared and hidden draw counters retain reference semantics: the
  LightEffect callback's draw is counted as hidden only after it returns.
- Aura box bounds, current-frame modulo 30, owner/nonplayer/self filtering,
  ordered PvP same-team callbacks, both ally weapons, and the original minimum
  damage threshold of 31. Callback-returned order and duplicates are retained.

`named_callbacks` supplies allocation (`pool_new`), `LightEffect_ctor`, `res_find`,
`sound_playAtPos`, `particle_spawn`, `fx_shockModel`, `world_collectObjectsInBox`,
`Creature.v16/v22`, holder virtual calls, Spell deletion and `handle_link`.
Missing callbacks record their attempted call and throw `Unsupported`; no default
allocation, query, rendering or holder implementation is fabricated. Unknown Spell
bodies also require named callbacks. `external_named` can force a ported Spell
body to its callback. Unknown getter identities refuse unless `type_info` supplies
the getter result; unknown attached spell getters use `Spell.v0` explicitly.

Named callback arguments flatten original vector arguments to x/y/z lanes, left to
right, while preserving all scalar patterns, signed values as low32 and pointer
lanes. For example, a box query receives `{low.x, low.y, low.z, high.x, high.y,
high.z, capacity}` and returns object-address lanes in order. Particle spawn gets
`{definition, point.x, point.y, point.z, velocity.x, velocity.y, velocity.z, zero}`.
Scalar callbacks return exactly one lane; void callback results are ignored.
This is an explicit native representation of the reference tuples, not an ABI
claim. Owner/field rereads after callbacks and exception prefixes match the model.
Callbacks may replace themselves and reenter; the active callback is copied.
List traversal and box result counts have a 65,536-entry refusal bound, and named
calls share the existing history/depth limits. No original lifetime manager,
weapon timer consumer, melee damage integration, or imported Creature behavior is
silently supplied. Fire Weapons (13) has a different light lifecycle and remains
in the continuation list.

The callers supply all tables, fields and virtual table rows. Individual `initStats`
and constructors are not provided: synthetic test numbers exercise these routines
without pretending to supply actual per-class statistics. `skill_call(skill, 5,
{level})` is the `mana_cost` wrapper equivalent; `skill_call(skill, 1, {caster})`
is the `can_use_slot` equivalent only for a supported base body or explicit callback.
The last reference override's inferred constant/carries shapes for unknown slot-1
bodies are not implemented.

Virtual table signed16 adjustments wrap the adjusted `this` to raw32, as in the
reference. Memory address expressions otherwise must remain within raw32 spans;
invalid spans refuse through the existing Memory exception. Signed statistic
indices are not silently clamped to five entries: the reference can read adjacent
memory, and supplied adjacent memory is honored. Level zero skips those reads.
A handle node with a null object still reads flags at address `0xc`; this surprising
reference behavior is deliberately preserved. No pointer-validity or lifetime rule
is fabricated.

All numerical calculations use integer-pattern core arithmetic under the caller's
selected FPU configuration, including all sixteen modeled settings. Subtractions
passed to `from_int` preserve low32 conversion. No host float, libm or Godot enters
these calculations. The randomized range's first draw selects rounding direction,
its second selects the actual value, for each bound, even if the clamp hides it.

Unknown/unported Skill bodies require an explicit registered callback and otherwise
throw `Unsupported`. Forced external slots use the same callback path. Callback
arguments are adjusted `this` followed by raw32 arguments; the return is a `Values`
vector (one value for scalar bodies, two for a range, empty for void). Callback
mutation, self-replacement, reentrancy and exceptions are preserved. The callback
is copied before invocation. Slot 5 always uses the supplied mana shape reader
when not resolved as a base body, even when marked external, matching the pin;
unrecognized shapes or missing constants refuse. Callers must keep the Model alive
through synchronous calls. Callback return values outside raw32 and the Python
model's unbounded address space are outside the portable domain.

Limits are explicit refusals: 64 nested virtual calls, 65,536 modeled writes or
calls, and the existing Memory's 1 MiB sparse-byte and 65,536 store-history caps.
No rollback or retry is added; exceptions retain prior writes, calls and draws.

## Exact public tests

The generator now produces **36,028** complete-state cases across all sixteen FPU
settings: the original 15,310 foundation cases, 13,089 weapon-family cases, and
7,629 cases in the shield continuation. The manifest and verifier pin the entire
original corpus prefix (SHA-256
`3dd4d5fd40bde30ae99a6795cf543aa76db3efb7db2ec6b3f654676a18ec6f1c`),
so no existing vector can change during this extension.
The native trace runner receives **only inputs**, never expected results. The
Python test verifies manifest identity/hash/coverage and compares return/refusal,
all sparse memory bytes (including unchanged bytes), ordered model writes,
ordered memory store history, ordered callback names/arguments, shared draw count
and hidden draw count. Each case starts from independent authored state.

Coverage includes signed thresholds other than -1, signed16 bases, signed level
extremes, cap behavior, all five statistics, negative/outside-five indices, signed
virtual adjustments, player/null/empty/null-object handle paths, untouched init
padding, raw arithmetic extremes, supplied synthetic mana immediates/call shapes,
and callback mutation/exception prefixes. The API suite checks mandatory callback
refusal, active callback replacement, recursion-limit recovery, missing scalar
results and context preservation after errors.

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

The committed gzip is deterministic (`mtime=0`, no filename); its manifest records
plain-vector and generator hashes. Ordinary tests need neither a research checkout
nor game data. Reports live under `build/native/core-traces/`; the test removes
prior reports before attempting verification. Regeneration with enforced dependency
import paths produced byte-identical gzip vectors; only the generator hash changed.

## Weapon-family continuation execution evidence

The Release configure/build and both affected CTests (`skills-a-api`,
`skills-a-exact`) passed on Apple Silicon macOS. All **28,399** complete-state
cases passed, including **13,089** appended cases, and the original **15,310**
rows remain byte-for-byte identical. The corpus covers all sixteen FPU settings,
full-register versus signed-byte levels, player/nonplayer paths, null
and thrown weapons, signed active-set indices, matching/nonmatching/full slots,
minimum damage around 31, expiry/opening/modulo boundaries, PvP rejection,
self/empty/duplicate query results, signed virtual adjustments, callback mutation,
missing services, allocation failure and exception prefixes. Additional API checks
cover named callback replacement/reentrancy, empty scalar results, cyclic-list
refusal/recovery, getter identity and explicit external Spell dispatch.

The independently compiled x86_64 runner executed under Rosetta and the arm64
AddressSanitizer + UndefinedBehaviorSanitizer runner each passed the same corpus.
Both `cmp` comparisons with the arm64 Release trace passed. All three complete
trace hashes are `731ca9cfb3ccacbb01f2c42517f5af7d381cf75830d2b50cf73e95bb9e11873f`.
The extra compiler/vector commands below in the prior evidence section were rerun
with this expanded corpus; the sanitizer comparison command was additionally:

```sh
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
```

The full Release sweep `ctest --test-dir build/native --output-on-failure -j 3`
completed in **2,742.02 seconds** with **100/102 passing**. The two exceptions
were `session-combat-transport` and `session-creature-transport`, each reaching
its Python harness's 600-second subprocess timeout under concurrent worktree
load. Their corresponding exact-state cases passed, and neither transport case
reported a state mismatch. No timeout setting or unrelated module was changed.

The isolated combat transport rerun passed in **151.70 seconds**:

```sh
ctest --test-dir build/native -R '^session-combat-transport$' --output-on-failure -j 1
```

The isolated creature transport rerun passed in **85.32 seconds**:

```sh
ctest --test-dir build/native -R '^session-creature-transport$' --output-on-failure -j 1
```

Thus every one of the **102** cases passed either in the full sweep or its
required isolated rerun. The full sweep itself returned failure because of the
two timeouts; both subsequent commands returned success. The skills tests also
passed within the sweep (`skills-a-exact`: 11.17 seconds).

This continuation changed only the existing `native/include/champions/skills_a.hpp`,
`native/src/skills_a.cpp`, `native/tools/skills_a_trace.cpp`,
`native/tests/skills_a_tests.cpp`, `tools/generate_skills_a_vectors.py`,
`tools/test_skills_a_vectors.py`, `native/tests/vectors/skills_a.tsv.gz`,
`native/tests/vectors/skills_a.json`, and this page.

No private data was used. The research checkout was read-only throughout. This
verifies standalone contract execution on arm64 and translated x64; physical
Intel, Windows, console behavior, Norrath and playable-scene integration remain
unverified. Original holder bodies, allocation/lifetime, world collection, sound,
rendering, weapon timer ticks and melee consumers remain explicit service or
integration boundaries. Further families are portable from the pinned evidence.

## Ported family: Undead Shield and Secondary Undead Shield (15)

The continuation implements all ten family methods: `undeadshield_can_use`,
`undeadshield_cast`, `undeadshield_ctor`, `_shield_glow` (native `shield_glow`),
`undeadshield_run`, `undeadshield_damage_attacker`, `_shield_reflect` (native
`shield_reflect`), `secondary_shield_ctor`, `secondary_shield_run`, and
`secondary_shield_damage_attacker`. Shared `spell_ball`, `spell_delete`, and
`flash_both` are now explicit native methods. Skill can-use/cast and both Spell
run bodies dispatch from the pinned virtual identities with signed16 adjustments.
No original per-class statistics or scene dispatch is supplied.

The source is the same pinned Python contract and spec §15 cited above. The port
preserves light record write order, callback-visible owner/field rereads, signed-byte
levels, signed frame comparisons, the floating-point secondary expiry calculation,
fade-in arithmetic, sine tint, all twelve matrix lanes, four sample particles,
unit-ball rejection and own RNG order. Reflection flashes both quads before its
percentage read, truncates before converting back to raw float, consumes one own
draw, and sends the original empty descriptor and scalar arguments to an explicit
`damage_apply` callback. It does not implement damage application itself.

Aura queries preserve callback-returned order and duplicates, self/player filtering,
PvP team calls, both shield-type lookups without short circuit, failed allocations,
and shared draws for each successful secondary constructor. Primary and secondary
run expiry calls the explicit Spell deletion virtual; no lifetime manager or
original owner spell scheduling is invented.

New named services are `light_link/relink/unlink`, `particleGroup_create/setMatrix/
begin/end`, `model_getSamplePointCount`, `model_getSamplePoint`, `sound_release`,
and `damage_apply`. Vectors and matrices flatten into raw32 lanes as before.
Sample points must return exactly three lanes. The empty damage descriptor is
represented by four raw32 `0xffffffff` lanes for its absent effect slots; its
implicit zero statistic halfwords and `0xff` bytes follow the reference descriptor
contract. This representation is a callback contract, not a retail memory layout.
General populated descriptors remain work. No missing service defaults to zero.
Ball rejection has a 65,536-attempt refusal cap in addition to existing write caps.

### Function accounting

The generator counts the pinned file with Python AST: **10 top-level functions +
186 directly declared class methods = 196 units**. It excludes only `shape_result`,
the differential callback/harness result-shaping helper. Item adapter classes
`_ImageExe` and `_SharedDraws` remain included because runtime item parsing uses
those methods. Both separately declared `skill_call` implementations count in the
denominator. Imported inherited functions do not count as declarations in this file.

The vector manifest lists the **49** fully ported units: 16 shared statistics,
numeric/passive entry points; 11 Spell/weapon support and generic weapon-family
methods; nine Lightning/Cold/Poison wrappers; ten shield-family methods; and three
shared shield helpers. Tests exercise wrapper entry points directly as well as
virtual routes. Partial dispatch (`skill_call`, `table_call`), the generic callback
adapter, imported RNG methods, general descriptor construction, and numeric
helpers embedded as expressions are conservatively not counted as full units.
Explicit refusals are not counted. This inventory deliberately separates native
support from completeness of the entire pinned Python declaration.

### This continuation's execution evidence

The Release configure/build and affected API/exact CTests passed. All **36,028**
complete-state cases passed on arm64 and the independent x86_64 runner under
Rosetta. Their full traces compare byte-identically, SHA-256
`68fce65f7ae39acae65a0dc4b8dd12f3f0a12b0aaa0c95a07351224ac7e7cfd3`.
The generator manifest hash matches the final source. Initial fixture callback
shape errors, a raw light-constant typo, and vector store-width differences were
corrected before this passing run. Vector copies preserve the reference eight-byte
store followed by a four-byte store, including both history widths.
The arm64 AddressSanitizer + UndefinedBehaviorSanitizer runner also passed all
36,028 cases, and `cmp` found its trace identical to Release and translated x64.
The compiler and vector-check commands in the prior foundation evidence section
were rerun against this corpus; both `cmp` commands in this page passed. The full
Release sweep passed **102/102 tests** in **1,871.79 seconds** with no timeout
rerun required. Its skills exact case passed in 42.61 seconds; the separately run
affected suite passed 2/2 in 20.31 seconds. The full log is under ignored
`build/native/skills-a-full-ctest.log`.

Final successful primary commands were:

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
```

This continuation changed only the existing header/source, native trace runner and
API tests, vector generator/verifier, gzip corpus/manifest, and this page (nine
files). The research checkout remained clean. This verifies authored synthetic
contract execution on arm64 and translated x64, with sanitizer coverage. Physical
Intel, Windows, console behavior, Norrath, engine services, original player spell
ordering, and playable-scene integration remain unverified. Further families in
the continuation list remain portable from the existing pin.

All previous 28,399 rows are pinned as a complete
byte-identical prefix in addition to the original foundation prefix. New tests
compare inputs, return/refusal, every sparse memory byte, both write logs, callback
order and arguments, and shared/hidden draw counts. The research tree is never
written, and no private inputs or game content are used.

## Prior foundation execution evidence

On Apple Silicon macOS, the final Release build, API suite and all 15,310 exact cases passed.
Two initial link attempts used a stale object missing newly added helpers; forcing
recompilation resolved them. Manual integrity checks also rejected a changed fixture
and expected-result columns in native input, and confirmed stale report removal.
Four additional input-driven handle contrast cases matched the reference complete
state: empty node, null object with low-memory player flag set/clear, and live
player, using distinct authored tier values (17 versus 100).
An independently compiled x86_64 runner executed under Rosetta also passed;
`cmp` found identical complete traces. An arm64 AddressSanitizer +
UndefinedBehaviorSanitizer runner passed the same corpus. All three trace hashes
are `ae7f53398708cc62320d24d223b91f5aa8aaa0e4a6655883af6a1153445f4756`.
The final full Release command `ctest --test-dir build/native --output-on-failure -j 3`
passed **102/102** tests (1,062.41 seconds on the shared, heavily loaded host); no
timeout rerun was needed.
This is arm64 execution plus translated x64 execution, not physical Intel or
Windows verification.

The two extra runners were built with these commands (output directories created
under ignored `build/` first):

```sh
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
```

## Exactly what remains

All per-id Skill constructors and overridden statistics initializers remain outside this subset.
The following original behavior is still unsupported even when its numeric reads
can use this foundation:

| IDs | Remaining behavior |
|---|---|
| 1 Wizard Beam | Cast, spell constructor/run, beams, damage and trail presentation |
| 2 Dodge | Skill initialization and missile-consumer dodge/debris behavior; only fraction helper is ported |
| 3 Resist Poison | Per-skill constructor and consumer integration (shared base initializer is covered) |
| 4 Disease Bolt | Can-use, cast, projectile constructor/run/hit |
| 5 Disease Shield | Can-use, cast, spell constructor/run, damage and presentation |
| 6 Disease Trail | Can-use, cast, spell constructor/run, segment constructor/run |
| 7 Fire Storm | Can-use, cast, both constructors, common initialization/run |
| 8 Fire Flares | Can-use, cast, spell constructor/destructor/run, flare constructor/run |
| 9 Riposte | Counterattack/damage and particles; only passive cast is ported |
| 10 Blessed | Can-use, cast, spell constructor/destructor/run, spell statistic helper |
| 11 Summon Skeleton | Imported summon can-use/cast/spell constructor/run and skeleton callback bridge |
| 12/14/26 Lightning/Cold/Poison Weapons | Skill constructors/statistics initialization and engine/consumer integration; full reference ability lifecycle is ported |
| 13 Fire Weapons | Can-use/cast, spell constructor/run, weapon enchantment and aura |
| 15 Undead Shield | Engine/consumer integration and original player spell ordering; all ten reference family methods are ported |
| 16 Holy Strike | Can-use/cast, projectile constructor/run/hit |
| 17 Hammer of Wrath | Can-use/cast, object/spell constructors/runs, pulling/blows, item parsing/resource adapter, third RNG |
| 18 Cone of Fire | Can-use/cast, spell constructor/run, damage descriptor |
| 19 Regeneration | Skill initialization and Player mana-regeneration consumer; only statistic helper/passive cast are ported |
| 20 Blinding Light | Can-use/cast, effect/spell constructors/runs |
| 21 Repulse Undead | Cast and radial-damage helper |
| 22 Dismiss Undead | Cast and HolyBolt constructor/run/hit |
| 23 Shock of Frost | Can-use/cast, projectile and shard constructors/runs/hits, debris |
| 24 Poison Rain | Can-use/cast, object constructor/run |
| 25/27/28/30 Disease Weapons/Hate Beam/Ball of Hate/Hate Effect | Per-class tables/initialization and consumer integration |
| 29 Disease Effect | Spell constructor/run, contagion |
| 31/32 Root | Can-use/cast and spell constructor/run (also aliases 52/85) |
| 33 Critical Hit | Can-use/cast and spell constructor/run, tint/bonus/aura (also aliases 53/81) |
| 34 Shield Bash | Can-use/cast and spell constructor/windup/run/done, animation and movement calls |

Shared machinery still to port: the imported owner per-tick spell walk (the
standalone base/own RNG/type lookup/attachment search and link callback are covered),
GameObject/Projectile constructors/own RNG/collision, object virtual body dispatch
and other spell bodies, generic RTTI/carries instruction-shape recognition,
object unit-ball rejection and puff helpers (spell-ball rejection and both flash quads are covered), other light/flash helpers, other query and
player-list adapters, damage descriptors, and their ordered external calls.
The weapon family already covers its light creation, second flash quad and box
query argument construction. Imported Creature and arena behavior is not implicitly supplied by this
module's Memory dependency.

The reference itself leaves all draw functions and `describeRank` outside the model,
Wizard Beam trail point arrays unmodeled, and Hammer states 1/2 unsupported. These
must remain explicit callback/refusal boundaries in any future extension. Real
object lifetime and owner/spell run order, damage application, player enemy checks,
terrain/world queries and actual console divide/FPU behavior are separate evidence
boundaries. Nothing here establishes console, Norrath, Windows, native Intel,
Session or playable-client compatibility. Existing evidence supports further portable
families, so this continuation is **STATUS: MORE**.
