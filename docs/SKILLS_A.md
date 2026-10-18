# Return to Arms Skills: foundation, weapon buffs, shields, support, Shield Bash, Fire Storm, Disease Trail, Frost, Fire Flares, homing projectiles, Blinding Light, disease status and channelled fire/Wizard Beam, Blessed, offensive undead, Poison Rain, Hammer of Wrath, Riposte, Summon Skeleton, item adapters and generic dispatch

Selected continuation family: **GameObject constructor (the final reference unit)**.

PROGRESS: 196 of 196 reference functions ported

The [final constructor section](#final-constructor-unit-documented-gameobject-domain)
records the current implementation. Earlier continuation sections retain their
historical counts and blocker language as dated evidence.

The [RTA Skill arithmetic kernel](SKILLS_KERNEL.md) now supplies the shared
statistic/tier and LCG primitives; this module retains its memory, rounding,
range, callback and failure contracts. Scene integration remains separate.

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
swarm, radius growth, contagion and periodic damage family. Fire Weapons (13) and
Cone of Fire (18) add weapon enchantment/aura and channelled area damage.
Wizard Beam (1) adds its channelled attack and stored chain/target/ring state,
with the same explicit presentation boundaries as the reference. Blessed (10) adds all six modeled methods, including its destructor and the statistic interface used by mana regeneration.
Repulse Undead (21) and Dismiss Undead (22) now add radial damage and the full
HolyBolt homing/bounce/hit lifecycle. Poison Rain (24) adds the full modeled
monster area-attack lifecycle with rain, terrain splashes, periodic poison damage
and expiry. Hammer of Wrath (17) adds its full modeled object/spell lifecycle,
item adapter and third RNG. Engine services and presentation are explicit
callbacks. Session, scene integration and other active abilities remain work.
It depends on `champions-core` and the existing `creature_hp::Memory` through
`champions-creature-hp`, and now reuses `champions-items` for descriptor parsing;
no other native module was changed. The namespace is explicitly `simulation::rta::skills_a`.

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
silently supplied. Fire Weapons (13) now has its own separately ported light lifecycle, documented below.

The callers supply all tables, fields and virtual table rows. Individual `initStats`
and constructors are not provided: synthetic test numbers exercise these routines
without pretending to supply actual per-class statistics. `skill_call(skill, 5,
{level})` is the `mana_cost` wrapper equivalent; `skill_call(skill, 1, {caster})`
is the `can_use_slot` equivalent only for a supported base body or explicit callback.
The last reference override's constant/carries shapes for unknown slot-1
bodies are now implemented through `can_use_slot` and `skill_call`.

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
| 1 Wizard Beam | Engine/channel scheduling, original per-class initialization and trail point arrays; all five reference family methods are ported |
| 2 Dodge | Skill initialization and missile-consumer dodge/debris behavior; only fraction helper is ported |
| 3 Resist Poison | Per-skill constructor and consumer integration (shared base initializer is covered) |
| 4 Disease Bolt | Engine/scene integration and original per-class initialization/draw; all five reference family methods are ported |
| 5 Disease Shield | Engine/scene integration, original per-class initialization and presentation; all four reference family methods are ported |
| 6 Disease Trail | Engine/scene integration and original per-class initialization; all seven reference family methods are ported |
| 7 Fire Storm | Engine/scene integration, original per-class initialization and external flare body; all seven reference family methods are ported |
| 8 Fire Flares | Engine/scene integration, original per-class initialization and draw; all seven reference family methods are ported |
| 9 Riposte | Original per-class initialization, damage-consumer/scene integration and model/presentation services; the complete counterattack and passive cast are ported |
| 10 Blessed | Engine/player mana-consumer integration and original per-class initialization; all six reference family methods are ported |
| 11 Summon Skeleton | Engine/scene integration, original per-class initialization, skeleton/terrain/holder services; all four imported lifecycle methods, sample helper and local callback bridge are ported |
| 12/14/26 Lightning/Cold/Poison Weapons | Skill constructors/statistics initialization and engine/consumer integration; full reference ability lifecycle is ported |
| 13 Fire Weapons | Engine/consumer integration and original per-class initialization; all four reference family methods are ported |
| 15 Undead Shield | Engine/consumer integration and original player spell ordering; all ten reference family methods are ported |
| 16 Holy Strike | Engine/scene integration, owned target-list/trail services and original per-class initialization/draw; all five reference family methods are ported |
| 17 Hammer of Wrath | Engine/scene integration, original per-class initialization, model/particle/damage services and reference-refused states 1/2; all twelve modeled family/adapter methods are ported |
| 18 Cone of Fire | Engine/channel scheduling, actual damage consumer and original per-class initialization; all four reference family methods are ported |
| 19 Regeneration | Skill initialization and Player mana-regeneration consumer; only statistic helper/passive cast are ported |
| 20 Blinding Light | Engine/scene integration, raw caster lifetime, original per-class initialization and presentation; all six reference family methods are ported |
| 21 Repulse Undead | Original per-class initialization, direct damage consumer and LostSoul/scene integration; both reference family methods are ported |
| 22 Dismiss Undead | Original per-class initialization, raw target lifetime, engine/presentation and scene integration; all four reference family methods are ported |
| 23 Shock of Frost | Engine/scene integration and original per-class initialization; all ten reference family methods are ported |
| 24 Poison Rain | Engine/monster dispatch, original per-class initialization, terrain/presentation and damage consumers; all four reference family methods are ported |
| 25/27/28/30 Disease Weapons/Hate Beam/Ball of Hate/Hate Effect | Per-class tables/initialization and consumer integration |
| 29 Disease Effect | Damage consumer/scene integration and original per-class initialization; both reference family methods are ported |
| 31/32 Root | Engine/consumer integration and original owner scheduling; all four reference family methods are ported (also aliases 52/85) |
| 33 Critical Hit | Engine/consumer integration and original damage dispatch; all six reference family methods are ported (also aliases 53/81) |
| 34 Shield Bash | Original animation/movement services and scene integration; all six reference family methods are ported |

The exact uncounted declaration inventory is one unit: `gameobject_ctor`.
Its no-property, nonhidden domain is implemented and tested, but the pinned
reference itself refuses hidden-grid relocation and property/name-table behavior.
It is conservatively excluded from progress because refusal does not implement
those behaviors. Completing it requires new contracts for both paths, including
ordered memory writes, name resolution and grid unlink/relink semantics.

The denominator is an AST count of ten top-level functions (excluding harness
`shape_result`) and 186 class methods in the pinned file. Redefined `skill_call`
methods count separately and are qualified as `skill_call.original` and
`skill_call.override` in the manifest. Imported functions are outside this total.
The imported Summon Skeleton lifecycle and cosmetic helpers have independent
native tests but add nothing to the denominator. All other declared units now
have portable implementations and exact tests; reference-delegated services
remain explicit callbacks. No further complete declaration can be added from
this pin alone: status BLOCKED. Engine/scene integration listed in the table
above remains separate from this contract-port task.

Integration still to supply: imported owner per-tick spell walks, inherited
Creature/arena virtual bodies, lifetime management, damage consumers, terrain and
world services, rendering/audio and scene scheduling. They remain explicit services.
General Skill/Spell/object dispatch, descriptor construction and the declared
particle/light helpers are now tested. The no-property GameObject constructor
retains the pinned hidden/property refusals and is not counted as fully ported.

The reference itself leaves all draw functions and `describeRank` outside the model,
Wizard Beam trail point arrays unmodeled, and Hammer states 1/2 unsupported. These
must remain explicit callback/refusal boundaries in any future extension. Real
object lifetime and owner/spell run order, damage application, player enemy checks,
terrain/world queries and actual console divide/FPU behavior are separate evidence
boundaries. Nothing here establishes console, Norrath, Windows, native Intel,
Session or playable-client compatibility. The remaining constructor paths need new research, so the current declaration
continuation is **STATUS: BLOCKED**.


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


## Ported family: Fire Weapons (13) and Cone of Fire (18)

All eight reference methods are ported: `fireweapons_can_use`, `fireweapons_cast`,
`fireweapons_ctor`, `fireweapons_run`, `coneoffire_can_use`, `coneoffire_cast`,
`coneoffire_ctor` and `coneoffire_run`. The same pinned Python contract and spec
§12–13/18 supply the evidence. The AST inventory still contains 196 declarations
(10 top-level functions excluding harness helper `shape_result`, and 186 class
methods). Eight complete methods increase the count from 127 to 135; partial
shared dispatch and explicit refusals add no units. The manifest lists all 135
names and the generator verifies each occurs exactly once in the pinned AST.

Fire Weapons preserves its own light record, ordered halfword/word writes,
shared seed, full-register first duration read, subsequent signed-byte levels,
integer aura radius, both player weapons and nonplayer holder slot 2. Opening
particles consume two own rejection-sampled balls each. Light range flickers until
the final 30 frames (with the original opening precedence), then uses the exact
floating minimum/fade. Position follows the supplied facing and position. Expiry
and dead nonplayer cleanup retain holder slot 8 and deletion order. Aura queries
preserve duplicates, self/player filtering, PvP checks and the minimum-damage
threshold of 31, using the existing enchantment contract.

Cone of Fire preserves its linked constructor light and looping sound release,
per-frame light writes without relinking, all 28 spray particles and their two
independent angle draws. Quarter-rate damage runs every 15 frames, rereads the
arc statistic, queries from the backward offset, checks each returned enemy and
consumes one own damage draw per accepted entry. The maximum is quarter-scaled a
second time: reversed ranges and the low=1/high=0 case remain unchanged in the
descriptor. No downstream divide or damage behavior is guessed. Hit sound precedes
stack-list cleanup, and failures retain the preceding writes, draws and calls.

`Holder.v2` is the sole new named service. Existing light, sound, particle, query,
enemy, damage, holder-expiry and deletion callbacks remain explicit. Skill and
Spell virtual bodies use signed16 adjustments and honor external overrides.
The arc query uses the existing `STACK_QUERY` token and 65,536-result refusal
bound. Missing callbacks, invalid scalar responses and existing history/depth
limits refuse without rollback. Original light destruction, channel release,
spell ownership/scheduling and damage application remain engine responsibilities.
No new engine implementation, dependency or shared build-file change is added.

Synthetic cases cover all sixteen FPU settings, signed level/frame/facing edges,
opening/fade/expiry/modulo boundaries, own seeds, player/nonplayer/dead owners,
failed allocation, attached spell identities, both weapons, aura/PvP filtering,
query duplicates/nulls/empty results, statistic extremes, callback mutations and
exceptions, missing services and signed virtual dispatch. API tests separately
check callback replacement/reentry, constructor failure prefixes, unchanged shared
RNG counts, reversed damage ranges, bounded-query refusal/recovery and forced
external dispatch. All 99,939 previous rows have a new enforced prefix hash.

The continuation table above remains authoritative. Hammer of Wrath (17),
Wizard Beam (1), Blessed (10), Poison Rain (24), Dismiss Undead (22), and the
Repulse/Riposte helpers remain portable candidates; delegated behavior and
reference refusals must remain explicit. Scene integration, original statistics,
channel/input behavior, console results, Norrath and platform exports remain
separate evidence requirements. Private acceptance measurements were skipped
because `../../private-validation` is unavailable, as instructed; no alternate
location was searched.


### Execution evidence for the fire continuation

Vector generation and the affected Release CTests passed (2/2, 41.62 seconds).
All **102,321** complete states passed, including **2,382** new cases; all
**99,939** previous rows are byte-for-byte unchanged (also checked directly
against the committed gzip). The generator verified every pinned research file
against checkout content and its pinned commit. The research checkout is clean.
The first API rebuild found colliding local test variable names; enclosing the
new test group in its own scope fixed the compilation error. The subsequent
build and extended API suite passed.

The separately compiled x86_64 runner under Rosetta and arm64 AddressSanitizer +
UndefinedBehaviorSanitizer runner each passed the same complete corpus. Both
`cmp` comparisons with arm64 Release passed. No sanitizer findings were reported.
All three trace hashes are
`b0cc999df3ba9cfc019f7e9232cb3cc8bcd1db650e7e2e5f68ab0a593fd8b364`;
the vector hash is
`6ef047051d61fea147898038cd3d58db1ce53ad21b03eebeed001315c00f3019`.
Physical Intel, Windows, console and original-client behavior remain unverified.

Commands (final invocations passed unless stated above):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces > build/skills-a-x64/fire-validation.log 2>&1
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces > build/skills-a-sanitize/fire-validation.log 2>&1
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3 > build/native/fire-full-ctest.log 2>&1
git diff --check
```

The full Release sweep passed **102/102 tests** in **297.01 seconds**,
with no failures, timeouts or reruns. The skills exact case passed again within
the sweep. Full output is in `build/native/fire-full-ctest.log`; architecture
and sanitizer logs are in their respective ignored `build/skills-a-*` directories.

Only the nine existing Skills files changed: header, source, trace runner, API
suite, vector generator, verifier, compressed vectors, manifest and this page.
Shared build files, other modules and sibling worktrees were untouched.


## Ported family: Wizard Beam (1)

All five declared family methods are ported: `wizardbeam_cast`, `wizardbeam_ctor`,
`_beam_flash` (native `beam_flash`), `_beam_chain` (native `beam_chain`) and
`wizardbeam_run`. Evidence is the pinned Python contract, lines 3927–4198,
and spec §1. This is a substantial channelled player-attack family: main/side
chain simulation, targeting, damage and presentation call ordering. Base can-use
was already implemented; original channel-input and owner lifecycle consumers
remain integration work. The AST count remains 196 portable declarations:
10 top-level functions excluding harness `shape_result`, plus 186 class methods
(including repeated override declarations and imported interface adapters).
Five complete methods increase the tested inventory from 135 to 140; callback
boundaries, partial dispatch and bounded refusals add no units.

The constructor preserves one shared seed draw and one own hue draw, exact HSV
colours, nine main/ring/target trails, two trails per side beam, signed-byte level
reads, wrapped integer scaling, negative side counts and the upper clamp of six.
The run preserves initial node/velocity writes, strict 3-D primary selection and
plane-distance limit, callback order and duplicate side targets, sampled model
points with ten own draws, ordered target tint writes, raw library `powf` weights,
per-node spring/normalization order and dynamically reread lengths. Damage checks
the signed global frame independently for each target and uses one own draw per
accepted damage call. Negative frame ring ages and signed shifted/remainder ring
indices retain the reference's adjacent-memory behavior.

Five new named presentation services are explicit callbacks:
`trail_setPoints_1918a0`, `trail_clear_191e58`, `curve_sample_1911e8`,
`ring_draw_1920e0`, and `Creature_getVisualPos`. Stack arrays/output pointers are
represented by the existing `STACK_QUERY` token (`0xffffffff`) in their original
argument positions. The reference does not build the trail arrays; this port
makes the same calls and does not fabricate their content. Visual position
returns three raw lanes. Existing trail creation, enemy/model virtual calls,
model point sampling, sound, resource lookup and damage callbacks remain required.
Skill/Spell dispatch retains signed16 adjustments and explicit forced overrides.
Missing callbacks or malformed scalar/vector responses refuse at the existing
prefix, without rollback. Query, side-chain and node loops have a 65,536-entry
bound; if callback mutation grows a side count beyond the previously built spoke
array, the port explicitly refuses. Shared history/memory/depth caps still apply.

Synthetic vectors exercise all sixteen FPU settings, signed level/frame/facing
edges, first/repeated runs, no target and multiple/duplicate targets, distance
boundaries, negative/zero/max side counts, sample counts, raw arithmetic extremes,
ring age/index boundaries, constructor failure, virtual dispatch adjustments,
missing callbacks and synchronous failure/mutation prefixes. API checks also cover
callback replacement/reentry, malformed result refusal, bounded-query recovery,
forced virtual override and unchanged RNG ownership. All 102,321 previous rows
have an enforced prefix hash in both generator and verifier.

The continuation table above remains authoritative. Hammer of Wrath (17) is the
next suggested large family; preserve its imported item adapters, third RNG and
state-1/2 refusals. Blessed (10), Dismiss Undead (22), Poison Rain (24) and
Repulse/Riposte also remain portable. Private acceptance measurements are skipped
because `../../private-validation` is unavailable, as instructed; no alternate
location was searched. Scene integration, original channel/input behavior,
console results, Norrath, physical Intel and Windows remain unverified.

### Execution evidence for the Wizard Beam continuation

Vector generation and the affected Release CTests passed (2/2, 53.35 seconds).
All **105,797** complete states passed, including **3,476** appended cases;
all **102,321** previous rows are byte-for-byte unchanged, also checked directly
against `HEAD`'s compressed corpus. The generator verified the research content
against the pinned commit; the research checkout remained clean. The extended
API suite passed. An initial build caught a C++ hex-token spacing error in the new
API test; it was corrected and the complete rebuild passed. One prematurely
started API command reported Not Run while the rebuild was unfinished; both
subsequent API and affected-suite commands passed.

The independently compiled x86_64 runner under Rosetta and arm64
AddressSanitizer + UndefinedBehaviorSanitizer runner each passed all **105,797**
states. Both byte-for-byte comparisons with arm64 Release passed, without
sanitizer findings. All three complete trace hashes are
`a98085044d42c9134ab78081cfb0068f73c65f70fb21163f5363c642805eb6e6`;
the vector hash is
`05522993a7a9b52b174087335178c9b5734450636da63a686edee25aca340311`.

Commands (final validation invocations passed; full sweep result below):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re > work/wizard-generation.log 2>&1
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4 > build/native/wizard-build.log 2>&1
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure > build/native/wizard-affected-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces > build/skills-a-x64/wizard-validation.log 2>&1
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces > build/skills-a-sanitize/wizard-validation.log 2>&1
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3 > build/native/wizard-full-ctest.log 2>&1
git diff --check
```

Changes are confined to the nine existing Skills files: header, source, trace
runner, API suite, vector generator, verifier, gzip corpus, manifest and this
page. Shared build files and sibling worktrees were untouched. No private data
was used; original runtime services, channel release, scene wiring and playable
behavior remain separate evidence requirements. Further families are portable
from the pinned evidence.

The full Release sweep passed **102/102 tests** in **393.62 seconds**,
with no failures, timeouts or reruns. Both Skills tests passed again in the
sweep (`skills-a-exact`: 48.55 seconds). Full output remains in
`build/native/wizard-full-ctest.log`; architecture and sanitizer logs remain
in their respective ignored `build/skills-a-*` directories. `git diff --check`
passed, and the read-only research checkout remained unchanged.


## Ported family: Blessed (10)

All six declared methods are now ported: `blessed_can_use`, `blessed_cast`,
`blessed_ctor`, `blessed_dtor`, `blessed_stat0` and `blessed_run`. Evidence is the
same pinned Python contract, lines 4778–4923, and spec §10. This player support
family exposes the original mana-regeneration statistic interface; the original
Player mana consumer itself is outside this module. No original statistic table,
scene dispatcher or rendering implementation is fabricated.

The AST inventory contains 196 portable declarations: 10 top-level functions
excluding harness helper `shape_result`, and 186 class methods, including repeated
override declarations and production imported-interface adapters. The generator
checks all 146 tested names occur exactly once. Six complete methods increase the
count from 140 to 146. Callback boundaries and partial/refused methods add no units.

The constructor preserves one shared seed draw, zero-range inverse arithmetic,
ordered light writes, original signed-byte level argument for duration even when
callbacks mutate stored level, six initialized sparkle records and own rejection
sampling. The run preserves signed expiry, owner rereads after callbacks, light
relink/tint/inverse ordering, negative sparkle counts and the six-record upper cap,
rate update order, distance/height clamps and signed angle conversion. Each glow
retains the reference's colour lane order; all six then four particles draw their
velocity ball before their position ball, with the distinct raw speed/spread values.
The destructor switches tables in order, unlinks/destroys the light, clears the
handle and frees only for flag bit 0. Skill and Spell dispatch preserve signed16
adjustments and forced overrides; `blessed_stat0` remains a direct consumer method,
as in the reference.

No new services are needed. Allocation, light link/relink/unlink/destruction,
handle clearing, freeing, glow and particle emission use the existing explicit
named callbacks. Missing services and malformed scalar results refuse at the
attempted call prefix without rollback. Scene/Session wiring, original mana
regeneration and actual rendering/lifetime remain separate integration work.

Synthetic vectors cover all sixteen FPU settings, signed level/frame extremes,
expiry edges, own seeds, negative/zero/over-cap counts, radius/height/rate/angle
raw extremes, attached-spell identity, failed allocation, signed virtual dispatch,
and every named-service failure/missing-service/mutation boundary. API checks
exercise callback replacement/reentry, original-argument duration, malformed
scalar refusal, destructor write order, deletion/free flags, forced dispatch and
recovery after refusal. The entire prior 105,797-row corpus is hash-pinned in both
generator and verifier.

The remaining-family table above is the continuation list. Hammer of Wrath (17)
is the next suggested large family; retain its imported item adapters, third RNG
and documented state-1/2 refusals. Poison Rain (24), Dismiss Undead (22),
Repulse/Riposte, and imported summon/owner-walk bridges also remain candidates.
Private acceptance measurements are skipped because `../../private-validation`
is unavailable, as instructed; no alternate location was searched.

### Execution evidence for the Blessed continuation

Vector generation and the affected Release CTests passed (2/2, 75.12 seconds).
All **109,110** complete states passed, including **3,313** appended cases;
all **105,797** previous rows retain their exact bytes through the enforced
prefix hash. The generator verified the public research files against both
checkout content and the pinned commit. The extended API suite passed.
An initial validation stopped at the verifier's unchanged expected-family list;
adding Blessed corrected that metadata gate. The subsequent affected validation
and architecture comparisons use the same generated corpus.

The vector SHA-256 is
`90af1ec0bf135123768b61f1d0962a2a6209f413a77e3dca010c76309fd3efb4`.
The arm64 Release and x86_64 Rosetta runners both passed all states; their complete
traces are byte-identical, SHA-256
`900ac46437525d971e306e3ab4ad622ca8d36148d17af3faafbb338b3889f6c5`.

Commands used (successful final invocations; initial metadata failure above):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re > work/blessed-generation.log 2>&1
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > build/configure-blessed.log 2>&1
cmake --build build/native -j 4 > build/native/blessed-build.log 2>&1
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure > build/native/blessed-affected-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace > build/skills-a-x64/blessed-build.log 2>&1
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces > build/skills-a-x64/blessed-validation.log 2>&1
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace > build/skills-a-sanitize/blessed-build.log 2>&1
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces > build/skills-a-sanitize/blessed-validation.log 2>&1
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3 > build/native/blessed-full-ctest.log 2>&1
git diff --check
```

Only the nine existing Skills files changed: header, source, trace runner, API
suite, generator, verifier, gzip corpus, manifest and this page. Shared build
files and sibling worktrees were untouched. Private acceptance measurements
were skipped because the private directory is unavailable. Scene integration,
original mana/lifetime services, playable behavior, console results, Norrath,
physical Intel and Windows remain unverified. Further exact families remain
portable from the pinned evidence.

The arm64 AddressSanitizer + UndefinedBehaviorSanitizer runner also passed all
109,110 states with no sanitizer findings. Its trace is byte-identical to both
Release architectures, with the same complete trace SHA-256 above.

The full Release sweep passed **102/102 tests** in **349.93 seconds**,
with no failures, timeouts or reruns. Both Skills cases passed again in the
sweep. Full output remains in `build/native/blessed-full-ctest.log`.
`git diff --check` passed and the read-only research checkout remained clean.


## Ported family: Repulse Undead and Dismiss Undead (21/22)

This continuation implements all six methods: `repulse_cast`, `damage_radial`,
`dismissundead_cast`, `holybolt_ctor`, `holybolt_run` and `holybolt_hit`.
Skill cast bodies `0x2f4d34` / `0x2f645c` and HolyBolt object run/hit bodies
`0x2f57b0` / `0x2f5fb4` dispatch through the existing virtual machinery,
including signed16 `this` adjustments and forced external overrides.
The evidence is the same pinned Python contract above, spec §21–22, and the
associated executed notes. No research files or original tables were imported.

Repulse preserves the signed machine quotient and angle walk, callback argument
order, soul allocation failures, hidden-draw accounting after successful delegated
constructors, light call, both statistic reads, radial call and final sound.
`damage_radial` preserves ordered/duplicate box results, undead/enemy/self/skip
filtering, the octagonal distance approximation, supplied object radius, fallback
scale 100, minimum/full clamps, truncation and direct slot-2 damage dispatch.
No armour, critical, hit roll or RNG is added to that direct consumer call.

HolyBolt preserves all light writes, the untouched voice-handle word `+0xb0`,
callback-visible field rereads, absolute expiry, and raw target pointer. Target
selection uses the original nearer-than-either-side test; a qualifying later
entry can replace a nearer same-side entry. Empty-target aim uses two separately
rounded products (12 then 4000). Run preserves wrapped signed16 homing, vertical
conversion/dead zone, collision callbacks, exact trail arithmetic, terrain-only
bounce (`result == 1`), eleven bounce draws, ten raycasts, particles, voice/light
updates and signed expiry. Hit preserves short-circuit order, expiry clearing,
light/particle/sound prefixes, damage statistics and own RNG, null descriptor,
and the post-damage HP check before the delegated LostSoul allocation.

New named callbacks are `voice_setPosition`, `LostSoul_ctor` and `Creature.v2`;
they use the existing flattened raw-lane representation. The LostSoul body stays
external; its successful constructor contributes one hidden draw exactly as the
reference does. The reference also delegates actual damage, terrain, world queries,
light, trails, sound and deletion. Missing services and malformed scalar results
refuse without rollback. Repulse's angle walk has a 65,536-iteration refusal bound,
matching this module's existing bounded history domain; nonterminating zero or
negative-step reference walks are outside that domain. Existing query/history,
recursion, sparse-memory and ball-rejection bounds remain unchanged.

The generator recounts **196 portable declarations** in the pinned AST (10
top-level functions excluding harness `shape_result`, plus 186 declared class
methods, retaining repeated declarations as separate units). It verifies that
all 152 counted ported names are unique declarations. Explicit callbacks replacing
behavior absent from the reference do not add to the ported count. The previous
109,110 complete-state rows are pinned byte-for-byte by SHA-256
`90af1ec0bf135123768b61f1d0962a2a6209f413a77e3dca010c76309fd3efb4`.

The remaining-family table above is the continuation list. Hammer of Wrath (17)
is the next larger combat family; its item/resource adapters and third RNG need
porting together. Summon Skeleton, Poison Rain and Riposte also remain portable
from public evidence. Engine/Session/scene integration and original initialization
remain separate work even for completed reference families.

### Offensive-undead execution evidence

The generator appended **7,106** synthetic complete-state cases, bringing the
corpus to **116,216**. Both affected Release CTests passed (`skills-a-api` and
`skills-a-exact`; 56.40 seconds together). All prior rows remained byte-identical.
The API suite additionally verifies missing/malformed callbacks, replacement and
reentry, failure prefixes, duplicate direct-damage dispatch, signed virtual
adjustments, history-limit refusal and recovery. Iteration caught and corrected
one undeclared world-address identifier and two incorrect expectations in the
new API fixtures; the final build and affected tests pass.

The arm64 Release and separately compiled x86_64 Rosetta runners both passed all
116,216 states. Their complete traces compare byte-for-byte, with SHA-256
`922e533eb767bb0ab2b5d680f3f06de91eaae1a70438ac25a5b9869769181dac`.
The vector SHA-256 is
`ef937fd65f3d8200112f02a61b18e2009916effcff4bd8ef2f831246810948ed`.

Commands executed for this continuation (logs and reports are ignored build output):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re > build/skills-a-generator.log 2>&1
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4 > build/skills-a-build.log 2>&1
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure > build/native/undead-skills-ctest.log 2>&1
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace > build/skills-a-x64/undead-build.log 2>&1
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces > build/skills-a-x64/undead-validation.log 2>&1
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace > build/skills-a-sanitize/undead-build.log 2>&1
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces > build/skills-a-sanitize/undead-validation.log 2>&1
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3 > build/native/undead-full-ctest.log 2>&1
git diff --check
```

Only the nine existing Skills files changed: header, source, trace runner, API
suite, generator, verifier, gzip corpus, manifest and this page. Shared build
files, sibling worktrees and the read-only research checkout were untouched.
Private acceptance measurements were skipped because `../../private-validation`
is unavailable. Original tables/initialization, engine lifetime and raw-target
ownership, damage consumers, presentation, Session/playable-scene integration,
console behavior, Norrath, physical Intel and Windows remain unverified.

The arm64 AddressSanitizer + UndefinedBehaviorSanitizer runner also passed all
116,216 states with no findings. Its trace is byte-identical to arm64 Release
and x86_64 Rosetta, with the same complete trace SHA-256 above. Both `cmp`
commands passed. Generator, Release configure/build, affected CTests,
architecture/sanitizer compilation and exact-state verification all completed
successfully. `git diff --check` passed, and the research checkout stayed clean.

The final full Release sweep passed **102/102 tests** in **457.87 seconds**,
with no failures, timeouts or reruns. Both Skills tests passed again inside the
sweep. Full output remains in `build/native/undead-full-ctest.log`.


## Ported family: Poison Rain (24)

All four methods in the pinned Python contract are now implemented in the existing
module: `poisonrain_can_use`, `poisonrain_cast`, `poisonrain_ctor`, and
`poisonrain_run`. Skill virtual identities `0x2f9dec` / `0x2f9a3c` and the object
run identity `0x2f9254` dispatch with signed16 adjusted `this`, preserving explicit
external overrides. Evidence is the same pinned `ref_skills_a.py` lines 1491–1628
and specification §24 cited above. This family gives the future scene a complete
modeled monster area attack, using the existing object constructor, query and
numerical foundation. Hammer of Wrath remains the larger next family with its
item-table adapter and third RNG; none of it is claimed by this continuation.

The constructor preserves full-register levels, unconditional owner linking,
callback-visible rereads, nonplayer delay conversion, absolute player end frames,
all four statistic reads when the light allocation succeeds, exact light arguments,
optional sound and scheduling order. The run preserves strict start gating, signed
expiry, sound/light/deletion ordering, callback-mutated radius loop bounds, captured
base position, three own draws per drop, four per splash and terrain height sampling.
The periodic check rereads the current frame after particles; the box uses truncated
radius, retains returned order and duplicates, captures the owner after collection,
and rereads damage statistics for every enemy. Descriptor values store low16,
including negative and overflowing amounts. Shared and hidden draws remain distinct;
the light hidden draw is counted only after its callback returns.

Terrain, allocation, linking, scheduling, particle, light, sound, query, enemy and
damage services remain mandatory explicit callbacks. No consumer is invented.
Particle loops refuse beyond 65,536 iterations, alongside existing call, store,
query and memory bounds; malformed scalar results refuse with their earlier state
prefix retained. This bounds the portable domain without changing modeled behavior
for covered inputs. Original per-class initialization, monster dispatch, actual
particles, audio, terrain and damage consumer integration remain separate work.

Function accounting recounts 196 declarations by AST (10 top-level functions,
186 class methods, excluding `shape_result` only). The four new full methods raise
the tested count from 152 to 156; partial base object machinery and explicit
refusals are not counted as full ports. All existing 116,216 complete-state rows
are protected as a byte-identical prefix, SHA-256
`ef937fd65f3d8200112f02a61b18e2009916effcff4bd8ef2f831246810948ed`.
New synthetic cases cover all sixteen FPU configurations, signed levels/delays,
owner flags and empty handles, radius/count boundaries, enemy filters/duplicate
queries, allocation failure, signed virtual adjustments, descriptor truncation,
callback mutations and missing/throwing service prefixes. API checks add malformed
scalars, reentry/replacement, explicit overrides, query/loop bounds and recovery.

Private acceptance measurements are skipped: `../../private-validation` is absent
as stated by the task. No private files are sought or used, and the research
repository remains read-only. Standalone contract tests do not establish scene,
Norrath, original console or target-platform compatibility.


### Poison Rain execution evidence

The Release configure/build and affected `skills-a-api` / `skills-a-exact` cases
passed. There are **120,072** complete-state vectors, **3,856** newly appended;
all **116,216** existing rows match the protected prefix byte for byte. The plain
vector SHA-256 is
`5f6cb7a0ebc536feca7f1e089655456e51f6892153fa3ef414b30504425b09f7`.
The arm64 Release and x86_64 Rosetta runners passed all states with matching
trace SHA-256
`a2823df4ece03502751f416a0761be51231138539bfe102b173bb2ac0f7069d4`.

Commands (repository root):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_a_rebuilt.tsv.gz
cmp native/tests/vectors/skills_a.tsv.gz work/skills_a_rebuilt.tsv.gz
cmp native/tests/vectors/skills_a.json work/skills_a_rebuilt.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3
```

During iteration, one unbounded reference stress input was stopped and removed;
a malformed synthetic query result was corrected. The first API stress fixture hit
the memory-history cap before its intended loop guard; isolating that guard fixed
the fixture. These are test-fixture corrections, with no research edits or behavior
changes to existing ports. A temporary fast generator in ignored `work/` reused
the committed prefix for iteration; the normal generator independently recomputes
all rows for final reproducibility verification.

Changed files are the existing skills header/source, trace runner, API tests, vector
generator/verifier, gzip/JSON corpus and this docs page (nine files). Shared build,
roadmap and workflow files are untouched. Further exact work remains portable from
the pin. Physical Intel hardware, Windows, Norrath, console execution and playable
scene/monster integration remain unverified; private acceptance is skipped because
inputs are unavailable.


Normal regeneration and both vector/manifest `cmp` checks passed byte for byte.
The arm64 ASan+UBSan runner also passed all 120,072 cases with no sanitizer
findings and the identical trace hash above. Both trace `cmp` comparisons passed.
The affected skills tests passed again within the full repository sweep.
Architecture/sanitizer logs are `build/skills-a-{x64,sanitize}/poison-validation.log`;
the full test log is `build/native/poison-full-ctest.log`.


The full Release command `ctest --test-dir build/native --output-on-failure -j 3`
passed **102/102** tests in **451.24 seconds**, including `skills-a-api` (0.22 s)
and `skills-a-exact` (69.58 s). No timeout-only reruns were necessary. All final
commands listed above returned success; the earlier premature trace comparison
was repeated successfully after CTest finished rewriting its trace. The research
checkout remained clean. This continuation is complete; the contract as a whole
still has 40 uncounted declarations and further evidence-backed families to port.


## Ported family: Hammer of Wrath (17)

The continuation implements `hammer_can_use`, `hammer_cast`, `hammer_ctor`,
`hammer_spell_ctor`, `hammer_spell_run`, `_hammer_pull`, `_hammer_blow`,
`hammer_run`, `seed3_draw`, `item_tables`, `item_parse` and `item_resource_index`.
The existing module now links the existing `champions-items` parser rather than
adding another item parser. Evidence is the same pinned `ref_skills_a.py`
lines 3091–3495, imported `ref_items.py` contracts, specification §17 and executed
notes. All public data and descriptor strings in the tests are authored synthetic
fixtures. No research file, original table or asset was imported or edited.

Skill identities `0x2efef8` / `0x2efb20`, Spell run `0x2efa04` and object run
`0x2ee068` use existing signed16 virtual adjustments and external overrides.
The cast retains allocation failure prefixes and separate shaft/light hidden draw
accounting. The constructor retains ordered item initialization, the full-register
level-selected descriptor, optional owner linking, level increment before duration
statistics, stale record bytes and the wrapping byte counter. The spell retains
its own shared seed and deletes only when its hammer handle is empty.

Run covers follow/target selection (state 0), timed attack (state 3), spinning
attack (state 4), and the reference's fallthrough for other stored states. It
preserves raycast/enemy order, nearest strict comparisons, counter transitions,
PvP reset, callback-visible handle/statistic rereads, goal writes in z/y/x order,
three orientation springs, velocity wobble, grid update, model matrix generation,
particle sampling/rejection, third RNG writes and expiry. Third RNG draws remain
separate from shared and hidden draw counters. States 1/2 refuse precisely where
the reference itself refuses; no behavior is substituted for a modeled state.

The item adapter lazily decodes supplied memory definitions, presets, prefixes,
suffixes, scanner formats and static strings, then calls the existing item parser.
The table cache is owned by each native Model and can be explicitly reset or supplied
via `item_tables_cache`; this corresponds to an isolated reference table context
(the Python reference holds its cache on the class). Resource indexing reads current
record/definition bytes, including signed rows and conditional variant bits.
Lazy prefix/suffix text, weapon multiplier and quality-row lookups retain current
image reads, including signed category/subtype/row offsets. Supplied cache images
must outlive their users. Record writes retain definition/count/64-bit flags and occupied modifier order;
all padding and empty modifier values remain unchanged. Inlined memory-image and
shared-draw adapters do not count as separate ports of `_ImageExe` / `_SharedDraws`.

`model_transformSamplePoint` receives model, twelve matrix lanes and index; it
returns six lanes (point x/y/z followed by size x/y/z). Other service calls use the
existing callback representation. Allocation, handle links, world queries, damage,
sound, sample points, particles, grid/lifetime services remain explicit callbacks.
Malformed callback results refuse with the prior state prefix retained. Supplied
item strings/tables, query results, particle counts and ball rejection attempts
have 65,536 bounds, in addition to the existing raw32 memory/history limits.
No original scheduling, model renderer or damage consumer is supplied.

The AST accounting retains 10 top-level functions and 186 declared class methods
(excluding only harness helper `shape_result`, counting repeated declarations).
These twelve complete methods raise the ported count to 168; explicit replacements
of reference behavior are not counted. All 120,072 existing vectors are protected
by a byte-identical prefix hash:
`5f6cb7a0ebc536feca7f1e089655456e51f6892153fa3ef414b30504425b09f7`.

Private acceptance measurements are skipped because `../../private-validation`
is unavailable. Scene integration, actual console behavior, Norrath, Windows and
physical Intel hardware remain unverified. The remaining inventory above supports
further exact public-contract work.


### Hammer execution evidence

The Release configure/build and API suite passed. The corpus has **126,775**
complete states, including **6,703** new cases. The normal generator independently
recomputed every row and produced gzip and JSON files byte-identical to the
committed corpus/manifest. All 120,072 previous rows retain their protected hash.
The vector SHA-256 is
`99cf9f3ccaa8ce95d313cec6c7382f9194fcac75569ba498752677b8ab316576`.
The x86_64 Rosetta runner passed all 126,775 states; its complete trace SHA-256 is
`bf32779fc611634c89245a4d6f02753b16eedac90c4bdb58b96be9d68181d786`.
The arm64 runner passed all 6,703 newly appended states before the final sweep.

The first API iteration failed because a new fixture expected deletion flag 1;
the existing/reference lifetime contract uses 3. The fixture was corrected.
The first full Skills comparison then found that synthetic 64-bit table flags had
been encoded in one raw32 input lane, dropping their high half. Splitting only
new-family 64-bit patches into two adjacent raw32 patches fixed the vector inputs;
all old vector rows remain unchanged. No native behavior mismatch appeared in the
corrected Hammer corpus. A superseded full generation was stopped after this input
correction; the final normal regeneration below completed successfully.
An ignored fast generator reused the committed prefix for iteration only; final
reproducibility was checked against normal independent regeneration.

Final commands run from the repository root (all completed commands below passed):

```sh
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re --output work/hammer-final.tsv.gz
cmp native/tests/vectors/skills_a.tsv.gz work/hammer-final.tsv.gz
cmp native/tests/vectors/skills_a.json work/hammer-final.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
build/native/champions-skills-a-trace work/hammer-input.tsv work/hammer-actual.tsv
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/items.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors work/hammer-fast.tsv.gz --manifest work/hammer-fast.json --trace-dir build/skills-a-x64/traces
git diff --check
```

Changed files are the existing header/source, Skills target dependency, trace runner,
API suite, generator/verifier, gzip/JSON corpus and this page (ten files). Common
build files, roadmap, workflows and sibling worktrees were untouched. The research
checkout stayed clean. No sanitizer run is claimed for this continuation. Private
acceptance, physical Intel, Windows, original console, Norrath and playable-scene
integration remain unverified. The final full-sweep result is recorded below.


The final command `ctest --test-dir build/native --output-on-failure -j 3`
passed **102/102 tests** in **459.48 seconds**, without failures, timeouts or
reruns. `skills-a-api` passed in 0.21 seconds and `skills-a-exact` passed in
69.34 seconds; arm64 verified all **126,775** states. The complete arm64 and
x86_64 Rosetta traces compare byte for byte with the identical SHA-256 above.
The final commands also passed:

```sh
ctest --test-dir build/native --output-on-failure -j 3
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
git diff --check
```

The complete sweep log is `build/native/hammer-full-ctest.log`; the x86_64
verification log is `build/skills-a-x64/hammer-validation.log`. Further exact
contract work remains portable; this continuation is **STATUS: MORE**.


## Ported family: Riposte counterattack (9/82) and numeric helpers

The complete `riposte_counter_attack` now uses the pinned `ref_skills_a.py`
contract at lines 1673–1708 and spec §9. Level zero skips all reads, calls and
draws. Slot 12 reads damage then chance with the original level and defender;
the shared chance draw precedes both flash quads and model sampling. A zero
sample count skips the counter-hit and its second shared draw. Otherwise the
routine emits 200 main particles and 100 alternating sparks, preserving signed
sample-count remainder, signed16 virtual adjustment/facing, callback-visible
model/position rereads, and the original empty descriptor/no-critical damage call.
Damage and sample-point services stay required callbacks; no damage consumer or
asset-derived model behavior is invented.

`lcg`, `unit`, `frac`, `round_half`, `spread_max`, `rem`, `quot` and
`half_toward_zero` now have public independently tested native equivalents.
The default spread factor is the exact raw `0x3e4ccccc`. Division widens signed
inputs before computing the quotient/remainder, preserving low32 results at the
INT_MIN/-1 boundary and the reference's reported zero-divisor behavior. These
reported results do not establish original-console execution. All arithmetic
uses the existing integer-pattern FPU model and all sixteen configurations.

The inherited presentation RNG follows the already-pinned `ref_arena_monsters.py`
methods `cosmetic_draw`, `_unit`, and `_cosmetic_ball` (lines 328–332,
1292–1295, 1603–1611). Its seed at `0x404460` changes independently of the shared
seed at `0x404464`; it does not increment shared/hidden draw counts. Each rejected
ball consumes three ordered draws. The existing write-history limit bounds the
rejection loop without fabricating a point. These imported helpers are outside the
progress denominator. None of the previous implementations or vectors is changed.

New synthetic cases cover chance boundaries, signed levels/counts, RNG seeds,
zero sample counts, every reached service's refusal/exception/mutation prefix,
virtual adjustments and all sixteen FPU settings. API checks cover missing/empty
sample returns, callback self-replacement, successful 300-particle emission and
the no-sample recovery path. The manifest/verifier pin the entire prior Hammer
corpus, including all earlier families, before appending these cases.

The generator appended **6,882** cases to the prior **126,775**, producing
**133,657** complete states. The prior corpus SHA-256 remains
`99cf9f3ccaa8ce95d313cec6c7382f9194fcac75569ba498752677b8ab316576`.
The 946 new counterattack cases and direct helper cases all pass the arm64
Release runner. Both affected CTests pass (`skills-a-api`, `skills-a-exact`);
the latter completed in 60.41 seconds. The x86_64 runner under Rosetta passes
all states as well, and `cmp` confirms its trace is byte-identical to arm64.
Both trace SHA-256 values are
`a3a0a54557b841a7825dba55fc93cae16cde32983a8a31b179c1a4e1c2819a03`.

Commands for this continuation (from the worktree root):

```sh
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/items.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
c++ -std=c++17 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/items.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-sanitize/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-sanitize/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-sanitize/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-sanitize/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3 > build/native/riposte-full-ctest.log 2>&1
```

Release configure/build, generator, affected CTests, x64 compilation/comparison
and sanitizer compilation passed. During iteration, the initial API compilation
failed on duplicate local test variables; a block scope fixed it. An affected-test
run started before regeneration completed and rejected the old manifest. The first
new generator run rejected a scalar test callback where a point was required;
its authored fixture now supports mutation plus a three-lane point. Regeneration
and the affected tests then passed. These were corrected local test/harness errors.
The arm64 ASan+UBSan runner passed all **133,657** states without findings,
with the same trace hash as arm64 Release and x64. Both final `cmp` commands
passed after the full sweep recreated the Release trace. An earlier sanitizer
`cmp` attempt returned file-not-found while that sweep was regenerating it.
The full Release sweep passed **102/102** tests in **404.77 seconds**, with no
timeouts or reruns; the skills exact test passed again in 98.08 seconds.
Full output remains in `build/native/riposte-full-ctest.log`. `git diff --check`
passed and the read-only research checkout remained clean.

Only the existing skills header/source, trace runner, API test, vector generator,
vector verifier, gzip corpus, JSON manifest and this docs page changed. No shared
build, roadmap, CI, scene or research files were edited. Further dispatch and
instruction-shape contracts remain portable from the pin, so status is MORE.
Private acceptance measurements were skipped: `../../private-validation` is
unavailable as specified. No private files or research files were modified.
Standalone contract tests do not establish playable scene integration, Norrath,
physical Intel, Windows, or original-console behavior.


## Ported family: shared can-use/mana entry points and instruction shapes

This continuation completes `carries_getter`, `can_use_slot`, `mana_cost`,
`frame` and `is_player`, from pinned `ref_skills_a.py` lines 258–265, 423–425
and 2790–2822. It extends the existing module, trace runner, generator, verifier
and API suite. Only caller-supplied synthetic instruction words and memory are
used; the pin and research checkout remain unchanged.

The getter reader always reads both words, recognizes a JAL getter followed by
the exact documented find-spell call, and returns an optional identity (getter
zero is distinct from no match). Unknown slot-1 bodies use that getter only when
its type identity is supplied. Otherwise the exact constant-one shape is checked,
then the existing explicit callback path. A recognized carries shape takes
precedence over constant-one, retains attached-list order and empty nodes, and
uses the original caster rather than adjusted Skill `this`. Known bodies skip
shape inference; imported Summon Skeleton identities remain known even though
their behavior is still unsupported. Forced external calls skip inference,
retain signed16 `this` adjustment and preserve callback exception prefixes.

`first_skill_call` exposes the original reference dispatch layer separately from
its later can-use override. Both share the existing dispatcher, recursion/history
bounds, known-body implementations and mana-shape reader. The public mana wrapper
retains raw32 full-register levels and delegates slot 5; its slot-14/15 callbacks
retain ordering and mutations. Frame values retain their signed32 bit patterns;
player detection short-circuits null objects before reading flag `0x40`.

Accounting is deliberately conservative: five complete functions increase 177
to 182. The two declared `skill_call` methods remain uncounted because their
known imported Summon Skeleton bodies still need implementation. Generic
`call`, initialization, `vcall` and `table_call` remain uncounted until their
complete inherited behavior is independently tested. Reference refusals are
retained, not replaced with fabricated results. AST accounting still finds ten
top-level functions plus 186 class methods, excluding only harness `shape_result`.

The verifier protects the entire previous 133,657-state corpus before checking
the appended family. New states cover all sixteen FPU settings, optional getter
zero, nonmatching instruction words, shape precedence, getter-known/unknown
branches, attached empty/matching nodes, signed16 adjustments, forced callbacks,
missing services, mutation/exception prefixes, frame sign boundaries, null/player
flags, full-register mana levels and slot-14/15 delegation. API checks also cover
callback replacement, recovery after refusal and exact getter read order.

Private acceptance measurements are skipped because `../../private-validation`
is unavailable. Scene integration, original-console behavior, Norrath, Windows
and physical Intel hardware remain unverified. Further public-contract work is
portable from the pin; the remaining inventory above is the continuation list.

### Shared dispatch execution evidence

The Release configure/build and both affected CTests passed on arm64 macOS;
`skills-a-exact` took **112.85 seconds** while the x64 comparison ran alongside
it. All **140,665** complete states passed, including **7,008** appended cases.
All previous **133,657** states retain the protected SHA-256
`e72ca0521d268ad56fc6a699258a2cdaabae6742d7457ca7f60c8c01ef07ca57`.
Normal generation independently recomputed all rows. An ignored fast generator
that reused only the previous corpus produced a byte-identical gzip; the manifests
also match after excluding only their different generator-file hashes.
The new plain-vector SHA-256 is
`ad32ad315f3daf02a77fe019856bcec7aa7c97989be82ca62d64463b086070e3`.

The x86_64 runner compiled and passed all states under Rosetta. Its complete trace
SHA-256 is `079a8a285a791145cdd16da2e0a3afd103ef7a6b5db5ebe793e3719cd117e7b9`.
No sanitizer run is claimed for this continuation. During generator preparation,
the first run was stopped after spotting that the test's MRO lookup would select
an imported ancestor rather than the first declaration in this pinned file. The
lookup now restricts its selection to the pinned module; the final generation and
exact comparisons passed. No native behavior mismatch was reported.

Commands run from this worktree (all completed commands below passed):

```sh
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
python3 work/dispatch-fast.py --research-dir ../../champions-return-to-arms-re --output work/dispatch-fast.tsv.gz
cmp native/tests/vectors/skills_a.tsv.gz work/dispatch-fast.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/items.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
git diff --check
```

Changed files are the existing header/source, trace runner, API tests, generator,
verifier, gzip corpus, JSON manifest and this docs page (nine files). Shared build,
roadmap, CI, scene and research files are untouched. The full-sweep result and final
trace comparison are recorded below after execution.

The full Release sweep passed **102/102 tests** in **369.82 seconds**, with no
failures, timeouts or isolated reruns. Skills passed again within that sweep
(`skills-a-api`: 0.15 seconds; `skills-a-exact`: 58.10 seconds). The final arm64
and x64 trace `cmp` passed, and both traces have the identical SHA-256 above.
The final commands passed:

```sh
ctest --test-dir build/native --output-on-failure -j 3
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
shasum -a 256 build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
git diff --check
git diff --cached --check
```

The full sweep log is `build/native/dispatch-full-ctest.log`; affected-test and
x64 logs are `work/dispatch-affected.log` and `work/dispatch-x64.log`. The research
checkout remains clean. This family is complete; fourteen declarations remain
uncounted, and further evidence-backed work is available. Status remains MORE.


## Ported family: Summon Skeleton (11) and skeleton-light bridge

This continuation ports the complete imported `summon_can_use`, `summon_cast`,
`summon_spell_ctor`, `summon_spell_run` and `_summon_sample` (native
`summon_sample`), plus the local `skelight_ctor` callback bridge. Evidence is the
unchanged pin: `ref_skills_a.py` lines 2448–2467, `ref_arena_monsters.py` lines
1584–1730, and `skills-01-34.md` section 11. Both reference Skill dispatch layers
now resolve the summon can-use/cast identities, and Spell dispatch resolves the
summon run identity with signed16 adjustments. No per-class constructor or
statistic table is invented.

Can-use preserves player-slot refusal (unsigned slot >= 4), pointer-table reads,
monster shared names and the explicit name-query result. Cast preserves the exact
flash write order, 32 opening plus 200 following particles, per-sample model
rereads, signed sample counts including INT_MIN, presentation-ball rejection and
cosmetic seed writes. Direct sampling with zero count is outside the reference
call domain and explicitly refuses after the model call and cosmetic draw. Terrain
height and free-spot selection are callbacks; the rounded height difference is compared as an IEEE double in the reference, so the
native magnitude comparison is independent of the emulated FPU comparison mode
and excludes IEEE NaNs. Free-spot callback results flatten to x/y/z/found; only
found's low signed byte controls replacement.

The first shared draw occurs after terrain callbacks, before strength lookup and
allocation. Property-list selection rereads caster fields after the statistic
callback. Successful skeleton construction increments `hidden_draws` only after
its explicit service returns; missing/throwing services retain the prior prefix.
Elemental selection consumes its additional shared draw even without a holder.
Player charm preserves the full 64-bit flag word and ordered team/charm/range
callbacks. Sound still occurs when skeleton allocation fails. Spell allocation
failure returns zero without deleting the skeleton. The imported Spell base has
a distinct write order, including its temporary base table, and is implemented
explicitly instead of changing existing `spell_base` vectors. Empty skeleton
handles invoke the Spell destructor with argument 3; nonempty handles stay alive.

New named services are `world_findObjectByName`, `world_findFreeSpot`,
`Skelight_ctor`, `Creature_setCharm`, and Creature slots 34/37. All services,
including original skeleton construction and terrain search RNG/lifetime, remain
explicit callbacks. The bridge never supplies the imported skeleton's creature
implementation. Missing callbacks refuse; callback mutation, replacement and
exception prefixes follow the existing module's limits and representation.

### Accounting and verification boundary

AST accounting remains ten top-level functions plus 186 declared class methods,
excluding only harness `shape_result`: 196 units. This family adds one fully
ported declaration, `skelight_ctor`, increasing 182 to 183. The five inherited
lifecycle/sample methods are independently tested but are not declarations in
this pinned file and add nothing to the denominator. Both declared Skill dispatch
layers now have their known summon bodies; they remain conservatively uncounted
until complete generic behavior is tested. The remaining thirteen declarations
are listed in the continuation inventory above.

The generator appends authored synthetic states and the verifier protects all
140,665 preceding states with SHA-256
`ad32ad315f3daf02a77fe019856bcec7aa7c97989be82ca62d64463b086070e3`.
Cases cover all sixteen FPU settings, player/monster/invalid slots, name-query
results, allocation failures, chance/seed boundaries, signed counts, terrain
thresholds/NaNs, found-byte truncation, charm flags, constructor ordering, Skill
and Spell adjustments, and missing/mutating/throwing callbacks. API tests check
constructor callback replacement, hidden-draw success boundaries and spell
expiry refusal/recovery.

Private acceptance is skipped: `../../private-validation` is unavailable and no
private files were sought elsewhere. This standalone contract port does not
verify Norrath, original console behavior, scene integration, Windows or physical
Intel hardware. Research files and shared build/roadmap/CI files remain untouched.
Execution results follow.


### Summon Skeleton execution evidence

Release configuration/build and the affected API/vector CTests passed on arm64
macOS. The affected sweep passed 2/2 tests in 65.79 seconds (`skills-a-exact`:
65.66 seconds). All 143,969 complete states passed, including 3,304 appended
states. The prior 140,665-state prefix remains byte-for-byte identical. The new
plain-vector SHA-256 is
`ab44e6f3340ec0c21bfa4521730c7a183fb76c3131a3342c0129dcee5a3379cc`.
The generator independently recomputed every state and its recorded source hash
matches the final generator.

The x86_64 runner compiled and passed every state under Rosetta. Its trace and
the arm64 Release trace compare byte-for-byte, both SHA-256
`26f08f5c31d3c903f249ea549b6528c8602d8c8f98804e3fa74b9b23bb991c24`.
No sanitizer execution is claimed for this continuation.

A preliminary scratch generation/trace check failed on sixteen direct sample
cases: its adapter passed negative sample counts as unsigned Python integers.
The native cast already used signed counts; the generator now converts direct
sample inputs with `s32`, matching that call domain. Final full generation and
both architecture comparisons passed. Two preparation generation processes were
interrupted while fixing test adapters; no interrupted run is counted as passing.

Commands completed successfully (full-sweep result recorded below):

```sh
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/items.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
shasum -a 256 build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
git diff --check
```

Changed files: this page, `native/include/champions/skills_a.hpp`,
`native/src/skills_a.cpp`, `native/tools/skills_a_trace.cpp`,
`native/tests/skills_a_tests.cpp`, `tools/generate_skills_a_vectors.py`,
`tools/test_skills_a_vectors.py`, `native/tests/vectors/skills_a.tsv.gz`, and
`native/tests/vectors/skills_a.json`. Logs are under ignored `work/`:
`summon-generator.log`, `summon-build.log`, `summon-affected.log`,
`summon-x64-build.log`, `summon-x64-test.log`, and `summon-full-ctest.log`.


### Full sweep, verifier memory work and final timeout boundary

The full Release sweep `ctest --test-dir build/native --output-on-failure -j 3`
returned failure: 101/102 passed in 1,126.47 seconds. Only `skills-a-exact`
timed out (CTest recorded 390.92 seconds); it reported no state mismatch.
The required isolated rerun also timed out in 121.67 seconds. These results do
not replace the earlier successful affected CTests or arm64/x64 exact comparison.
No timeout setting or other agent process was changed.

A subsequent uncapped run was interrupted after more than twenty minutes while
still preparing vectors, before launching the native runner. The existing
verifier retained twenty historical corpus copies and repeatedly split the full
1,907,510,627-byte plain corpus. A same-prefix benchmark measured 36.399 seconds
for the original split/join/hash versus 16.014 seconds for a view/hash; both
produced the same protected 140,665-state digest. The verifier now scans line
boundaries once, computes every historical prefix hash incrementally, and
streams input preparation and expected/actual comparison. It retains the plain
corpus plus line offsets and individual records, instead of retaining all prefix,
input, expected and actual copies. This changes verification storage, not vectors,
coverage, callbacks, native inputs or the compared state bytes.

Both intermediate optimized affected reruns still timed out under the unchanged
120-second limit (124.03 seconds with incremental hashes; 121.74 seconds after
streaming). The streamed run reached the native runner. API tests passed in both
runs. No performance guarantee under concurrent machine load is claimed.
The final direct streamed comparison and trace comparison are recorded below.

Additional commands and outcomes:

```sh
# FAIL: one timeout in the full sweep; all other 101 cases passed.
ctest --test-dir build/native --output-on-failure -j 3
# FAIL: skills-a-exact timeout.
ctest --test-dir build/native -R '^skills-a-exact$' --output-on-failure -j 1
# FAIL: skills-a-exact timeout; skills-a-api passed, with the optimized verifier.
ctest --test-dir build/native -R '^skills-a-' --output-on-failure -j 1
```

The timed runs are `work/summon-full-ctest.log`,
`work/summon-exact-rerun.log`, `work/summon-optimized-rerun.log`, and
`work/summon-streamed-rerun.log`. The synthetic prefix benchmark is
`work/summon-prefix-benchmark.json`. The uncapped pre-optimization run was
interrupted; an intermediate view-only affected run was also interrupted while
consolidating hashes. Neither is counted as a completed passing run.


The final direct run of the streamed verifier passed all 143,969 complete states;
its SHA-256 is the same `26f08f5c31d3c903f249ea549b6528c8602d8c8f98804e3fa74b9b23bb991c24`.
The final arm64 trace again compares byte-for-byte with the independently compiled
x64 trace. Thus every state and historical prefix remains verified with the final
verifier, and each of the 102 test cases has passing execution evidence (the
skills cases in the initial affected sweep/direct runs; the other 101 cases in
the full sweep). The full sweep itself failed and later isolated CTests still
timed out; no passing final CTest timing gate is claimed. Machine-load timing
and the 120-second gate remain an explicit limitation. Further native contract
work is portable from the pin: status MORE, thirteen declarations still uncounted.

Final successful commands:

```sh
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
python3 tools/test_skills_a_vectors.py --native build/native/champions-skills-a-trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/native/core-traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
git diff --check
git diff --cached --check
```

The final verifier log is `work/summon-streamed-direct.log`; its exact report
is `build/native/core-traces/skills_a_report.json`. The research checkout remained
clean, and no private acceptance measurements were run.


## Ported family: item image/RNG adapters and damage descriptors

This continuation completes six units: `descriptor`, `_ImageExe.__init__/read/cstr`
(native `ImageExe`), and `_SharedDraws.__init__/next` (native `SharedDraws`). Evidence
remains the pinned `ref_skills_a.py`, lines 176–186 and 3091–3116. AST accounting
still finds ten top-level functions plus 186 class methods, excluding only harness
`shape_result`. The manifest qualifies the adapter methods to distinguish their
constructors from Model initialization. Six complete units increase 183 to 189;
the seven uncounted declarations and next-family suggestions are listed above.

`ImageExe` owns a synchronous load callback without opening a file or reading
during construction. `read` loads ascending bytes, including NUL/high bytes;
zero/negative sizes perform no reads. `cstr` excludes its terminating zero.
Both observe live memory and loader replacement between reads, preserving load
exception prefixes. The active callable is copied before invocation so it can
replace itself safely. Existing item-table strings and descriptor text now use
this reader. Raw32 address spans, 65,536-byte reads and the existing 65,536-read
string cap are explicit portable limits; out-of-domain/nonbyte results refuse.
No address wraps or fabricated terminators are supplied.

`SharedDraws` delegates to its currently bound Model's live shared draw operation,
including seed writes, counters and exceptions. Item parsing's existing consumed-
draw replay now uses the adapter; the item module's nonvirtual RNG and parsing/
replay order remain unchanged and protected by the historical vectors. No engine
service or hidden draw is introduced. Callers must keep the bound Model alive.

`descriptor` returns four optional slots in caller order, truncating supplied lanes
to low16. Empty supplied entries remain present; absent entries remain empty.
Like Python, entries retain their length, including lengths other than four.
More than four entries refuses. This canonical value is distinct from writing an
item record; the helper performs no memory writes or RNG calls.

### Item-adapter verification

The generator independently recomputed **145,409 complete states**, including
**1,440 appended states** across all sixteen FPU settings. Every preceding
**143,969 state** remains byte-for-byte identical, protected by SHA-256
`ab44e6f3340ec0c21bfa4521730c7a183fb76c3131a3342c0129dcee5a3379cc`.
The final manifest's generator hash matches the final generator. New corpus SHA-256:
`4b77d8478c3a800f44c93ff9db509c3a543cdca383e8736b3ac2dfc7f9c1c4d5`.

API tests additionally cover lazy construction, ordered loads, loader replacement,
callback-visible byte changes and exceptions, address/string bounds, independent
Model bindings/rebinding, live seed replacement, history-limit failure/recovery,
and empty versus absent descriptor slots. A scratch selection also independently
checked all 1,440 new states on both architectures before full-corpus verification.

Release configuration/build and the affected CTests passed on arm64 macOS (2/2
in **36.67 seconds**, exact test **36.48 seconds**). The independent x86_64 runner
passed all states under Rosetta. Its full trace compares byte-for-byte with arm64;
both have SHA-256 `3433e9bb1f398077b2c0e4b447109072a7726cd57fb9f025eac435d8c4d31c30`.
The final full Release sweep passed **102/102 tests** in **331.90 seconds**, with
no failures, timeouts or isolated reruns. Skills passed again inside that sweep
(API **0.17 seconds**, exact **37.23 seconds**), retaining the same trace hash.
No sanitizer execution is claimed for this continuation.

Commands (all PASS):

```sh
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/items.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git diff --cached --check
```

Changed files are the existing header/source, trace runner, API tests, generator,
verifier, gzip corpus, JSON manifest and this page (nine files). No parallel module,
dependency or shared build/roadmap/CI changes were added. Logs are under ignored
`work/`: `adapters-generator.log`, `adapters-configure.log`, `adapters-build-final.log`,
`adapters-api-final.log`, `adapters-affected.log`, `adapters-x64-build.log`,
`adapters-x64-test.log`, and `adapters-full-ctest.log`. Exact reports are under
`build/native/core-traces/` and `build/skills-a-x64/traces/`.

The research checkout remains clean. Private acceptance is skipped because
`../../private-validation` is unavailable; no private files were sought elsewhere.
Original console behavior, Norrath, playable-scene integration, Windows and physical
Intel hardware remain unverified. The selected six-unit family is complete;
further work is portable from the pinned public evidence: status MORE.


## Ported family: Model initialization and generic dispatch

This continuation completes six declarations: original Model `__init__`, `call`,
`vcall`, `skill_call`, `table_call`, and the later `skill_call` override. Evidence
is the same pinned Python, lines 215–259 and 2808–2820, with the delegated
`ref_arena_monsters` and `ref_creature` interfaces pinned by the manifest.
Native Model construction owns supplied synthetic Memory and service maps,
initializes counters/histories, and translates forced Skill names into slot flags;
it performs no memory reads, writes or service calls. Callback lifetime follows
the existing owned native representation.

Object `vcall` now resolves this file's OBJECT_BODIES before delegation, even
when a Creature external name is present, exactly as the original dispatch layer.
Inherited Creature/arena bodies remain delegated services, never imported code.
General `table_call` always delegates for non-Spell classes, even if the supplied
function happens to identify a known object body. Spell calls recognize modeled
bodies at any caller-supplied table offset, with signed16 wrapping of adjusted
this. Unknown functions and forced Spell/Skill paths retain explicit callbacks.
The original Skill layer and its later can-use shape override remain separately
callable and tested. Slot-5 shape dispatch remains mandatory even when external.

Named callbacks preserve empty, scalar and multiple result lanes, live replacement,
reentrancy and exception prefixes. Missing callbacks refuse explicitly; the
reference harness's default result synthesis is not an engine implementation.
No default service results are invented. Existing history/depth/raw32 limits
remain bounded portable-domain refusals. The existing API checks now distinguish
object calls from general Creature table callbacks; no parallel module was added.

Private acceptance measurements are skipped: `../../private-validation` is absent.
Norrath, console behavior, scene integration, Windows and physical Intel execution
remain unverified. The single uncounted constructor needs the new evidence named
in the continuation inventory above.

### Generic-dispatch verification

The generator recomputed **149,569 complete states**, appending **4,160** synthetic
states across all sixteen FPU settings. All **145,409** preceding states retain
SHA-256 `4b77d8478c3a800f44c93ff9db509c3a543cdca383e8736b3ac2dfc7f9c1c4d5`.
The verifier checks this immutable prefix and the 195/196 AST accounting. The new
vector SHA-256 is `0537634d9def4db8daa996cc441a7e0a12be449f3b2c2b24e55418f0cc116288`.

The arm64 Release affected tests passed **2/2 in 34.97 seconds**. The independent
x86_64 runner under Rosetta passed all states, and `cmp` confirmed identical traces:
SHA-256 `6977791fa8f8edf43f12c2439dc17f2d38fe3db43e9ee6237008a4bba1451ee7`.
API tests additionally cover supplied Model memory/services, forced Skill routing,
empty/multiple callback lanes, active replacement, known object dispatch despite
external flags, generic table delegation, recursion refusal and recovery.
No sanitizer execution is claimed in this continuation.

Commands executed (PASS unless noted below):

```sh
python3 -m py_compile tools/generate_skills_a_vectors.py tools/test_skills_a_vectors.py
python3 tools/generate_skills_a_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-a-api$' --output-on-failure
ctest --test-dir build/native -R '^skills-a-' --output-on-failure
c++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/items.cpp native/src/skills_a.cpp native/tools/skills_a_trace.cpp -o build/skills-a-x64/skills_a_trace
python3 tools/test_skills_a_vectors.py --native build/skills-a-x64/skills_a_trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir build/skills-a-x64/traces
cmp build/native/core-traces/skills_a_actual.tsv build/skills-a-x64/traces/skills_a_actual.tsv
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git diff --cached --check
```

The first build failed on a duplicate default-constructor declaration introduced
in this continuation; it was removed. Subsequent Release builds and API checks
passed. Logs are under ignored `work/dispatch-*.log`; exact reports are in
`build/native/core-traces/` and `build/skills-a-x64/traces/`.
Nine existing files changed: this page, skills header/source, API tests, trace
runner, vector generator/verifier, gzip corpus and JSON manifest. No shared
build/roadmap/CI files or research files changed. No push or external publication.

The final full Release sweep passed **102/102 tests in 386.31 seconds**, with no
failures, timeouts or isolated reruns. Skills passed inside the sweep as well
(API 0.17 seconds, exact 40.53 seconds), preserving the same full trace hash.

## Final constructor unit: documented GameObject domain

The normal native `gameobject_ctor` now follows the addressed constructor
supplement from read-only research worktree `t3/research-anim` at commit
`2502e2bfac58203ac0174fccd7e0953b36d06439`. Pinned SHA-256 values:

| Source | SHA-256 |
|---|---|
| `docs/spec/gameobject-constructor.md` | `8e858b73b98b0ffe3ba314d875dca789ea8a28debcf878e5c0f38ac02129779c` |
| `tools/ref_gameobject_ctor.py` | `fad9fce5bacae43bab0d0d2be376963b335e2d2a6c9c704776b651e0bbd6b305` |
| `tools/test_ref_gameobject_ctor.py` | `0ba986167704bf9d5b9946ece4dd8f0f340b1c3232a2c1ba3480aa280670e1ca` |
| `docs/spec/research-anim-handoff.md` | `192189db8e4339411ac5e7b41c7f8d368acb7afe8f534a1532ce0c7b2a0ddd82` |
| existing `tools/ref_skills_a.py` | `6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae` |

The implementation preserves the ordered prefix, callback-visible class
definition reread, grid flags and signed bounds, stale `hide` lookup, peer-node
unlink/reinsert, property publication before the shared draw, repeated ordinal
name searches, in-place quote edits, signed hash and tombstone probing. Missing
`res_find` and `handle_link` services refuse after completed writes. The finite
ASCII, terminated string, valid list, non-overlapping memory domain and host
budgets are the supplement's explicit limits. Arbitrary aliasing, malformed
lists, full-table original nontermination and C integer overflow remain outside
the supported contract; the full table refuses after quote and count writes.

`native/tests/skills_a_ctor_tests.cpp` ports synthetic cases from the pinned
reference tests, including incoming versus stale hide, nonempty and off-grid
relocation, negative coordinates, property key mutation, duplicate/quoted names,
tombstones, full-table prefix, resource mutation and a missing service. The
normal public constructor uses this behavior. The immutable 149,569-row corpus
and its 195-unit manifest still describe the earlier restricted research model;
the trace runner sets `legacy_constructor_vectors` solely while replaying those
historical rows. This keeps their exact snapshots unchanged without using that
mode in the new constructor test or normal Model construction. The combined
evidence credits `gameobject_ctor` as the final unit.

The concurrent `skills-spell-kernel` branch owns shared RNG and constructor-tail
support. This branch changes only Skills-A constructor code, tests and docs and
uses the already integrated draw primitive. Re-run affected tests after merging
any later shared-primitive change. Private acceptance inputs remain unavailable,
and no console, Norrath, Windows, scene or physical Intel behavior is claimed.
