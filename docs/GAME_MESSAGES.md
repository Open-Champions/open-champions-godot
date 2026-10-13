# Return to Arms game messages, abilities and HUD controls

The standalone `champions-messages` C++17 library implements a bounded subset of
Return to Arms (SLUS_209.73, US v2.00) in `champions::messages::rta`. It is independent
of Godot, sockets and Session. Ability bonuses use the existing exact
`champions-core` arithmetic operations. This is message
transport/model parity, not inventory gameplay or a playable port. Norrath is
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

Specification sections 1, 3, 5.6, 7 and 11 and the corresponding reference
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
HUD dispatch remains refused until the other HUD families are implemented.

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

## Exact verification

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

## Precisely remaining work

`Machine.deliver` refuses the inventory, game/HUD and shop handler addresses with
`Unsupported`, before executing their bodies or making callbacks. Online send
can queue these payloads without claiming their execution is supported.

Unported reference methods: `inv_dispatch`, `send_character_to_lobby`,
`same_identity`, `item_object`, `check_equip_allowed`, `item_value`, `grid`,
`remove_item`, `give_item`, `first_recall_hint`,
`unref_item`, `drop_item`, `equip_item`/`_equip_item`, `unequip_item`,
`on_join_panel`, `on_stat_point`, `on_skill_point`, `hud_on_msg`, `recall`, `stop_stream`,
`on_player_left`, `on_disconnect_request`, `set_pending_level`, `on_checksum`,
`shop_on_msg`, `shop_buy`, `shop_sell`, and material `sound_name`.
Only the internal record, signed-read, c-string and sound helpers needed by this
family were added; general copy/wide-string/vector helpers remain work.
Gem combining remains an external callback in the reference, not implementable
from this model's observations alone. No game/HUD/shop body is replaced with an
invented callback or successful no-op.

Original stack-buffer reuse on network ticks, ready-frame integration, inventory
and player ownership adapters, sockets, original executable parity, original
content acceptance, Norrath behavior, Windows execution, and native Intel-host
execution remain unverified. The local arm64 and x86_64 (Rosetta) runners
match all 8,366 expectations and each other's complete state traces. Their common
trace SHA-256 is
`ec1889ea361bd2620e9d2b7d57798872fa367d4813eb27d77c47064a8d7eda8f`.
This does not establish a Windows or physical Intel-host result.

## Local verification record (continuation)

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
