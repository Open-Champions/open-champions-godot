# Return to Arms arena monsters: Zek construction, state entry and ground-wave lifecycle

PROGRESS: 63 of 78 reference functions ported

`champions-arena-monsters` is a handwritten portable C++17 module in
`champions::simulation::rta::arena_monsters`, independent of Godot. It implements
Skelight's non-wander set-state body, shatter, the complete base state filter,
and the arena reference's creature base lifecycle and AI-service family, plus
the complete ArenaScript level-progression family and scaled-time scheduling,
and the complete Tagar’s Insects and Summon Skeleton cast and spell lifecycles,
including the Skelight constructor required by summoning, and the complete shared
holder/data and signed attack-scaling helper family, plus Zek construction,
state entry, special-attack selection, slot35 and the complete ground-wave lifecycle.
It does not run an arena, monster AI, or a playable game.

## Pinned evidence

Research checkout: `champions-return-to-arms-re`, commit
`356add27d50e50972b2a671b31e8f4ec93e6778c`. Contract revision: Return to Arms
SLUS_209.73 US v2.00. These exact files at that commit are the evidence:

| File | SHA-256 |
|---|---|
| `tools/ref_arena_monsters.py` | `84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16` |
| `docs/spec/arena-monsters.md` | `d9a1a7b81b79c293b64ce5911112456ddd177df15fb8d0ffa0c09230887cd87b` |
| `tools/ref_creature.py` | `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `tools/difftest_arena_monsters.py` | `d342f5ca8b2fa7edde61c05350481900fa758c80d8a4df300da63ae8804dc911` |

Arena specification section 4.4 describes `Skelight_setState` 0x2a91b8 and
`Skelight_shatter` 0x2a946c. The base reference supplies `filter_state` 0x10a9c0
and `obj_rand` 0x1509d0. Section 13 reports instruction-interpreter differential
coverage, including 11/11 edges and 176/176 blocks for set-state/shatter. This
is research evidence, not a newly executed retail comparison or console result.
The native tests use only authored synthetic memory. No research source,
decompiler output, game bytes or assets are copied into the repository.

## Files

- `native/include/champions/arena_monsters.hpp`, `native/src/arena_monsters.cpp`: public API and authored bodies.
- `native/tests/arena_monsters_tests.cpp`: input runner, complete state traces and host refusals.
- `native/tests/vectors/arena_monsters.tsv`, `arena_monsters.json`: synthetic reference output and provenance.
- `tools/generate_arena_monsters_vectors.py`: pinned Python oracle generator.
- `tools/test_arena_monsters_vectors.py`: fixture-integrity check and native test invocation.
- `docs/ARENA_MONSTERS.md`: this scope/evidence page.
- Minimal additive registration in `native/CMakeLists.txt`, `native/simulation_targets.cmake` and `docs/ROADMAP.md`.

## Supported contract and boundary

The borrowed `creature_hp::Memory` supplies sparse little-endian raw32 storage.
Inputs select the creature address, initial bytes, definition pointers and
virtual table. Missing bytes read as zero. Widths, masked stores and checked
address/resource bounds retain the existing memory API's policy. The model owns
ordered writes, calls and combined events; memory also keeps callback stores.
Callbacks execute synchronously and may change memory or replace handlers.
Exceptions preserve the observed prefix, with no rollback or retry latch.
History, memory and nested-operation limits are explicit host refusals.

`filter_state` preserves PvP/inert refusals, death-timer clearing order,
root/stun/cooldown priorities, and clearing a missing target's cooldown. A zero
result invokes the required `handle_clear` callback before writing the handle.

`skelight_set_state` supports resolved states 0, 3, 5, 6, 0x10, 0x12, 0x13,
0x14 and 0x17, and the original fallback for every other value except 1. It
retains the old state for 0x12, updates the idle timer only for state 0, and
advances exactly one object-local LCG draw for state 6. Shared RNG is untouched.
State requests use signed raw32 interpretation; the final state store narrows
to 16 bits. Timers and seeds wrap to 32 bits. Animation timing is the exact
0x3dcccccc pattern; arithmetic needs no host floating point or FPU operation.

Death sets the documented flags, clears the eight-byte shared shatter static,
queries a signed sample count, visits indices 0, 3, 6, ... below that count,
spawns effect 2, calls delete, then stores the state. Slot16 is re-resolved for
every query, with signed table adjustment. Its documented 0x3c2738 body returns
the class-definition model pointer; unknown virtual bodies require
`Creature.v16`. `external_slot16` explicitly forces that callback. Other known
arena bodies redirected into slot16 refuse rather than running invented logic.

All animation, handle, sample, effect and deletion services remain required
callbacks. There are no implicit zero-success handlers. Scalar callback arguments
are masked raw32, vector arguments retain three raw patterns, and sample results
are explicit pairs of vectors (`Sample`); the second vector is unused by this
body. Count and virtual model results require integers. The caller owns deletion,
asset identities, animation effects and sample geometry. No loot/death-script
behavior is added to Skelight death.

A filtered state 1 throws `Unsupported` after the base filter's effects, before
path/wander effects. A request for 1 that is refused or redirected by the filter
still follows that filter exactly. Callers must inspect partial observations
when handling any refusal.

## Reference vectors and validation

Regenerate from immutable git blobs, validating every hash first:

```sh
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R arena-monsters-exact --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

The generator imports pinned reference dependencies from a temporary directory,
without editing or importing the research working-tree files. It does not execute
the differential harness or read private game files. The TSV contains initial
pokes, operation sequences, callback inputs, and Python-computed expectations.
The JSON manifest pins evidence, vector hash and case count. Expectations compare
**all** sparse bytes, memory store log, model write/call logs, chronological
combined events, shared draw count and operation results. The native test never
computes expected behavior from its implementation.

Cases cover every supported/fallback state, signed request/timer boundaries,
filter priorities, PvP/inert gates, RNG parity and multi-call state retention,
signed counts and this-adjustments, forced/unknown slot16 callbacks, callback
memory mutations and failure prefixes through death. Host checks cover missing
handlers, unsupported wander/body redirects, malformed callback values, address,
history and recursion refusals, handler replacement, and reuse after failure.

## Arena creature base lifecycle and AI services (earlier run)

This family ports all fourteen remaining base entry points from arena specification
sections 2.1–2.5 and the pinned reference's "Pieces of the creature base class":

| Entry point | Contract |
|---|---|
| `creature_ctor` | Ordered base initialization, serial-byte wrap, properties, balance lookup and statistics call-out |
| `set_def_entry_params` | Four shared class-definition mask/time writes, re-resolving the record each pair |
| `turn_to_target` | Target position and signed virtual-table adjustment supplied to slot26 |
| `can_see_target_in` | Signed 16-bit facing/arc narrowing and unnormalized visibility result |
| `is_target_charging` | Exact raw-pattern bearing/speed math, unwrapped facing difference and strict speed threshold |
| `count_allies_near_target`, `is_blocked_by_target` | Required service callback only when a target handle exists |
| `is_not_rooted`, `is_attack_ready`, `is_not_stunned` | Timer tests; attack readiness first clears cooldown when targetless |
| `slot16_model`, `slot34_set_home_range` | Definition model pointer and unsigned low-16-bit HomeRange conversion |
| `slot25_can_see_target` | Pointblank then facing/arc checks, re-reading target/range/facing after callbacks |
| `slot28_on_death` | Follower flags, selected death script, optional named loot, hidden deletion, PvP credit and music stop |

The constructor retains repeated property reads and short-circuit light queries.
The home vector uses the reference's eight-byte then four-byte store sequence.
Only specified fields are initialized; existing unrelated bytes survive. The
constructor's final facing read observes callback changes. `balance_lookup` must
return `Balance {found, factors[5]}`; absent/malformed handlers refuse. A not-found
result supplies five exact 1.0 patterns to `Creature_setStats`. GameObject setup,
properties, balance/statistics, grid changes and name registration remain
callbacks. No shared or object seed draw is added for these delegated services;
callback memory stores remain visible in the complete memory log.

Turning resolves slot26 through the current table and signed this adjustment.
Unknown functions require `Creature.v26`. The documented base turning body
0x10bb08 requires explicit `external_slot26`, matching the reference's external
selection; otherwise it refuses before dispatch. Redirects to arena bodies with
incompatible signatures also refuse unless explicitly external. This wrapper does
not integrate the standalone native turning module or infer engine dispatch.

Death callbacks run synchronously. A failing death script leaves the selected
object global set, exactly as the reference's failure prefix; cleanup is not
invented. Callers own loot, kill attribution, animation and deletion effects.
All numeric calculations use the existing raw-pattern core arithmetic and math,
with the caller's current FPU context (vectors use the reference default PS2
model). No host floating-point calculation or new game constants are introduced.

The generator appends 1,916 authored synthetic cases to the unchanged 1,544
original cases: **3,460 total**. New cases cover each entry point, constructor
properties with and without type/balance results, repeated construction, serial
wrap, callback mutation/failure prefixes, event-byte narrowing, missing targets,
signed table adjustments, delegated turning, two-stage visibility and charging
threshold/facing/raw-pattern boundaries. Host checks add malformed balance,
turning refusals, death-script selection retention, partial event-history limits,
and upfront raw32 address validation at every new entry point. The full original TSV prefix remains byte-for-byte identical.

## Exactly what remains

- Skelight run; resolved set-state 1's clear-path/wander contract.
- Horsemen constructor, run, set-state, mounted lifecycle and casts.
- Zek run and its AI switch, plus take-damage and inherited creature integration.
  Construction, state entry, slot35, specials and the wave tail are now ported.
- Summon Skeleton, its cosmetic RNG/sampling helpers and required Skelight
  constructor are now ported; engine-service integration remains.
- ArenaScript engine integration and script-task dispatch (the standalone contract is now ported).
- All animation, rendering, weapon, world, skill, pool and engine-service
  integration; original dispatch and Session wiring.
- Norrath behavior, retail acceptance and Windows execution.

For exact continuation, the **15 uncounted units** in the pinned Python file
are grouped below. Partial native equivalents do not remove a unit from this list.
`Model.` is omitted from method names in the table.

| Family | Remaining exact Python names |
|---|---|
| Horsemen | `horsemen_ctor`, `horsemen_run`, `_horsemen_cast`, `horsemen_set_state` |
| Skelight | `skelight_run`, `skelight_set_state` (resolved wander remains) |
| Zek tick and damage | `zek_take_damage`, `zek_run`, `_zek_switch` |
| Shared dispatch and helpers | `call`, `vcall`, `skill_call`, `table_call`, `set_state` |
| Top-level helpers | `shape_result` |

The reference's draw/save/load/clone routines are observed-only research and
are not implemented here. No claim of full monster or level coverage follows
from these state tests.

## Local execution evidence

On 2026-10-13, AppleClang 21 / macOS arm64 Release passed the 1,544 reference
cases and host checks. Regeneration to `work/arena_monsters.tsv` and its JSON
manifest was byte-identical (`cmp` for both files). An x86_64 build executed
under Rosetta and produced exactly the same complete per-case state trace as
arm64. Both trace SHA-256 values were
`ebfcbed95384b6ca18b78566b9c1c4acacb182f673c18151fbfddf0f579e8d7b`.
This is local architecture parity, not native Intel hardware or Windows evidence.

The explicit x64 build and comparison commands were:

```sh
/usr/bin/clang++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/arena_monsters.cpp native/tests/arena_monsters_tests.cpp -o build/native/arena-monsters-tests-x64
build/native/champions-arena-monsters-tests native/tests/vectors/arena_monsters.tsv work/arena-arm64.trace
arch -x86_64 build/native/arena-monsters-tests-x64 native/tests/vectors/arena_monsters.tsv work/arena-x64.trace
cmp work/arena-arm64.trace work/arena-x64.trace
shasum -a 256 work/arena-arm64.trace work/arena-x64.trace
```

Initial compilation found a missing parenthesis in the new source; it was fixed
before the successful Release build. The first standalone x64 link omitted
`core_math.cpp`; adding that required existing dependency resolved the link.
The initial configure, regenerated fixtures, affected CTest and final builds
passed. No research/private-validation inputs were modified or copied.

Fixture reproducibility and final affected-check commands (all passed):

```sh
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re --output work/arena_monsters.tsv
cmp native/tests/vectors/arena_monsters.tsv work/arena_monsters.tsv
cmp native/tests/vectors/arena_monsters.json work/arena_monsters.json
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
git diff --check
```

Final full-suite command `ctest --test-dir build/native --output-on-failure -j 3`
passed **101/101 tests**, zero failures, in 1149.19 seconds while other sibling
worktrees were testing concurrently. No timeout-only rerun was needed. The final
Release configure/build and the affected arena/simulation-boundary rerun passed.
Research HEAD and clean working-tree status were checked again after validation.

## Current-run execution evidence (2026-10-13)

Release configure/build and the affected arena/simulation-boundary checks passed.
Fixture regeneration and both `cmp` checks passed; all 1,544 previous rows remain
byte-identical. The initial affected test caught an incorrect home-vector store
width; correcting it to the reference's 8+4 sequence resolved the mismatch. A
signed-comparison warning in the test callback was also corrected.

Commands used for current fixture and native validation:

```sh
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re --output work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.tsv work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.json work/arena-regenerated.json
```

No private game inputs were used. Original engine/Session dispatch, playable
behavior, Norrath, Windows and console/retail execution remain unverified.

The current-run x86_64 command is the same explicit compiler command above.
Arm64 and x86_64/Rosetta each passed all 3,460 cases and host checks, then
`cmp work/arena-arm64.trace work/arena-x64.trace` passed. Both complete traces
have SHA-256 `2089003c2c4855139eaf89593771e693b6e7f6decc753523373f12f7916cf05f`.
This establishes local architecture parity only; it does not establish Windows
or native Intel hardware execution.

Final review added upfront raw32 creature-address guards to all fourteen new
entry points, preventing a huge 64-bit address from wrapping during field-offset
addition. Host checks verify refusal before memory or callback effects. The
Release target rebuild and affected check passed, with all vectors unchanged:

```sh
cmake --build build/native --target champions-arena-monsters-tests -j 4
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
```

The full `ctest --test-dir build/native --output-on-failure -j 3` run completed
all 101 tests in 2797.99 seconds: 99 passed, including the final rebuilt arena
binary, and two unchanged Session transport tests exceeded their existing
600-second native subprocess timeout under concurrent sibling-worktree load.
Neither reported a state mismatch. `session-creature-transport` passed when run
alone in 289.87 seconds with its original timeout:

```sh
ctest --test-dir build/native -R '^session-creature-transport$' --output-on-failure
```

`session-animation-transport` also passed alone in 147.49 seconds with its original
timeout. Together, the full run and these two isolated reruns provide passing
results for all 101 tests; the initial parallel run itself was 99/101, not a
zero-failure run. No timeout was increased and no unrelated code was changed.
The final full Release build and staged whitespace check passed. Research HEAD
remained at the pinned commit and its working tree remained clean.

```sh
ctest --test-dir build/native -R '^session-animation-transport$' --output-on-failure
cmake --build build/native -j 4
git diff --cached --check
```

## ArenaScript level progression and scaled-time scheduling (earlier run)

Arena specification section 10 and the pinned Python contract's final section
supply this complete family: `scaled_time` plus all fifteen `ArenaScript` methods,
including construction, `defer`, `frame`, all trigger/death/cutscene entries and
mission exit. It shares the existing module, generator, runner and vector manifest.
`ScriptCall` retains ordered typed requests in `log`; engine execution stays with
the caller. These are script observations, not completed cutscenes, sound,
quest delivery, world changes or Session integration.

The quest map begins with the nine reference keys and applies caller overrides,
including additional keys. Globals 0 and 4 reset on construction. The caller
owns persisted quests; no original save-file format is inferred. Names queued
in `back` are promoted after a Script frame and run once on the next frame.
A 65th deferred name is silently dropped. Pausing stops counter increments,
while queued entries and threshold checks still run. Knight completion resets
global 4 and waits for scaled time greater than 300. Boss completion retains
both deferred copies after the sound threshold, sets mission quest variables,
and emits the final sequence once. Repeated deaths and entries preserve the
reference's resets, duplicate queues and idempotence.

`scaled_time` uses the existing raw-pattern FPU conversion, multiplication and
signed saturating conversion: no host float or approximate division by 60.
Integer quest/counter values use a signed64 host domain. An attempted increment
past INT64_MAX explicitly refuses before mutation; Python's unbounded integer
storage is not emulated. Deferred names include all no-argument script entries and constructor reset.
Recursive `frame` dispatch has the existing 64-operation host depth bound.
Unknown names and argument-requiring methods refuse
when dispatched, retaining the queue and preceding effects. Clearing a refusal
is the caller's responsibility. The host log has an explicit history bound and
retains its successful prefix on failure. There are no implicit engine handlers.

The generator adds **146** script cases to the unchanged **3,460** prior rows,
for **3,606 total**. Observations after every command compare the full quest and
global maps, both queues, pause state, typed log and scalar result. Repeated-frame
commands cover threshold checkpoints, 302 counted knight frames, 877 unpaused
boss frames after queue promotion, pauses, signed/nonzero quest flags, duplicate
boss entries, queue saturation and deferred dispatch of every valid entry.
Raw32 conversion cases include signed32/64 boundaries. Host checks cover history
and counter refusal prefixes, unknown/recursive deferred names, reset during
queue iteration and reuse after failure.

### Historical progress audit after ArenaScript

The pinned file has 78 portable units: four top-level functions, 59 directly
owned Model methods, and fifteen ArenaScript methods. Nested local functions
are not separate units; inherited ref_creature methods are not in this file.
No test/harness entry points appear in this pinned file. `shape_result` is included
because production callback fallback uses it. Private-prefix helpers are included.

The 31 fully implemented and tested units counted here are:

- Thirteen base methods: `creature_ctor`, `set_def_entry_params`,
  `can_see_target_in`, `is_target_charging`, `count_allies_near_target`,
  `is_blocked_by_target`, `is_not_rooted`, `is_attack_ready`, `is_not_stunned`,
  `slot16_model`, `slot34_set_home_range`, `slot25_can_see_target`, `slot28_on_death`.
- `Model.clip` and `Model.skelight_shatter`.
- `scaled_time` and all fifteen ArenaScript methods.

`turn_to_target` is conservatively excluded because the current virtual dispatcher
refuses the internally modeled base slot26 body unless explicitly delegated.
`skelight_set_state` is excluded because resolved wander state 1 refuses.
`filter_state` is inherited and excluded from the denominator. General Model
call/virtual plumbing is partial and excluded from the numerator. Equivalent
helpers not yet exposed and tested as complete units are not counted twice.
The remaining 47 units include those partial ports and all monster/spell families
listed above; `shape_result`, signed half/third helpers and remaining plumbing
also need complete port evidence. No refusal is counted as a completed unit.

### Historical continuation inventory (superseded)

The ArenaScript run's 47-unit remainder has since been reduced. Use
[Exactly what remains](#exactly-what-remains) for the current 15-unit inventory
and the manifest's `ported_units` list for the complete count. Completing monster
ticks still requires their inherited `ref_creature` behavior or the reference's
explicit external selection; an implicit callback cannot replace an internally
modeled body.

### Continuation validation (2026-10-13)

Release configure/build, the arena/simulation-boundary checks and the final arena
rebuild/check passed. Regeneration to `work/arena-script-regenerated.tsv` and
both `cmp` checks passed. A `git show HEAD:native/tests/vectors/arena_monsters.tsv`
prefix comparison confirmed all 3,460 prior rows are byte-identical.
The final 3,606 cases and host checks passed on arm64 and x86_64/Rosetta.
Complete per-case traces were byte-identical, both SHA-256
`7a8e5702a04f880ec723b27b7c7b2fac37e174919d98130f507f8c8beca41e9a`.
This is local architecture parity; Windows and native Intel hardware remain untested.

Commands (passed):

```sh
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
cmake --build build/native --target champions-arena-monsters-tests -j 4
ctest --test-dir build/native -R '^arena-monsters-exact$' --output-on-failure
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re --output work/arena-script-regenerated.tsv
cmp native/tests/vectors/arena_monsters.tsv work/arena-script-regenerated.tsv
cmp native/tests/vectors/arena_monsters.json work/arena-script-regenerated.json
/usr/bin/clang++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/arena_monsters.cpp native/tests/arena_monsters_tests.cpp -o build/native/arena-monsters-tests-x64
build/native/champions-arena-monsters-tests native/tests/vectors/arena_monsters.tsv work/arena-script-arm64.trace
arch -x86_64 build/native/arena-monsters-tests-x64 native/tests/vectors/arena_monsters.tsv work/arena-script-x64.trace
cmp work/arena-script-arm64.trace work/arena-script-x64.trace
shasum -a 256 work/arena-script-arm64.trace work/arena-script-x64.trace
```

No private game input was read. Research files were read-only immutable contract
sources. Monster constructors/ticks, engine execution, original dispatch,
Session integration, Norrath and retail/console behavior remain unsupported or
unverified as listed above. Shared build registrations and ROADMAP were unchanged.

Full-suite command `ctest --test-dir build/native --output-on-failure -j 3`
finished in **2007.21 seconds** with **100/101 passing** (exit 8). The final
arena and simulation-boundary tests passed. The sole non-passing test was the
unchanged `objects-runtime-transport`: its Unicode-path native subprocess hit
the existing 60-second timeout, with no state mismatch reported.

As required, it was rerun alone with its original timeout:

```sh
ctest --test-dir build/native -R '^objects-runtime-transport$' --output-on-failure
cmake --build build/native -j 4
git diff --cached --check
```

The isolated rerun also exited 8, after **143.97 seconds**, solely because its
truncated/duplicate-input subcase hit the same 60-second subprocess timeout.
The Unicode-path subcase passed on that rerun. No assertion or state mismatch
was reported. This transport check remains **unverified within its time bound**;
it is not recorded as passing. The objects library and trace executable were
confirmed to use `-O3 -DNDEBUG`, and that trace target links the unchanged objects
module, not the arena module. Neither its code nor timeout was changed. Concurrent
sibling work was active; no specific cause beyond the observed timeout is claimed.

The final full Release build and staged whitespace check passed. Research HEAD
remained pinned and its working tree remained clean. The arena family has complete
passing reference and architecture-parity evidence; the unrelated transport timeout
remains an explicit limitation of the full-suite result.


## Tagar’s Insects cast and complete spell lifecycle (this run)

Family selected: Tagar’s Insects cast and complete spell lifecycle. Pinned arena
specification sections 7.1–7.4 and the corresponding Python bodies supply ten
new complete units: `tagar_cast`, `tagar_spell_ctor`, `tagar_spell_run`,
`tagar_tick_victims`, `tagar_pick_target`, `tagar_is_valid`, `spell_base_ctor`,
`spell_rand`, `_unit` (native `unit`) and `_spell_ball` (native `spell_ball`).
The existing module, generator, test runner, vector manifest and this page are
extended; no shared build or roadmap files are changed.

The cast resolves skill slot14, requests sound, allocates a spell and constructs
it when the pool returns a block. Construction preserves signed-byte level use,
word/byte order, the 8+4 vector-copy stores, one shared draw and exact facing math.
Target selection preserves query list order, invalid/marked/duplicate exclusion,
strict nearest-distance ties and the stack-list handle-clear request. Victim
updates re-read the count each iteration, expire markers against the old clock,
retain flash-write order, apply player low/high or nonplayer spread through
explicit callbacks, and make all five particle ball samples per active victim.

Run retains clock increment and hunt/limit early exits, invalid current-slot
handling, reflected heading correction, flight-height adjustment, the division
by zero computed on an arrival tick, slot times and marker allocation (one
shared draw only if allocation succeeds). Wander and all fifty cloud particles
use the spell’s own LCG, including every rejected ball attempt. Omitting cosmetic
particles would change later movement, so their draws and callback order are
preserved. The final marker particle observes frame and heading exactly where
the reference reads them. All arithmetic uses the existing raw-pattern FPU/core
math; no host floating-point approximation or invented constant is used.

`Objects` is the ordered list result required from world collectors;
`DamageRange` is the raw32 low/high result required from skill slot8. A string
argument represents the reference’s `handle_clear('stack')`. Missing callbacks,
malformed list/range/integer results, oversized lists, raw32 address violations,
history exhaustion and nested-operation exhaustion retain their observed prefix.
The host must consume world, damage, pool, sound, marker and particle requests;
these vectors do not establish original dispatch or playable scene integration.

Virtual addresses and signed this-adjustments are re-resolved at every call.
Unknown bodies remain required callbacks. A known inherited enemy body requires
explicit `external.insert("Creature.v20")`, matching the reference’s external
selection; incompatible known creature/skill redirects and unported Summon spell
run redirects refuse explicitly. Known Tagar spell-run redirects execute internally
and are depth-bounded. Those partial dispatch adapters are not counted as ported
reference units. The public `external` selections apply to this new family;
legacy `external_slot16`/`external_slot26` behavior and all earlier vectors stay
unchanged. Spell addresses, callback scalar values and list object addresses use
the explicit raw32 domain; sparse memory and history bounds remain host limits.

### Function accounting and synthetic coverage

An AST enumeration of the immutable pinned file finds 78 top-level functions and
direct class methods. There are no test/harness entry points in that file;
`shape_result` is the model’s callback result marshaller and is included in the
denominator. Inherited `ref_creature` methods are excluded. Private underscore
methods are included. The fixture manifest lists the 41 fully supported/tested
units by their exact Python names; the generator validates membership, uniqueness
and both counts, and the integrity runner checks the same list. The prior 31
are the fourteen arena base entry points, `skelight_shatter`, `scaled_time` and
all fifteen ArenaScript methods. The ten entries above bring the count to 41.
Partial Skelight set-state and partial dispatch/marshalling helpers do not count.

The generator adds **723 cases**, for **4,329 total**. All **3,606 previous rows**
remain byte-for-byte identical. New observations compare every sparse byte,
memory store, model write, call, chronological event, shared draw count and result.
Cases include seeded and repeated RNG/ball samples, raw draw boundaries, signed
levels/facing/this-adjustments, allocation failure, list order/ties/range cap,
expired/invalid/missing victims, player/nonplayer damage, flight reflection,
zero-distance arrival, marker allocation, limit and expiry exits, repeated ticks,
callback mutation/re-reads and failure prefixes at sixty run callback positions.
Host checks additionally exercise missing/malformed callbacks, known-body refusals
and explicit delegates, all new address guards, rejection-loop history bounds,
internal spell recursion and reuse after refusal.

### Tagar execution evidence (2026-10-13)

AppleClang 21 / macOS arm64 Release configure, full build and affected tests
passed. The initial affected check failed only its new host recursion-prefix
assertion: a depth of four permits three run clock writes before refusing the
next victim tick. Correcting that expected count resolved the host check; the
reference vector expectations were not changed to match native output.
Fixture regeneration, both file comparisons and the complete old-prefix check
passed. Arm64 and x86_64/Rosetta each passed all 4,329 cases and host checks;
their complete state traces matched byte-for-byte, with SHA-256
`1472a3881a9c7e925fcc6323ab07c93e37a62cb9338a4c5d98822e33abe1516f`.
This is local architecture parity, not Windows or native Intel hardware evidence.

Exact commands (passed unless separately recorded below):

```sh
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake --build build/native --target champions-arena-monsters-tests -j 4
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re --output work/tagar-regenerated.tsv
cmp native/tests/vectors/arena_monsters.tsv work/tagar-regenerated.tsv
cmp native/tests/vectors/arena_monsters.json work/tagar-regenerated.json
/usr/bin/clang++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/arena_monsters.cpp native/tests/arena_monsters_tests.cpp -o build/native/arena-monsters-tests-x64
build/native/champions-arena-monsters-tests native/tests/vectors/arena_monsters.tsv work/tagar-arm64.trace
arch -x86_64 build/native/arena-monsters-tests-x64 native/tests/vectors/arena_monsters.tsv work/tagar-x64.trace
cmp work/tagar-arm64.trace work/tagar-x64.trace
shasum -a 256 work/tagar-arm64.trace work/tagar-x64.trace
git diff --check
```

No private game inputs were used. The research checkout remained read-only at
the pinned commit. The remaining monster/Summon families are portable from the
existing evidence; their absence is not a new research blocker. Engine services,
Session wiring, playable scene behavior, Norrath, Windows and retail/console
execution remain unverified or unsupported.

Final full-suite command:

```sh
ctest --test-dir build/native --output-on-failure -j 3
```

The parallel run completed all **101 tests in 1714.14 seconds**, with **100
passing** and exit 8. The sole non-passing check was the unchanged
`session-objects-transport`: its Unicode-path native subprocess exceeded the
existing 600-second timeout, with no state mismatch reported. The rebuilt
arena-monsters and simulation-boundary checks passed in this full run.

The required isolated rerun used its original timeout:

```sh
ctest --test-dir build/native -R '^session-objects-transport$' --output-on-failure
```

It **passed**, exit 0, in **405.36 seconds**. Together the full run and isolated
rerun give passing results for all 101 tests; the initial parallel run itself
was 100/101. No test timeout or unrelated source was changed. The final full
Release rebuild and staged whitespace check passed. Research HEAD and clean
working-tree status were verified after validation.

```sh
cmake --build build/native -j 4
git diff --cached --check
git -C ../../champions-return-to-arms-re rev-parse HEAD
git -C ../../champions-return-to-arms-re status --porcelain
```

## Summon Skeleton cast and spell lifecycle (this run)

Chosen family: **Summon Skeleton cast and spell lifecycle**, all six remaining
Summon methods plus `cosmetic_draw` and the prerequisite `skelight_ctor`.
Evidence is the pinned contract above and arena specification sections 4.2, 6
and 9, with inherited virtual dispatch from pinned `ref_creature.py`. No new
constants, asset content, private inputs or guessed engine behavior are used.

The eight newly counted units are `summon_can_use`, `_cosmetic_ball`,
`summon_cast`, `_summon_sample`, `summon_spell_ctor`, `summon_spell_run`,
`cosmetic_draw` and `skelight_ctor`. The AST accounting counts every top-level
function and class method in the pinned file (78 total; no harness/test helpers).
The generator and manifest list the 49 fully covered units explicitly. Shared
helper implementations used here remain conservatively uncounted until their
full standalone APIs/dispatch domains are covered; Skelight set-state still
refuses resolved wander and remains uncounted.

Summoning preserves all four flash writes, 32 first particles plus 200 second
particles, cosmetic rejection loops and signed sample-count magnitude (including
INT_MIN). Cosmetic draws write only 0x404460; shared draws keep their original
positions and failure prefixes. The strict double comparison of an f32 height
difference is implemented by exact raw ordering, including unordered NaNs,
infinities and the +/-24 boundary. `world_findFreeSpot` must return
`FreeSpot {point, found}`; only its signed low byte chooses the returned point.
Terrain, sample geometry, particles, pool allocation, statistics, elemental
weapons, charm, animation and sound remain synchronous required callbacks.
Sampling here requires `Vec3`, whereas Skelight death's two-output query requires
`Sample`. Malformed callback results refuse after the exact observed prefix.

The prerequisite constructor initializes the embedded holder, preserves ordered
base writes, temporarily replaces the monster-level global for one statistics
call, compares bounded strings with ASCII byte case folding, restores the global
only after success, writes the complete 64-bit hide mask, enters rise and joins
the supplied frame service. Callback failure does not invent restoration.
Summon player charm uses signed slot37 team and low-16-bit slot34 home range
through their matching documented virtual bodies and signed this adjustments.
Other delegated virtual bodies remain callbacks; unported/incompatible modeled
redirects retain explicit refusals. Spell destruction uses slot1 with argument 3,
including modeled spell-run redirection and bounded recursion. No attached-spell
list or automatic monster-spell cleanup is invented.

The existing module, generator, runner and fixture manifest are extended in place.
The 4,329 previous TSV rows remain byte-identical; 531 new authored synthetic
cases bring the total to **4,860**. Coverage includes every new entry, all player
slots and name-lookup results, full particle emissions, RNG seeds/elemental picks,
allocation failures, signed level/count/facing limits, terrain/found-byte boundaries,
64-byte strings and non-ASCII bytes, signed virtual adjustments, repeated
construction/casts, callback mutations, and failure prefixes. Host checks cover
missing/malformed handlers, raw32 address refusals, rejection-loop resource limits,
internal spell recursion and reuse after refusal.

### Summon execution evidence (2026-10-13)

Release configure/build and affected tests passed on macOS arm64 / AppleClang 21.
The first affected check exposed a missing virtual-table setup in the new malformed
free-spot host fixture; fixing that fixture made it reach the intended callback.
No reference expectation was changed to fit native output. Regeneration, both
file comparisons, and the complete previous-prefix check passed.

Exact commands run (passed unless a later result explicitly states otherwise):

```sh
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake --build build/native --target champions-arena-monsters-tests -j 4
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re --output work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.tsv work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.json work/arena-regenerated.json
/usr/bin/clang++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/arena_monsters.cpp native/tests/arena_monsters_tests.cpp -o build/native/arena-monsters-tests-x64
build/native/champions-arena-monsters-tests native/tests/vectors/arena_monsters.tsv work/arena-arm64.trace
arch -x86_64 build/native/arena-monsters-tests-x64 native/tests/vectors/arena_monsters.tsv work/arena-x64.trace
cmp work/arena-arm64.trace work/arena-x64.trace
shasum -a 256 work/arena-arm64.trace work/arena-x64.trace
ctest --test-dir build/native --output-on-failure -j 3
```

Arm64 and x86_64/Rosetta each passed all 4,860 cases and host checks. Their
complete state traces matched byte-for-byte with SHA-256
`a9677fe402eda13fa55160342946fcd0d34f29a469d55bada0a819b83113aa72`.
This establishes local architecture parity only.

Engine/Session integration, monster ticks, playable scene behavior, Norrath,
Windows, native Intel hardware and retail/console execution remain unverified.
The remaining 29 contract units are portable from existing evidence; this is
STATUS: MORE. The research repository is read-only and no private files were used.

The final full Release suite passed **101/101 tests**, exit 0, in **940.91 seconds**.
No timeout-only failures or isolated reruns occurred. The affected arena and
simulation-boundary tests also passed in that full run. Final whitespace and
research-state checks passed; the research HEAD remained pinned and its working
tree remained clean. Changes were committed locally on `codex/port-arena-monsters`;
no push, pull request or issue action was performed.

```sh
git diff --check
git diff --cached --check
git -C ../../champions-return-to-arms-re rev-parse HEAD
git -C ../../champions-return-to-arms-re status --porcelain
```


## Shared holder/data and signed attack-scaling family (2026-10-13)

This run completes all nine units in this family: `Model.holder_call`,
`Model.cstr`, `Model.class_model`, `Model.clip`, `Model.weapon_descriptor`,
`Model._prop_int`, `Model._init_holder`, `half_toward_zero` and
`third_toward_zero`. C++ exposes the last two Model helpers as `prop_int` and
`init_holder`. Existing constructor consumers retain the same store and call
sequences. Their implementations are authored from the pinned Python plumbing
section and holder-initialization contract; arithmetic helpers narrow to signed
16 bits before truncating division toward zero.

Holder calls always require `Holder.v<slot>` callbacks, preserving the signed
16-bit table adjustment, raw32 argument masking and unchanged callback result.
No holder engine body is inferred. A missing holder makes `weapon_descriptor`
return zero without dispatch; otherwise it requests slot 17, hand 1. Property
reads require `obj_getPropInt` with the explicit zero default. Strings preserve
arbitrary bytes through NUL or the supplied bound. Class model and clip reads
resolve the current definition pointer on every invocation. Holder initialization
retains every ordered table/byte/halfword store and optional final table.

The accounting generator enumerates the pinned file's top-level functions and
class methods with Python AST: 78 units, no test/harness helpers. Its explicit
fully supported list now has 58 entries. The inherited `filter_state` is outside
that file; partially supported `skelight_set_state` and unresolved dispatch
wrappers remain uncounted. Promoting existing private helpers to tested public
operations is counted only after complete direct vector coverage, rather than
counting their earlier constructor-only usage.

The generator appends 693 synthetic cases, giving 5,553 total; the complete
4,860-row prior TSV prefix is byte-identical. New vectors compare all sparse
bytes, memory stores, model writes/calls/events, shared draw count and results.
They exhaust all 65,536 signed-16 patterns for both damage-scaling helpers with
nonzero discarded upper bits. Other cases cover all holder slots used by these
monsters, signed this-adjustments including raw32 wrap, missing holders, repeated
calls after callback mutation, failure prefixes, byte-string termination/bounds,
and optional holder tables over nonzero initial storage. Host checks cover missing
handlers, raw32/span/history/depth refusals, reuse after recursion failure and
unchanged noninteger callback result forwarding.

Monster constructor/tick/state families remain the next priority. Full tick
ports need the inherited creature damage, run, activity, animation and movement
bodies from pinned `ref_creature.py`; these are documented evidence, not newly
invented callbacks. This run does not replace them with callback-success stubs.
No private files were used. Engine/Session integration, retail acceptance,
Norrath, Windows and console execution remain unverified.


Current-family verification commands (passed):

```sh
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re --output work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.tsv work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.json work/arena-regenerated.json
/usr/bin/clang++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/arena_monsters.cpp native/tests/arena_monsters_tests.cpp -o build/native/arena-monsters-tests-x64
build/native/champions-arena-monsters-tests native/tests/vectors/arena_monsters.tsv work/arena-arm64.trace
arch -x86_64 build/native/arena-monsters-tests-x64 native/tests/vectors/arena_monsters.tsv work/arena-x64.trace
cmp work/arena-arm64.trace work/arena-x64.trace
shasum -a 256 work/arena-arm64.trace work/arena-x64.trace
git diff --check
```

Both local architectures passed 5,553 cases plus host checks with identical
complete traces, SHA-256
`588e94bdb7faa7c9ac80967b1e3c882849b8a65f6aea0841bba6ee16cee688f8`. x86_64 used Rosetta; native Intel hardware remains unverified.
The first affected test rejected a generated clip address spanning beyond the
raw32 memory domain. The fixture was corrected to use a supported address;
separate host checks verify refusal for invalid spans. Regeneration reproduces
both fixture files byte-for-byte, and the entire prior TSV prefix is unchanged.
Research checkout HEAD remains pinned and its working tree is clean.


Final full-suite command passed **101/101 tests**, zero failures, in 480.67
seconds on macOS arm64 Release; no timeout-only rerun was needed:

```sh
ctest --test-dir build/native --output-on-failure -j 3
```

Only this module's header/source, existing generator/runner, vector TSV/JSON and
this page changed. No shared build files, research files or private inputs changed.


## Zek construction, state entry and ground-wave lifecycle (2026-10-13)

Selected family for this run: **Zek construction, state entry, special-attack
selection and ground-wave lifecycle**, including its level-derived slot35.
All five complete units are counted: `Model.zek_ctor`, `Model.zek_set_state`,
`Model._zek_specials`, `Model._zek_wave`, and `Model.zek_slot35`. The Python AST
inventory still contains 78 top-level functions and class methods, with no test
or harness helpers. The manifest records all 63 counted names; partial dispatch
and the partially ported Skelight state setter remain uncounted.

Evidence is the same immutable commit and hashes above: arena specification
sections 5.2, 5.4 and 5.6, the special-attack branches in 5.3, and the pinned
Python bodies. Inherited path clearing, wander selection, random-point-near-home,
and slot35 are authored from `ref_creature.py` at that same pin. Those inherited
helpers do not add units to this file's accounting.

The constructor reuses the existing base constructor and holder implementation,
retains the eleven event-entry writes in their original order, and adds no RNG
draw. State entry covers every filtered state, including complete state-1 path
clearing and wandering, cooldown initialization, exact clip/blend flags, the
remembered attack state, one object-local draw for clip choice, and both death
branches. Death compares the two raw byte strings case-sensitively; loot, weapon
dropping and animation remain required synchronous callbacks. Slot28 resolves
the current table and signed adjustment, executes the ported on-death body when
selected, and otherwise requires the documented virtual callback or explicitly
refuses an unported/incompatible internal redirect. This does not count the
remaining general `vcall` dispatcher as complete.

`zek_specials` takes the supplied state function explicitly, matching Python's
`st` argument. It uses signed clocks, strict thresholds and callback-time rereads;
a successful wave selection prevents the ground-pound query. A state function is
required only on a selected attack. It can call `zek_set_state` directly or route
through the caller's supported dispatcher. Neither engine dispatch nor the full
Zek AI tick is inferred.

`zek_wave` preserves y-before-x movement stores, the terrain query, the height
store and its optional 8+4-byte undo, steering toward a linked enemy, strict
36/800-unit termination tests, and the complete impact sequence. The absolute
height-difference comparison retains the Python double comparison after its
raw32 subtraction, including infinity/NaN behavior. Termination makes exactly
one object-local decal draw, attempts the optional light allocation, clears the
handle, and still emits the last effect-7 call. Shared RNG stays untouched.
Callbacks can alter position, facing, clock and definition pointers; later reads
observe their writes. Failures retain all effects before the failing service.
This API exposes the wave tail independently; callers must supply the original
active/tick gating before scene integration.

The generator appends **8,533 Zek cases** to **5,553 unchanged prior cases**:
**14,086 total**. Coverage includes constructor property/balance branches and
failure positions, every state/fallback and filter priority, both death branches,
signed slot28 adjustment, wander skip/home/ten-attempt behavior, signed timer
wrap, special priority, wave steering, exact distance and height thresholds,
infinities/NaNs, repeated waves, allocation failure and callback mutations.
Native host checks add missing/malformed terrain responses, missing state setter,
raw32 address refusals, history limits, recursive callbacks and reuse afterward.

Validation commands and outcomes for this run are recorded below. No private game
inputs were used. Full original ticks, damage processing, original engine/Session
integration, playable scenes, Norrath, Windows and console execution remain
unverified. Research checkout remains read-only throughout.

Release configuration/build, affected checks, fixture regeneration and all
byte-identity comparisons passed:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(arena-monsters-exact|simulation-boundary)$' --output-on-failure
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_arena_monsters_vectors.py --research-dir ../../champions-return-to-arms-re --output work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.tsv work/arena-regenerated.tsv
cmp native/tests/vectors/arena_monsters.json work/arena-regenerated.json
```

A Python byte-prefix assertion against `work/arena-before.tsv` verified all
5,553 previous rows unchanged. The existing integrity checker verifies the
manifest's count, complete unit-name list, pin hashes and vector hash before
executing the affected native test.

Local arm64 and x86_64/Rosetta execution passed all 14,086 cases plus host checks;
the entire traces matched, SHA-256
`230581b3f9e5d5407ef4165e7970dc5fe2aa4ef864136f0091e6e571ff742390`.
Commands (all passed):

```sh
/usr/bin/clang++ -arch x86_64 -std=c++17 -O3 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/creature_hp.cpp native/src/arena_monsters.cpp native/tests/arena_monsters_tests.cpp -o build/native/arena-monsters-tests-x64
build/native/champions-arena-monsters-tests native/tests/vectors/arena_monsters.tsv work/arena-arm64.trace
arch -x86_64 build/native/arena-monsters-tests-x64 native/tests/vectors/arena_monsters.tsv work/arena-x64.trace
cmp work/arena-arm64.trace work/arena-x64.trace
shasum -a 256 work/arena-arm64.trace work/arena-x64.trace
git diff --check
```

This is local architecture parity, not Windows or native Intel hardware evidence.

Final full-suite command passed **101/101**, zero failures, in **678.18 seconds**:

```sh
ctest --test-dir build/native --output-on-failure -j 3
```

No timeout-only reruns were needed. The research repository's HEAD remained
`356add27d50e50972b2a671b31e8f4ec93e6778c` and its working-tree status remained
clean. Only this worktree's existing arena header, source, test runner, vector
TSV/manifest, generator and this docs page changed; shared build/roadmap files
needed no edits. Final Release build and whitespace checks also passed:

```sh
cmake --build build/native -j 4
git diff --check
git diff --cached --check
```
