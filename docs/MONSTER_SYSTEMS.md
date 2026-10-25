# Return to Arms monster contract

Family in this continuation: complete virtual dispatch and inherited base slots
7 (path completion), 30 (stun) and 31 (no-target), including all existing owner
domains and complete callback return forwarding.

PROGRESS: 34 of 34 reference functions ported

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
| Complete virtual dispatch | (three inherited tables) | All derived bodies, base slots, overrides and complete callback returns |
| Base path completion | `0x11a260` | Answered index, point reset and failure count |
| Base stun / no-target | `0x118a58` / `0x118af8` | Ordered gates, timers, state/handle callbacks and flash stores |
| Skelight constructor | `0x2a8770` | Base initialization, embedded weapon holder, temporary level global, rise and shared event records |
| Skelight run | `0x2a8b84` | Deferred rising damage, ignored activity result, holder updates, all states and swing countdown |
| Skelight set-state | `0x2a91b8` | All states, filter redirects, one-draw swing, ignored reaction and sample-driven deletion |
| Skelight shatter | `0x2a946c` | Argument-independent eight-byte static clear |
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
callback overrides, with signed16 `this` adjustment. The Skelight entry domain
also recognizes both Skelight virtual bodies by address at any supported slot.
Horsemen and Zek now have complete dedicated entry domains.
Arena skill casts have a dedicated entry domain; earlier entry domains retain their refusals. Public `vcall` now selects all complete owner domains. Earlier bounded
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
Full virtual dispatch and the arena bodies not completed below remain portable
follow-up work.

### Skelight lifecycle continuation (2026-10-13)

The same immutable `ref_arena_monsters.py` contract above supplies the complete
Skelight constructor, run, set-state, shatter and holder initialization bodies.
The working research HEAD differs from the pin, but the two monster Python files
have the pinned hashes; generation still reads every reference with `git show`
at the immutable revision. The research working tree is unchanged.

`skelight_ctor`, `skelight_run`, `skelight_set_state`, `skelight_shatter` and
`skelight_vcall` are explicit native entry points. The bounded virtual entry
supports slots 0–37, signed16 this adjustment, address-based Skelight body lookup
at unexpected slots and explicit overrides. Its domain is restored on success
and exception, so older APIs and every previous fixture retain their behavior.
Unported recognized Horsemen/Zek/cast bodies and the remaining base slots
2/7/30/31 refuse; an unrecognized body remains the reference's host callback.
This is a complete Skelight family, not a claim that full `vcall` is complete.

The constructor clears the embedded holder's hand tables in order, delegates
Holder calls, stores the raw level, and temporarily replaces the monster-level
global only for statistics. Its 64-byte level-name comparisons fold ASCII case
exactly like Python bytes, using supplied memory only. Callback failure during
statistics retains the temporary global rather than rolling it back. Definition
event pointers are reread for each mask/time pair. No retail text is embedded.

Rising skips pending damage processing. Activity updates occur but never gate
Skelight's AI. Holder slot 15 selects the signed, wrapping item-model row; the
holder pointer is reread before slot 22. Hand-1 descriptors remain supplied
values. A target swing without a playing clip decrements its countdown; a
playing clip is reread after turning, while a missing target uses its own branch.
Death sets hidden/creature flags, clears the shatter static, emits effect 2 at
sample indexes 0,3,6,... and deletes without loot or the ordinary death hook.
Each sample rereads the model, facing and position. Runaway samples refuse after
4,096 iterations, retaining the prefix. The object span includes +0x460's level.

The appended opcodes are 28 (constructor), 29 (run), 30 (set-state), 31 (shatter)
and 32 (bounded Skelight virtual entry). Callback IDs and all earlier recipe
counter lengths are unchanged. The generator adds 2,036 authored recipes in
sixteen arithmetic configurations (32,576 new complete-state observations).
Progress remains 33 of 34: these methods are inherited from the arena reference,
and partial virtual dispatch is deliberately excluded from the ported-unit set.

macOS arm64 and x86_64 through Rosetta, AppleClang 21, Release:

- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed with **244,656** complete observations, including **32,576** new ones.
- `python3 work/skelight/check_legacy.py` passed: all **212,080** old rows are
  byte-for-byte unchanged in their original sixteen configurations. The declared
  inventory is still 34 units, with the same honest 33-name complete ported set.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed; the final rebuild passed too.
- `build/native/champions-monsters-1-tests` passed **149** checks. They include
  constructor/statistics exception prefixes, hand initialization, temporary-level
  restoration, scope restoration, unexpected-slot dispatch, signed this adjustment,
  explicit overrides, unported-body refusal, malformed samples and iteration bounds.
- The input-only replay `build/native/champions-monsters-1-trace work/skelight/subset.tsv`
  matched all **2,036** new configuration-0 observations. The x64 replay with
  `build/native-x64/champions-monsters-1-trace work/skelight/subset.tsv` also passed;
  `cmp work/skelight/actual.txt work/skelight/actual-x64.txt` found identical traces.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure -V` passed both
  cases in **38.21 seconds**, including 149 checks and 244,656 full observations.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed, including the final rebuild. `build/native-x64/champions-monsters-1-tests`
  passed all **149** checks.
- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure -V` passed
  both cases through Rosetta in **57.75 seconds**, with the same checks and observations.
  Both full trace hashes equal
  `07ade442d938d6a24f6c525a337786675fef4685f62f43bc2143d86de3467d77`.
- `ctest --test-dir build/native --output-on-failure -j 3` passed all **102**
  tests in **301.07 seconds**. Monster exact passed again within that full run.
  No final failures or timeouts occurred; no isolated rerun was needed.
- `git diff --check` passed. The research working tree remained clean, with its
  Python file hashes unchanged; no shared build files were modified.

The uncompressed fixture SHA-256 is
`4e4567dd4e03d673324b4c629488260e62778284f7fa7eae227d9c0727353f67`.
Private acceptance was skipped because `../../private-validation` is unavailable;
no alternate locations were searched. Windows, physical Intel hardware, console
fidelity, original-data acceptance, scene integration and Norrath remain unverified.


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
are excluded. All 34 complete units are covered by exact vectors:

- `vcall`, `shape_result`, full `call` (public `call_value`), `anim_flags`, `start`,
  `swing`, `drop_loot`, `events`, `str_prefix_differs`,
  `_shed_chunk` (some have equivalent internal names or use equivalent internal loops in C++).
- `see`, `dead_update`, `_badger_pick`, `_badger_chase`, `_scorpion_attack`, `_badger_attack`.
- All five constructors and all five set-state methods.
- `smallspider_on_touch`, `mosquito_lose_part`, `scorpion_sever_part`.
- All five ordinary run loops (`smallspider_run`, `gnaw_run`, `mosquito_run`,
  `scorpion_run`, `badger_run`).

No declared portable unit remains in the pinned `ref_monsters_1.py` contract.
Public `vcall` combines ordinary monsters, Skelight, Horsemen, Zek, arena skill
casts and inherited base virtual slots. Derived bodies resolve by address at
unexpected slots; base bodies resolve only at their documented slot/address pair.
Explicit overrides and unknown bodies remain supplied callbacks. Existing bounded
entry APIs retain their original refusals and vectors. Inherited dependencies
add no units to the 34-unit denominator; explicit refusals are not counted.

Further work is integration with Session/World and a controllable scene, supplying
the reference-delegated services, original-data acceptance and independent
platform/console validation. These are outside the completed Python contract;
the port supplies no engine service, assets, Norrath profile or playable scene.

Zek's `zek_ctor`, `zek_run`, `zek_take_damage`, `zek_set_state`, `zek_slot35`,
`_unit`, `_zek_specials`, `_zek_switch` and `_zek_wave` are complete in its
entry domain. These inherited methods add no units to the denominator.

Arena casts are complete in their dedicated entry domain: `summon_can_use`,
`summon_cast`, `tagar_cast`, `spell_base_ctor`, `summon_spell_ctor`,
`tagar_spell_ctor`, `_cosmetic_ball`, `_summon_sample` and cosmetic draws. Reuse
them from Horsemen/Zek; they are local bodies, not invented service callbacks.
The attached spell tick bodies were already included in the ordinary-run family.

Skelight's `skelight_ctor`, `skelight_run`, `skelight_set_state`,
`skelight_shatter`, default Weapon1 holder initialization and hand-1 descriptor
lookup are complete in the new dedicated entry domain. Reuse these bodies;
they add no declared units to the denominator.

The run family translates `process_damage`, `run`, `update_active` and `step_anim`
internally, with all their ordinary inherited dependencies. It does not substitute
invented Creature lifecycle callbacks. Attached SummonSkeleton and Tagar's Insects
spell tick bodies are included because the inherited spell list recognizes them.
These inherited dependencies add no units to this pinned file's denominator.
Unrestricted body dispatch and all base slots now execute in the complete owner domain. Existing reference-delegated services
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

### Arena cast continuation (2026-10-13)

The complete arena cast dependency family follows the same immutable
`ref_arena_monsters.py` pin: `summon_can_use`, `summon_cast`, `tagar_cast`,
`spell_base_ctor`, `summon_spell_ctor`, `tagar_spell_ctor`, cosmetic draws,
`_cosmetic_ball` and `_summon_sample`. These inherited methods add no units to
`ref_monsters_1.py`'s denominator. Full `vcall` remains incomplete and uncounted.

The public `arena_skill_call` selects the new bounded domain, resolves all three
recognized skill bodies by address at any supplied slot 0–14, honors explicit
`Skill.vN` overrides and signed16 this adjustment, and restores both domain flags
on success or exception. Direct cast entry points select this same domain. Prior
entry points preserve their earlier arena-cast refusal behavior. Holder services,
model sampling, terrain/free-spot queries, name lookup, allocation, resources,
sound, statistics, charm and handle linking remain explicit callbacks.

Summon preserves the four flash stores, 32 first-kind and 200 second-kind particles,
three cosmetic draws per ball attempt and one cosmetic sample-index draw per
particle. Negative sample counts use their absolute magnitude, including INT32_MIN.
The host supplies the cast's sample as a direct three-word vector; Skelight death's
existing sample-plus-optional-output shape stays unchanged. Ball rejection refuses
after 4,096 attempts, retaining the prefix. The height comparison interprets the
already-rounded difference as a double: exactly 24 does not trigger fallback,
infinities do, and NaN does not. Free-spot acceptance tests only the signed low byte.

The first shared draw occurs even if skeleton allocation fails. Elemental selection
uses another shared draw only when the signed chance comparison succeeds. The
existing full Skelight constructor executes for a successful skeleton allocation.
Player charm uses the current caster slot/team, signed positive level, and ordered
stores/callbacks. Sound precedes the second allocation. Each constructed spell
gets exactly one shared seed draw; Summon links its optional skeleton, while Tagar
initializes eight empty handles and computes its offset from the owner's current
position/facing. No implicit spell-list attachment or invented engine behavior is
added. Callback mutations and failures retain the complete earlier prefix.

Transport opcodes 33–39 append can-use, both casts, three constructors and skill
body dispatch. Numeric external slots 100–114 mean `Skill.v0`–`Skill.v14`; legacy
Creature slots and every callback ID/counter length are unchanged. There are 524
new authored recipes, each evaluated under all sixteen arithmetic configurations,
including allocation/chance/charm branches, exceptional arithmetic, signed facing,
negative sample counts, cosmetic retries, callback rereads/failures, late particle
failures, unexpected-slot body lookup and virtual overrides. The same eight owned
files are extended; research and shared build configuration are untouched.

Release validation on macOS arm64 and x86_64 through Rosetta, AppleClang 21:

- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed with **253,040** complete observations (**8,384** new).
- `python3 work/arena-casts/check_legacy.py` passed: all **244,656** committed
  rows remain byte-for-byte unchanged within their original sixteen configurations.
  The independent AST inventory confirms one top-level function and 33 Model
  methods; the same 33 complete names remain counted, with only `vcall` excluded.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed, including the final rebuild.
- `build/native/champions-monsters-1-tests` passed **164** checks. The additional
  checks cover missing services, invalid player slots, flash/handle failure prefixes,
  cosmetic-versus-shared draws, malformed samples, constructor span bounds,
  signed-this virtual body lookup/overrides and restored refusal domains.
- Input-only new-family comparison against pinned Python passed all **524**
  configuration-0 recipes. `build/native/champions-monsters-1-trace work/arena-casts/subset.tsv`
  and `build/native-x64/champions-monsters-1-trace work/arena-casts/subset.tsv`
  matched; `cmp work/arena-casts/actual.txt work/arena-casts/actual-x64.txt` passed.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure -V` passed both
  tests in **42.81 seconds**, including 164 checks and 253,040 exact observations.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed, including the final rebuild. `build/native-x64/champions-monsters-1-tests`
  passed all **164** checks. An intermediate unit run exposed a missing synthetic
  base-model table entry; the fixture was corrected before these final runs.
- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure -V` passed
  both tests through Rosetta in **69.61 seconds**, with identical observations.
  Both complete trace SHA-256 values are
  `2393d9f73ff3b7799d68b9e5b6b089ded875889f873ef37226b046e65819f090`.

The uncompressed fixture SHA-256 is
`7040acf79970531d21dba2af01f1ab21df01a993af0a885949591f595f9ded53`.
Private acceptance was skipped because `../../private-validation` is unavailable;
no other locations were searched. Windows, physical Intel hardware, console fidelity,
original-data acceptance, Session/scene integration and Norrath remain unverified.

`ctest --test-dir build/native --output-on-failure -j 3` passed all **102** tests
in **379.85 seconds**. Monster exact passed again within the full run in 45.46
seconds. No failures or timeouts occurred; no isolated rerun was required.
`git diff --check` passed, and the research working tree remained clean.
The arena cast continuation was followed by the Horsemen family below.
Next: Zek and the remaining base virtual slots/unrestricted dispatch. Existing
bounded entry domains and vectors must continue to retain their behavior.


### Horsemen continuation (2026-10-14)

The complete Horsemen dependency family follows pinned `ref_arena_monsters.py`
and `docs/spec/arena-monsters.md`: `horsemen_ctor`, `horsemen_run`,
`horsemen_set_state`, `_horsemen_cast`, `is_target_charging`,
`count_allies_near_target` and `is_blocked_by_target`. These inherited methods
add no declared units to `ref_monsters_1.py`'s 34-unit denominator. The AST
inventory still counts one top-level function and 33 directly declared Model
methods; only full `vcall` remains incomplete and uncounted.

Public `horsemen_vcall` selects a dedicated domain for slots 0–37, resolves
both Horsemen virtual bodies by address even at unexpected slots, preserves
signed16 this adjustment and explicit callback overrides, and restores all
three domain flags on success or exception. Direct Horsemen entry points select
the same domain. This domain also enables the existing Skelight lifecycle and
arena skill bodies, allowing summoner death and local Summon/Tagar casts without
fabricated callbacks. Older entry domains keep their previous refusals.

The constructor extends the existing Weapon1 initialization with the optional
final WeaponShield table and byte store, preserves both hand calls and all eleven
clip event updates, and delegates Nightmare construction and handle linking.
Mounted ticks copy position/facing before base lifecycle and reread the mount's
target after callbacks. On-foot ticks preserve holder/damage/run/activity/animation/
knockback/target-selection order; activity does not suppress the Horsemen switch.
Charging preserves the unwrapped bearing difference, unsigned interval test and
strict speed threshold. State changes preserve filter prefixes, mounted refusals,
nested default transitions followed by storing the requested state, two-hand
loot/drop order, pet death and the 48-unit dismount lift after handle clearing.

All four signed-variant cast branches use the existing skill dispatcher. Disease
Bolt and Fire Storm remain delegated Skill services in the pinned evidence;
Tagar attaches its result, while Summon discards its result. Cast clocks use the
own generator, and body-owned shared/cosmetic draws retain their existing owners.
Missing services, malformed spans and existing iteration limits refuse while
retaining complete earlier writes and calls. Horsemen objects require a complete
0x478-byte raw32 span. No new engine services, scene policy or Norrath rule is added.

Transport opcodes 40–44 append run, state, cast, virtual dispatch and constructor.
All old opcodes, callback IDs/counter lengths and recipe outputs are retained.
The existing header, source, trace runner, unit tests, generator, vector/manifest
pair and this page are extended. Shared build configuration and research files
are untouched.

This continuation was followed by the complete Zek family below; current
remaining work is base slots 7/30/31 and unrestricted virtual dispatch. Count
`vcall` only when all recognized bodies and owner domains are implemented and
exactly tested; no explicit refusal counts as ported.

Release validation on macOS arm64 and x86_64 through Rosetta, AppleClang 21:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed. The final build reported no warnings.
- `build/native/champions-monsters-1-tests` passed all **185** checks.
  New cases cover mounted/filter refusal order, reaction suppression, callback
  failure prefixes, post-callback dismount height, signed cast/virtual adjustments,
  overrides, restored domains, constructor spans/holder writes and own RNG use.
- `python3 work/horsemen/subset16.py` produced **21,984** exact observations:
  **1,374** new authored recipes under all sixteen arithmetic configurations.
  `build/native/champions-monsters-1-trace work/horsemen/subset16.tsv` matched
  every Python observation (`python3 work/horsemen/compare16.py`: zero mismatches).
  Recipes cover every state, mounted/on-foot ticks, filters, charging thresholds,
  ally/blocking decisions, cast variants/local allocation, exceptional arithmetic,
  successful mount linking, eleven constructor event clips, repeated ticks,
  signed body lookup, overrides and callback mutation/failure prefixes.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed. `build/native-x64/champions-monsters-1-tests` passed all **185** checks.
  `build/native-x64/champions-monsters-1-trace work/horsemen/subset16.tsv`
  also passed; `cmp work/horsemen/actual16.txt work/horsemen/actual16-x64.txt`
  found identical traces. Both new-family hashes equal
  `197dbc20f2ba085ddf04c9dd35f98559661826a4d74bcdb03270568d4eb24ca1`.

- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed with **275,024** complete observations (**21,984** new).
- `python3 work/horsemen/check_legacy.py` passed: all **253,040** committed
  observations are byte-for-byte unchanged within their original configurations.
  An independent comparison also confirms every pre-existing recipe function
  remains byte-for-byte unchanged. Recipe families are prepared once before the
  configuration loop; the preserved output proves compatibility of that reuse.
  The manifest retains the 34-unit AST denominator and 33 complete names.

- `ctest --test-dir build/native --output-on-failure -j 3` passed all **102**
  tests in **416.13 seconds**. Monster exact passed all **275,024** observations
  in **45.39 seconds**; monster unit passed all **185** checks. No failures or
  timeouts occurred, so no isolated rerun was needed.
- `git diff --check` passed. The research checkout stayed clean, and its pinned
  Python hashes are unchanged. All eight changed files belong to this module.

The uncompressed fixture SHA-256 is
`6d77d7817694e476b124efdb545f7efbc24c33ac2119fcd2e026fbc7391583eb`.

Private acceptance was skipped because `../../private-validation` is unavailable;
no other locations were searched. Windows, physical Intel hardware, console
fidelity, original-data acceptance, Session/scene integration and Norrath remain
unverified. The research working tree remains clean, with both reference Python
hashes unchanged. No shared build configuration was modified.


### Zek continuation (2026-10-14)

The complete Zek family follows pinned `ref_arena_monsters.py`, inherited
`ref_creature.py` and `docs/spec/arena-monsters.md` sections 5.1–5.6.
The independent AST inventory still counts one top-level function and 33 directly
specified Model methods in `ref_monsters_1.py`. All 33 previously complete units
remain counted; full `vcall` remains incomplete. Inherited Zek methods and base
damage do not inflate the 34-unit denominator.

Public `zek_vcall` selects slots 0–37 and dispatches all four Zek bodies by address,
even at unexpected slots, with signed16 this adjustment and explicit overrides.
It enables the existing Horsemen, Skelight and arena-cast bodies, and implements
base slot 2 locally. All four flags restore on success or exception; older entry
points retain their refusal domains. Direct Zek entry points select this domain.
Zek objects require a complete 0x480-byte raw32 span. The level body ignores this,
reads the supplied global level and multiplies by exact pattern `0x3e999999`.

The constructor initializes its embedded Weapon1 holder, five resistances and
wave fields, schedules the tick and writes all eleven event clips in reference
order. All attack states preserve first-match sight/cone priority, signed clocks,
last-attack exclusions, exact flags/descriptors and callback rereads. Ground pound
and wave launch retain allocation/link prefixes. Inactive Zek skips the holder,
animation, switch and wave tail after base lifecycle. Every completed switch path
executes the wave tail; a state callback's return does not escape that tail.

Damage sparks precede base damage even for inert or IgnoreDamage creatures.
Each of fifty sparks uses three own draws per unit-ball attempt and accumulates
velocity from the previous spark. Static particle setup writes its ready word
only after the service returns. Rejection has the existing 4,096-attempt safety
budget per particle. Base damage preserves inert and IgnoreDamage branches,
frame/flag stores, swapped-coordinate hit bearing, callback mutation, source
handle precedence and ordered HP/pending writes. No supplied attacker is guessed
or replaced by a scene object.

The wave writes Y before X, calls terrain height, then stores height plus one.
Its absolute height-difference comparison uses the already-rounded float promoted
to double: exactly twelve survives, infinities end the wave, NaN does not. Excessive
steps restore the complete old position. Target steering uses the reverse bearing,
signed16 offset, separately truncated step/damping and strict 36/800 distance
bounds. Explosion preserves ring/sound/damage/decal/light/handle order, consumes
one own decal draw and emits effect 7 on its final tick. Callback failures retain
all preceding writes and host mutations. Effects, damage application, animations,
resources, allocation, holders, paths, terrain and handles remain explicit host
services, including GroundPoundEffect and LightEffect construction.

Transport opcodes 45–51 append constructor, run, set-state, take-damage, level,
wave and Zek-domain dispatch. All existing opcodes and callback IDs/count lengths
are unchanged. New fixtures comprise 1,066 authored recipes under sixteen arithmetic
configurations (17,056 complete observations), including every state, filters,
priority decisions, sparks/retries, exceptional arithmetic, text comparisons,
activity gates, signed virtual adjustment/overrides, repeated ticks and host
mutation/failure prefixes. No research or shared build files were edited.

Next family: base slots 7/30/31 and the complete unrestricted `vcall` contract.
Preserve all existing bounded entry APIs/vectors; count `vcall` only when every
recognized body and complete callback return behavior is exactly tested.

Private acceptance was skipped because `../../private-validation` is unavailable;
no other locations were searched. Windows, physical Intel hardware, console
fidelity, original-data acceptance, Session/scene integration and Norrath remain
unverified.

Zek-family Release validation on macOS arm64 and x86_64 through Rosetta:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed after correcting a mixed signed/unsigned
  local declaration and a test callback's type/scope. Final builds have no warnings.
- `build/native/champions-monsters-1-tests` passed **211** checks. An intermediate
  assertion expected level 10 × `0x3e999999` to truncate to 3; pinned Python and
  both native platforms correctly produce 2. The assertion was corrected.
- `python3 work/zek/check.py` produced **17,056** pinned complete observations.
  `build/native/champions-monsters-1-trace work/zek/subset.tsv` matched every
  Python observation across all sixteen arithmetic configurations (zero mismatches).
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed. `build/native-x64/champions-monsters-1-tests` passed **211** checks.
  `build/native-x64/champions-monsters-1-trace work/zek/subset.tsv` passed;
  `cmp work/zek/actual.txt work/zek/actual-x64.txt` found identical traces.
  Both new-family SHA-256 values are
  `fd4def2582c52edeea7cdb527b546ff85882a2758a846b7740608bec66a447d8`.
- `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  passed with **292,080** observations (**17,056** new).
- `python3 work/zek/check_legacy.py` passed: all **275,024** prior input/expected
  rows remain byte-for-byte unchanged within their sixteen original configuration
  blocks. The manifest retains 34 total units and the same 33 complete names.
  Independent AST enumeration confirms one top-level function plus 33 class methods,
  with only `vcall` absent from the fully ported set.
- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure -V` passed
  both tests through Rosetta in **134.08 seconds**, including **292,080** complete
  observations. Full x64 trace SHA-256 is
  `16e4061a406251e7c5b0c24919967dec59b0c1f409cf66f484e810d364f98b88`.

The uncompressed fixture SHA-256 is
`eaba5270f2947e06f9ce3d03074857bb2c513faf2c30ae402a5bf310daede6da`.

- `ctest --test-dir build/native --output-on-failure -j 3` passed all **102**
  tests in **356.34 seconds**. Monster exact passed **292,080** observations in
  **51.66 seconds**; monster unit passed **211** checks. No failures or timeouts
  occurred, so no isolated rerun was required.
- `git diff --check` passed, the final research status stayed clean, and no shared
  build configuration changed. Only the same eight monster module files changed.


### Complete dispatch continuation (2026-10-14)

The final family follows pinned `ref_monsters_1.py::Model.vcall`, both inherited
dispatch tables and `ref_creature.py::slot7_path_done`, `slot30_stun` and
`slot31_no_target`. The AST inventory remains one top-level function and 33
methods declared directly on Model; `vcall` is now included, completing all 34.
Earlier continuation sections above describe their historical bounded domains.

Public `Model::vcall` returns the existing full `Result` type and temporarily
selects every implemented owner domain. It preserves signed16 this adjustment,
raw32 this wrapping, derived lookup by body, base lookup by slot/body pair,
explicit overrides and arbitrary supplied virtual callback slots. Top-level
integer results retain raw64 patterns; None, tuples, lists, buffers and compound
callback values remain distinct. Runs forward their final state callback value;
void bodies discard nested returns and numeric base bodies own their returns.
Reentrant calls and exceptions restore the previous result carrier and all
five domain flags. Existing bounded APIs are unchanged.

Path completion writes the answered index first, then clears failures on success
or resets both points and increments the wrapping failure counter. Stun calls
the supplied boss-rank service before the corpse test, stores the low16 timer,
dispatches state 19, then writes all seven flash words in reference order when
requested. No-target checks PvP before boss rank, stores its low16 timer, calls
handle clear, then writes zero over any callback mutation. Callback exceptions
retain every preceding store and call. Full-domain path turning uses the owner
dispatcher, including recognized bodies in unexpected slots.

Opcode 52 adds complete virtual dispatch to the existing transport. Legacy
virtual callback IDs 4/5 may additionally carry tagged returns when explicitly
encoded as lists; all existing scalar recipes, opcodes, callback IDs and counter
lengths remain unchanged. Only the same eight module-owned files are changed;
research and shared build configuration are untouched.

Final-family validation on macOS arm64 and x86_64 through Rosetta:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` passed, including the final rebuild, with no
  warnings. `build/native/champions-monsters-1-tests` passed **235** checks.
  Intermediate fixture runs exposed missing callback prerequisites and a retained
  stun timer; those authored fixtures were corrected before the final passes.
- `python3 work/full-dispatch/check.py` evaluated **1,484** authored recipes under
  all sixteen arithmetic configurations: **23,744** complete observations. These
  include every derived body and base slot, wrong-slot delegation, overrides,
  signed adjustment, timer gates/wrapping, flash/handle failure prefixes, None,
  signed integers, tuples/lists/buffers and full lifecycle/path/cast entry cases.
  `build/native/champions-monsters-1-trace work/full-dispatch/subset.tsv` and
  `python3 work/full-dispatch/compare.py` passed with **zero mismatches**.
- `python3 work/full-dispatch/inventory.py` passed the independent AST count:
  **one** top-level function plus **33** class methods. The complete ported set
  equals the declared inventory. Both inherited dispatch tables exactly match the
  generator's exhaustive derived-body and base-slot recipe inventories.
- `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`
  and `cmake --build build/native-x64 --target champions-monsters-1-trace champions-monsters-1-tests -j 4`
  passed. `build/native-x64/champions-monsters-1-tests` passed **235** checks.
  `build/native-x64/champions-monsters-1-trace work/full-dispatch/subset.tsv` and
  `cmp work/full-dispatch/actual.txt work/full-dispatch/actual-x64.txt` passed.
  Both final-family trace SHA-256 values equal
  `eeabbc9ab8ec1334cbfc788bcf765d62742cc2d9a80f4d028b7d75e6ed9405e4`.
- Full `python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz`
  was started, then **stopped before fixture writes** while redundantly replaying
  the old families. It is not claimed as a completed run. The final fixture uses
  the byte-exact committed legacy blocks plus the current generator's pinned
  final-family replay: `python3 work/full-dispatch/assemble.py` passed with
  **315,824** observations and updated manifest hashes/accounting.
  `python3 work/full-dispatch/check_legacy.py` passed: all **292,080** prior rows
  remain byte-for-byte unchanged within their original configuration blocks.
  The full generator retains the same complete recipe sequence for regeneration;
  final full-fixture native checks below cover the assembled result.

The final uncompressed fixture SHA-256 is
`f40a5d08777899184bcdacf9255d85e396c3be69a4bb092f1083e8431980e2e8`.

Private acceptance was skipped: `../../private-validation` is unavailable, and
no other locations were searched. Windows, physical Intel hardware, console
fidelity, original-data acceptance, Session/scene integration and Norrath remain
unverified. Contract completion does not establish a playable port.


- `ctest --test-dir build/native-x64 -R monsters-1 --output-on-failure -V`
  passed both tests through Rosetta in **117.30 seconds**, including **235**
  checks and all **315,824** exact observations. The complete x64 trace SHA-256 is
  `6689e7b2be454a6012018da85e4c29afe9256cef57b15c2c70a1eb20e80c6f94`.
- All **17** pre-existing generator helper/recipe function bodies remain
  byte-for-byte unchanged (independent AST/source comparison against HEAD).

- `ctest --test-dir build/native --output-on-failure -j 3` passed all **102**
  tests in **365.04 seconds**. Monster exact passed all **315,824** observations
  in **66.84 seconds**, and monster unit passed all **235** checks. No failures
  or timeouts occurred; no isolated rerun was required.
- `git diff --check` passed. The final research working tree remained clean.
  All changes are confined to the eight existing monster module files; no shared
  build configuration, research file or private input was changed.
