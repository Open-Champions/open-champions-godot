# Return to Arms arena monsters: bounded Skelight state port

`champions-arena-monsters` is a handwritten portable C++17 module in
`champions::simulation::rta::arena_monsters`, independent of Godot. It implements
Skelight's non-wander set-state body, shatter, and the complete base state filter.
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

## Exactly what remains

- Skelight constructor and run; resolved set-state 1's clear-path/wander contract.
- Horsemen constructor, run, set-state, mounted lifecycle and casts.
- Zek constructor, run, state transitions, take-damage, slot35 and ground wave.
- Base constructor, definition event-table writes, visibility/charging/allies/
  blocking/turn helpers, timer helpers, slot25, slot28 and slot34 entry points.
- Summon Skeleton can-use/cast and spell constructor/run/base constructor.
- Tagar's Insects cast, spell constructor/run, targeting, validity, victim tick,
  spell RNG and cosmetic sampling helpers.
- ArenaScript and scaled-time level scripting.
- All animation, rendering, weapon, world, skill, pool and engine-service
  integration; original dispatch and Session wiring.
- Norrath behavior, retail acceptance and Windows execution.

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
