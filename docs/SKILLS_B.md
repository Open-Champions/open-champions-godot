# Return to Arms skills 35–68: elemental arrows / Multi Fire continuation

PROGRESS: 183 of 239 reference functions ported

`champions-skills-b` is a standalone C++17, raw32-memory port of the common Skill
statistics/mana contract, passive statistic queries, paired handle lists, mutation-safe
iterators, spell lifecycle, shared projectile movement, healing/protection, and
enemy/undead conversion, Giant Growth, Ancestral Call, melee buffs/strikes,
Resurrection, Cyclone, Cone of Frost / DummyDamager, and elemental arrows / Multi
Fire in `ref_skills_b.py`.
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
zero-valued stub. Unknown Skill active-cast callbacks remain refused; the healing/protection and
conversion, Giant Growth, Ancestral Call, Bash, Critical Hit, Charge, Resurrect,
Cyclone, Cone of Frost, Fire/Cold/Poison/Explode Arrow and Multi Fire casts dispatch
internally. Spell run and
GameObject constructor callbacks now carry a typed three-pattern vector by value.
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

The base constructor does not allocate or attach a spell. Attachment, per-tick
walking and ending are separate operations. Ending delegates slot 1 with argument
3; destruction and freeing remain caller responsibilities. GameObject construction
is an explicit callback; Projectile construction supplies its documented local body.
No Norrath behavior,
player cast sequencer, original skill-table initialization, GUI description,
particles, sound, networking, engine resources or playable scene is established.

## Exact synthetic validation

The preceding shared-runtime corpus had 6,272 input-driven cases across all 16 FPU configurations.
The latest 1,552 rows cover the shared lifecycle/projectile family; all 4,720
rows from the preceding commit remain byte-for-byte unchanged.
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

Local verification covers Apple Silicon macOS and the affected x64 target under
Rosetta. Both architectures matched those 6,272 complete expected states; the
latest family validation is recorded below. Windows and
physical Intel hardware remain unverified. No private game inputs or console
differential execution were used for this port.

## Exact remaining function scope

Every named remaining entry below is registered in the pinned Python `FUNCTIONS`
inventory and is not implemented here. Common statistics and mana costs apply to every ID,
with caller-supplied tables. Minor Healing, Holy Shield, Healing, Convert Enemy, Convert Undead and Giant Growth are fully
implemented standalone contracts with explicit service callbacks. Ancestral Call,
Bash, Critical Hit, Charge, Resurrect, Cyclone and Cone of Frost are also complete.
Fire, Cold, Poison and Explode Arrow, plus Multi Fire, now have complete standalone
B-reference bodies. Root (52) is
owned by `ref_skills_a.py`; its active behavior is not
in the B reference inventory and also remains unsupported here.

| ID | Skill | Remaining registered functions |
|---|---|---|
| 35 | charge | None; complete cast/constructor/run/scaled-statistic/contact contract |
| 36 | darkness | `SkillDarkness_cast`, `DarknessProjectile_ctor`, `DarknessProjectile_run`, `ambientBytes_14a028` |
| 37 | shock | `SkillShock_cast`, `ShockProjectile_ctor`, `ShockProjectile_run`, `ShockProjectile_buildChain`, `ShockProjectile_inChain` |
| 38 | groundpound | `SkillGroundPound_canUse`, `SkillGroundPound_cast`, `GroundPoundEffect_ctor`, `GroundPoundEffect_run` |
| 39 | convertenemy | None; complete can-use/cast/constructor/run contract |
| 40 | convertundead | None; complete can-use/cast/constructor/run contract |
| 41 | cyclone | None; complete eligibility/cast/constructor/run/effects contract (also ID 84) |
| 42 | harmtouch | `SkillHarmTouch_canUse`, `SkillHarmTouch_cast`, `SpellHarmTouch_ctor`, `SpellHarmTouch_run`, `SpellHarmTouch_applyToHit` |
| 43 | firearrow | None; complete eligibility/cast/both constructors/flight/hit/end contract |
| 44 | coldarrow | None; complete eligibility/cast/both constructors/flight/hit/end contract |
| 45 | poisonarrow | None; complete eligibility/cast/both constructors/flight/hit/end contract |
| 46 | multifire | None; complete eligibility/cast/delegated MissileWeapon fan contract |
| 47 | explodearrow | None; complete eligibility/cast/both constructors/flight/hit/end contract |
| 48 | archery | None; common/passive statistic subset only |
| 49 | bash | None; complete can-use/cast/constructor/run/wind-up contract |
| 50 | minorhealing | None; complete cast/constructor/run contract |
| 51 | ancestralcall | None; complete can-use/cast/constructor/run/statistic/query/tint contract |
| 52 | root | Active Root bodies in ref_skills_a.py |
| 53 | criticalhit | None; complete can-use/cast/bonus/constructor/run/tint contract |
| 54 | holyshield | None; complete cast/constructor/run/absorb contract |
| 55 | healing | None; complete cast/constructor/run contract |
| 56 | coneoffrost | None; complete cast/constructor/run and DummyDamager constructor/run/touch contract |
| 57 | shockoffire | `SkillShockOfFire_cast`, `Sparks_ctor`, `Sparks_run`, `Sparks_onTouch` |
| 58 | froststorm | `SkillFrostStorm_cast`, `FrostStormSpell_ctor`, `FrostStormSpell_run`, `FrostStormIceShard_run` |
| 59 | lifetap | `SkillLifeTap_cast` |
| 60 | blunt | None; common/passive statistic subset only |
| 61 | slashing | None; common/passive statistic subset only |
| 62 | strongstance | `SkillStrongStance_cast`, `SkillStrongStance_trigger` |
| 63 | naturalresistance | None; common/passive statistic subset only |
| 64 | stunningstrike | `SkillStunningStrike_spawnEffect` |
| 65 | giantgrowth | None; complete can-use/cast/constructor/run/scale/particle contract |
| 66 | planarwind | `SkillPlanarWind_cast`, `SpellPlanarWind_ctor`, `SpellPlanarWind_run` |
| 67 | shadowstep | `SkillShadowStep_cast`, `SpellShadowStep_ctor`, `SpellShadowStep_run` |
| 68 | resurrect | None; complete eligibility/target/cast/constructor/run/owner-death contract |

Remaining shared registered functions: none. Every named per-skill entry above
still remains; rows marked complete have no remaining B-reference bodies.

Unregistered query-list filling is now implemented. Additional model plumbing
remains outside the subset: complete virtual dispatch on Skill/Spell/Creature/effect objects. Model-stack
allocation and spell walking/ending are available. Delegated Spell/Creature bodies
use explicit callbacks; unported registered bodies still refuse unless forced external.
Paired node allocation/freeing, list observations,
iterator helpers and spell attachment are implemented explicitly, alongside the
healing/protection helpers named in the accounting inventory below. Other model
helpers that feed remaining active functions are not implicitly available.

Next scene-oriented families portable from the pinned evidence include Shock (37),
Darkness (36), Ground Pound (38), and Frost Storm with Ice Shard (58). Shock offers
target-chain traversal and damage; Frost Storm offers another sustained projectile
family. Elemental arrows and Multi Fire are complete as standalone contracts;
plain MissileWeapon and poisonGasSpell internals remain delegated by this reference.

## Reference function accounting

The generator parses the pinned Python AST and records an explicit name inventory
in `reference_progress` in the existing fixture manifest. There are 241 source
function definitions: 234 directly declared class methods and seven top-level
functions. Two inventory/harness helpers are excluded: `shape_result` (default
stub output shaping) and `_register` (inventory registration). This leaves 239
portable units. The runtime `_mana_cost_method` factory counts once and remains
unported as a factory. Generated per-ID mana aliases are separately tracked in
the registered-body inventory; they are not additional source definitions. Inherited methods
from other Python files are not counted.

The manifest names all 183 credited source definitions and the 56 remaining ones.
This continuation adds all 30 registered arrow/Multi Fire bodies and 16 shared
helpers. The registered-body inventory now has 156 supported and 36 remaining
entries. The ID-selected native methods implement each corresponding Python body
with explicit branches for its differing read, draw and write order; shared native
helpers are implemented functions, not merely inlined arithmetic credited twice.
All credited definitions have native implementations exercised by exact vectors or
host checks. Partial dispatch infrastructure, callback refusals, inlined helper
equivalents without their own native implementation, and harness glue receive no
credit. Historical counts below describe their earlier continuation only. These
counts do not imply a complete skills contract or scene integration.

## Shared lifecycle and projectile family

- Spell walking copies the position into the same monotonically allocated model
  scratch region, starts newest first and reads the creature's signed16 facing
  before each call. Null objects are skipped. Self-removal and new head insertion
  retain the original iterator behavior. Exceptions retain the scratch copy,
  iterator state, previous writes and callback history; no cleanup is invented.
- `spell_end` skips null and calls the adjusted Spell slot 1 with 3. Spell slot 2
  passes `(adjusted self, three-pattern position, facing)` through `typed_callbacks`.
  Table adjustment is signed16 and wraps self to raw32. Address naming and forced
  external dispatch apply; unported registered bodies refuse by default.
- `game_object_ctor` increments the shared-draw **count** before its callback;
  the callback owns the actual seed write and object fields. Projectile construction
  then writes its owner head, vtable, radius, splash byte and flags in order,
  followed by paired owner attachment when non-null. Failure prefixes are retained.
- `obj_move_and_collide` uses the core FPU's exact 1/60 step, grid update, strict
  terrain comparison and iterative back-off. A water splash sets its latch, takes
  an object-local draw, formats a caller-supplied sound name and delegates resource,
  sound and water services. Box candidates are supplied in callback order, including
  duplicates. The original flag, owner, vertical and strict planar filters apply;
  both contact callbacks run even if the first mutates memory.
- Vector assignment records an 8-byte store followed by a 4-byte store. Scratch
  copying uses three sequential unlogged pokes. These differences are checked in
  complete-state vectors, including aliased scratch and inputs.

`events` preserves the complete scalar/typed callback order; the existing scalar
`calls` history and callback ABI remain available. Typed arguments are raw32,
three-pattern vectors or strings. Typed returns are raw32, object lists or strings;
terrain/water/virtual returns require raw32, box queries require a list, and
formatted names require a string. Other shapes refuse rather than being coerced.
Callback lifetimes are copied before invocation, preserving self-replacement and
exception behavior. Missing callbacks refuse; no resource, world or destructor
fallback is provided.

New host boundaries are 64 KiB of monotonically consumed aligned scratch, 65,536
spell visits/back-off steps/query results/callback events and depth 64. Earlier
writes survive a refusal. Scratch-address scalar callback arguments remain refused;
vector components are raw patterns and are not mistaken for addresses. No active
cast, destructor, Session integration, real world services or scene is added.

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


## Shared lifecycle/projectile validation record

Continuation on 2026-10-13, Apple Silicon macOS, AppleClang 21.0.0, Release.
Only the existing header, source, native tests, generator, two vector files and
this page changed. Research checkout status and pinned-file SHA-256 are unchanged.
The local corpus contains 6,272 cases, with every preceding 4,720 row unchanged.
Both arm64 and x86_64 binaries passed the same exact-state corpus (x64 via Rosetta);
this establishes parity for these synthetic cases, not target-platform gameplay.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 6,272 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_b.tsv.gz` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills_b.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/skills_b.json` | PASS |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS; final build |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; 3/3, final accounting manifest |
| `arch -x86_64 /usr/bin/true` | PASS; Rosetta available |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure` | PASS; 3/3, 4.71 seconds |
| `ctest --test-dir build/native --output-on-failure -j 3` | 102/103 PASS; 2,054.37 seconds; one subprocess timeout in unchanged `objects-runtime-transport` |
| `ctest --test-dir build/native -R '^objects-runtime-transport$' --output-on-failure -j 1` | PASS; isolated rerun, 94.16 seconds |
| `git diff --cached --check` | PASS |

An initial build found a test-runner namespace typo; it was corrected. The first
new exact-vector comparison exposed the vector write-width mismatch (three
4-byte stores instead of 8+4); it was corrected and all affected tests rerun.
No private inputs or original-console execution were used. Active skill bodies,
complete virtual dispatch, original external services, destructors and scene
integration remain unsupported. See the remaining function table and manifest
name inventory for the continuation scope.

The full suite was run once under concurrent sibling-worktree load. Its sole
failure was a 60-second subprocess timeout in the unchanged object transport
test, with no state mismatch reported. That test passed alone. All 103 tests
therefore have passing results, with the full-run timeout recorded above rather
than represented as a clean full-suite run.

## Healing and protection family

This continuation ports every listed body for Minor Healing (50), Holy Shield
(54) and Healing (55). The constants, field offsets, branch conditions, arithmetic
order and callback arguments come from the same immutable Python contract and
specification sections 50, 54 and 55. The executed notes document these families
as matched by the research harness; that harness was not executed here.

- All casts adjust level before allocation and preserve null-pool results. The
  constructors retain base seed draws, sound/shaft order, signed-byte spell levels,
  record indexing and every store. Shaft and light constructors count one shared
  draw before their callback; that callback owns seed/object writes.
- Minor Healing uses the owner's record even without a player flag, checks
  `HP + 1 <= maximum`, adds without a clamp, and retains strict tint/expiry clocks.
- Holy Shield retains duration and flash clocks, the exact owner matrix, tint
  order, 241 sample-point particles, every spell-local ball rejection draw and
  conditional light allocation. Absorb returns its statistic fraction; charging
  mana and the player's original damage chain remain separate integration work.
- Healing preserves planar monster queries, vertical-independent acceptance,
  ally callbacks, query duplicates and no HP clamp. Player recipients use the
  per-machine alive-slot list, 3D distance, PvP callbacks and record HP guards.
  The selection pass precedes record writes. Callback mutations are visible at
  the documented re-reads. Ring particles use native core matrix/trigonometric
  functions, spell-local draws and strict terrain-height filtering.
- The callback ABI now accepts raw 12-pattern matrices and a sample-point return
  containing a three-pattern position plus the discarded second output. The
  second output may be a vector, as in the research callbacks, or scalar metadata. Incorrect
  result shapes refuse. Existing scalar/vector/string APIs remain compatible.

Additional host bounds: a zero model sample count refuses where the reference
would divide by zero; ball rejection attempts and alive-player visits are capped
at 65,536. Healing reach outside signed32 refuses before conversion. These are
explicit host boundaries, with earlier state retained; no fallback behavior is
credited as ported. Delegated allocation, resource, sound, model, particles,
light construction, network prediction, terrain, ally queries and destruction
still require caller callbacks. Casts do not attach spells automatically.

The latest corpus contains 9,456 cases across all 16 core FPU configurations:
3,184 appended family cases and all preceding 6,272 rows unchanged byte-for-byte.
The source inventory remains 239 portable definitions; 65 now have complete
native implementations and test coverage. The registered-body inventory is
69 supported / 123 remaining. No Session, Godot scene, original damage-chain
integration, Norrath support or private-data validation was added.

### Validation record for healing/protection

Executed on Apple Silicon macOS in Release; x64 ran through Rosetta. Only the
existing seven skills-B files changed (header, source, native tests, generator,
two vector files and this page). Research status stayed clean.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 9,456 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_b.tsv.gz` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills_b.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/skills_b.json` | PASS |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS; final code |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; 3/3, final code, 2.80 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4` | PASS; final code |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure` | PASS; 3/3, final code, 4.82 seconds |
| `ctest --test-dir build/native --output-on-failure -j 3 > work/skills-b-full-ctest.log 2>&1` | 102/103 PASS; 1,472.35 seconds; sole error was a 600-second subprocess timeout in unchanged `session-objects-transport` |
| `ctest --test-dir build/native -R '^session-objects-transport$' --output-on-failure -j 1 > work/skills-b-timeout-rerun.log 2>&1` | PASS; isolated rerun, 427.41 seconds |
| `git diff --cached --check` | PASS |

The decompressed-prefix assertion passed for every preceding 6,272 row. An early
test invocation rejected the stale generator manifest while regeneration was
still running. The first new corpus run found a one-bit tick constant mistake
(`0x3c888889` instead of the documented truncated `0x3c888888`); it was corrected,
then all affected checks passed on both architectures. The final callback audit
also checked the reference's discarded vector second sample output and preserved
existing scalar/vector/string variant indices; all affected checks passed again.
The full-suite invocation occurred before that ABI-only audit; its skills tests
run after the final rebuild. Windows, physical Intel hardware, original console
execution and real scene/engine integration remain unverified.

The full suite ran once at `-j 3`. The final skills-B tests passed in that run
(host, exact and transport), using the rebuilt final code. No state mismatch was
reported by the sole timeout case; its isolated rerun passed. All 103 tests have
passing results, with the concurrent full-run timeout explicitly retained above.
No shared build files, roadmap, other modules or research files changed.


## Convert Enemy / Convert Undead family

This continuation ports the complete skills 39–40 family from the same pinned
Python file and specification sections 39–40: both eligibility checks, both casts,
both constructors and both runs. It also ports their four shared conversion
methods and all three query-list filling helpers. These 15 definitions raise the
portable-source accounting to 80/239; the registered inventory is 77 supported
and 115 remaining. The denominator includes top-level functions and class
methods in the pinned source, excluding `shape_result` and `_register` harness
glue. Generated mana aliases remain one factory definition; refusals, partial
virtual dispatch and inherited methods receive no additional credit.

- Eligibility preserves PvP short-circuiting, signed creature-kind reads,
  the undead box query, slot-20 votes, and HUD notice order. A nonplayer can
  still reach the documented unguarded player-slot read for that notice.
- Casts allocate their scratch head before requesting the spell block, retain
  null allocation, and do not adjust level. Constructors use signed byte levels,
  exact per-ID frame/group fields, sound order and arc arguments. Their filters
  deliberately ask boss rank and charm timer in different orders. Accepted
  targets attach newest first; duplicate query entries and callback mutations
  remain observable.
- Query helpers consume the supplied free list using sequential unlogged pokes,
  preserve query order, and retain node-zero writes on exhaustion. Clearing the
  query uses the existing logged handle implementation. No cleanup is invented
  after a callback throws.
- Runs preserve wrapped spiral angles, raw32 arithmetic, strict four-second
  timing, group indexing around null targets, complete target-list snapshots,
  shared rolls, half-away chance rounding for wide targets, immunity/resistance
  notices, signed player teams, 64-bit creature-flag writes and wrapped
  seconds-to-frames conversion. FlyingText construction counts its shared draw
  before delegation. Destruction still delegates Spell slot 1 with argument 3.

All service behavior delegated by the reference remains an explicit callback:
allocation, arc/box queries, boss rank, enemy votes, resources, sound, particles,
text/HUD, FlyingText construction, Creature_setCharm and destruction. In
particular, the local flag/timer stores do not implement the delegated charm
service. Casting does not attach the spell to a caster or charge mana.

The existing query-result/visit bound is 65,536. A query callback must return an
object list or raw zero; other shapes refuse. Only conversion IDs 39 and 40 are
accepted by the shared methods. Earlier writes and callback history survive
refusals. These are explicit portable host boundaries, not guessed PS2 behavior.

The corpus now has 13,536 cases across all 16 core FPU configurations, including
4,080 appended conversion cases. Every preceding 9,456 row is byte-for-byte
unchanged. Cases cover complete lifetimes, timing and radius boundaries, signed
statistics and teams, target filters/order/duplicates/nulls, exhausted node pools,
allocation failures, callback failure prefixes and mutations, query helpers and
signed virtual self adjustments. Native host checks cover invalid profiles,
query result shapes, oversized results and null queries.

### Validation record for conversion

Apple Silicon macOS, Release; x64 runs through Rosetta. Only the existing header,
source, tests, generator, two vector files and this page changed. No private game
files were used; the research repository stayed clean and its pinned contract
SHA-256 was rechecked. Cross-architecture parity
covers the exact synthetic corpus only. Windows, physical Intel hardware,
original console execution, engine services, Norrath and playable scene/Session
integration remain unverified or unsupported.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 13,536 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_b.tsv.gz` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills_b.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/skills_b.json` | PASS |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS; final code |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; 3/3, 5.41 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4` | PASS; final code |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure` | PASS; 3/3, 9.12 seconds |
| `ctest --test-dir build/native --output-on-failure -j 3 > work/skills-b-full-ctest.log 2>&1` | PASS; 103/103, 693.14 seconds; no timeouts |
| `git diff --cached --check` | PASS |

The remaining table above and manifest list the next continuation scope. All
remaining registered families still need their active behavior ported; Root's
active behavior belongs to the separate A contract.


## Giant Growth family

This continuation implements all seven remaining definitions for skill 65 from
section 65 of the same pinned Python contract and prose specification. Portable
accounting is 87/239 definitions; the registered inventory is 84 supported and
108 remaining. Accounting uses the generator's AST inventory, with the same
harness exclusions documented above. No refusal receives implementation credit.

Eligibility delegates type information and spell lookup in that order. Casting
adjusts level before the player check and preserves null allocations. Construction
retains the light record's exact store order, paired owner handle, signed byte
level, integer frame multiplication, fifty particle slots and one shared plus one
hundred private draws. Negative sample counts retain signed remainder behavior.
The scale step preserves exact raw32 equality, clamp stores, wrapped countdown
writes (including the wide pre-write threshold comparison), light radius and
inverse-square calculations, and the signed-slot player movement-factor writes.
Runs preserve owner-handle loss, scale/particle/light order and delegated ending.

The typed callback result ABI adds `GrowthSample` after every existing variant
alternative: position, a raw-pattern number sequence, and a nine-pattern matrix.
The emitter consumes only the first number, rotates the returned matrix about
axis 2, preserves the 11/4/47 slot choices, wrapped angles/ages and private-draw
refresh order. Light, particles, model sampling, allocation, resources, sound,
spell lookup and destruction stay caller services. Zero sample counts and missing
or malformed sample results explicitly refuse with earlier writes retained.
No destructor restoration, automatic spell attachment, mana charging, original
engine service, Session/scene integration or Norrath behavior is added.

The appended synthetic cases exercise all seven functions, virtual dispatch with
signed self adjustment, player/null allocation checks, negative/large sample counts,
countdown boundaries including INT32_MIN, scale overshoot/clamps, particle aging,
callback failures/mutations, lost/null owners, exhausted handle nodes and an entire
small authored growth/hold/shrink lifetime. Host tests verify zero-count and
malformed-result refusals. Every preceding 13,536 row remains byte-for-byte unchanged.

### Validation record for Giant Growth

Apple Silicon macOS, Release; x64 through Rosetta. Only the existing seven skills-B
files changed. No private inputs were used; the research checkout stayed clean
and its Python SHA-256 still matches the pin. Windows, physical Intel hardware,
original console execution and real engine/scene integration remain unverified.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 15,984 complete-state cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_b.tsv.gz > work/skills-b-regeneration.log` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills_b.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/skills_b.json` | PASS |
| Decompressed-prefix assertion against `work/skills_b-before.tsv.gz` | PASS; all 13,536 preceding rows unchanged |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4 > work/skills-b-build.log` | PASS; final code |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; 3/3, 8.03 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/skills-b-x64-configure.log` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/skills-b-x64-build.log` | PASS; final code |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure` | PASS; 3/3, 13.01 seconds |


## Ancestral Call family

This run completes all eight section-51 definitions from the pinned authored
`ref_skills_b.py` at `356add27` and the prose specification above. It preserves
and validates the pre-existing, uncommitted Giant Growth continuation. The current
AST inventory is **95/239 portable source definitions** and the registered-body
inventory is **92 supported / 100 remaining**. The manifest names every credited
unit; explicit refusals and partial virtual dispatch receive no credit.

- Eligibility, level adjustment and allocation preserve callback order and null
  results. Constructor animation precedes the prediction-copy check; signed weapon
  classes select caller-supplied animation resources. A real caster receives the
  light callback, ordered action/beam/clock stores, radius and particle group.
- The complete run preserves animation completion, sound ownership, three beams
  with caller-supplied frame offsets, tint/glow stores, private random ring draws,
  terrain filtering and dynamic alive-player traversal. Owner and caster-copy fields
  remain distinct. Callback mutations affect the documented re-reads.
- Both fractions and the aura query retain raw32 arithmetic and signed levels.
  Query outputs are written before target checks, including aliased outputs; range
  is inclusive in the query and strict in the run. Wrapped integer radius products
  and signed player slots are preserved.
- `Argument` appends `std::monostate` to represent the reference's `None` format
  argument without changing existing variant indices. Attachment-point callbacks
  return exactly three raw patterns through the existing list result. Malformed
  attachment points or format-name results refuse with earlier state retained.
  Alive-list traversal retains the existing 65,536-visit host bound.

Allocation, animation, resources/formatting, sound, lights, beam services, model
attachments/glow, prediction lookup, terrain, particles, ally votes and destruction
remain explicit caller callbacks. No skill tables or beam timings are embedded.
Automatic spell attachment, mana charging, engine services, Session/scene integration,
Norrath behavior, Windows, physical Intel hardware and original-console execution
remain unsupported or unverified. No private game inputs were used or copied.

The corpus contains **19,840 complete-state cases across all 16 FPU settings**:
3,856 new Ancestral Call cases and all 15,984 prior rows byte-for-byte unchanged.
It covers all eight bodies, signed virtual adjustments, signed/overflowing radii,
output aliases, allocation failure, prediction copies, all documented weapon styles,
beam timing and allocation failure, exact range/terrain/expiry boundaries,
callback exceptions/mutations, dynamic/duplicate player entries and a short complete
lifetime. Host checks cover malformed callback returns and preservation of `None`.

### Validation record for Ancestral Call

Apple Silicon macOS, Release; x64 executes through Rosetta. Only the seven existing
skills-B files changed (header, source, tests, generator, two fixtures and this page).
The research Python SHA-256 still matches the pin, and research tracked diff is empty.
The previous Giant Growth full-suite attempt encountered disk exhaustion; it is
not counted as passing. The current full-suite result is recorded below.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 19,840 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills_b-ancestral.tsv.gz > work/ancestral-regeneration.log 2>&1` | PASS |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills_b-ancestral.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/skills_b-ancestral.json` | PASS |
| Decompressed prefix assertion against `work/skills_b-ancestral-before.tsv.gz` | PASS; all 15,984 prior rows unchanged |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/ancestral-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/ancestral-build.log 2>&1` | PASS |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/ancestral-tests.log 2>&1` | PASS; 3/3, final 19,840-case corpus |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/ancestral-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/ancestral-x64-build.log 2>&1` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/ancestral-x64-tests.log 2>&1` | PASS; 3/3, final 19,840-case corpus |
| `ctest --test-dir build/native --output-on-failure -j 3 > work/ancestral-full-ctest.log 2>&1` | PASS; 103/103, 426.48 seconds; no timeouts |
| `git diff-tree --check HEAD <staged-tree>` | PASS; equivalent staged-tree check |


Git's normal status, staging and cached-diff paths stalled reading the shared
`.git/info/exclude` iCloud placeholder. Only this run's stuck Git processes were
terminated. The shared file was left untouched. Owned files were staged with
`git hash-object -w -- <file>` and `git update-index --index-info`; `git write-tree`
and `git diff-tree --check HEAD <staged-tree>` verified that exact tree. The
initial index tree matched HEAD, and the staged change contains only the seven
owned files. Final local commit creation uses `git commit-tree <tree> -p <HEAD>`
and an atomic `git update-ref refs/heads/codex/port-skills-b <commit> <HEAD>`.
Nothing is pushed and no issue or pull request is created.


The full suite used the final native code and the initial 19,648-row Ancestral
Call corpus. The final 192 additional vector rows were added while it ran; the
completed 19,840-row corpus then passed the targeted host/exact/transport checks
on both architectures (arm64 6.03 seconds, x64 9.55 seconds). No native code changed
after the full-suite build. The independent regeneration and both prefix checks
passed for the final fixtures. The first build caught a missing namespace constant;
it was corrected to the pinned contract's existing world-global address before
all successful checks. No shared build, roadmap, other module or research file changed.

## Melee buffs and strikes family

This continuation completes Bash (49), Critical Hit (53, with the contract's 33/81
profiles) and Charge (35), using sections 49, 53 and 35 of the same pinned Python
and prose specification. All 16 registered bodies and their five source helpers
are implemented. The AST manifest names 116/239 credited source definitions and
108/192 supported registered entries. Explicit refusals receive no credit.

- Bash preserves the constructor's distinct store order, shared clip event writes,
  first-object target selection quirk, unconditional owner action stores, signed
  player-slot pad address, vector-taking turn callback and animation/latch ending.
  The strike preserves list order, statistic re-reads, level-dependent knock flags,
  HP/stun short-circuit draw order, first-victim light allocation, tint writes,
  fallback query on the same list and final hit-done store. Wind-up uses exactly
  203 sample/ball particle sequences from the private spell stream.
- Critical Hit preserves signed ranks, three skill-ID particle-count profiles,
  countdown/fade boundaries, the private ring stream, terrain rejection and live
  player-list traversal. The bonus selects the first qualifying aura, with the
  documented 3-D filter, strict planar filter, PvP vote, rank-query order and
  callback-visible record re-reads. Wide radius products retain the reference's
  signed32 conversion boundaries; a negative best rank can terminate the search
  without applying a bonus.
- Charge preserves ten trail records, null trail handles, cast/prediction action
  states, unsigned elapsed-frame conversion, null-ghost stores during ending,
  contact self-damage, signed weapon-class lookup and ordered trail/sample/ball
  updates. Contact preserves dead/already-contacted early returns, eight wall
  puffs, direct/box victim filters and private damage draws, 132 particle pairs,
  callback-visible owner re-reads and the final contact store. Its shared
  `from_uint` helper and scaled steering statistic use raw32 arithmetic.

All movement, damage, model sampling/attachments, resources, text/HUD, sound,
lights, trails, terrain, particles, player rank queries, prediction lookup and
spell destruction remain explicit services. Nothing attaches a spell or charges
mana automatically. Malformed callback shapes, zero sample counts, missing
handlers and the existing host memory/history/recursion/walk limits refuse with
prior writes and random draws retained. No original engine behavior is invented.
Bash's Creature slot 26 forwards the adjusted self and a vector by value, including
address naming and forced external dispatch; callback lifetime survives removal.

The final corpus has **28,432 complete-state cases across all 16 FPU settings**,
including **8,592 appended cases**. Every one of the 19,840 preceding rows remains
byte-for-byte unchanged. New coverage includes all 21 definitions, null allocations,
signed/wide statistic values, first-target filtering, stun/knock thresholds,
critical countdown/light/range boundaries, dynamic/duplicate ally entries,
charge unsigned wraparound, wall/direct/box contacts, callback failures and
mutations, virtual self adjustments and short complete lifetimes. Host tests cover
malformed attachment/box results, zero-sample failure prefixes and the typed turn ABI.

### Validation record for melee buffs and strikes

Release on Apple Silicon macOS and x64 through Rosetta. Only the seven existing
skills-B files changed: header, source, native tests, generator, TSV fixture,
JSON manifest and this page. Independent regeneration matches both fixtures.
The research tracked files remain unchanged and the contract SHA-256 matches
its pin. No private game inputs were used.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz > work/melee-generation.log 2>&1` | PASS; 28,432 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/skills-b-melee-regenerated.tsv.gz > work/melee-regeneration.log 2>&1` | PASS |
| `cmp native/tests/vectors/skills_b.tsv.gz work/skills-b-melee-regenerated.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/skills-b-melee-regenerated.json` | PASS |
| Decompressed-prefix assertion against `work/skills-b-melee-before.tsv.gz` | PASS; all 19,840 preceding rows unchanged |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/melee-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/melee-build.log 2>&1` | PASS |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/melee-tests.log 2>&1` | PASS; 3/3, 6.11 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/melee-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/melee-x64-build.log 2>&1` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/melee-x64-tests.log 2>&1` | PASS; 3/3, 10.11 seconds |
| `ctest --test-dir build/native --output-on-failure -j 3 > work/melee-full-ctest.log 2>&1` | PASS; 103/103, 356.82 seconds (retained prior-run log) |

Windows, physical Intel hardware, original-console execution, real engine services,
Norrath and Session/scene integration remain unverified or unsupported. Resurrect
and the elemental projectile families remain portable continuation candidates;
the exact remaining inventory is in the table above and fixture manifest.

Normal Git status/diff refresh again stalled on the previously documented shared
exclude placeholder. Only this run's stalled Git processes were stopped; the
shared exclude file and other agents' processes were left alone. Direct tracked
blob comparison established a clean starting worktree, and the initial index tree
matched HEAD. The same `hash-object` / `update-index --index-info` staging route
preserves the index outside these seven owned files. `diff-tree --check HEAD <tree>`
checks the complete staged tree. Local commit creation uses `commit-tree` followed
by an atomic branch `update-ref`; no remote action is performed.


## Resurrection family (68)

This continuation ports all seven section-68 bodies from the pinned Python contract,
with the prose and executed correction notes cited above. The existing melee-family
continuation was preserved and committed separately after its 28,432-case corpus
passed in a fresh Release build. No earlier vector row changed.

- Eligibility retains the PvP player warning, no-target warning and type/lookup order.
  The target search preserves signed dead countdowns, signed facing, strict forward
  bounds, the one-sided sideways test, nearest-forward selection and lower-slot ties.
  Alive-player traversal re-reads its count and entries after service callbacks.
- Casting adjusts the level before selecting or allocating. Multiplayer re-reads the
  target record after allocation; single-player uses the caster. Construction preserves
  the exact light stores, one shared draw, raw target/caster pointers, signed level byte,
  double-precision animation-rate calculation, sound ordering and movement-factor write.
  The animation blend is the contract's raw `0x3dcccccc`.
- Running preserves the wrapped pulse product, signed remainder, mode re-reads, tint
  ordering and typed particle vectors. The revival threshold is strictly below 0.25.
  Respawn, idle reset and life callbacks precede re-reading the new target object;
  the new record receives maximum HP times statistic 1 times 0.01, in that order.
  The animation callback precedes the per-machine rebuild byte; ending stays delegated.
- Owner death preserves the signed chance comparison, shared draw before statistics,
  null allocations, delegated phantom/light constructor draw accounting, half-maximum
  HP result, and failure flag/frame/light-unlink sequence. No phantom behavior or
  respawn internals are substituted for callbacks.

The corpus contains 37,776 complete-state cases across all 16 FPU configurations:
9,344 appended resurrection cases and all 28,432 preceding rows unchanged byte for
byte. It covers every new body, strict geometric/countdown/expiry boundaries,
negative counts and slots, signed levels and frame wrap, zero/negative/large rate
inputs, once-only revival, dynamic lists and object replacement, null allocations,
callback mutations/failures, signed virtual self adjustment and short complete
lifetimes with player-count changes. Host checks cover unavailable services,
retained failure prefixes, the 65,536-visit target-search bound and the typed
particle ABI. Explicit refusals and partial virtual dispatch receive no credit.

### Validation record for Resurrection

Release on Apple Silicon macOS; the affected x64 target executes through Rosetta.
Only the seven existing skills-B files changed: header, source, native tests,
generator, TSV fixture, JSON manifest and this page. The research checkout is
read-only and unchanged. Private acceptance measurements were skipped because
`../../private-validation` is unavailable; no alternative private inputs were sought.

| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 37,776 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/resurrect-regenerated.tsv.gz` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/resurrect-regenerated.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/resurrect-regenerated.json` | PASS |
| Decompressed prefix assertion against `work/resurrect-before.tsv.gz` | PASS; all 28,432 prior rows unchanged |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; 3/3, 6.70 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure` | PASS; 3/3, 12.67 seconds |
| `ctest --test-dir build/native --output-on-failure -j 3` | PASS; 103/103, 502.90 seconds; no timeouts |
| `git diff --check` | PASS |

Windows, physical Intel hardware, original-console execution, Norrath, real engine
services and Session/scene integration remain unverified or unsupported. Spell
attachment, mana charging, destructor restoration, phantom internals, particles,
lights, resource lookup, animation and respawn remain caller responsibilities.
Other active skill families and the remaining model plumbing are listed above and
by exact source-definition name in the existing JSON manifest. Nothing was pushed;
no issue or pull request was created.


## Cyclone family (41/84)

This continuation implements all five registered Cyclone bodies, using section 41
of the pinned Python contract and the linked specification/executed correction
notes. This sustained melee/movement family is useful to future scene integration;
its native body remains independent of any scene. No additional source helper
is credited. Every preceding vector row is retained byte for byte.

- Eligibility preserves the signed weapon-class gate and warning callback order.
  Casting preserves null allocation. Construction retains the unconditional owner
  action state, shared clip-header stores before the prediction-copy test, one
  shared draw and two private draws, exact light stores, and paired candidates in
  query order (including duplicate entries).
- Running retains merged animation-event masks, shared event-time wrapping,
  signed-frame hit periods, strict animation/status checks, stop-clip event slot 2,
  ghost animation dispatch, signed top-track offsets and player slots, camera
  steering, callback-visible position re-reads, and ordered anchor updates.
  Hits walk the live candidate links, invoke slot 20 and use the strict planar
  weapon-tip distance. Local-slot rebasing follows the hit/effects block.
- Effects retain the light pulse, signed phase updates, complete 72-particle
  Hermite helix, raw arithmetic order and three private spark draws at signed
  levels at least 9 on frames divisible by 3. Presentation calls remain recorded
  typed callbacks; no particle, movement or melee-hit internals are invented.

Animation, resources, voice/sound, world query, movement, camera yaw, enemy vote,
weapon hits, particles, lights, prediction lookup, local-slot checks and spell
ending remain explicit services. A malformed attachment result or missing service
refuses with prior writes/draws retained. The live target walk uses the existing
65,536-visit host bound; this is not an original engine restriction. Slot 2
Spell dispatch and Skill eligibility/cast dispatch run the new bodies internally,
including signed virtual self adjustment and explicit forced-external dispatch.

### Validation record for Cyclone

The final corpus has **47,472 complete-state cases across all 16 FPU settings**,
with **9,696 appended Cyclone cases**. All 37,776 preceding rows are byte-for-byte
unchanged. The complete sparse memory, ordered write/callback history, returned
values, exception categories and shared draw counts match on Apple Silicon macOS
and x64 through Rosetta. The corpus covers every new body, signed levels/frames,
clip/time/stop boundaries, prediction copies, both periods, duplicate/null targets,
live-link changes, steering and signed slots/tracks, service mutations/failures,
virtual self adjustments and short full lifetimes. Host checks cover typed movement
and particle ABI, callback self-removal, malformed attachment results, missing
services and cyclic candidate walks.

Only the seven existing skills-B files changed: header, source, native tests,
generator, compressed TSV fixture, JSON manifest and this page. Independent
regeneration matches both fixture files. An independent AST count confirms the
239-unit denominator and 128 credited definitions. The research tracked files
are unchanged and the primary contract SHA-256 matches its pin.

| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz` | PASS; 47,472 cases |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/cyclone-regenerated.tsv.gz` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/cyclone-regenerated.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/cyclone-regenerated.json` | PASS |
| Decompressed prefix assertion against `work/cyclone-before.tsv.gz` | PASS; all 37,776 prior rows unchanged |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure` | PASS; 3/3, 6.88 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure` | PASS; 3/3, 13.25 seconds |
| `ctest --test-dir build/native --output-on-failure -j 3` | PASS; 103/103, 289.29 seconds; no timeouts |
| `git diff --check` | PASS |

The first compile detected a missing signed-byte conversion name. It was corrected
to an explicit fixed-width conversion before the successful builds and tests.
The synthetic statistic offsets were corrected before final fixture generation.
No prior input or expectation changed.

Private acceptance measurements were skipped because `../../private-validation`
is unavailable, as instructed. No private files or alternative private locations
were searched. Windows, physical Intel hardware, original-console execution,
Norrath, engine-service internals and Session/scene integration remain unverified
or unsupported. The exact remaining source/registered inventories are in this
page and the existing manifest. Nothing is pushed and no issue/PR is created.


## Cone of Frost and DummyDamager family (56)

This continuation ports all six section-56 bodies, using the pinned Python contract,
section 56 of the specification and the executed corrections (fourth particle draw,
fan constants, terrain fire and cold priority). It also implements the existing
`local_vec`, `obj_ball` and `descriptor` source helpers. None of the partial virtual
routing machinery or refused engine behavior receives progress credit.

- Casting preserves slot-14 adjustment and null allocation. Construction preserves
  the one shared draw, light write order, owner position snapshot and callback-visible
  owner re-read before the positional looping sound. It never attaches the spell.
- Running preserves all three particle fans, exact private draw order (including the
  discarded fourth draw), signed levels/facing, per-statistic re-reads, eighth-frame
  projectile creation and light pulse. The unscaled third fan and separately doubled
  projectile velocity retain their original arithmetic order.
- DummyDamager construction snapshots velocity before the delegated base constructor,
  uses paired owner handles, signed radius conversion, 16-bit life/extra stores and
  the original flags order. Running decrements life before movement, dispatches live
  collision touches, retains terrain fire/cold priority, 128 cold particle pairs and
  callback-visible bit/position/velocity re-reads before conditional destruction.
- Touch queries the victim's enemy slot, gives cold priority over fire, packs all four
  descriptor lanes as unsigned16, zeroes the damage before delivery and preserves
  subsequent zero-valued touches. Non-creatures also receive the damage callback.
  Empty owner handles retain the Python sparse-memory read at address 0x0c for creature
  votes; this establishes no console behavior for the invalid original pointer.

The new descriptor argument is an owned typed value; existing scalar, vector, matrix
and string argument formats are unchanged. GameObject/StaticFire construction,
resource lookup, sound, particles, lights, terrain/water, queries, enemy votes, damage,
run registration and destruction remain explicit callbacks. Missing services retain
all reached writes/draws and refuse. The existing host memory/history/recursion/stack
limits apply; no engine behavior, skill tables, mana charging or player channel state
machine is invented. Skill cast, Spell run and DummyDamager run/touch virtual bodies
route internally with signed self adjustment, naming and existing external boundaries.

The final corpus contains **53,328 complete-state cases in all 16 FPU configurations**,
including **5,856 appended cases**. All 47,472 preceding rows remain byte-for-byte
unchanged. Coverage includes every new body and helper, all 32 behavior-bit
combinations, terrain/object collision and internal touch dispatch, creature votes,
empty owner handles, cold/fire/no descriptors, signed damage/life/radius boundaries,
null allocations, callback mutations/failures, signed virtual adjustments, a complete
45-frame projectile life and nine channel frames. Host tests independently verify
four-lane descriptor masking, owned typed ABI, callback self-removal, retained missing
service prefixes and scratch allocation/pokes.

Only the existing header, source, native tests, generator, compressed TSV fixture,
JSON manifest and this page change. The AST inventory independently confirms 239
portable definitions and 137 credited definitions. Research tracked files are unchanged
and the primary contract hash matches its pin. Private acceptance measurements were
skipped because `../../private-validation` is unavailable; no other private locations
were searched. Windows, physical Intel hardware, console execution, Norrath, engine
service internals, original channel sequencing and Session/scene integration remain
unverified or unsupported. At that continuation, the elemental arrows/Multi Fire family remained a large
portable continuation; it is now implemented below. Remaining entries are listed
above and in the manifest.


### Validation record for Cone of Frost / DummyDamager

Release on Apple Silicon macOS and x64 through Rosetta. The exact commands below
include their local log destinations. Prefix comparison used Python `gzip.decompress`
and `new.startswith(old)` against `work/cone-before.tsv.gz`; it passed for all
47,472 prior rows. A separate Python AST traversal checked the 239-unit denominator
and the manifest's complete credited/remaining partition. Both are PASS.

| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/cone-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/cone-build.log 2>&1` | PASS |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz > work/cone-generation.log 2>&1` | PASS; 53,328 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/cone-tests.log 2>&1` | PASS; 3/3, 8.00 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/cone-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/cone-x64-build.log 2>&1` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/cone-x64-tests.log 2>&1` | PASS; 3/3, 13.27 seconds |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/cone-regenerated.tsv.gz > work/cone-regeneration.log 2>&1` | PASS |
| `cmp native/tests/vectors/skills_b.tsv.gz work/cone-regenerated.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/cone-regenerated.json` | PASS |
| `git -C ../../champions-return-to-arms-re diff --exit-code` | PASS; research tracked files unchanged |
| `shasum -a 256 ../../champions-return-to-arms-re/tools/ref_skills_b.py` | PASS; matches the pinned digest |
| `ctest --test-dir build/native --output-on-failure -j 3 > work/cone-full-ctest.log 2>&1` | PASS; 103/103, 262.56 seconds; no timeouts |
| `git diff --check` | PASS |

The initial targeted test command ran before fixture generation finished and failed
its generator-hash check (host test passed). It was rerun after generation; both
final architecture runs above passed the final corpus. No native code changed after
the full-suite build. Nothing is pushed and no issue or pull request is created.


## Elemental arrows and Multi Fire family (43–47)

All 30 registered bodies and 16 supporting source helpers are ported from the
pinned Python contract, specification sections 43–47 and the shared bow path,
including the executed corrections SB7 (two cosmetic draws in the nonplayer
Multi Fire damage calculation), SB8 (an explosion draw per accepted creature
or prop), and the 601-movement lifetime. ID-selected native entry points reuse
one module and retain each arrow's distinct constructor/store and callback order.

- Bow eligibility keeps the signed weapon-class check and warning order. Fire,
  Cold and Poison adjust the level only after successful allocation; Explode
  adjusts before copying the cast position and allocating. Both constructors
  retain paired owners, facing versus normalized three-axis direction, raw light
  stores, own/shared seeds, radius, speed, run registration and launch sound.
- Flight moves before testing expiry. Collision uses the internal adjusted hit
  virtual bodies and keeps damaging multiple overlaps after the once-only end.
  Player pass-through, enemy votes, current weapon record, tier/cap selection,
  four-lane descriptors, level-dependent knock flags and chill/poison timers
  retain their documented callback-visible re-reads.
- Cold ending retains the live level bounds, planar reflection and discarded
  second ball sample. Poison emits its ring even on expiry, then delegates a
  gas cloud if allocation succeeds. Explode retains ice/stone selection, typed
  formatted resource name, light/draw accounting, camera shake, ordered box
  traversal, owner proximity, creature votes and prop damage.
- Multi Fire preserves the original versus adjusted level uses, signed active
  bow slot, half-away damage rounding, two cosmetic draws for nonplayers,
  shared per-arrow height draw even on allocation failure, and all seventeen
  delegated MissileWeapon arguments. Integer-to-float conversions mask to
  signed32 as the Python core does, including intermediate overflow.

GameObject, MissileWeapon, poisonGasSpell and LightEffect construction, damage,
particles, light services, resources, sound, warnings, timers, camera and world
queries remain explicit services. Missing/ill-typed services refuse with reached
writes/draws retained. This does not implement the player release sequencer,
ammunition/mana consumption, engine destructors or any Session/scene integration.
The shared flight helper also accepts explicit caller-owned end/particle
callbacks; their internals are never guessed. Existing host memory, history,
stack and traversal bounds still apply. Large cold bursts and Multi Fire fans
can refuse at those bounds, with their reached prefixes retained.

The corpus contains **70,128 complete-state cases across all 16 FPU settings**,
including **16,800 appended arrow/Multi Fire cases**. All 53,328 preceding rows
remain byte for byte unchanged. Both architecture runs produce trace SHA-256
`05f353ebb2038876da0dc4608effcbd65b98c028e2d2bece2b2cf0b907b423ab`.
It covers all new bodies and helpers,
player/nonplayer and missing owners, null allocations, signed levels/facing/slots,
zero/subnormal/extreme directions, age wrapping and expiry, collision versus
terrain, friendly-object exclusions, enemy votes, repeated contacts, descriptor
masking, all ending reasons, live callback mutations/failures, ordered explosion
queries with duplicates and props, both cosmetic seeds, mixed fan allocation,
virtual self adjustment and explicit external/named object dispatch. It includes
a full 601-movement Fire Arrow life and short complete lifetimes of the other
arrows. Host checks separately exercise missing services, retained prefixes,
callback self-removal, signed32 conversion wrapping, the arbitrary flight
callback ABI and all seventeen MissileWeapon fields with owned vectors.

Only the seven existing skills-B files changed: header, source, native tests,
vector generator, compressed TSV, JSON manifest and this page. No shared build
files, research files or private inputs are changed. Remaining functions are
listed above and individually in the manifest.

### Validation record for elemental arrows / Multi Fire

Release on Apple Silicon macOS and x64 through Rosetta. Final results and exact
commands are recorded below. All 53,328 preceding rows are
retained byte for byte. The reference SHA-256 and AST denominator are checked
independently. Private acceptance measurements are skipped because the instructed
`../../private-validation` inputs are unavailable; no alternate locations are
searched. Windows, physical Intel hardware, original-console execution, Norrath,
engine-service internals and scene integration remain unverified or unsupported.

The first targeted pass found incomplete temporary Skill tables in new virtual
cast fixtures and an ambiguous overload in a new host assertion. Both were
corrected. The comparison also verified that the reference core masks integer
conversion to signed32; new intermediate conversions now preserve that wrapping.
No preceding input or expectation was altered. A temporary diagnostic macro
required a variadic form for existing template expressions before compilation
passed; it now reports failing host assertion line numbers.


| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/arrows-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/arrows-build.log 2>&1` | PASS |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz > work/arrows-generation.log 2>&1` | PASS; 70,128 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/arrows-tests.log 2>&1` | PASS; 3/3, 18.65 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/arrows-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/arrows-x64-build.log 2>&1` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/arrows-x64-tests.log 2>&1` | PASS; 3/3, 31.21 seconds |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output work/arrows-regenerated.tsv.gz > work/arrows-regeneration.log 2>&1` | PASS; independent regeneration |
| `cmp native/tests/vectors/skills_b.tsv.gz work/arrows-regenerated.tsv.gz` | PASS |
| `cmp native/tests/vectors/skills_b.json work/arrows-regenerated.json` | PASS |
| `python3 work/audit-arrows.py` | PASS; pinned SHA-256, independent AST count/partition, preceding-row prefix and unchanged research tracked files |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `git diff --check` | PASS |

The ignored audit script uses a separate AST traversal of `git show` at the pin,
checks the 239-unit denominator and the manifest's 183/56 partition, and checks
`gzip.decompress(new).startswith(gzip.decompress(old))` against
`work/arrows-before.tsv.gz`. It also compares the live contract to the pin and
runs `git -C ../../champions-return-to-arms-re diff --exit-code`. Both target
executables are verified Mach-O arm64/x86_64, respectively; x64 runs via Rosetta.

Changed tracked files are `native/include/champions/skills_b.hpp`,
`native/src/skills_b.cpp`, `native/tests/skills_b_tests.cpp`,
`tools/generate_skills_b_vectors.py`, `native/tests/vectors/skills_b.tsv.gz`,
`native/tests/vectors/skills_b.json` and this page. Nothing is pushed and no
issue or pull request is created.


| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/arrows-full-ctest.log 2>&1` | PASS; 103/103, 446.27 seconds; no failures or timeouts |

No native code or fixtures changed after the final full-suite build. The final
`git diff --check` and reference/fixture audit passed before the local commit.
