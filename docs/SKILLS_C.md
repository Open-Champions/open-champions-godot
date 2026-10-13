# Return to Arms skills 69–101: shared runtime, protection and Blood Lust

PROGRESS: 67 of 175 reference functions ported

`champions-skills-c` is a standalone portable C++17 subset of the RTA skills
contract. It supplies shared statistics, mana and damage-range calculation, and
all six passive helpers, base spell construction, independent spell/object random
streams, unit-ball sampling, visual position and paired handle lists. It also supplies the complete Retribution, Spirit Shield and Frost Armor families.
It now includes all of Blood Lust (78), its shout setup/refund helpers and callee stores.
It has no Godot dependency or Session integration.
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
these earlier runtime operations do not implicitly dispatch active classes.

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
and body address at slot*8+4. Adjusted this wraps to raw32. It dispatches the
ten base bodies listed in `ref_skills_c.py::SKILL_BASE`. The three protection classes and Blood Lust additionally dispatch their can-use/cast/mana bodies.
Other bodies, including
other per-class mana wrappers, delegate to an explicitly installed `callbacks[slot]`.
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

`cast_or_run(id)` remains an unsupported id-based integration router and always
throws `Unsupported`. The three protection families and Blood Lust have explicit named operations. `CastDefault` is the explicitly
named base no-op, never an active-class cast router.

## Caster protection and retaliation family

The previous run added every method in `Retribution`, `SpiritShield` and `FrostArmor`:
can-use, cast, constructor, per-frame run, healing/absorption/retaliation and
`_spiritshield_particles`. That is 16 reference methods, including both particle
modes in the single Python helper. Eight supporting Base methods are also now
available: `find_spell_by_type`, `is_player`, `record`, `flash`, `light_effect`,
`model_sample`, `sound_at`, and `spell_destroy`. The same pinned contracts above
are the evidence; no new game data or dependencies were introduced.

Operations 30–45 follow those three classes in that order (see the header enum).
Can-use takes `[skill,caster]`; cast takes
`[skill,level,caster,unused-position,unused-facing]`; constructor takes
`[spell,level,caster,skill]`. Retribution run takes
`[spell,x,y,z,unused-facing]`, heal takes `[spell,damageRaw]`.
Shield and armor run take `[spell,unused-position,unused-facing]`;
shield particles take `[spell,progressRaw,dissolveBoolean]`, absorb takes `[spell]`,
and armor hit takes `[spell,attacker,damageRaw]`.
Operations 46–53 expose the eight supporting methods in the order listed above.
`LightEffect` takes block, three position words, follow, flag, four colour words,
four secondary colour words and four stack float words. `ModelSample` takes
owner, model, three position words and index. `SoundAt` takes lump, name-address
and three position words. `Flash` takes object, quad and four colours.

Service ids 16–34 append to the existing callback registry (ids 0–15 retain their
Skill-slot meanings), in this order:

```text
typeinfo, creature_findSpellByType, pool_new, light_link, res_find,
sound_playAtPos, particleGroup_create, particleGroup_begin,
fx_emitFromModelVerts, particleGroup_end, light_relink, Spell.v1,
LightEffect_ctor, Creature.v16, model_getSamplePointCount,
model_getSamplePoint, particle_spawn, damage_apply, fx_spawn
```

Arguments flatten fixed-signature vectors in place into unsigned raw32 words.
The `damage_apply` descriptor expands into 18 header bytes (sixteen zero bytes,
two 0xff bytes) and four four-halfword slots; unused slots are `(0xffff,0,0,0)`.
This preserves the reference's descriptor information without engine-owned item
storage. Callback result vectors hold scalar returns, three sample coordinates,
or no result for void services. `sound_at` forwards its callback's result.
Every missing service refuses after logging the call; no synthetic default world
or presentation service is supplied. A caller must explicitly install callbacks.
Creature-model lookup and destruction retain signed16 this-adjustment and raw32
wrap. The three new Skill can-use/cast/mana bodies dispatch through `skill_call`;
other class bodies still delegate. General `table_call` remains incomplete and
is not counted as ported.

Retribution's constructor leaves +0x58 untouched. Its heal flashes seven shorts,
leaves +0x98 untouched, returns zero for a player with nonpositive HP, and writes
then clamps HP only above maximum. Shield phases 1–3 skip duration checks; flags,
progress arithmetic, deadlines, sound and shared absorb draws preserve reference
order. Frost Armor emits ten particles then one from the second definition,
with ball draws before the sample-index draw. Retaliation always draws once for a
nonzero attacker, and uses `trunc(incoming damage - factor)` narrowed to halfwords,
as confirmed by executed correction SC4. LightEffect/damage/presentation callee
random draws and memory effects belong to callbacks and are never synthesized.
A zero sample count refuses at the reference division-by-zero point, after its
preceding private draws; no fallback point is invented.

## Blood Lust family (78)

This run ports all nine methods in `BloodLust`, plus the complete
`Roar.shout_animation`, `Roar.shout_interrupted` and `Base.callee_store` helpers.
The same pinned reference and executed corrections are the evidence. The
constructor makes exactly one shared draw, narrows its rank to signed8 for
statistics, starts the style-selected shout animation, and stores prediction-copy
hand outputs with callee pokes (no model or memory-store log entries). The run
preserves animation activation, action-state loss, half-mana refund with upper
clamp, deadline equality, owner-state restoration and destruction ordering.
Armour complement and damage multiplier return 1 while inactive; active
percentages are deliberately not clamped, including complement below zero.

The full presentation path is implemented as a contract: soft-double pulse,
four hand glows, local-detail gating, squared-motion thresholds at 4/25/100,
every-fourth-frame nearly stationary trails, drip counts and ten streamer pairs.
Private draws/rejection loops and 8-byte-plus-4-byte vector stores preserve
reference order. The end fade overrides the start fade when both apply; negative
fades remain negative. Doubles, texture words, animation resources and positions
come from caller memory or callbacks; no game-content values are supplied.
Network mode and local-slot callbacks set only the documented force-rebase byte.

Operations 54–65 append without renumbering earlier operations:

| Operation | Arguments |
|---|---|
| BloodLustCanUse | skill, caster |
| BloodLustCast | skill, level, caster, x, y, z, facing |
| GhostHands | owner; returns six words, first hand then second |
| BloodLustCtor | spell, level, owner, skill |
| HandStreamers | spell, definition, first hand xyz, second hand xyz, last0 offset, last1 offset |
| BloodLustRun | spell, unused position, unused facing |
| BloodLustGlow | spell, first hand xyz, second hand xyz |
| BloodLustComplement / BloodLustMultiplier | spell |
| ShoutAnimation | owner, default name, flags, name count, style/name pairs, event count, slot/seconds/mask triples |
| ShoutInterrupted | spell, owner, sound-present boolean, sound-handle offset |
| CalleeStore | address, byte width, low32 value, high32 value |

Shout style keys and event indices use signed raw32 patterns. Masks for all
events are stored before any event times. Empty names/events are supported.
CalleeStore covers the existing Memory domain (0–8 bytes); larger spans are
host refusals. The virtual Blood Lust cast accepts the existing raw position
**address** argument, reads three words, and passes them to the explicit cast.
General Skill/Spell dispatch remains incomplete and uncounted.

Service ids 35–42 append in this order:

```text
net_objOrGhost, model_getAttachPoint, libc_sprintf, anim_startBlendInOut,
net_isLocalSlot, Player_animStep, glow_add, sound_stop
```

AttachPoint returns exactly three raw coordinates. StringFormat takes the name
address and an explicit zero marker for Python's None argument. AnimationBlend
takes state, clip, flags and two blend floats. GlowAdd takes xyz, three colours,
opacity, texture, signed turn, 3, size and reach. All other signatures flatten
in reference order; missing or malformed service results refuse explicitly.
Animation, prediction-copy selection, formatting, sound, glows and particles
remain callbacks. Callback memory mutations and failure prefixes are preserved.

### Progress inventory

The denominator is an AST count of the immutable `ref_skills_c.py`: five top-level
functions and 170 directly declared class methods, including internal behavioral
helpers. None are test/harness helpers. Inherited methods from other files are
excluded. `shape_result` is a runtime callback-shaping function and remains in the
denominator. The numerator contains only complete tested bodies:

- 12 original shared statistics/default/mana methods; `_round`, `_tier_value`.
- Six original passive entry methods and `_rage`.
- `spell_base`, `spell_unit`, `spell_ball`, `obj_unit`, `obj_ball`, `visual_pos`,
  `_h_take`, `_h_unlink_free`, `handle_remove`, `handle_items`.
- All 16 methods of the three protection classes and the eight Base methods above.
- All nine Blood Lust methods; `shout_animation`, `shout_interrupted`, `callee_store`.

`handle_link` and `handle_clear` have tested memory-backed behavior but their
stack-local callback branches still refuse; neither counts as fully ported.
`skill_call`, `table_call`, general `call`, and inherited `spell_rand`/`obj_rand`
are not included in the numerator. Counts are reference units, not operation,
vector, skill-id, machine-function or playable-feature counts.

## Tests and transport

The generator creates 417 authored scenarios across all 16 core FPU
configurations (four add models, multiplication-bias and division-rounding flags),
for 6,672 vectors. It covers signed/zero/extreme levels, negative bases and first
levels, tier fallback, caps, players/monsters/null owners, empty handles and
nodes with a null object, zero maximum HP, negative frame ramps, random draws,
complements beyond 100%, every passive helper, and delegated scalar/range slots.
The 48 added scenarios cover construction and byte narrowing, independent RNG
streams, repeated rejection sampling, zero and nonzero visual offsets, signed
facing, empty lists, pool exhaustion, paired front/back ordering, removal, clear
and reuse. All previous 2,688 complete records remain byte-for-byte identical;
the runner independently verifies both earlier corpus hashes (1,920 and 2,688 records). Callback cases
include forced and unknown-body routes, signed this adjustment,
mutation and mutation followed by a raised exception.

The 106 protection scenarios exercise all 24 added operations, player and
nonplayer owners, null/successful allocation, signed level narrowing, independent
RNGs, negative/zero sample counts, particle rejection, unclamped negative progress,
all shield phases and deadline sides, absorption chance/duration gates, healing
and upper HP clamp, retaliation descriptor narrowing, and mutation-before-failure
at every one of the 19 service boundaries. API tests add all nine new virtual
Skill-body routes, malformed sample results, missing services, self-replacement,
service recursion and recovery.

The 143 Blood Lust scenarios cover every new operation, signed rank narrowing,
all five style selections and fallback, inactive/active factors, animation events
and flag loss, interruption and refund, expiry equality, signed turns and local
network/detail gates. They also exercise start/end fades (including negative
values), trail-distance thresholds and nearly-still frame gating, drip and
streamer draws, general shout event ordering and empty event lists, callee
store widths 0, 1, 2, 3, 4, 5, 7 and 8, and callback mutation/failure prefixes. API checks cover all
three new virtual Skill bodies, position-pointer transport, malformed hand
returns, callback replacement, recursion/recovery and callee-store provenance.
All 4,384 protection-era records remain independently hash-pinned alongside
the original 1,920 and shared-runtime 2,688 corpora.

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

The remaining active class can-use/cast/constructor/run/hit/particle/query families are:
70 Unspoken Word; 72 Summon Axe; 75 Summon Arrows; 76 Roar (shared shout helpers now ported);
77 Battle Cry; 80 Pus Storm; 81 Critical Hit; 82 Riposte counterattack;
83 Dodge fraction and zero mana cost; 85 Root; 86 Healing; 88 Drowsy;
89 Poison Bolt; 90 Summon Badger; 91 Raise Army; 95 Wave of Flame;
97 Disease Blade; 98 Fire Axe; 99 Cold Axe; 100 Lightning Strike;
101 Cone of Shock. Id 84 Cyclone's class is owned by `ref_skills_b.py`; id 87
Tagar's Insects is inherited from `ref_arena_monsters.py`; neither class is ported.
Per-class mana body dispatch outside the three protection classes and Blood Lust, statistics/static initialization, rank-description
and presentation functions remain, even where the generic shared mana helper is
available. The remaining Base helpers are general `call`, complete `skill_call` and
`table_call` dispatch, `light_record`, and `attach_point`.
Top-level `shape_result`, `c_div`, `c_rem`, `desc`, and `item_record` remain
unported as complete general functions; their inlined narrow uses are not counted. Stack-local handle callbacks
remain unsupported; memory-backed lists and the shared spell runtime above are
ported.

Everything behind reference callbacks stays unimplemented: damage application,
world queries, object/effect factories, animation, sounds, particles, inventory,
creature virtual services and original player dispatch. Integrating existing
native modules needs its own contract and tests. Actual game-content tables,
console fidelity, Norrath compatibility, Session/scene integration and Windows
execution remain unverified. This subset is not a playable port.

## Previous protection-run verification (2026-10-13)

The pinned generator command above passed with 4,384 complete-state vectors
(274 scenarios × 16 FPU configurations). Both older corpora (1,920 and 2,688
records) retain their exact hashes; the runner verifies them independently.
Release configure/build and arm64 scoped CTest passed 2/2. The x64 commands
below passed 2/2 as well:

```sh
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
```

Both architectures matched all Python expected observation words, including
store widths/order and the complete callback/failure history. Their complete
observation stream SHA-256 is
`f2e06bf4ca9636e20941b4866f1fcdea440af9cac3802407baa2658b74c54b5a`.
The independently pinned plain-vector corpus SHA-256 is
`4e674c59477c73c879536a39516ad8112ee27ed3cee592a915a9a8c1155dffd4`.
The x64 executable ran through Rosetta on Apple Silicon; this does not verify
an Intel host or Windows. Python syntax checks and `git diff --check` passed.

The required full Release command
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102**
in 1,872.97 seconds, including both skills tests. There were no failures or
timeout retries. Runtime under shared machine load is not a benchmark.
Public inputs remain authored synthetic fixtures only.
Research and private game inputs were not modified. Console fidelity, Windows,
Session/scene integration and Norrath compatibility remain unverified.

## Blood Lust continuation verification (2026-10-13)

The immutable generator passed with 6,672 complete-state vectors (417 authored
scenarios × 16 FPU configurations). The reference AST count was independently
rechecked: five top-level functions plus 170 declared class methods, with no
test/harness helpers. The 12 new complete tested functions raise the numerator
from 55 to 67; inherited RNG methods, partial routers and stack-list refusals
still do not count. The research checkout was read only; no private game files
were loaded as validation inputs or modified. Only authored synthetic fixtures
were used.

Release configure/build passed, as did arm64 scoped CTest (2/2, 53.19 seconds).
All earlier complete records retain their independent hashes (1,920, 2,688 and
4,384 records). Python syntax checks and `git diff --check` passed. The new
plain-vector corpus SHA-256 is
`96fdf5384ea18d7d7b710c455c71a1860ce7cc23a15560960f540156406b29d3`.
Both architectures matched every expected observation token; their shared
complete native observation stream SHA-256 is
`50bbfd15f2161b79672d4345bc07ff89ffeef6a56d48dae362c24851039a714b`.

The x64 Release configure/build commands in the previous section also passed
for this continuation, followed by scoped CTest (2/2, 60.91 seconds). The exact
full command `ctest --test-dir build/native --output-on-failure -j 3` passed
102/102 tests in 1,548.66 seconds, with no failures or timeout retries. The
optional real-game decoder validation remained skipped; this suite does not
establish private game-content acceptance. Rosetta execution does not verify
an Intel host or Windows. Console fidelity, original animation/presentation services,
Session/scene integration and Norrath compatibility remain unverified.
