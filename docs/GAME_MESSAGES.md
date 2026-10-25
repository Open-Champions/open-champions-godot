# Return to Arms game messages, HUD, inventory and shop transactions

PROGRESS: 101 of 101 reference functions ported

The standalone `champions-messages` C++17 library implements the complete pinned message
contract for Return to Arms (SLUS_209.73, US v2.00) in `champions::messages::rta`. It is independent
of Godot, sockets and Session. Ability bonuses use the existing exact
`champions-core` arithmetic operations. This is message
transport/model parity with complete standalone HUD, inventory and shop dispatch,
not a playable port. Norrath is
unsupported.

## Pinned evidence

The authored translation follows the MIT-licensed research contracts at commit
`356add27d50e50972b2a671b31e8f4ec93e6778c` in
[champions-return-to-arms-re](https://github.com/Open-Champions/champions-return-to-arms-re/tree/356add27d50e50972b2a671b31e8f4ec93e6778c).
The checkout was read only. Exact SHA-256 values:

| File | SHA-256 |
|---|---|
| `tools/ref_messages.py` | `af6ea5fea7ccb6a06ba9992547a87062d9c3070b7ab0e9737241274def52e12c` |
| `docs/spec/game-messages.md` | `daa037829a8c182e4c925c32bd7c3b3647cc08a88af4230cdde922155de46fd8` |
| `tools/difftest_messages.py` | `efa79458e2bc239b8e6b83983d365b779ca09fb941b62241be470b6837b2e5ec` |
| `tools/ref_core.py` (Python import dependency) | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `tools/ref_items.py` (Python import dependency) | `513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e` |

Specification sections 1, 3, 5.2, 5.6, 7 and 11 and the corresponding reference
methods define this subset. The differential harness documents original-execution comparisons for
registration, transport and senders; its reported results are research evidence,
not a harness execution in this port. No executable, decompiler output, original
asset, table, or private validation input was used. Fixture bytes are authored
synthetic values. The repository MIT license covers this authored translation.

## Implemented contract

- Registration searches all sixteen words for an existing handler, then writes
  the first free word. A full table writes word seventeen and returns 16, exactly
  as the reference. Registering zero can find an empty word without a store.
  Startup registers eight families in order and stores the returned IDs at their
  documented destinations. The prose's description of “seven” calls conflicts
  with its eight-row table and the executed Python loop; the latter is retained.
- All **27** reference encoders: ten inventory, fourteen game/HUD, buy and sell,
  and null shop-closed. `encode(Kind, arguments, name)` takes arguments in the
  corresponding `msg_*` order. Integers are unsigned64 patterns and truncate to
  field width. Quiet, menu flag and sell mode must be supplied explicitly.
  `new_world` takes arbitrary bytes and appends a null, even if the input already
  contains nulls. `load_level` emits only `0b000000`; it supplies no level name.
  Wrong kinds/argument counts and name bytes supplied to another kind refuse
  with `std::invalid_argument` before producing a message.
- Any nonzero network-mode byte queues the channel byte, truncated length byte,
  and **every** supplied payload byte, even beyond 255. It uses the first local
  slot callback, the original 8192-byte ring mask, and raw32 address arithmetic.
  There is no invented capacity check, seed draw, or peer-slot validation.
- Offline send appends sixteen zero bytes before delivery, matching Python
  `Machine.send`. These zeros are a reference-model choice, not a claim about
  original stack initialization. Direct delivery preserves the full supplied
  buffer, including caller-supplied stale bytes; it does not synthesize a tail.
- Null delivery does nothing. Cutscene, voice, dialog and text remain an explicit
  external callback carrying the complete bytes and their length. Callback calls
  are recorded before invocation; thrown exceptions propagate with preceding
  writes/calls retained. Callback mutation and nested delivery are supported.
  Missing callbacks return zero or do nothing, as the reference specifies.
- The actual Python `deliver` reads the handler word using raw32 address
  arithmetic without checking channel range, despite its range-error docstring.
  C++ follows that implementation: an out-of-range ID can resolve a mapped
  handler; an unknown handler raises `std::invalid_argument`. This is sparse
  model behavior, not permission to dereference arbitrary host pointers.

`Memory` owns sparse little-endian bytes; absent bytes read as zero. `poke`
initializes or performs unlogged callback mutations. `store` retains every
ordered scalar write, including width and truncated value. Scalar bases are
raw32; bytes within a scalar can extend past that base's wrap boundary, matching
Python `Memory`. Only widths 1, 2, 4 and 8 are exposed. Resource addresses and
handler IDs are integer model keys, never host pointers.

## Player abilities and HUD controls (continuation family)

All eleven methods in this family are now ported: `skill_rank`, `set_skill_rank`,
`passive_level`, `raise_passive_level`, `rebuild_abilities`, `add_skill_ability`,
`on_quick_slot`, `on_assign_spell`, `on_quick_cast`, `mask_local_buttons`, and
`on_menu_state`. These are standalone methods on the existing `Machine`; complete
HUD dispatch is now implemented through the lifecycle family below.

- Skill lists compare signed16 IDs for reads but signed32 IDs for writes, and
  appending a rank does not add a terminator. Caller-supplied lists must terminate
  with ID -1, as in the reference. No arbitrary iteration cap is invented.
- Passive lookup rejects unsigned64 slots above 4; raising has no slot guard.
  Both scan all 48 class rows, selecting the first matching mask. Signed negative
  level bytes read as zero; raising compares the signed byte with signed32 level.
- Ability creation chooses the first of fifteen free records, refuses marker/ID
  duplicates and kind 3 skills, zeroes every byte in the record, and raises the
  rank to at least one. Rebuilding scans caller-supplied old-ability rows until
  the supplied end-marker string. It copies only the documented fields and in
  their original store order; ID 0x1d skips copying. Load/HP/mana bonuses use
  exact reference float patterns through `champions-core`, including clamping.
- Spell assignment selects the last of the first eight duplicate records and
  preserves self-assignment, signed16-to-unsigned64 comparisons, marker substring
  checks, raw32 addressing and original read-after-write ordering. Old-style
  fallback rebuilding is implemented directly, rather than delegated.
- Quick slots exchange bindings when the other signed byte equals the requested
  index. Quick casting preserves queued-cast refusal, the warning callback path,
  action-state 2 queuing, and reads through a null object model address.
- Menu state retains the no-object exit, signed state comparison, ordered flag
  updates, and closing input-lock max operation. The input-lock third word is
  written before frames and mask.

`Callbacks::service` exposes only the services this family calls:
`net_isLocalSlot`, `res_find`, `sound_playOnObject`, `text_lookup` and
`hud_showMessage`. `Call::arguments` records their complete scalar argument
patterns. Float sound gain remains the pattern `0x3f800000`. Missing callbacks
return zero; callbacks may mutate memory, reenter the machine or throw. Calls
are recorded before invocation, and failure retains the preceding writes/calls.
The existing transport callbacks and trace representation remain unchanged.
No original strings, skill objects or ability rows are embedded: the caller
supplies them in sparse memory. String reads retain the reference's 4096-byte
limit; table/list scans retain its requirement for supplied terminators.

## Exact verification (ability/control baseline)

The generator checks all five files against both their working-file hashes and
`git show` at the pin before importing Python with bytecode writing disabled.
It never imports or runs the executable-dependent differential harness.

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

The committed deterministic gzip corpus contains **8,366 input-driven cases**.
The C++ trace driver receives only action scripts. Python-generated expectations
independently compare the entire sparse byte map (including explicit zeros),
ordered scalar writes, callback names/arguments/buffer bytes, results and
exception prefixes. Cases cover every encoder, field truncation, registration
holes/duplicates/full-table overflow, repeated startup, raw slot patterns,
nonzero signed network modes, truncated length with complete payload, ring laps
and overwritten data, sequential sends, direct/offline delivery, callback
mutation and exceptions. All original 4,491 vector rows remain byte-for-byte identical as the corpus
prefix. The 3,875 added cases cover every method in the continuation family,
including negative/truncated IDs, passive signed levels, full/free ability slots,
owned masks, multi-row searches, field copying, bonus float edge patterns,
self/duplicate/no-match assignment, null/aliased addresses, callback mutation and
exception prefixes. The host API test has 19 checks, including absent services,
reentrant control calls and sound exceptions before stores, plus the existing
bad-argument and unsupported-handler refusal checks.

The manifest records the research commit, five source hashes, generator hash,
uncompressed vector hash, row count and per-group counts. Integrity tests reject
truncated fixture data, deliberately wrong expected state and malformed driver
input. Ordinary CTest needs no research checkout or game content. This suite is
registered in native CTest; it is not yet part of the existing cross-platform
trace artifact aggregation workflow.

## HUD message dispatch and player/level lifecycle (previous family)

All seven lifecycle methods are ported: `hud_on_msg`, `recall`, `stop_stream`,
`on_player_left`, `on_disconnect_request`, `set_pending_level`, and `on_checksum`.
Recall's complete dependencies `remove_item`, `grid`, and `same_identity` are also
ported, together with general `copy`, `wstr`, and `vec` helpers. Both string
readers now accept the reference's explicit limit, including negative and odd
wide-string limits. `deliver` executes the game/HUD handler; the shop continuation below completes delivery.

- HUD dispatch pads short payloads to sixteen bytes and preserves longer stale
  tails. Signed type bytes outside 0..13 do nothing. All fourteen branches run,
  including the earlier quick-slot, assignment, menu-state and quick-cast bodies.
  Recall's dialog/task tests short-circuit in order, and its countdown is read
  again before closing local menus. Load-level reads the name at payload + 4.
- Recall preserves no-save/dead/hub-target checks, exact core float comparisons
  (including NaNs), first matching bag-2 potion, unsigned16 charge underflow,
  zero-charge item removal, per-alive-player effect allocation, sound/stream call
  order, and every ordered flag write. Callbacks retain all external ownership;
  no world query, animation, effect, pause menu or audio service is invented.
- Removal clears paired equipment slots, copies complete 0x48-byte records in
  forward 8-byte stores, repoints equipment/weapon references, renumbers stamps,
  clears the tail and decrements the signed count. Local grids retain their
  row-before-column order and all 64 identity comparisons, including duplicate
  matches and repeated stamp writes. No range/capacity guards are invented.
  Raw address wrap and the reference's unwrapped temporary-pointer comparisons
  are preserved.
- Departure builds little-endian wide text from supplied name/space bytes and
  translated text, clears the object's menu flag, supplies its signed virtual
  `this` adjustment, and rereads the object after the damage callback. Disconnect
  tests every candidate active slot in order, with the original null-object reads
  and signed facing pattern, then delegates disconnection.
- Level names stop at the first null and have no invented length cap. The marker
  write follows the name copy even when it overwrites a long name. New-world and
  load-level have distinct load-flag behavior and the original callback/store
  order. Checksum counts wrap as bytes, use signed arrival-count comparisons,
  and compare both words independently; two mismatches can disconnect twice.
  The local slot and player count are reread at the reference's original points.

`Argument` retains a scalar pattern, owned byte string, or three-pattern
`Position`. `Callbacks::typed_service` receives complete ordered arguments for
built-text and position calls; when set it also receives scalar services.
`Callbacks::service` remains the scalar-only route. Missing callbacks return zero.
`net_getLocalPlayerSlot` continues through the existing dedicated callback.
New calls are recorded before invocation; mutation, reentry and exceptions keep
all preceding state and call history. The existing vector trace format is
unchanged for all old cases, with typed suffixes only for newly exercised values.
Zero-argument services have an explicit trace tag so they cannot be confused with
external message-buffer calls.

The corpus now contains **15,189** cases: all **8,366** pre-existing rows remain
byte-for-byte identical as its prefix, followed by **6,823** lifecycle/dependency
cases. They cover every HUD branch through direct dispatch, delivery and offline
send; shortened and stale buffers; signed slots; hub/error/potion paths; effect
positions; departure wide text and virtual offsets; disconnect gates; checksum
wrap, duplicates and mismatches; compaction, paired slots and grid duplicates;
callback failure prefixes and mutations; forward-copy aliasing, raw address
wrap, string limits and vector reads. Host API coverage has **26 checks**.

## Inventory ownership and player dispatch (previous family)

All fifteen inventory/player methods are ported: `inv_dispatch`,
`send_character_to_lobby`, `item_object`, `check_equip_allowed`, `item_value`,
`give_item`, `first_recall_hint`, `unref_item`, `drop_item`, `equip_item`,
`_equip_item`, `unequip_item`, `on_join_panel`, `on_stat_point`, and
`on_skill_point`. Their shared `sound_name` dependency is also complete.
Direct `_equip_item` retains untruncated item comparisons, while `equip_item`
first masks the address; both wrap every memory read/write.
The constructor now accepts caller-owned `Items::Tables` and exposes the
reference's equip recursion depth; unwind restores it on callback exceptions.
These seventeen units advance the count from 63 to 80. The AST was independently
recounted as 101 units after excluding only `Ext.__init__`.

Evidence: the unchanged pinned reference, specification sections 2 and 5,
and its `ref_items.py` dependency. The existing `champions-items` module supplies
item unpacking, equip restrictions and values, through a minimal additive link
in `native/messages_targets.cmake`. The caller must retain the supplied tables
for the Machine lifetime. No definition, name or asset is embedded in the module.
Missing tables refuse item interpretation; an unknown definition remains a null
item definition, as in `Item.unpack`, and cannot be used by requirement/value
operations. Such invalid input is not guessed into a definition.

- Dispatch pads short buffers to sixteen bytes and preserves stale tails.
  All eleven branches execute, including the empty type 3, signed byte fields,
  raw item addresses, delegated gem combining and potion use. Shop is completed below.
- Give preserves the first matching stack among 64 records, wrapped charges,
  single-copy names, occupied destination refusal, exact hint short-circuit
  order, count rereads after callbacks and forward 8-byte copy stores. Capacity
  and stat requirements are not invented.
- Equip preserves requirement precedence and first duplicate skill rank,
  both weapon sets, recursive primary-weapon restoration, dual wield, shield
  changes, rings/earrings, quiet sounds and every animation/stat call. Unequip
  retains unconditional armour-slot clearing and weapon-swap delegation.
  Unref performs the nested offline equip before clearing the second hand;
  network mode queues the original message instead.
- Drop retains signed counts, twenty-unit ammunition/thrown limits, material
  sound precedence, all 675 grid cells in row/column order, duplicate-cell
  count/clear writes, null-object writes, category-based removal, and callbacks
  receiving the reduced record before removal. Supplied world positions and
  allocation retain their original call order.
- Lobby upload lists active players with the original count rereads, snapshots
  the frame counter before the local-slot callback, and reads the shared seed
  after that callback for slot -1. It never advances the seed. A negative frame
  counter raises the reference's explicit error.
- Panel join uses the panel's owner, emits its nested level acknowledgement,
  and preserves offline/per-panel versus network/shared flag writes. Stat and
  skill point spending keeps negative indices, signed masks, rank caps and
  point deduction before ability allocation.

`Callbacks::result_service` supplies the three-pattern return of
`world_findFreeSpot`; scalar and typed argument callbacks remain compatible.
The position call is recorded before invocation. If allocation succeeds but
that callback returned a scalar (including the default zero), drop refuses at
the reference's tuple-conversion point, after `pool_new`, retaining the reduced
count and call prefix. No placement algorithm is substituted. All other services
remain explicit callbacks with default zero, mutable memory and exception/reentry
semantics. `inventory_combineGems` and `Player_usePotion` remain callbacks because
the reference delegates them; these do not count as native implementations of
those external services.

## Shop transactions and dispatch (current family)

All three shop methods are ported: `shop_on_msg`, `shop_buy`, and `shop_sell`.
`Machine.deliver` is now complete across every reference family, adding four
fully ported/tested units. The pinned AST was recounted as 101 portable units,
excluding only the executable-harness descriptor `Ext.__init__`.
Evidence is specification section 4 and the corresponding pinned Python bodies;
item values and arithmetic use the existing exact item/core modules.

- Dispatch zero-pads short buffers to sixteen bytes, preserves longer stale tails,
  decodes signed slot/bag/panel/mode bytes and column/row halves, and ignores every
  type except 0 and 1. Registered delivery and offline send execute these bodies.
- Buying uses exact float patterns `0x3f199999`, `0x3ee66666`, and `0x3f800000`
  in precedence order. A value of 100 at the default factor costs 59. The signed
  gold comparison precedes the debit, then `give_item` runs with announcement 1.
  Its refusal is ignored: an occupied/full bag or duplicate unique item can take
  gold and give nothing. No stock validation or decrement is invented.
- Selling computes value before drop's empty-cell check, preserves the separate
  sell-name stem, twenty-item multiplier, signed count division (including zero),
  minimum payment of one, and ammunition/bit-46 zero-payment override. Addition
  wraps to signed32 before comparison with 99,999,999. The uncapped gold store
  occurs first; a second cap store occurs only when needed. All seven supplied
  arguments then reach the existing drop body, including ground/silent modes.
- Callback mutation, exceptions and reentrant shop delivery retain earlier gold,
  item and call prefixes. All delegated inventory, placement, sound and player
  services keep their existing callback boundaries. Missing item tables continue
  to refuse interpretation; no original data or replacement engine behavior is
  supplied. These callbacks are reference behavior, not unported shop functions.

The corpus now contains **28,688** cases; all **24,667** preceding rows remain
byte-for-byte identical as the prefix. The **4,021** added cases cover price
rounding, factor/flag precedence, signed values/counts/gold, cap and wrap,
full/occupied/unique-item refusals, raw address wrap, all dispatch branches and
buffer lengths through three routes, callback failure prefixes and mutations.
The native API suite has **40 checks**, including shop debit-before-failure,
ordered cap writes and reentrant delivery.

## Progress accounting and precisely remaining work

The pinned Python AST has 102 top-level functions plus class methods. The sole
excluded harness unit is `Ext.__init__`, which describes executable-interpreter
callback signatures. The remaining **101** include constructors, memory helpers,
private byte encoders, and gameplay methods, without counting aliases twice.
The 101 fully implemented/tested units are:

- 27 `msg_*` encoders; `_b`, `_h`, `_w` mapped to the shared byte/halfword/word
  encoder operations; `s8`, `s16`, `s32` mapped to exact native signed conversion.
- `Machine.rec`, `cstr`, `wstr`, `vec`, `copy`, `sound`, `register_handler`,
  `register_all`, `send`.
- All eleven ability/control methods listed above, plus the seven lifecycle
  methods and three inventory dependencies listed in this family.

- All fifteen inventory/player methods listed in the previous family above,
  top-level `sound_name`, and `Machine.__init__` with supplied item tables,
  callbacks, owned call history and exception-safe equip depth.

- The three shop methods above and complete `Machine.deliver` across every
  reference handler, with reference-delegated handlers kept as callbacks.

- All eleven scalar wrappers (`u8`, `s8`, `u16`, `s16`, `u32`, `s32`, `u64`,
  `w8`, `w16`, `w32`, `w64`) and top-level `s64`.

- General `Memory.__init__`, `Memory.load`, `Memory.poke`, `Memory.store`, and
  `Machine.call`, completed by the generic backing-memory/callback family below.

**Remaining reference functions: none.** All 101 portable units in the pinned
file are implemented and tested. `Ext.__init__` remains excluded as an
executable-harness descriptor. Gem combining and the other reference-delegated
engine services remain callbacks; their underlying bodies are outside this
contract and are not counted as additional ports.

Session/ready-frame integration, sockets, original executable parity, private
content acceptance, Norrath behavior, Windows execution and physical Intel-host
execution remain unverified. This family has no private-input or game-asset use.

## Local verification record (previous ability/control continuation)

Apple Silicon macOS, AppleClang 21.0.0, Release build. Commands run:

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re --output work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.tsv.gz work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.json work/messages-reproduced.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
mkdir -p build/messages-sanitized build/messages-x64
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-sanitized/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-sanitized/api
build/messages-sanitized/api
python3 tools/test_messages_vectors.py --native build/messages-sanitized/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-x64/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-x64/api
arch -x86_64 build/messages-x64/api
python3 tools/test_messages_vectors.py --native build/messages-x64/trace
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native -R '^session-creature-transport$' --output-on-failure
ctest --test-dir build/native -R '^session-turn-transport$' --output-on-failure
git diff --check
```

Vector generation and deterministic reproduction passed, the message CTest
subset passed 3/3, and arm64, sanitized and x86_64 API runners passed 19 checks
apiece. Sanitized and x86_64 runners each passed all 8,366 exact cases, with
identical arm64/x86_64 complete traces. The original 4,491-row prefix was checked
against the pre-change corpus and is byte-for-byte identical. No original
executable-dependent harness or private-content validation was run.

Full CTest initially passed **101/103** in 2841.59 seconds. The two unchanged
Session transport checks (`session-creature-transport`, `session-turn-transport`)
hit their Python subprocess's 600-second timeout while other worktrees were also
testing; neither reported an exact-state mismatch. Each was then rerun **alone**:
creature passed in 206.54 seconds, turning passed in 116.29 seconds. All 103 cases
therefore have passing results across the full run and isolated reruns. No test
limit or unrelated implementation was changed. `git diff --check` passed and the
research checkout remained clean.

Changed files: `native/include/champions/messages.hpp`,
`native/src/messages.cpp`, `native/tools/messages_trace.cpp`,
`native/tests/messages_tests.cpp`, `native/tests/vectors/messages.tsv.gz`,
`native/tests/vectors/messages.json`, `tools/generate_messages_vectors.py`,
`native/messages_targets.cmake` (existing core linkage), and this page.
No other module, shared build list, research file or private input was changed.

## Local verification record (HUD lifecycle family)

Apple Silicon macOS, AppleClang 21.0.0, Release. Eight files changed: this page,
`native/include/champions/messages.hpp`, `native/src/messages.cpp`,
`native/tools/messages_trace.cpp`, `native/tests/messages_tests.cpp`,
`native/tests/vectors/messages.tsv.gz`, `native/tests/vectors/messages.json`,
and `tools/generate_messages_vectors.py`. No shared build list or sibling file
changed. No executable-dependent harness or private-content validation ran.

Exact commands run:

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re --output work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.tsv.gz work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.json work/messages-reproduced.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
mkdir -p build/messages-sanitized build/messages-x64
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-sanitized/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-sanitized/api
build/messages-sanitized/api
python3 tools/test_messages_vectors.py --native build/messages-sanitized/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-x64/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-x64/api
arch -x86_64 build/messages-x64/api
python3 tools/test_messages_vectors.py --native build/messages-x64/trace
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Generation and deterministic reproduction passed. The entire 8,366-row old
corpus is byte-for-byte identical as the new corpus prefix. The final targeted
CTest run passed **3/3** in 54.19 seconds. arm64, sanitizer and x86_64 API runners
passed **26 checks** each. All **15,189** exact states passed on arm64, sanitizer
and x86_64 (Rosetta). An independent full-output comparison found identical
arm64/x86_64 trace SHA-256:
`cf7ee79180bf9b27cbf5df5d1d171f814d5910d80f88042812d8a5fe5a033960`.
Windows and physical Intel-host execution remain unverified. Build and
`git diff --check` passed without compiler warnings; the research checkout stayed
clean. An early targeted run began before vector regeneration finished and
correctly rejected the stale generator hash; the final generated corpus and
all subsequent message checks passed.

Full repository CTest passed **103/103** in **2006.85 seconds**, with `-j 3`.
No timeout rerun was needed and no test limit or unrelated implementation changed.

## Local verification record (inventory ownership and dispatch)

Apple Silicon macOS, AppleClang 21.0.0, Release. Nine files changed: this page,
`native/include/champions/messages.hpp`, `native/src/messages.cpp`,
`native/tools/messages_trace.cpp`, `native/tests/messages_tests.cpp`,
`native/messages_targets.cmake`, `native/tests/vectors/messages.tsv.gz`,
`native/tests/vectors/messages.json`, and `tools/generate_messages_vectors.py`.
Only the message module's link list changed; no shared build list or sibling
file changed. No executable-dependent harness or private-content validation ran.

Exact final commands:

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re --output work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.tsv.gz work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.json work/messages-reproduced.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
mkdir -p build/messages-sanitized build/messages-x64
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tools/messages_trace.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tests/messages_tests.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/api
build/messages-sanitized/api
python3 tools/test_messages_vectors.py --native build/messages-sanitized/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/core_state.cpp native/src/items.cpp native/src/item_generation.cpp native/src/item_names.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-x64/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/core_state.cpp native/src/items.cpp native/src/item_generation.cpp native/src/item_names.cpp native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-x64/api
arch -x86_64 build/messages-x64/api
python3 tools/test_messages_vectors.py --native build/messages-x64/trace
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Generation, deterministic reproduction and the byte-for-byte 15,189-row prefix
comparison passed. The corpus contains **24,667** exact cases, including **9,478**
new cases for every method in this family: all inventory branches, signed fields,
stack/hint/refusal paths, material priorities, item decoding and requirements,
equipment slots and dual wield, nested sends, panel/stat/skill bookkeeping,
lobby gates and seed/frame selection, world position tuples, grid duplicates,
callback exceptions/mutations and direct-inner versus wrapper address truncation.
The final targeted CTest run passed **3/3** in **26.86 seconds**.
The native API runner passed **34 checks**. Sanitizer instrumentation covers the
message implementation and test/trace driver, linked to the existing Release
item/core libraries; it does not instrument those unchanged libraries.

Earlier generation attempts exposed malformed authored fixtures (a shield used
as a primary weapon, and an incorrectly encoded signed16 player slot); those
fixtures were corrected without changing the reference or inventing scan limits.
A first build exposed a mixed signed/unsigned `auto` declaration, also corrected.
Checks started during generator revision correctly rejected the stale manifest
hash. Two preliminary full-suite attempts were deliberately interrupted before
completion to finish address and fixture-coverage review; only the final completed run is claimed.

The arm64, sanitizer and x86_64 (Rosetta) API runners passed **34 checks** each.
All **24,667** complete states passed on all three runners, so arm64 and x86_64
outputs match the same full expected trace (SHA-256
`20c9cc914259c1e06fef20b0fbc3b9254a31f872548d101e289077bd808b13b8`).
Windows, physical Intel-host and Session/scene integration remain unverified.
The research checkout remained clean; `git diff --check` and builds passed.

Full repository CTest passed **103/103** in **1562.48 seconds**, with `-j 3`.
No timeout rerun was needed, and no test limit or unrelated implementation changed.

## Local verification record (shop transactions and dispatch)

Apple Silicon macOS, AppleClang 21.0.0, Release. Eight files changed: this page,
`native/include/champions/messages.hpp`, `native/src/messages.cpp`,
`native/tools/messages_trace.cpp`, `native/tests/messages_tests.cpp`,
`native/tests/vectors/messages.tsv.gz`, `native/tests/vectors/messages.json`,
and `tools/generate_messages_vectors.py`. No build lists, other modules, research
files or private inputs changed. The research checkout remained clean.

Exact commands run (all final results passed):

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re --output work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.tsv.gz work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.json work/messages-reproduced.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
mkdir -p build/messages-sanitized build/messages-x64
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tools/messages_trace.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tests/messages_tests.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/api
build/messages-sanitized/api
python3 tools/test_messages_vectors.py --native build/messages-sanitized/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/core_state.cpp native/src/items.cpp native/src/item_generation.cpp native/src/item_names.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-x64/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/core_state.cpp native/src/items.cpp native/src/item_generation.cpp native/src/item_names.cpp native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-x64/api
arch -x86_64 build/messages-x64/api
python3 tools/test_messages_vectors.py --native build/messages-x64/trace
python3 work/compare_message_arches.py
build/native/champions-messages-tests
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Generation and deterministic reproduction passed. The old 24,667-row prefix is
byte-for-byte identical, independently checked using decompressed old/new bytes.
All 28,688 exact cases passed on arm64, sanitized arm64 and x86_64 under Rosetta;
all three API runners passed 40 checks. The touched message source/driver/API
were instrumented with ASan/UBSan; the linked existing item/core libraries were
Release libraries without sanitizer instrumentation. This checks the changed
family, not sanitizer coverage of every dependency.

The independent comparison script supplies the same complete corpus to both
architecture drivers, checks each complete stdout against Python expectations,
then hashes it. arm64/x86_64 SHA-256 is identical:
`b46d87b8b2ef7e98dcafe8c157087c7f93e11834c6d8a917f2bfd8fbd4b8b1ce`.
The targeted CTest run passed 3/3 in 14.89 seconds. The first API iteration failed because its
reentrant fixture reused the destination copy retained by a deliberately thrown
callback; the fixture now clears that destination before reuse. Exact vectors
already passed, and no shop behavior was changed to accommodate that fixture.

Full Release CTest passed **103/103** in **840.48 seconds**, with `-j 3`.
No timeout rerun was needed. Build and `git diff --check` passed without warnings.
No original executable-dependent harness, private content, Windows, physical
Intel-host or scene/Session integration validation was run.

## Scalar memory access and signed conversion (previous family)

All twelve units are implemented: the seven scalar reads, four scalar writes,
and top-level `s64`. Evidence is the unchanged pinned `ref_messages.py` memory
wrapper and signed-conversion bodies. The generator independently counts 101
portable AST units, excluding only the executable harness descriptor
`Ext.__init__`, and records both progress counts in the existing manifest.

Addresses and values use raw64 patterns, including signed Python integers modulo
2^64; wrapper base addresses alone truncate to raw32. Multi-byte scalar reads
and writes retain bytes above the raw32 boundary. Writes truncate to field width
and retain every existing ordered write. Signed reads preserve exact two's
complement conversion; `s64` avoids implementation-defined unsigned-to-signed
casts, including at INT64_MIN. No generic-memory refusal is counted as complete.

The existing corpus has 29,918 cases, including 1,230 new signed conversion,
read/write, absent-byte, partial-byte, overlapping-write and wrapping-address
cases. All 28,688 previous rows remain byte-for-byte identical as its prefix.
The API runner adds nine checks (49 total), including sparse reads, conversion
extremes, base-only wrapping, truncation and ordered stores without service calls.

## Local verification record (scalar access family)

Apple Silicon macOS, Release. The changed files are this page, the existing
message header/source, trace driver, API tests, generator, and two vector files.
No research files, shared build lists, other modules or private inputs changed.
The generator verifies the five existing pinned evidence hashes before import.

Commands run:

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re --output work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.tsv.gz work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.json work/messages-reproduced.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
mkdir -p build/messages-x64 build/messages-sanitized
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/core_state.cpp native/src/items.cpp native/src/item_generation.cpp native/src/item_names.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-x64/trace
python3 tools/test_messages_vectors.py --native build/messages-x64/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tools/messages_trace.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tests/messages_tests.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/api
build/messages-sanitized/api
build/native/champions-messages-tests
python3 work/messages-compare.py
ctest --test-dir build/native --output-on-failure -j 3 > work/messages-full-ctest.log 2>&1
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Generation, reproduction, prefix comparison and Release builds passed. The final
targeted CTest passed 3/3 in 25.29 seconds; native and sanitized API runners passed
49 checks each. The touched message source/trace/API were instrumented with
ASan/UBSan; the unchanged linked item/core libraries were not instrumented.
The first scalar oracle rendered `s64` as a raw pattern; it was tightened to
signed decimal so the random vectors distinguish signed conversion errors.
The final corpus is used by the comparison and targeted checks.

The comparison script checks each driver's complete stdout against Python
expectations before hashing it. All 29,918 rows match on arm64, sanitized arm64 and x86_64 under
Rosetta, with SHA-256
`1627b83a6f2a282ac3314aaa95e569531bb4689433d9610eea76866e13d50425`.
Windows, physical Intel hardware, original executable behavior, private content
acceptance, Norrath and scene/Session integration remain unverified.

Full Release CTest passed **103/103** in **729.05 seconds**, with `-j 3`,
including the final message corpus. No timeout rerun was needed. Research status
and `git diff --check` were clean. The family advances progress from 84 to 96;
the subsequent family below completes those five generic-memory/callback units.

## Generic backing memory and callback dispatch (current family)

All five remaining reference units are now ported. Evidence is the unchanged
pinned `Memory` constructor/load/poke/store bodies and `Machine.call`. The
existing generator counts top-level functions plus class methods directly from
that file's AST: 102 units minus the sole harness descriptor `Ext.__init__` = 101.
No aliases or native support helpers increase that denominator.

`Integer` is handwritten C++17 sign/magnitude storage with exact infinite
two's-complement byte extraction, truncation, signed address ordering, address
increment and shifted bitwise OR. The general overloads accept an `Integer`
address/value and signed byte count, including widths beyond eight bytes and
addresses below zero or above 64 bits. `Memory.bytes` and each ordered `Write`
retain the complete integer key/value. There are no new dependencies. The
existing raw32/raw64 scalar convenience overloads retain their width guard;
use the general overloads for arbitrary widths.

- When every requested byte is absent, backing `load(address, size)` is called
  once and its complete result is returned without truncation. A zero-size load
  takes that same path. A negative-size load returns zero without calling under.
- Mixed loads snapshot every overlay byte before the first backing call, then
  request missing bytes individually in address order. Callback writes cannot
  change that snapshot. Backing results are shifted and ORed without an invented
  byte mask, including negative or wider-than-byte results. Fully overlaid loads
  never call backing memory.
- `poke` writes low bytes without logging, including negative-value sign
  extension. Nonpositive sizes do nothing. `store` truncates to the requested
  width, logs before writing, logs zero-width writes and rejects negative widths
  before logging. Backing exceptions retain preceding callback mutations.
- Public `Machine.call` records the name and owned `std::any` argument list
  before callback lookup, then invokes the supplied lookup result. Missing
  lookup/functions return integer zero. Results are returned unchanged, including
  empty `std::any` (the None counterpart), large integers, bytes, strings and
  user-defined objects. Pass mutable object identities as shared pointers;
  copies in call history then observe the same mutations. Lookup or invocation
  may reenter or throw; preceding calls and writes remain recorded.

The existing scalar/typed/position gameplay callback adapters keep their API and
trace representation. The unrestricted `Callbacks::lookup` boundary serves
public general `Machine.call`; it does not replace those gameplay adapters.
External engine bodies remain caller-owned callbacks throughout.

The corpus adds 2,002 cases (31,920 total) covering the complete family:
wide signed addresses/values, arbitrary widths, overlapping sparse bytes,
backing calls/results/order, callback mutations and exception prefixes, and
unrestricted call arguments/results. Every original 29,918 row remains
byte-for-byte identical as the corpus prefix. API tests additionally verify
snapshot timing, zero/negative widths, mutable object identity, recorded-before-
lookup ordering, lookup failure, nested calls and retained writes on exceptions.

## Local verification record (generic backing/callback family)

Apple Silicon macOS, Release. Eight files changed: this page, the existing
message header/source, trace driver, API tests, generator, and two vector files.
No shared build lists, research files, other modules or private inputs changed.

Exact successful commands (the comparison script is ignored scratch and checks
complete stdout against Python expectations before hashing it):

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re --output work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.tsv.gz work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.json work/messages-reproduced.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
build/native/champions-messages-tests
mkdir -p build/messages-x64 build/messages-sanitized
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/core_arithmetic.cpp native/src/core_math.cpp native/src/core_state.cpp native/src/items.cpp native/src/item_generation.cpp native/src/item_names.cpp native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-x64/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tools/messages_trace.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tests/messages_tests.cpp build/native/libchampions-items.a build/native/libchampions-core.a -o build/messages-sanitized/api
build/messages-sanitized/api
python3 work/messages-compare.py
ctest --test-dir build/native --output-on-failure -j 3 > work/messages-full-ctest.log 2>&1
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Generation, byte-for-byte deterministic reproduction, the old-corpus prefix
comparison, and Release builds passed. The final targeted CTest passed **3/3**
in **25.12 seconds**. Native and sanitized API runners passed **71 checks** each.
All **31,920** complete traces passed on arm64, x86_64 under Rosetta, and sanitized
arm64, with identical SHA-256:
`67423eaf2930f0e34c5954d1e5490a2a4bfc664db27d945c9dbefe1a9b5383cf`.
The message source, driver and API tests were instrumented with ASan/UBSan;
unchanged item/core libraries were linked in Release without instrumentation.

An initial trace check started before the rebuilt driver was ready and rejected
its new actions; the completed build passed. An API fixture initially had an incorrect
expected bitwise OR result for wide backing byte results; its expectation
was corrected, without changing the reference or load behavior. A final API
check verifies that a backing callable can replace itself while the selected
callable stays alive and later missing bytes use the replacement.

Full Release CTest passed **103/103** in **673.59 seconds**, with `-j 3`,
including the final message API and vector tests. No timeout rerun was needed.
`git diff --check` passed and the research checkout remained clean.

Windows, physical Intel-host execution,
original executable parity, private-content acceptance, Norrath and Session/scene
integration remain unverified. No private input or proprietary content was used.
