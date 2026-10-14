# Return to Arms monster contract subset

Family in this continuation: all five ordinary monster run loops, with inherited
Creature damage, lifecycle, activity, animation, following, target selection and
fleeing bodies.

PROGRESS: 33 of 34 reference functions ported

`champions-monsters-1` is an independent portable C++17 module. It uses borrowed
raw memory and the existing integer-pattern arithmetic, with no Godot dependency.
This is a bounded behavioral port, not a playable monster or a Norrath rule.

## Evidence and scope

The immutable research revision is
[`356add27d50e50972b2a671b31e8f4ec93e6778c`](https://github.com/Open-Champions/champions-return-to-arms-re/tree/356add27d50e50972b2a671b31e8f4ec93e6778c).
The authored translation follows its
[`tools/ref_monsters_1.py`](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_monsters_1.py)
and [specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/monsters-1.md).
These describe SLUS_209.73, US v2.00. The research harness compares instruction
interpreter results to Python; that harness was inspected and pinned here, but
was not executed locally against original inputs. This port tests synthetic
Python parity, and does not certify console behavior.

All files below are read from that exact commit with `git show` during fixture
regeneration, checked by SHA-256, and executed only from an isolated scratch
folder. Live working-tree Python is never imported. Research remains read-only.

| File | SHA-256 |
|---|---|
| `tools/ref_monsters_1.py` | `33a76a127c66ffd0b4c8c91c014d250c91c567cd168c2bc8870d5abae647c4d0` |
| `docs/spec/monsters-1.md` | `4c70c678d0f2d0060f330054b157778e2b3535fed56e9fca456ce2bedbe76348` |
| `tools/difftest_monsters_1.py` | `b68ce2a06c46e3919d8859fbf0c415059f6790fe0dc81e24b50231336447849e` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `tools/ref_creature.py` | `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7` |
| `tools/ref_arena_monsters.py` | `84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16` |

The research's MIT license and copyright (2026 TrustlessMike and contributors)
apply; the repository's MIT notice accompanies this translation.

| Implemented entry | Address | Scope |
|---|---|---|
| SmallSpider run | `0x2ab5d8` | Ordered inherited tick, inactive gate, destination movement, idle clock and corpse update |
| Gnaw run | `0x267654` | Repeated virtual model reads, all state decisions, arc hit, fleeing and deletion |
| Mosquito run | `0x297b30` | All states, animation guards, zero movement, edge hit and reaction/corpse movement |
| Scorpion run | `0x2a5068` | All states, clock transition, claw/sting helpers and flank movement |
| Badger run | `0x244380` | Chase/attack helpers, cone decision, all states and death event object flag |
| SmallSpider constructor | `0x2ab450` | Base initialization, facing, cosmetic-phase callback and 60-frame scheduling |
| Gnaw constructor | `0x26741c` | Software-double scale, own draw, direct idle state and every-frame scheduling |
| Mosquito constructor | `0x297788` | Supplied prefix/classification test, direct idle state and six event records |
| Scorpion constructor | `0x2a4d68` | Two own draws, software-double scale/radius, skin and shared event records |
| Badger constructor | `0x244018` | Signed-byte variant rules, raw32 definition selection and seven event records |
| SmallSpider set-state | `0x2ab704` | All requested states, filter refusals, idle timer, wander, ignored reactions and death |
| SmallSpider touch | `0x2ab8e0` | Player flag test, virtual death transition and base touch bit 59, including null toucher base read |
| Mosquito lose-part | `0x2982cc` | Exact part mapping, repeated/invalid parts, two mask calls and optional chunk |
| Scorpion sever-part | `0x2a59f0` | Low-five-bit shift, two mask calls and optional chunk |
| Gnaw set-state | `0x267a0c` | All states, cooldown, synchronized clips and six-effect death with allocation-gated own draws |
| Mosquito set-state | `0x297f54` | All states, reaction/death shedding, dodge and final water-test flag |
| Scorpion set-state | `0x2a55ec` | All states, claw/sting selection, reaction guards, death shedding and flank clips |
| Badger set-state | `0x244aa4` | All states, charm walk, chase suppression, attack latch, reaction guard and death |
| Scorpion attack | `0x2a529c` | Claw/sting hits, signed poison division and damage write-back, typed descriptor, turning and cooldown |
| Badger attack | `0x2447c4` | Inherited settling, damage flags, hit latch, turning guards and chained transitions |
| Badger chase | (run helper) | Complete first-match target/cone selection and inherited target-rim/path navigation |
| Badger pick | (run helper) | One own draw: modulo-five left/right/front selection |
| Target sight / death update | (shared helpers) | Signed16 facing/arc, target-handle lookup and explicit dead-update callback |
| Prefix differs | `0x159c20` | Signed byte indexing and supplied classification table |

SmallSpider support includes the documented base state filter, path clearing,
home-distance/wander selection, own RNG, vector-copy stores, base slot 27 and
base slot 28 death hook. The latter preserves death-script selection, optional
named-item callbacks, hidden deletion, PvP kill callback and boss-music callback.
It does not replace them with engine behavior. Virtual `this` adjustment is
signed16 and wraps to raw32. SmallSpider's own set-state and the base state/death
bodies dispatch internally at their documented pointers. The other four monsters'
slot-27 set-state pointers dispatch internally. The new run-family dispatch also
recognizes ordinary run and inherited lifecycle/target bodies and explicit virtual
callback overrides, with signed16 `this` adjustment. Arena creature and skill-cast
bodies still refuse explicitly; full `vcall` remains unported. Earlier bounded
entry points retain their previous dispatch domain and refusal behavior so all
committed vectors remain compatible. Unrecognized subclass bodies are callbacks.

The completed state-transition family additionally calls `anim_startBlendInOut`,
`anim_syncToSimFrame`, `Creature_dropLoot`, `GutChunk_ctor`,
`PhProp_initPhysics` and `fx_spawn`. All remain host callbacks. Gnaw recomputes
its effect position on each iteration and draws a chunk variant only after a
successful allocation. Mosquito writes its water-test flag after the final state
store; exceptions preserve the earlier prefix. Scorpion preserves the variable
claw/sting draw count. Badger retains current-state animation suppression and
writes its attack latch after the swing callback. The run bodies described above are tested separately from these transitions.

All five constructors now execute the documented `Creature_ctor` memory body from
pinned `ref_arena_monsters.py`, including ordered property queries, repeated reads,
serial wrapping, home vectors and balancing. `GameObject_ctor`, `Creature_setStats`,
`Creature_init`, `balance_lookup`, `obj_getPropInt`, `obj_moveOffGrid`, animation
clear/push/randomization and run-list registration remain explicit host services.
The host's GameObject callback supplies the definition pointer, initial position
and own seed. Balancing uses `Model::Balance` (found, HP, armour, damage, attack,
XP) through `balance_lookup`; the five factors default to ONE only when not found.
Missing typed balancing refuses under the normal policy. Callback replacement and
exceptions retain their prefix exactly. Constructor scheduling calls do not execute
monster ticks. Shared definition event records are written mask first, then time,
with the record pointer reread for each of four slots. The two Gnaw constants are
caller-supplied raw64 values, and scale calculations use the existing software
double arithmetic; Scorpion's 0.75 immediate is documented in the reference.

The attack-resolution family now preserves Scorpion's poison difficulty rule:
signed16 minimum/maximum damage is divided toward zero by four or eight and
written back before constructing the unsigned16 descriptor slots. Claw attacks
pass a zero descriptor. Animation flags are reread after hit/turn callbacks;
the cooldown decrements only on the documented branches, while a nonpositive
timer can still select a flank with a playing clip. Badger settles first, picks
variant/state-specific flags, and sets its hit latch only after the hit callback
returns. Its mutable state is reread between turning guards; a chain to state 6
can be followed by a transition to state 3 on the same tick.

Inherited `Creature_settle`, `Creature_moveBy`, water bookkeeping and turning use
the existing portable movement module, whose `ref_creature.py` hash is identical
to this pin. A minimal optional owner virtual-dispatch hook keeps derived touch
and empty-slot behavior in the monster owner. The bridge copies movement writes
into monster observations before each service/virtual dispatch and on success
or exception, without storing the bytes twice. Target sight and turning resolve
the handle node and signed16 virtual this adjustment. Collision, water, sound,
resource lookup, hit resolution and dead update remain explicit host callbacks.
The five ordinary run loops now execute the inherited lifecycle before their
state decisions. Badger chase executes its complete decision tree and inherited
navigation bodies.

The chase family preserves first-match priority: four target sight tests, three
supplied enemy-cone searches, the final all-direction sight test, then turning
or target-rim movement. Each side-facing test rereads the signed16 facing;
front/cone picks take exactly one own draw, and other decisions take none.
State dispatch retains the distinction between an internal body returning no
value and an external state callback's integer return.

Target movement computes the destination on the target's radius plus five,
then executes the inherited path and steering bodies locally: current/next
waypoints, up to sixteen advances, path exhaustion and pending index 16,
three movement rays, side-ray avoidance, turning, Drowsy/slow speed and the
existing collision step. Stuck recovery preserves the 30-tick side-step's own
draw, the 300-tick teleport threshold and the free-spot result's signed low
byte. Ray callbacks may mutate memory between rays; path callbacks write into
the supplied buffer before the pending index is stored. Partial failures retain
all earlier writes and calls. No navigation or path result is invented.

`creature_findEnemyInCone`, `world_raycast`, `path_requestToPoint`,
`world_findFreeSpot`, `SpellDrowsy_typeinfo`, `creature_findSpellByType` and
`SpellDrowsy_getStat3Fraction` are explicit host services. The free-spot callback
belongs in `structured_callbacks` and returns a tuple of (Vec3, integer found).
These are supplied navigation services; scene AI dispatch and Norrath behavior
remain separate work. The inherited lifecycle now implements its documented
shared draws, independently of those services.

The callback ABI continuation adds the public `call_value` entry and free
`shape_result` function. The existing integer-only internal entry forwards through
that same implementation. The ABI table is a handwritten projection of all 143
pinned `EXTERNALS` rows: only the argument-kind string and return kind participate
in shaping. The subsequent ordinary-run family uses that ABI while executing the
inherited Creature lifecycle (damage, status, activity, animation and target
selection) internally.

Result shaping preserves required/optional vector and word outputs, byte outputs,
object arrays and linked-list outputs, including consuming supplied optional
outputs when their pointer is false. Argument-position advancement exactly follows
`ipvelsSOWE`; required output markers do not advance it. Empty results become None,
one result collapses to a scalar or supplied container, and multiple results remain
a tuple. Floating returns select the supplied floating-point pattern rather than
the integer return. Text returns retain the `('buf', payload)` token, empty or
binary bytes, and even non-byte supplied payloads without inventing strings.
Unsigned64 heads in compound results remain unsigned instead of narrowing.

`call_value` wraps only top-level integer arguments to raw32; nested tuple/list
integers remain unchanged. `TaggedTuple` preserves element-record tags and
complete four-slot descriptors through the same call path; the existing Scorpion
single-effect descriptor remains compatible. It records the normalized call before invoking a copied
handler, preserving callback replacement, mutation and exception prefixes. A
complete-result handler takes precedence over a structured handler, which takes
precedence over an integer handler; hosts should register one handler per service.
Complete-result callbacks can also return unsigned64 compounds and arbitrary
buffer-token payloads. The existing
balancing callback also returns its complete six-value shape through this entry.
RequireHandlers remains the default; ReferenceZero alone enables the complete
pinned zero/None/container result shapes. An unknown service with an explicit
handler is callable, while a missing unknown ABI refuses. This is callback plumbing,
not an implementation of those 143 engine services or a tick/scene adapter.

## Host boundary and refusals

`Model` borrows `creature_hp::Memory`; that storage must outlive the model.
The caller supplies definitions, clip handles, character classification bytes,
positions, globals and service results. No game strings, tables or assets are
embedded. Integer callback arguments wrap to raw32, vector arguments preserve
Python tuple shape, and mask returns retain all 64 bits. Raw arithmetic uses the
caller's current FPU configuration. The own generator writes `+0x44`; inherited
runs also preserve the shared seed at `0x404464`, the third seed at `0x404468`,
and attached spell seeds at `+0x10`. The cosmetic seed remains untouched. Scorpion poison descriptors
use a typed `Descriptor` containing one four-word effect and three free slots.
Inherited movement services with list, tuple or buffer results use
`structured_callbacks`; integer-only handlers cannot fabricate those shapes.

Missing callbacks throw by default. `ReferenceZero` is an explicit opt-in to the
reference's zero return policy, useful for synthetic experiments. Callback
mutations, replacements and exceptions retain partial memory, calls and writes.
There is no rollback, fabricated allocation, animation, loot or terrain service.
Callbacks must not destroy the model during a call; arbitrary host state outside
memory is the caller's responsibility.

Creature entry addresses must accommodate the complete 0x3c8-byte base object.
Badger set-state and chase require a 0x3d0-byte span for its extension latch at +0x3c8.
Scorpion construction checks through its skin byte +0x3c8, and Badger construction
checks through its variant byte +0x3cc before invoking the base callback.
Callback pointer returns must fit raw32; oversized pointers explicitly refuse.
The memory implementation enforces raw32 spans and its existing byte/history
budgets; accesses outside that domain refuse. The model separately caps each
history at 65,536 entries and recursive monster transitions at 64. Prefix
scanning refuses after 4,096 nonterminal bytes. These are portable safety
boundaries, not original engine limits. An exception does not latch the model.

## Exact synthetic verification

The generator authors memory, callbacks and commands, then lets the pinned
Python methods compute expected observations. It never uses native output to
make expectations. Fixtures contain each initial snapshot and every command's
result, status, sparse memory, memory store log, model writes, ordered calls,
interleaved events, shared draw count and callback counters. Full-width masks,
RNG seeds and untouched supplied globals therefore participate in comparison.

The pre-chase corpus contained **98,000 cases**. Cases cover state/filter combinations, PvP gates, inert creatures, ignored
states, wander/follower paths and ten unsuccessful ground tests, virtual hooks,
repeated deaths, part bit mapping and repeated shedding, allocation failure,
mutating/throwing callbacks, exceptional raw arithmetic and signed prefix bytes.
The added 43,840 cases exercise all four state-transition bodies, distinct clip
handles, current-state/animation guards, charm timers, own-RNG branches, partial
allocation success, hidden deaths, slot-27 adjustments/overrides and mutating or
throwing services. All 19,376 legacy rows remain byte-for-byte unchanged.
The constructor family adds 15,120 cases covering all five bodies, own-RNG seeds,
null/matching/mismatching type names, signed facing and variants, serial overflow,
property rereads, balancing factors, shared event records, exceptional software
doubles and mutating/throwing service prefixes. All 63,216 earlier rows remain
byte-for-byte unchanged. All sixteen arithmetic configurations are exercised. Native execution receives
only the input columns; the wrapper independently compares every complete
observation and checks the fixture, generator and reference manifest hashes.
Normal builds require neither the research checkout nor game files. The attack family
adds 19,664 cases (1,229 recipes in every arithmetic configuration): signed
poison limits/difficulties, descriptor shapes, cooldown and own-draw branches,
Badger variants/states/events/latches, chained state transitions, signed sight
arguments, target absence, real inherited turning, water and collisions, base
and derived contacts, empty slot 11, and mutating/throwing services. All 78,336
earlier rows must remain byte-for-byte unchanged; the compatibility comparison
and local results are recorded below.

```sh
python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R monsters-1 --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

The chase continuation adds 30,688 rows (1,918 authored cases in each arithmetic
configuration): first-match decisions, own-pick residues, side-facing wrap,
virtual this adjustments/overrides and return values, target rim/arrival,
negative/current/pending/exhausted waypoint indexes, sixteen-advance bounds,
ray miss/NaN/side selection, Drowsy/slow speed, root motion and supplied water/
collision queries, signed clocks, stuck recovery, low-byte free-spot found flags,
and mutating/throwing service prefixes. All 98,000 earlier rows must remain
byte-for-byte unchanged within their original configurations.

The ABI continuation adds 50,608 rows (3,163 authored cases per arithmetic
configuration), bringing the corpus to 179,296 rows. Every pinned ABI row is
exercised with optional pointers false/true, absent/empty/partial/complete supplied
outputs, distinct integer/floating heads, negative heads and unsigned64 heads.
Direct calls cover every ABI default and registered callback, None, vectors,
lists, nested tuples, binary buffers, top-level wrapping, nested integers,
repeated calls, mutating callbacks and exception prefixes. Additional cases
check Python truth behavior for optional outputs (None, empty containers,
nonempty zero-valued tuples, buffer tokens and wide integers). All 128,688 earlier
rows must remain byte-for-byte unchanged within their original configurations.
The unit tests also exercise missing-service refusal, unknown ABI names,
callback self-removal, full-result callbacks and unsigned64 compound shapes.

The ordinary-run continuation appends opcodes 23–27 without changing any earlier
input or callback-counter shape. The corpus adds complete synthetic observations
for all state decisions, signed clocks and timers, root motion/slow/Drowsy,
activity/script transitions, damage materials, gibs, death rewards, boss/scavenger
state, following, target queries, fleeing and attached spells. Every service
reached by these fixtures also has a mutation case and a first-call exception
case. The transport now accepts virtual overrides for Creature slots 0–37 and
retains the original constraints for every legacy recipe. Symbolic stack handles
are represented as strings, preserving the reference's `handle_clear('stack')`
call without inventing a raw address.

Regression cases distinguish generic `table_call('Skill', ...)` during burning
from arena `skill_call(...)` in Tagar's Insects. A recognized arena cast address
in the burn table still calls the host, including signed16 this adjustment;
Tagar's recognized unported cast bodies still refuse. Repeated ticks exercise
timer expiry, persistent root motion, changing animation events and exceptional
raw arithmetic without resetting the borrowed state between commands.

## Local verification

### Previous state-transition continuation

On 2026-10-13, macOS arm64 with AppleClang 21 and a Release build:

- The regeneration command above passed and produced 63,216 cases.
- `python3 work/check_monsters_legacy.py` compared the old corpus preserved in
  ignored scratch with each configuration's regenerated legacy prefix: all
  19,376 rows were byte-for-byte unchanged.
- Both CMake configure and `cmake --build build/native -j 4` passed.
- `build/native/champions-monsters-1-tests` passed 39 checks.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure` passed
  both cases: 39 unit checks and 63,216 complete-state comparisons in 34.96 seconds.
- `ctest --test-dir build/native --output-on-failure -j 3` completed all 102
  tests in 2621.12 seconds: 99 passed, with three 600-second subprocess timeouts
  in Session object/combat/creature transport. Concurrent full suites in sibling
  worktrees were observed on the shared host. No assertion mismatch was reported.
- Each timeout-only test was then rerun individually and passed:
  `ctest --test-dir build/native -R '^session-objects-transport$' --output-on-failure`
  (128.55 seconds),
  `ctest --test-dir build/native -R '^session-combat-transport$' --output-on-failure`
  (69.99 seconds), and
  `ctest --test-dir build/native -R '^session-creature-transport$' --output-on-failure`
  (93.41 seconds). Every registered test has a passing local execution.
- `git diff --check` passed; the research checkout remained clean at the pin.

### Constructor continuation (2026-10-13)

macOS arm64, AppleClang 21, Release:

- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed: 78,336 cases, including 15,120 constructor cases.
- `python3 work/check_constructor_subset.py` passed: 945 complete constructor
  observations match pinned Python in arithmetic configuration 0.
- `python3 work/check_monsters_compatibility.py` passed: every one of the
  63,216 previous rows remains byte-for-byte unchanged in its configuration.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed.
- `build/native/champions-monsters-1-tests` passed 53 checks.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure` passed
  both tests in 49.31 seconds (78,336 exact complete-state comparisons).
- The constructor corpus observation trace SHA-256 is
  `19961e9b4a7104a9d48d309b2972c79ea23ca391e292cc92bc124fa3f053b60f`.
- `ctest --test-dir build/native --output-on-failure -j 3` passed all 102
  tests in 2122.33 seconds; monsters exact passed again in 56.53 seconds
  with the same trace hash after the final rebuild. No timeout or assertion
  failure occurred, and no
  individual rerun was required. Multiple sibling suites were active on the host.
- `git diff --check` passed and the research checkout remained unchanged.

### Attack-resolution continuation (2026-10-13)

macOS arm64, AppleClang 21, Release:

- The regeneration command above passed: 98,000 cases, including 19,664 new
  attack-resolution cases across all sixteen arithmetic configurations.
- `python3 work/check_attack_compatibility.py` passed: every one of the 78,336
  previous rows remains byte-for-byte unchanged within its configuration.
- An isolated configuration-0 replay (`work/attacks-subset.tsv`, authored with
  the existing generator) passed all 1,229 new complete observations.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed; the final rebuild passed too.
- `build/native/champions-monsters-1-tests` passed 64 checks.
- `ctest --test-dir build/native -R '^(creature-movement-host|creature-movement-exact|creature-movement-borrowed-host|creature-turn-host|creature-turn-exact)$' --output-on-failure`
  passed all five affected dependency tests in 1.66 seconds.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure` passed both
  tests in 44.56 seconds: 64 unit checks and 98,000 complete-state comparisons.
- The attack corpus observation trace SHA-256 is
  `04d974f16476ac5888a981f3aef135dcc8ad13c374a2bb6e9f096e4c2979bac0`.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed. The x86_64 unit executable also passed all 64 checks through Rosetta.
- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure` passed
  both tests in 81.54 seconds: 64 unit checks and 98,000 complete-state comparisons
  through Rosetta. Its observation SHA-256 equals the arm64 trace above exactly.
- `ctest --test-dir build/native --output-on-failure -j 3` passed all 102 tests
  in 1499.30 seconds. Monster exact passed again in 32.54 seconds with the same
  observation hash. No timeout or assertion failure occurred, and no individual
  rerun was required.
- `git diff --check` and the staged equivalent passed; the research checkout
  remained clean at the immutable pin. No original inputs were copied or tested.

### Chase/navigation continuation (2026-10-13)

macOS arm64, AppleClang 21, Release:

- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed: 128,688 cases, including 30,688 new chase/navigation observations.
- `python3 work/check_chase_subset.py` passed all 1,918 new configuration-0
  complete observations; its pinned AST inventory contains exactly 34 units.
- `python3 work/check_chase_compatibility.py` passed: every one of the 98,000
  previous rows remains byte-for-byte unchanged within its configuration.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed after correcting the initial missing
  math-header include. The final rebuild passed with no compiler warnings.
- `build/native/champions-monsters-1-tests` passed 82 checks, including missing
  services, callback replacement/failure, signed side-facing wrap, first-match
  priority, complete raw64 transition returns and malformed free-spot results.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure` passed both
  tests in 49.00 seconds: 82 checks and 128,688 complete-state comparisons.
- The arm64 observation trace SHA-256 is
  `cdd0fe33b7440ddd2c2a8d7be6bd100384a6e2e3621d459a540d0714005605cf`.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed.
- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure` passed
  both tests in 87.15 seconds under Rosetta: 82 checks and 128,688 complete
  comparisons. Its trace SHA-256 equals the arm64 hash above exactly.
- `ctest --test-dir build/native --output-on-failure -j 3` passed all 102 tests
  in 691.77 seconds. Monster exact passed again in 28.05 seconds with the same
  observation hash. No timeout or assertion failure occurred, and no individual
  rerun was required.
- `git diff --check` and the staged equivalent passed; the research checkout
  remained clean at the immutable pin. No private original inputs were used.

### Callback ABI/result-shaping continuation (2026-10-13)

macOS arm64, AppleClang 21, Release:

- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed: 179,296 cases, including 50,608 new ABI observations across all sixteen
  arithmetic configurations. The generator inventories 34 reference units and
  records the complete 28-name ported set.
- `python3 work/check_abi_compatibility.py` passed: every one of the 128,688
  previous rows remains byte-for-byte unchanged within its configuration.
- An isolated configuration-0 replay with
  `build/native/champions-monsters-1-trace work/abi-subset.tsv` matched all 3,163
  new observations from the pinned Python; the x86_64 replay matched arm64
  byte-for-byte with `cmp work/abi-actual.txt work/abi-x64-actual.txt`.
- A local comparison of the handwritten native signature table to the isolated
  pinned Python `EXTERNALS` passed for all 143 argument-kind/return-kind pairs.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed. An intermediate build rejected three
  signed literals passed to the result variant; those call sites now use explicit
  unsigned values or signed structured integers. The final build has no warnings.
- `build/native/champions-monsters-1-tests` passed 105 checks, including complete
  element-record and four-slot descriptor argument preservation.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure` passed both
  tests in 44.90 seconds: 103 unit checks and 179,296 complete observations.
- The arm64 observation trace SHA-256 is
  `524e8893441eaf84a3b5f97a49e0f9f1d85d365b0080493677d6eb58ff490a6c`.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed. `build/native-x64/champions-monsters-1-tests` passed all 105 checks.
- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure` passed
  both tests in 70.56 seconds under Rosetta: 103 checks and 179,296 complete
  comparisons. Its observation hash equals the arm64 hash above exactly.
- The resumed `ctest --test-dir build/native --output-on-failure -j 3` passed
  all 102 tests in 422.29 seconds. Monster exact passed again in 28.85 seconds
  with the same observation hash. No timeout or assertion failure occurred;
  no individual rerun was required.

This continuation resumed the uncommitted ABI family above rather than mixing
unfinished verification with a second family. The resumed Release configure and
`cmake --build build/native -j 4` passed. The compatibility command again passed
all 128,688 previous rows. `ctest --test-dir build/native -R monsters-1 --output-on-failure`
passed both tests in 26.91 seconds with the final **105** unit checks and 179,296
observations. The x86_64 Release configure/target build passed, and
`ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure` passed both
tests in 51.17 seconds with the same 105 checks, observations and trace hash.
An independent comparison again matched all 143 native ABI signatures to the
isolated pinned reference. The research HEAD is still the immutable pin and its
working-file diff is empty.

`python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output work/abi-regenerated.tsv.gz`
passed with 179,296 cases. Both
`cmp native/tests/vectors/monsters_1.tsv.gz work/abi-regenerated.tsv.gz` and
`cmp native/tests/vectors/monsters_1.json work/abi-regenerated.json` passed:
the entire committed candidate corpus and manifest reproduce byte-for-byte.
`git diff --check` passed. Standard Git status stalled reading the shared,
iCloud-evicted `.git/info/exclude`; a read-only download request also stalled.
Only this run's blocked inspection processes were stopped. No shared Git
configuration or ignore file was changed. Local staging/commit use
`update-index`, `write-tree`, `diff-tree`, `commit-tree` and an atomic `update-ref`
with the expected branch tip, checking that exactly the eight owned files differ.

Only local macOS arm64 and x86_64-through-Rosetta execution is established for
this continuation. Windows, console, physical Intel hardware, original-data
acceptance, Session/scene AI dispatch and Norrath remain unverified or unsupported.

The previous state-transition observation trace SHA-256 was
`bc9e3c3af10f717bd16c4f1247e88a3ddfc28d361c1ca6167d1e593ef6f37306`.
Only local macOS execution is established. Windows, console and physical Intel
hardware runs were not performed, and no private original monster-data acceptance
was measured. The x86_64 check runs locally through Rosetta.

### Ordinary run/lifecycle continuation (2026-10-13)

This run resumed and completed the existing uncommitted ordinary-run family.
The five monster ticks and their inherited bodies use the same immutable pin
listed above. No research file or shared build configuration was changed.

- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed with **212,080** cases, including **32,784** new run-family observations.
- `python3 work/monster-review/check_legacy.py` passed: all **179,296** committed
  rows remain byte-for-byte unchanged within all sixteen arithmetic configurations.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed with AppleClang 21 on macOS arm64.
- `build/native/champions-monsters-1-tests` passed **124** checks.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed. `build/native-x64/champions-monsters-1-tests` passed **124** checks
  through Rosetta.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure -V` passed
  both cases in **42.29 seconds**: 124 checks and 212,080 complete observations.
- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure -V` passed
  both cases through Rosetta in **70.55 seconds**, with the same checks and
  complete observations. Both observation traces have SHA-256
  `9ae63784426443b9cc98590fa702919e20fdce334c9873af27e7188a16b561db`.
- `ctest --test-dir build/native --output-on-failure -j 3` passed all **102**
  tests in **444.06 seconds**. Monster exact also passed within that full run.
  No failures or timeouts occurred; no isolated rerun was needed.
- `git diff --check` passed; the research checkout remained clean.

The uncompressed fixture SHA-256 is
`7b325db7b9a80d15c4bce51100d4eb5c4f547c30e9650f444aaa008756843f8a`.
Private acceptance was skipped because `../../private-validation` is unavailable;
no alternate locations were searched. Windows, physical Intel hardware, console
fidelity, original-data acceptance, scene integration and Norrath remain unverified.
Full virtual dispatch and its arena bodies remain portable follow-up work.

## Changed files

This continuation extends `native/include/champions/monsters_1.hpp`,
`native/src/monsters_1.cpp`, `native/tools/monsters_1_trace.cpp`,
`native/tests/monsters_1_tests.cpp`, `tools/generate_monsters_1_vectors.py`,
the existing vector/manifest pair and this page. The transport retains opcodes
0–5 and appends state-transition opcodes 6–9; the six added callback IDs follow
the original sixteen. Legacy inputs keep their original sixteen callback counters.
The constructor continuation appends opcodes 10–14 and callback IDs 22–32,
including structured balance return recipes. Both earlier recipe families keep
their original callback-counter lengths.

The attack continuation appends opcodes 15–19 and callback IDs 33–48, retaining
all earlier recipe and counter lengths (constructor recipes explicitly stop at
ID 32). Tagged returns describe structured collision lists/slide results and
resource buffers. The existing monster header/source, trace runner, unit tests,
generator, vector/manifest pair and this page are extended. The only dependency
changes are an optional virtual-dispatch hook in the existing movement header
and source, and one added movement link on the monster target in
`native/simulation_targets.cmake`. No research file was changed.

The chase continuation appends opcode 20 and callback IDs 49–55. All prior
recipes retain their original counter lengths, including attack recipes ending
at ID 48. Complete observations include the chase return value. The same eight
module/header/runner/tests/generator/vector/manifest/documentation files are
extended; no shared build configuration or research file is changed.

## Progress accounting and precisely what remains

An AST inventory of the pinned file contains 34 portable units: one top-level
function (`shape_result`) and 33 methods declared directly on `Model`. No test or
harness definitions occur in this file. Inherited methods, imported helpers,
address-table entries, lambdas and presentation functions absent from the file
are excluded. The 33 complete units covered by exact vectors are:

- `shape_result`, full `call` (public `call_value`), `anim_flags`, `start`,
  `swing`, `drop_loot`, `events`, `str_prefix_differs`,
  `_shed_chunk` (some have equivalent internal names or use equivalent internal loops in C++).
- `see`, `dead_update`, `_badger_pick`, `_badger_chase`, `_scorpion_attack`, `_badger_attack`.
- All five constructors and all five set-state methods.
- `smallspider_on_touch`, `mosquito_lose_part`, `scorpion_sever_part`.
- All five ordinary run loops (`smallspider_run`, `gnaw_run`, `mosquito_run`,
  `scorpion_run`, `badger_run`).

The remaining **one** unit is full `vcall`, portable from the pinned evidence.
The next coherent family is its complete inherited/arena dispatch chain. Ordinary
run dispatch now includes base spell updates, enemy/target/team queries, model and
sight accessors and ordinary run bodies. Full dispatch still needs the arena
Horsemen, Skelight and Zek run/state/damage bodies and the remaining base virtual
slots (including damage, path completion, stun and no-target), plus arena skill
cast dependencies recognized by non-creature table dispatch. Preserve earlier
bounded APIs and vectors when extending that domain; count `vcall` only after all
recognized bodies are implemented and tested, including body lookup by address
at unexpected slots. No refusal is counted as a complete virtual-dispatch port.

The run family translates `process_damage`, `run`, `update_active` and `step_anim`
internally, with all their ordinary inherited dependencies. It does not substitute
invented Creature lifecycle callbacks. Attached SummonSkeleton and Tagar's Insects
spell tick bodies are included because the inherited spell list recognizes them.
These inherited dependencies add no units to this pinned file's denominator.
Arena-class/skill-cast dispatch remains the explicit remaining unit's boundary;
it is not a new engine-service callback. Existing reference-delegated services
(effects, animation, damage application, world queries, paths, handles and scripts)
remain host callbacks. Malformed spans and bounded runaway iteration refuse.

The ABI continuation appends opcodes 21 (`shape_result`) and 22 (`call`) and
callback IDs 56–142 for the remaining external names. All earlier input shapes,
opcodes, callback IDs and counter lengths are retained. Its generic return
transport preserves signed values and unsigned64 heads without changing the
legacy integer callback transport. The same eight module/header/runner/tests/
generator/vector/manifest/documentation files are extended; shared build files
and the research repository are unchanged.

The inherited navigation methods are implementation dependencies and add no units
to the pinned file's denominator. The generator independently inventories its
AST and records the 34-unit count and the current 33-name ported set in the manifest.
Unsupported malformed addresses outside the raw32 memory domain remain refusals;
negative waypoint indexes within that domain preserve the reference's address
arithmetic rather than being silently clamped.

Badger's empty slot 11 is inherited rather than declared in the pinned file;
it now dispatches during inherited settling and is excluded from the denominator.
The five draw functions are outside the reference model. All external services require host implementations;
those callbacks are not extra functions in this progress denominator.
Vampire, Ogre, WarBoar, Gremlin and Ghoul have no implementation in this reference
batch. Session/World adapters, AI dispatch, original-data acceptance, playable
integration, console fidelity and Norrath support remain separate work.
