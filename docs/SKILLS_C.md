# Return to Arms skills 69–101: shared runtime and active families

PROGRESS: 122 of 175 reference functions ported

Current continuation family: **Disease Blade (97), all six methods including
weapon selection and the complete modifier-write helper**.

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
It now includes the complete Storms and Disease Blade families.
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
ten base bodies listed in `ref_skills_c.py::SKILL_BASE`. The three protection classes, Blood Lust, SharedClasses and SummonItems additionally dispatch their supported can-use/cast/mana bodies. ThrownAxes adds both can-use/cast/mana sets. BattleCry adds its three Skill bodies. Storms adds both cast/mana pairs. Disease Blade adds its can-use/cast/mana bodies.
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
throws `Unsupported`. The three protection families, Blood Lust and SharedClasses have explicit named operations. `CastDefault` is the explicitly
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

`handle_link` and `handle_clear` have tested memory-backed behavior but their
stack-local callback branches still refuse; neither counts as fully ported.
`skill_call`, `table_call`, general `call`, and inherited `spell_rand`/`obj_rand`
are not included in the numerator. Counts are reference units, not operation,
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

The remaining active class can-use/cast/constructor/run/hit/particle/query families are:
70 Unspoken Word; 76 Roar (shared shout helpers now ported);
85 Root; 88 Drowsy;
89 Poison Bolt; 90 Summon Badger; 91 Raise Army; 95 Wave of Flame;
101 Cone of Shock. Id 84 Cyclone's class is owned by `ref_skills_b.py`; id 87
Tagar's Insects is inherited from `ref_arena_monsters.py`; neither class is ported.
Per-class mana body dispatch outside the three protection classes, Blood Lust, Dodge, Healing, SummonItems, CriticalHit, ThrownAxes, BattleCry, Storms and Disease Blade, statistics/static initialization, rank-description
and presentation functions remain, even where the generic shared mana helper is
available. The remaining Base helpers are general `call`, complete `skill_call` and
`table_call` dispatch.
Top-level `shape_result`, `c_div`, `c_rem`, and `desc` remain
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
