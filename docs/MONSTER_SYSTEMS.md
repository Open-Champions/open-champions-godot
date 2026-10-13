# Return to Arms monster contract subset

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
| SmallSpider set-state | `0x2ab704` | All requested states, filter refusals, idle timer, wander, ignored reactions and death |
| SmallSpider touch | `0x2ab8e0` | Player flag test, virtual death transition and base touch bit 59, including null toucher base read |
| Mosquito lose-part | `0x2982cc` | Exact part mapping, repeated/invalid parts, two mask calls and optional chunk |
| Scorpion sever-part | `0x2a59f0` | Low-five-bit shift, two mask calls and optional chunk |
| Prefix differs | `0x159c20` | Signed byte indexing and supplied classification table |

SmallSpider support includes the documented base state filter, path clearing,
home-distance/wander selection, own RNG, vector-copy stores, base slot 27 and
base slot 28 death hook. The latter preserves death-script selection, optional
named-item callbacks, hidden deletion, PvP kill callback and boss-music callback.
It does not replace them with engine behavior. Virtual `this` adjustment is
signed16 and wraps to raw32. SmallSpider's own set-state and the base state/death
bodies dispatch internally at their documented pointers. The other monsters'
modelled set-state pointers explicitly refuse; unrecognised subclass pointers
call the named virtual callback. `external` permits explicit callback overrides
for slots 27 and 28. Other external slot entries are outside the API contract.

## Host boundary and refusals

`Model` borrows `creature_hp::Memory`; that storage must outlive the model.
The caller supplies definitions, clip handles, character classification bytes,
positions, globals and service results. No game strings, tables or assets are
embedded. Integer callback arguments wrap to raw32, vector arguments preserve
Python tuple shape, and mask returns retain all 64 bits. Raw arithmetic uses the
caller's current FPU configuration. The own generator writes `+0x44`; shared and
cosmetic generators are untouched by this subset.

Missing callbacks throw by default. `ReferenceZero` is an explicit opt-in to the
reference's zero return policy, useful for synthetic experiments. Callback
mutations, replacements and exceptions retain partial memory, calls and writes.
There is no rollback, fabricated allocation, animation, loot or terrain service.
Callbacks must not destroy the model during a call; arbitrary host state outside
memory is the caller's responsibility.

Creature entry addresses must accommodate the complete 0x3c8-byte base object.
Callback pointer returns must fit raw32; oversized pointers explicitly refuse.
The memory implementation enforces raw32 spans and its existing byte/history
budgets; accesses outside that domain refuse. The model separately caps each
history at 65,536 entries and recursive SmallSpider transitions at 64. Prefix
scanning refuses after 4,096 nonterminal bytes. These are portable safety
boundaries, not original engine limits. An exception does not latch the model.

## Exact synthetic verification

The generator authors memory, callbacks and commands, then lets the pinned
Python methods compute expected observations. It never uses native output to
make expectations. Fixtures contain each initial snapshot and every command's
result, status, sparse memory, memory store log, model writes, ordered calls,
interleaved events, shared draw count and callback counters. Full-width masks,
RNG seeds and untouched supplied globals therefore participate in comparison.

The corpus contains **19,376 cases**. Cases cover state/filter combinations, PvP gates, inert creatures, ignored
states, wander/follower paths and ten unsuccessful ground tests, virtual hooks,
repeated deaths, part bit mapping and repeated shedding, allocation failure,
mutating/throwing callbacks, exceptional raw arithmetic and signed prefix bytes.
All sixteen arithmetic configurations are exercised. Native execution receives
only the input columns; the wrapper independently compares every complete
observation and checks the fixture, generator and reference manifest hashes.
Normal builds require neither the research checkout nor game files.

```sh
python3 tools/generate_monsters_1_vectors.py --research-dir ../../champions-return-to-arms-re --output native/tests/vectors/monsters_1.tsv.gz
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R monsters-1 --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

## Local verification

On 2026-10-13, macOS arm64 with AppleClang 21 and a Release build:

- The regeneration command above passed and produced 19,376 cases.
- Both CMake configure and `cmake --build build/native -j 4` passed.
- `ctest --test-dir build/native -R monsters-1 --output-on-failure` passed
  both cases: 27 unit checks and 19,376 complete-state comparisons.
- `ctest --test-dir build/native --output-on-failure -j 3` passed all 102 tests
  in 987.35 seconds. No timeout or failed case required a rerun.
- `git diff --check` passed; the research checkout remained clean at the pin.

The monsters observation trace SHA-256 is
`1a1c02eb34a7187b0a64d5b2ff115569237b9bba5ecd9e3189b93779090723f6`.
Only this local arm64 execution is established. Windows/x64 and console runs
were not performed, and no private original monster-data acceptance was measured.

## Changed files

The new library is `native/include/champions/monsters_1.hpp` and
`native/src/monsters_1.cpp`. Its authored runners/tests are
`native/tools/monsters_1_trace.cpp`, `native/tests/monsters_1_tests.cpp` and
`native/tests/monsters_1_transport.hpp`. Python generation/comparison lives in
`tools/generate_monsters_1_vectors.py` and `tools/run_monsters_1_vectors.py`;
`native/tests/vectors/monsters_1.tsv.gz` and `monsters_1.json` contain the exact
synthetic corpus and manifest. This page is new. Shared changes are only additive
registration in `native/CMakeLists.txt`, `native/simulation_targets.cmake` and
four lines in `docs/ROADMAP.md`. No other modules or research files were edited.

## Precisely what remains

- SmallSpider constructor and run loop, including their creature base execution.
- All Gnaw constructor, run and set-state behavior.
- Mosquito constructor, run and set-state behavior (part loss alone is ported).
- Scorpion constructor, run and set-state behavior (part severing alone is ported).
- Badger constructor, run, set-state and empty slot 11.
- The five presentation draw functions, outside the reference model.
- All reference call-outs still require host implementations: animation, terrain,
  allocation/chunk construction, decals, handles, properties, scripts, loot,
  deletion, arena kills and boss music.
- Vampire, Ogre, WarBoar, Gremlin and Ghoul were not implemented in this reference
  batch and have no inferred implementation here.
- Session/World identity adapters, monster AI dispatch, original-data acceptance,
  playable integration, console fidelity, Norrath support and cross-architecture
  state comparison require separate evidence.
