# Return to Arms player tick and recovery helpers

`champions-player` is a handwritten portable C++17 module in
`native/include/champions/player.hpp` and `native/src/player.cpp`. It implements
complete scripted-control ticks, original input helpers, and the complete
health/mana recovery, potion consumption and enchantment-expiry helper family.
It is independent of Godot and does not connect original controls to Session or
the scene. Ordinary gameplay ticks remain unsupported.

## Immutable evidence

The source is the read-only `champions-return-to-arms-re` research checkout,
commit **`356add27d50e50972b2a671b31e8f4ec93e6778c`**:

| File | SHA-256 |
|---|---|
| [tools/ref_player.py](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_player.py) | `ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d` |
| [docs/spec/player-tick.md](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/player-tick.md) | `ba83f8672626cc5625ae108dd2838b187de06d8d37c54cdb57afae3cc71e3f37` |
| tools/ref_core.py | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| tools/difftest_player.py | `dabb453fa62eb3e3ca66c28d5d80decc9e89fdcef86bf8266027bc5af0bcee47` |

The research reference uses the MIT license, copyright (c) 2026 TrustlessMike
and contributors. That license and attribution apply to this authored translation. The specification describes Return to Arms US v2.00; this does not
establish Norrath behavior. Its differential harness compares the Python model
with an instruction interpreter and stubbed services. This port records that
harness as evidence but does not execute it or claim a new original-code test.
No retail input, generated decompiler output, original assets or game tables are
included. All public inputs are authored synthetic state.

## Implemented boundary

| Entry point | Coverage |
|---|---|
| `Tick::run` | Section 1 entry, including breath sampling, creature/knockback callbacks, white light, immunity, spawn protection, idle voice, recast and low-health rumble; complete scripted-control early return for every state |
| `choose_pad` | Section 2, all twelve ordered short-circuit gates; returns the selected address without assigning a local pad |
| `idle_fidget` | Section 9.1, stick magnitude, signed track count, extra-track cancellation, shared draw, animation-class selection and callbacks |
| `can_block` | Section 9.2, state/equipment/bow checks |
| `try_block_move` | Section 9.3, input masks, original state/byte stores, animation callback |
| `regen_health`, `regen_mana` | Natural and pending-potion delivery; passive/equipment bonuses; Blessed and Regeneration callbacks; original caps and write order |
| `tick_enchant_timers` | All 17 equipment slots, supplied terminated ID list, first empty/matching modifier rewrite and duplicate expiry |
| `use_potion`, `use_potion_real` | Both kinds; nine supplied name strings, last matching bag cell, size fallback, pending amounts, local sound/menu message paths, stack removal, HUD notices, three-bag load recount and use counters |
| `has_anim`, `skill_rank`, `passive_level` | Complete reference lookup bodies, including signed fields, terminator behavior, first match and slot-4 acceptance |

An ordinary tick executes the supported entry then throws `Unsupported` at
`0x1a5614`, before the `P+0x4c4` store, weapon trails, pad selection and remaining
pre-switch work. Entry writes and callback effects survive that refusal. Calling
it again repeats the entry; this is not a successful partial gameplay tick.
`run()` returns `"scripted"` only after the complete scripted early return.
The reference helper lookups can be invoked independently; their availability
does not imply that the unported main action loop dispatches them.

Every external service reached by this subset remains an explicit callback.
The closed `Service` enumeration maps to original reference callback names via
`service_name`. Arguments distinguish uint32 scalar patterns from three-word
vectors. The callbacks retain original order; callback returns, memory mutations,
self-removal/replacement and exceptions propagate synchronously. Calls and writes
are recorded before invocation/store, retaining failure prefixes. Missing
handlers throw `MissingHandler` by default. `ReferenceZero` is an explicit opt-in
to the scalar-zero defaults of this subset's reference services; it does not
supply actual movement, animation, sound, effects or session behavior.

`Tick` borrows the existing bounded sparse `creature_hp::Memory`. Keep both memory
and tick alive throughout callbacks; never destroy either during an operation.
Callbacks are copied before invocation to allow self-replacement safely. Keep
persistent callback state in memory or stable external captures; mutable
value-capture identity is a host boundary. Callback-owned external state remains
the caller's responsibility. All numeric
lanes use the existing integer-pattern core with the caller's selected FPU
configuration; no host float, libm or physics enters simulation. Pointer offsets
use checked raw32 spans. The `record()` accessor and potion menu-cell expression
retain the reference's explicit wrapping conversions; other negative/out-of-range address expressions
refuse. Helpers can operate on an explicitly supplied player address distinct
from the tick's own address, as in the reference.

The supported bounds are 1 MiB sparse backing, 65,536 entries per history,
30 service bindings, callback recursion depth 64, 4,096 breath attempts and
4,096 skill-rank pairs, 4,096 timed modifier IDs per equipment slot,
65,536 total potion-search iterations and 65,536 load-recount iterations per call.
These are native resource refusals, not original game rules. An unterminated rank list is refused with existing effects retained.
Potion kinds other than 0/1 and computed sizes outside Python's nine-element
list indexing domain explicitly refuse with `Unsupported`; the reference itself
has no usable result for such indexing. String comparison reads at most 256 bytes,
exactly as the reference (an unterminated string is compared at that length).
Timed modifier IDs, potion-name pointers and equipment/bag contents must be
supplied by the caller; no original tables or strings are embedded. Dynamic bag
counts are reread after callbacks, as in the reference. Natural mana regeneration
still invokes spell/skill queries when class 0 makes its base amount zero.
The helper-externalization option remains unported: these bodies call the existing
rank/passive helpers directly, with their own external services kept as callbacks.
Repeated helper calls retain the shared draw count and ordered histories.
The object's own breath seed and unrelated third seed remain distinct from
shared draws.

The exact Python entry takes immunity through a wrapping byte decrement then a
signed-byte negative clamp, rather than an unsigned saturated decrement. Synthetic
cases exhaust all 256 starting bytes and preserve this distinction.

## Exact validation and reproduction

`tools/generate_player_vectors.py` reads all evidence with `git show` at the
immutable commit and verifies SHA-256 before executing the pinned Python
reference. It never imports the live research files. Fresh reference executions
must agree twice for every case. The committed gzip fixture and JSON manifest
record evidence, generator, compressed and expanded hashes and category counts.
The fixture contains explicit initial pokes, service bindings with callback
return/effect/failure scripts, and command sequences.

`champions-player-trace` receives only those inputs. The Python comparison wrapper
verifies fixture integrity and checks complete observations after each command:
sorted sparse bytes including explicit zeros, memory stores, model writes,
ordered typed calls, interleaved events, shared draws and the captured record
address `R0`. Expected output never enters the native process. The ordinary refusal
case obtains the prefix by observing the original Python's first ordinary-only
store and stopping before it; it does not implement the reference suffix.

The committed corpus contains **24,384 cases**, including all **14,448 original
cases preserved byte-for-byte**, and covers all sixteen FPU configurations,
scripted states, repeated ticks, all immunity bytes, spawn/copy differences, signed counters, callback mutations
and exceptions, rumble gates, light edge patterns, breath sampling, every pad
gate, rare fidget starts/cancellation, equipment combinations, block transitions,
track lookups, signed rank pairs and 64-bit passive masks. The 9,936 added cases
cover both resources and potion kinds, all nine potion sizes and fallback paths, all 17 equipment slots, signed bonus/modifier/rank
fields, duplicate modifier expiry, repeated recovery, local inventory-message
and direct-stack branches, load sums, callback mutations and failure prefixes.
For subsequent families, preserve the current recipe bindings (19 entry/input
services, 30 recovery services) when adding callbacks; do not widen old recipes
with new default bindings. The full expanded fixture hash is
`5d58e95c8b82432ac354c277cd592f0ed5ff980f9bc76382d19dd3a48fe99704`.
The comparison wrapper independently checks the original 14,448-row expanded
SHA-256 (`74fad4e6cf7e1e71472b3a0addd19c3f196701ce4fd3857293fab2a8ab5b82ac`).
Separate host tests exercise resource/missing-service refusals, callback self-removal and recursion
recovery. Transport tests reject malformed shapes and out-of-domain memory spans.

```sh
python3 tools/generate_player_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^player-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

The input command opcodes are `0` run, `1` choose pad, `2` fidget, `3` can
block, `4` try block move, `5` has animation, `6` skill rank, `7` passive
level, `8` unlogged poke and `9` logged store. New opcodes are `10` health
regeneration, `11` mana regeneration, `12` enchantment timers (each takes a player
address), `13` potion consumption (kind), and `14` the full potion helper
(player, kind, list bag, list cell, list panel). Helper operands are their public
argument order. A poke/store uses address, width (0–8) and uint64 value. Each
callback binding is `[serviceIndex,uint32Return,effects,throwFlag]`; effects
are `[loggedFlag,address,width,uint64Value]` applied on every invocation before
an optional exception. Service indices are listed in the manifest. Callback
arguments are `[0,uint32]` or `[1,xRaw,yRaw,zRaw]`. Input parsing has explicit
shape, integer, depth and count limits, independent of fixture expectations.

## Previous subset verification

On 2026-10-13, Apple Silicon macOS with AppleClang 21.0.0 and Python 3.14.8:

- `python3 tools/generate_player_vectors.py --research-dir ../../champions-return-to-arms-re`
  passed: 14,448 cases; each executed twice freshly, with identical observations.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` passed.
- `cmake --build build/native -j 4` passed after correcting a trace-runner
  variable redeclaration found in the initial build.
- `ctest --test-dir build/native -R '^player-' --output-on-failure` passed,
  2/2, after rerunning an initial attempt made before the executables existed.
- After tightening invalid uint64 pad-address offsets,
  `cmake --build build/native --target champions-player-tests champions-player-trace -j 4`
  and the same player CTest command passed again, including new raw32-span regressions.
- `ctest --test-dir build/native --output-on-failure -j 3` passed **102/102**,
  949.54 seconds under concurrent sibling-worktree load; no test failed or timed out.
  The final player-only Release and sanitizer reruns include the pad-span fix.
- `python3 tools/check_simulation_boundary.py .` and `git diff --check` passed.

The same two player suites passed with ASan/UBSan using these exact commands:

```sh
cmake -S native -B build/player-sanitized -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/player-sanitized --target champions-player-tests champions-player-trace -j 4
ctest --test-dir build/player-sanitized -R '^player-' --output-on-failure
```

## Recovery family verification

This run uses the same immutable evidence commit and hashes above, on arm64
macOS with AppleClang 21.0.0, Python 3.12.3 for generation and Python 3.14.8
for CTest comparison. The generator command above
passed with 24,384 cases, each observed twice freshly. A separate SHA-256
comparison against the previous fixture confirmed all 14,448 original rows are
byte-for-byte identical; that check is now enforced by the comparison wrapper.
The Release configure/build commands above passed.
`ctest --test-dir build/native -R '^player-' --output-on-failure` passed 2/2
in 95.45 seconds, including 24,384 complete-state comparisons over all 16 FPU
configurations and the unchanged-original-vector hash check. The host suite includes
missing-service, invalid-kind, unterminated timed-list, oversized bag-scan and
load-recount refusals with retained state prefixes.

`ctest --test-dir build/native --output-on-failure -j 3` completed in 2626.61
seconds: 100/102 passed, including `player-exact` (60.65 seconds) and `player-host`.
Only `session-objects-transport` and `session-combat-transport` failed, each on
its existing 600-second subprocess timeout; neither reported a state mismatch.
The corresponding exact-state suites passed. The host was heavily loaded during
this run. `session-objects-transport` subsequently passed alone, 1/1 in 102.04
seconds, and `session-combat-transport` passed alone, 1/1 in 409.70 seconds.
Both retained their existing timeout unchanged. All 102 registered tests therefore
passed across the full run and these individual retries; the single full run
itself remains recorded as 100/102. The individual retry commands are:

```sh
ctest --test-dir build/native -R '^session-objects-transport$' --output-on-failure
ctest --test-dir build/native -R '^session-combat-transport$' --output-on-failure
```

The sanitizer configure/build commands above passed.
`ctest --test-dir build/player-sanitized -R '^player-' --output-on-failure`
passed 2/2 in 208.17 seconds with ASan/UBSan, including the expanded 24,384-case
corpus and host refusal tests.
An x86_64 Release build also passed 2/2 player tests in 99.39 seconds under
Rosetta on the same Apple Silicon host. `file` verified separate arm64 and x86_64
Mach-O executables. Both architectures matched the same 24,384 complete-state
observations across all 16 FPU configurations; this is an emulated x64 comparison,
not execution on physical Intel hardware or Windows.

```sh
cmake -S native -B build/player-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/player-x64 --target champions-player-tests champions-player-trace -j 4
ctest --test-dir build/player-x64 -R '^player-' --output-on-failure
```

`python3 tools/check_simulation_boundary.py .` and `git diff --check` passed.
The research checkout was read only; shared CMake, roadmap and workflow files
were not changed.

No original-runtime differential harness, private inputs, scene integration,
Norrath, physical x64 or Windows execution was used in this run.

## Exactly what remains

- Ordinary entry after scripted control: weapon trails; fidget dispatch/dialog
  turn; aura/movement/minimum-attack-time stores; potion and ability-button input;
  status mana payment; regeneration/enchantment dispatch; kill plane, damage,
  swap/interaction gates; instant abilities and block request (sections 1.11–5).
- All action-state switch bodies (states 0–14), including ranged, melee, neutral,
  hit, dead, cast, jump, block, pound, rush, war cry and vehicle behavior; their
  internal targeting, combo, projectile, legacy ability and skill paths (section 6).
- The entire ordinary tail: animation/movement bookkeeping, aura light, sound,
  countdowns and remaining world/record effects (sections 7–8).
- Modelled helpers `Player_updateCast`, `Player_weaponSwapInput` and
  `Player_castSkill`. None is replaced with invented behavior or called by this
  subset. Recovery, potion and enchantment helpers are now ported but remain
  independently callable until ordinary tick dispatch is implemented. The
  reference's selectable helper-externalization dispatch also remains
  unported; implemented helpers run directly and their external services callback.
- Real service implementations and adapters, Session/scene control integration,
  original runtime/console comparison, private-data acceptance, Norrath validation
  and physical x64/Windows validation. The arm64/Rosetta-x64 player comparison
  establishes only the pinned model subset on the executed host.
