# Return to Arms player tick and gameplay helpers

PROGRESS: 87 of 94 reference functions ported

Current family: **complete scalar-width access and bounded string reads**,
top-level `s8` and Tick's `u8`, `u16`, `u64`, `w8`, `w16`, `w32`, and
configurable `cstr`. All eight bodies extend the existing player module.

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
The complete melee state and next-swing body now run independently with exact
callback boundaries. The ranged state, both release paths and missile creation
are also complete standalone bodies. The final casting state and action-state
dispatcher and ordinary pre-switch are now complete independently callable bodies.
Ordinary gameplay `run()` now executes the complete pinned tick model through
explicit service callbacks; this is not scene integration.

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

The 87 fully implemented, tested equivalents are `s16`, `s32`, `Memory.poke`,
`Memory.store`, and these Tick methods: `s8`, `s16`, `u32`, `s32`, `_w`, `vec`,
`draw`, `slot`, `R`, `is_copy`, `skill_of`, `item_modifier`, `hud_notice`,
`choose_pad`, `use_potion`, `use_potion_real`, `idle_fidget`, `can_block`,
`try_block_move`, `has_anim`, `skill_rank`, `passive_level`, `update_cast`,
`regen_health`, `regen_mana`, `tick_enchant_timers`, `weapon_swap_input`,
`hands_midpoint`, and `cast_skill`. Earlier state work added Tick's `anim_step`, `move`, `frames`,
`top_track`, `stop_loop_sound`, `clear_extra_tracks`, `slow_scale`, `haste_of`,
`anim_name`, `selected_ability`, `sound`, `unit_sphere_shared`, `state_hit`,
`state_dead`, `state_jump`, `state_rush`, `state_block_move`, `state_warcry`,
`state_vehicle`, `state_pound`, `state_skill`, and `state_block`. Neutral work added
`old_arrow_notices`, `state_neutral`, `neutral_start_ability`, and `neutral_attack`.
Melee work adds `state_melee`, `melee_next_swing`, and `draw3`.
Ranged work adds `state_ranged`, `ranged_throw`, `ranged_shoot`, `missile`,
`aim_at_target`, `obj_rand`, and `strstr`. Casting/dispatch work adds
`state_cast`, `cast_old_style`, `light_effect`, `old_fire_nova`, `strcmp`, and
`switch` (native entry `dispatch_state`). Pre-switch work adds `top` and
`instant_abilities`. Full-tick work adds `tail` and `run`. Scalar/string work
adds top-level `s8` and Tick's `u8`, `u16`, `u64`, `w8`, `w16`, `w32`, and `cstr`.
Shared scalar conversions use the already
validated core; memory pokes/stores use the existing sparse Memory. The exact
corpus exercises these operations and complete observations, not just returns.

Partial equivalents are deliberately uncounted: `call`/`vcall` (closed service
subset) and `Tick.__init__`/`helper` (no selectable helper externalization).
`Memory.load` and its constructor do not implement the reference's
optional underlying-memory delegate; they remain uncounted despite the supported
zero-backed sparse path. Ordinary refusal is never credited as a ported function.

## Implemented boundary

| Entry point | Coverage |
|---|---|
| `Tick::run` | Complete ordered top, action-state switch and tail; scripted, recall and invalid-ability-index Leaves return their exact reasons |
| `Tick::run_legacy_entry` | Historical section-1 entry and scripted return, with ordinary refusal before `+0x4c4`; frozen command 0 |
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
| Melee state and combo transition | Complete state 1, spell/haste/slow speed, target scoring, sweep iterator, offline/network timing, instant-skill and block gates, end/track writes; independent third RNG |
| Ranged state and projectile releases | Complete state 0, haste/slow/net timing, bow and throw releases, target lead and virtual turn, ordered draws, projectile callbacks, ammunition removal and rereads; arbitrary-object RNG and binary substring checks |
| Casting state, legacy effects and dispatch | Complete state 5 and all fifteen action-state mappings; phase/cast-point/held-cost gates, held/timed/single-use legacy bodies, root/unholy/light constructors and 219-particle fire nova via callbacks; binary bounded string equality |
| Ordinary pre-switch | Complete `top`, including entry/scripted Leave, weapon trails, chosen pad/fidget, dialog turn, aura clamps, attack interval, potions, ability selection, status mana, recovery/expiry, kill plane, damage, swap/interaction, instant abilities and block request |
| Instant abilities | Complete kind-7 selection, forced/button order, movement/recast/name/can-use/rank/cost gates, virtual adjustment and post-callback record/button rereads |
| Scalar and string utilities | Complete low-byte `s8`, width reads and logged stores; binary `cstr` with caller-selected signed limit, default 256, first-NUL termination and no reads for nonpositive limits |
| Shared action helpers | Animation/move dispatch, frame subtraction, signed track addressing, loop sound stop, extra-track clearing, slow/haste scaling, animation/ability selection, sound lookup, shared unit-sphere sampling |
| `has_anim`, `skill_rank`, `passive_level` | Complete reference lookup bodies, including signed fields, terminator behavior, first match and slot-4 acceptance |

The historical `run_legacy_entry()` executes the supported entry then throws `Unsupported` at
`0x1a5614`, before the `P+0x4c4` store, weapon trails, pad selection and remaining
pre-switch work. Entry writes and callback effects survive that refusal. Calling
it again repeats the entry; this is not a successful partial gameplay tick.
`run_legacy_entry()` returns `"scripted"` only after the complete scripted early return.
The primary `run()` now executes the entire model and returns `"end"` or the
exact control-exit reason. Non-Leave failures propagate with their prefixes.
The helper lookups also remain independently callable.

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
95 service bindings, callback recursion depth 64, 4,096 arc object addresses, 4,096 breath attempts and
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
from an unsupported operation. No state body runs the pre-switch path or tail. `top()` supplies `pad`, `R0`,
`weapon0` and `instant`, but does not itself dispatch a state or run the tail.
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

The committed corpus contains **231,744 cases**, including all **218,336 previous
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
independent of full `run()`; the frozen ordinary-entry refusal is retained by
`run_legacy_entry()`.

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

## Melee/combo boundary

`state_melee` implements the complete pinned state-1 body and section 6's melee
contract; `melee_next_swing` implements its entire combo transition. Callers
supply `pad` and `instant`; `run_legacy_entry()` retains its existing ordinary refusal
boundary. The record and input captured on entry stay distinct from later slot
and record rereads. Speed preserves the ordered max spell factor, item haste,
berserker callback and slow scaling. Target selection keeps later ties, PvP
fallback, class-2 sweep links and Creature virtual this-adjustments. Network hits,
local/copy sound gates and wraparound frame differences follow the reference.
`Player_pickAttackAnim` remains an explicit callback, including its delegated
random behavior; implementing the state does not implement that service.

Thirteen appended services (indices 67–79 in the manifest) cover ancestral and
battle-cry types/queries, berserker factor, box collection, Creature slot 12,
sweep iteration, melee hit/removal and attack-animation selection. Query returns
use `QueryResult{accepted, rawFactor}` and transport `[4,accepted,rawFactor]`;
box returns use the existing list shape. Malformed returns refuse after recording
the call and its side effects. ReferenceZero explicitly supplies empty query pairs
and lists. Strict missing-handler policy remains the default. Bounds: 65,536
alive-player iterations, 4,096 box objects and 65,536 sweep iterations, plus the
existing history limits; over-limit inputs refuse without rollback.

The new authored transport opcode 23 is `[23,helper,a,b,R0,instant]`:
helper 0 supplies pad `a` and calls the state; helper 1 calls next-swing with
record `a`, signed steps `b` and start frame `R0`; helper 2 draws the third RNG.
`draw3` advances only 0x404468, with no shared-draw count change. All earlier
transport indices retain their meaning.

## Melee/combo verification

On 2026-10-13, arm64 macOS with AppleClang 21.0.0: the same immutable
reference commit and hashes above produced **119,888** complete-state cases,
including **32,048** new cases from 2,003 melee recipes over all sixteen FPU
configurations. Each Python case ran twice freshly; generation used Python
3.12.3 and CTest comparison uses Python 3.14.8. An AST recount of the pinned
file independently confirmed 94 portable units with the documented harness
exclusions. The three newly counted units are fully implemented and tested;
the remaining 32 include every previous partial equivalent.

The generator command and Release configure/full-build commands recorded above
passed. The initial smoke exposed unsigned fixture addresses produced by negative
start frames, then the old 67-binding host cap; masking frame patterns and
extending the cap to the appended 80-service set resolved those failures.
Final `python3 work/smoke_melee.py` passed all 2,003 first-configuration recipes.
`python3 work/smoke_melee.py build/player-sanitized/champions-player-trace`
also passed all 2,003 with ASan/UBSan. Every new callback has success,
missing-handler and injected-failure observations. Coverage includes all weapon
classes, signed combo wrapping/clamping, heavy attacks, haste and spell speed,
zero-speed slow thresholds, berserker factors, PvP target filtering and later ties,
virtual this-adjustments, sweep iteration, net/local/copy gates, third-RNG sound,
instant skill casts, blocking, track fades and callback mutations of captured
versus current record/slot/input state.

The x64 and sanitizer configure/build commands recorded above passed.
Direct `build/native/champions-player-tests`,
`build/player-x64/champions-player-tests`, and
`build/player-sanitized/champions-player-tests` passed. Host regressions cover
malformed pairs/lists, missing handlers, callback self-removal, invalid captured
record spans, oversized box/alive lists and nonterminating sweep iteration,
retaining the reference's failure prefixes. `file build/native/champions-player-trace
build/player-x64/champions-player-trace` confirmed arm64 and x86_64 executables.
Sanitizer evidence covers the new first-configuration recipes and host policies;
a full sixteen-configuration sanitizer run is not claimed.

`ctest --test-dir build/native -R '^player-' --output-on-failure` passed
**2/2 in 151.64 seconds**. All 119,888 observations matched, and all five frozen
historical subsets (14,448, 24,384, 34,128, 59,888 and 87,840 rows) remained
byte-for-byte unchanged.

`ctest --test-dir build/player-x64 -R '^player-' --output-on-failure` passed
**2/2 in 121.03 seconds** under Rosetta. Both architectures matched the same
119,888 complete-state observations over all sixteen FPU configurations. This
is an emulated x64 comparison, not physical Intel or Windows execution.

The inherited melee work is included in the full-suite ranged verification below.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`,
and this page. No research, shared build, roadmap or workflow file was edited.
No private data, original runtime/console harness, Session/scene integration,
Norrath, physical x64 or Windows validation is claimed.

## Ranged/projectile boundary

`state_ranged` is the complete state-0 body. Callers explicitly supply `pad` and
`R0`; the original ordinary refusal remains in `run_legacy_entry()`. Its subordinate releases
`ranged_throw`, `ranged_shoot` and `missile` are callable independently. Target
acquisition and turning stay callbacks; `aim_at_target` implements only the
reference's lead calculation, including target rereads after turn and passive
lookup, optional speed divisor and the intentionally unused scale argument.
`obj_rand` accepts an optional arbitrary raw32 object (explicit zero differs
from absence) and preserves the independent object seed. `strstr` uses the
reference's bounded, binary substring semantics, including empty needles.

Eight appended services (indices 80–87) cover load recalculation, resource
indices, player-projectile creation, modified-field query, script hook, Skill
slot 10, damage-range query and missile creation. Each resource index is queried
separately; projectile and missile construction remains delegated. No constructor
behavior or original strings/models are invented. Throw attach-point calls require
`AttachResult` (position and direction); bow calls require `Vec3`. The transport
encodes the pair as `[5,[x,y,z],[dx,dy,dz]]`. `ReferenceZero` supplies the pair only
when direction output is requested. All earlier callback/command indices retain
their meaning. New command 24 is `[24,helper,a,b,extra,R0]`: helpers 0–6 are state,
throw, shoot, missile (vectors read from `a`/`b`, signed facing `extra`), aiming
(optional divisor `a`, presence flag `b`, unused scale `extra`), object RNG
(optional object `a`, presence flag `b`), and substring lookup (`a`/`b`).

The port retains release equality, network integer halving, haste before slow,
shared draws whose values are discarded with a target, pitch clamp and target
reaction, current versus captured records, signed ammunition arithmetic, callback
count/slot/kind/copy rereads, and distinct removal-loop break behavior for bow
and throw. Stack clearing occurs only at the successful bow tail; failures
retain calls, stores and draw prefixes without rollback. Native bounds and
strict callback policy remain explicit host refusals.

## Ranged/projectile verification

On 2026-10-13, arm64 macOS: the pinned generator produced **145,840** exact
complete-state cases, including **25,952** new cases from 1,622 ranged recipes
across all sixteen FPU configurations. Every Python case ran twice freshly.
Generation used Python 3.12.3; CTest comparison uses Python 3.14.8. An AST recount
confirmed 98 units minus the same four harness exclusions = 94 portable units.
At that checkpoint, seven newly completed units brought the tested inventory
to 69; 25 remained uncounted, including all previous partial equivalents.

The Release configure and full build passed. Arm64 player CTest passed **2/2
in 84.77 seconds**; x86_64 player CTest passed **2/2 in 115.54 seconds** under
Rosetta. Both matched all 145,840 observations, and all six historical subsets
(14,448, 24,384, 34,128, 59,888, 87,840 and 119,888 rows) retained their independent
frozen hashes byte-for-byte. `file` confirmed distinct arm64 and x86_64 Mach-O
executables. This is an emulated x64 comparison, not physical Intel or Windows.

The new one-configuration smoke passed all 1,622 recipes on the Release and
ASan/UBSan executables. Sanitizer configure/build and direct host tests passed;
no full sixteen-configuration sanitizer run is claimed. Host tests cover incorrect
attach shapes, oversized/malformed arc lists, bad pad/record/object spans,
constructor failure effects, callback self-removal, explicit object zero,
binary strings and empty needles. Tests retain failure prefixes and the prior
ordinary-tick refusal. Exact commands (all passed):

```sh
python3 tools/generate_player_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-player-tests
ctest --test-dir build/native -R '^player-' --output-on-failure
cmake -S native -B build/player-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/player-x64 --target champions-player-tests champions-player-trace -j 4
build/player-x64/champions-player-tests
ctest --test-dir build/player-x64 -R '^player-' --output-on-failure
cmake -S native -B build/player-sanitized -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/player-sanitized --target champions-player-tests champions-player-trace -j 4
build/player-sanitized/champions-player-tests
python3 work/smoke_ranged.py
python3 work/smoke_ranged.py build/player-sanitized/champions-player-trace
file build/native/champions-player-trace build/player-x64/champions-player-trace
python3 tools/check_simulation_boundary.py .
python3 -m py_compile tools/generate_player_vectors.py tools/test_player_vectors.py
git diff --check
ctest --test-dir build/native --output-on-failure -j 3
```

The full native suite passed **102/102 in 436.53 seconds**, with no failure,
timeout or retry. Its final player exact suite passed in 98.28 seconds and
player host in 0.04 seconds. `git diff --check`, Python compilation and the
simulation boundary check also passed. The first smoke had a generator-local variable named
`copy` shadowing the Python module; renaming the local corrected the fixture
builder before successful generation. No C++ mismatch was found in either smoke.
The inherited uncommitted melee family was preserved and is included in these
complete-corpus comparisons and final native suite.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`,
and this page. Research files and shared build/roadmap/workflow files were not
edited. No private inputs, original runtime/console harness, Session/scene
integration, Norrath, physical x64 or Windows validation is claimed.

## Casting/legacy/dispatch boundary

`state_cast` implements the final state-5 body. `cast_old_style` covers all three
legacy kinds, including null ability, held-button release, timed animation exit,
single-use root/fire-nova/unholy effects and status-counter toggles. Copy flags
are intentionally ignored wherever the reference ignores them. `light_effect`
retains allocation-before-position reads and pads fewer than eight colour words;
more than eight remain distinct supplied arguments. `old_fire_nova` lazily builds
the particle definition and emits every one of the 219 angle samples through the
existing integer-pattern math core, then sound, light and passive-level damage.
No constructor, particle renderer or area-damage implementation is invented.

`dispatch_state` implements Python `switch`, selecting exactly states 0–14
(including the shared 6/14 body); unknown states are no-ops. Callers still supply
`pad`, `R0` and `instant`. Dispatch does not run entry, pre-switch or tail. All
fifteen action states dispatch to complete standalone bodies. The full-tick
family now integrates them in `run()`; the historical refusal remains in
`run_legacy_entry()` and command 0.
`strcmp` returns only 0/1 for bounded binary strings, exactly like the reference.

Services 88/89 append `Root_ctor` and `UnholyAura_ctor`; all earlier indices
are unchanged. Command 25 is `[25,helper,a,b,extra,R0]`: helpers 0–5 call the
cast state (pad `a`), old-style body (ability `a`, pad `b`), light (height `a`,
colour array `b`, length `extra`, owner zero), nova (ability `a`), string equality
(addresses `a`/`b`), and dispatcher (pad `a`, instant `extra`). The public light
API also accepts a supplied owner. More than 64 colour words refuse before any
allocation; this is a host resource bound. Missing services, malformed results,
callback self-removal, synchronous effects and exceptions retain their prefixes.
Negative track indices use checked raw32 expressions; the reference's invalid
phase-1 track access is refused after its animation/movement/query calls, rather
than repaired or clamped. Skill-kind 6 retains its short circuit around that read.

## Casting/legacy/dispatch verification

On 2026-10-13, arm64 macOS with AppleClang 21.0.0, an AST recount of the
immutable pinned file confirmed 98 units minus the same four harness exclusions
= 94 portable functions. The six complete additions bring the inventory to 75;
all 19 partial or missing equivalents remain uncounted. The inherited uncommitted
casting/legacy implementation and synthetic recipes were reviewed against that
contract and completed with frozen-baseline comparison, transport checks and docs.

The new corpus contains **162,912 complete-state cases**, including **17,072**
new cases from 1,067 recipes over all sixteen FPU configurations. Each generated
Python case executes twice freshly. Generation uses Python 3.12.3; CTest uses
Python 3.14.8. Seven independent historical hashes freeze the 14,448, 24,384,
34,128, 59,888, 87,840, 119,888 and 145,840 preceding subsets.

Both Release builds and direct host tests passed. The new first-configuration
smoke passed all 1,067 recipes on Release and ASan/UBSan. Sanitizer configure,
build and direct host checks passed without a finding; no full sixteen-mode
sanitizer corpus is claimed. Cases cover signed/unsigned phases, cast-point and
track-weight floating patterns, held mana, legacy status/effect ids, allocation
failure, binary/unterminated strings, null/selected/captured abilities, copy/net
gates, callback mutations and all reached service failure prefixes. Independent
host tests also cover wrong shapes, self-removing callbacks, invalid track reads,
light owner/argument bounds and partial nova failure. Dispatcher vectors exercise
every supported mapping and unknown no-ops; direct-vs-dispatch host comparisons
preserve effects. The arm64 player suite passed 2/2 in 120.27 seconds (exact 120.21, host 0.06).
All 162,912 observations and seven historical frozen subsets matched. The final
x86_64 player suite passed 2/2 in 124.05 seconds (exact 123.87, host 0.17)
under Rosetta, matching the same complete-state corpus across all sixteen FPU
configurations. `file` confirmed distinct arm64 and x86_64 Mach-O executables.
This is emulated x64 validation, not physical Intel or Windows. Fresh generation
reproduced the recorded expanded hash exactly.

The full native suite passed **102/102 in 494.68 seconds**, without failure,
timeout or retry. Its player-exact test passed in 129.21 seconds and player-host
in 0.03 seconds. All commands below passed. Simulation-boundary validation,
Python compilation and `git diff --check` also passed. Private-input/report tests
exercise the public tooling; their success is not private-file acceptance.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`, and
this page.

Exact commands:

```sh
python3 tools/generate_player_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-player-tests
python3 work/smoke_legacycast.py
ctest --test-dir build/native -R '^player-' --output-on-failure
cmake -S native -B build/player-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/player-x64 --target champions-player-tests champions-player-trace -j 4
build/player-x64/champions-player-tests
ctest --test-dir build/player-x64 -R '^player-' --output-on-failure
cmake -S native -B build/player-sanitized -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/player-sanitized --target champions-player-tests champions-player-trace -j 4
build/player-sanitized/champions-player-tests
python3 work/smoke_legacycast.py build/player-sanitized/champions-player-trace
file build/native/champions-player-trace build/player-x64/champions-player-trace
python3 -m py_compile tools/generate_player_vectors.py tools/test_player_vectors.py
python3 tools/check_simulation_boundary.py .
git diff --check
ctest --test-dir build/native --output-on-failure -j 3
```

The research checkout was not edited. No shared build, roadmap or workflow file
changed. Private inputs are unavailable at `../../private-validation`, so private
acceptance measurements were skipped. No original runtime/console comparison,
Session/scene integration, Norrath, physical Intel or Windows execution is claimed.

## Ordinary pre-switch boundary

`top()` implements sections 1–5 from the pinned reference, including the entire
entry and scripted early exit. Its ordinary path continues through the block
request at `0x1a5e7c`; it does not call the action-state switch or tail. Scripted
control throws `Leave("scripted")` after its original writes. `instant_abilities()`
also runs independently with a supplied `pad`. Both preserve the reference's
captured versus reread addresses, signed field reads, raw32 wrapping at explicitly
wrapped ability selection, float operation order, short circuits and callbacks.
The HP record stays captured from entry even when services change the current
slot. The chosen input pad is a local for the pre-switch body. Recast decrement
happens at entry, so a starting counter of one permits instant selection later.

`run_legacy_entry()` deliberately retains the historical ordinary refusal at
`0x1a5614` and complete scripted result, preserving every existing vector. The
full-tick family now integrates `top`, switch and tail in the primary `run()`.
Independent `top()` still throws `Leave` and does not itself complete a tick.
The supported default uses the implemented helper bodies. Selectable helper
externalization is still an explicit unported option, as already recorded for
`Tick.__init__` and `helper`; these additions do not credit either unit. All
services delegated by the reference remain callbacks; no trails, damage,
interaction, dialog or ability engine implementation is supplied.

Service indices 90–93 append `Player_weaponTrails`, `dialog_22fbd0`,
`Player_processDamage`, and `Player_interact`; all earlier indices remain fixed.
Trace command `[26]` calls `top` and returns `[pad,instant,weapon0]` on success;
`[27,pad]` calls `instant_abilities` and returns `instant`; `[28]` observes all
three locals, including after failures or Leave. The existing full memory,
write, call, interleaved event, draw and `R0` observation format is unchanged.

## Ordinary pre-switch verification

The pinned AST recount confirms 98 units minus the same four harness exclusions
= 94 portable functions. Completing `top` and `instant_abilities` brings the
fully ported and tested inventory to 77; 17 missing/partial units remain uncounted.
The corpus adds 902 synthetic recipes over all sixteen FPU configurations:
14,432 new cases, 177,344 total. Each reference case executes twice freshly.
Eight independent historical hashes protect all 162,912 previous rows, including
the unchanged ordinary `run` refusal. The generator uses only `git show` copies
at the immutable commit, with SHA-256 verification.

The Release builds and direct host tests pass on arm64 and Rosetta x86_64.
The new first-configuration smoke matches all 902 recipes on arm64, Rosetta
x86_64 and ASan/UBSan; the sanitizer host test also passes without a finding.
No full sixteen-configuration sanitizer corpus is claimed. Tests cover aura and
kill-plane raw patterns, signed levels and weapon indices, dialog turn, both
ability buttons, potion order and authored name matching, pending regeneration
and modifier expiry, copy/combat/state gates, swap/block/interaction, forced
instant selection and callback-adjusted virtual dispatch. Every service reached
by the richer pre-switch scenarios has missing-handler and synchronous-effect
exception prefix cases. Host tests retain malformed result effects, invalid pad
refusals, scripted Leave, callback self-removal and the old run refusal.

The arm64 player CTest passes 2/2 in 69.56 seconds (exact 69.49, host 0.07).
The Rosetta x86_64 player CTest passes 2/2 in 95.55 seconds (exact 95.43,
host 0.12). Both match all 177,344 complete observations across sixteen FPU
configurations, including all eight historical hashes. `file` confirms distinct
arm64 and x86_64 Mach-O trace executables. Release configure/build, fixture
generation, Python compilation, simulation-boundary validation and
`git diff --check` all pass. The expanded fixture hash is
`cbab5e308a11d633743656df79a7acb0e759bdaf9d07c92be18e7635e4df3cc4`.

The final full native suite passes **102/102 in 379.79 seconds**, without
failure, timeout or retry. Its player-exact check passes in 94.30 seconds and
player-host in 0.03 seconds. Private-input/report cases exercise public synthetic
tooling; their success does not establish private-file acceptance. All exact
commands below pass after the scratch-harness normalization described below.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`, and
this page.

Exact commands (run from this worktree):

```sh
python3 tools/generate_player_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-player-tests
python3 work/smoke_preswitch.py
ctest --test-dir build/native -R '^player-' --output-on-failure
cmake -S native -B build/player-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/player-x64 --target champions-player-tests champions-player-trace -j 4
build/player-x64/champions-player-tests
python3 work/smoke_preswitch.py build/player-x64/champions-player-trace
ctest --test-dir build/player-x64 -R '^player-' --output-on-failure
cmake -S native -B build/player-sanitized -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/player-sanitized --target champions-player-tests champions-player-trace -j 4
build/player-sanitized/champions-player-tests
python3 work/smoke_preswitch.py build/player-sanitized/champions-player-trace
file build/native/champions-player-trace build/player-x64/champions-player-trace
python3 -m py_compile tools/generate_player_vectors.py tools/test_player_vectors.py
python3 tools/check_simulation_boundary.py .
git diff --check
ctest --test-dir build/native --output-on-failure -j 3
```

The first smoke attempt failed in the scratch comparison harness because it
compared Python tuples directly with JSON lists. Normalizing the expected
observation through JSON corrected that harness; the repeated smoke passed.
No native mismatch was found.

The smoke script is scratch under ignored `work/`; it sends only the new recipe
inputs to the trace command and compares observations with fresh pinned Python
runs. The checked-in comparison uses the complete committed corpus instead.
The research checkout and shared build/roadmap/workflow files were not changed.
Private acceptance was skipped because `../../private-validation` is unavailable.
No original runtime/console, Session/scene, Norrath, physical Intel or Windows
execution is claimed. Rosetta is an emulated x64 comparison on this arm64 host.

## Ordinary tail and full-tick boundary

`tail()` implements sections 7–8 of the pinned model: positive-fade light linking,
selected-ability early exit, conditional forward offset, frame-wave radius,
inverse-square fields, all eight signed-colour stores, update-cast dispatch,
unsigned record countdowns, flag clearing and forced-ability cleanup. Slot and
record addresses are captured and reread at the original sites, including after
light and cast callbacks. No original strings, colours or ability tables are
embedded; the host supplies them. The exact FPU core evaluates every arithmetic
operation. `light_link2` is appended as service 94; previous indices are unchanged.

The primary `run()` now calls `top()`, `dispatch_state()` and `tail()` in that
order, catches only `Leave`, and returns `"end"`, `"scripted"`,
`"recall during block move"` or `"ability button index"` as appropriate. A
callback-thrown Leave is treated identically. The histories, seed and draw count
provide the reference Outcome observations without duplicating its uncounted
harness class. Missing handlers, malformed returns, callback exceptions and
resource refusals propagate with retained effects. Failed ticks are not rolled
back. Helper externalization remains unported and uncounted; full ticks use the
complete internal helper bodies with all external services callback-bound.

`run_legacy_entry()` retains the old ordinary refusal exactly for command 0.
Command `[29,pad]` invokes standalone tail. Command `[30]` invokes full run,
returning 0 for end, 1 for scripted, 2 for recall and 3 for the ability-index exit.
This result mapping belongs only to the test transport; the public API returns
the exact reason. Existing command/result/snapshot shapes are unchanged.

## Ordinary tail and full-tick verification

On 2026-10-13, the generator produces **218,336 complete-state cases**, including
**40,992 new cases** from 2,562 recipes across all sixteen FPU configurations.
Each expected observation is produced twice freshly from the immutable reference.
Generation uses Python 3.12.3. Nine independent frozen subset hashes protect all
177,344 historical rows, including command 0's unchanged refusal and every earlier
helper/state observation. The expanded hash is
`ec38f0de6c2e42c46fae39b74c49b195a7491f1359e22d599229deca5b6dcb64`.

The new one-configuration smoke passes all 2,562 recipes on arm64 Release,
Rosetta x86_64 Release and ASan/UBSan. Direct host checks pass on all three.
No full sixteen-configuration sanitizer run is claimed. Synthetic cases cover
all dispatch mappings, scripted/recall/invalid-selector exits, repeated ticks,
positive/nonpositive/raw-edge fade patterns, frame-wave angles, signed colour
conversions, ability-name matching and recall positioning, unsigned countdowns,
copy gates, cast cleanup and captured/reread slots. All 902 pre-switch input
scenarios are also exercised as full ticks with added state/tail bindings.
Reached services have missing-handler and synchronous-effect failure cases.
Host checks additionally exercise malformed callback results, self-removal,
callback-thrown Leave, invalid pad spans and failure effects.

The arm64 player CTest passes **2/2 in 148.20 seconds** (exact 148.16, host
0.03). Rosetta x86_64 passes **2/2 in 183.89 seconds** (exact 183.74, host
0.15). Both compare the complete corpus across all sixteen FPU configurations,
including all nine historical hashes. The two suites ran concurrently. `file`
confirms distinct arm64/x86_64 Mach-O executables. This is an emulated x64
comparison, not physical Intel or Windows execution. Release configure/build,
Python compilation, simulation-boundary checks and diff whitespace checks pass.

The final full native suite passes **102/102 in 383.23 seconds**, with no
failure, timeout or retry. Its player-exact check passes in 140.10 seconds and
player-host in 0.03 seconds. Every validation command below passes. Public
private-input/report tests use synthetic fixtures; their success is not private
acceptance. Private game files are unavailable, so no private measurement was
attempted. No original runtime/console, Session/scene integration, Norrath,
physical Intel or Windows validation is claimed.

Changed files: `native/include/champions/player.hpp`, `native/src/player.cpp`,
`native/tests/player_tests.cpp`, `native/tests/player_trace.cpp`,
`tools/generate_player_vectors.py`, `tools/test_player_vectors.py`,
`native/tests/vectors/player.tsv.gz`, `native/tests/vectors/player.json`, and
this page. Shared build/roadmap/workflow files and research files were not edited.

Commands run from this worktree (the smoke helper is ignored scratch):

```sh
python3 tools/generate_player_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target champions-player-tests champions-player-trace -j 4
cmake --build build/native --target champions-player-tests -j 4
cmake --build build/native -j 4
build/native/champions-player-tests
python3 work/smoke_tick.py
ctest --test-dir build/native -R '^player-' --output-on-failure
cmake -S native -B build/player-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/player-x64 --target champions-player-tests champions-player-trace -j 4
build/player-x64/champions-player-tests
python3 work/smoke_tick.py build/player-x64/champions-player-trace
ctest --test-dir build/player-x64 -R '^player-' --output-on-failure
cmake -S native -B build/player-sanitized -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/player-sanitized --target champions-player-tests champions-player-trace -j 4
build/player-sanitized/champions-player-tests
python3 work/smoke_tick.py build/player-sanitized/champions-player-trace
file build/native/champions-player-trace build/player-x64/champions-player-trace
python3 -m py_compile tools/generate_player_vectors.py tools/test_player_vectors.py
python3 tools/check_simulation_boundary.py .
git diff --check
ctest --test-dir build/native --output-on-failure -j 3
```

## Scalar-width access and string boundary

The eight-unit family implements the pinned low-byte signed conversion, all three
remaining unsigned reads, all three logged width stores, and the configurable
binary string reader. `Tick::signed8` now uses the same public low-byte conversion.
The width stores use the existing write path: mask the value, record the Tick
write and event, then call Memory.store. Attempted-store effects remain visible
when the sparse backing refuses. Signed host values convert modulo 2^64 before
width truncation; 64-bit reads preserve their entire unsigned result.

`cstr(address, limit)` returns bytes preceding the first NUL or the supplied
limit. The default remains 256, preserving every earlier string comparison and
vector. Negative and zero limits return empty without inspecting the address.
The native signed limit domain is int64; output is capped at 1 MiB and memory
addresses retain the existing checked raw32 domain. These caps are explicit host
resource refusals, not original game behavior. Large limits can still succeed
when a NUL is found below the cap. No strings or game tables are embedded.

Trace command `[31,helper,address,value]` appends utility operations without
changing earlier commands or snapshot shapes. Helpers 0–3 are top-level s8,
u8, u16 and u64 (trailing value must be zero); 4–6 are w8, w16 and w32; 7 is
cstr with a signed limit. Strings return unsigned byte arrays, and u64 returns an
unsigned JSON number without passing through a signed result lane. Synthetic
cases cover every signed byte with high-bit noise, unaligned and sparse reads,
unsigned 64-bit boundaries, negative/truncated writes, overlapping stores,
last valid raw32 spans, all nonzero byte values, limits around 256, embedded NULs
and empty reads. Host checks also exercise invalid spans, output-cap refusal and
logged store failures. Delegated services and helper selection are unchanged.

## Scalar/string verification

On 2026-10-13, Python 3.12.3 generated **231,744 complete-state cases**, including
**13,408 new cases** from 838 recipes across all sixteen FPU configurations.
Every expected observation is produced twice freshly from the immutable reference.
The expanded fixture SHA-256 is
`8c9cec57f70df51086871514f26a06de032c5630436b1db82490f15c5adc3397`.
Ten frozen historical subset hashes protect all 218,336 earlier rows; the newest
subset hash is the preceding whole-corpus hash. Existing expected observations
are not rebaselined to native output.

The new-family smoke matches all 13,408 complete observations on arm64 Release,
Rosetta x86_64 Release and ASan/UBSan across all sixteen configurations. Direct
host checks pass on all three, including the 1 MiB string refusal. `file` confirms
distinct arm64 and x86_64 Mach-O trace executables. Rosetta supplies an emulated
x64 comparison; no physical Intel or Windows execution is claimed. No
ASan/UBSan diagnostics were emitted. No full historical-corpus sanitizer or
x64 run is claimed for this change.

The arm64 targeted player CTest passes **2/2 in 119.85 seconds** (exact 119.81,
host 0.03), matching all 231,744 complete-state cases and all ten historical
hashes. Python compilation, simulation-boundary validation and whitespace checks
also pass.

The final full native suite passes **102/102 in 375.89 seconds** with no failure,
timeout or retry. Its player-exact case passes in 134.23 seconds and player-host
in 0.06 seconds. Registered private-input/report cases use public synthetic
fixtures; those passes do not establish private game-file acceptance.

Changed files are the existing player header/source, host/trace tests, vector
generator/comparison wrapper, gzip fixture/JSON manifest and this page. No shared
build, roadmap or workflow file was edited. The research checkout remains clean.
Private acceptance was skipped because `../../private-validation` is unavailable;
no alternate private-input search was performed. Original runtime/console,
Session/scene integration and Norrath behavior remain unverified.

## Scalar/string verification commands

Commands run from this worktree (the smoke helper is ignored scratch):

```sh
python3 tools/generate_player_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake --build build/native --target champions-player-tests -j 4
build/native/champions-player-tests
python3 work/smoke_utility.py
ctest --test-dir build/native -R '^player-' --output-on-failure
cmake -S native -B build/player-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/player-x64 --target champions-player-tests champions-player-trace -j 4
cmake --build build/player-x64 --target champions-player-tests -j 4
build/player-x64/champions-player-tests
python3 work/smoke_utility.py build/player-x64/champions-player-trace
cmake -S native -B build/player-sanitized -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/player-sanitized --target champions-player-tests champions-player-trace -j 4
cmake --build build/player-sanitized --target champions-player-tests -j 4
build/player-sanitized/champions-player-tests
python3 work/smoke_utility.py build/player-sanitized/champions-player-trace
file build/native/champions-player-trace build/player-x64/champions-player-trace
python3 -m py_compile tools/generate_player_vectors.py tools/test_player_vectors.py
python3 tools/check_simulation_boundary.py .
git diff --check
ctest --test-dir build/native --output-on-failure -j 3
```

The initial full build failed because a new test variable collided with an existing
one in the same function. Renaming that test variable fixed the compile failure;
the full Release build and subsequent host checks pass. No native behavioral
mismatch was observed in the new-family smoke tests.

## Exactly what remains

The seven uncounted units are `Memory.__init__`, `Memory.load`,
`Memory.write_bytes`, and Tick's `__init__`, `call`, `vcall`, and `helper`.
The partial-equivalent exclusions in Progress accounting still apply. An AST
recount of the pinned Python source confirms 98 units minus the four named
harness units = 94 portable functions; 79 previous complete units plus all eight
scalar/string units = 87 fully ported and tested.

Next coherent families are **complete Memory infrastructure** (constructor,
optional underlying read delegate, unlogged byte writes) and **generic callbacks
and selectable helper externalization** (Tick constructor, call, vcall, helper).
Both have pinned portable evidence. Extend this same module and synthetic
transport, preserving all 231,744 rows and historical hashes. Memory's optional
underlying delegate must remain an explicit host callback. Its existing
read-only overlay has different semantics and does not complete that unit.
Do not count partial or refused behavior as a completed unit.

Real service implementations and adapters, Session/scene control integration,
original runtime/console comparison, private-data acceptance, Norrath validation
and physical x64/Windows validation remain independent milestones. A full pinned
player tick with synthetic callbacks is not a playable port. Private acceptance
is skipped because `../../private-validation` is unavailable; no alternate
private-input search is permitted and the research checkout is read-only.
