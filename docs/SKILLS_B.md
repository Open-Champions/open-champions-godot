# Return to Arms skills 35–68: statistics, handles and iterators

`champions-skills-b` is a standalone C++17, raw32-memory port of the common Skill
statistics/mana contract, passive statistic queries, paired handle lists, mutation-safe
iterators and spell attachment in `ref_skills_b.py`.
It is independent of Godot and depends on the existing core arithmetic and
creature-HP sparse memory libraries. It has no Session, combat or scene integration.
This is a coherent subset, not a complete port of all 34 active skills.

## Evidence and ownership

The immutable research revision is
[`356add27d50e50972b2a671b31e8f4ec93e6778c`](https://github.com/Open-Champions/champions-return-to-arms-re/tree/356add27d50e50972b2a671b31e8f4ec93e6778c).
The primary authored contract is
[`tools/ref_skills_b.py`](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_skills_b.py),
SHA-256 `b71cbdd9e82cd72abb855b9f6ab99d11fb78a58613cb69d314a5041457bbdd95`.
The prose specification and executed correction notes are
[`skills-35-68.md`](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/skills-35-68.md)
and
[`skills-35-68-executed.md`](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/skills-35-68-executed.md).
The latter documents instruction-interpreter differential coverage, including
shared Skill slots and the passive queries. Those research results are evidence
for the contract, not tests executed by this port.

The generator extracts only pinned authored Python files with `git show`, checks
their SHA-256 before importing them from a temporary directory, and checks both
specifications and `difftest_skills_b.py`. It does not run that harness, read an
executable, run retail initializers or copy retail tables. Every table, pointer,
seed and callback return in the public vectors is synthetic. The research checkout
is read-only. Research contributors' MIT license applies to the authored contract;
this implementation is handwritten from its documented behavior.

| Pinned file | SHA-256 |
|---|---|
| `docs/spec/skills-35-68-executed.md` | `698400358c3ca2f221c9e79fa19bf64703006069925f01f2e983a5706fd681f5` |
| `docs/spec/skills-35-68.md` | `a7092cba387542fd7e926145f245ddfcb5339a374442b478e1d810f52049eb6b` |
| `tools/difftest_skills_b.py` | `06778b17758b7b12e30df0a0640706f456f4df58a3e39dda67cf28f3d1c39b47` |
| `tools/ref_arena_monsters.py` | `84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `tools/ref_creature.py` | `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7` |
| `tools/ref_skills_b.py` | `b71cbdd9e82cd72abb855b9f6ab99d11fb78a58613cb69d314a5041457bbdd95` |

## Implemented contract

- Paired handle lists: `handle_link`, `handle_clear`, `handle_removeNode` and
  `list_push`, including node free-list allocation/recycling at 0x864ea8,
  partner unlinking and exact store order. Pool exhaustion retains the reference
  writes through address zero; no allocation failure policy is invented.
- `iter_begin`, `iter_next` and `iter_end` preserve the iterator embedded behind
  the current node, null-object/flag filtering, deletion during a walk and
  deferred visitation of new head insertions. End does not reset iterator fields;
  its flags argument is ignored as in the reference.
- `creature_addSpell` attaches newest first with paired back-links and skips a
  null spell. Read-only `handle_object` and `list_objects` expose the documented
  model helpers, including null objects in list observations.
- Base Skill slots 1 and 2 return 1 and 0; base slot 5 returns raw 1.0.
- Slots 10 and 11 use signed16 first/base fields and raw32 steps; level zero,
  tier fallback, signed caps, arithmetic order and half-away rounding are exact.
- Slots 12 and 13 retain the player-bit distinction and the original handle
  dereference path. In particular, a nonzero node with a zero object pointer
  still reads flags at address 0x0c, as the Python contract does.
- Slot 14 returns its level unchanged. Slot 15 calls slots 14 and 10 in order.
  All 30 per-ID mana overrides retain their exact forwarded slot and second
  argument; 59/62 call slot 14 before returning zero. IDs 60/61/63/64 use 1.0.
- Slot 8 preserves four shared draws, with separate sign-probe and magnitude
  draws for each bound. Its unclamped write and possible clamping write occur
  immediately; aliased output pointers can affect subsequent draws.
- Archery (48), Life Tap (59), Strong Stance (62), Natural Resistance (63) and
  Stunning Strike (64) expose their documented statistic helpers. Boss rank is
  an explicit `creature_bossRank` callback; PvP, target flags and miniboss caps
  preserve the model's short-circuit order. Blunt (60) and Slashing (61) have
  only common base bodies in this contract.
- The Spell base constructor preserves its byte level, ordered stores,
  temporary base vtable, final vtable and one shared draw. The stored spell
  seed is the draw's upper16 result, not the shared generator's full state.
  Shared, spell, object, cosmetic and third-generator LCG steps are exposed.

Raw patterns use the caller's selected core FPU model and both arithmetic options;
there is no host floating-point arithmetic. Virtual dispatch reads the vtable,
signed16 `this` adjustment and body address. Implemented common bodies and mana
cost bodies run internally; unknown bodies require explicit `Skill.vN` handlers.
`external` can force an implemented body to a handler; `addr_names` can name a
callback by function address. Handler changes to memory, exceptions and partial
state are retained, with copied callable lifetime during dispatch.

Missing handlers throw `Unsupported`, rather than the research tool's default
zero-valued stub. Active-cast callbacks use vector arguments in Python; this subset
refuses their dispatch explicitly rather than changing that ABI to a pointer.
`cast_base` returning zero is the documented base body, not an active-skill fallback.
There are no embedded statistic tables, guessed effects or damage calculations.

## Explicit domain and refusals

Statistic indices are limited to 0–4. Level-minus-first must fit signed32 before
conversion; a larger Python integer intermediate refuses instead of wrapping.
Memory uses the existing raw32 address/span checks, missing-byte zero reads,
1 MiB sparse backing cap and 65,536-entry write-history limit. Virtual slots above 15 refuse. Virtual/callback
recursion refuses at depth 64; call history refuses at 65,536 entries. Callback
arguments that would become the reference's private `stack` token refuse.
Read-only list walks and each iterator skip/clear walk are bounded to 65,536
node visits; malformed cycles refuse (write-history limits may refuse earlier
for mutating walks). Earlier writes are retained. These limits are portable
host boundaries, not original PS2 behavior.

The base constructor does not allocate, attach, schedule, end or free a spell.
Attachment is available separately through `creature_add_spell`; spell walking,
execution, destruction and freeing remain outside this family.
World-object construction remains outside this subset. No Norrath behavior,
player cast sequencer, original skill-table initialization, GUI description,
particles, sound, networking, engine resources or playable scene is established.

## Exact synthetic validation

The committed corpus has 4,720 input-driven cases across all 16 FPU configurations.
All original 3,664 rows are unchanged byte-for-byte. The 1,056 appended cases
cover this complete list/iterator/attachment family: masks, null objects, empty
lists, deletion of current/next nodes, clearing back-links during iteration, new
head insertion, early iterator end, node recycling, aliased heads/storage,
exhausted pools and newest-first spell attachment.
Cases include tier fallback, signed fields and levels, caps, player/nonplayer/null
casters, empty and populated handles, signed vtable adjustments, overridden
statistics, callback mutations and failures, all mana overrides, random sign-probe
mismatches, output aliases, constructor state and independent random streams.
Each comparison includes every returned raw value, error category, shared draw
count, every sparse byte (including untouched bytes), ordered writes and ordered
callback calls. Inputs and expectations are separate: native dispatch never reads
expected columns. The manifest pins plain-vector and generator hashes and records
supported/unsupported function names. Transport tests reject wrong expectations,
truncation and damaged fixtures, and verify changed commands change native state.
Host tests cover explicit domain refusals, missing callbacks, recursion recovery,
forced external dispatch and callback naming.

```sh
python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^skills-b-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

Local verification covers Apple Silicon macOS only. Windows, Intel macOS and
cross-architecture trace parity remain unverified. No private game inputs or
console differential execution were used for this port.

## Exact remaining function scope

Every entry below is registered in the pinned Python `FUNCTIONS` inventory and
is not implemented here. Common statistics and mana costs apply to every ID,
with caller-supplied tables. The table does not claim any active cast or effect
as supported. Root (52) is owned by `ref_skills_a.py`; its active behavior is not
in the B reference inventory and also remains unsupported here.

| ID | Skill | Remaining registered functions |
|---|---|---|
| 35 | charge | `SkillCharge_cast`, `SpellCharge_ctor`, `SpellCharge_run`, `SpellCharge_getStat3Scaled`, `SpellCharge_onContact` |
| 36 | darkness | `SkillDarkness_cast`, `DarknessProjectile_ctor`, `DarknessProjectile_run`, `ambientBytes_14a028` |
| 37 | shock | `SkillShock_cast`, `ShockProjectile_ctor`, `ShockProjectile_run`, `ShockProjectile_buildChain`, `ShockProjectile_inChain` |
| 38 | groundpound | `SkillGroundPound_canUse`, `SkillGroundPound_cast`, `GroundPoundEffect_ctor`, `GroundPoundEffect_run` |
| 39 | convertenemy | `SkillConvertEnemy_canUse`, `SkillConvertEnemy_cast`, `SpellConvertEnemy_ctor`, `SpellConvertEnemy_run` |
| 40 | convertundead | `SkillConvertUndead_canUse`, `SkillConvertUndead_cast`, `SpellConvertUndead_ctor`, `SpellConvertUndead_run` |
| 41 | cyclone | `SkillCyclone_canUse`, `SkillCyclone_cast`, `SpellCyclone_ctor`, `SpellCyclone_run`, `SpellCyclone_updateFx` |
| 42 | harmtouch | `SkillHarmTouch_canUse`, `SkillHarmTouch_cast`, `SpellHarmTouch_ctor`, `SpellHarmTouch_run`, `SpellHarmTouch_applyToHit` |
| 43 | firearrow | `SkillFireArrow_canUse`, `SkillFireArrow_cast`, `FireArrowProjectile_ctor`, `FireArrowProjectile_ctor2`, `FireArrowProjectile_run`, `FireArrowProjectile_onHit`, `FireArrowProjectile_end` |
| 44 | coldarrow | `SkillColdArrow_canUse`, `SkillColdArrow_cast`, `ColdArrowProjectile_ctor`, `ColdArrowProjectile_ctor2`, `ColdArrowProjectile_run`, `ColdArrowProjectile_onHit`, `ColdArrowProjectile_end` |
| 45 | poisonarrow | `SkillPoisonArrow_canUse`, `SkillPoisonArrow_cast`, `PoisonArrowProjectile_ctor`, `PoisonArrowProjectile_ctor2`, `PoisonArrowProjectile_run`, `PoisonArrowProjectile_onHit`, `PoisonArrowProjectile_end` |
| 46 | multifire | `SkillMultiFire_canUse`, `SkillMultiFire_cast` |
| 47 | explodearrow | `SkillExplodeArrow_canUse`, `SkillExplodeArrow_cast`, `ExplodeArrowProjectile_ctor`, `ExplodeArrowProjectile_ctor2`, `ExplodeArrowProjectile_run`, `ExplodeArrowProjectile_onHit`, `ExplodeArrowProjectile_explode` |
| 48 | archery | None; common/passive statistic subset only |
| 49 | bash | `SkillBash_canUse`, `SkillBash_cast`, `SpellBash_ctor`, `SpellBash_run`, `SpellBash_windupFx` |
| 50 | minorhealing | `SkillMinorHealing_cast`, `SpellMinorHealing_ctor`, `SpellMinorHealing_run` |
| 51 | ancestralcall | `SkillAncestralCall_canUse`, `SkillAncestralCall_cast`, `SpellAncestralCall_ctor`, `SpellAncestralCall_run`, `SpellAncestralCall_getStat0Fraction`, `SpellAncestralCall_getStat1Fraction`, `SpellAncestralCall_queryFor`, `SpellAncestralCall_tintAndGlow` |
| 52 | root | Active Root bodies in ref_skills_a.py |
| 53 | criticalhit | `SkillCriticalHit_canUse`, `SkillCriticalHit_cast`, `SkillCriticalHit_critBonusFor`, `SpellCriticalHit_ctor`, `SpellCriticalHit_run`, `SpellCriticalHit_setTint` |
| 54 | holyshield | `SkillHolyShield_cast`, `SpellHolyShield_ctor`, `SpellHolyShield_run`, `SpellHolyShield_absorb` |
| 55 | healing | `SkillHealing_cast`, `SpellHealing_ctor`, `SpellHealing_run` |
| 56 | coneoffrost | `SkillConeOfFrost_cast`, `SpellConeOfFrost_ctor`, `SpellConeOfFrost_run`, `DummyDamager_ctor`, `DummyDamager_run`, `DummyDamager_onTouch` |
| 57 | shockoffire | `SkillShockOfFire_cast`, `Sparks_ctor`, `Sparks_run`, `Sparks_onTouch` |
| 58 | froststorm | `SkillFrostStorm_cast`, `FrostStormSpell_ctor`, `FrostStormSpell_run`, `FrostStormIceShard_run` |
| 59 | lifetap | `SkillLifeTap_cast` |
| 60 | blunt | None; common/passive statistic subset only |
| 61 | slashing | None; common/passive statistic subset only |
| 62 | strongstance | `SkillStrongStance_cast`, `SkillStrongStance_trigger` |
| 63 | naturalresistance | None; common/passive statistic subset only |
| 64 | stunningstrike | `SkillStunningStrike_spawnEffect` |
| 65 | giantgrowth | `SkillGiantGrowth_canUse`, `SkillGiantGrowth_cast`, `SpellGiantGrowth_ctor`, `SpellGiantGrowth_run`, `SpellGiantGrowth_stepScale`, `SpellGiantGrowth_emitParticles`, `SpellGiantGrowth_getOwnerField0dc` |
| 66 | planarwind | `SkillPlanarWind_cast`, `SpellPlanarWind_ctor`, `SpellPlanarWind_run` |
| 67 | shadowstep | `SkillShadowStep_cast`, `SpellShadowStep_ctor`, `SpellShadowStep_run` |
| 68 | resurrect | `SkillResurrect_canUse`, `SkillResurrect_findTarget`, `SkillResurrect_cast`, `SkillResurrect_isSlotBeingRaised`, `SpellResurrect_ctor`, `SpellResurrect_run`, `SpellResurrect_onOwnerDeath` |

Remaining shared registered functions: `Creature_updateSpells`, `Projectile_ctor`, `obj_moveAndCollide`.

Additional unregistered model plumbing remains outside the subset: query filling,
model-stack allocation, spell walking/ending and virtual dispatch on
Spell/Creature/effect objects. Paired node allocation/freeing, list observations,
iterator helpers and spell attachment are now implemented explicitly. No other
model helper that feeds the listed active functions is implicitly available.

The next coherent shared family is spell walking/ending with typed Spell virtual
callbacks (slot 2 passes a three-pattern position by value); projectile construction
also needs a typed GameObject constructor callback. These are portable from the
pinned contracts, but are not implemented by this run.

## Prior statistics validation record

Executed on 2026-10-13, Apple Silicon macOS, AppleClang 21.0.0, Release:

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 3,664 expected-state cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_b.tsv.gz` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills_b.tsv.gz` | PASS; identical compressed vectors |
| `cmp native/tests/vectors/skills_b.json work/skills_b.json` | PASS; identical manifests |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; all 3 new tests, including final address-refusal checks |
| `ctest --test-dir build/native --output-on-failure -j 3` | PASS; all 103 tests, 1,071.90 seconds; no timeout reruns required |
| `git diff --cached --check` | PASS |

Only eight new files and additive lines in `native/CMakeLists.txt`,
`native/simulation_targets.cmake` and `docs/ROADMAP.md` belong to this change.
No other module or research file was edited. At that commit, 144 registered
bodies remained unported and 48 were covered; the current inventory above and
fixture manifest include the subsequent handle-list family. Neither milestone
establishes a complete skill implementation.


## Handle-list and iterator family validation record

Continuation on 2026-10-13, Apple Silicon macOS, Release; authored from the same
pinned contract and its shared-machinery executed notes. This run modifies only
`native/include/champions/skills_b.hpp`, `native/src/skills_b.cpp`,
`native/tests/skills_b_tests.cpp`, both existing vector files,
`tools/generate_skills_b_vectors.py` and this page. No shared build files or
research files are changed. There are now 56 supported and 136 remaining
registered functions. No active skill, spell destructor, Session integration or
playable scene is added.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 4,720 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_b.tsv.gz` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills_b.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/skills_b.json` | PASS |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; all 3 tests, including traversal bounds and a mutation-during-walk host check |
| `ctest --test-dir build/native --output-on-failure -j 3` | 100/103 PASS; 2,802.05 seconds; three subprocess-timeout errors under concurrent sibling-worktree load |
| `ctest --test-dir build/native -R '^objects-runtime-transport$' --output-on-failure -j 1` | PASS; isolated rerun, 85.69 seconds |
| `ctest --test-dir build/native -R '^session-hp-transport$' --output-on-failure -j 1` | PASS; isolated rerun, 191.96 seconds |
| `ctest --test-dir build/native -R '^session-turn-transport$' --output-on-failure -j 1` | PASS; isolated rerun, 171.92 seconds |
| `git diff --cached --check` | PASS |

A direct decompressed-prefix comparison also passed: all 3,664 original input
and expectation rows are byte-for-byte unchanged. Native comparisons include
all sparse bytes, ordered writes, callbacks, raw results and draw counts. No
private game files were used. Cross-architecture parity and other platforms
remain unverified.

The full run reported only subprocess timeouts in those three unchanged
transport tests, with no state mismatch. Each passed when rerun alone; all
103 cases therefore have passing results. The full suite was run once.
