# Native item game rules

`champions-items` ports the published Return to Arms Python item model to C++17.
It links only `champions-core` and the standard library. The namespace
`champions::simulation::rta::items` identifies the validated game explicitly.
These are simulation components for future integration, not a playable client.

## Pinned contract

The contract is [tools/ref_items.py at 14f2f2d](https://github.com/Open-Champions/champions-return-to-arms-re/blob/14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512/tools/ref_items.py),
SHA-256 `513edad0b8eacf620c5d15f21c42a03a1adfd6f19e4f5620b80c21dc2f4bc75e`.
The [item specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512/docs/spec/items.md)
provides its behavior and evidence limits. Its MIT license and contributor
copyright apply to this authored translation. No behavior was rederived from
raw decompilation, and no original table contents or assets are shipped.

| Component | Public header | Exact generated cases |
|---|---|---:|
| Item records, parser, stats, value, equip reasons and resources | `items.hpp` | 113,999 |
| Generation, shop retries, world callbacks and loot | `item_generation.hpp` | 6,403 |
| Localized grammar, affixes, formatting and names | `item_names.hpp` | 4,458 |

The record suite covers every signed16 input, arbitrary integer and arithmetic
patterns, all ten equipment outcomes, descriptors, stale record bytes and error
paths. A 0x48-byte serialization writes only the model-defined fields and
preserves caller-supplied padding. Unpack reads through 0x43, accepting 68–71-byte
inputs like Python. Identity and mutable-copy operations follow modeled fields;
they do not infer raw retail stack-padding comparisons or bag-index writes.
Unknown definition addresses become a missing definition rather than a pointer.

Generation tests compare output records, final RNG seed/draw count, and ordered
allocation, gold, throw and terrain callbacks. They exercise all loot endings,
allocation failures, duplicate stock retries, a retry producing nothing,
callback consumption of randomness, item mutation and exceptions after draws.
Naming tests compare Unicode codepoints and lookup order, including grammar
languages, non-BMP characters, combining marks, NUL, missing text, formatting
failures and the 128-character boundary. UTF-8 test serialization is independent
of Windows' and macOS' different `wchar_t` widths.

## Arithmetic and caller-owned inputs

All gameplay numeric values use integer-backed raw patterns. Item operations
select the contract's fixed `Ps2,false,false` arithmetic, even when a caller has
selected an alternative core FPU configuration. `ScopedFpu` restores the complete
thread-local configuration on nested calls and exceptions. The item suite cycles
all sixteen caller settings and checks restoration. Positive half-exponent
`powf` remains unsupported where the item model leaves it unsupported.
Viewer binary64 math and engine physics are not simulation dependencies.

`Tables` contains caller-supplied definitions, presets, modifier formats,
prefix/suffix text, quality rows, value multipliers and resource name keys.
Addresses are explicit 32-bit serialization identities, never host pointers.
The Python table loader reads a private executable; this port intentionally
accepts typed inputs instead. Missing supplied rows fail rather than dereferencing
unavailable memory. Synthetic generators construct tables without loading an
executable or publishing its contents.

`World` supplies allocation, construction and positioning behavior. Its default
events retain owned item snapshots, whereas Python's default list retains mutable
object aliases. This lifetime boundary is explicit: event records describe values
at the callback, not later alias mutation. Caller overrides may mutate items,
consume RNG draws, fail allocation or throw; tests preserve callback order and
state. No constructor, free-space-search or terrain draws are invented. Gold
scatter uses an explicit ground-height callback. Sound and physical launch
behavior are outside this model.

## Reproduce the checks

The normal native CMake build and CTest run all item tests without game files or
a research checkout. Committed deterministic gzip fixtures carry manifests with
reference, generator and plain-vector hashes. To regenerate, explicitly select
the external research checkout at the pin above:

```sh
python3 tools/generate_items_vectors.py --research-dir /path/to/champions-re --output work/items.tsv
python3 tools/generate_item_generation_vectors.py --research-dir /path/to/champions-re --output work/item_generation.tsv
python3 tools/generate_item_names_vectors.py --research-dir /path/to/champions-re --output work/item_names.tsv
```

All three optimized suites and their ASan/UBSan builds pass on Apple Silicon.
The shared wrapper verifies complete fixture identity and independently derives
expected traces from Python result fields. Actual C++ results are serialized and
compared exactly; stale reports are removed before each attempt. CI requires all
six core/item suites on all five native hosts and rejects identical wrong results
on every host. Each report identifies the exact implementation commit.

The evidence establishes pinned Python-model parity with synthetic inputs.
Console fidelity, original-data loading, gameplay integration, save support,
network compatibility and Norrath behavior require independent evidence.
Combat, AMX and network-wire ports follow in that order. Rendering coverage
remains a later task, and RTA byte-exact reconstruction stays parked pending the
period compiler.
