# Return to Arms game-message transport and encoders

The standalone `champions-messages` C++17 library implements a bounded subset of
Return to Arms (SLUS_209.73, US v2.00) in `champions::messages::rta`. It is independent
of Godot, sockets, Session, and other simulation modules. This is message
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

Specification sections 1, 7, 11 and the reference's `msg_*` functions define this
subset. The differential harness documents original-execution comparisons for
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

The committed deterministic gzip corpus contains **4,491 input-driven cases**.
The C++ trace driver receives only action scripts. Python-generated expectations
independently compare the entire sparse byte map (including explicit zeros),
ordered scalar writes, callback names/arguments/buffer bytes, results and
exception prefixes. Cases cover every encoder, field truncation, registration
holes/duplicates/full-table overflow, repeated startup, raw slot patterns,
nonzero signed network modes, truncated length with complete payload, ring laps
and overwritten data, sequential sends, direct/offline delivery, callback
mutation and exceptions. The host API test separately covers absent callbacks,
reentrancy, bad arguments and the unsupported-handler refusal boundary.

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
`same_identity`, `item_object`, `check_equip_allowed`, `item_value`, `skill_rank`,
`set_skill_rank`, `passive_level`, `raise_passive_level`, `rebuild_abilities`,
`add_skill_ability`, `grid`, `remove_item`, `give_item`, `first_recall_hint`,
`unref_item`, `drop_item`, `equip_item`/`_equip_item`, `unequip_item`,
`on_join_panel`, `on_stat_point`, `on_skill_point`, `hud_on_msg`, `on_quick_slot`,
`on_assign_spell`, `mask_local_buttons`, `on_menu_state`, `recall`, `stop_stream`,
`on_player_left`, `on_disconnect_request`, `set_pending_level`, `on_checksum`,
`on_quick_cast`, `shop_on_msg`, `shop_buy`, `shop_sell`, and material `sound_name`.
Their memory/string/vector/sound convenience helpers are also not exposed.
Gem combining remains an external callback in the reference, not implementable
from this model's observations alone. No game/HUD/shop body is replaced with an
invented callback or successful no-op.

Original stack-buffer reuse on network ticks, ready-frame integration, inventory
and player ownership adapters, sockets, original executable parity, original
content acceptance, Norrath behavior, Windows execution, and native Intel-host
execution remain unverified. The local arm64 and x86_64 (Rosetta) runners both
match all 4,491 Python expectations and their entire output traces match each
other. Their common full-state trace SHA-256 is
`e73d28065d7686e333a8d4e687fbd8ca33bc382650946a29acdc4edecd9f136f`.
This does not establish a Windows or physical Intel-host result.

## Local verification record

Apple Silicon macOS, AppleClang 21.0.0, Release build. The following commands
passed; the full regression result is recorded in the completion report:

```sh
python3 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re
/opt/homebrew/bin/python3.14 tools/generate_messages_vectors.py --research-dir ../../champions-return-to-arms-re --output work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.tsv.gz work/messages-reproduced.tsv.gz
cmp native/tests/vectors/messages.json work/messages-reproduced.json
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^messages-' --output-on-failure
python3 -B -c 'import unittest, runpy; runpy.run_path("../../champions-return-to-arms-re/tools/test_ref_messages.py", run_name="__main__")'
mkdir -p build/messages-sanitized
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-sanitized/trace
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-sanitized/api
build/messages-sanitized/api
python3 tools/test_messages_vectors.py --native build/messages-sanitized/trace
mkdir -p build/messages-x64
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/messages.cpp native/tools/messages_trace.cpp -o build/messages-x64/trace
c++ -arch x86_64 -std=c++17 -O2 -Wall -Wextra -Wpedantic -I native/include native/src/messages.cpp native/tests/messages_tests.cpp -o build/messages-x64/api
file build/messages-x64/trace
arch -x86_64 build/messages-x64/api
python3 tools/test_messages_vectors.py --native build/messages-x64/trace
git diff --check
```

The message CTest subset passed 3/3, each API runner passed 11 checks, and both
sanitized and x86_64 runners passed 4,491 exact cases. The research synthetic
unit suite passed 17/17. Initial direct invocation
`python3 -B ../../champions-return-to-arms-re/tools/test_ref_messages.py` failed
because research `tools/dis.py` shadowed standard-library `dis` during
`unittest` import. Preloading `unittest` as above resolved it without research
edits. No executable-dependent research harness was run.

Changed files: `native/include/champions/messages.hpp`,
`native/src/messages.cpp`, `native/tools/messages_trace.cpp`,
`native/tests/messages_tests.cpp`, `native/tests/vectors/messages.tsv.gz`,
`native/tests/vectors/messages.json`, `tools/generate_messages_vectors.py`,
`tools/test_messages_vectors.py`, `native/messages_targets.cmake`,
`native/messages_tests.cmake`, this page, and minimal additive lines in
`native/CMakeLists.txt`, `native/simulation_targets.cmake`, `docs/ROADMAP.md`.
No other module or research file was changed.
