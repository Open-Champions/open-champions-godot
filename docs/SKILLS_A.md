# Return to Arms Skills: foundation, weapon buffs, shields, support, Shield Bash, Fire Storm, Disease Trail, Frost, Fire Flares, homing projectiles, Blinding Light and disease status

Selected continuation family: **Disease Shield (5) and Disease Effect (29): complete swarm, contagion and periodic damage lifecycles**.

PROGRESS: 127 of 196 reference functions ported

`champions-skills-a` is an independent portable C++17 **subset** of the skills
1–34 reference. It implements shared Skill arithmetic/dispatch, the Dodge and
Regeneration numeric helpers, and the complete Lightning/Cold/Poison Weapons
family (IDs 12/14/26): can-use, cast, spell constructors, opening effects, expiry,
weapon enchantment and player auras. It also implements the complete Undead Shield
and Secondary Undead Shield family (ID 15), including aura and damage reflection. Root and Critical Hit player support spells
(IDs 31/32/33 and aliases 52/53/81/85) now include their full modeled lifecycles
and the Critical Hit bonus lookup. Shield Bash (ID 34) now includes its complete
modeled can-use/cast, constructor, wind-up, impact/run and done polling. Fire Storm (ID 7) now adds the complete modeled area-attack family, including both
constructors, warning delay, light animation, flare calls and radial box damage.
Disease Trail (ID 6) now adds the full modeled moving-caster spell and one-shot
segment trap lifecycle, including both constructors and ordered particle emission.
Shock of Frost and Frost Shard (ID 23) now add their complete modeled projectile
lifecycles, ordered collisions, cold damage, debris and shard spawning.
Fire Flares (ID 8) now adds the full caster orbit and flare hit/burst lifecycle,
including spell cleanup.
Disease Bolt (ID 4) and Holy Strike (ID 16) now add their full modeled homing
projectile lifecycles, target selection, hit damage and expiry effects.
Blinding Light (ID 20) adds the complete six-method area effect and victim status
family, including first-entry targeting, boss chance adjustment and periodic damage.
Disease Shield (ID 5) and Disease Effect (ID 29) now add the complete modeled
swarm, radius growth, contagion and periodic damage family.
Engine services and presentation are explicit
callbacks. Session, scene integration and other active abilities remain work.
It depends on `champions-core` and the existing `creature_hp::Memory` through
`champions-creature-hp`; no other native module was changed. The namespace is explicitly `simulation::rta::skills_a`.

## Evidence and pin

The read-only research checkout is `../../champions-return-to-arms-re`; contract
evidence is pinned to commit `356add27d50e50972b2a671b31e8f4ec93e6778c`.
The authored translation follows:

- [ref_skills_a.py](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_skills_a.py), SHA-256 `6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae`.
- [skills-01-34.md](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/skills-01-34.md), SHA-256 `3e7ea2c519e301d5a70633d04763a9fec2c198ace5969f86b6ef12a9280f80aa`: shared machinery §0.1–0.2, Dodge §2, Regeneration §19, weapon buffs §12/14/26, Root §31/32 and Critical Hit §33; projectile step §0.6 and Shock of Frost §23.
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

The shield continuation produced **36,028** complete-state cases across all sixteen FPU
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

The shield continuation counted **49** fully ported units: 16 shared statistics,
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

## Ported family: Root and Critical Hit player support (31/32/33)

This continuation implements the four Root methods (`root_can_use`, `root_cast`,
`root_spell_ctor`, `root_spell_run`), six Critical Hit methods (`crit_can_use`,
`crit_cast`, `crit_spell_ctor`, `crit_set_tint`, `crit_spell_run`, `crit_bonus_for`),
and the shared `player_object` record lookup. The same classes cover Root aliases
52/85 and Critical Hit aliases 53/81. The source remains the pinned Python contract,
spec §31/32 and §33, with authored synthetic statistics and fixtures.

Root preserves PvP HUD refusal, adjusted cast rank, ordered arc targets, enemy/boss/
flag/timer filters, failed-allocation target accounting, and both stack-list clears.
The spell preserves phase-specific particle counts, delayed shared resistance draw,
strict success threshold, low16 monster timer, player knockback arithmetic and angle,
expiry, and own rejection draws even for the unused phase-1 balls. The modeled signed
remainder returns the draw for zero sample count; it does not skip particles.

Critical Hit preserves the light record's ordered writes, duration and tint,
light relink/unlink/fade branches, alias-specific ring counts, cosmetic ghost
placement, terrain height cutoffs, and living-player tint aura. The bonus lookup
preserves ordered alive slots, strict 3-D and planar ranges, same-team virtual
call direction, carries checks, rank calls in order 53/81/33, signed best ranks,
wrapped radius arithmetic, first qualifying candidate termination, and the final
slot-14 adjustment even for zero level. Alive-list counts are captured once per
walk, as in Python `range`; callbacks may mutate subsequent slots and record reads.
No callback result is silently synthesized.

New named services are `text_lookup`, `hud_showMessage`, `sound_playOnObject`,
`world_collectObjectsInArc`, `Creature.v20`, `creature_bossRank`, `handle_clear`,
`net_objOrGhost`, `terrain_heightAtPoint`, and `player_getSkillRank`. Arc query
arguments flatten to `{x,y,z,signed16_facing,STACK_QUERY,filter,radius,angle,zero}`;
`STACK_QUERY = 0xffffffff` denotes the caller's temporary list, not a retail pointer.
Both clear calls receive that token. Arc callbacks return object lanes in supplied
order, retaining duplicates. The Root sample callback returns just the three
point lanes; the reference's second output is unused and omitted. The vector
script encodes the Python `(point, second)` result explicitly without changing
existing shield sample callbacks. Missing services and malformed native scalar/
point results refuse while retaining preceding state. Arc and alive traversals
have a 65,536-entry refusal bound. Original model sampling, world queries,
lifetimes, damage consumers, HUD, sound and rendering remain explicit services.

The AST inventory still contains 196 units (10 top-level functions and 186 class
methods, excluding only `shape_result`). The manifest now lists 60 fully ported
and tested units: the prior 49 plus these 11. Partial dispatch and refusals remain
uncounted. The reference pin and every prior vector remain unchanged. The continuation adds
8,840 complete-state cases for a total of 44,868 across all sixteen FPU settings.
The generator and verifier pin the full 36,028-case prefix with SHA-256
`94170ab12e4094409aceb36fae8aa244bf34ddc892efd29e925479f57c11ff3a`.
New checks compare returns/refusals, sparse bytes, both ordered write logs, callbacks
and shared/hidden draw counts. API checks cover failed-allocation target limits,
stack cleanup, nonplayer timer truncation, missing/malformed callbacks, query limits
and recovery, zero sample counts and Critical Hit class identity.

### This continuation's execution evidence

Release configure/build and the affected CTests passed (2/2, 93.15 seconds;
`skills-a-exact` took 92.69 seconds). All 44,868 complete states passed on arm64,
the independently compiled x86_64 runner under Rosetta, and the arm64
AddressSanitizer + UndefinedBehaviorSanitizer runner. Both `cmp` comparisons passed.
All three full trace hashes are
`050efbc45ea29298820ad07990d8cf79fccf6c927bb651e0d27b512d7b5a98e3`.
The vector hash is
`ebaf48a42ea47ed6f037e41c5bb0afb988db2b0f0c1a02c0726c1ea59745a65c`.
Initial generator constant lookup and stack-token encoding errors were corrected
before the passing generation and exact run. No native mismatches occurred.

The full Release sweep passed **102/102 tests** in **1,467.34 seconds**, with no
timeout reruns required. Its Skills exact case passed in 30.38 seconds and API
case in 0.07 seconds. The final rebuild includes the explicit standard algorithm
header; the full sweep tested those final arm64 binaries.

The primary successful commands were:

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
```

The two direct compiler commands and their corresponding vector verifier commands
in the prior foundation evidence section were rerun successfully for this corpus.
The full CTest log is under ignored `build/native/skills-a-full-ctest.log`.
This continuation changes only the existing header/source, native trace runner/API
tests, generator/verifier, gzip corpus/manifest and this docs page (nine files).
The research checkout remained clean; no private data was used. This establishes
standalone synthetic contract execution on arm64 and translated x64 with sanitizer
coverage. Physical Intel, Windows, console behavior, Norrath, original engine
services, Session and playable-scene integration remain unverified.

## Ported family: Shield Bash (34)

All six pinned family methods are implemented: `shieldbash_can_use`,
`shieldbash_cast`, `shieldbash_ctor`, `shieldbash_windup_fx`, `shieldbash_run`,
and `shieldbash_done`. The shared `light_effect` helper is also fully implemented
and directly tested. Skill and Spell virtual dispatch preserves signed16 adjusted
`this`; forced external calls remain callbacks. Evidence is the same pinned
`ref_skills_a.py` (lines 3520–3677), spec §34 and its executed notes.

The constructor preserves its non-base write order, shared draw, owner action-state
write, shared animation-header event writes, and the list-head target selection
when any entry qualifies. Wind-up produces exactly 203 particles using own draws,
signed remainder (including zero divisor), unit-ball rejection and sample points
from the callback-selected ghost. Run preserves animation then movement then turning,
event/done gating, signed-byte rank thresholds, signed quotas and chances, raw
float damage bounds, killed-target draw skipping, ordered tint halfwords, first-hit
light creation, and sphere results prepended to the previous arc results. Cleanup,
delete and local ghost-rebase stores occur in original order. Callback mutations
remain visible to subsequent reads; exceptions retain the exact preceding state.

New services are `anim_startBlendInOut`, `Player_animStep`, `Player_move`,
`Creature.v26/v30`, `model_getAttachPoint`, `world_collectInSphere`, and
`net_isLocalSlot`. They use the existing flattened raw32 lane convention and
`STACK_QUERY` token. Attach and sample points require three lanes; the reference's
unused second sample output is omitted. Damage supplies null descriptor/auxiliary
fields as zero lanes, rather than an invented damage implementation. A sphere
callback returns its newly collected entries; the native method prepends these to
the stored arc entries, preserving duplicates. Query lists and combined fallback
walks retain the 65,536-entry refusal bound. Original animation/player movement,
damage, world queries, prediction, allocation and presentation remain callbacks.

Accounting remains 196 portable declarations (10 top-level functions plus 186
class methods, excluding only harness helper `shape_result`). This continuation
adds seven fully ported and tested units, reaching 67. Partial generic dispatch,
imported behavior and explicit refusals remain uncounted. The complete prior
44,868-case corpus is pinned byte-for-byte with SHA-256
`ebaf48a42ea47ed6f037e41c5bb0afb988db2b0f0c1a02c0726c1ea59745a65c`.

### This continuation's execution evidence

The Release build and affected CTests passed (2/2, 20.14 seconds). All **49,136**
complete-state cases passed on arm64, the independently compiled x86_64 runner
under Rosetta, and the arm64 AddressSanitizer + UndefinedBehaviorSanitizer runner.
Both full-trace `cmp` comparisons passed. The continuation adds **4,268** cases
across all sixteen FPU settings. API checks cover missing services, constructor
failure prefixes, shared clip writes, oversized query refusal/recovery, list-head
selection, malformed sample output, light exception counters, adjusted completion
polling and forced external dispatch. The complete trace hash on all three runners
is `4251126e7c07fb7b856d9ccef7a5bde2e9013e42a59130bf6f7f99016c2b2889`;
the vector hash is
`e01b19da02da2b8c9aa14908ef03c7daa7475538f49f5ccd5516bde61fb9e13d`.

Two initial vector-generation attempts exposed invalid harness argument/point
shapes; these were corrected. The first exact comparison then exposed three trace
script mismatches: scalar zero query returns must become empty lists, whereas a
list containing zero must remain nonempty. That script encoding was corrected;
no modeled Shield Bash mismatch remained in the passing runs. Earlier failed
attempts do not count as passing checks.

Commands executed successfully:

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
git diff --check
```

The full Release sweep `ctest --test-dir build/native --output-on-failure -j 3`
passed **102/102 tests** in **655.13 seconds**, without timeout reruns. The Skills
exact case passed in 14.39 seconds and API case in 0.01 seconds. The full log is
under ignored `build/native/skills-a-full-ctest.log`.

Changes are limited to the existing header/source, native trace runner/API tests,
generator/verifier, gzip corpus/manifest, and this page (nine files). No shared build
configuration or sibling worktree was changed. The research checkout remained
clean and read-only, and no private game data was used. Physical Intel, Windows,
console behavior, Norrath, engine-service implementation, Session and playable
scene integration remain unverified. More families are portable from this pin.

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
| 4 Disease Bolt | Engine/scene integration and original per-class initialization/draw; all five reference family methods are ported |
| 5 Disease Shield | Engine/scene integration, original per-class initialization and presentation; all four reference family methods are ported |
| 6 Disease Trail | Engine/scene integration and original per-class initialization; all seven reference family methods are ported |
| 7 Fire Storm | Engine/scene integration, original per-class initialization and external flare body; all seven reference family methods are ported |
| 8 Fire Flares | Engine/scene integration, original per-class initialization and draw; all seven reference family methods are ported |
| 9 Riposte | Counterattack/damage and particles; only passive cast is ported |
| 10 Blessed | Can-use, cast, spell constructor/destructor/run, spell statistic helper |
| 11 Summon Skeleton | Imported summon can-use/cast/spell constructor/run and skeleton callback bridge |
| 12/14/26 Lightning/Cold/Poison Weapons | Skill constructors/statistics initialization and engine/consumer integration; full reference ability lifecycle is ported |
| 13 Fire Weapons | Can-use/cast, spell constructor/run, weapon enchantment and aura |
| 15 Undead Shield | Engine/consumer integration and original player spell ordering; all ten reference family methods are ported |
| 16 Holy Strike | Engine/scene integration, owned target-list/trail services and original per-class initialization/draw; all five reference family methods are ported |
| 17 Hammer of Wrath | Can-use/cast, object/spell constructors/runs, pulling/blows, item parsing/resource adapter, third RNG |
| 18 Cone of Fire | Can-use/cast, spell constructor/run, damage descriptor |
| 19 Regeneration | Skill initialization and Player mana-regeneration consumer; only statistic helper/passive cast are ported |
| 20 Blinding Light | Engine/scene integration, raw caster lifetime, original per-class initialization and presentation; all six reference family methods are ported |
| 21 Repulse Undead | Cast and radial-damage helper |
| 22 Dismiss Undead | Cast and HolyBolt constructor/run/hit |
| 23 Shock of Frost | Engine/scene integration and original per-class initialization; all ten reference family methods are ported |
| 24 Poison Rain | Can-use/cast, object constructor/run |
| 25/27/28/30 Disease Weapons/Hate Beam/Ball of Hate/Hate Effect | Per-class tables/initialization and consumer integration |
| 29 Disease Effect | Damage consumer/scene integration and original per-class initialization; both reference family methods are ported |
| 31/32 Root | Engine/consumer integration and original owner scheduling; all four reference family methods are ported (also aliases 52/85) |
| 33 Critical Hit | Engine/consumer integration and original damage dispatch; all six reference family methods are ported (also aliases 53/81) |
| 34 Shield Bash | Original animation/movement services and scene integration; all six reference family methods are ported |

Shared machinery still to port: the imported owner per-tick spell walk (the
standalone base/own RNG/type lookup/attachment search and link callback are covered),
object virtual bodies beyond Fire Storm, Disease Trail, Frost, Fire Flares and homing projectiles
and other spell bodies, generic RTTI/carries instruction-shape recognition,
other particle helpers (generic `puff`, object/spell-ball rejection, object own RNG, both flash quads,
Disease Trail particle loop and cube query adapter are covered), other light/flash helpers, other query adapters (the alive-player
record lookup is covered), general damage descriptor construction, and their
ordered external calls. The no-property GameObject constructor is implemented up
to the same documented hidden/property refusals; it is not counted as fully ported.
The Projectile constructor/collision step and Fire Storm, Disease Trail, Frost,
Fire Flares and homing projectile object virtual dispatch are covered, with other
object bodies requiring explicit callbacks.
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


## Ported family: Fire Storm (7)

Authored from the pinned `ref_skills_a.py` and spec §7 above; the research checkout
remained read-only. All seven declared family methods are implemented:
`firestorm_can_use`, `firestorm_cast`, `_firestorm_base` (native `firestorm_base`),
`firestorm_ctor`, `firestorm_ctor2`, `firestorm_init_common`, and `firestorm_run`.
Shared `obj_ball` and `delete_self` are also fully ported, taking the tested
function count from 67 to 76. The denominator is unchanged: the generator checks
10 top-level functions and 186 class methods in the pinned AST, excluding only
`shape_result`. Imported `obj_rand` does not add a unit to that denominator.
The no-property `gameobject_ctor` preserves the reference's explicit hidden-object
and property-list refusals and is deliberately excluded from the ported count.
General `descriptor`, generic `vcall`/`table_call`, and imported lifetime/scheduling
bodies are also excluded; only Fire Storm's supported object body dispatch is added.

The port preserves caller-supplied stats, signed owner/level/facing behavior,
callback-visible rereads, ordered light writes, monster warning delay, grid linking,
class resource generation, single-precision flicker/fade and flare normalization.
Own draws include every rejected ball sample, even unused z components. Shared
constructor draws and hidden warning/flare draws retain their original order;
a failed external constructor does not increment the hidden count. Frame/period
remainder uses the reference's reported divide-by-zero behavior; console hardware
confirmation remains absent. Queries retain callback order and duplicates.

New named services are `terrain_height`, `obj_getPropInt`, `obj_runEveryFrame`,
`FireStormFlare_ctor` and object deletion through `Creature.v1`. Existing handle,
light, sound, enemy-query and damage callbacks remain explicit. No callback silently
returns zero. Fire Storm's object virtual body honors signed16 `this` adjustment;
`external_named` can force it to `Creature.v4`. Unported object bodies stay external.

For `damage_apply`, populated descriptor slots flatten to four unsigned low16 lanes
(type, minimum, maximum, auxiliary); an empty slot is a single `0xffffffff` lane.
Fire Storm fills slot zero and leaves three empty slots, so its callback has sixteen
lanes. The fixed eight zero stat halfwords and two `0xff` bytes remain implicit,
as with existing empty descriptors. This is an authored callback representation,
not a claim about original binary ABI. Sphere samples and query/resource walks
have explicit 65,536-entry bounds, in addition to existing memory/history limits.

All prior 49,136 complete-state vectors are preserved byte-for-byte, with the
whole prefix SHA-256 `e01b19da02da2b8c9aa14908ef03c7daa7475538f49f5ccd5516bde61fb9e13d`
enforced in both generator and verifier. The appended corpus covers all sixteen
FPU configurations, both constructors/variants, player/monster/null owners,
expiry/start/fade/flicker boundaries, zero/negative flare periods, signed damage
and statistic limits, own RNG seeds, duplicate/empty query lists, allocation
failure, callback mutations/exceptions/missing services and virtual adjustments.
API tests additionally verify external dispatch, oversized-query refusal/recovery,
null deletion and hidden draw behavior after callback failure.

### Execution evidence

Release configure/build and both affected CTests passed (2/2, 12.35 seconds).
All **56,226** complete-state cases matched the pinned Python contract, including
**7,090** appended cases. The arm64 complete trace hash is
`29d5bf1c8ffc24ce0264970ee7c8eee55ae490117537a3d22a00c3cb1bd3506f`;
the vector hash is
`edd6413ae031ec3d5e7f40f1a094ed139e253b44ab3f5bc994962efb81ac2fd0`.
The first generator attempt failed on a missing local `ZERO` name; that harness
issue was corrected before regeneration and exact validation. It does not count
as a passing check.

Commands run for this continuation (cross-architecture, sanitizer and full-suite
results follow below):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3 > build/native/firestorm-full-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
```

Changed files: existing header/source, native trace runner/API tests, vector
generator/verifier, gzip corpus/manifest, and this page (nine files). No shared
build configuration or sibling worktree was changed. No private game inputs
were used. Scene/Session wiring, physical Intel and Windows execution, Norrath,
original lifetime management and console behavior remain unverified. Further
families in the continuation table remain portable from the existing pin.


The independently compiled x86_64 runner under Rosetta and the arm64
AddressSanitizer + UndefinedBehaviorSanitizer runner each passed all 56,226 cases.
Both full-trace comparisons with arm64 Release passed, with the same trace hash
shown above. No sanitizer finding was reported.

```sh
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
git diff --check
```


The former suggested next family, Shock of Frost and Frost Shard (ID 23), is
now covered by the continuation below. Disease Bolt (4) or Holy Strike (16) can
reuse the projectile/collision foundation; imported Creature services remain callbacks.


The final Release sweep passed **102/102 tests** in **350.17 seconds**; no timeout
reruns were needed. Skills exact passed again in 14.12 seconds and API in 0.01
seconds. Full output is under ignored `build/native/firestorm-full-ctest.log`.
All validation commands above returned success after the initial corrected
vector-generator failure. A transient shared-disk exhaustion was resolved by
removing only this task's scratch and obsolete Skills traces; it did not change
fixtures or the passing results. Ordinary Git status/diff commands stalled on an
iCloud-evicted shared `.git/info/exclude`; the local commit outcome is recorded
in the final report.

## Ported family: Disease Trail (6)

Authored from the pinned `ref_skills_a.py` and spec §6, using the existing module,
vector generator, native runner, verifier and API suite. The research checkout was
read-only; no private inputs were used. The seven family methods are
`diseasetrail_can_use`, `diseasetrail_cast`, `diseasetrail_spell_ctor`,
`diseasetrail_spell_run`, `diseasetrail_ctor`, `diseasetrail_run` and `_trail_puffs`
(native `trail_puffs`). Shared `flash2` and `box_around` also have complete behavior
and tests. These nine units bring the count to 85. The pinned AST still has 10
top-level functions (excluding harness-only `shape_result`) and 186 class methods.
Existing partial dispatch machinery and the explicitly incomplete GameObject
constructor remain excluded from the ported count.

The spell preserves signed-byte level rereads, its float-valued expiry (including
frame-to-float and float-to-integer rounding), both strict segment-drop thresholds,
allocation failure behavior and last-position updates. The segment preserves
caller-supplied tier-1 statistics, wrapping expiry arithmetic, living-creature flags,
ordered enemy callbacks, query order and duplicates, independent own RNG draws,
low16 descriptor lanes, damage calls, sound and self-deletion. The particle loop
accepts explicit draw/ball callbacks and preserves rejection draws and the 1/1/2
particle ordering. Callback-visible rereads and failures retain all prior writes,
calls and draws. A new `obj_updateGridCell` callback joins the already explicit
allocation, handles, per-frame registration, queries, particles, damage, sound and
virtual deletion services. Missing services refuse; no engine implementation is
invented. Caller-supplied getter identity is still required for can-use lookup.
Skill, Spell and object virtual dispatch preserve signed16 this adjustments;
forced external dispatch remains supported.

All prior **56,226** vectors remain byte-for-byte unchanged; generator and verifier
independently pin that full prefix to SHA-256
`edd6413ae031ec3d5e7f40f1a094ed139e253b44ab3f5bc994962efb81ac2fd0`.
The **8,206** appended cases span all sixteen FPU settings, null/live owners,
signed levels/frames/statistics, float expiry and distance boundaries, empty and
duplicate queries, enemy rejection, own seeds, allocation failures, attached-spell
lookup, virtual adjustments, callback mutation, exceptions and missing callbacks.
API tests cover generic draw/ball order, callback replacement and failure prefixes,
query refusal/recovery, duplicate retention and forced external dispatch.

### Execution evidence

Release configure/build and affected CTests passed: **2/2**, all **64,432** exact
complete states. The arm64 trace hash is
`c5249e196112951622cf94beeb04af0c9f0eee0609c0b98333641755631879b5`;
the vector hash is
`1d3bc3b81f8355d66b4c424e39b3997b49c5f0e711862d5cbf3912720fe8af07`.
Commands (cross-architecture, sanitizer and full-suite outcomes recorded below):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3 > build/native/diseasetrail-full-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
git diff --check
```

Changes are confined to the nine existing Skills files: header, source, trace
runner, API tests, generator, verifier, gzip corpus, manifest and this page.
Shared build files and sibling worktrees were not changed. Physical Intel,
Windows, console behavior, Norrath and scene/Session integration remain unverified.
Imported lifetime, scheduling, world-query and damage services remain callbacks.
The remaining-family table above is the continuation list; Shock of Frost and
Frost Shard (ID 23) have since been covered by the continuation below. More is portable from this pin.

Both the x86_64 runner under Rosetta and the arm64 AddressSanitizer +
UndefinedBehaviorSanitizer runner passed all **64,432** complete-state cases.
Both `cmp` comparisons with the arm64 Release trace passed, with the identical
trace hash above. No sanitizer finding was reported. This is translated x64
execution, not physical Intel or Windows validation.

The final full Release sweep passed **102/102 tests** in **258.11 seconds**,
without timeout reruns. Skills exact passed again in 10.30 seconds and API in
0.01 seconds. Full output is under ignored
`build/native/diseasetrail-full-ctest.log`. All validation commands above passed.
Ordinary Git status/cached-diff commands stalled on iCloud-evicted shared Git
metadata; their read-only processes were stopped. Before staging, `git write-tree`
returned the exact HEAD tree, confirming the index had no pre-existing staged
changes. Local commit preparation uses explicit owned paths with
`git update-index --add`, `git write-tree`, `git commit-tree` and a compare-and-swap
`git update-ref refs/heads/codex/port-skills-a` against the original HEAD. No active
repository hooks were present (only sample files). No shared metadata file was
replaced, no sibling process was stopped, and no push or PR operation was used.


## Ported family: Shock of Frost and Frost Shard (23)

The selected family adds all ten reference methods: `shockoffrost_can_use`,
`shockoffrost_cast`, `shockoffrost_ctor`, `_frost_debris` (native `frost_debris`),
`shockoffrost_run`, `_frost_hit` (native `frost_hit`), `shockoffrost_hit`,
`frostshard_ctor`, `frostshard_run`, and `frostshard_hit`. Shared `projectile_ctor`,
`move_and_collide` and `puff` add three more tested units. The generator recounts
196 units from the pinned AST (10 top-level functions excluding harness-only
`shape_result`, plus 186 class methods) and verifies each of the 98 counted
ported names occurs exactly once. Partial generic dispatch, general descriptor
construction, imported bodies and the incomplete GameObject constructor remain
excluded. A reference refusal is preserved, never counted as implemented behavior.

Evidence is the same read-only pin above: `ref_skills_a.py` lines 548–629,
1087–1323 and generic `puff`, plus spec §0.6 and §23. No additional constants,
original Skill tables, engine behavior, dependencies or assets are invented.
The public fixtures use synthetic statistics and memory. No research file is edited.

The projectile step preserves per-component single-precision stepping, grid
updates, ground backtracking in accumulated tenths (keeping the last attempt),
one-time water state, the discarded own splash draw and fixed clip argument 1.
It preserves callback query order/duplicates, flag/owner/self filtering, strict
planar range and asymmetric height overlap, then calls the other object's slot 6
before its own slot 6. Geometry is reread for subsequent contacts; the owner is
snapshotted before the query as in the reference. The bolt clears last-hit before
every contact and processes multiple contacts; shards skip their excluded target
and stop after the first accepted hit. Damage uses explicit cold descriptor lanes,
with the existing callback representation, signed statistics and low16 truncation.

The family also preserves light and ribbon writes, particle/rejection draw order,
signed frames/levels/headings, spin and expiry ordering, debris masks and velocity
updates, hidden draw accounting only after a successful prop constructor, eleven
own draws per shard, and the last successful of ten terrain-ray headings. Shard
allocation failure does not construct a shard; the owner-node read on successful
allocation deliberately has no null test. Skill and object virtual dispatch retain
signed16 adjustments and explicit external overrides. Generic `puff` takes a ball
callback and optional spread, using one or two ball samples respectively.

New named callbacks: `water_test`, `libc_sprintf`, `water_disturb`, `PhProp_ctor`,
`PhProp_initPhysics`, `fx_spawn`, `terrain_raycast` and `Creature.v6`.
Existing terrain, handles, allocation, light, particle, sound, enemy and damage
callbacks remain explicit. Vectors flatten to x/y/z lanes in argument order.
Missing services record the attempted call and refuse. There is no fabricated
water, geometry, physics, damage, formatter, scheduler or lifetime implementation.
The Projectile constructor uses the existing no-property GameObject path and
retains its documented hidden/property refusal boundary. Queries and positive
shard counts are bounded to 65,536 entries, in addition to existing history,
memory and callback limits. Exceptions preserve the complete prior state.

All **64,432** earlier vectors remain byte-for-byte unchanged. Generator and
verifier independently pin that full prefix to SHA-256
`1d3bc3b81f8355d66b4c424e39b3997b49c5f0e711862d5cbf3912720fe8af07`.
The **4,415** appended cases cover all thirteen added units, all sixteen FPU
configurations, both constructors and virtual bodies, signed levels/frames,
particle paths, debris seeds, ground/water/splash branches, collision filters,
ordered repeated hits, exclusion and immunity, enemy refusal, allocation failure,
ray heading replacement, signed virtual adjustments and callback mutation,
exceptions and missing-service prefixes. API tests additionally check oversized
query refusal/recovery, geometry mutation between duplicate contacts, mandatory
services, forced external dispatch, generic ball counts, and failed debris
constructor draw accounting. Initial API runs exposed two fixture mistakes
(a slot-6 row offset and an expectation that every debris mask was 1); both were
corrected, and the affected suite then passed.

### Execution evidence

Release configure/build passed. Both affected CTests passed (2/2, 16.80 seconds),
including all **68,847** complete-state vectors. Arm64 trace SHA-256:
`3cd71d9765d0925e1b73715aaa1d325adf323d999aa1daa883d8bec85cd35036`.
Vector SHA-256:
`249b5ed5fbe45377b04d444506829a6192b82a22c9a46d7cc5e8c590b8f898cc`.

Commands (full-suite results follow below):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-exact$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3 > build/native/frost-full-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
git diff --check
```

Changes are confined to the nine existing Skills files: header, source, native
runner, API tests, generator, verifier, gzip vectors, manifest and this page.
Shared build files and sibling worktrees remain untouched. Private acceptance
measurements were skipped because `../../private-validation` is unavailable;
no search elsewhere was made. Physical Intel, Windows, console execution,
Norrath, original Creature services, Session/scene wiring and playable behavior
remain unverified. Next suggested family: Holy Strike (16) or Disease Bolt (4),
reusing the now-tested projectile/collision foundation. The remaining table above
is the continuation checklist; further exact work is portable from this pin.


Both the x86_64 runner under Rosetta and the arm64 AddressSanitizer +
UndefinedBehaviorSanitizer runner passed all **68,847** complete-state cases.
No sanitizer finding was reported. Both complete-trace `cmp` comparisons passed
with the same arm64 trace hash above. This verifies translated x64 execution; it does not
establish physical Intel or Windows compatibility.

The final Release sweep passed **102/102 tests** in **330.43 seconds**, with no
failures, timeouts or reruns. Skills API passed again in 0.01 seconds and Skills
exact passed again in 18.00 seconds during the concurrent sweep. Full output is
under ignored `build/native/frost-full-ctest.log`. All commands in this continuation
passed after the two corrected API-fixture failures described above.


## Ported family: Fire Flares (8)

This run selects the complete offensive orbit family: `fireflares_can_use`,
`fireflares_cast`, `fireflares_ctor`, `fireflares_dtor`, `fireflares_run`,
`fireflare_ctor` and `fireflare_run`. All seven bodies and their modeled Skill,
Spell and object virtual routes extend the existing module. The pinned AST still
contains 196 portable declarations (10 top-level functions excluding harness-only
`shape_result`, plus 186 class methods); 105 declarations are now fully ported and
tested. Partial dispatch and explicit refusals remain excluded from the numerator.

Evidence: the unchanged read-only `356add27` pin above, `ref_skills_a.py`
lines 4535–4766 (and `VT_SPELL_BASE` at line 4209), spec §8 and its executed
contract notes. The documented reference controls where condensed spec prose is
less precise, including the 110 **pairs** of burst particles. No original stats,
assets, decompiler output or research changes are included; all fixtures are authored.

The constructor preserves the light's division by zero, full-word levels, both
signed integer divisions and their modeled zero-divisor result, count cap 16,
individual allocation failure, shared seed draws, low16 angle stepping, and looping
sound acquisition/release. Orbit run preserves signed expiry, radius growth and
wave rounding under all sixteen FPU configurations. List order, duplicate objects,
null objects and callback-visible rereads follow the reference: run does not skip
null flare objects; cleanup does. Both walks snapshot the next node before callbacks.

Flare run preserves the 6/7/4 particle groups, own rejection draws, signed frame
modulo, first-enemy direct hit, ordered explosion query and duplicate victims,
low16 fire descriptors, distinct direct/explosion upper-bound rounding and captured
victim position. Bursts increment the signed16 size by seven, generate 110 points,
and emit two particles sharing each point and velocity. Missing services preserve
attempted calls and prior writes/draws. New explicit callbacks are `sound_stop`,
`light_dtor` and `pool_free`; existing allocation, light, sound, handle, grid,
query, enemy, damage, particle and deletion services remain callbacks. Cleanup
sets the Spell base table and frees only when flag bit 0 is set.

List walks retain the 65,536-node refusal bound and existing memory/write/call
limits. No lifetime manager, sound/light service, scheduling order, damage body or
scene behavior is fabricated. Constructor paths retain the existing GameObject
hidden/property refusal boundary. Original Skill initialization and draw routines
are outside this family contract.

The generator and verifier independently pin the entire prior **68,847**-vector
prefix to SHA-256
`249b5ed5fbe45377b04d444506829a6192b82a22c9a46d7cc5e8c590b8f898cc`.
Appended tests cover all seven bodies, virtual dispatch and adjustments, all FPU
settings, signed levels/frames/counts/angles/radii, allocation failure, lookup,
expiry, list cleanup, first-hit versus explosion damage, duplicate queries, burst
limits, callback mutation, missing callbacks and complete exception prefixes.
API checks add callback replacement, snapshot-before-delete behavior, cycle and
query refusal/recovery, forced external dispatch and shared burst particle lanes.

### Execution evidence

Release configure/build and affected CTests passed (2/2, 24.20 seconds).
All **75,873** complete-state cases passed, including **7,026** appended cases.
Arm64 trace SHA-256:
`5e100ef0607479d436bb09f0c12cf6c7e96b1446b052ca0d3a247c9d744eb7b3`.
Vector SHA-256:
`fb9192cfa2cfbfc77f679a9dc65fa87c6b2956ffb823b285b0422c663f7285d3`.
The first generation attempt exposed a new fixture returning a scalar for a
nonempty query; that script now returns a list, and regeneration passed.

Commands (cross-architecture, sanitizer and full-suite outcomes follow below):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3 > build/native/flares-full-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
git diff --check
```

Changes are confined to the nine existing Skills files: header, source, API tests,
trace runner, generator, verifier, gzip corpus, manifest and this page. Shared build
files and sibling worktrees were not changed. Private
acceptance measurements are skipped: `../../private-validation` is unavailable;
no search elsewhere was made. Scene/Session integration, original run-list order,
console behavior, Norrath, physical Intel and Windows remain unverified.
Next coherent family: Holy Strike (16) or Disease Bolt (4), using the existing
projectile/collision foundation. The remaining table above is the continuation list.

The independent x86_64 runner under Rosetta and arm64 AddressSanitizer +
UndefinedBehaviorSanitizer runner each passed all **75,873** complete states.
No sanitizer findings were reported. Both full-trace `cmp` comparisons with the
arm64 Release trace passed, with the identical trace hash above. Translated x64
execution does not establish physical Intel or Windows compatibility.

The final full Release sweep passed **102/102 tests** in **305.18 seconds**,
with no failures, timeouts or reruns. Output is under ignored
`build/native/flares-full-ctest.log`. All final validation commands above passed;
the one corrected generation-fixture failure is recorded above. No private
acceptance measurements, push, pull request or issue operation was performed.


## Ported family: homing attack projectiles — Disease Bolt (4) and Holy Strike (16)

This continuation adds all ten reference family methods: `diseasebolt_can_use`,
`diseasebolt_cast`, `diseasebolt_ctor`, `diseasebolt_run`, `diseasebolt_hit`,
`holystrike_can_use`, `holystrike_cast`, `holystrike_ctor`, `holystrike_run` and
`holystrike_hit`. Skill and object virtual dispatch use the pinned body identities
and signed16 this adjustments. The generator recounts the pinned AST: 10 top-level
functions excluding harness-only `shape_result`, plus 186 declared class methods,
196 total. It checks that each of the 115 fully ported names occurs once; partial
constructors, dispatch adapters and refusals remain excluded from the numerator.

Evidence: the unchanged read-only pin above, `ref_skills_a.py` lines 649–797 and
1341–1485, `skills-01-34.md` §4 and §16, and the executed contract notes. The Python
contract controls exact call/write order where prose differs, including Holy
Strike setting has-hit before flashing the victim. No original statistics,
assets, decompiler output, private bytes or research changes are included.

Both lifecycles preserve full input levels until low16 storage, signed frame and
heading behavior, one shared constructor seed, own sphere rejection draws, all
particle groups and per-particle position rereads, ordered owner handles, target
selection, homing, collision, hit-once behavior and expiry effects. Homing uses
plain angle subtraction across the signed16 seam; rate multiplication wraps to
signed32 before comparison. Neither the target node's object nor cast caster is
silently null-checked. Disease Bolt selects the first enemy in the returned arc
order and clears the temporary stack list; Holy Strike delegates population of its
owned list and follows its first node, ignoring the query return. No nodes or
ordering are synthesized by C++.

Disease Bolt builds the low16 disease descriptor and consumes one own draw for
the continuous damage maximum. With no owner and a noncreature, it writes has-hit
before applying damage. Holy Strike selects the undead statistic by signed byte,
snapshots PvP flags, consumes two draws, preserves the unusual positive maximum
`trunc(rolled + 1)`, and flashes both victim quads after successful damage.
Damage owners are reread after statistic callbacks. Missing services preserve
attempted calls, mutation, draws and all prior writes without rollback.

New named services are `trail_create` (one raw32 result) and `trail_addPoint`
(void); existing allocation, arc/list, enemy, handle, particle, sound, world,
terrain, water, light and damage services remain explicit callbacks. Vectors
flatten into x/y/z lanes in argument order as before. Disease Bolt's temporary
arc query has the existing 65,536-entry refusal bound. Holy Strike's arc return is
unused and has no invented bound; owned-list storage/lifetime belongs to the
callback. Both constructors retain the GameObject hidden/property refusal boundary.
No scheduler, target lifetime, trail renderer, Creature/damage body or scene
integration is supplied.

The generator and verifier independently pin every prior 75,873 vector to SHA-256
`fb9192cfa2cfbfc77f679a9dc65fa87c6b2956ffb823b285b0422c663f7285d3`.
The new synthetic cases cover all ten bodies and all sixteen FPU settings,
signed levels/frames/headings, angle seams, exact rate comparisons, zero/negative/
overflowing rates, null target objects, owner variants, immunity, undead bytes,
PvP, negative and extreme statistics, allocation failure, ordered duplicate
contacts, ground expiry, virtual adjustments and callback mutation/failure prefixes.
Synthetic and API checks add oversized-query refusal/recovery, first-enemy selection under
callback replacement, owned-list mutation, mandatory trail services, damage
failure prefixes, reentrant damage callbacks and explicit external dispatch.

### Execution evidence

Release configure/build and both affected CTests passed (2/2, 34.47 seconds).
All **90,846** complete-state cases passed, including **14,973** appended cases.
Arm64 trace SHA-256:
`069a1b09584d6685b45d9990c03625639eb03906d18518b922b1b37f1a6cb34c`.
Vector SHA-256:
`5e7d0d2fca9ac2eb845dcc656e39434977b0d58a808d93bde21629fa09baf51d`.
The initial API-only attempt failed because its new constructor fixture omitted
mandatory `obj_getPropInt`; the fixture supplied that callback and corrected its
flash-field assertion to +0x90. The following API rerun and exact suite passed;
no implementation correction was needed. Generator prefix assertions passed.

Commands (cross-architecture, sanitizer and full-suite outcomes follow below):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3 > build/native/homing-full-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
git diff --check
```

Changes are confined to the nine existing Skills files: header, source, API tests,
trace runner, generator, verifier, gzip corpus, manifest and this page. Shared
build files and sibling worktrees are untouched. Private acceptance measurements
are skipped because `../../private-validation` is unavailable; no search elsewhere
was made. Console behavior, Norrath, physical Intel, Windows, original Creature
services, Session/scene wiring and playable behavior remain unverified. The
remaining table above is the continuation checklist. Next suggested family:
Hammer of Wrath (17), a larger complete object/spell family; its imported item,
resource and third-RNG behavior must retain explicit service boundaries. Further
exact work is portable from the existing pin.

The independent x86_64 runner under Rosetta and the arm64 AddressSanitizer +
UndefinedBehaviorSanitizer runner each passed all **90,846** complete states.
No sanitizer finding was reported. Both complete-trace `cmp` comparisons with
arm64 Release passed, with the identical trace hash above. This verifies
translated x64 execution, rather than physical Intel or Windows validation.

The final full Release sweep passed **102/102 tests** in **427.60 seconds**,
with no failures, timeouts or reruns. Output is under ignored
`build/native/homing-full-ctest.log`. All final validation commands above passed;
the initial corrected API fixture failure is recorded above. No private acceptance
measurement, push, pull request or issue operation was performed.


## Ported family: Blinding Light (20)

The six complete reference methods are `blindinglight_can_use`,
`blindinglight_cast`, `blindinglight_effect_ctor`, `blindinglight_effect_run`,
`blindinglight_spell_ctor`, and `blindinglight_spell_run`. This family supplies
area crowd control and periodic victim damage for later scene integration.
Evidence is the pinned Python contract above, spec §20 and its executed-contract
notes. No research files, original statistics or private assets were imported.

Cast preserves both caster flashes (even with a null caster), screen flash, arc
query, sound, first-entry-only enemy check, allocation and stack-list clear order.
A friendly first entry prevents casting even when later entries are enemies.
The arc result is inspected only at its first entry and has no fabricated
traversal bound. The effect constructor retains the shared GameObject seed,
wrapped signed32 radius, ordered box results and duplicates, integer boss chance
halving toward zero plus one, strict chance comparison, one own draw per enemy,
Spell seed/attachment, and the no-target timer even if Spell allocation fails.
The existing box-result refusal bound and GameObject hidden/property refusals
remain explicit.

The effect emits twelve particles per live tick with exactly four ordered own
draws per particle, and deletes at the signed end frame. Radius and position are
reread for each particle, preserving callback mutation. Light and shaft
constructor draws are counted as hidden only after those callbacks return.
The victim Spell stores the caster as the reference's raw pointer, without
fabricating a handle or lifetime guarantee. Expiry precedes HP/modulo checks.
Periodic damage uses signed-byte rank, integer minimum halving toward zero,
single-precision spread then multiplication by one half, and victim/caster
rereads after statistic callbacks. Allocation, damage, target lifetime, rendering
and scheduling remain supplied services.

New named callbacks are `gfx_setColorOffset` (four scalar lanes, void),
`GlowingShaft_ctor` (shaft, position x/y/z, duration, three color lanes; void),
and `Creature.v31` (adjusted victim, duration; void). Existing callback shapes,
scalar-result checks, copied active callbacks, reentry, signed16 virtual
adjustments and explicit external overrides are unchanged. Missing services
record the attempt and refuse while preserving prior writes and draws.

The AST inventory remains **196** declarations (10 top-level functions excluding
harness-only `shape_result`, plus 186 directly declared class methods). The six
new fully tested names raise the ported count from 115 to **121**. Dispatch
adapters, imported helpers, embedded numeric expressions and partially refused
constructors remain excluded from the numerator. The generator verifies each
counted name occurs exactly once in the pinned AST.

The generator and verifier pin every prior **90,846** vector, including its
header, to SHA-256
`5e7d0d2fca9ac2eb845dcc656e39434977b0d58a808d93bde21629fa09baf51d`.
New synthetic cases cover all six direct methods, Skill/object/Spell dispatch
with signed adjustments, all sixteen FPU settings, signed levels/frames,
wrapped radii/durations, HP patterns, boss/chance thresholds, seeds, allocation
failure, empty/null/duplicate query results, and service mutation/failure prefixes.
API checks also exercise first-friendly targeting, null caster flashes, failed
Spell allocation with timer calls, callback replacement, precise particle draw
prefixes, raw-caster/victim rereads, expiry and forced external dispatch.

### Execution evidence

Release configure/build and affected CTests passed (2/2, 41.38 seconds).
The strengthened API suite passed again after adding mandatory scalar refusal,
object/Skill overrides and reentrant damage checks. All **97,626** complete
states passed, including **6,780** appended cases. The original corpus-prefix
assertions passed in both generation and verification.

The independent x86_64 runner executed under Rosetta and the arm64
AddressSanitizer + UndefinedBehaviorSanitizer runner also passed all **97,626**
states, with no sanitizer finding. Both full-trace `cmp` comparisons with arm64
Release passed. All three traces share SHA-256
`622f19eb161d056c7004cd88c2fdcaea235068d61e87b2df40ee0116cda16e50`.
The vector SHA-256 is
`248ccb90d3eed7302a7acdedd3cc9ff2a51fd176ae16f25b37a4c7db116ab12b`.

The full Release sweep passed **102/102 tests** in **533.42 seconds**, with no
failures, timeouts or reruns. All validation commands passed.

Commands executed successfully:

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re > work/blinding-generation.log 2>&1
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure > build/native/blinding-affected-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces > build/skills-a-x64/blinding-validation.log 2>&1
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces > build/skills-a-sanitize/blinding-validation.log 2>&1
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3 > build/native/blinding-full-ctest.log 2>&1
git diff --check
```

Changes are confined to the nine existing Skills
files: header, source, API tests, trace runner, generator, verifier, gzip corpus,
manifest and this page. No shared build file or sibling worktree was edited.
Private acceptance measurements are skipped because `../../private-validation`
is unavailable on this machine; no search elsewhere was made. Original Creature
services, Session/scene wiring, playable behavior, console behavior, Norrath,
physical Intel and Windows remain unverified. Further exact families remain
portable from the existing pin; Hammer of Wrath (17) remains the next suggested
large family, with item/resource/third-RNG service boundaries to preserve.


## Ported family: Disease Shield (5) and Disease Effect (29)

All six declared methods are ported and tested: `diseaseshield_can_use`,
`diseaseshield_cast`, `diseaseshield_ctor`, `diseaseshield_run`,
`disease_spell_ctor` and `disease_spell_run`. Evidence is the pinned Python
contract above (lines 2351–2444 and 4229–4359) and spec sections 5 and 29.
The existing AST accounting remains 10 top-level functions plus 186 methods,
excluding only the harness `shape_result`; these six bring the tested inventory
to 127. Embedded expressions and partial dispatch adapters add no units.

The family preserves shield signed-byte and disease full-register statistic
arguments. The shield retains its cap values, constructor's untouched
current-radius field, half-unit growth,
owner drift, callback-dependent particle count rereads, low cloud and mote draw
order, sixteen swarm updates, owner-position rereads, and hit sound ordering.
Its damage descriptor contains `(0x26, dmg & 0xffff, dmg & 0xffff, 0)` followed
by three absent-slot tokens. Its maximum is deliberately halved **after** the
spread roll; this may leave it below the minimum. The callback decides how to
apply that damage.

The disease status preserves its null base Skill field and separate full-width
level and Skill fields, source handle, sixteen position/velocity pairs, second
flash quad, global-frame modulo timing and ordered contagion. Creature flags,
enemy and attached-status checks retain query order and duplicates. Negative or
zero contagion durations still construct and seed a status; allocation failure
never fabricates an attachment. Periodic damage clamps the halved statistic to
at least one and rereads the source handle after the statistic callback. The
shield and status use their distinct documented swarm radius constants.

`glow_add` is an explicit new presentation service. Its arguments flatten as
`{x,y,z,0,0,0,alpha,texture,0,0,size,range}`. The texture word is read anew from
`0x4f8498` each time. Existing allocation, handle linkage, enemy virtual, damage,
query, sound and deletion callbacks remain mandatory when reached. Unknown type
getters refuse unless supplied in `type_info`. Both spell run bodies and the
shield Skill bodies dispatch with signed16 `this` adjustments and retain forced
external callback overrides. No original damage consumer or engine lifecycle is
silently supplied. Existing history, ball rejection and query bounds still apply;
each radius-dependent particle loop additionally refuses after 65,536 iterations.
Exceptions keep prior draws, writes and callback effects.

The appended synthetic vectors exercise all sixteen FPU settings, signed levels,
full stored levels, signed frame boundaries, constructor seeds, radius thresholds,
outside-swarm pullback, resistance extremes, existing attached statuses,
null/non-creature sources, duplicates, enemy rejection, allocation failure,
callback failures/mutations and virtual adjustments. API tests additionally verify
callback replacement/reentry, full-level statistic arguments, source/victim
rereads, missing presentation, query bounds and recovery, and forced external
spell dispatch. A new prefix hash protects **all 97,626 prior rows**, in addition
to the existing historical prefix checks.

Further families remain portable. The continuation table above is authoritative;
Hammer of Wrath (17) is the next large family, with its imported item adapters,
third RNG and documented state-1/2 refusals preserved. Disease status consumers,
original per-class statistics initialization, Session/scene wiring and rendering
remain separate integration work.

### Execution evidence

Release configure/build and the affected CTests passed (2/2, 52.35 seconds).
All **99,939** complete states passed, including **2,313** appended cases;
all **97,626** existing rows retained their exact bytes. The generator verified
the pinned research files against both checkout content and the recorded commit.
The research checkout remained unchanged. Two exploratory generation runs were
interrupted while narrowing synthetic extreme-radius and signed-dispatch fixtures;
the completed generation and affected tests returned success.

The independent x86_64 runner executed under Rosetta and the arm64
AddressSanitizer + UndefinedBehaviorSanitizer runner also passed all **99,939**
states, without sanitizer findings. Both byte-for-byte trace comparisons against
arm64 Release passed. All three trace SHA-256 values are
`9274cc454e188098a37cc22772c52e5fdff7c751be87b9b9c42cf48ef09d6da1`.
The vector SHA-256 is
`e2fbadb97554a7c0195a2184b23b4b81ab0537c833583b6d7a73f3d95e25e7c4`.

The full Release sweep passed **102/102 tests** in **447.85 seconds**,
with no failures, timeouts or reruns. The skills exact case passed again within
the full sweep. Logs and trace reports remain under ignored `build/`.

Commands used for this continuation (successful final validation unless noted):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re > work/disease-generation.log 2>&1
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure > build/native/disease-affected-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces > build/skills-a-x64/disease-validation.log 2>&1
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces > build/skills-a-sanitize/disease-validation.log 2>&1
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3 > build/native/disease-full-ctest.log 2>&1
git diff --check
```

Only the nine existing Skills files changed: header, source, trace runner, API
suite, generator, verifier, compressed vectors, manifest and this page. Shared
build files and sibling worktrees were untouched. Private acceptance measurements
were skipped because `../../private-validation` is unavailable, as instructed;
no alternate location was searched. Session/scene integration, original service
implementations, playable behavior, console behavior, Norrath, physical Intel and
Windows remain unverified. Further exact families remain portable from the pin.
