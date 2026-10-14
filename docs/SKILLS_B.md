# Return to Arms skills 35–68: model setup, scalar helpers and mana factory

PROGRESS: 239 of 239 reference functions ported

The [RTA Skill arithmetic kernel](SKILLS_KERNEL.md) now supplies the shared
statistic/tier and LCG primitives; this module retains its memory, rounding,
range, callback and failure contracts. No new port credit or scene integration.

`champions-skills-b` is a standalone C++17, raw32-memory port of the common Skill
statistics/mana contract, passive statistic queries, paired handle lists, mutation-safe
iterators, spell lifecycle, shared projectile movement, healing/protection, and
enemy/undead conversion, Giant Growth, Ancestral Call, melee buffs/strikes,
Resurrection, Cyclone, Cone of Frost / DummyDamager, elemental arrows / Multi
Fire, Shock, Frost Storm / Ice Shards, Ground Pound, Harm Touch, Darkness,
Shock of Fire / Sparks, Planar Wind, Shadow Step, Strong Stance trigger and
Stunning Strike effects in `ref_skills_b.py`.
It is independent of Godot and depends on the existing core arithmetic and
creature-HP sparse memory libraries. It has no Session, combat or scene integration.
All 239 portable B-reference source units are implemented and tested, including
model setup, scalar helpers and the runtime mana factory. Engine integration
remains separate work; delegated services remain explicit callbacks.

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
zero-valued stub. Unknown Skill active casts use explicit typed callbacks; the healing/protection and
conversion, Giant Growth, Ancestral Call, Bash, Critical Hit, Charge, Resurrect,
Cyclone, Cone of Frost, Shock, Frost Storm, Ground Pound, Harm Touch, Darkness,
Shock of Fire, Planar Wind, Shadow Step, Fire/Cold/Poison/Explode Arrow and Multi Fire casts dispatch
internally. Skill cast, Spell run and
GameObject constructor callbacks carry a typed three-pattern vector by value.
`cast_base` returning zero is the documented base body, not an active-skill fallback.
There are no embedded statistic tables, guessed effects or damage calculations.

## Explicit domain and refusals

Statistic indices are limited to 0–4. Level-minus-first must fit signed32 before
conversion; a larger Python integer intermediate refuses instead of wrapping.
Memory uses the existing raw32 address/span checks, missing-byte zero reads,
1 MiB sparse backing cap and 65,536-entry write-history limit. Skill/Spell virtual
slots above 15 and Creature/generic table slots above 64 refuse. Virtual/callback
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

No entry in the pinned Python `FUNCTIONS` inventory remains unported. The table
below records the completed B-reference scope and the separate Root boundary. Common statistics and mana costs apply to every ID,
with caller-supplied tables. Minor Healing, Holy Shield, Healing, Convert Enemy, Convert Undead and Giant Growth are fully
implemented standalone contracts with explicit service callbacks. Ancestral Call,
Bash, Critical Hit, Charge, Resurrect, Cyclone and Cone of Frost are also complete.
Fire, Cold, Poison and Explode Arrow, plus Multi Fire, now have complete standalone
B-reference bodies. Shock, Frost Storm / Ice Shards, Ground Pound, Harm Touch,
Darkness, Shock of Fire / Sparks, Planar Wind and Shadow Step are also complete. Root (52) is
owned by `ref_skills_a.py`; its active behavior is not
in the B reference inventory and also remains unsupported here.

| ID | Skill | Remaining registered functions |
|---|---|---|
| 35 | charge | None; complete cast/constructor/run/scaled-statistic/contact contract |
| 36 | darkness | None; complete cast/constructor/cloud-run/ambient-byte/box-query contract |
| 37 | shock | None; complete cast/constructor/flight/chain/beam/particle contract |
| 38 | groundpound | None; complete eligibility/cast/constructor/effect-run contract |
| 39 | convertenemy | None; complete can-use/cast/constructor/run contract |
| 40 | convertundead | None; complete can-use/cast/constructor/run contract |
| 41 | cyclone | None; complete eligibility/cast/constructor/run/effects contract (also ID 84) |
| 42 | harmtouch | None; complete eligibility/cast/constructor/countdown/tint/landed-hit contract |
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
| 57 | shockoffire | None; complete cast/constructor/flight/burst/touch contract |
| 58 | froststorm | None; complete cast/storm construction/run/shard run contract |
| 59 | lifetap | None; complete base-cast alias and statistic contract |
| 60 | blunt | None; common/passive statistic subset only |
| 61 | slashing | None; common/passive statistic subset only |
| 62 | strongstance | None; complete base-cast alias, statistic and trigger contract |
| 63 | naturalresistance | None; common/passive statistic subset only |
| 64 | stunningstrike | None; complete statistic and effect contract |
| 65 | giantgrowth | None; complete can-use/cast/constructor/run/scale/particle contract |
| 66 | planarwind | None; complete cast/constructor/strike/front-run contract |
| 67 | shadowstep | None; complete cast/constructor/run contract |
| 68 | resurrect | None; complete eligibility/target/cast/constructor/run/owner-death contract |

Remaining shared registered functions: none. No registered B-reference entry remains unported; Root belongs to the A reference.

Unregistered query-list filling and complete virtual/callback routing are implemented.
Skill, Spell and Creature body inventories dispatch internally; external services
remain explicit callbacks. Generic class tables preserve the inherited callback-only
path. Model-stack allocation, spell walking/ending, node allocation/freeing and list
observation are also implemented.

Remaining portable B-reference source units: none. Model setup, scalar/statistic
helpers and the runtime mana factory are complete. No function receives credit
for an explicit refusal or merely an inline equivalent. Future work requires
consumer integration or contracts from other reference modules.
Plain MissileWeapon, poisonGasSpell, StrongStancePhantom and LightEffect internals
remain delegated by this reference.

## Reference function accounting

The generator parses the pinned Python AST and records an explicit name inventory
in `reference_progress` in the existing fixture manifest. There are 241 source
function definitions: 234 directly declared class methods and seven top-level
functions. Two inventory/harness helpers are excluded: `shape_result` (default
stub output shaping) and `_register` (inventory registration). This leaves 239
portable units. The runtime `_mana_cost_method` factory counts once and is implemented as a
callable capturing the slot and forwarded argument. Generated per-ID mana aliases are separately tracked in
the registered-body inventory; they are not additional source definitions. Inherited methods
from other Python files are not counted.

The manifest names all 239 credited source definitions and has no remaining units.
This continuation adds `Model.__init__`, `Model._stat_via_slot14`, `Model.cstr`,
`Model.f32`, `Model.half_toward_zero`, `Model.is_player` and `_mana_cost_method`: seven
source units. The preceding routing continuation added six source units.
The preceding passive continuation added two source units and four registered
entries. The registered-body inventory
has 192 supported and zero remaining entries. All credited definitions have native
implementations exercised by exact vectors or host checks. Callback refusals,
inlined helper equivalents without their own native implementation, and harness
glue receive no credit. Historical counts below
describe their earlier continuation only. These counts establish the bounded standalone Python contract port, including
model setup; they do not establish engine or scene integration.

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

## Shock projectile and recursive chain family (37)

The complete family is authored from the pinned `ref_skills_b.py` Shock section,
`docs/spec/skills-35-68.md` section 37, and its second-pass matched execution
record in `docs/spec/skills-35-68-executed.md`. Five registered bodies and the
supporting `_shock_wants` method add six credited portable units. Counting still
uses top-level definitions and class methods in the pinned file; only the two
inventory/harness helpers are excluded. Delegated service implementations are
never credited as locally ported behavior.

- Cast and construction retain the conditional allocation, local position copy,
  signed facing/level, ordered light initialization, paired caster handle,
  normalized planar step, live slot-13 statistic read, and both launch sounds.
  `GameObject_ctor` remains an explicit service with the existing shared-draw ABI.
- Flight moves before collision and takes remaining lifetime before any callback.
  Terrain stops and eighth-frame box tests preserve the original store/call order.
  Caster slot 20 or the empty-owner creature filter remains callback-visible.
- Chain construction walks candidates from the head, uses strict planar distance
  comparisons, skips dead/already-chained victims, then recursively visits the new
  victim and the original origin. Victims are paired handles at the head; damage
  uses reverse selection order and re-reads the caster for each call. Duplicate
  candidates, null entries, signed byte capacities and zero-capacity edge behavior
  are retained. Capacity is not silently clamped to the eight beam slots.
- One shared draw builds the masked four-lane damage descriptor. Beams create
  once and then update using owned typed vectors and live next-node reads. Light
  pulsing/fade, 42 own-generator particle groups, occasional second particles,
  and final slot-1 deletion preserve their reached writes and random states.
  Internal Skill cast and adjusted object run virtual dispatch are extended;
  explicit external/named dispatch remains available.

Allocation, terrain/world queries, enemy votes, damage, beams, sound, resources,
lights, particles and destruction remain explicit services. Missing or malformed
services refuse, preserving reached prefixes. The existing memory/history/walk
bounds apply; recursive chain selection additionally shares the 64-depth guard.
Exceeding it refuses rather than using an invented truncated chain, and failures
restore the guard. Host checks cover cyclic candidate walks, excessive recursive
depth, callback self-removal, typed beam fields and missing-service partial state.
No original skill tables, engine internals, player cast sequencing, or Session/scene
integration are supplied by this standalone port.

Only the existing seven files change: `native/include/champions/skills_b.hpp`,
`native/src/skills_b.cpp`, `native/tests/skills_b_tests.cpp`,
`tools/generate_skills_b_vectors.py`, `native/tests/vectors/skills_b.tsv.gz`,
`native/tests/vectors/skills_b.json`, and this page. Shared build/roadmap/CI files
and the research repository are unchanged. The remaining table above and the
manifest retain the exact continuation inventory; Darkness, Ground Pound,
Harm Touch, Shadow Step, Shock of Fire, Frost Storm, Planar Wind and remaining
passive effects/infrastructure still need work.

Private acceptance measurements are skipped: the instructed
`../../private-validation` inputs are unavailable and no alternative locations
were searched. Windows, physical Intel hardware, original-console execution,
Norrath, service internals and playable integration remain unverified.

### Validation record for Shock

The final corpus contains **79,136 complete-state cases across all 16 FPU
configurations**, including **9,008 appended Shock cases**. All 70,128 preceding
rows are retained byte for byte. Apple Silicon arm64 and x64 through Rosetta both
match trace SHA-256
`a0a4e13929c75c99d54f7514626b64ebac90e95d74779566d76ef14d7ab42ce6`.
Cases exercise every new method, player/nonplayer/null casters, signed levels and
facing, dead/null/duplicate candidates, strict chain ties and radius boundaries,
zero/negative capacities, terrain versus object strikes, final-frame strikes,
beam creation/update, empty queries, particles, descriptor overflow, mutation and
failure prefixes, virtual self adjustment and explicit overrides. A complete
terrain-stop lifetime runs through its final particles and deletion.

The ignored `work/audit-shock.py` independently traverses the pinned source AST,
confirms 239 units and the manifest's 189/50 credited/remaining partition, compares
the complete prior decompressed fixture prefix, verifies reference/generator/vector
hashes, and checks unchanged research tracked files. PASS. The first host-check
build found a duplicate local variable name; it was fixed before both final builds.
All targeted commands below use the final code and final fixture.

| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/shock-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/shock-build.log 2>&1` | PASS |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz > work/shock-generation.log 2>&1` | PASS; 79,136 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/shock-tests.log 2>&1` | PASS; 3/3, 16.08 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/shock-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/shock-x64-build.log 2>&1` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/shock-x64-tests.log 2>&1` | PASS; 3/3, 25.32 seconds |
| `python3 work/audit-shock.py` | PASS; independent count/partition, pins and old-row prefix |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `git -C ../../champions-return-to-arms-re status --short` | PASS; clean |
| `git diff --check` | PASS |

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/shock-full-ctest.log 2>&1` | PASS; 103/103, 353.47 seconds; no failures or timeouts |

Both affected executables are verified Mach-O arm64/x86_64, respectively. No
native code or fixture changed after the final builds. The final audit and
`git diff --check` pass before the local commit. Nothing is pushed; no issue or
pull request is created.

## Frost Storm and Ice Shard family (58)

This complete family is handwritten from the pinned `ref_skills_b.py` section 58,
`docs/spec/skills-35-68.md` section 58, and the matched third-pass execution notes
in `docs/spec/skills-35-68-executed.md`. Four registered bodies and four supporting
units add eight credited reference functions. The supporting units are
`_froststorm_point`, `_iceshard_particle`, `down`, and `c_rem`; each has its own
native implementation and direct synthetic coverage. No delegated service body
is credited as implemented here.

- Cast reads the adjusted level and all four live statistics in order, offsets a
  player centre along signed16 facing, wraps radius/duration integer products,
  caps a nonplayer radius at 180, and plays the sound even when allocation fails.
- The storm is a GameObject, not an attached Spell. Construction preserves its
  paired caster handle, light fields, drift conversion and scheduling order.
  Nonplayer creatures get the float-converted 40-frame delay and optional warning
  LightEffect built inline, including its second shared draw and ordered stores.
- Active runs preserve movement, light relinking, both snow loops, ground-height
  checks, and every-tenth-frame shard allocation. Particle loop comparisons use
  the selected raw32 FPU model, including their exact multiplication order. The
  downward-magnitude helper preserves NaN payloads and canonicalizes both zeros
  to positive zero. Own and shared random streams stay separate.
- Every-thirtieth-frame damage queries keep list order, duplicate/null objects,
  slot-20 votes, and caster re-reading after a callback. An empty caster handle
  accepts every supplied object. The descriptor stores truncated half-damage,
  while both floating damage arguments retain the half. Shrinking still ticks;
  ending writes 0.95 and deletes through slot 1 only below radius 12.
- Shards preserve gravity, ordered movement, trail particles, strict landing
  height, burst particles, and deletion on landing or timeout. Internal virtual
  dispatch covers Skill cast and both adjusted object run bodies. Named and
  forced external object dispatch remain available.

Engine allocation/construction, light/resource/sound services, terrain and world
queries, enemy votes, damage, particles, scheduling and destruction remain
explicit callbacks. Warning LightEffect run behavior is delegated. Missing or
malformed services refuse with reached writes/draws retained. The existing
history, memory, recursion and scratch bounds still apply; a storm's particle
loops share a 65,536-iteration host guard, and each shard run has the same guard.
These guards refuse instead of substituting a truncated particle loop. Host
checks exercise infinite loop counts, invalid query results, missing services,
callback self-removal and the typed vectors/descriptor ABI.

Only the existing seven skills-B files change: header, source, native tests,
vector generator, compressed TSV, JSON manifest, and this page. Shared build,
roadmap and CI files stay untouched. The remaining table and manifest are updated
for the next continuation; Darkness, Ground Pound, Harm Touch, Shock of Fire,
Planar Wind, Shadow Step, passive effects and remaining infrastructure still need
work.

Private acceptance measurements are skipped because the instructed
`../../private-validation` inputs are unavailable. No alternative locations are
searched. Windows, physical Intel hardware, original-console execution, Norrath,
service internals and Session/playable scene integration remain unverified or
unsupported.

### Validation record for Frost Storm / Ice Shards

The final corpus contains **90,272 complete-state cases across all 16 FPU
configurations**, including **11,136 appended cases**. All 79,136 preceding rows
are retained byte for byte. Both arm64 and x64 through Rosetta match trace SHA-256
`e71dc92fb499c4e0bafb1cfc4755dadd6759280066c6a4a7b0e15e556173f202`.
Coverage includes player/nonplayer/null casters, signed facing/levels/frames,
integer wrapping, allocation failure and warning-light branches, start/end
boundaries, exact particle loop thresholds, shrinking damage ticks, ordered
queries with duplicates/null objects, enemy votes, live callback mutation and
failure prefixes, shard falling/landing/timeouts, direct helpers, virtual self
adjustment, and named/forced external object dispatch. Synthetic run sequences
cover active storms through shrinking/deletion and shard landing/burst behavior.
Host checks also verify typed callback ownership and bounded infinite counts.

The generator now streams output into a staged file instead of retaining all
rows and the joined corpus in memory. Optional `--reuse-prefix` checks the prior
reference pin set, credited-unit compatibility, full vector hash, row count,
and every preceding input row against the current scenario iterator before
reusing its expectations. Newly appended rows still execute the pinned Python
contract. Invalid evidence, hash, inputs, count and gzip trailers all reject
without replacing the destination. `--verbose` identifies newly generated rows
for diagnosis. The default path still executes every Python expectation.

An independent ignored script regenerates all 11,136 new rows through the default
streaming path (selecting the appended scenario range with `islice`) and compares
every byte against the final suffix. The independent AST/pin audit confirms
239 portable units, the 197/42 partition, fixture/generator hashes, the unchanged
old prefix, and clean research tracked files. Both executables are verified
Mach-O arm64/x86_64, respectively.

The initial native compile caught one mixed-type `auto` declaration; it was
split before final builds. Initial generation was stopped when a malformed new
synthetic caster node caused the reference's unbounded clear walk to stall: its
missing backlink was corrected. The next staged generation exposed a missing
explicit `Creature.v4` callback in forced-external fixtures; that callback was
supplied before the successful generation. These corrections changed no old
input or expectation. Generator streaming and diagnostic output made the stall
visible and reduced memory retention. The stopped attempts returned 143; the
missing-callback attempt returned 1. Final successful commands follow.

| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/frost-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/frost-build.log 2>&1` | PASS after the declaration fix |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/frost-before.tsv.gz > work/frost-generation-stream.log 2>&1` | PASS; 90,272 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/frost-tests.log 2>&1` | PASS; 3/3, 24.72 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/frost-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/frost-x64-build.log 2>&1` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/frost-x64-tests.log 2>&1` | PASS; 3/3, 35.37 seconds |
| `python3 work/audit-frost.py` | PASS; independent inventory, pins, old-row prefix, hashes and research cleanliness |
| `python3 work/verify-frost-generation.py > work/frost-regeneration.log 2>&1` | PASS; independently regenerated all appended rows |
| `python3 work/test-frost-prefix.py > work/frost-prefix-tests.log 2>&1` | PASS; five invalid-prefix rejection cases |
| `build/native/champions-skills-b-tests > work/frost-host.log 2>&1` | PASS |
| `build/native-x64/champions-skills-b-tests > work/frost-x64-host.log 2>&1` | PASS |
| `git diff --check` | PASS |

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/frost-full-ctest.log 2>&1` | PASS; 103/103, 366.20 seconds; no failures or timeouts |

No native code or fixture changed after the successful architecture builds and
final full-suite run. The final inventory/pin/prefix audit and `git diff --check`
pass before the local commit. Nothing is pushed, and no issue or pull request
is created.


## Ground Pound family (38)

The complete family uses the pinned authored `ref_skills_b.py` can-use and
Ground Pound sections, specification section 38, and the second-pass matched
execution/correction notes. Four registered bodies plus `_groundpound_stat`
add five credited portable units; delegated services receive no implementation
credit. The AST denominator remains 239 source definitions after excluding only
`shape_result` and `_register`.

- Eligibility retains signed weapon-class/set bytes, wrapped item-record indexing,
  item-definition checks and the 180-frame HUD notice. Nonplayers return early.
- Player casts set action state 10 and write the clip's four shared event slots
  in exact order before starting the blend and playing sound. They create no
  effect and retain no level. Nonplayer casts return zero without side effects.
- Construction retains GameObject's delegated shared draw, light stores/linking,
  facing/level truncation, paired owner handle, scheduling, the 30-unit forward
  centre and strict 144-unit ground-distance test. Water stores its byte return;
  wet and ledge branches unlink the light before final placement/relinking.
- First-run effects preserve all 164 dust-ring particles (492 own draws), the
  optional delegated LightEffect/shared draw, 60 burst particles, dry/wet debris,
  sign-extended decal angle, camera shake, ordered query and damage/stun calls.
  Third-generator cosmetic draws remain separate from own stun draws and shared
  construction draws. Reach multiplication and stun frames wrap as documented;
  the chance subtraction uses the reference's wider integer comparison.
- Damage re-reads owner handles and statistics after callbacks, preserves strict
  planar reach, clamps both rounded damage bounds to at least one, and draws for
  living creatures even at zero stun chance. Counter 30 calls deletion, then
  still performs reached spark/ripple/light updates and byte counter increment.
  A supplied synthetic 31-run sequence covers both dry and wet complete lifetimes.
- Skill eligibility/cast and adjusted effect run virtual dispatch now execute
  internally; forced external and named callback dispatch retain their ABI.

Allocation, GameObject/LightEffect construction, animation, resources, sound,
lights, terrain/water, particles, decals, camera, query, enemy votes, damage,
stunning and deletion remain explicit services. Water's discarded output is
passed as the reference's literal `stack` token; it is not an invented host
pointer. Missing or malformed callbacks refuse with earlier writes/draws retained.
The existing memory, history, query-count, recursion and ball-sampling bounds apply.
No Player_run animation-event sequencer, original resource data, effect destructor,
engine service implementation, Session or scene integration is supplied.

Changed files are the existing header, source, native tests, generator, compressed
TSV, JSON manifest and this page. Shared build, roadmap and CI files are unchanged.
The remaining table and manifest now leave Darkness, Harm Touch, Shock of Fire /
Sparks, Planar Wind, Shadow Step, passive effects and model infrastructure for
later continuations. Private acceptance measurements are skipped because the
instructed private inputs are unavailable; no other locations were searched.
Windows, physical Intel hardware, original-console execution, Norrath and playable
integration remain unverified.

### Validation record for Ground Pound

The final corpus contains **97,408 complete-state cases across all 16 FPU
configurations**, including **7,136 appended Ground Pound cases**. All 90,272
preceding rows remain byte for byte. arm64 and x64 through Rosetta both match
trace SHA-256
`66d072ac09197c5cf033438468497eac6a152bec657f580a47b59b125a95a9f7`.
Coverage includes signed weapon/set bytes and levels, item definitions,
player/nonplayer/null casters, exact clip-write order, ground-distance and
planar-distance boundaries, water-byte truncation, live callback mutation,
empty/null/duplicate query entries, damage rounding/clamping, stun chance/HP
boundaries, integer wrapping, seed variation, fading and counter wrap,
allocation/service failures, named/external/adjusted virtual dispatch, and
31-run dry/wet lifetimes. Host checks verify typed water output placeholders,
missing-service prefixes, invalid and oversized query refusals, callback
self-removal, owner removal before damage, and continuation after deletion.

The first targeted run passed all exact vectors and transport checks but failed
one new host fixture: scalar callbacks had been supplied where the existing
runtime requires typed particle/decal callbacks, and its owned-statistic path
needed `Skill.v12`. The fixture was corrected before final builds; no contract
body or preceding vector required a compatibility fix. The first generator
attempt was interrupted (130) to correct a new fixture's player-record constant
and signed-byte expression before execution reached it. The final generation
and independent suffix regeneration both pass.

| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/pound-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/pound-build.log 2>&1` | PASS; final build has no warnings |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/pound-before.tsv.gz > work/pound-generation.log 2>&1` | PASS; 97,408 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/pound-tests.log 2>&1` | PASS; 3/3, 45.75 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/pound-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/pound-x64-build.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/pound-x64-tests.log 2>&1` | PASS; 3/3, 70.64 seconds |
| `build/native/champions-skills-b-tests > work/pound-host.log 2>&1` | PASS |
| `python3 work/audit-pound.py > work/pound-audit.log 2>&1` | PASS; independent AST count/partition, pins, hashes, old-row prefix and research cleanliness |
| `python3 work/verify-pound-generation.py > work/pound-regeneration.log 2>&1` | PASS; all 7,136 appended rows independently regenerated and byte-compared |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; Mach-O arm64 and x86_64 respectively |
| `git -C ../../champions-return-to-arms-re diff --exit-code` and `git -C ../../champions-return-to-arms-re status --short` | PASS; unchanged and clean |
| `git diff --check` | PASS |

The independent ignored audit script traverses the pinned AST separately,
confirms the 202/37 credited/remaining partition of 239 portable units and
169/23 registered bodies, verifies all pinned and generated hashes, and compares
the entire preceding decompressed prefix. The independent regeneration script
executes only the new scenario range through the default Python expectation
path and compares every byte of its result to the final corpus suffix.

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/pound-full-ctest.log 2>&1` | PASS; 103/103, 1,437.54 seconds; no failures or timeouts |

No native code or fixture changed after the successful final builds and full
CTest run. The final independent audit and `git diff --check` passed before the
local commit. No push, issue, or pull request is created.


## Harm Touch family (42)

This continuation ports all six source methods: `harmtouch_can_use`,
`harmtouch_cast`, `harmtouch_spell_ctor`, `_harmtouch_tint`, `harmtouch_run`,
and `harmtouch_apply_to_hit`. Evidence is the pinned authored Python section 42,
specification section 42, and matched execution/correction notes. The independent
AST denominator remains 239 after excluding `shape_result` and `_register`.

- Player eligibility warns for weapon classes 4/6; nonplayers return early.
  Cast allocates without level adjustment, returns the constructed spell, and
  leaves attachment to its caller. Construction makes one shared draw and
  preserves the timer, sound and exact write/callback order.
- Run subtracts the raw32 1/60 constant, ends strictly below zero, and otherwise
  updates the owner's three tint fields. Null owners still write through zero.
  Direct tint coverage checks signed zero, infinities and NaNs in every FPU mode.
- A noncreature hit refreshes tint and leaves the range and spell armed. A creature
  hit emits 102 pairs of sampled particles, with 204 private draws and two model
  virtual calls per pair. Count and model are re-read for every pair; the second
  draw is discarded, while the second particle scales the first velocity.
- Optional LightEffect construction adds one delegated shared draw before the
  live statistic lookup. Damage replaces the supplied minimum/maximum, uses the
  205th private draw for the spread, and reads the minimum back before storing
  the maximum. Output aliases may change the seed/owner/skill/timer. Slot 1 then
  ends the spell through an explicit callback.
- Internal virtual dispatch covers adjusted Skill eligibility/cast and Spell
  countdown; named and forced external Spell dispatch remain available.

HUD, allocation, sound/resources, model sampling, particles, LightEffect and
spell destruction remain explicit callbacks. Missing/malformed services refuse
with reached writes and draws retained. Zero model-point counts have no defined
sample index in the evidence: this domain refuses after the first private draw
and count callback, before the second model virtual call. It is not replaced by
an invented point. Existing memory/history/recursion/scratch bounds remain.
No Player melee-hit sequencer, engine services, Session adapter or playable
integration is added. No Norrath behavior is inferred.

Only the existing header, source, tests, generator, compressed vector file,
manifest and this page change. Shared build, roadmap and CI files are untouched.
The remaining table now leaves Darkness, Shock of Fire / Sparks, Planar Wind,
Shadow Step, passive effects and model infrastructure for later runs. Private
acceptance measurements are skipped because the instructed inputs are absent;
no alternative locations were searched. Windows, physical Intel hardware,
original-console execution, engine service internals and scene integration remain
unverified or unsupported.


### Validation record for Harm Touch

The final corpus contains **101,760 complete-state cases across all 16 FPU
configurations**, with **4,352 appended Harm Touch cases**. All 97,408 preceding
rows are retained byte for byte. Coverage includes player/nonplayer/null owners
and targets, signed byte levels and weapon classes, allocation failure, timer
boundaries and full countdown, noncreature hits, signed sample counts, degenerate
and nonfinite particle directions, seeds, optional light construction, signed
statistic extremes, output aliases, live callback writes, failures at every
service boundary, adjusted and external/named dispatch, and an attached-spell
landed-hit sequence. Host checks additionally verify the undefined zero-count
prefix, invalid sample shapes and callbacks that remove themselves.

The initial build exposed a test-only use of the module-private `DT` identifier;
it was replaced by the documented raw32 1/60 constant before successful final
builds. An initial generation was interrupted (130) to finalize the new callback
mutation fixtures. Independent suffix generation succeeded, but its first
comparison ran before the full fixture replacement and compared against the old
corpus (exit 1); the subsequent standalone comparison uses the completed corpus.
These corrections changed no preceding input or expectation and no native body.

Exact executed validation commands and final results are recorded below.


The independent streaming audit confirms all 239 AST units, the 208/31
partition, 174 supported/18 remaining registered bodies, every evidence pin,
vector/generator hashes, the complete unchanged old-row prefix and clean
research tracked files. Independently generated appended expectations compare
byte for byte with the final suffix. Expected complete-state trace SHA-256 is
`e110af76255fb8dfb38770c0f8442fdd9ee674a047faf314b09ce4d46a57d6c3`.
The first ignored audit attempt used a mistakenly substituted denominator in its
assertion; the independent source traversal found it and the corrected audit
passes. No product source, generator or fixture changed for that correction.


Both affected executables are verified Mach-O arm64 and x86_64. The x64
Release CTest run passes 3/3 (80.57 seconds), including all 101,760 exact cases.
The additional arm64 streaming comparison matches all 101,760 states with the
same trace hash, while retaining bounded comparison memory. The first arm64
CTest group passed host/transport but hit the 150-second exact-test limit; the
required isolated retry hit the runner's 120-second subprocess limit. Observed
host load averages were above 250 during this contention. The unchanged exact
test subsequently passed in the final full-suite run (62.33 seconds). No timeout
limit or production test runner was changed.

| Exact command | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/harm-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/harm-build-final.log 2>&1` | PASS; final build has no warnings |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/harm-before.tsv.gz > work/harm-generation-final.log 2>&1` | PASS; 101,760 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/harm-tests.log 2>&1` | 2/3; exact case timed out under contention |
| `ctest --test-dir build/native -R '^skills-b-exact$' --output-on-failure > work/harm-exact-alone.log 2>&1` | Runner timeout after 120 seconds; subsequently passes unchanged in full suite |
| `python3 work/check-harm-stream.py build/native/champions-skills-b-tests > work/harm-stream-arm64.log 2>&1` | PASS; all 101,760 full states |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/harm-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/harm-x64-build.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/harm-x64-tests.log 2>&1` | PASS; 3/3 |
| `build/native/champions-skills-b-tests > work/harm-host.log 2>&1` | PASS |
| `build/native-x64/champions-skills-b-tests > work/harm-x64-host.log 2>&1` | PASS |
| `python3 tools/test_skills_b_vectors.py --exe build/native/champions-skills-b-tests --vectors work/harm-new-only.tsv.gz > work/harm-suffix-tests.log 2>&1` | PASS; all 4,352 new states |
| `python3 work/verify-harm-generation.py > work/harm-regeneration.log 2>&1` | Generated all 4,352 new rows; comparison was premature (old full corpus) |
| `python3 work/audit-harm.py > work/harm-audit-final.log 2>&1` | PASS; independent inventory/pin/prefix/hash audit |
| `python3 work/compare-harm-suffix.py > work/harm-suffix-comparison.log 2>&1` | PASS; independently regenerated suffix byte comparison |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; arm64 and x86_64 respectively |
| `git -C ../../champions-return-to-arms-re diff --exit-code` and `git -C ../../champions-return-to-arms-re status --short` | PASS; unchanged and clean |
| `git diff --check` | PASS |


| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/harm-full-ctest.log 2>&1` | PASS; 103/103, 399.63 seconds; no failures or timeouts |

No native code, generator or fixture changed after the successful final builds
and full CTest run. arm64 and x64 match the complete expected-state trace hash
above. Final `git diff --check` passes; only the seven scoped files are staged
for a local commit on `codex/port-skills-b`. No push, issue or pull request is
created. The exact remaining inventory above and in the manifest is the next
continuation's starting point.


## Darkness family (36)

The complete family ports `darkness_cast`, `ambient_bytes`, `darkness_ctor`,
`_darkness_box`, and `darkness_run`: five credited source units, four registered
bodies. Evidence is the pinned authored Python section 36, specification section
36 and the second-pass execution/correction notes. An independent AST traversal
confirms 239 portable source definitions, excluding only `shape_result` and
`_register`; callback refusals receive no implementation credit.

- Cast ignores the supplied position, uses signed16 facing and unnormalised
  trigonometric velocity at 373 units/second, reads through a null caster, and
  returns zero even after successful construction.
- Construction uses GameObject directly, retains its one delegated shared draw,
  captures position/velocity before callbacks, writes and links the light in exact
  order, preserves raw32 level and 108-frame life, attaches a paired caster handle,
  and reads all four ambient bytes. Three halfword colours store one minus each
  byte, with exact truncation.
- Flight updates coordinates sequentially without a grid relink and stops only
  when terrain is strictly above its height. Box queries preserve order, duplicates
  and null entries. The first accepted target attaches the cloud; every accepted
  entry is voted on again before damage. Owner and Skill pointers are re-read
  after services, while level is captured once per run.
- Attached clouds roll only on signed global frames divisible by 30, after the
  target vote, strict positive HP and zero signed16 no-target timer. One private
  draw tests `r % 100 + 1 < signed chance`; frame multiplication wraps to signed32,
  and nonzero boss rank applies an arithmetic half plus one.
- Every reached run relinks the light and emits eight particles from private
  rejection-sampled balls before decrementing life. Deletion occurs only at -1
  and precedes light unlinking. These particle draws remain coupled to later
  blindness rolls. Internal adjusted Skill cast and Creature slot-4 dispatch,
  named callbacks and forced external object dispatch retain the existing ABI.

Allocation, base GameObject construction, resource/sound services, terrain,
box queries, enemy votes, boss rank, no-target timer application, damage, particles,
lights, scheduling and deletion remain explicit callbacks. Missing or malformed
callbacks refuse with earlier writes and draws preserved. Existing scratch,
memory, history, recursion, ball-sampling and query-size bounds apply. No engine
service internals, destructor, Session adapter or playable integration is added.
No Norrath behavior is inferred.

Only the existing header, source, tests, vector generator, compressed TSV,
manifest and this page change. Shared build files, roadmap and CI are unchanged.
Remaining portable skill families are Shock of Fire / Sparks, Planar Wind,
Shadow Step and passive effects; the manifest lists the remaining model plumbing.
Private acceptance measurements are skipped because the instructed private inputs
are unavailable; no alternative locations were searched. Windows, physical Intel
hardware, original-console execution and playable integration remain unverified.

### Validation record for Darkness

The corpus contains **107,456 complete-state cases across all 16 FPU
configurations**, including **5,696 appended Darkness cases**. All 101,760
preceding rows remain byte for byte. Coverage includes signed facing/level/attached
bytes, null casters and owner handles, ambient-byte extremes, aliased constructor
inputs, allocation failure, movement/terrain boundaries, duplicate and null box
entries, vote sequences, HP/timer/chance boundaries, signed frame remainders,
wrapped statistics and durations, boss adjustments, callback mutations and failures,
adjusted/named/external dispatch, a complete 109-run empty flight and a complete
61-run attached lifetime. Host tests check malformed/oversized query and terrain
results, missing service prefixes, removal of owner handles between votes,
callback self-removal, all eight particles before expiry, and deletion before unlink.

Initial compilation exposed unqualified global identifiers and a test lambda's
missing capture; these were corrected before final builds. The first generator
was interrupted to correct a synthetic handle backlink. The first exact run
found a new adjusted-object fixture without the corresponding adjusted object
state: it reached an unavailable statistic callback in native code and a default
stub in Python. The fixture now supplies valid adjusted state. No preceding input,
expectation or native behavior changed for these fixture corrections.

The independent audit confirms the 213/26 source-unit partition, 178/14
registered-body partition, all pins and hashes, the complete unchanged preceding
prefix and clean research tracked files. Independent regeneration executes all
5,696 appended scenarios through the pinned Python expectation path and compares
every byte. arm64 and x64 under Rosetta match the complete expected-state trace
SHA-256 `158f775b3bc55c7f9383dd25c4ce0e60ab31e5629b20ca8af6e8ce7447c5b7a5`.
The executables are verified Mach-O arm64 and x86_64 respectively.

| Exact command | Final result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/dark-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/dark-build.log 2>&1` | PASS; no warnings in the final build |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/dark-before.tsv.gz > work/dark-generation.log 2>&1` | PASS; 107,456 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/dark-tests.log 2>&1` | PASS; 3/3, 25.36 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/dark-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/dark-x64-build.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/dark-x64-tests.log 2>&1` | PASS; 3/3, 39.12 seconds |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `python3 work/audit-dark.py > work/dark-audit.log 2>&1` | PASS; independent AST/pin/prefix/hash/cleanliness audit |
| `python3 tools/test_skills_b_vectors.py --exe build/native/champions-skills-b-tests --vectors work/dark-new-only.tsv.gz > work/dark-suffix-tests.log 2>&1` | PASS; 5,696 new complete states |
| `python3 work/verify-dark-generation.py > work/dark-regeneration.log 2>&1` | PASS; every appended row independently regenerated and byte-compared |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; arm64 and x86_64 |
| `git -C ../../champions-return-to-arms-re diff --exit-code` and `git -C ../../champions-return-to-arms-re status --porcelain` | PASS; unchanged and clean |
| `git diff --check` | PASS |

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/dark-full-ctest.log 2>&1` | PASS; 103/103, 345.09 seconds; no failures or timeouts |

No functional native code, generator or fixture changed after the successful
final builds and full CTest run. The final independent inventory/pin/prefix/hash
and research-cleanliness audit and `git diff --check` pass before the local
commit on `codex/port-skills-b`. Nothing is pushed; no issue or pull request is
created. The exact remaining inventory above and in the manifest supports the
next continuation.


## Shock of Fire / Sparks family (57)

The complete family ports `shockoffire_cast`, `sparks_ctor`, `sparks_run` and
`sparks_on_touch`: four source units and four registered bodies. Evidence is
section 57 of the pinned authored Python contract, specification section 57 and
its third-pass execution/correction notes. Independent AST accounting retains
239 portable units and excludes only `shape_result` and `_register`. Refusal
and partial dispatch infrastructure receive no new implementation credit.

- Cast captures position before statistic callbacks. Level adjustment precedes
  tier-1 direct damage for every caster; splash/radius/speed use slot 12, with
  caps 0/10/10. Signed16 facing produces unnormalised trigonometric velocity.
  Radius multiplication wraps to raw32; allocation failure and success return 0.
- Construction captures position and velocity before delegated base construction,
  preserves the shared draw, owner handle, exact light writes/link order, signed
  end-frame wrapping, damage fields, scheduling and sound. The tag stores last;
  the voice handle is never overwritten by this constructor.
- Flight uses the existing shared collision kernel. Only result exactly 1 clears
  the end frame; combined terrain/object contact retains it unless a hit clears
  it. All remaining flight effects still occur that frame: three ball samples,
  five discarded/used sample pairs, an odd-draw trail vote, optional trail ball,
  voice position and light relink. Every ball uses the object's private generator.
- Expiry uses a signed frame comparison, emits 50 particles first, and applies
  positive signed splash through explicit ring/radius-damage callbacks. An absent
  handle uses the object as attacker; a present handle with a null object uses
  zero. Attacker is captured before the ring callback, while position, splash
  and radius are read again afterward. Destruction delegates slot 1 with 3.
- Touch captures its owner before the enemy vote. A valid hit optionally delegates
  LightEffect construction (one shared draw), sounds, rereads low/high, builds the
  four-halfword fire descriptor and delegates damage. Only then does it clear end.
  Further enemies in the same walk receive no direct hit. Adjusted internal cast,
  run and touch dispatch, named callbacks and forced external dispatch are tested.

Base GameObject construction, light/resource/sound/voice services, terrain/water,
object queries and other creatures' contact/vote slots, particles, LightEffect,
ring effects, damage, scheduling and destruction remain explicit callbacks.
Missing or malformed services refuse with reached writes/draws retained. Existing
memory, history, recursion, scratch and sampling bounds apply. No engine service
internals, destructor, Session adapter, playable scene or Norrath behavior is added.

Only the existing header, source, tests, generator, compressed TSV, manifest and
this page change. Shared build/roadmap/CI files are untouched. Remaining active
families are Planar Wind, Shadow Step and passive effects; the manifest enumerates
all 22 remaining source units, including model plumbing and the runtime mana
factory. Private acceptance measurements are skipped because the instructed
inputs are unavailable; no alternative locations were searched. Windows, physical
Intel hardware, original-console execution and playable integration are unverified.

### Validation record for Shock of Fire / Sparks

The corpus contains **113,040 complete-state cases across all 16 FPU
configurations**, including **5,584 new cases**. All 107,456 preceding rows remain
byte for byte. New coverage includes tier selection, signed levels/facing/frames,
frame wrap, aliased constructor inputs, allocation failures, null/deleted owners,
terrain-only/object-only/combined collision, duplicate queries and enemy rejection,
water services, nonfinite motion, signed splash/radius boundaries, descriptor
truncation, particle draw order, live callback writes and failures, adjusted and
named/external dispatch, cast-to-hit-to-delayed-burst and the final ten flying
frames through expiry. Complete state includes sparse bytes, ordered writes,
callback arguments, returns/errors and shared draw counts. Host checks cover
malformed/oversized typed queries, unavailable services, callback self-removal,
constructor failure prefixes, captured owner and descriptor halfwords.

An initial build was attempted before asynchronous CMake configuration completed
and reported a missing cache; the subsequent configured build passed. Initial
host validation used a scalar handler for the typed terrain service; the test was
corrected. Initial exact validation exposed a synthetic adjusted Skill pointer
without its adjusted vtable, allowing a Python default stub and a native refusal;
valid adjusted state was added. No native body or preceding vector changed for
these test corrections. Final validation commands/results follow below.

The final independent audit confirms the 217/22 AST partition, 182/10
registered-body partition, every evidence pin, generator/vector hashes, the
unchanged complete preceding prefix and clean research tracked files.
Independent regeneration compares all 5,584 new rows byte for byte. Verified
Mach-O arm64 and x86_64 executables both match the complete expected-state trace
SHA-256 `8b7bff2453d674448e552ce1d11a62e9c30e8f6f45d72ee54e59be6886bcbed6`;
x64 execution uses Rosetta.

| Exact command | Final result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/sparks-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/sparks-build-final.log 2>&1` | PASS; no warnings |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/sparks-before.tsv.gz > work/sparks-generation-final.log 2>&1` | PASS; 113,040 cases |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/sparks-tests-final.log 2>&1` | PASS; 3/3, 27.60 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/sparks-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/sparks-x64-build.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/sparks-x64-tests.log 2>&1` | PASS; 3/3, 47.01 seconds |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `python3 work/check-sparks-suffix.py > work/sparks-suffix-tests.log 2>&1` | PASS; all 5,584 new complete states |
| `python3 work/audit-sparks.py > work/sparks-audit-final.log 2>&1` | PASS; independent AST/pin/prefix/hash/research audit |
| `python3 work/verify-sparks-generation.py > work/sparks-regeneration-final.log 2>&1` | PASS; every new row independently regenerated and byte-compared |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; arm64 and x86_64 |
| `git -C ../../champions-return-to-arms-re diff --exit-code` and `git -C ../../champions-return-to-arms-re status --porcelain` | PASS; unchanged and clean |
| `git diff --check` | PASS |

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/sparks-full-ctest.log 2>&1` | PASS; 103/103, 378.39 seconds; no failures or timeouts |

No native code, generator or fixture changed after the successful final builds
and full CTest run. Final `git diff --check` and research-cleanliness checks pass.
The seven scoped files are committed locally on `codex/port-skills-b`; nothing is
pushed, and no issue or pull request is created. The exact remaining table and
manifest inventories are ready for the next continuation.


## Planar Wind family (66)

The complete family ports `planarwind_cast`, `planarwind_spell_ctor`,
`_planarwind_strike` and `planarwind_run`: four credited source units and three
registered bodies. Evidence is section 66 of the pinned authored Python contract,
specification section 66 and third-pass execution/correction notes. Independent
AST accounting confirms 239 portable definitions, excluding only `shape_result`
and `_register`. Partial model dispatch and callback refusals receive no credit.

- Cast snapshots its position before adjusted-level and allocation callbacks,
  ignores supplied facing, and returns the constructed spell or zero.
- Construction retains the one shared draw, exact light store order, live owner
  facing, signed-byte level tiers, width/range/count arithmetic and rotated scatter
  matrix. The one-time arc query remains delegated. Its order and duplicates are
  preserved, with paired target handles inserted in reverse query order.
- Run moves the front in the documented store order. It saves each next link
  before the strike, tests the strict three-axis dot boundary and removes struck
  nodes. Player and nonplayer statistics, conditional PvP draw, unconditional burn
  draw, live owner/burn reads, sound, and 72 model-sample particles retain their
  exact order. No enemy, health, distance or corridor vote is invented at strike.
- Light relinking precedes speed decay. Stream particles use six private draws
  each, rotating the matrix and accumulating the three velocity nudges across
  particles. Signed level chooses the rarity mask. Terrain is queried after all
  effects; the counter increments before ending on the 54th run or strict terrain
  penetration. Signed32 counter overflow preserves raw32 stores.

Allocation, arc queries, damage, effect timers, sound/resources, lights, terrain,
model queries/sampling, particles and destruction remain explicit callbacks.
Missing/malformed callbacks retain reached writes and draws. Existing memory,
history, traversal, recursion and stack bounds apply; stream loops are bounded
by the existing history cap. A model count of zero refuses after the private draw
rather than inventing the undocumented console divide-by-zero result. This is a
standalone RTA contract; Session, engine internals and playable integration remain
work, and no Norrath behavior is inferred.

The exact remaining source units are `Model.__init__`, `Model._stat_via_slot14`,
`Model._virtual`, `Model.call`, `Model.cstr`, `Model.f32`, `Model.half_toward_zero`,
`Model.is_player`, `Model.shadowstep_cast`, `Model.shadowstep_run`,
`Model.shadowstep_spell_ctor`, `Model.skill_call`, `Model.spell_call`,
`Model.strongstance_trigger`, `Model.stunningstrike_effect`, `Model.table_call`,
`Model.vcall`, and `_mana_cost_method`. Shadow Step is the next complete active
family portable from the pinned contract. The remaining registered inventory is
listed in the table above and fixture manifest; source and registered inventories
are deliberately separate.

### Validation record for Planar Wind

The corpus contains **119,808 complete-state cases across all 16 FPU
configurations**, including **6,768 appended Planar Wind cases**. All 113,040
preceding rows remain byte for byte. Coverage includes null owners, signed
levels/facings/counters, light range/count boundaries, input aliasing, allocation
failure, query order/duplicates/nulls, removed targets, strict plane boundaries,
both statistic paths, PvP/burn rolls, signed/zero stream counts, model/no-model
sampling, negative sample counts, callback mutations/failures, adjusted and
external/named dispatch, and a complete 54-run lifecycle. Host checks verify
missing/malformed terrain services, expiry prefixes, zero model-sample count,
malformed sample results and callback self-removal. Every vector compares sparse
bytes, ordered writes, callbacks, returns/errors and shared draw counts.

Initial compilation found a missing explicit empty virtual-argument vector and
host-test type/constant errors; these were corrected. The first exact comparison
caught unsigned comparisons of signed-byte rank; both new rank branches were
corrected. A host check caught a scalar timer callback routed through typed
services; the timer now uses the scalar callback ABI. No preceding native body,
vector input or expectation changed. Independent regeneration executes and
byte-compares every appended row through the pinned Python expectation path;
an independent AST traversal validates the 221/18 source-unit partition and pins.

Only the existing header, source, tests, vector generator, compressed TSV,
manifest and this page change. Shared build files, roadmap and CI are untouched.
The research checkout remains clean and unchanged. Private acceptance measurements
are skipped because the instructed private inputs are unavailable; no alternative
locations were searched. Windows, physical Intel hardware, original-console
execution and playable integration remain unverified. x64 testing uses Rosetta.

Verified Mach-O arm64 and x86_64 executables both match all 119,808 complete
expected states (x64 through Rosetta), hence agree on expected-state trace SHA-256
`8b0a528ee68d73d736b4e7bb708562bab760897d9b03aaa3522814cd64302073`.

| Exact command | Final result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/wind-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/wind-build-final.log 2>&1` | PASS; no warnings |
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/wind-before.tsv.gz > work/wind-generation.log 2>&1` | PASS; 119,808 cases; preceding 113,040 rows hash-checked and input-checked |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/wind-tests-final.log 2>&1` | PASS; 3/3, 28.73 seconds |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/wind-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/wind-x64-build.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/wind-x64-tests.log 2>&1` | PASS; 3/3, 52.10 seconds |
| `python3 work/verify-wind.py > work/wind-regeneration.log 2>&1` | PASS; independent AST inventory, evidence hashes and byte comparison of all 6,768 regenerated new rows |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; arm64 and x86_64 |
| `git -C ../../champions-return-to-arms-re diff --exit-code` and `git -C ../../champions-return-to-arms-re status --porcelain` | PASS; unchanged and clean |
| `git diff --check` | PASS |

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/wind-full-ctest.log 2>&1` | PASS; 103/103, 400.59 seconds; no failures or timeouts |

No functional code, generator or fixture changed after the successful final builds
and test runs. The final generator/vector hash, complete preceding-prefix,
registered inventory, research-cleanliness and `git diff --check` audits pass.
The seven scoped files are committed locally on `codex/port-skills-b`; nothing is
pushed and no issue or pull request is created. Eighteen source units remain;
the updated table and manifest support continuation with Shadow Step.


## Shadow Step family (67)

Ports the complete `shadowstep_cast`, `shadowstep_spell_ctor` and `shadowstep_run`
family, from section 67 of the pinned Python contract and its specification and
executed corrections. No research files are edited.

- Cast ignores supplied position/facing, adjusts the level and allocates 0x34 bytes.
  Construction preserves its shared draw, live owner reads, action state, flags,
  normalized velocity, reverse facing and ordered raw32 stores.
- Tier-one statistics choose the calm timer and fire count. The centre fire and
  secondary ring retain allocation failures, signed counts, accumulated angles,
  previous-height input, the inclusive 72-unit height boundary and plain owner
  pointers. StaticFire construction remains delegated with its shared draw count.
- Box results keep order and duplicates. Creature/enemy/target-handle filters,
  the calm callback and conditional inward knock-back preserve their order.
  All 86 particles use private ball and sample draws; the optional no-ring light
  retains its shared draw and complete argument sequence.
- Run preserves offline screen clamping, two collision calls with one captured
  velocity and separately reread movers, cubic scale, the full 64-bit hidden flag,
  prediction byte, strict facing/progress boundaries and 25-call expiry.

Screen clamping, collision, queries, model sampling, particles, fire/light object
construction, sound/resources and destruction remain explicit callbacks. The
screen-clamp result must contain exactly three raw32 patterns; malformed services
refuse with reached writes retained. Existing traversal/history limits bound the
ring and queries. Zero model sample counts refuse after reached private draws; no
console divide-by-zero behavior is invented. No destructor body exists in this
Python family; its invoked end service stays delegated. No scene integration,
Norrath behavior or engine-service internals are claimed.

Remaining source units (15): `Model.__init__`, `Model._stat_via_slot14`,
`Model._virtual`, `Model.call`, `Model.cstr`, `Model.f32`,
`Model.half_toward_zero`, `Model.is_player`, `Model.skill_call`,
`Model.spell_call`, `Model.strongstance_trigger`, `Model.stunningstrike_effect`,
`Model.table_call`, `Model.vcall`, `_mana_cost_method`. Partial implementations
and explicit refusals receive no credit. The four remaining registered entries
are listed above and in the manifest.

### Validation record for Shadow Step

The corpus contains 125,472 complete-state cases across all 16 FPU configurations,
including 5,664 appended cases. The preceding 119,808 rows are checked and reused
byte for byte. Coverage includes null owners, signed levels/fires/facing,
allocation failures, ring height boundaries, duplicate/filtering queries,
negative sample counts, nonfinite patterns, callback mutation/failure prefixes,
adjusted and named/external dispatch, offline/network runs and full lifecycles.
Host checks additionally cover malformed screen-clamp results, oversized queries,
missing services, zero sample counts, callback self-removal and 64-bit flags.

An older host check expected Shadow Step dispatch to refuse as unported; it now
checks that dispatch reaches the required clamp callback. The first exact run
rejected a 64-bit fixture literal in the input runner's raw32 format; the same
synthetic value is now supplied by two 32-bit pokes. A signed-comparison compiler
warning was corrected. Final command results are recorded below.

Only the existing header, source, tests, generator, TSV, manifest and this page
change. Private acceptance measurements are skipped because the instructed inputs
are unavailable; no alternative locations were searched. Windows, physical Intel
hardware, original-console execution and playable integration remain unverified.

Both arm64 and x86_64 executables match all 125,472 expected complete states
(x64 through Rosetta); trace SHA-256
`6910ae088366bdf3bb8e52713a075c3933688e880c76f6799eeddd62c9067694`.
The independent audit parses the pinned AST, checks fixture/generator hashes and
byte-compares the complete preceding prefix before comparing all 5,664 new native
states. Registered-body accounting is 188 supported / 4 remaining.

| Exact command | Final result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/shadow-before.tsv.gz > work/shadow-generation.log 2>&1` | PASS; 125,472 cases, original prefix unchanged |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/shadow-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/shadow-build.log 2>&1` | PASS; no warnings |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `python3 work/check-shadow.py > work/shadow-audit.log 2>&1` | PASS; AST inventory, hashes, prefix and every new complete state |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/shadow-tests-final.log 2>&1` | PASS; 3/3, 30.91 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/shadow-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/shadow-x64-build.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/shadow-x64-tests.log 2>&1` | PASS; 3/3, 59.65 seconds |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; Mach-O arm64 and x86_64 |
| `ctest --test-dir build/native --output-on-failure -j 3 > work/shadow-full-ctest.log 2>&1` | PASS; 103/103, 369.39 seconds; no failures or timeouts |
| `git -C ../../champions-return-to-arms-re diff --exit-code` and `git -C ../../champions-return-to-arms-re status --porcelain` | PASS; research unchanged and clean |
| `git diff --check` | PASS |

No functional code, generator or fixture changed after the successful final builds
and tests. Only header comments and this validation record were refined afterward.
The seven scoped files are committed locally on `codex/port-skills-b`; nothing is
pushed, and no issue or pull request is created. Passive effects and the explicitly
listed model plumbing remain portable from the existing evidence.


## Passive effects family (59/62/64)

Ports the complete remaining registered passive family: `SkillLifeTap_cast`,
`SkillStrongStance_cast`, `SkillStrongStance_trigger` and
`SkillStunningStrike_spawnEffect`. The cast aliases return the documented base
zero without reading memory or emitting effects. Evidence is the pinned Python
passive section, specification sections 59/62/64 and their executed correction
notes. This adds two source units; aliases are not additional AST definitions.

Strong Stance captures the signed sample count before light allocation, then
emits exactly 25 particles. Each uses one third-generator draw for its sample
index and cosmetic rejection draws for a normalized direction at speed 10.
The model pointer, position and facing are live reads on each sample callback.
Light and phantom construction each account for their delegated shared draw only
when allocation succeeds. The phantom receives both supplied values unchanged.
Stunning Strike filters null/noncreatures before any service, then retains sound,
light, shared duration draw and adjusted target slot-30 order. Callback mutation
and exception prefixes survive; no extra creature vote is invented after callbacks.

LightEffect, model sampling, sound/resources, particles, StrongStancePhantom and
Creature stun internals stay explicit services. A zero sample count refuses after
the reached third-generator draw, preserving the existing sample-domain boundary;
the console zero-divisor index is undocumented. Missing/malformed services refuse.
No Session, scene, original player dispatch or Norrath compatibility is established.

Remaining source units at the end of the passive continuation (13): `Model.__init__`,
`Model._stat_via_slot14`, `Model._virtual`, `Model.call`, `Model.cstr`,
`Model.f32`, `Model.half_toward_zero`, `Model.is_player`, `Model.skill_call`,
`Model.spell_call`, `Model.table_call`, `Model.vcall`, `_mana_cost_method`.
The manifest is the authoritative current inventory; earlier continuation records
above retain their historical counts. Model plumbing is portable from existing
public evidence and is the next family. No registered function remains unsupported.

### Validation record for passive effects

The corpus contains 128,528 complete-state cases across all 16 FPU configurations,
including 3,056 appended cases. Every preceding row is reused only after matching
its complete input and verifying the preceding manifest hash. New cases cover
positive/negative sample counts, independent RNG seeds, allocation outcomes,
nonfinite positions, signed facing, target filtering, adjusted/named/external
stun dispatch, exact callback mutation/failure prefixes, alias dispatch and repeated
effects. Every comparison includes returns/errors, draw counts, sparse bytes,
ordered writes and callback arguments. Host checks additionally cover zero/malformed
samples, missing services, phantom failure after 25 particles, callback self-removal
and the stun-duration range.

Only the existing header, source, tests, generator, compressed TSV, manifest and
this page change. Shared build files, roadmap and CI are untouched. Private
acceptance measurements are skipped because the instructed inputs are unavailable;
no other locations are searched. Windows, physical Intel hardware, original-console
execution and playable integration remain unverified.

Initial compilation found a duplicate host-test variable name; it was renamed.
No preceding native implementation or vector was changed. The final Release build
has no warnings. An independent audit of the pinned AST confirms the 226/13
source partition and 192/0 registered partition, verifies every evidence pin and
both generator/vector hashes, and byte-compares the complete preceding prefix.
Expected-state trace SHA-256 is
`82e8b20113e39ef040b0b310dbc952ef4786934d141fbd2c6cde7d5a410b1da2`.

| Exact command | Final result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/passives-before.tsv.gz > work/passives-generation-final.log 2>&1` | PASS; 128,528 cases, preceding prefix unchanged |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/passives-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/passives-build-final.log 2>&1` | PASS; no warnings |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/passives-tests.log 2>&1` | PASS; 3/3, 31.67 seconds |
| `python3 work/audit-passives.py > work/passives-audit.log 2>&1` | PASS; AST, evidence/hash, preceding-prefix and research-cleanliness audit |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/passives-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/passives-x64-build.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/passives-x64-tests.log 2>&1` | PASS; 3/3, 54.52 seconds |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; Mach-O arm64 and x86_64 |
| `git -C ../../champions-return-to-arms-re diff --exit-code` and `git -C ../../champions-return-to-arms-re status --porcelain` | PASS; research unchanged and clean |
| `git diff --check` | PASS |

Both arm64 and x86_64 match all 128,528 complete expected states, hence agree on
the trace hash above. x64 execution uses Rosetta. No functional code, generator
or fixture changes follow the successful builds and affected tests.

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/passives-full-ctest.log 2>&1` | PASS; 103/103, 382.53 seconds; no failures or timeouts |

The seven scoped files are committed locally on `codex/port-skills-b`; nothing is
pushed and no issue or pull request is created. The current table and manifest
support continuation with the 13 model-plumbing source units above.


## Virtual dispatch and callback routing family

Ports all six remaining dispatch source units: `Model.call`, `Model._virtual`,
`Model.skill_call`, `Model.spell_call`, `Model.vcall` and `Model.table_call`.
Evidence is the pinned B-reference plumbing section and the pinned
`ref_creature.py` inherited `table_call`. No engine behavior is inferred.

- One shared `virtual_call` reads signed16 adjustment and body address, wraps
  `this` to raw32, selects the class body inventory and honors `external` before
  invoking a body. Existing registered bodies retain their original implementation.
- Address names apply only to callback routing; an empty name falls back to the
  virtual name. Skill slot 2 snapshots its third argument as a three-pattern
  vector; Spell slot 2 snapshots its first argument. Other arguments keep order.
- `table_call` ignores the supplied table offset for Skill/Spell. Other classes,
  including Creature, use the supplied offset and signed adjustment and always
  delegate to the class/slot callback. They do not consult body names or inventories.
- `call` executes `handle_link`, `handle_clear`, and `handle_removeNode` internally,
  ignoring callback overrides and emitting no service history for these helpers.
  Other names retain ordered arguments, mutation/failure prefixes and copied
  callable lifetime. Scalar and typed callback ABIs remain available; typed-only
  callbacks can return the existing structured result types through `call`.

The original explicit host bounds remain: recursion/history/memory limits and
stack-token refusals, Skill/Spell slots 0–15, Creature/generic slots 0–64.
`virtual_call` exposes the three documented class/body-inventory combinations.
Missing services and malformed raw32 virtual returns refuse after reached state.
Delegated behavior is not counted as an implemented engine body. No Session,
scene integration or Norrath behavior is established.

Remaining source units at the end of the routing continuation (7): `Model.__init__`,
`Model._stat_via_slot14`, `Model.cstr`, `Model.f32`,
`Model.half_toward_zero`, `Model.is_player`, `_mana_cost_method`.
These are portable from the pinned public evidence. The AST inventory still has
239 portable units (241 directly declared definitions minus two harness helpers).
There are 232 implemented/tested units and 192/0 supported/remaining registered
entries. The earlier continuation sections retain their historical inventories.

### Validation record for routing

The corpus contains 159,728 complete-state cases across all 16 FPU configurations,
including 31,200 appended routing cases. All 128,528 preceding rows remain byte
for byte. Coverage includes every routing entry point, all three class inventories,
signed adjustments, explicit tables, named/empty/forced callbacks, vector snapshots,
callback mutation/failure prefixes, generic callback-only tables, internal handle
routing and scalar calls. Host checks cover malformed callback results, missing
services, recursion recovery, callback self-removal, structured returns and limits.
Every vector compares returns/errors, draw counts, sparse bytes, ordered writes
and callback arguments.

Only the existing header, source, host/vector tests, generator, compressed TSV,
manifest and this page change (seven files). No shared build file or research file
is changed. Private acceptance is skipped because the instructed inputs are
unavailable; no other location is searched. Windows, physical Intel hardware,
original-console execution and playable integration remain unverified. x64
verification uses Rosetta. Exact final commands and results follow.

The independent audit confirms the 232/7 source partition, every evidence pin,
fixture/generator hashes and the complete unchanged preceding prefix. Both arm64
and x86_64 match all 159,728 expected complete states; expected-state trace SHA-256
is `0e1542196dd9dac4c00808c70a3242f2e3e8d3f433436d21fed5454d182fe358`.

Initial compilation found an object-body inventory still needed by the existing
Bash callback path; it was restored. Initial generation caught a synthetic table
case incorrectly assuming Creature uses the Skill/Spell offset override; the
fixture setup was corrected. Neither issue changed a preceding contract body or
vector. Final Release builds have no warnings.

| Exact command | Final result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/routing-before.tsv.gz > work/routing-generation.log 2>&1` | PASS; 159,728 cases, preceding prefix unchanged |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/routing-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/routing-build-final.log 2>&1` | PASS; no warnings |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/routing-tests.log 2>&1` | PASS; 3/3, 25.21 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/routing-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/routing-x64-build-final.log 2>&1` | PASS; no warnings |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/routing-x64-tests.log 2>&1` | PASS; 3/3, 41.80 seconds |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; Mach-O arm64 and x86_64 |
| `python3 work/audit-routing.py > work/routing-audit.log 2>&1` | PASS; independent AST, pins/hashes, full preceding-prefix and clean research |
| `git diff --check` | PASS |

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/routing-full-ctest.log 2>&1` | PASS; 103/103, 289.88 seconds; no failures or timeouts |

No functional code, generator or fixture changed after the successful final builds
and test runs. The seven scoped files are committed locally on `codex/port-skills-b`;
nothing is pushed and no issue or pull request is created. The seven
setup/scalar/factory source units above were left for the following continuation.


## Model setup, scalar helpers and runtime mana factory family

Completes the seven source units listed above from the pinned authored Python
contract. `Model` adopts supplied sparse memory, scalar services and external
routing names by value, retaining supplied bytes/write history while starting its
own callback histories, draw count, address names, recursion and scratch fresh.
The native API owns these values; it does not borrow Python dictionary identities.
The default constructor supplies empty memory and services. Typed services may
still be installed through the existing API.

`f32` returns the exact stored raw32 pattern. `cstr` returns raw bytes in a native
string, stopping at zero or the signed limit (default 64); zero/negative limits do
not read even an invalid address. It never decodes text. Limits above 65,536 refuse
as an explicit host bound, and reached address/span overflow uses the existing
raw32 memory refusal. `is_player` reads bit 0x40 even for a zero object pointer.
`half_toward_zero` reinterprets signed32 and divides by two toward zero, including
INT32_MIN. `_stat_via_slot14` completes slot 14 before a live slot 10 read;
mutation, signed adjustments and failure prefixes are retained. Existing passive
helpers now reuse this implementation without changing their expected states.

The mana factory returns an independent callable capturing `(slot, n)` by value.
Slot 15 returns that virtual call's result; every other captured slot calls slot
14 and returns raw zero. Both paths forward level and `n` unchanged. Existing
per-ID mana methods now use this factory. It supplies no engine service defaults.

No portable source unit or registered body in this pinned B contract remains.
Root is owned by the A reference. Plain MissileWeapon, poisonGasSpell,
StrongStancePhantom, LightEffect, world/combat/resource services and all other
reference delegations remain explicit callbacks. Missing services and the
previous host-domain refusals retain their documented boundaries. Completion
here does not establish Session/scene integration or Norrath compatibility.

### Validation record for model setup

New synthetic cases directly exercise every helper and factory, including raw
nonfinite patterns, unaligned/high addresses, all returned string bytes, signed
limits, null/player flag toggles, signed extrema, callback mutation/failure
prefixes, live table replacement, internal statistic dispatch and arbitrary
factory slots. Host checks additionally verify adopted memory/history, initialized
services/external routing, independent models/scratch, factory capture lifetime,
all 255 nonzero byte values and string/address bounds.

Private acceptance measurements are skipped because the instructed inputs are
unavailable. No other location is searched. Windows, physical Intel hardware,
original-console execution and playable integration remain unverified.


The corpus now contains 174,352 complete-state cases across all 16 FPU
configurations, with 14,624 appended cases. The independent pinned-AST audit
confirms 239/239 portable source definitions and 192/0 supported/remaining
registered bodies, verifies every evidence and fixture/generator hash, and
byte-compares the full 159,728-row preceding prefix. Expected-state trace SHA-256:
`8cc3de69a6ac850569dce235ca4e17e69bb357bf8336047b03c2ce083d961362`.
Only the existing header, source, test runner/host checks, generator, compressed
TSV, manifest and this page change. Shared build files and research are untouched.

Both arm64 and x86_64 match all 174,352 complete expected states, hence agree on
the trace hash above. x64 execution uses Rosetta. Release builds have no warnings.

| Exact command | Result |
|---|---|
| `python3 tools/generate_skills_b_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/skills_b.tsv.gz --reuse-prefix work/setup-before.tsv.gz > work/setup-generation.log 2>&1` | PASS; 174,352 cases; preceding prefix unchanged |
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release > work/setup-configure.log 2>&1` | PASS |
| `cmake --build build/native -j 4 > work/setup-build.log 2>&1` | PASS |
| `build/native/champions-skills-b-tests` | PASS; host checks |
| `ctest --test-dir build/native -R '^skills-b-' --output-on-failure > work/setup-tests.log 2>&1` | PASS; 3/3, 38.44 seconds |
| `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 > work/setup-x64-configure.log 2>&1` | PASS |
| `cmake --build build/native-x64 --target champions-skills-b-tests -j 4 > work/setup-x64-build.log 2>&1` | PASS |
| `ctest --test-dir build/native-x64 -R '^skills-b-' --output-on-failure > work/setup-x64-tests.log 2>&1` | PASS; 3/3, 67.81 seconds |
| `file build/native/champions-skills-b-tests build/native-x64/champions-skills-b-tests` | PASS; Mach-O arm64/x86_64 |
| `python3 work/audit-setup.py > work/setup-audit.log 2>&1` | PASS; AST inventory, evidence/hash/prefix audit, research clean |
| `git diff --check` | PASS |

| Exact final full-suite command | Result |
|---|---|
| `ctest --test-dir build/native --output-on-failure -j 3 > work/setup-full-ctest.log 2>&1` | PASS; 103/103, 356.83 seconds; no failures or timeouts |

No functional code, generator or fixture changed after the successful final
builds and test runs. The seven scoped files are committed locally on
`codex/port-skills-b`; nothing is pushed and no issue or pull request is created.
This completes the pinned B-reference portable contract under the documented
host bounds and explicit callback boundaries. Future work belongs to consumer
integration or separate reference contracts, with independent evidence.
