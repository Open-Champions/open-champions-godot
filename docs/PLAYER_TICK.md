# Return to Arms player tick and gameplay helpers

PROGRESS: 59 of 94 reference functions ported

Current family: **complete neutral state**, including ability start, attack start,
and old-arrow notices.

`champions-player` is a handwritten portable C++17 module in
`native/include/champions/player.hpp` and `native/src/player.cpp`. It implements
complete scripted-control ticks, original input helpers, and the complete
health/mana recovery, potion consumption and enchantment-expiry helper family.
The casting lifecycle and weapon-set input family now covers `update_cast`,
`cast_skill`, `hands_midpoint`, `skill_of` and `weapon_swap_input`.
It is independent of Godot and does not connect original controls to Session or
the scene. The bounded action-state family now implements hit, dead, jump, rush, block,
block-move, war cry, vehicle, ground pound, and skill-controlled states as
independently callable bodies. The complete neutral state now adds idle movement,
block requests, ability-cost/start gates, legacy ability starts, attack transitions,
bow pressure, last-match ammunition scanning and close-target punch fallback.
Ordinary gameplay ticks remain unsupported.

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

## Progress accounting

The pinned Python AST contains 98 top-level functions plus class methods,
including property accessors and constructors. Excluded as harness infrastructure:
`Ext.__init__` (external-service metadata), `shape_result` (stub return shaping),
`Outcome.__init__` and `Outcome.written` (harness result reporting). This leaves
94 portable units, including all five Memory methods and the Tick constructor.
No function is removed from the denominator because this port has not reached it.

The 59 fully implemented, tested equivalents are `s16`, `s32`, `Memory.poke`,
`Memory.store`, and these Tick methods: `s8`, `s16`, `u32`, `s32`, `_w`, `vec`,
`draw`, `slot`, `R`, `is_copy`, `skill_of`, `item_modifier`, `hud_notice`,
`choose_pad`, `use_potion`, `use_potion_real`, `idle_fidget`, `can_block`,
`try_block_move`, `has_anim`, `skill_rank`, `passive_level`, `update_cast`,
`regen_health`, `regen_mana`, `tick_enchant_timers`, `weapon_swap_input`,
`hands_midpoint`, and `cast_skill`. Earlier state work added Tick's `anim_step`, `move`, `frames`,
`top_track`, `stop_loop_sound`, `clear_extra_tracks`, `slow_scale`, `haste_of`,
`anim_name`, `selected_ability`, `sound`, `unit_sphere_shared`, `state_hit`,
`state_dead`, `state_jump`, `state_rush`, `state_block_move`, `state_warcry`,
`state_vehicle`, `state_pound`, `state_skill`, and `state_block`. This run adds
`old_arrow_notices`, `state_neutral`, `neutral_start_ability`, and `neutral_attack`.
Shared scalar conversions use the already
validated core; memory pokes/stores use the existing sparse Memory. The exact
corpus exercises these operations and complete observations, not just returns.

Partial equivalents are deliberately uncounted: `run`/`top` (entry/scripted
subset only), `call`/`vcall` (closed service subset), `obj_rand` (own player only),
`cstr` (fixed default limit), and `Tick.__init__`/`helper` (no selectable helper
externalization). Width wrappers without standalone C++ entry points are also
uncounted. `Memory.load` and its constructor do not implement the reference's
optional underlying-memory delegate; they remain uncounted despite the supported
zero-backed sparse path. Ordinary refusal is never credited as a ported function.

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
| `update_cast`, `cast_skill`, `hands_midpoint`, `skill_of` | Both attach-point calls; software-double height clamp; virtual aim/can-use/enemy/rank/cast/cost/stop dispatch; first accepted arc target; real/copy spell handling, recast and mana writes |
| `weapon_swap_input` | State and block gates; force/latch paths, three-attempt set selection, equipment/secondary/notice callbacks and final damage recalculation |
| Bounded action states | Complete states 3, 4, 6/14, 7, 8, 9, 10, 11, 12, 13; exact transitions, animation/movement dispatch, water, vehicle, legacy pound/cry effects, death decal and block particles via callbacks |
| Neutral state and subpaths | Complete state 2, ability gates and costs, legacy/new-style start dispatch, melee/bow/throw transitions, punch fallback, low/no-ammunition notices, block requests, idle timer, movement and footstep events |
| Shared action helpers | Animation/move dispatch, frame subtraction, signed track addressing, loop sound stop, extra-track clearing, slow/haste scaling, animation/ability selection, sound lookup, shared unit-sphere sampling |
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
`service_name`. Arguments distinguish uint32 scalar patterns, three-word
vectors, an explicit `StackHandle` marker for the reference string `"stack"`,
and `NullArgument` for the reference `None` formatting argument. `Sprintf` remains
a service callback returning a supplied address; the port embeds no formatted
strings or original animation names.
Callbacks return `Result`: uint32, a vector, or an ordered object-address list.
Existing scalar-return lambdas remain usable. Attach-point and arc services check
their return shape; incorrect shapes throw `Error` after recording the call.
`ReferenceZero` supplies zero vectors and empty lists for these two services.
The callbacks retain original order; callback returns, memory mutations,
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
use checked raw32 spans. The `record()` accessor, potion menu-cell expression and virtual `this`
adjustment retain the reference's explicit wrapping conversions; other
negative/out-of-range memory address expressions refuse. Helpers can operate on an explicitly supplied player address distinct
from the tick's own address, as in the reference.

The supported bounds are 1 MiB sparse backing, 65,536 entries per history,
67 service bindings, callback recursion depth 64, 4,096 arc object addresses, 4,096 breath attempts and
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

## Bounded action-state boundary

Callers supply `Tick::pad` and `Tick::R0` before a state body, corresponding to
original pre-switch locals. `choose_pad()` still only returns an address.
`selected_ability()` returns an optional raw32 value, preserving Python `None`
separately from a selected zero address (which a host regression exercises).
The trace command normalizes absence to zero; the skill body does the same
explicitly, as Python's `or 0` does. Selected records retain wrapping addresses. Move's extra argument distinguishes the
scalar zero from the skill state's zero vector. Virtual vehicle/spell calls
read the signed 16-bit `this` adjustment from caller-supplied tables.
`state_block_move()` raises `Leave` immediately during recall, matching the
reference's control exit and retaining any previous history. This is distinct
from an unsupported operation. No state body runs the pre-switch path or tail.
The shared rejection sampler has the same 4,096-attempt resource cap as breath.
Signed track counts and mutable slots are reread at the same sites as Python;
block's captured `R0` and input pad remain stable across callbacks. The supplied
death double is compared through software-double arithmetic, rather than an
invented embedded death-frame constant. Strings, animation rows, ability and
skill tables, and effects remain caller-supplied synthetic state or services.

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

The committed corpus contains **87,840 cases**, including all **59,888 previous
cases preserved byte-for-byte**, and covers all sixteen FPU configurations,
scripted states, repeated ticks, all immunity bytes, spawn/copy differences, signed counters, callback mutations
and exceptions, rumble gates, light edge patterns, breath sampling, every pad
gate, rare fidget starts/cancellation, equipment combinations, block transitions,
track lookups, signed rank pairs and 64-bit passive masks. The 9,936 recovery cases
cover both resources and potion kinds, all nine potion sizes and fallback paths, all 17 equipment slots, signed bonus/modifier/rank
fields, duplicate modifier expiry, repeated recovery, local inventory-message
and direct-stack branches, load sums, callback mutations and failure prefixes.
The 9,744 casting/weapon cases additionally cover every family gate, signed
selector and skill IDs, primary/secondary sets, latch/force/fallback behavior,
virtual signed-this adjustment, repeated casts, real/copy mutations, ordered
targets, numeric aim/cost edge patterns, track caps and fade writes, callback
failures/missing handlers and alternate-player arguments. `hands_midpoint`
returns the two attach points exactly: its `limit_height` parameter is unused
in the reference; the callers perform the midpoint and height clamp.
The 27,952 neutral cases cover both selected/forced ability paths, offline
selector and level/status/recast gates, all legacy ability branches, new-style
kinds, mana/passive/cost bit patterns, signed tracks, target/punch thresholds,
copy gates, pressure short circuits, last-match ammunition scanning, timer
caps, block requests and callback failure/missing/mutation prefixes.
Trace command `[22, helper, a, b, R0, instant]` invokes neutral (helper 0, pad a),
ability start (1, ability a and cost b), attack (2, pad a and mask b), or notices
(3). Helpers 1/2 return 0/1. Argument tags 0/1/2 retain their old scalar/vector/
stack meanings; tag 3 represents None. Old snapshots and commands are unchanged.
For subsequent families, preserve the current recipe bindings (19 entry/input
services, 30 recovery services, 45 casting/weapon services, 60 action services, 67 neutral services) when adding callbacks; do not widen old recipes
with new default bindings. The full expanded fixture hash is
`1e5126fa7e007ce1e159fb96a44e650f87bfc25caef2440e072bb8cf167b6ee0`.
The 25,760 action cases cover all ten bounded bodies and twelve shared helpers,
all sixteen FPU configurations, callback failure/missing/mutation prefixes,
signed track counts/slots/flags, allocation absence, animation reuse, skill
selection and activity, water gates, recall exits, death double patterns,
all pound event combinations and repeated block particle generation.
The comparison wrapper independently checks all 59,888 previous rows against
SHA-256 `1c28ce889847e6504996f319f2626992c96cb77f894882fa0a8b79ed02d1e4d3`,
and all 34,128 previous rows against
SHA-256 `7ab0008239aa8ec2d346fc8605d16f7e70e9edac5260cbe1b806ba50058ad0d6`.
It also independently checks the previous 24,384-row expanded
SHA-256 (`5d58e95c8b82432ac354c277cd592f0ed5ff980f9bc76382d19dd3a48fe99704`)
and the original 14,448-row expanded
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
(player, kind, list bag, list cell, list panel). New opcodes: `15` update cast (player, pad), `16` weapon swap
(player, pad, force), `17` cast skill (player, ability), `18` hand points
(player, 0/1 limit flag; returns two vectors), and `19` skill lookup (ability).
Opcode `20` takes `(stateIndex, pad, capturedRecord)`, selecting hit/dead/jump/
rush/block-move/warcry/vehicle/pound/skill/block at indices 0–9. Opcode `21`
takes `(helperIndex, a, b)` for animation step/move/frames/top-track/stop-loop/
clear-tracks/slow/haste/animation-name/selected-ability/sound/unit-sphere at
indices 0–11; unused operands are zero. Signed offsets use int32 patterns.
These transport indices are authored test commands, not original state IDs.
Helper operands are their public argument order. A poke/store uses address, width (0–8) and uint64 value. Each
callback binding is `[serviceIndex,returnSpec,effects,throwFlag]`; effects
are `[loggedFlag,address,width,uint64Value]` applied on every invocation before
an optional exception. Service indices are listed in the manifest. Callback
arguments are `[0,uint32]`, `[1,xRaw,yRaw,zRaw]` or `[2,0]` (stack handle).
Callback returns retain the original scalar format; vector results are
`[1,xRaw,yRaw,zRaw]`, lists are `[2,[objectAddresses]]`, and a scripted result
sequence is `[3,[results]]` (at most 128 results, then repeats its last result).
Sequence cursors have stable shared state across copied callback invocations;
effects/failure flags still apply on every invocation. Input parsing has explicit
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

## Casting and weapon family verification

On 2026-10-13, arm64 macOS with AppleClang 21.0.0, Python 3.12.3 for
generation and Python 3.14.8 for CTest: the generator command above passed
with 34,128 cases, each executed twice freshly. The Release configure and full
build commands above passed. The player CTest command passed 2/2 in 133.11
seconds on arm64. The x86_64 configure/build/player CTest commands above also
passed, 2/2 in 163.39 seconds under Rosetta. `file` confirmed arm64 and x86_64
Mach-O executables. Both matched the same 34,128 complete-state observations
across all 16 FPU configurations. The comparator independently confirmed all
24,384 previous rows and all 14,448 original rows byte-for-byte unchanged.
This is an emulated x64 comparison, not physical Intel or Windows evidence.

Host tests passed for missing/incorrect vector returns, attach callback
self-replacement, oversized arc lists, invalid pad spans and oversized track
indices with retained call prefixes. The first one-configuration smoke check
passed all 609 new recipes after normalizing Python tuples to their JSON
transport representation; this required no implementation correction.
`python3 tools/check_simulation_boundary.py .` and `git diff --check` passed.
`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102**
in 1937.06 seconds, with no timeout or retry. This includes the final player
host and exact suites alongside the existing parser, simulation, transport
and Session checks.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`,
and this page. The evidence is the same pinned research commit and SHA-256
above; no research files or shared build/roadmap/workflow files were edited. No original-runtime harness, private inputs, scene integration,
Norrath, physical x64 or Windows execution is claimed.

## Bounded action-state family verification

On 2026-10-13, arm64 macOS: the same pinned generator command above passed
with **59,888** cases, each executed twice freshly. Generation used Python
3.12.3; CTest uses Python 3.14.8 and AppleClang 21.0.0. It contains **25,760** new
cases for the ten state bodies and twelve shared helpers. The Release configure
and full build commands above passed. The arm64 player CTest command passed
**2/2 in 198.61 seconds**, including all sixteen FPU configurations and all
three historical vector hashes (14,448, 24,384 and 34,128 unchanged rows).
The x64 player CTest command above passed **2/2 in 116.82 seconds** under
Rosetta. `file build/native/champions-player-trace
build/player-x64/champions-player-trace` confirmed distinct arm64 and x86_64
Mach-O binaries. Both architectures matched the same 59,888 observations;
this establishes an emulated x64 comparison, not physical Intel or Windows.
A first-configuration smoke comparison passed all 1,516 initial action recipes;
an ASan/UBSan execution of that same smoke also passed. Generation was restarted
after adding the final gate cases, giving 1,610 action recipes in the final
corpus. The generator's ordinary-boundary observer now applies only during
`run()`, allowing independently invoked extra-track clearing to write through
the same address without an artificial harness exit; historical hashes confirm
this does not change earlier rows.

The sanitizer and x64 configure/build commands recorded above passed. Direct
`build/native/champions-player-tests`,
`build/player-x64/champions-player-tests`, and
`build/player-sanitized/champions-player-tests` passed after the final API change.
Host regressions distinguish a selected zero address from absent ability,
verify callback self-removal and failure prefixes, refuse extreme signed field
offsets and invalid pads, and preserve pre-attach death aura writes on malformed
callback results. The ordinary tick still refuses at its previous boundary.
`python3 tools/check_simulation_boundary.py .` and `git diff --check` passed.

`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102**
in **645.36 seconds**, with no failure, timeout or retry. This includes the
expanded `player-exact` (80.22 seconds) and `player-host` alongside existing
native parsers, simulation, transport and Session checks. The isolated arm64
and x64 timings above remain separate observations under differing host load.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`,
and this page. The research repository and shared CMake/roadmap/workflow files
were not edited. No private input, original-runtime harness, Session/scene
integration, Norrath, physical Intel or Windows execution is claimed.

## Neutral-state boundary

`state_neutral` and its three subpaths implement the entire pinned state-2 body
and section 6's neutral description. Callers supply `pad`, `R0` (the captured
record from ordinary entry), and `instant` (the pre-switch instant-input word).
They do not select a pad or compute instant abilities. `neutral_start_ability`
and `neutral_attack` return whether the state body exits immediately; this is
independent of `run()` and never bypasses its ordinary-tick refusal.

The new services are `text_lookup`, `hud_showMessage`, `libc_sprintf`,
`Player_findTarget`, `Player.v26`, `str_prefixDiffers`, and `pad_buttonValue`.
Target acquisition, turning, text, prefix comparison and pressure reads remain
callbacks. The port preserves virtual this-adjustments, captured versus current
record reads, callback short circuits, own-player RNG use, and failure prefixes.
The no-combat flag produces notices but does not veto every attack path in the
reference; the port preserves that behavior. Ammunition scanning visits exactly
64 authored bag cells and retains every matching store, selecting the last match.
The low-ammunition helper uses unsigned count-minus-one; the punch fallback uses
signed count, as the reference does. Copy gates remain specific to each branch.

## Neutral-state verification

On 2026-10-13, arm64 macOS with AppleClang 21.0.0: the pinned generator command
recorded above passed with **87,840** cases, including **27,952** new cases from
1,747 neutral recipes across all sixteen FPU configurations. Every case was
executed twice freshly in Python. Generation used Python 3.12.3; CTest uses
Python 3.14.8. The Release configure and full build commands above passed.
The arm64 player CTest command passed **2/2 in 119.88 seconds**. The comparator
confirmed all 59,888 preceding rows, and the earlier 34,128, 24,384 and 14,448
subsets, byte-for-byte unchanged using their independent frozen SHA-256 values.

The x64 configure/build and player CTest commands recorded above passed,
**2/2 in 122.20 seconds** under Rosetta. `file build/native/champions-player-trace
build/player-x64/champions-player-trace` confirmed distinct arm64 and x86_64
Mach-O binaries. Both matched all 87,840 complete-state observations across the
same sixteen FPU configurations. This is an emulated x64 comparison, not physical
Intel or Windows execution.

The one-configuration neutral smoke command `python3 work/smoke_neutral.py`
passed all 1,747 recipes. The same smoke passed with the executable argument
`build/player-sanitized/champions-player-trace`. The sanitizer configure/build
commands recorded above passed, and direct
`build/player-sanitized/champions-player-tests` passed after all host regressions
were added. This sanitizer evidence covers the new first-configuration recipes
and host policies; no full expanded sanitizer corpus run is claimed.
Host checks preserve missing-service/malformed-result prefixes, distinguish
formatting None from stack and address zero, verify formatting self-removal,
refuse invalid captured record/pad spans, preserve copy arrow-selection stores,
and confirm the final ammunition match and no-ammunition notice.
An initial host-test failure exposed a test setup whose forced timed ability
short-circuited the bad pad read; setting it to the non-timed path corrected the
fixture without changing the implementation.

`ctest --test-dir build/native --output-on-failure -j 3` passed **102/102 in
556.62 seconds**, with no failure, timeout or retry. The final `player-exact`
passed in 55.91 seconds and `player-host` in 0.03 seconds under this run's host
load; the earlier isolated arm64 and x64 timings remain separate observations.
`python3 tools/check_simulation_boundary.py .` and `git diff --check` passed.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`,
and this page. The evidence commit and hashes are unchanged. The research
checkout and shared CMake/roadmap/workflow files were not edited. No private
input, original-runtime harness, Session/scene integration, Norrath, physical
Intel or Windows execution is claimed.

## Exactly what remains

The 35 uncounted units are top-level `s8`; `Memory.__init__`, `Memory.load`,
`Memory.write_bytes`; and Tick's `__init__`, `u8`, `u16`, `u64`, `w8`, `w16`,
`w32`, `cstr`, `strcmp`, `strstr`, `call`, `vcall`, `helper`, `draw3`, `obj_rand`,
`run`, `top`, `instant_abilities`, `tail`, `switch`, `state_melee`,
`melee_next_swing`, `state_cast`, `light_effect`, `cast_old_style`, `old_fire_nova`,
`aim_at_target`, `state_ranged`, `ranged_throw`, `ranged_shoot`, and `missile`.
The partial-equivalent exclusions above still apply. Start the next run with
ordinary pre-switch input or the complete melee/combo-state family; both have pinned
portable evidence. Do not call the ordinary tick complete until pre-switch,
all states, and tail are implemented and tested together.

- Ordinary entry after scripted control: weapon trails; fidget dispatch/dialog
  turn; aura/movement/minimum-attack-time stores; potion and ability-button input;
  status mana payment; regeneration/enchantment dispatch; kill plane, damage,
  swap/interaction gates; instant abilities and block request (sections 1.11–5).
- Action-state dispatcher `switch` and the three remaining state bodies:
  ranged (`state_ranged`, `ranged_throw`, `ranged_shoot`, `missile`), melee
  (`state_melee`, `melee_next_swing`), and cast
  (`state_cast`, `light_effect`, `cast_old_style`, `old_fire_nova`,
  `aim_at_target`). Skill-controlled states 6/14 share the already ported body;
  eleven bounded state methods are complete and callable independently.
- The entire ordinary tail: animation/movement bookkeeping, aura light, sound,
  countdowns and remaining world/record effects (sections 7–8).
- The ported gameplay helpers are independently callable. Main-loop
  dispatch remains work: casting, weapon swap, recovery, potion and enchantment
  availability does not imply ordinary ticks execute them. The
  reference's selectable helper-externalization dispatch also remains
  unported; implemented helpers run directly and their external services callback.
- Remaining accessors/utility units in the pinned inventory, including the
  arbitrary-object and third RNGs, width wrappers, string utilities and the
  legacy/ranged/melee helpers listed above; full callback return-shape coverage,
  optional underlying Memory and selectable helper externalization.
- Real service implementations and adapters, Session/scene control integration,
  original runtime/console comparison, private-data acceptance, Norrath validation
  and physical x64/Windows validation. The arm64/Rosetta-x64 player comparison
  establishes only the pinned model subset on the executed host.
