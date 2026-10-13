# Return to Arms skills 69–101: statistics, passives and shared spell runtime

`champions-skills-c` is a standalone portable C++17 subset of the RTA skills
contract. It supplies shared statistics, mana and damage-range calculation, and
all six passive helpers, base spell construction, independent spell/object random
streams, unit-ball sampling, visual position and paired handle lists. It has no
Godot dependency or Session integration.
It reuses the existing raw-pattern core arithmetic and sparse `creature_hp::Memory`
without changing either module. No native Creature HP behavior is invoked.

## Evidence

Research repository: `champions-return-to-arms-re`, exact commit
`356add27d50e50972b2a671b31e8f4ec93e6778c`.
The checkout was read only. The authored reference and its dependencies have
these SHA-256 identities, all retrieved from that commit's Git objects:

| File | SHA-256 |
|---|---|
| `tools/ref_skills_c.py` | `9e673475a0b89292e483075d76b2f277d1251ce820a0956053e68c83faa81e91` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `tools/ref_creature.py` | `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7` |
| `tools/ref_arena_monsters.py` | `84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16` |
| `tools/difftest_skills_c.py` | `e61c7c70e63632f8ff6de8c393fe967315355c96f32ff50a3370c8868bac4712` |
| `docs/spec/skills-69-101.md` | `b2e97932bb9b8976990ea1eb1d2d18c6baf84087ac05044c56b67d59c47f3ed0` |
| `docs/spec/skills-69-101-executed.md` | `3a1248945e21b1af3e895141e003d7fc5fbb9563036b796dd452f874f24e3a73` |

[The reference](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_skills_c.py),
[written specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/skills-69-101.md),
and [executed corrections](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/skills-69-101-executed.md)
are the behavioral sources. The last describes research instruction-interpreter
comparisons, not console execution. Its correction to randomized slot 8 is
preserved: the first draw selects rounding direction; only the second sets the
bound. The research differential harness was inspected for its proof boundary;
it was not run here and no original executable or private input was loaded.
The authored translation follows the repositories' MIT license.

## Implemented subset

Call `skills_c::Model::execute(Operation, args)` with canonical unsigned raw32
arguments; the statistics/passive operations below take the skill address first.
Negative integers use their two's
complement pattern. Floats are raw32 patterns; returned values are raw32 too.
The caller supplies the skill object's two five-statistic tables in memory. No
retail tables or static initializers are embedded. The same shared bodies can
serve all 33 ids; this does not initialize or implement each active class.

| Operation | Arguments following skill address | Reference method |
|---|---|---|
| `Stat` | statistic, level | `skill_get_stat` (10) |
| `Tier2` | statistic, level, cap | `skill_get_stat_tier2` (11) |
| `ForCaster` | statistic, level, caster, cap | `skill_stat_for` (12) |
| `ForHandle` | statistic, level, handle-word address, cap | `skill_stat_for_handle` (13) |
| `Adjust` | level, n | `skill_adjust_level` (14) |
| `ManaFromStat4` | level, n | `skill_mana_from_stat4` (15) |
| `Range` | statistic, level, randomise, spreadRaw | `skill_stat_range` (8), returns two integers |
| `CanUseDefault` | caster | `skill_can_use_default` (1) |
| `CastDefault` | level, caster, position-placeholder, facing | `skill_cast_default` (2), returns zero |
| `ManaDefault` | level | `skill_mana_default` (5), returns 1.0 |
| `ManaCost` | level | `skill_mana_cost`, explicitly calls slot 15 with n=20 |
| `StatisticSpread` | statistic, level, owner | `statistic_spread`, returns two integers |
| `RageSwing` / `RageDamage` | rank, player-record address, frames | id 71 swing/damage factors |
| `Complement` | rank | ids 73/74 weight complement |
| `Throwing` | rank | id 79 fraction |
| `BattleMage` | rank | id 92 fraction |
| `PenetratingArrows` | rank | id 93 two fractions |

Tier 1 is never capped. Tier 2 clamps positive caps and falls back to tier 1
when its signed first level is negative. Zero level returns before table access.
Randomized ranges consume four shared draws at `0x404464`; nonrandomized ranges
consume none. Player damage ranges round and floor at one; nonplayer ranges
truncate with no floor. Rage calls adjustment before testing HP, clamps only the
upper end of its frame ramp, and writes eight halfwords in reference order only
for the swing helper and a nonzero player object. Rank zero does not bypass that
flash path. Damage, inventory weight and projectile consumers remain external.

## Shared spell runtime and paired handle lists

This continuation ports the complete shared runtime family from `Base`: `spell_base`,
`spell_unit`, `spell_ball`, `obj_unit`, `obj_ball`, `visual_pos`, `handle_link`,
`handle_remove`, `handle_clear`, and `handle_items`, including internal `_h_take`
and `_h_unlink_free`. It also ports inherited `spell_rand` and `obj_rand` from the
same pinned authored dependencies. This family prepares active spell lifecycles;
no active class is added or implicitly dispatched.

The following operations append to the existing enum (numbers 18–29); all old
operation numbers, inputs and observations stay unchanged. The first argument is
the named spell/object/head/node address rather than necessarily a skill.

| Operation | Arguments | Result |
|---|---|---|
| `SpellBase` | spell, level, owner, skill, table | empty |
| `SpellRand` / `ObjectRand` | spell / object | unsigned high 16 bits of updated seed |
| `SpellUnit` / `ObjectUnit` | spell / object | raw float |
| `SpellBall` / `ObjectBall` | spell / object | three raw floats |
| `VisualPosition` | creature | three raw floats |
| `HandleLink` | head-word address, back-word address, object | front node |
| `HandleRemove` | node | removed object |
| `HandleClear` | head-word address | empty |
| `HandleItems` | head-word address | flat alternating node/object words |

`SpellBase` writes the narrowed level byte, skill, owner, base table, age zero,
one shared draw as the private seed, and finally the supplied derived table in
reference order. Private seeds at spell+0x10 and object+0x44 use the documented
LCG; their draws do not increment the shared counter. Balls use three private
draws per attempt and the exact left-associated float rejection comparison.
Visual position preserves zero/signed-zero offset handling, signed16 facing,
ordered angle arithmetic and height at +0x3c.

Handle lists use the caller-supplied free list at 0x864ea8. Taking a nonzero node
clears its +0x10 halfword. Each link takes two nodes and connects both fronts in
reference order. Removal returns the object and frees the partner before the
front node; clearing traverses front to back and leaves reference stale fields
intact. Exhaustion deliberately preserves reference address-zero writes; no
replacement allocator or success guarantee is introduced. `HandleItems` refuses
cycles or more than 65,536 nodes; mutating traversals and sampling are bounded by
the existing write-history limit. These are host resource refusals.

The reference's string sentinel `head == 'stack'` delegates link/clear to an
external callback. The raw32 API accepts only memory-backed head addresses;
stack-local handle operations are explicitly unsupported, never coerced to zero.
Spell destruction/virtual plumbing and other engine services remain separate.

All operations use the caller's existing thread-local FPU context; no host
floating-point arithmetic, engine physics, replacement constants or automatic
RTA/Norrath profile selection is used.

## Callback and refusal boundaries

`skill_call` reads the table at skill+0x60, signed16 this adjustment at slot*8,
and body address at slot*8+4. Adjusted this wraps to raw32. It dispatches only the
ten base bodies listed in `ref_skills_c.py::SKILL_BASE`. Other bodies, including
per-class mana wrappers, delegate to an explicitly installed `callbacks[slot]`.
A slot in `external` forces a callback even for a known base body.
Callbacks receive adjusted-this followed by unsigned canonical arguments and
return one raw32 value, or two for ranges. They can change memory, replace their
handler or throw. Calls are logged before invoking a handler; missing handlers
throw `Unsupported`. Copies of active callables preserve self-replacement safety.
No zero-valued world service is silently substituted. External callback failures
preserve their memory changes and prior call/write history without rollback.

The memory domain is raw32 spans with missing bytes reading zero; out-of-domain
spans refuse through the reused Memory error type. Sparse memory and its store
log inherit that class's documented limits. Model writes/calls each cap at 65,536;
combined operation/virtual dispatch depth caps at 64 and restores after failure.
These limits are explicit host refusals, not original game limits. This subset
has no overlay integration tests. Concurrent use of one Model and destroying it
from callbacks are outside the lifetime contract.

`cast_or_run(id)` always throws `Unsupported`. `CastDefault` is the explicitly
named base no-op, never an active-class cast router.

## Tests and transport

The generator creates 168 authored scenarios across all 16 core FPU
configurations (four add models, multiplication-bias and division-rounding flags),
for 2,688 vectors. It covers signed/zero/extreme levels, negative bases and first
levels, tier fallback, caps, players/monsters/null owners, empty handles and
nodes with a null object, zero maximum HP, negative frame ramps, random draws,
complements beyond 100%, every passive helper, and delegated scalar/range slots.
The 48 added scenarios cover construction and byte narrowing, independent RNG
streams, repeated rejection sampling, zero and nonzero visual offsets, signed
facing, empty lists, pool exhaustion, paired front/back ordering, removal, clear
and reuse. All original 1,920 complete records remain byte-for-byte identical;
the runner independently verifies their original corpus hash. Callback cases
include forced and unknown-body routes, signed this adjustment,
mutation and mutation followed by a raised exception.

Every observation includes the initial state and, after every command, status,
return patterns, all sorted sparse bytes, the complete memory store log, model
write log, ordered callback log and argument vectors, draw count, external slots
and callback configuration. The seed is part of the full byte snapshot. Native
code receives inputs only; Python expected state never reaches it. The runner
checks fixture hashes, all observation tokens, empty/malformed input and an input
mutation. API tests separately check active-class refusals, missing handlers,
callback self-replacement, recursion refusal and depth recovery.

The text protocol is `config TAB input TAB expected`, with a header
`# skills_c_vectors_v1`. Integers are canonical nonnegative decimal. Every list
starts with its element count, then recursively encoded elements. Input is
`[initial_pokes, external_slots, callback_configs, commands]`; pokes are
`[address,width,value]`, callback configs are `[slot,return_vector,poke_or_empty,
throw_flag]`, commands are `[operation_number,args]`. Operation numbers follow the
header enum. Observation is `[initial_snapshot,[status,return_vector,snapshot],…]`;
a snapshot is `[byte_pairs,memory_log,model_writes,calls,draws,external,configs]`.
Calls are `[slot,args]`, writes `[address,width,value]`, status 0 means success,
and status 1 means the authored callback raised. Refusals are tested separately.
The native stdin transport omits the expected column and prints observations.

Regenerate from immutable Git objects; imports occur only in ignored scratch
under `work/`, never from the live research working tree. Manifests retain all
source, generator and plain-vector SHA-256 values. Ordinary CTest needs neither
the research checkout nor any game files.

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

## Exactly what remains

All active class can-use/cast/constructor/run/hit/particle/query methods remain:
69 Retribution; 70 Unspoken Word; 72 Summon Axe; 75 Summon Arrows; 76 Roar;
77 Battle Cry; 78 Blood Lust; 80 Pus Storm; 81 Critical Hit; 82 Riposte counterattack;
83 Dodge fraction and zero mana cost; 85 Root; 86 Healing; 88 Drowsy;
89 Poison Bolt; 90 Summon Badger; 91 Raise Army; 94 Spirit Shield; 95 Wave of Flame;
96 Frost Armor; 97 Disease Blade; 98 Fire Axe; 99 Cold Axe; 100 Lightning Strike;
101 Cone of Shock. Id 84 Cyclone's class is owned by `ref_skills_b.py`; id 87
Tagar's Insects is inherited from `ref_arena_monsters.py`; neither class is ported.
Per-class mana body dispatch, statistics/static initialization, rank-description
and presentation functions remain, even where the generic shared mana helper is
available. The remaining Base presentation/plumbing helpers (virtual spell
dispatch/destruction, type lookup, record lookup, lights, flash, model/attachment
sampling, sound and callee stores) are unported. Stack-local handle callbacks
remain unsupported; memory-backed lists and the shared spell runtime above are
ported.

Everything behind reference callbacks stays unimplemented: damage application,
world queries, object/effect factories, animation, sounds, particles, inventory,
creature virtual services and original player dispatch. Integrating existing
native modules needs its own contract and tests. Actual game-content tables,
console fidelity, Norrath compatibility, Session/scene integration and Windows
execution remain unverified. This subset is not a playable port.

## Local verification

The generator command above passed, producing 2,688 vectors from verified pinned
sources. Primary Release configure/build passed, and the scoped skills CTest
command passed 2/2. The runner verifies all original 1,920 records against their
unchanged original SHA-256. Python syntax checks and `git diff --check` passed.

The x64 commands also passed (scoped CTest 2/2):

```sh
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
```

Both arm64 and x86_64 macOS independently emitted all 2,688 complete-state
observations with SHA-256
`ad26114e75e84197f3769e92d397c2b06902b124bf084d858363dc8dc5caa048`,
identical to the pinned Python expected observations. The x64 executable ran
through Rosetta on Apple Silicon; this does not verify an Intel host or Windows.
Public inputs remain authored synthetic fixtures only; the research checkout
and private game inputs were not modified.
The full Release command `ctest --test-dir build/native --output-on-failure -j 3`
completed with 100/102 passed in 2,808.84 seconds. The two failures were solely
600-second subprocess timeouts in `session-turn-transport` and
`session-animation-transport` while sibling worktrees were testing concurrently;
no state mismatch was reported. Both subsequently passed one at a time (2/2,
515.78 seconds total) with:

```sh
ctest --test-dir build/native -R '^(session-turn-transport|session-animation-transport)$' --output-on-failure -j 1
```

Thus every registered test passed, including both skills tests in the full run.
The initial full run was not wholly green; these isolated retries resolve its
two timeout results. Runtime under shared machine load is not a benchmark.
Console fidelity, Windows execution, Session/scene integration and Norrath
compatibility remain unverified. This is a standalone contract port.
