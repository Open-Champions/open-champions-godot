# Return to Arms skills 69–101: shared runtime and active families

PROGRESS: 175 of 175 reference functions ported

The [RTA Skill arithmetic kernel](SKILLS_KERNEL.md) now supplies the shared
statistic/tier and LCG primitives; this module retains its memory, rounding,
range, callback and failure contracts. No new port credit or scene integration.

Current continuation family: **complete inherited Skeleton Summon lifecycle and reachable construction helpers**.
All 175 units declared in the pinned skills-C reference are now implemented.
The two virtual dispatchers include both inherited arena skill/spell families.
Completion is within the explicit canonical value, callback and host domains below.

`champions-skills-c` is a standalone portable C++17 subset of the RTA skills
contract. It supplies shared statistics, mana and damage-range calculation, and
all six passive helpers, base spell construction, independent spell/object random
streams, unit-ball sampling, visual position and paired handle lists. It also supplies the complete Retribution, Spirit Shield and Frost Armor families.
It now includes all of Blood Lust (78), its shout setup/refund helpers and callee stores.
It now includes all nine SharedClasses methods for Riposte (82), Dodge (83) and Healing (86).
It now includes the entire SummonItems family (Summon Axe 72 / Summon Arrows 75)
and the general canonical `item_record` value builder.
It now includes all seven CriticalHit methods (81, shared with 33/53) and the
general `light_record` helper.
It now includes all six ThrownAxes methods for Fire Axe (98) and Cold Axe (99).
It now includes the entire BattleCry family and body-model attach-point helper.
It now includes the complete Storms, Disease Blade, Raise Army, Drowsy, Wave of Flame, Roar, Unspoken Word, Poison Bolt, Summon Badger, Root and Cone of Shock families.
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
external callback. `HandleLinkStack` and `HandleClearStack` explicitly select
that branch with a required zero marker. Ordinary `HandleLink` / `HandleClear`
continue to treat every raw32 head, including zero, as memory-backed. No stack
address or temporary allocator is invented.
Spell destruction/virtual plumbing and other engine services remain separate.

All operations use the caller's existing thread-local FPU context; no host
floating-point arithmetic, engine physics, replacement constants or automatic
RTA/Norrath profile selection is used.

## Callback and refusal boundaries

`skill_call` reads the table at skill+0x60, signed16 this adjustment at slot*8,
and body address at slot*8+4. Adjusted this wraps to raw32. It dispatches the
ten base bodies listed in `ref_skills_c.py::SKILL_BASE`. The three protection classes, Blood Lust, SharedClasses and SummonItems additionally dispatch their supported can-use/cast/mana bodies. ThrownAxes adds both can-use/cast/mana sets. BattleCry adds its three Skill bodies. Storms adds both cast/mana pairs. Disease Blade and Raise Army add their can-use/cast/mana bodies. Drowsy adds cast/mana. Wave of Flame and Roar add can-use/cast/mana. Unspoken Word and Poison Bolt add cast/mana. Summon Badger and Root add can-use/cast/mana. Cone of Shock adds cast/mana.
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
throws `Unsupported`. The ported active families have explicit named operations. `CastDefault` is the explicitly
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
other class bodies still delegate. General `table_call` was incomplete at this milestone; the final Skeleton
continuation below completes its inherited dispatch dependencies.

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

## Shared defensive and recovery family (82, 83, 86)

This continuation ports the entire nine-method `SharedClasses` mixin: Riposte's
no-op cast and counterattack, Dodge's mana and fraction, and Healing's cast,
constructor, amount/flash helpers and run. Evidence is the same immutable Python
contract plus the linked written/executed skills notes; no retail tables or
private inputs were used. The denominator was rechecked with Python AST: five
top-level functions plus 170 class methods. No inherited method, partial router
or refusal is newly counted.

Riposte does nothing at rank zero. At nonzero rank it asks both statistics and
makes one shared draw before the chance test. A successful test flashes both
quads before asking the model count; zero points stop before cosmetic draws or
damage. Nonzero counts execute all 200 cosmetic sample/ball steps (including
negative-count signed remainder), then make the second shared draw and delegate
the damage descriptor. Sample scale is exactly ONE, independent of owner +0xdc.
The cosmetic seed is 0x404460; its writes never increment shared draw count.
Dodge mana asks slot 14 even though it returns zero; rank-zero fraction has no
calls, and nonzero fractions are not clamped.

Healing constructs the base spell, object sound, optional GlowingShaft callback
and particle group in reference order. Rank is narrowed to signed8 before
statistics. Player maximum HP comes from its record, nonplayer maximum from its
object. Per-frame amounts preserve `(max * share) * (percent * 0.01)` association.
Live nonplayers use the supplied box query and ally callback, measure 2-D
radius, and write object HP even above maximum. Players use the supplied alive
list and optional team callback, measure 3-D radius, and gate record HP writes
on `hp + 1 <= maximum`; all nearby players still flash during the initial window.
Neither path clamps the result. The particle path uses a prediction-copy
position, original core matrix math, private draws and inclusive terrain-height
bounds; the clock destroys only strictly above the documented threshold.

Operations 66–74 append in the header order:

| Operation | Arguments |
|---|---|
| RiposteCast | skill, level, caster, unused position, unused facing |
| RiposteCounterAttack | skill, victim, attacker, rank |
| DodgeManaCost / DodgeFraction | skill, level or rank |
| HealingCast | skill, level, caster, unused position, unused facing |
| HealingCtor | spell, level, caster, skill |
| HealingAmount | maximum HP raw32, percent signed raw32 |
| HealingFlash | spell, object |
| HealingRun | spell, unused position, unused facing |

Service ids 43–49 append: `sound_playOnObject`, `GlowingShaft_ctor`,
`world_collectObjectsInBox`, `Creature.v21`, `Creature.v22`,
`particleGroup_setMatrix`, `terrain_heightAtPoint`. Box-query returns are flat
object-address vectors, including duplicates and an empty vector. Creature
virtual services retain signed16 this adjustment. SetMatrix transports the
reference's literal `stack` marker as an explicit zero marker; it conveys no
matrix bytes or real pointer. Matrix computation remains local to the particle
coordinates just as in the Python contract. All missing services refuse and
preserve prior calls and writes. The alive-list host limit is 65,536 entries;
it is a host refusal rather than a game limit. General dispatch remains partial.

## SummonItems family (72 / 75)

Selected continuation family: all nine `SummonItems` methods, plus top-level
`item_record`. The same pinned reference and written/executed notes establish
these bodies. Inventory capacity, giving, equipping, descriptor parsing, sound,
light construction and particle services remain explicit callbacks. No actual
inventory implementation, item-table contents or unwritten stack bytes are
invented. This also preserves the previously pending SharedClasses continuation
in the same existing files.

Axe item names follow signed8 rank boundaries 5 / 9 / 13 / 17 / 21. Quantity and
modifier values narrow to halfwords; the general item value retains arbitrary
slot lengths, empty slots and more than four supplied slots exactly like Python.
The constructor uses the caster's player slot without an is-player gate, checks
load plus weight inclusively, then gives/recalculates/equips/resets in order.
Bag narrows to signed8; the returned inventory index is signed32. Their final
address argument wraps to raw32. Overweight items trigger voice, reread the
player record, refund mana with an upper clamp, and mark the start frame -1.
Callback changes and exceptions retain all prior calls and writes.

The run frees/destroys immediately at start -1. Otherwise it asks model count
and prediction copy even after the emission window, emits through age 30 with
two private draws per particle, and frees/destroys strictly above age 120.
Negative and wrapping frame ages, negative/zero sample counts and signed sample
remainders preserve the reference behavior. The zero-divisor failure preserves
the preceding private draw. Group matrix callbacks receive the same explicit
zero `stack` marker used by Healing; no matrix pointer or bytes are fabricated.

Operations 75–84 append without changing any existing operation:

| Operation | Arguments |
|---|---|
| SummonArrowsCanUse | skill, caster |
| SummonAxeCast / SummonArrowsCast | skill, level, caster, xyz, unused facing |
| SummonCast | configuration, skill, level, caster, xyz |
| SummonAxeBuildItem | level, quantity, plain |
| SummonCtor | configuration, spell, level, caster, skill |
| SummonAxeRun / SummonArrowsRun | spell, unused position, unused facing |
| SummonRun | configuration, spell |
| ItemRecord | definition, quantity, supplied-slot count, (length, halfwords)… |

Configuration 0 selects Axe, 1 selects Arrows; other values refuse before
mutation. ItemRecord returns the canonical value transport
`[definition, quantity16, 18 header bytes, slot count, (length, halfwords)…]`.
Missing slots up to four retain the reference's one-halfword `(0xffff,)` shape;
no padding is added inside supplied slots. SummonAxeBuildItem returns this value
followed by its definition. The public `item_record` C++ function supplies the
same value directly. Unwritten bytes, including record +6, are absent; the
transport is not a serialized 0x48-byte game record.

Service ids 50–57 append: `item_parseDescriptor`, `player_itemWeight`,
`inventory_giveItem`, `player_recalcLoad`, `inventory_equipItem`,
`player_resetIdleAnim`, `player_voice`, `particleGroup_free`. Parser takes
`name, variant, 0` and returns the definition pointer. Weight takes `slot, item
value`; GiveItem takes `slot, item value, 1`. Others flatten reference arguments
in order. Malformed scalar results and missing services refuse. Skill virtual
body routing adds both casts, both mana wrappers and Arrows' can-use; virtual
casts dereference their position address before the explicit xyz operation.
General Skill/Spell routers remain partial and uncounted.

## CriticalHit combat-buff family (81, shared with 33/53)

All seven methods of the pinned `CriticalHit` class are complete: can-use,
cast, mana, constructor, tint, run and bonus query. `Base.light_record`, required
by construction, is also complete and tested as a general explicit operation.
The cast does not adjust rank; construction narrows it to signed8 through the
base spell and consumes one shared draw. Engine type lookup, allocation, lights,
particles, terrain height, ghost lookup, team checks and rank queries are explicit
callbacks. Missing services or malformed scalar results refuse after the
observed call prefix; callback mutation is retained on failure.

The spell decrements its remaining time before expiry, tints the owner and
strictly nearby players, follows/fades the light through age two and unlinks it
on every later live frame. All three class ids choose their documented particle
first level; unknown ids use one. Each ground particle uses one private draw.
Signed radius products remain wide for the positive gate and narrow at the
reference's FPU conversions/squared-int conversion. No null filter is invented
for alive-list objects. Bonus queries preserve list order, the strict 1000-unit
3-D gate, PvP checks, rank calls in order 53/81/33, this skill's aura statistics,
and final adjustment even when no source grants a level. Negative best ranks
retain the reference's nonzero test and stop-with-zero behavior when in radius.

These operations append to the existing transport; all earlier numbers retain
exact meaning:

| Number | Operation | Arguments |
|---|---|---|
| 85 | CriticalHitCanUse | skill, caster |
| 86 | CriticalHitCast | skill, rank, caster, unused position, unused facing |
| 87 | CriticalHitManaCost | skill, rank |
| 88 | CriticalHitCtor | spell, rank, caster, skill |
| 89 | CriticalHitTint | spell, creature |
| 90 | CriticalHitRun | spell, unused position, unused facing |
| 91 | CriticalHitBonus | skill, attacker, rank |
| 92 | LightRecord | light, four colours, radius, falloff, extra, xyz |

Service ids 58/59 append `light_unlink(light)` and
`player_getSkillRank(slot, id)`. `Creature.v22` keeps the existing signed16 this
adjustment from the creature's table. The Skill router adds the three published
CriticalHit bodies. Cast's position pointer is unused and is not dereferenced.
General Skill/Spell routers remain partial and uncounted.

## ThrownAxes family (98 / 99)

All six methods are ported: Fire Axe and Cold Axe can-use/cast, plus their shared
`_axe_can_use` and `_axe_cast`. Nonplayers pass can-use immediately; players
require equipment slot 2 and item flag bit 42, otherwise explicit text/HUD
callbacks display the configured warning. Cast itself has no player gate.

The cast adjusts through slot 14 (20 for fire, 2 for cold), flashes both quads,
asks for the light before the sound, then gets speed and hand position. A target
uses original core atan/square-root math and no shared draws; no target uses
exactly two shared draws, even when no primary weapon is equipped. Light and
projectile callee effects, including their own random draws, remain callbacks.
Both resource-index calls are retained and may return different model rows.

The callback copies mutable fields from the primary weapon in the signed8 active
set into the skill object's wrapped per-player item address. The first empty or
matching modifier gets its element, narrowed value, rounded 1.2 value and 600
frames; a full unrelated modifier array stays unchanged. Projectile callbacks
receive that shared item pointer and the captured damage range, rather than an
invented item copy. Slot, stack count and equipment re-reads follow the reference
across callbacks. Low-ammunition notices cover counts 1–20 before decrement;
prediction copies skip decrement but still test removal with signed16 count.
Removal scans only the 64 bag-zero entries and retains callback mutations.

| Number | Operation | Arguments |
|---|---|---|
| 93 / 94 | FireAxeCanUse / ColdAxeCanUse | skill, caster |
| 95 | AxeCanUse | cold boolean, caster |
| 96 / 97 | FireAxeCast / ColdAxeCast | skill, rank, caster, xyz, unused facing |
| 98 | AxeCast | cold boolean, skill, rank, caster, xyz |

Service ids 60–66 append, in order: `text_lookup`, `hud_showMessage`,
`item_getResourceIndex`, `item_copyMutableFields`, `playerProjectile_ctor`,
`hud_countActiveNotices`, `inventory_removeItem`. Fixed-signature arguments
flatten into unsigned raw32 words in reference order. The projectile call has
20 words, with xyz position and velocity, all stack arguments and the item
address. AttachPoint (existing id 36) now also accepts the reference's request for
both output vectors: its result must contain six raw words (point then direction);
its existing point-only calls still require three. Direction is unused by the
cast. Missing callbacks and malformed scalar/vector results refuse explicitly.
The six published Skill bodies dispatch through signed16 this adjustment; casts
read the supplied position pointer. Neither projectile flight/hit nor inventory,
item-copy, HUD, light, sound or model engine behavior is synthesized.

## BattleCry: shout, stun and swing-speed aura

Operations 99–105 implement every method in `BattleCry`: can-use, cast,
constructor, `_battlecry_streamers`, tint, per-frame run and affects query.
Can-use is `[skill,caster]`; cast is `[skill,level,caster,unused-position,unused-facing]`;
constructor is `[spell,level,owner,skill]`; streamers is `[spell,has-definition,definition]`
(the boolean distinguishes Python `None` from definition zero); tint is
`[spell,creature]`; run is `[spell,x,y,z,unused-facing]`; affects is
`[spell,creature,want-fraction]` and returns `[covered,fractionRaw]`. The reference
computes and returns the fraction even when the output-pointer flag is zero.
Operation 106 implements complete `Base.attach_point` as
`[owner,index,has-model,model]`, returning three raw floats. Explicit model zero
is preserved separately from an omitted model.

The constructor retains one shared draw, narrowed rank, ordered clock/aura/action
stores, all three animation-header events, the sound handle and both callee hand
stores. Streamers use body attach points and the existing exact private ball
sampler; a null definition updates hand positions without consuming random draws.
The run retains interruption refunds and sound stop, event ordering, expiry,
network rebase byte, private particle draws, rotated ground queries, inclusive
three-dimensional aura tinting and first-cry stun filters. The affects query
always covers the original owner; other creatures require the cry and radius,
with the documented PvP callback gate. No game tables are supplied implicitly.

Services 67/68 append `Creature.v20` (enemy query, scalar) and `Creature.v30`
(stun, void), with signed16 this adjustments at the existing creature table.
All engine services remain callbacks, including animation, sound, particle groups,
terrain, world collection and stun application. Existing attach-point callback
uses retain their original three- or six-word shapes; this complete helper
requires exactly three words. Missing or malformed results refuse after the
logged callback prefix. No Scene or Session integration is claimed.

## Storms continuation

All eight methods of `Storms` are implemented from the same pinned contract:
Lightning Strike and Pus Storm casts/runs, their three shared bodies, and
Pustule construction. These are standalone operations; engine object creation,
scheduling, terrain, destruction, enemy queries, damage and Lightning Bolt
construction remain explicit callbacks. `Pustule_run` is outside this reference
and remains unsupported. No implicit GameObject constructor stores or shared RNG
draws replace the callee callback.

| Number | Operation | Arguments |
|---|---|---|
| 107 / 108 | LightningStrikeCast / PusStormCast | skill, rank, caster, xyz, facing |
| 109 | StormCast | variant, skill, rank, caster, xyz, facing |
| 110 | StormCtor | variant, object, centre xyz, velocity xyz, frames, interval, low, high, caster, stepsRaw |
| 111 / 112 | LightningStrikeRun / PusStormRun | object |
| 113 | StormRun | variant, object |
| 114 | PustuleCtor | object, start xyz, ground xyz, drift xyz, angle |

Variant 0 selects Lightning Strike; 1 selects Pus Storm. Other variants refuse.
Operations and services append without changing previous numbers. Services
69–74 are `GameObject_ctor(object, xyz, 0, 0)`,
`terrain_height(world, xyz, radiusRaw)`, `obj_runEveryFrame(object)`,
`obj_updateGridCell(object)`, `Creature.v1(adjustedObject, 3)`, and
`LightningBolt_ctor(block, xyz, 17, 0)`. Virtual Skill casts transport the
caller-supplied position pointer as xyz; both published mana wrappers use the
existing generic helper. Object destruction and enemy queries preserve signed16
this adjustments. Complete general virtual routers remain uncounted.

The casts preserve Pus Storm's slot-8 damage range for every caster and its
sound-before-allocation order; Lightning Strike uses `statistic_spread` and
plays sound after allocation/construction. The constructor keeps warning-light
allocation, handle linking and scheduling order. Pus Storm computes its fall
time with the configured FPU square root of `0x45160189`; it separates spawning
and damage end frames and positions its light behind the moving object.

Runs preserve signed frame comparisons, expiry/empty-owner destruction,
movement/grid/light order, flicker/fade, object-private ball rejection and draw
order, signed interval scheduling, inclusive damage boundaries every 30 frames,
query order/duplicates, signed odd integer damage halves and narrowed descriptor
fields. Interval zero uses the reference's dividend remainder; its console
fidelity is explicitly unestablished by the executed research notes. Callback
mutations and failure prefixes are retained without rollback.

## Disease Blade: weapon modifiers and refresh aura

All six `DiseaseBlade` methods are implemented: can-use, cast, `player_weapons`,
`write_disease`, constructor and run. Operations 115–120 follow that order.
Can-use takes `[skill,caster]`; cast takes `[skill,rank,caster,ignored-position,ignored-facing]`;
weapon selection takes `[player]` and returns two item addresses. Modifier writes
use `[item,value,frames,limit]`, with signed raw32 frames/limit at the public
boundary. Constructor takes `[spell,rank,caster,skill]`; run takes
`[spell,x,y,z,ignored-facing]`. Internal duration multiplication keeps the full
signed 64-bit product before narrowing stores, matching Python's wide limit
comparison. All float arithmetic uses the existing FPU context.

The first disease slot's signed value controls replacement, even if an earlier
empty slot receives the actual write. The old value must be nonnegative, less
than the frame-count limit, and at most the new value. Flag bit 24 skips an item.
There is no invented modifier eviction, timer ticking or on-hit damage behavior.
Weapon selection retains signed active-set indexing and raw32 player-record wrap.
Constructor and aura pass the captured two weapon addresses in original order.

Aura refresh uses the supplied position and an inclusive engine box query every
30 signed simulation frames, while frame is below signed expiry. It preserves
query order, duplicate results, player filtering and PvP team delegation. Expiry
clears a nonplayer creature's holder element before destroying the spell.
Callback mutation re-reads, partial writes and failure prefixes are retained.

The three published Skill bodies dispatch via `skill_call` (can-use `0x318730`,
cast `0x318764`, mana `0x318aa0`). New service ids 75/76 are `Holder.v7` and
`Holder.v8`: adjusted holder followed by `[1]` or `[1,0]`. Table pointer is at
holder+0, with signed16 this adjustment. Missing handlers explicitly refuse;
original element construction, its internal RNG draw and element clearing remain
caller responsibilities. This adds no complete general table router. The light,
sound, world collection and team services reuse existing callback ids.

## Raise Army (91)

All seven reference methods are exposed through appended operations 121–127.
`RaiseArmyCanUse` takes `[skill,caster]`; `RaiseArmyCast` takes
`[skill,rank,caster,x,y,z,facing]`; `RaiseArmyIsRaisable` takes `[spell,creature]`;
`RaiseArmyCtor` takes `[spell,rank,caster,skill]`; `RaiseArmyRun` takes
`[spell,unused-position,unused-facing]`; `RaiseArmyRaiseInRing` takes
`[spell,ringRaw]`; `RaiseArmyDress` takes `[spell,corpse,newCreature]`.
The three Skill bodies (`0x3134b0`, `0x3134e4`, `0x3137a4`) use the existing
virtual dispatcher; its cast reads three raw words through the supplied position
pointer. Existing operation and callback identifiers retain their values.

Eligibility preserves every object/creature/death-animation gate, including the
high-half creature flag, and calls the supplied boss-rank service. Construction
uses one shared draw, tier-1 radius and a capacity-256 box query; eligible
entries are linked at the head in reverse query order. Run computes the original
float duration ratio, emits 206 particles using only the spell RNG, raises
reached candidates and advances the clock. Zero-duration and unusual float
behavior follows the existing exact arithmetic context. The reached test uses
2-D square root followed by subtraction of the unsquared object radius.
Missing/stale candidates, failed factories and noncreature results preserve the
reference removal/count behavior and signed16 count/limit comparison.

Dress preserves HP flooring, ordered 64-bit flag stores, the unguarded caster
slot byte, charm/team, signed top-track index and animation arguments, two-hand
model/element copying, prefix short circuit, variant/hide-mask reconstruction,
conditional tint and corpse sinking. It re-reads callback-mutable fields where
the reference does. A missing service refuses after its logged prefix; a callback
failure retains prior writes. Constructors and services supplied by callbacks
remain responsible for their own random draws and state mutations.

New service ids 77–87, in order: `creature_bossRank`, `world_createObject`,
`Creature_setCharm`, `Creature.v37`, `anim_pushWithWeights`, `Creature.v27`,
`Holder.v15`, `Holder.v16`, `Holder.v1`, `str_prefixDiffers`,
`model_buildHideMask`. Vectors flatten in signature order. Creature/holder
services receive signed16-adjusted raw32 this pointers. The hide-mask callback
returns exactly `[low32,high32]`; malformed shapes refuse. Existing `Holder.v8`,
model, sound, light, particles, box and run-list callbacks are reused. Original
factories, animation/holder bodies, charm expiry and model/string services are
external; no creature construction or engine integration is inferred.

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
- All nine SharedClasses methods, including `_healing_amount` and `_healing_flash`.
- All nine SummonItems methods, including its three shared helpers; `item_record`.
- All seven CriticalHit methods, including tint and bonus queries; `light_record`.
- All six ThrownAxes methods, including `_axe_can_use` and `_axe_cast`.
- All seven BattleCry methods; complete `Base.attach_point`.
- All eight Storms methods, including `_storm_cast`, `_storm_ctor`, `_storm_run`
  and `pustule_ctor`.
- All six DiseaseBlade methods, including `player_weapons` and `write_disease`.
- All seven RaiseArmy methods, including `raisearmy_is_raisable`,
  `raisearmy_raise_in_ring` and `_raisearmy_dress`.
- All five Drowsy methods, including `_drowsy_burst` and `drowsy_fraction`.
- All five WaveOfFlame methods, including `waveofflame_hit_test`.
- All six remaining Roar methods, including `_roar_breath` and `roar_hit_ring`;
  its two shared shout helpers are counted with Blood Lust above.
- All five UnspokenWord methods, including `_unspokenword_phase` and
  `unspokenword_hit_ring`.

- All five Poison Bolt methods and all four Summon Badger methods.
- All four Root methods and all three Cone of Shock methods.
- Complete `handle_link` and `handle_clear`, including stack callbacks.

`handle_link` and `handle_clear` now have tested memory-backed and delegated
stack behavior; both count as fully ported. The delegated callee itself is not
implemented.
`skill_call`, `table_call`, and inherited `spell_rand`/`obj_rand`
are not included in the numerator. General `call` now is included. Counts are reference units, not operation,
vector, skill-id, machine-function or playable-feature counts.

## Tests and transport

The Blood Lust-era generator created 417 authored scenarios across all 16 core FPU
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

No reference units remain in `tools/ref_skills_c.py`. The pinned AST has five
top-level functions and 170 directly declared class methods (175 distinct units).
All are implemented and tested; no explicit refusal is counted as a ported body.
Inherited dependencies are outside this denominator. `skill_call` and
`table_call` now select all 73 local Skill body addresses and all 18 local Spell
body addresses, plus inherited Tagar cast/run and Skeleton can-use/cast/run.
Unknown virtual bodies and reference engine services retain explicit callbacks;
forced external selections still delegate. Host shape/resource refusals remain
as documented. Other arena creature behavior is outside the construction domain
and explicitly refuses rather than becoming an invented engine service.

**Next work:** integration with owned engine services and Session/scene consumers
requires its own evidence and tests; it is outside this completed standalone
reference contract. No further skills-C reference family needs porting.

Id 84 Cyclone's class is owned by `ref_skills_b.py`; it remains outside this
contract. Tagar's Insects (87) is inherited from `ref_arena_monsters.py` and its
complete lifecycle is now available through explicit operations and dispatch. Static initialization,
rank-description, presentation and original player routing remain outside this
standalone module. Top-level `shape_result`, `c_div`, `c_rem`, `desc` and
`Base.call` are complete within the documented canonical value domain.
Memory-backed and delegated stack lists, and the shared spell runtime above, are
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

## SharedClasses continuation verification (2026-10-13)

The immutable generator passed with 8,064 complete-state vectors (504 authored
scenarios × 16 FPU configurations). The 87 new scenarios cover all nine methods,
rank zero and signed rank narrowing, Dodge's mandatory adjustment call, allocation
success/failure, creature/player maximum HP, live/dead owner flags, 2-D and 3-D
radius differences, team/ally filtering, duplicate box-query results, HP eligibility
and no-clamp writes, initial flash and expiry boundaries, rotated particle matrices
and terrain bounds. Riposte tests include chance failure, zero and negative sample
counts, complete cosmetic rejection sampling and all 200 particle steps, plus
ordered failure prefixes at every new service and the reused services it reaches.
API checks add the four virtual bodies, missing services, malformed input,
callback self-replacement and recursion recovery. All four prior corpora
(1,920 / 2,688 / 4,384 / 6,672 records) retain their independent hashes.

The first exact comparison exposed use of the file's earlier alive-list constants.
The module's later assignment at reference line 2334 is effective:
count 0x4f8420, slots 0x4f8440. C++ was corrected; the Python-generated expected
state was unchanged. The revised arm64 scoped CTest passed 2/2 in 40.47 seconds.
The current plain corpus SHA-256 is
`f63ff2c4925a8ce92b6cf3ced1fee9060ad3ee9fdabea8af72e75573152f41d3`.
Both arm64 and x64 matched every observation token; their shared complete native
observation stream SHA-256 is
`3898ecd633a32aabb0fd4dc76645644075088892c0943a07c1e290575ee8007e`.

Exact commands used for this continuation:

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
```

Release configure/build, Python syntax checks and diff whitespace checks passed.
The revised x64 scoped CTest passed 2/2 in 61.95 seconds through Rosetta; this
does not verify an Intel host or Windows.
Research checkout remained clean, read only. No private game files were used.
The nine complete tested reference methods raise progress from 67 to 76 of 175.
Inherited RNG methods, partial dispatch and stack-handle refusals are not counted.
Actual console fidelity, Windows/Intel-host execution, Norrath and Session/scene
integration remain unverified. More active families are portable from the pinned
reference; see the remaining list above.

## SummonItems continuation verification (2026-10-13)

The immutable reference AST was rechecked: five top-level functions plus 170
class methods, including nine SummonItems methods. The new family and
`item_record` add ten complete tested bodies, raising progress from 76 to 86.
Partial dispatch, stack-handle refusals and inherited RNG methods do not count.

The generator adds 152 authored scenarios (656 total) across all 16 FPU
configurations: 10,496 complete-state vectors. New coverage includes every
entry/helper, all signed8 name boundaries and plain/parser variants, quantity
and modifier narrowing, arbitrary slot shapes, empty/overfull slot lists,
allocation paths, inclusive carrying capacity, signed bag/index address
calculation, refund clamping and callback-induced player-slot/mana changes,
frame wrapping, emission and destruction boundaries, negative/zero point counts,
private draws, and mutation/failure prefixes at every delegated boundary. API
checks exercise all five virtual bodies, position-pointer transport, missing
and malformed results, callback replacement, recursive refusal/recovery,
malformed item/configuration inputs and light-before-spell allocation order.
All earlier corpora (1,920 / 2,688 / 4,384 / 6,672 / 8,064 records) retain their
independent hashes; ordinary CTest checks all five.

The generator command, Release configure/build, Python syntax checks and diff
whitespace checks passed. Arm64 scoped CTest passed 2/2 in 22.78 seconds; x64
scoped CTest passed 2/2 in 34.18 seconds through Rosetta. Both architectures
matched every Python observation token and produced the identical complete
observation stream SHA-256:
`c92757a4b608f496a10370c5ef352c78cb140430cfa6e4bbf5271ef5294d9214`.
The new plain-vector corpus SHA-256 is:
`391e7d5dd67673786bf9a090974c7a061f9aa2a0da7bb8f5149dcfdf66af7d2d`.

Exact final commands (build output under ignored `build/`, logs under `work/`):

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
GIT_COMMON_DIR="$PWD/work/git-common-overlay" git diff --check
```

Ordinary Git status/diff stalled on the shared common directory's iCloud-evicted
`info/exclude`. A temporary common-dir overlay in ignored `work/` links every
original metadata entry and omits only this inaccessible ignore file. Git
checks/explicit staging use the overlay; original metadata is unchanged and
branch commits still update the original refs/objects and worktree index. Only
the nine existing skills-C files are staged. The preserved pending SharedClasses
work is included in this continuation; no sibling worktree is edited.

Research was read only; its tracked working-tree diff remained empty. No private
game data was used. Rosetta does not verify an Intel host or Windows. Console
fidelity, inventory/effect services, actual item definitions, Norrath and
Session/scene integration remain unsupported or unverified. Other active
families remain portable from the pinned contract.

The required full Release CTest command passed **102/102** in 398.52 seconds,
including both skills-C checks. No test failed or required a timeout retry.
Optional real-game decoder validation remained skipped; the suite does not
establish private game-content acceptance. Initial API iteration exposed a
reused test callback asserting a different position; the assertion was reset
for that independent case and the final API checks passed. No expected-state
records were altered to accommodate native output.

## CriticalHit continuation verification (2026-10-13)

The immutable AST count remains five top-level functions plus 170 declared
class methods. All seven CriticalHit methods and the required general
`Base.light_record` add eight complete tested bodies: 94 of 175. Partial
routers, stack-handle refusals and inherited RNG methods still do not count.

The generator adds 141 authored scenarios (797 total), producing 12,752
complete-state vectors across all 16 FPU configurations. Coverage includes all
seven methods and the light helper, all three class ids and fallback, direct
cast rank and signed8 narrowing, allocation success/failure, expiry and two-second
light boundaries, fading store order, private particle draws and terrain drop
limits, nonpositive/extreme radius products, strict planar/3-D aura boundaries,
negative/zero ranks, PvP gates, alive-list order and negative counts. Callback
cases retain mutation/failure prefixes at all new boundaries and reused services
and Skill slots reached by the family. API checks add the three virtual routes,
rank query order 53/81/33 and maximum selection, unused position pointers,
missing/malformed scalar results, callback replacement and recursion recovery.

All six previous complete-state corpora (1,920 / 2,688 / 4,384 / 6,672 / 8,064 /
10,496 records) retain their independently checked hashes. Expected states are
always generated from the pinned Python contract, never from native output.
The pending 12,752-record corpus has plain-vector SHA-256
`4b98c7488aa3d6aabf73d553da55b6fc7d07c7cd6cbcdb6e777342fdb84c4d2e`.
The takeover found a stale verifier pin for an earlier 12,688-record intermediate
corpus. The ThrownAxes continuation below corrects the verifier identity,
independently retains this entire pending corpus byte-for-byte, and rechecks
both families on arm64 and x64. Its final results supersede the intermediate
CriticalHit-only verification below.

Exact verification commands:

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
GIT_COMMON_DIR="$PWD/work/git-common-overlay" git diff --check
git -C ../../champions-return-to-arms-re diff --name-only
```

Generation, Release configure/build, arm64 scoped checks (2/2, 21.15 seconds),
Python syntax checks and diff whitespace checks passed. The research working-tree
diff was empty; it was only read through pinned Git objects. No private game
files were used. An initial API assertion addressed the wrong light colour
halfword; the assertion was corrected, with native behavior and expected-state
vectors unchanged. Console fidelity, original light/particle/terrain/rank
services, Norrath, Windows and Session/scene integration remain unverified.

## ThrownAxes continuation verification (2026-10-13)

Family chosen: all of ThrownAxes (Fire Axe 98 / Cold Axe 99). The pinned AST
again counts five top-level functions plus 170 directly declared methods. Its
six complete tested methods raise progress from 94 to 100 of 175. The pending
CriticalHit/light-record changes present at takeover are preserved and verified
in the same continuation; relative to the last commit this includes fourteen
additional reference units. No refusal or partial router is counted.

The generator adds 122 authored scenarios (919 total), producing **14,704**
complete-state vectors across all 16 FPU configurations. Coverage includes all
six methods, exact flag-bit gating and warnings, both targeting modes, signed
facing, rank adjustment, all modifier positions/matching/full arrays, value
narrowing and rounding extremes, null allocations, absent primary weapons,
signed active-set and resource indices, quantity endpoints, prediction copies,
HUD/removal ordering, and successful/throwing callback mutations. API checks
exercise all six published virtual bodies, position-pointer transport, distinct
results from the two resource queries, projectile argument order, missing and
malformed results, callback replacement, recursion recovery and explicit
out-of-domain memory refusal.

All seven prior corpora (1,920 / 2,688 / 4,384 / 6,672 / 8,064 / 10,496 /
12,752 records) retain independent compatibility hashes in the ordinary test.
The entire pending CriticalHit corpus was also compared byte-for-byte to its
initial compressed fixture after regeneration. Expected observations continue
to come only from immutable reference Git objects and synthetic fixtures.
Current plain-vector SHA-256:
`ba8736bceee122349fdc86a170e3c89dbcb4afbd414b35b4fbf5ebdd5077d5c8`.
Both arm64 and x64 match every observation token; their shared complete native
observation SHA-256 is:
`a14df6267fceb7e073a3749bbfbea0df4443b5980bd01c134d957b048b82d5a3`.

Exact commands (logs in ignored `work/thrown-axes/`):

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re diff --name-only
```

Generation, Release configure/build, arm64 scoped checks (2/2, 14.66 seconds),
x64 scoped checks (2/2, 20.43 seconds through Rosetta), Python syntax checks and
whitespace checks pass. An initial API build found a duplicate test-local name;
it was renamed. Initial generation needed its callback slot captured to shape
the two attach outputs. The initial exact check found one new resource-index
fixture outside the documented raw32 memory domain; it was replaced with a valid
negative row, with the out-of-domain refusal retained as a separate API check.
No Python expected observation was replaced with native output.

The research checkout's tracked diff remains empty and it was never edited.
Private acceptance measurements are skipped: `../../private-validation` is
unavailable, as specified by the task. Nothing under Documents or Desktop was
read. Rosetta does not verify an Intel host; Windows, actual console fidelity,
Norrath and Session/scene integration remain unverified. Projectile flight/hit,
original engine callbacks and actual game-content tables remain unsupported.
Other active families on the remaining list are portable from existing evidence.

The final full Release CTest run passed **102/102** in **310.62 seconds** with
`-j 3`. No test failed or required a timeout retry. This verifies public synthetic
contracts only; optional private game-data acceptance remains skipped.

## BattleCry continuation verification (2026-10-13)

Family chosen: all seven BattleCry methods (77), with the complete
`Base.attach_point` needed by its constructor and streamers. The pinned AST
was recounted: five top-level functions and 170 directly declared class methods,
175 total. The eight complete tested units raise progress from 100 to 108.
The accounting excludes inherited methods, partial routers and stack-handle
refusals. The remaining-family list above is updated for the next continuation.

The generator adds 265 authored synthetic scenarios (1,184 total) across all
16 FPU configurations, producing **18,944 complete-state vectors**. Coverage
includes every new body, signed rank narrowing, style variants, null/successful
allocation, constructor animation-event/header stores and callee hand stores,
all three run events, animation flag loss, network gating, interruption before
and after the cry, refund/sound stop, expiry boundaries, streamer zero/None,
aura growth and minimum radius, nonpositive particle counts, rotated terrain
queries and both 72-unit drop boundaries, tint/alive-list ordering, inclusive
three-dimensional affects queries, owner coverage before the cry, first-cry
enemy/HP/radius filters, repeated query results, chance gates and stun calls.
Callback cases retain successful mutations and mutation-before-failure prefixes
at every service boundary and all delegated Skill slots reached by the family.
API checks cover the three Skill bodies, ignored cast position pointer, missing
and malformed handlers, attach argument transport, explicit model zero, callback
replacement, recursion refusal and recovery.

All eight prior complete-state corpora (1,920 / 2,688 / 4,384 / 6,672 / 8,064 /
10,496 / 12,752 / 14,704 records) retain independently checked compatibility
hashes in ordinary CTest. No old expected-state record changed. Expected states
come exclusively from pinned reference Git objects and synthetic inputs.
The current plain-vector SHA-256 is
`703f2b69695f08368d5dea2e2c3d1c5bdb8344e79971dfddf28058208f423d18`.
Arm64 and x64 matched every observation token and produced the same complete
native observation stream SHA-256:
`14f0236fa9bd382413b08de2dbc08cd807352d6528d4c805b52b3eaa989eacd2`.

Exact commands (logs in ignored `work/battlecry/`):

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re diff --name-only
```

Generation, Release configure/build, arm64 scoped CTest (2/2, 24.79 seconds),
x64 scoped CTest (2/2, 36.06 seconds through Rosetta), Python syntax and diff
whitespace checks pass. Rosetta does not verify an Intel host or Windows.
The research working-tree diff remains empty; the checkout was read only.
Private acceptance measurements were skipped because `../../private-validation`
is unavailable. Nothing under Documents or Desktop was read. Engine callback
implementations, actual console fidelity, game-content tables, Norrath and
Session/scene integration remain unsupported or unverified. Other families
remain portable from the pinned public evidence.

The final full Release CTest command passed **102/102** in **487.10 seconds**
with `-j 3`. No test failed or required a timeout retry. The suite verifies
public synthetic contracts; optional private game-data acceptance remains
skipped. No branch was pushed and no pull request or issue was created.

## Storms continuation verification (2026-10-13)

Family chosen: all eight Storms methods (Lightning Strike 100 / Pus Storm 80),
including Pustule construction. The immutable AST again contains five top-level
functions and 170 directly declared class methods, 175 total. The eight new
complete tested units raise progress from 108 to 116. The inventory above keeps
inherited methods, partial routers, stack-handle refusals and external callee
bodies outside the numerator.

The generator adds 357 authored synthetic scenarios (1,541 total), yielding
**24,656 complete-state vectors** across all 16 FPU configurations. Coverage
includes all eight bodies, cast and constructor allocation paths, caster flag
combinations, signed rank/facing/frame boundaries, warning lights, zero and
negative lifetimes/intervals, computed fall time, drifting/fading light stores,
empty handles, zero-object destruction gates, decoration allocation failures,
private RNG seed endpoints, ball rejection, zero-divisor scheduling, signed odd
and extreme damage halves, repeated/empty world results, enemy gates and
mutation/failure prefixes across all new services and delegated Skill slots.
API checks add four published Skill routes, position-pointer transport,
missing/malformed callbacks, variant/argument refusals, callee mutation and
replacement, recursion refusal/recovery and adjusted object destruction.

Ordinary CTest independently retains all nine previous corpus hashes (1,920 /
2,688 / 4,384 / 6,672 / 8,064 / 10,496 / 12,752 / 14,704 / 18,944 records).
Every expected observation comes from the pinned Python contract and authored
fixtures. Current plain-vector SHA-256:
`404e5460b5cba1b05098883869f0ac515cae7213858a7614ac2b47602e407848`.
Arm64 and x64 matched every token and produced the identical complete native
observation stream SHA-256:
`2803253517423c34168a80d598a0cdf9e9da38c91884cfa6c77da17d91a2956a`.

Exact commands (logs under ignored `work/storms/`):

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re diff --name-only
```

Generation, Release configure/build, arm64 scoped CTest (2/2, 20.31 seconds),
x64 scoped CTest (2/2, 28.22 seconds through Rosetta), Python syntax and
whitespace checks passed. An initial API assertion used the wrong flattened
sound argument shape; it was corrected to the existing five-word signature.
Neither native behavior nor expected-state vectors changed for that correction.
Rosetta does not verify an Intel host or Windows. The research checkout's
tracked diff remains empty; it was read only. Private acceptance measurements
were skipped because `../../private-validation` is unavailable. Nothing under
Documents or Desktop was read. Actual console fidelity (including zero-divisor
remainder), original engine services, Pustule flight, actual game-content tables,
Norrath and Session/scene integration remain unsupported or unverified. Other
families remain portable from the pinned evidence. No push, pull request or
issue was created.

The final full Release CTest run passed **102/102** in **372.60 seconds** with
`-j 3`. No test failed or required a timeout retry. Public synthetic contract
checks do not establish private game-content acceptance or a playable scene.

## Disease Blade continuation verification (2026-10-13)

Family chosen: all six DiseaseBlade methods (97), including the complete weapon
selection and modifier-write helpers. The immutable reference AST was recounted:
five top-level functions plus 170 directly declared class methods, 175 total.
Six complete tested units raise progress from 116 to 122. Inherited methods,
partial routers, stack-handle refusals and external callee bodies remain outside
the numerator. The remaining-family list above is updated for continuation.

The generator adds 548 authored synthetic scenarios (2,089 total), producing
**33,424 complete-state vectors** across all 16 FPU configurations. Coverage
includes all six methods, allocation success/failure, player/monster flag gates,
signed rank and active-set narrowing, signed player-slot record wrapping, all
four modifier positions, first writable slot versus first matching value,
negative/zero/equal/limit values, full arrays, thrown-item flags, integer and
rounding endpoints, wide duration products, signed frame expiry and modulo gates,
positive/negative zero radius, duplicate/empty world results, PvP acceptance,
owner retargeting, adjusted holder calls, and callback mutation/failure prefixes.
API tests cover the three published Skill routes, ignored cast position,
missing/malformed callbacks, holder activation/clear argument transport,
retained writes on failure, callback replacement and recursion recovery.

Ordinary CTest independently checks all ten previous corpus hashes, including
the entire 24,656-record Storms corpus. No prior expected observation changed.
Expected observations come only from the immutable reference Git objects and
synthetic inputs. Current plain-vector SHA-256:
`f585dcfbb4baf2b472791af13200e8167df82c335d938272015d917de07ab526`.
Arm64 and x64 matched every observation token and produced the same complete
native observation SHA-256:
`f79d92ef0a96245927bbec0d16441942b5f2f5dbc7b3e4d636bfc16e4aa6017b`.

Exact validation commands (logs under ignored `work/diseaseblade/`):

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re diff --name-only
```

Generation, Release configure/build, direct arm64/x64 API execution, arm64 scoped
CTest (2/2, 35.47 seconds), x64 scoped CTest through Rosetta (2/2, 60.24
seconds), Python syntax and whitespace checks passed. No native
or vector correction was required by the exact comparison. The research checkout
was read only and its tracked diff remains empty. Private acceptance measurements
were skipped because `../../private-validation` is unavailable. Nothing under
Documents or Desktop was read. Original holder element bodies, weapon timer
updates, on-hit disease damage, engine callbacks and actual game-content tables
remain external. Console fidelity, Norrath, Windows and Session/scene integration
remain unverified; Rosetta does not verify an Intel host. Other listed families
remain portable from existing evidence. No push, pull request or issue was made.

The final full Release CTest run passed **102/102** in **469.08 seconds**
with `-j 3`. No test failed or required a timeout retry. The suite verifies
public synthetic contracts; private game-content acceptance remains skipped.

## Raise Army continuation verification (2026-10-13)

Family chosen: all seven RaiseArmy methods (91), including the complete
`_raisearmy_dress` helper. Recounting the pinned AST gives five top-level
functions plus 170 directly declared methods, 175 total. Seven complete tested
units raise progress from 122 to 129. Inherited methods, partial dispatchers,
stack-handle refusals and delegated callee implementations are excluded from
the numerator. The remaining list and inventory above are updated.

The generator adds 483 authored scenarios (2,572 total), producing **41,152
complete-state vectors** across all 16 FPU configurations. New coverage includes
all eligibility flag bits, null creatures, death-animation readiness, boss and
post-callback flags; signed rank/counter/stat narrowing; query reversal,
duplicates and empty results; inclusive ring reach; missing names and failed or
noncreature factories; duration endpoints and unusual float values; private RNG;
HP floor; signed animation-track indices; absent/present source/destination
holders; variant-prefix short circuit; tint kinds; 64-bit hide masks and flags;
and successful/throwing callback mutations at every reachable service boundary.
API checks add the three Skill routes, cast-position pointer transport, missing
services, malformed mask results, signed this adjustments, retained failure
prefixes, callable replacement and recursion recovery.

Ordinary CTest retains all eleven earlier corpus hashes, including every one of
the 33,424 Disease Blade-era observations. Expected observations come only from
immutable reference Git objects and synthetic fixtures. The new plain-vector
SHA-256 is `28eb229c855144e45f54ab154f269f70d136011e1b714e8a2f32cf35350f3c2c`.
A preliminary direct comparison passed all 483 new scenarios in FPU configuration
0. Initial generation found a single-entry box-result fixture encoded as a scalar;
it was changed to an explicit sequence. Initial trace execution found the old
callback identifier bound; the existing reader was extended to the new final
service id. No native behavior or reference expected observation was changed to
resolve either transport issue.

Exact validation commands (logs under ignored `work/raisearmy/`):

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re diff --name-only
```

The research checkout was read only. Private acceptance measurements are skipped
because `../../private-validation` is unavailable; nothing under Documents or
Desktop was read. Original object factories, creature/holder bodies, charm
expiry, animation, rendering and engine services remain external. Console
fidelity, Windows, Norrath and Session/scene integration remain unverified;
Rosetta does not verify an Intel host. Other listed families remain portable
from the pinned public evidence. No push, pull request or issue was created.

Generation, Release configure/build, direct arm64/x64 API execution, arm64 scoped
CTest (2/2, 43.92 seconds), x64 scoped CTest through Rosetta (2/2, 62.07 seconds),
Python syntax and whitespace checks passed. Both architectures matched every
observation token and produced the identical complete native observation stream
SHA-256: `b63b721dd367203caec0b62347cbf62b903db495beadbc9a47b16e57a28a3969`.

The final full Release CTest run passed **102/102** in **381.13 seconds** with
`-j 3`. No test failed or required a timeout retry. These are public synthetic
contract checks; they do not establish private game-content acceptance or a
playable scene.

## Drowsy continuation (2026-10-14)

Family chosen: all five `Drowsy` methods (88): `drowsy_cast`,
`drowsy_spell_ctor`, `_drowsy_burst`, `drowsy_spell_run` and `drowsy_fraction`.
Evidence is the existing immutable `ref_skills_c.py` pin
`356add27d50e50972b2a671b31e8f4ec93e6778c` and its authored specification and
executed notes (Drowsy row: 49/49). The AST recount is five top-level functions
plus 170 directly declared class methods, 175 total. Five complete tested units
raise the ported count from 129 to 134; inherited methods, incomplete general
routers and refused behaviors are not added.

Operations 128–132 append to the existing enum without changing older numbers:
cast `[skill,level,caster,x,y,z,facing]`, constructor `[spell,level,victim,skill]`,
burst `[spell,definition,speedRaw,flat]`, run `[spell,unusedPosition,unusedFacing]`,
and fraction `[spell]`. Cast returns zero; fraction returns raw float; other
operations return empty. Published Skill cast/mana bodies also dispatch, with
cast's pointer expanded to xyz and signed16 facing retained.

Cast selects victims in callback order up to the signed statistic-0 count,
respects enemy/flag/boss/signed-timer gates, and counts a failed allocation while
still calling `creature_addSpell(victim,0)`. Constructor uses the victim as owner,
a shared base draw and signed-byte rank. Burst consumes two private draws and a
rejected-ball sequence per particle. The slow roll follows all 15 particles,
so earlier particle draws affect the chance. Phase-2 timer is the narrowed integer
duration-times-60, independent of float expiry. Letter heights, both flash quads
and the network force-rebase byte retain write order and boundary comparisons.
The returned stat-3 percentage is unclamped.

Three new services append to the callback registry: `CollectObjectsInArc` (88),
`CreatureAddSpell` (89), and `HandleClearStack` (90). The last transports the
explicit stack sentinel as `[0]`, following the existing protocol convention;
it is a callback marker, never a fabricated memory address or silently skipped
cleanup. Missing services refuse; callbacks retain their writes before failure.
Creature enemy and spell destruction callbacks receive signed-adjusted this.
Original world queries, victim attachment, particle effects, network ownership
and destruction remain delegated, with no implicit engine implementation.

Synthetic vectors cover all five methods, signed rank/facing/duration/count,
null caster/spell, allocation failure, repeated/empty query results, every victim
gate, phases 0/1/2/unknown, delayed success/failure, inclusive time endpoints,
private seed extremes, timer overflow, height wrap, NaN/negative-zero arithmetic,
network/player/local gates, repeated ticks, and callee mutation/failure prefixes.
API tests cover published dispatch, pointer transport, stack sentinel, missing
and malformed services, retained mutations, callback replacement and recursion
recovery. All prior corpus hashes remain independently checked.

Private acceptance is skipped: `../../private-validation` is unavailable. No
Documents or Desktop paths were read. Public fixtures do not establish original
console fidelity, game-content acceptance, Norrath compatibility, Windows or
Session/scene integration. Rosetta execution does not verify an Intel host.
Other remaining families are portable from the existing public evidence.

The generator adds 305 scenarios (2,877 total) and produces 46,032 complete-state
vectors across all 16 FPU configurations. Plain-vector SHA-256:
`85a9fdafae504c7ccface4b1afa8552603a18e7e85728fc8f8f1f2b9fb1f08cd`.
Ordinary CTest independently retains all twelve earlier corpus hashes, including
every 41,152-row Raise Army observation.

Exact validation commands (logs under ignored `work/drowsy/`):

```sh
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

The initial config-0 comparison matched all 433 new scenarios after correcting
new fixture callback identifiers for sound/light construction. A preliminary
full generation was stopped to make those fixture corrections; native behavior
and prior expected observations were not changed by the correction.

Generation, Release configure/build, direct API execution on both architectures,
Python syntax and whitespace checks passed. Final scoped CTest passed arm64
2/2 in 76.58 seconds and x64 2/2 through Rosetta in 93.53 seconds. Both matched
every token in all 46,032 vectors and produced the identical complete native
observation SHA-256:
`f932104b52b5cc5a555d0ee41b0d9f5870fd8b9aed5d155c2303d76fb448978c`.
The final compressed fixture is 96,518,726 bytes. Repeated traces retain 32 ticks
(the delayed roll and next phase); irrelevant cross-products of duration in
phases that do not read it were removed. All duration, chance, phase and
callback boundary coverage remains. The earlier 433-scenario / 48,080-vector
corpus also passed on both architectures before this fixture compaction.
The research checkout's status remained empty; it was read only.

The final full Release CTest run passed **102/102** in **416.25 seconds** with
`-j 3`. No test failed or required a timeout retry. These are public synthetic
contract checks; private acceptance, console fidelity, Windows, Norrath and
Session/scene integration remain unverified. No push, pull request or issue
was created.

## Wave of Flame continuation (2026-10-14)

Family chosen: all five `WaveOfFlame` methods (95): `waveofflame_can_use`,
`waveofflame_cast`, `waveofflame_spell_ctor`, `waveofflame_spell_run` and
`waveofflame_hit_test`. Evidence is the immutable reference pin and authored
specification above, including the executed Wave of Flame row (55/57). The
pinned AST has five top-level functions and 170 directly declared class methods,
175 total. Five complete tested methods raise progress from 134 to 139.
The operation-method inventory has 138 names: subtract two inherited random
helpers and two incomplete handle methods, then add the five tested internal
methods (`_round`, `_tier_value`, `_rage`, `_h_take`, `_h_unlink_free`) to get 139.
Inherited methods, partial general dispatchers and incomplete stack-handle
branches stay excluded. The remaining inventory is updated above.

Operations 133–137 append without changing older numbers. Can-use takes
`[skill,caster]`; cast `[skill,level,caster,unusedPosition,unusedFacing]`;
constructor `[spell,level,caster,offsetX,offsetY,offsetZ,skill]`;
run `[spell,unusedPosition,unusedFacing]`; hit test `[spell,ringRaw]`.
Can-use returns one; cast returns the allocated spell (including zero on failure);
the other three return empty. Published Skill can-use/cast/mana bodies dispatch.
Cast ignores the supplied position pointer, including an invalid pointer, because
this contract fixes the centre from the caster's visual position.

Construction keeps the single shared base draw, signed-byte rank, ordered centre
stores, sound, optional delegated light and sphere query. The sphere callback
`CollectInSphere` (91) takes `[x,y,z,signedFacing,reachRaw]` and returns objects in
callback order. Valid candidates are inserted at the head, so the saved list is
the reverse of that returned order; the pinned Python governs this where the
written notes describe query-order reversals differently. Reach is computed as
`steps*12 + steps*12`, retaining both multiplications. Stack cleanup uses the
existing explicit `HandleClearStack([0])` service.

Run advances the float clock before testing strict expiry. The hit test uses
inclusive `(dx*dx + dy*dy) - radius <= ring*ring`, with the object radius
subtracted unsquared. Null candidates are dropped. Reached creatures with flags
9 or a false enemy callback are dropped without damage; noncreatures bypass
those checks. Damage uses the existing statistic spread and the exact fire
slot/flags descriptor, followed by sound and removal. A callback failure keeps
its mutations and the preceding writes; a sound failure leaves a candidate in
the list even when its damage callback already ran. Three particle rings retain
the private angle draw, rejected-ball draws, frame wobble and exact raw floats.

The generator adds 333 authored scenarios, covering all five methods, signed
rank/facing, zero/nonzero allocation, offsets and visual-position edge values,
empty/single/duplicate/null query results, every candidate flag gate, player and
nonplayer spread, inclusive radius boundaries, negative/NaN/zero arithmetic,
expiry, private seed and frame extremes, all three particle definitions,
repeated ticks, and successful/throwing mutations at each reached callback.
API checks exercise published dispatch, ignored position pointers, missing
services, signed destruction adjustment, self-replacing callbacks, retained
failure state, one-time removal, recursion recovery and argument refusals.
All thirteen prior corpus hashes remain independently checked.

Original world queries, light construction, damage, particles, sound and spell
destruction remain explicit callbacks. No new runtime services or constants are
invented. Private acceptance is skipped because `../../private-validation` is
unavailable. Nothing under Documents or Desktop was read, and the research
checkout was read only. Console fidelity, Windows, Norrath, actual game-content
tables and Session/scene integration remain unverified. Rosetta does not verify
an Intel host. Other listed families remain portable from existing evidence.

Validation commands (logs under ignored `work/waveofflame/`):

```sh
python3 work/waveofflame/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

The preliminary config-0 comparison matched all 333 new scenarios. The first
API run found an absent tier-2 body in the new authored virtual-table fixture;
the fixture was completed. Both architecture API tests then passed. No native
behavior or prior expected observation was changed to resolve this fixture issue.

The final generator contains 3,210 scenarios and 51,360 complete-state
vectors across all 16 FPU configurations. Plain-vector SHA-256:
`311b3a1b1093aff89af2dac9552f38ff651fa486e405964c72a33d856c599e12`.

The initial 51,360-row corpus passed arm64 2/2 in 65.95 seconds and x64 2/2
through Rosetta in 82.83 seconds, with identical observation hash
`a7f872f9efe939f056cfdbad393741adb97cf7574564737d95aaeed21db1eeb2`.
Fixture measurement then found 241,539,282 of 1,894,559,323 uncompressed bytes
in the new 35-tick cumulative trace. That trace was shortened to five ticks
starting at clock 0.5: three live ticks, expiry, and another expiry tick. Other
cases retain early-clock, private-RNG and particle coverage. All 333 final new
scenarios again matched config 0. The reference and native implementation were
unchanged; no prior scenario was altered. Final corpus validation follows.

Final scoped CTest passed arm64 **2/2 in 42.36 seconds** and x64 **2/2 in
77.14 seconds** through Rosetta. Both matched all 51,360 observations, preserved
all thirteen prior corpus hashes and produced identical complete native
observation SHA-256:
`fe8c13703c7a9eefe35843e32a0c9d50338fd7e9a95e84eb48c407bd25690872`.
The final compressed fixture is 102,795,613 bytes, down from 170,894,400 bytes
in the preliminary corpus. Generation, Release configure/build, direct API
execution on both architectures, Python syntax and whitespace checks passed.

The final full Release CTest run passed **102/102 in 425.24 seconds** with
`-j 3`. No test failed or required a timeout retry. The full suite's skills-C
case also matched the final observation hash above. These checks use public
synthetic fixtures; private acceptance, console fidelity, Windows, Norrath and
Session/scene integration remain unverified. The research checkout's status
remained empty. No push, pull request or issue was created.


## Roar continuation (2026-10-14)

Family chosen: all six remaining `Roar` methods (76): `roar_can_use`,
`roar_cast`, `roar_spell_ctor`, `_roar_breath`, `roar_spell_run` and
`roar_hit_ring`. Its two shared shout helpers were already complete. Evidence
is the immutable reference pin above plus the authored specification and executed
notes (Roar row: 79/81). The executed corrections specifically establish the
water-test ground height and interrupted linger particle draws. No research
checkout files were edited.

The pinned AST recount is five top-level functions plus 170 directly declared
class methods, 175 total. Six complete tested methods increase progress from
139 to 145. The operation-method inventory contains 144 names: subtract two
inherited random helpers and two incomplete stack-handle methods, then add five
internal methods (`_round`, `_tier_value`, `_rage`, `_h_take`, `_h_unlink_free`).
The remaining 30 units are listed above; no partial dispatcher or refusal counts.

Operations 138–143 append in the existing module without renumbering earlier
operations: can-use `[skill,caster]`; cast
`[skill,level,caster,unusedPosition,unusedFacing]`; constructor
`[spell,level,owner,skill]`; breath
`[spell,facing,mouthX,mouthY,mouthZ,definition,speedRaw,centred]`; run
`[spell,unusedPosition,facing]`; hit ring `[spell,ringRaw]`. Can-use returns 0/1,
cast returns the allocated spell (including zero), and the others return empty.
Published Skill can-use/cast/mana bodies dispatch through the existing router.
Cast ignores its supplied position pointer; run narrows facing to signed16,
while the standalone breath helper retains the integer facing supplied to it.

Construction keeps one shared base draw, signed-byte rank, original store order,
style-selected animation header events and retained sound handle. Run preserves
animation event equality, pre-shout half-mana refund and upper clamp, candidate
front insertion, mouth-private draws, particle counts at signed rank 10, half-second
wave/end-time comparisons, and clock increment after all effects. The hit ring
uses `sqrt(dx*dx + dy*dy) - radius <= ring`, the exact three damage-flag bands,
and an absent descriptor transported as the explicit zero marker. Removal follows
damage; failed callbacks retain mutations and the unconsumed candidate. Model
attach outputs use callee pokes with no store-log entries. Ring/dust loop counts
are chosen once at each loop entry, even if a particle callback changes rank.

New services append as `CameraShake` (92), `WaterTest` (93), and `WaterDisturb`
(94). Their flattened arguments are `[0,strengthRaw]`,
`[world,x,y,groundZ,0,8Raw]`, and `[world,x,y,groundZ,3500Raw,36Raw]`.
Existing world, animation, attach-point, damage, particle, sound, network and
stack-cleanup services are reused. Missing services and malformed required scalar
or vector results explicitly refuse; no world or presentation behavior is invented.

The generator adds 559 authored synthetic scenarios covering every new method,
allocation failure, signed rank/style/facing and player-slot wrapping, animation
state/events, candidate query/list/flag order, exact ring endpoints and unusual
float values, refund/expiry equality, water/network gates, both mouth modes,
rank-dependent particle counts, private seeds, short repeated traces, and
successful/throwing callback mutations. API checks separately exercise published
bodies and signed this adjustment, ignored pointers, missing/malformed services,
event headers, ground-height water transport, callback self-replacement, retained
failure state, one-time hits, destruction adjustment and recursion recovery.
All previous corpus hashes are retained independently, including the entire
51,360-row Wave of Flame corpus.

Private acceptance is skipped because `../../private-validation` is unavailable;
no Documents or Desktop paths were read. Console fidelity, Windows, Norrath,
actual game-content tables and Session/scene integration remain unverified.
x64 runs use Rosetta on Apple Silicon and do not verify an Intel host. The five
remaining active families are portable from the existing public contracts.

Validation commands (logs under ignored `work/roar/`):

```sh
python3 work/roar/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

The first config-0 pass exposed a null-spell fixture entering a wave with no
supplied Skill and a style fixture using a record pointer where the contract
expects a player slot. Those authored inputs were corrected. All 559 new
scenarios then matched config 0, and direct arm64 API checks passed. Native
behavior and previous expected observations were not changed to repair fixtures.

Final generation produced 3,769 scenarios and **60,304 complete-state vectors**
across all 16 FPU configurations (8,944 new rows). Plain-vector SHA-256:
`065b6686be3f44419fc539934b6d33eace1befdb3eeba929056004fd3376d0e8`.
The compressed fixture is 115,228,167 bytes. Arm64 scoped CTest passed **2/2 in
56.46 seconds**, matching every observation token and all fourteen prior corpus
hashes. Complete native observation SHA-256:
`56a71e11f216518e909a9e3dd91f278369a7c18326e2ff6e58d07fd9ddc4ebf9`.

x64 scoped CTest through Rosetta passed **2/2 in 65.40 seconds**, also matching
all 60,304 vectors and all fourteen earlier corpus hashes, with the identical
complete observation SHA-256 above. Release configure/build, direct API execution
on both architectures, generation, Python syntax and whitespace checks passed.

The final full Release CTest run passed **102/102 in 380.96 seconds** with
`-j 3`; no test failed or required a timeout retry. Its skills-C trace again
matched the complete observation hash above. The research checkout's status
remained empty. These are public synthetic contract checks; private acceptance,
console fidelity, Windows, Norrath and Session/scene integration remain unverified.
The commit is local only; no push, pull request or issue was created.


## Unspoken Word continuation (2026-10-14)

Family chosen: all five `UnspokenWord` methods (70): `unspokenword_cast`,
`unspokenword_spell_ctor`, `_unspokenword_phase`, `unspokenword_spell_run` and
`unspokenword_hit_ring`. Evidence is the immutable reference pin above, the
written skill-70 specification, and the executed corrections (57/58 row;
level-zero particle division and the shared cosmetic/stun private stream).
The research checkout is read only. The pinned Python AST recount gives five
top-level functions plus 170 directly declared class methods, 175 total. Five
complete tested methods increase progress from 145 to 150. No partial router,
stack-handle refusal or inherited helper is newly counted. The remaining list
above is updated for the next run.

Operations 144–148 append to the existing enum: cast
`[skill,level,caster,x,y,z,facing]`, constructor `[spell,level,caster,skill]`,
phase `[spell,signedAge]`, run `[spell,unusedPosition,unusedFacing]`, and hit ring
`[spell,ringRaw]`. Cast returns the allocated spell or zero; phase returns its
unsigned raw32 pattern; other methods return empty. Published Skill cast/mana
bodies dispatch, including signed this adjustment and virtual position-pointer
expansion. The standalone phase accepts signed raw32 age; internal angular
arithmetic retains the trigonometric low bits without signed-overflow behavior.

Cast preserves both flashes before sound, optional delegated light, allocation
failure and exactly one shared base draw on construction. Construction retains
frame, trail-handle and owner stores in reference order. Candidate creatures
passing flags 0xd are linked at the front in reverse callback order, followed by
explicit stack-list cleanup. Reach uses `(steps*12)*1.5` in that order. No game
statistic tables are embedded.

Run asks for the statistic before testing age, destroys at age 30, and retains
signed frame subtraction, signed remainder in its phase, the ten-point spirals,
strict fade after half age and all length/colour arguments. Level-zero particle
loops consume no private draws and do not evaluate their unused division. Both
particle spirals run before ring hits using the same private generator as stun.
The first uses the centre captured at entry; the second reloads the centre on
every particle, including callback changes. The hit test keeps inclusive
`sqrt(dx*dx+dy*dy)-radius <= ring`, four signed-rank damage-flag bands, tier-1
statistic-0 range for every caster, survival HP testing after damage, strict
chance comparison and signed-adjusted stun. Candidate removal follows damage
and any stun callback; failures retain preceding mutations, draws and the node.

New callback services append as `TrailCreate` (95), `TrailSetLength` (96),
`TrailSetColor` (97), and `TrailSetPoints` (98). Create carries
`[1,length,5,r,g,b,a,widthRaw]` and requires one handle result. Length and colour
carry `[handle,length]` and `[handle,r,g,b,a]`; points carry
`[handle,30 flattened coordinate words,10,0x78,zeroRaw,oneRaw]`.
World queries, damage, stun, lights, sound, trails, particles and destruction
remain explicit callbacks; missing required services or malformed scalar results
refuse. No delegated engine behavior is synthesized.

The generator adds 577 authored synthetic scenarios covering all five methods,
signed rank/age/facing/frame boundaries, zero/nonzero allocations, candidate
filtering/order/duplicates/nulls, exact half-age and expiry thresholds, negative
statistics, zero particle counts, both spirals and rejection sampling, every
rank/flag/enemy/HP/chance gate, radius boundary arithmetic, unusual floats,
repeated ticks and callback mutation/exception prefixes. Direct API checks add
published virtual bodies, signed this adjustment, position-pointer expansion,
light and trail signatures, malformed trail results, self-replacing callbacks,
missing-service prefixes, damage-before-survival, failed stun state, one-time
hits and recursion recovery. All fifteen prior corpus identities are retained
independently, including all 60,304 previous Roar-era rows.

Private acceptance is skipped: `../../private-validation` is unavailable by
owner instruction. Nothing under Documents or Desktop was read. Original
console execution, Windows, Norrath, actual game-content tables and Session/scene
integration remain unverified. x64 execution through Rosetta is not Intel-host
verification. The four remaining active families are portable from public evidence.

Validation commands (logs under ignored `work/unspokenword/`):

```sh
python3 work/unspokenword/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

The method-name audit independently matches 149 operation names against the
pinned AST: exclude two inherited RNG helpers and the two incomplete handle
methods, then include the five already-tested internal helpers to obtain 150.
All 25 unported names match the remaining inventory above.

The initial API-test compilation found a hexadecimal token requiring whitespace
before `+`; a subsequent command had run an older binary. The spacing was fixed,
both architecture builds were required to exit successfully, and both freshly
rebuilt API binaries passed. The config-0 comparison matched all 577 new
scenarios; no prior fixture or native behavior was changed to resolve the
compile error.

Final generation produced 4,346 scenarios and **69,536 complete-state
vectors** across all 16 FPU configurations (9,232 new rows). Plain-vector
SHA-256: `f3bfb9d8c7b277a1d43d16a00ae55aa90597279800be663396a249a844081d85`.
The compressed fixture is 120,544,181 bytes.

The first scoped exact test timed out without reporting an observation mismatch
(1,338.85 s elapsed, exceeding its configured 240 s limit). Its isolated retry
was stopped deliberately after measuring the old verifier at 4,087,392 KiB
resident memory. The verifier retained full copies of every historical corpus.
It now streams the unchanged fixture and all fifteen prefix hashes, spools native
input/output under ignored `work/`, and compares every observation row. This
changes resource use, not expected data or verification coverage.

A measured isolated streaming attempt still reached the 240 s timeout under
concurrent host load: 241.71 s wall, 64.99 s user and 4.76 s system CPU; maximum
resident set 48,005,120 bytes. Its native trace was still running at the limit.
The existing skills-C CTest timeout alone is now 1,200 s and its finite native
subprocess limit is 600 s (previously 240/180 s). Other tests and their limits
are untouched. These limits allow hashing, disk spooling, tracing and comparison
while other worktrees share the host; they do not change any result checks.

Additional diagnostic/retry commands:

```sh
ctest --test-dir build/native -R '^skills-c-exact$' --output-on-failure
/usr/bin/time -l ctest --test-dir build/native -R '^skills-c-exact$' --output-on-failure
/usr/bin/time -l ctest --test-dir build/native -R 'skills-c-' --output-on-failure -V
```

The subsequent 1,200/600 s attempt reached the native subprocess timeout after
600 s without an observation-mismatch report (819.68 s total CTest elapsed,
106.87 s user/8.41 s system CPU; maximum resident set 46,022,656 bytes). At its
limit the trace had emitted 1,566,361,345 of 1,815,139,879 expected synthetic
output bytes. The final finite limits are therefore 3,600 s for this one CTest
and 2,400 s for its native subprocess. Final arm64 validation uses the required
full suite, avoiding an extra complete arm64 pass; x64 retains the scoped run.

A one-second, 10 ms-interval profile of the old x64 trace found 26/83 main-thread
samples in synchronized stdin reading and 20/83 in output formatting. The
trace transport now uses C++ buffered stdin without stdio synchronization/ties
and C++17 `to_chars` for canonical unsigned decimal output, avoiding locale
formatting. Input validation and observations are unchanged. The pre-optimization
x64 diagnostic run was deliberately stopped to rebuild this runner.

A before/after benchmark used the identical first FPU configuration: all 4,346
scenarios, 113,448,848 output bytes. Before: 38.66 s wall, 10.45 s user, 0.67 s
system, 75,706,368 bytes maximum RSS. After: 17.44 s wall, 5.92 s user, 0.48 s
system, 55,029,760 bytes maximum RSS. Both native outputs match the independently
computed Python expected SHA-256
`ea870ed483b009ca27521ffec0f33d7062bf068b2d7593aa0281943a1568ba44`.
Wall times share a contended host and are not a controlled host-performance claim;
CPU time and exact output were measured for the same workload. Streaming adds
temporary disk I/O to avoid retaining multi-gigabyte copies in memory.

After this measured transport improvement, final finite limits are **1,800 s
CTest / 1,200 s native subprocess**, reduced from the provisional 3,600/2,400 s
budget. Only this test's limits change. The final larger-budget x64 and full
arm64 suite runs use the optimized transport.

Benchmark commands (inputs and outputs stay under ignored `work/`):

```sh
sample 13319 1 10 -file work/unspokenword/trace-sample.txt
/usr/bin/time -l work/unspokenword/trace-before-x64 < work/unspokenword/benchmark-input.tsv > work/unspokenword/benchmark-before.out 2> work/unspokenword/benchmark-before.time
/usr/bin/time -l build/native-x64/champions-skills-c-trace < work/unspokenword/benchmark-input.tsv > work/unspokenword/benchmark-after.out 2> work/unspokenword/benchmark-after.time
/usr/bin/time -l ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
```

Final optimized x64 scoped CTest through Rosetta passed **2/2 in 96.01 s**.
All 69,536 complete observations, all fifteen historical corpus hashes and the
transport checks passed. Complete native observation SHA-256:
`58540a9370add08cf4ef0e98632ec98d9786977cf7fb7cdd70dd85d2dd40fd69`.
The timed run recorded 96.04 s wall, 54.00 s user, 3.05 s system and
244,338,688 bytes maximum resident set. Direct freshly rebuilt API tests pass on
both architectures. The final full arm64 suite follows.

The required final full Release arm64 command
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
370.17 s**. Its skills-C exact test passed in **43.14 s**, matched all 69,536
observations and all fifteen earlier corpora, and produced the same complete
native observation SHA-256 as Rosetta:
`58540a9370add08cf4ef0e98632ec98d9786977cf7fb7cdd70dd85d2dd40fd69`.
The final full suite had no failures or timeouts and needed no retries.

Final generation, Release configure/build, direct API execution on both
architectures, the optimized x64 scoped CTest, full arm64 CTest, Python syntax
and whitespace checks all passed. Earlier timeout-only diagnostics, the stopped
profiling runs and the initial API compile error are recorded above; none changed
the pinned expected observations. The research checkout's status is empty.
No private files, Documents or Desktop inputs were accessed. Console fidelity,
Windows, Norrath, actual game-content tables and Session/scene integration remain
unverified; delegated engine services remain explicit callbacks. The work is
committed locally only; no push, pull request or issue was created.


## Poison Bolt continuation (2026-10-14)

Family chosen: all five methods of **Poison Bolt (89)**. The pinned authored
`ref_skills_c.py` class, the skill-89 written specification and executed notes
are the evidence. No research files were changed. The AST recount gives five
top-level functions and 170 directly declared class methods, 175 total. Five
complete tested methods bring the numerator from 150 to 155. Inherited RNG
helpers, partial dispatch and incomplete stack-list methods are not counted.
The remaining inventory above now has 20 units, including three active families.

Operations 149–153 append to the existing enum: cast
`[skill,level,caster,x,y,z,facing]`, constructor
`[object,x,y,z,facing,level,caster,skill]`, run `[object]`, touch
`[object,other]`, and burst `[object]`. Cast always returns zero; the other four
return empty. Published Skill cast/mana bodies dispatch with signed this
adjustment and position-pointer expansion. Earlier operation and service numbers
and vectors remain unchanged.

Cast preserves adjustment, both unconditional caster flashes, allocation failure
and delegated projectile construction. Construction retains embedded light writes,
signed16 heading/level, exact velocity arithmetic, both paired handles, frame
wrap and first-enemy selection with explicit stack cleanup. The inherited
projectile constructor owns its base fields, owner handle and shared draw; this
port neither fabricates those fields nor duplicates that draw.

Run preserves the strict signed homing-start gate and the **nonwrapped** angular
difference, including both sides of the 0x222 clamp. Collision is a callback
that may reenter the touch method. Frame, hit and expiry are reloaded afterward;
terrain and successful touches retain both separate expiry writes. The live path
uses the old position captured before collision, 36 private-stream trail
particles, and light relinking. Expiry plays the end sound, bursts and then calls
the signed-adjusted object destroy slot. Missing services and callback exceptions
retain the preceding state and call/write histories.

Touch keeps the ownerless prop/creature distinction, enemy and blocking gates,
owner-handle reload after the enemy callback, signed16 damage bands, tier-1 range
for every owner and timer-stat reload after damage. The hit flag is written only
after damage and the optional effect timer succeed. Burst uses the second owner
handle at **object+0x4c**, retains all three statistic calls before testing radius,
passes narrowed raw32 products to the delegated gas constructor and emits all
321 particles. Vertical velocity uses ordered soft-double absolute value and
two multiplications from caller-supplied double constants.

Services 99–102 append as `ProjectileConstructor`, `ObjectMoveAndCollide`,
`PoisonGasConstructor` and `CreatureSetEffectTimer`. Their flattened signatures
are respectively `[object,x,y,z,owner,0]`, `[object,vx,vy,vz]`,
`[block,x,y,z,lifeFrames,radius,0,owner,value]`, and
`[creature,3,owner,secondsRaw]`. Collision requires exactly one raw32 result.
World queries, damage, lights, audio, particles, timers, construction and
object destruction remain explicit callbacks; no delegated behavior is synthesized.

The generator adds 562 authored synthetic scenarios across all 16 FPU modes:
rank/facing/frame boundaries, allocation failure, owner/target lists, signed
nonwrapped turn clamps, all collision bits, repeated hits and ticks, damage gates,
radius decisions, private seeds, zero/negative/subnormal/infinite/NaN double
inputs and callback mutation/exception prefixes. Direct API tests cover published
bodies, negative this adjustment, flattened callback signatures, malformed scalar
results, missing-service prefixes, callback replacement, reentrant collision touch,
owner removal, timer failure, one-time hits and recursion recovery. All sixteen
historical corpora are independently hash checked, including the complete 69,536
Unspoken Word-era rows.

Private acceptance is skipped: `../../private-validation` is unavailable by
owner instruction. No Documents or Desktop paths were read. Console fidelity,
Windows, Norrath, real game-content tables and Session/scene integration remain
unverified. Rosetta execution is not Intel-host verification.

Validation commands (logs under ignored `work/poisonbolt/`):

```sh
python3 work/poisonbolt/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

During iteration, the new-only checker initially exposed synthetic callback setup
errors and the trace runner's old service-id ceiling; both were corrected before
regenerating the final corpus. A direct collision-signature assertion initially
expected exact zero sine at facing zero. The pinned math returns `0x34800000`,
so the test now checks its exact `0x38af0000` velocity residue. An expiry-recursion
test first reached the write-history resource bound because it repeated the
321-particle burst; it now reenters from collision, testing the recursion-depth
guard and subsequent recovery directly. No previous expected row was changed.
The final config-0 new-only checker passes all 562 scenarios, and freshly rebuilt
API tests pass on both architectures.

Final generation passed with **4,908 scenarios / 78,528 complete-state vectors**
(8,992 new rows). Plain-vector SHA-256:
`8d6fefbb859175c46299107623977e9f3ca9b96fe41a146ca319cb7aacfccf49`.
The compressed fixture is 195,622,962 bytes; the manifest's generator identity
also matches the final generator file.

The final Release x64 scoped CTest through Rosetta passed **2/2 in 67.11 s**;
its exact test took 65.90 s. All 78,528 complete observations, all sixteen earlier
corpus identities and the malformed/tampered transport checks passed. Native
observation SHA-256:
`837539c7dd51932a375379e2792af7909c96e8f62859137ee2a6f06083a4ae33`.

The required final full arm64 Release suite passed **102/102 in 383.92 s**.
Its skills-C exact test passed in **46.47 s**, matched every observation and all
sixteen earlier corpus hashes, and produced the same native observation SHA-256
as x64 above. There were no failures or timeouts in either final CTest run, so
no isolated timeout retry was needed.

Final generation, Release configure/build, direct API checks on both
architectures, x64 scoped CTest, full arm64 CTest, Python syntax, manifest identity
and whitespace checks all passed. The research checkout's tracked status remains
empty. No private game inputs, Documents or Desktop paths were accessed. The
three remaining active families and other inventory entries above remain
portable from existing public evidence. Local commit only; no push, pull request
or issue was created.


## Summon Badger continuation (2026-10-14)

Family chosen: **Summon Badger (90), all four methods**. Evidence is the same
immutable reference pin, skill-90 specification and executed corrections above.
The AST audit independently counts five top-level functions and 170 directly
specified class methods: 175 units. Operation names minus inherited RNG helpers
and the two incomplete stack-handle methods, plus five complete internal helpers,
match 159 distinct reference units. All four new methods are fully tested;
refusal-only dispatch and delegated creature behavior are excluded. The remaining
inventory above exactly matches the 16 unported names.

Operations 154–157 append: can-use `[skill,caster]`, cast
`[skill,level,caster,x,y,z,facing]`, constructor
`[spell,level,caster,skill,badger]`, and run `[spell,unusedPosition,unusedFacing]`.
Can-use returns zero/one; cast returns the spell allocation or zero; ctor/run
return empty. Published can-use/cast/mana bodies dispatch with signed this
adjustment and explicit position-pointer expansion. Earlier operations and
callback ids retain their numbers.

Can-use selects the caller-supplied name for each player slot 0–3; larger
unsigned slots return before any world lookup. Nonplayers share their supplied
name. Cast retains level adjustment, ordered facing arithmetic, terrain radius
48 and strict absolute height difference greater than 24 after binary32
subtraction. The reference's double conversion is exact for a binary32 value;
ordered magnitude comparison retains infinity and NaN behavior. Fallback success
uses a signed low byte. Strength, property selection and post-constructor player
checks reload caster state at the reference's callback boundaries.

The delegated creature constructor owns its own draws and base fields. The port
preserves the full 64-bit flag word, followed slot, charm/team and home range,
flash/light/sound ordering and all three distinct allocation failures. Only the
spell base makes the port's one shared draw; construction resets the clock and
links the paired badger handle without fabricating allocation success.

Run distinguishes empty handles (destroy) from nodes with null objects (no-op).
It evaluates the old clock's triangular share, advances by the pinned 1/60 word,
then tests visibility using the new clock. The second-half share retains
`1-(clock-1)` arithmetic. Zero/nonpositive shares return before model services.
Particles visit model samples 0,8,16,... with signed remainder; zero counts keep
advancing the index. Each particle uses the spell's private ball generator,
never an extra sample-index draw. Position is captured once; model/facing/scale
are reloaded as specified. Missing services, malformed results and callback
exceptions preserve prior clock, visibility, writes, calls and RNG state.

Services 103–106 append: `WorldFindObjectByName` `[name]` returns a scalar;
`WorldFindFreeSpot` `[x,y,z,71,24,0]` returns `[x,y,z,foundByte]`;
`CastBadgerConstructor` `[block,type,x,y,z,signedFacing,properties,strength]`
is void; `CreatureSetHomeRange` `[adjustedBadger,500]` is void. Existing terrain,
team/charm, model/sample, sound, light, particle and destroy services are reused.
All delegated engine behavior stays an explicit callback; malformed scalar,
free-spot and sample results refuse. No retail statistics or content are embedded.

The generator appends 469 synthetic scenarios across all 16 FPU configurations,
covering slot/name and allocation gates, signed level/facing boundaries, exact
height/fallback thresholds, signed found bytes, null/live handles, both clock
halves and expiry, zero/negative sample counts, private seeds, repeated ticks,
NaNs/infinities and callback mutations/exception prefixes. Direct API tests cover
published virtual bodies, signed this adjustment, position expansion, distinct
allocation results, service shapes, malformed results, self-replacing handlers,
clock/visibility failure prefixes, the fixed sample stride and recursion recovery.
All seventeen prior corpus identities are independently retained, including
all 78,528 Poison Bolt-era rows. No prior expected observation is changed.

Private acceptance is skipped: `../../private-validation` is unavailable by
owner instruction. No Documents or Desktop inputs were read. Console fidelity,
Windows, Norrath, real content tables and Session/scene integration remain
unverified. x64 execution under Rosetta does not establish Intel-host execution.
The two remaining active families and other listed units have public evidence.

Validation commands (logs under ignored `work/summonbadger/`):

```sh
python3 work/summonbadger/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

During iteration, the direct API compilation found two local variable names
already used by earlier test families. The new test block now has its own scope;
both architecture builds and freshly rebuilt API tests are required to pass.
The config-0 new-only checker passed all 469 observations on its first execution.

Final generation passed with **5,377 scenarios / 86,032 complete-state
vectors** (7,504 new rows). Plain-vector SHA-256:
`758278a0f77f8d4f177f31660831a6c198b9f35b026569c48fb6e3bcfca8df81`.
The compressed fixture is 201,824,010 bytes.
Final direct API execution passed on both architectures. The arm64 scoped
CTest passed **2/2 in 52.43 s**, including all 86,032 complete observations,
all seventeen historical corpora and malformed/tampered transport checks.
The x64 scoped CTest through Rosetta passed **2/2 in 74.02 s** (exact test
72.94 s). Both architectures produced the same complete native observation
SHA-256: `4ce7c8c29c1678bb51f0c36c3a30932f7e6bb6dc9c0e0fcc49edc916ae1f2240`.
No scoped test failed or timed out.

The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
403.19 s**. The skills-C exact test passed in **50.84 s** and produced the
same complete observation SHA-256 recorded above. No full-suite test failed or
timed out; no timeout retry was needed.

Final generation, Release configure/build, freshly rebuilt direct API tests on
both architectures, both scoped CTest runs, full arm64 CTest, Python syntax,
manifest identity, historical corpus compatibility and whitespace checks passed.
The research checkout's tracked status is empty. All four Summon Badger methods
are tested; original creature construction and engine services remain delegated.
Only nine skills-C files changed, with no shared build files or research edits.
Local commit only; no push, pull request or issue was created.

## Root continuation (2026-10-14)

Family chosen: **Root (85), all four methods**. The evidence is the immutable
reference pin, skill-85 specification and executed corrections above. The
reference AST contains five top-level functions and 170 directly declared
class methods, 175 units. The generator now checks that its complete tested
method inventory plus five internal helpers matches 163 distinct reference
units. Partial dispatch, inherited RNG methods and the two incomplete stack
handle methods are excluded from the numerator. Twelve reference units remain;
Cone of Shock is the remaining active family.

Operations 158–161 append to the existing enum: can-use `[skill,caster]`, cast
`[skill,level,caster,x,y,z,facing]`, constructor `[spell,level,victim,skill]`,
and run `[spell,unusedPosition,unusedFacing]`. Can-use returns zero/one, cast
returns zero, ctor/run return empty. Published Skill can-use/cast/mana bodies
`0x2e6534`, `0x2e6598`, `0x2e6894` dispatch with signed this adjustment;
cast expands the supplied position pointer. No existing operation or callback
id changes. No new service ids are needed.

Can-use preserves the PvP/player short circuit and warning callback order,
including reloading the player slot after text lookup. Cast preserves adjustment,
sound, signed victim limit, signed16 facing and root timer, enemy and boss gates,
allocation failure counting and null-spell attachment. It clears two stack lists
at the exact point specified by the pinned Python body (after the iteration).
The written notes describe an earlier cleanup point; this port follows the
explicit pinned executable contract, without inventing an alternate order.
Construction retains one shared draw, signed8 rank, particle group creation,
post-callback victim position capture and integer duration statistic.

Run retains clock advance before phase selection, seven pending particles,
the strict delay comparison and one shared success roll. Equality of chance
and `roll % 100 + 1` fails. Failure emits 100 private-RNG ball particles before
destruction; success flashes both quads, writes only a nonplayer's timer and
increments the reloaded phase. The timer uses the **stored raw32 integer word
multiplied by 60 and narrowed to a halfword**, without float conversion.
Hold samples 22 points with signed remainder; each makes an index draw and a
required discarded ball draw sequence. Its sample service uses output-normal
flag one and scale one, rather than the generic helper's flag zero and owner
scale. The normal is unused; the C++ callback returns the three position words.
Player pullback preserves 2-D square-root association, atan2 argument order and
halfword angle narrowing. Duration comparison converts the stored signed32
word to float, subtracts the exact delay from the reloaded clock and destroys
only after a strict comparison. Unknown phases still advance and expire.

All world, model, sound, HUD, particle, stack-list and destruction behavior
remains explicit callbacks. Stack matrices retain the existing `[group,0]`
sentinel transport; no engine matrix service or renderer integration is implied.
Malformed scalar/sample results, absent callbacks and callback exceptions refuse
with their preceding writes, calls and RNG state retained. Zero sample counts
refuse after the index draw, at the reference remainder point.

The generator appends **447 authored scenarios / 7,152 complete observations**
across all 16 FPU configurations. Coverage includes PvP modes, cast ranks and
signed limits, failed allocation, every eligibility gate, clock boundaries,
all phases, signed durations/sample counts, private seeds, unusual float clocks,
repeated ticks and mutation/exception prefixes at every reachable service.
Direct API cases additionally verify published bodies, signed this adjustment,
position expansion, exact flattened signatures, self-replacing callbacks,
malformed results, failed-allocation accounting, both chance outcomes, raw-word
nonplayer timers, player pullback, zero-sample failure and recursion recovery.
All eighteen preceding corpora are independently hash checked, including all
86,032 Summon Badger-era observations.

Private acceptance is skipped: `../../private-validation` is unavailable by
owner instruction. No private inputs, Documents or Desktop paths were accessed.
The research repository was read only. Console fidelity, Windows, Intel-host
execution, Norrath, original game tables and Session/scene integration remain
unverified. x64 execution below is under Rosetta on this arm64 host.

Validation commands (logs under ignored `work/root/`):

```sh
python3 work/root/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

During iteration, the new-only comparison found incorrect synthetic callback
ids and an incorrectly shaped point/normal return in the new Python setup.
Those fixtures were corrected before full regeneration. The final new-only
comparison passed all 7,152 observations; native and x64 direct API checks passed.
No earlier expected observation was edited.

Final generation passed with **5,824 scenarios / 93,184 complete-state vectors**.
Plain-vector SHA-256:
`1be850f8b2fbcaa8c4c575cd7f8f4eb29ae1172da8d735771d1417e8b64b8844`.
The compressed fixture is 208,740,084 bytes. The generator identity matches
the manifest. The arm64 scoped CTest passed **2/2 in 46.00 s** (exact test
45.92 s); x64 through Rosetta passed **2/2 in 65.14 s**. Both retained every
previous corpus identity and passed malformed/tampered transport checks.
Both produced the same complete native observation SHA-256:
`155dd68ef12b16840a00d2e3bd06e61c2133f616e5c1f037d74d4f3430331d9b`.
Neither scoped test failed or timed out.

The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
364.62 s**, with no failures or timeouts and no retry required. The skills-C
exact test passed again with the same native observation hash above.
Final generation, both Release builds, freshly rebuilt direct API checks,
both scoped CTest runs, full CTest, Python syntax, manifest identity,
historical corpus compatibility and whitespace checks passed.
The research checkout's status is empty. Eight existing skills-C files changed;
no shared build files or research files were edited. The work is committed
locally only, with no push, pull request or issue.


## Cone of Shock continuation (2026-10-14)

Family chosen: **Cone of Shock (101), all three methods**. Evidence is the
immutable Python contract pin and skill-101 written/executed notes above.
The generator's AST audit counts five top-level functions plus 170 class methods,
175 units; its tested inventory now counts 166 distinct complete units. Partial
Base dispatch, inherited RNG methods and incomplete stack branches do not count.
Nine general units remain portable from public evidence for the next continuation.

Operations 162–164 append: cast `[skill,level,caster,x,y,z,facing]`, constructor
`[spell,level,caster,skill]`, and run `[spell,x,y,z,facing]`. Cast returns the
allocated spell or zero; ctor/run return empty. Published Skill cast/mana bodies
`0x31b8f0` and `0x31ba94` dispatch with signed this adjustment and position pointer
expansion. Existing operation and callback ids retain their numbers.

Cast adjusts with `(level,1)` before allocating 0x58 bytes. Construction captures
the light position before the Spell base's one shared draw, writes the canonical
embedded light at +0x18, links it and stores the looping sound handle at +0x54.
Run retains signed-byte rank, signed16 facing and arc narrowing, integer division
and remainder toward zero, including explicit zero-divisor refusals. Decoration
uses one private draw before allocation; no delegated lightning draw is fabricated.
Service 107, `LightningBoltConstructor2`, carries
`[block,farX,farY,farZ,nearX,nearY,nearZ,11,0]` and returns empty.

The second frame gate reloads the global frame after decoration callbacks. Each
enemy check uses the reloaded owner's signed virtual adjustment. Each hit reloads
skill/rank/owner, draws once from the same private stream, and sends the existing
canonical damage descriptor transport with one lightning slot and three empty
slots. The hit sound precedes one stack cleanup callback. Final flicker reloads
the frame and retains the four ordered radius/falloff writes before relink.
Empty/nonenemy/duplicate object lists, callback mutations, failed allocation and
exception prefixes preserve the pinned ordering. No lifetime/expiry behavior is
invented: the original owner's state changes and destructor remain delegated.

The generator appends **533 synthetic scenarios / 8,528 complete observations**
across 16 FPU configurations. It covers signed rank/frame/angle boundaries,
zero denominators and periods, private seeds, both independent frame gates,
allocation failure, unusual statistics and float inputs, repeated runs, ordered
queries and callback mutations/exceptions. Direct API checks additionally cover
published bodies, allocation sizes, exact service shapes, signed this adjustment,
canonical damage, absent/malformed callbacks, self-replacement, failure prefixes
and recursion recovery. The Root-era 5,824-scenario corpus is retained as the
nineteenth independent historical hash; every prior expected row stays unchanged.

Private acceptance is skipped: `../../private-validation` is unavailable by
owner instruction. No Documents or Desktop paths were accessed. The research
checkout remains read only. Console fidelity, Windows, Intel-host execution,
Norrath, original content and Session/scene integration remain unverified.
x64 execution through Rosetta is distinct from Intel-host validation.

Validation commands (logs under ignored `work/coneofshock/`):

```sh
python3 work/coneofshock/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3 -V
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

During iteration, compilation required explicit raw32 casts for narrowed damage
halfwords. Synthetic missing-callback cases were changed to throwing callbacks:
the Python harness supplies defaults for some absent services, whereas the native
API deliberately refuses absent services. Direct API tests cover that boundary.
The direct zero-denominator test initially used rank -8 (a valid negative period);
it was corrected to rank -4. The complete new-only comparison passed all 8,528
observations. Review also added ranks -124/-120 with `INT_MIN` frames; widened signed
remainder avoids C++ overflow for `INT_MIN % -1` while matching the reference.
Final freshly rebuilt tests below are the completion evidence.


Final generation passed with **6,357 scenarios / 101,712 complete-state vectors**
(8,528 new rows). Plain-vector SHA-256:
`36a4a5b17b5c11637a59f7186f61592168a80e2d2135ab4ba1b89c38e77e96ec`.
The compressed fixture is 212,658,692 bytes; generator identity matches its
manifest. The final arm64 scoped CTest passed **2/2 in 50.17 s** (exact test 50.07 s).
x64 scoped CTest through Rosetta passed **2/2 in 73.20 s** (exact test 72.10 s).
Both matched every expected observation, retained all nineteen historical corpus
hashes and passed malformed/tampered transport checks. No scoped test failed or
timed out. x64 complete native observation SHA-256:
`12ded7ea16dac91ba5f8f0b13a9d2bf2725c7b13d40253261fc81616ac6b768e`.


The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3 -V` passed **102/102 in
410.11 s**, with no failures or timeouts; no isolated timeout retry was needed.
Its skills-C exact test passed in 58.77 s and produced the same complete native
observation SHA-256 as x64 above. Existing transport harnesses reported their
own skipped subtests; no skills-C check was skipped.

Final generation, both Release builds, freshly rebuilt direct API checks,
both scoped CTests, full CTest, Python syntax, generator/manifest identity,
all nineteen historical corpus hashes and whitespace checks passed. The research
checkout's status is empty. Only nine existing skills-C files changed; no shared
build files, research files, or private inputs changed. All three Cone of Shock
methods are fully ported and tested; nine general reference units remain as
listed above. The work is committed locally only; no push, pull request or issue.


## Complete paired handle-list continuation (2026-10-14)

Family chosen: **complete `handle_link` and `handle_clear`, including both
stack branches**. All active skills-C families were already complete. This
continuation finishes the temporary-list boundary used by spell queries.
Evidence is the same immutable Python contract: `Base.handle_link` delegates
`head == 'stack'` to `call('handle_link', head, back, obj)`;
`Base.handle_clear` delegates to `call('handle_clear', head)`. No new constants,
engine allocator or stack storage are inferred.

Operations 165–166 append `HandleLinkStack [0,back,object]` and
`HandleClearStack [0]`. The required zero word denotes the string sentinel
only in these named operations. Existing operations 26 and 28 keep address
zero as real sparse memory. Service 108, `HandleLinkStackCallback`, appends
`handle_link [0,back,object]`; clear reuses service 90 and `[0]`. Both operations
forward the callback result unchanged, including empty or multiple result words,
as the reference returns the delegated result. Missing services explicitly
refuse after logging; callback exceptions retain their changes and call prefix.
The callee behavior stays delegated, exactly as the reference requires.

The AST denominator remains five top-level functions and 170 class methods,
175 units. Both complete handle methods now count, raising the numerator by two
to 168. Inherited RNG methods, partial dispatch and refusal-only integration
routers remain excluded. Seven reference units remain in the updated list.

The generator appends 73 authored scenarios / 1,168 complete observations
across all 16 FPU configurations. Cases cover zero/high-bit/max return words,
empty and multiple returns, repeated calls, mutations of the free-list word and
object memory, throwing callbacks, and coexistence with memory-backed address
zero. Direct API checks cover exact signatures, missing callbacks, callable
self-replacement, propagated exceptions, recursion refusal/recovery, malformed
sentinel/argument shapes, no invented pool writes or random draws, and unchanged
address-zero selection. The prior 6,357-scenario corpus is independently pinned
as the twentieth historical compatibility hash.

Private acceptance was skipped: `../../private-validation` is unavailable by
owner instruction. No private inputs, Documents or Desktop paths were accessed.
Research files were read only. Console fidelity, Windows, Intel-host execution,
Norrath, original content and Session/scene integration remain unverified.
The x64 checks execute through Rosetta on this arm64 host.

Validation commands (logs under ignored `work/stack-handles/`):

```sh
python3 work/stack-handles/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

During iteration, the API check exposed that its `refuses` helper catches only
`Unsupported`, while the authored throwing clear callback used `runtime_error`.
The clear case now uses `Unsupported`; a separate link case verifies arbitrary
`runtime_error` propagation and retained mutation. The corrected API checks
passed on both architectures. The new-only comparison passed all 1,168
observations. No earlier expected observation was edited.

Final generation passed with **6,430 scenarios / 102,880 complete-state vectors**
(1,168 new rows). Plain-vector SHA-256:
`1a11411a2d52c108b3f6de566f33ce4ae2ea73a28aa3ccc047dc0e4f5a285d55`.
The compressed fixture is 212,691,987 bytes; the generator identity matches its
manifest. Fresh direct API checks passed on both architectures. The arm64 scoped
CTest passed **2/2 in 57.74 s** (exact test 57.64 s); x64 through Rosetta passed
**2/2 in 75.74 s** (exact test 74.65 s). Both matched every expected observation,
retained all twenty historical corpus hashes and passed malformed/tampered
transport checks. No scoped test failed or timed out. Both complete native
observations have SHA-256:
`b60a796e5858b5e4d2074192a5927128ac5c7f78ef5c32fb360d0d7a0179df7d`.

The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
363.71 s**. Its skills-C exact test passed again in 65.55 s. No full-suite test
failed or timed out; no isolated timeout retry was needed.

Final generation, both Release builds, freshly rebuilt direct API checks,
both scoped CTests, full CTest, Python syntax, generator/manifest identity,
all twenty historical corpus hashes and whitespace checks passed. The research
checkout's tracked status is empty. Nine existing skills-C files changed;
no shared build files, research files or private inputs changed. The family is
complete; seven general reference units remain portable from public evidence.
Local commit only; no push, pull request or issue was created.


## General contract helper continuation (2026-10-14)

Family chosen: **all four top-level general helpers**, from the immutable
`ref_skills_c.py` pin above. This is the largest remaining family; all declared
active skill families were already complete. The denominator is independently
AST-counted: five top-level functions and 170 directly declared class methods,
175 units. Four new complete tested helpers bring the numerator to 172.
The three general Base dispatch methods remain uncounted.

The public `shape_result` API resolves a name in a caller-owned EXTERNALS
registry and refuses an unknown name. A lower-level overload accepts the selected
row's `ints` and `ret` fields directly. Registry metadata is caller-supplied, like
the existing caller-supplied skill tables; no callback or callee execution is implied.
`ResultValue` preserves None, scalar raw64, list, tuple, bytes and tagged buffer
results. Output consumption precedes nullable-output suppression; output-only
fields do not advance the argument cursor. Exhausted outputs use exactly the
reference defaults. A string return uses the next unconsumed output, or an empty
byte buffer; scalar/float/void selection and singleton tuple collapse are retained.
None and an empty supplied output list have the same reference behavior.

`c_div` and `c_rem` accept signed32 integers, the PS2 contract domain. Their signed64
result preserves `INT32_MIN / -1` without C++ undefined behavior. Division by zero
refuses. Python arbitrary-precision integers outside this native raw32 domain are
not an API promise. `desc` masks every supplied word to 16 bits, retains each
slot's actual length, pads with one-word `(0xffff,)` slots only until there are
four slots, and retains more than four supplied slots. The 18-byte header remains
sixteen zero bytes followed by two 0xff bytes. All four helpers are pure: no memory,
callback log, random stream or engine service changes.

Operations 167–170 append without renumbering anything:

- `ShapeResult`: return-kind ASCII word, signature length and ASCII words,
  argument count and structured values, integer and float structured values,
  output count and structured values. Returns one structured value.
- `SignedDivide` / `SignedRemainder`: two signed raw32 operands; returns signed64
  as low then high raw32 words.
- `Descriptor`: slot count followed by each slot's length and raw32 words;
  returns 18 header-byte words followed by slot count and length-prefixed slots.

Structured values encode kind 0=None, 1=scalar (one raw32 word), 2=list,
3=tuple, 4=bytes, 5=tagged buffer. All nonscalar non-None kinds contain a count
followed by recursively encoded children; the buffer has one child. The public API preserves raw64 scalars; this transport selects the raw32 subset
and refuses an unrepresentable result. It preserves nesting and sequence type,
rather than flattening away distinctions.
The transport rejects truncation, unknown tags, trailing words and nesting at
64 levels. These are host bounds, not inferred game behavior.

The generator exercises **every distinct signature in the pinned EXTERNALS
registry** with zero/nonzero/high-bit optional arguments, absent/partial/excess
outputs, nested sequences and buffers. Signed arithmetic includes all sign
combinations, zero divisors, signed limits and the widened overflow pair.
Descriptors cover empty slots, extra slots, arbitrary slot lengths and high-bit
masking. There are 1,470 new synthetic scenarios / 23,520 complete observations
across all 16 FPU configurations. The preceding 6,430-scenario corpus is retained
as the twenty-first independent historical compatibility hash. Direct API checks
cover named/unknown row lookup, output consumption, defaults, floating/void/buffer selection, malformed
transport, descriptor lengths/masking and absence of side effects.

Private acceptance is skipped: `../../private-validation` is unavailable per
owner instruction. No private files, Documents or Desktop paths were accessed.
The research checkout was read only. Console fidelity, original content,
Norrath, Windows, Intel-host execution and Session/scene integration remain
unverified. x64 execution through Rosetta is separate from Intel-host validation.

Validation commands (logs under ignored `work/general-helpers/`):

```sh
python3 work/general-helpers/check_new.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Final generation passed: **7,900 scenarios / 126,400 complete-state vectors**.
Plain-vector SHA-256:
`f8b45b46f85c6fef684481d085199e21abf0c316ba4b2bf9eee511a5fab4d773`.
The compressed fixture is 212,885,612 bytes, and its generator identity matches
the manifest. Every one of the 21 previous corpus identities passed unchanged.
The fresh arm64 scoped suite passed **2/2 in 64.96 s** (exact comparison 64.87 s);
x64 through Rosetta passed **2/2 in 85.14 s** (exact comparison 84.98 s).
Both returned the same complete native observation SHA-256:
`2807a65682f1dbc4b257355846e52cc04a8632916378f0556d0f792288ebe16a`.
No scoped test failed or timed out. The new-only comparison also passed all
23,520 observations on both architectures. Direct API checks preserve 64-bit
callback results, including high-bit nullable arguments and outputs. The raw32
vector transport remains explicit; it never silently truncates a wider result.

An initial full-suite run was intentionally interrupted for the raw64 API
refinement, then restarted with freshly built binaries. This was not a target
test failure. The final full-suite result is recorded below.

The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
341.10 s**. Skills-C exact passed again in 50.00 s. No final-suite test failed
or timed out; no isolated timeout retry was needed.

Both Release builds, direct API checks, scoped suites, the full suite, Python
syntax, generator/manifest identity, all historical compatibility hashes and
whitespace checks passed. The research checkout's status is empty. Only eight
existing Skills C files changed: the header, source, API tests, vector generator,
vector harness, manifest, compressed fixture and this page. No shared build file,
research file or private input changed. This family is complete; the three Base
dispatch methods remain portable from the pinned public evidence. Local commit
only, with no push, pull request or issue.


## Base dispatch continuation (2026-10-14)

Family chosen: **Base dispatch: `call`, `skill_call`, `table_call`**. All three
entry points were extended, but the family is not fully complete. Independent
AST accounting of the immutable reference still finds five top-level functions
and 170 directly declared class methods, 175 units. Only `call` is newly counted;
there are now 173 fully ported units. Both virtual dispatchers remain uncounted
because of the five inherited body routes listed above. No refusal is counted as
an implementation.

General `Model::call(name, args)` accepts the existing `ResultValue` types. It
masks only top-level scalar integers to raw32, logs canonical arguments before
lookup, copies the active callback for self-replacement safety, and forwards its
result unchanged. Nested scalar raw64 values and raw64 returns survive the direct
API. A missing named handler applies the reference `shape_result` defaults using
the caller-owned `signatures` registry; an unknown registry name refuses after
logging. This pure reference plumbing does not install any engine services.
Existing numeric service callbacks retain their required-handler behavior.
Named callbacks may mutate memory, throw or recursively call the model; failure
prefixes persist and the shared depth guard recovers. The six value kinds remain
None, scalar, list, tuple, bytes and tagged buffer. Arbitrary Python objects are
outside this canonical native value domain.

`table_call` selects the supplied class, object, table offset and slot. It
preserves signed16 adjustment and raw32 `this` wrapping. All 18 locally owned
Spell bodies dispatch by address, independent of the requested slot; other class
names always delegate. `named_external` forces a named virtual callback. This
adapter takes canonical raw32 arguments; for a two-argument Spell follow call,
the first is a position pointer read as three raw words and presented as a tuple
to the callback. The reference's position-using bodies receive the same tuple.
Unforced inherited arena bodies refuse before fabricating a callback log. Unknown
body routes require a named callback or explicitly supplied result metadata.

Operations 171–173 append without renumbering anything:

- `NamedCall`: length-prefixed ASCII name and signature, return-kind ASCII word,
  argument count and structured values; returns a structured value. Registry
  metadata is explicit input, not embedded content or an engine service.
- `SkillCall`: skill, slot, remaining canonical arguments. The existing public
  `skill_call` wrapper retains its historical 0–15 bound; this general adapter
  supports slots through the explicit host bound 65,535.
- `TableCall`: length-prefixed ASCII class, object, table offset, slot and remaining
  canonical arguments. Slot bounds and memory-span bounds are host refusals.

Historical inputs and all seven-field snapshots are untouched. Scenarios using
these new operations append an eighth snapshot field containing all named calls:
`[order, name-byte-list, structured-argument-word-lists]`. `order` preserves each
named call's position relative to numeric calls. Named callback configurations
109–113 select `authored`, `Spell.v2`, `Holder.v2`, `Other.v3`, and `Skill.v16`;
their result vectors contain the structured value encoding from the general
helper transport. Default result signatures are supplied by each `NamedCall`
command; registry metadata is host configuration rather than simulated memory.

The generator appends 586 authored scenarios / 9,376 complete observations across
all 16 FPU configurations. They exercise every distinct EXTERNALS signature's
no-handler defaults, all supported value kinds, callback mutation/exception
prefixes, all 18 local Spell body routes, all 73 local Skill body addresses at an
unusual slot, forced spell callbacks, unknown virtuals, signed adjustments,
raw32 wrapping and arbitrary class routing. Direct API checks cover raw64 values,
masking depth, unknown names, recursion/recovery, self-replacement, inherited
refusals and malformed transports. The complete preceding 7,900-scenario corpus
is retained as the twenty-second independent historical compatibility hash.

Private acceptance is skipped: `../../private-validation` is unavailable per
owner instruction. No private files, Documents or Desktop paths were accessed;
the research checkout was read only. Console fidelity, actual game content,
Norrath, Windows, Intel-host execution and Session/scene integration remain
unverified. x64 execution through Rosetta is separate from Intel-host validation.

Validation logs are under ignored `work/base-dispatch/`. Final results follow.

Commands (all final checks must pass before committing):

```sh
python3 work/base-dispatch/probe.py
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
python3 work/base-dispatch/check_new.py
ctest --test-dir build/native -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

During iteration, the scenario-selection assertion caught that mixed protection
scenarios selected only their first spell family. Selection now runs separately
for each of the 18 bodies. The Python reference also rejected three initially
authored argument/result shapes: virtual cast positions supplied as integers,
a scalar instead of a statistic-range pair, and a scalar instead of an attach
point/direction pair. Those fixture inputs were corrected to the documented
canonical shapes before final generation. The initial-config new-only probe then
passed all 586 scenarios; no existing expected observation was edited.

Final generation passed: **8,486 scenarios / 135,776 complete-state vectors**.
Plain-vector SHA-256:
`37d08ee71e99486fed0d02f13d82fb5e3bf5007b861fdac7c60be7a0ad4b2138`.
The compressed fixture is 217,775,341 bytes; generator/manifest identity matches.
The new-only comparison passed all 9,376 observations. The final arm64 scoped
suite passed **2/2 in 41.61 s** (exact comparison 41.52 s); the final x64 through
Rosetta scoped suite passed **2/2 in 54.79 s** (exact comparison 53.98 s).
Both matched every expected observation and all 22 historical corpus hashes,
and passed transport-tampering checks. Neither scoped suite failed or timed out.
Both complete native observation SHA-256 values are:
`5ec6f1ec1831df9b73a4b781845946bd98ccd8a8057e498f22e0e8c98b5bbf58`.

The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
358.42 s**. Skills-C exact passed again in 62.57 s. No full-suite test failed or
timed out; no isolated timeout retry was needed.

Both Release builds, direct API binaries, scoped suites, the full suite, Python
syntax, generator identity, all historical corpus hashes and whitespace checks
passed. The research checkout's tracked status is empty. Nine existing skills-C
files changed: header, source, API tests, trace runner, vector generator, vector
harness, manifest, compressed fixture and this page. No shared build file,
research file or private input changed. Local commit only; no push, pull request
or issue. This family remains incomplete: the two virtual dispatchers require
the inherited arena behavior listed above before they can count as ported.


## Inherited Tagar’s Insects continuation (2026-10-14)

Family chosen: **complete inherited Tagar’s Insects lifecycle (87)**. Evidence is
`ref_arena_monsters.py` at the existing immutable commit and digest, with inherited
`ref_creature.py::is_enemy`, `slot37_team`, and their virtual selection rules.
All six Tagar methods are implemented: `tagar_cast`, `tagar_spell_ctor`,
`tagar_is_valid`, `tagar_pick_target`, `tagar_tick_victims`, `tagar_spell_run`.
Supporting arena spell construction and `_unit`/`_spell_ball` sampling are exact.
The existing skills-C spell constructor and RNG implementation are unchanged.

Operations 174–179 append to the existing module and transport:

| Operation | Arguments | Result |
|---|---|---|
| `TagarCast` | skill, level, caster, raw xyz, facing | spell or zero |
| `TagarCtor` | spell, level, owner, skill | empty |
| `TagarValid` | spell, object, marker-test boolean | zero or one |
| `TagarPickTarget` | spell, later boolean | object or zero |
| `TagarTickVictims` | spell | zero or one |
| `TagarRun` | spell, unused position, unused facing | empty |

Service 114 appends `creature_findSpell` (object, skill id, owner). Services
109–113 retain their existing named-callback transport assignments. Existing
service ids and vectors retain their numbers. The existing `handle_link` service
108 also represents the arena's delegated memory-head link; it carries the exact
head, object, object arguments and never invents an allocator. Queries, marker
lookup, handle operations, damage, allocation, sound and particles remain explicit
callbacks. Tagar damage has no descriptor: its two zero arguments remain zeros,
followed by the computed float low/high values. Unknown enemy/team virtual bodies
use the existing services with signed16-adjusted this; the inherited base enemy
and signed team-byte bodies execute locally. Known arena creature bodies placed
at unrelated enemy/team slots explicitly refuse; no unported body is relabelled
as an engine callback. Forced virtual services still delegate.

Victim traversal rereads the signed count, preserves marker destruction and tint
write order, computes player/nonplayer damage differently, and consumes five
private ball samples per active victim. First target selection uses list order;
later selection uses strict 3D distance and excludes existing victim handles.
Both preserve delegated stack-clear calls. Steering reflects errors outside a
quarter turn, truncates the fifth-error step toward zero, shortens ETA, computes
the arrival division before replacing speed, and preserves the duration's original
absolute-time store. Marker allocation consumes a shared draw only on success.
Wander and all 51 swarm particles preserve ordered private draws, sampling,
normalization and raw FPU arithmetic. The arena base constructor has no age store; Tagar subsequently clears its eight
victim handles. Unrelated stale memory is preserved.

Progress was recounted from the pinned AST: five top-level functions and 170
class methods, 175 total. These inherited dependency methods add no units to that
denominator. Both dispatchers still refuse Skeleton Summon, so 173 units remain
fully ported; neither refusal is counted. No claim of a completed whole contract
or playable scene is made.

Private acceptance was skipped because `../../private-validation` is unavailable
per owner instruction. No search for private inputs or reads under Documents or
Desktop were performed. Research was read only. Console fidelity, real content,
Norrath, Windows, Intel-host execution and Session/scene integration are unverified.
Cross-architecture comparison here uses arm64 and x64 through Rosetta.

Validation logs and new-only comparison helpers are under ignored `work/tagar/`.
Final command results and corpus identities follow below.

Commands used for this continuation:

```sh
python3 work/tagar/check_new.py --all
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
ctest --test-dir build/native -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

The final new-only arm64 comparison passed 546 scenarios / 8,736 observations,
covering all 16 FPU configurations. Both final direct API binaries passed.
Iteration corrected synthetic service identifiers, an overlapping adjusted skill
pointer and a stale-byte assertion that initially selected a cleared victim handle.
The forced inherited-cast comparison exposed a native callback position-shape
mismatch; numeric callbacks now flatten xyz, and named callbacks retain the tuple.
The final comparison includes both legacy and unusual forced slots. A generation
run was stopped when the last cases were added, then restarted from the final
source; that interruption was not a target test failure.


Final generation passed: **9,032 scenarios / 144,512 complete-state vectors**.
Plain-vector SHA-256:
`05460150d5c3d09771b6250a021ed464df494c6b3c56ff3850d911c9f9d2ff07`.
The compressed fixture is 254,857,671 bytes; generator/manifest identity matches.
All 23 historical corpus identities, including the entire preceding 8,486-scenario
corpus, passed unchanged. Final arm64 scoped CTest passed **2/2 in 44.26 s**;
x64 through Rosetta passed **2/2 in 64.73 s**. Both matched every observation,
passed transport-tampering checks and returned the same native observation hash:
`6bb778c38e927c1281490500ac0c9bcc50cdd01093965de803fb92d59d9ab040`.

The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
302.81 s**. No final scoped or full-suite test failed or timed out; no isolated
timeout retry was needed. Both Release builds, final API binaries, new-only
comparison, scoped suites, full suite, Python syntax, manifest identity and
whitespace checks passed. The research checkout remains clean.

Nine existing Skills C files changed: header, source, API tests, trace runner,
vector generator, vector harness, manifest, compressed fixture and this page.
No shared build file, research file or private input changed. This family is
complete within its documented callback and host domains; Skeleton Summon remains
portable from public evidence and keeps the two dispatchers uncounted. Local
commit only; no push, pull request or issue.


## Inherited Skeleton Summon continuation (2026-10-14)

Family chosen: **complete inherited Skeleton Summon lifecycle and reachable
construction helpers**. Evidence is the same pinned `ref_arena_monsters.py`,
with `ref_creature.py::filter_state`, base team-byte dispatch and the arena's
class-model/home-range bodies. No research file or shared build file changed.

Operations 180–187 append: `SkeletonCanUse`, `SkeletonCast`, `SkeletonCtor`,
`SkeletonRun`, `SkeletonSample`, `CosmeticDraw`, `CosmeticBall`, `SkelightCtor`.
Can-use takes skill/caster; cast takes skill/level/caster/raw xyz/facing;
spell construction takes spell/level/caster/skill/skeleton; run takes
spell/unused-position/unused-facing; sample takes caster/signed count;
cosmetic operations take a required zero marker; Skelight construction takes
object/raw xyz/facing/property-list/level. No old operation changes number.

The family includes all four inherited lifecycle entry points, cosmetic draw,
ball rejection and model sampling, the entire reachable Skelight constructor,
its no-type-name Creature constructor path, embedded Weapon1 initialization,
bounded case-insensitive strings, class/clip lookup, shared animation-definition
parameter writes and the exact rise-state filter outcomes. Only the constructor's
requested state 23 and its filter outcomes (0, 5, 16, 19, refusal) are exposed;
this does not claim a general Skelight AI/state machine port. Reference callbacks
may mutate fields before the filter. Arena class-model and home-range virtual
bodies execute locally by address; unrelated modelled arena bodies explicitly
refuse. Base signed team bytes execute locally at slot 37. Unknown/forced bodies
remain callbacks. GameObject construction, property lookup, statistics, animation,
model queries, object scheduling, charm, allocation, handle linking, sound and
particles all remain caller-owned services, with no invented callee effects.

Service ids 115–123 append property integer/string lookup, off-grid movement,
statistics, animation clear/push, creature initialization, animation start/sync.
Holder slot 2 uses the existing named `Holder.v2` transport (111); slot 4/6
callbacks use 125/126. Id 124 is reserved; no existing service id is renumbered.
64-bit hide masks retain the existing low/high raw32 callback result transport.

Can-use preserves player name lookup's unsigned four-slot bound and monster name
lookup. Cast preserves four flash stores in reference order, 32 plus 200 cosmetic
particles, signed point counts, repeated rejection draws, the 72-unit terrain
spot and signed-byte fallback result. The height threshold uses an IEEE reading
of the rounded float difference: NaN skips fallback and infinity takes it.
Shared and cosmetic streams stay separate. Strength/chance, elemental holder
selection, player charm and distinct object/spell allocation follow the reference
order. The arena spell base stays separate from the older skills-C base.

Creature construction preserves stale fields, serial-byte wrapping, short-circuit
property lookups and repeat reads. The global monster level is overridden only
for the statistics call and restored afterward; a throwing callback preserves
the preceding override, exactly as the reference does. Bounded ASCII strings
preserve Python bytes.lower behavior. No game strings or content tables are
embedded. Constructor animation-event writes reread the shared definition record
for every mask/time pair. Empty spell handles destroy through the explicit
virtual service; live handles leave the spell alive.

The generator appends 321 authored scenarios / 5,136 complete observations across
all 16 FPU configurations. They cover names, every allocation boundary, player
and monster casters, invalid slots, ranks, signed angles/counts, cosmetic rejection,
all 232 particles, terrain threshold/NaN, free-spot byte narrowing, properties,
stale memory, strings, serial wrap, filter outcomes, raw64 masks, lifecycle and
callback failure prefixes. Direct API checks additionally cover separate skeleton
and spell allocations, local home-range dispatch, signed team charm, exception
propagation, temporary-global failure state and depth recovery.

The original 9,032-scenario corpus is retained as a twenty-fourth independent
compatibility hash. Expected observations for existing scenarios are unchanged.
Progress is independently recounted from the immutable AST; the two inherited
virtual dispatchers are now counted, for all 175 declared reference units.

Private acceptance is skipped: `../../private-validation` is unavailable per the
owner's instruction. No private search or Documents/Desktop read occurred.
Console fidelity, actual game content, Norrath, Windows, Intel-host execution,
Godot and Session/scene integration remain unverified. x64 through Rosetta is
separate from Intel-host validation. This contract completion is not a playable
port. Validation logs and new-only comparison helpers are under ignored
`work/skeleton/`; final verification results follow below.


Final validation commands:

```sh
python3 work/skeleton/check_new.py --all
python3 tools/generate_skills_c_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_c.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-skills-c-api-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-c-trace champions-skills-c-api-tests -j 4
build/native-x64/champions-skills-c-api-tests
python3 work/skeleton/check_new.py --all --x64
ctest --test-dir build/native -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native-x64 -R 'skills-c-' --output-on-failure -V
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_c_vectors.py tools/test_skills_c_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Final generation passed: **9,353 scenarios / 149,648 complete-state vectors**.
Plain-vector SHA-256:
`1881bfb823b26fa03e971f20a4698fe0f329c0fbdbac60a6db21c335de0aad46`.
The compressed fixture is 259,348,725 bytes; generator/manifest identity matches.
All 24 historical corpus hashes passed unchanged. The final arm64 scoped suite
passed **2/2 in 46.86 s** (exact comparison 46.78 s); x64 through Rosetta passed
**2/2 in 60.92 s** (exact comparison 60.11 s). Both matched all expected states,
passed transport-tampering checks and returned the same native observation hash:
`3143476e0e3d0e81cfc96e873802ab8d4ac23e8f3344b7e92d43d08f3edfb682`.
Both final direct API binaries and both 5,136-observation new-only comparisons
passed. A separate input-only probe verified that reserved callback id 124
explicitly refuses on both architectures.

Iteration corrected synthetic callback ids, an assertion that assumed sin16(0)
was exactly zero, callback-exception test handling, and a missing table pointer
in an existing API setup after its superseded refusal test was removed. The exact
comparison found the terrain threshold's IEEE NaN behavior, which now matches the
reference across every FPU configuration. One generation was interrupted to add
live-target filter coverage, then restarted from the final scenario set. This was
an intentional regeneration, not a target test failure. The destructor path now
uses the table dispatcher, preserving modelled-body recursion, optional run
arguments and forced numeric Spell.v1 callbacks. Final checks use rebuilt binaries.


The required final full arm64 Release suite
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
295.68 s**. Skills-C exact passed again in 54.54 s. No final scoped or full-suite
test failed or timed out; no isolated timeout retry was needed. Both Release
builds, both API binaries, both new-only comparisons, both scoped suites, full
suite, Python syntax, manifest identity and whitespace checks passed. The
research checkout remains clean.

Exactly nine existing Skills-C files changed:

- `native/include/champions/skills_c.hpp`
- `native/src/skills_c.cpp`
- `native/tests/skills_c_tests.cpp`
- `native/tools/skills_c_trace.cpp`
- `tools/generate_skills_c_vectors.py`
- `tools/test_skills_c_vectors.py`
- `native/tests/vectors/skills_c.json`
- `native/tests/vectors/skills_c.tsv.gz`
- `docs/SKILLS_C.md`

No shared build file, research file or private input changed. The complete
skills-C reference is ported and tested within its explicit callback and host
domains. All 175 declared reference units count as implemented; inherited
construction helpers add no units to that denominator. Engine services and
playable integration remain separate. Local commit only; no push, pull request
or issue.
