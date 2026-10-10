# Native core game systems

`champions-core` ports the published Return to Arms Python reference model into
portable C++17. It has no Godot, archive, animation-viewer or rendering dependency.
It provides rules for future gameplay systems; the browser does not run a game.
The namespace `champions::simulation::rta` makes the currently validated title
explicit. Norrath requires its own behavior evidence before sharing these rules.

## Contract and coverage

The contract is authored research `tools/ref_core.py`, at commit
[`41e847af5360724ab62d208594f8027ea3c1b686`](https://github.com/Open-Champions/champions-return-to-arms-re/blob/41e847af5360724ab62d208594f8027ea3c1b686/tools/ref_core.py),
SHA-256 `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123`.
Its MIT license and contributor copyright apply to this authored translation.
The companion [core specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/41e847af5360724ab62d208594f8027ea3c1b686/docs/spec/core.md)
describes the reference's arithmetic and evidence limits.

| Component | Implementation | Python-generated checks |
|---|---|---:|
| Arithmetic | Integer-backed single FPU, conversions, comparisons, software doubles and double remainder; 29 operations | 435,998 |
| Math | Angles, scalar/color/vector/box/matrix/quaternion operations, noise and library math; 76 modeled functions/aliases plus coefficient audit | 343,392 |
| State | Separate random domains, libc RNG, pool metadata, paired handle lists and mutation-safe iterators | 4,862 |
| Total | Exact results, including complete state after state operations | **784,252** |

The sine and cosine corpus covers every 16-bit angle. Math checks include 787,344
raw output words. Arithmetic and math exercise all four adder models and both
multiplier/division options; RNG state checks cover the same 16 configurations.
State traces include 3,520 RNG operations, 53 pool operations (including batches),
and 1,289 handle/iterator operations. Pool traces compare every address/link,
head, base, end and last allocation; handle traces compare the complete symbolic
pointer graph, object IDs/flags, iterator fields and retained freed-node fields.

Host-float diagnostic helpers (`f32`, `to_float`, `d64`, `to_double`) and runtime
decimal parsing (`const`, `dconst`) are intentionally absent. Test inputs and
constant tables use generated raw patterns. The reference does not model pool
payload bytes; this port likewise models allocation metadata rather than a heap.

## Arithmetic boundary

Gameplay values are `Raw`/`DoubleRaw` integer bit patterns. Operations do not use
host floating-point instructions, host libm, compiler extensions such as
`__int128`, or additional libraries. Wide intermediates use fixed 32-bit limbs.
The default FPU is the model's `ps2` variant; alternative settings are available
for sensitivity checks. Free-function contexts are thread-local, while explicit
`Fpu` instances retain their own settings. Future peers must agree on settings.

Each single operation preserves the reference's truncation, exponent-zero
behavior, exponent-255 finite numbers, signed zeros, saturation and conversion
rules. Software doubles use their separate reference rules. Trigonometry uses
the game's 16-bit polynomial and coefficient patterns, including nonzero
`sin16(0)`. Matrices use row vectors; gameplay quaternions are x,y,z,w.

The existing binary64 pose/skinning operations remain presentation tools. They
are not imported by `champions-core`. A registered source-boundary check rejects
host float/double types, host math/random headers and presentation includes in
the simulation sources and headers.

Exact agreement establishes **Python reference-model parity**. The reference was
compared with original instructions in an isolated interpreter, rather than a
console. Last-bit hardware FPU variants remain unsettled. These checks do not
establish console fidelity, a linked PS2 game, gameplay integration, lockstep
network compatibility or Norrath compatibility.

## Reproduce and validate

Build and run CTest as described in the native README. Testing also requires
Python 3 for unpacking fixture data and validating trace artifacts. Checked-in
fixtures are deterministic gzip files; no research checkout or game files are
needed for ordinary builds and tests.

To regenerate one fixture, explicitly select the external research checkout:

```sh
python3 tools/generate_core_arithmetic_vectors.py --research-dir /path/to/champions-re --output work/core_arithmetic.tsv
python3 tools/generate_core_math_vectors.py --research-dir /path/to/champions-re --output work/core_math.tsv
python3 tools/generate_core_state_vectors.py --research-dir /path/to/champions-re --output work/core_state.tsv
```

Generators require the exact source pin and record source/generator/vector
hashes and coverage in companion manifests. They do not change the research
model. Native runners compare every result exactly and emit actual result/state
records. The wrapper checks fixture hashes and coverage and deletes previous
evidence before a rerun, so a failed invocation cannot reuse old success.

The native workflow runs all three suites on Apple Silicon and Intel macOS,
ARM64 and x64 Windows, and Linux x64. It uploads normalized actual traces, then
requires byte-identical traces across all five hosts. Comparison validates the
source commit, manifests, fixture identity, actual trace hashes, counts and
complete suite/platform inventory. Missing, changed or divergent results fail.
Expected trace hashes are derived independently from the Python-generated
vectors, so identical wrong results on every host also fail. Checkout attributes
keep source and manifest line endings at LF for identical provenance hashes.

Local verification: all three optimized suites pass on Apple Silicon, as do
their ASan/UBSan runs. The integrated native suite passes 20 CTest cases;
artifact tests reject stale evidence, missing hosts, changed provenance,
tampered data and internally consistent but divergent traces. Platform runtime
proof is supplied by the workflow for the implementation commit.

## Remaining game systems

The [item model port](ITEM_SYSTEMS.md) now supplies records, stats, generation,
loot, shops and localized names. The [combat port](COMBAT_SYSTEMS.md) adds damage,
statistics and experience. The [AMX script machine](SCRIPT_MACHINE.md) follows
with exact state tests; the network-wire model is next. Item,
combat, script, save, collision, navigation and networking behavior have not
been integrated into a playable scene. Viewer features are not being extended.
Reflection, shell and facing-point model coverage remains a later decoder task.
Byte-exact RTA reconstruction is parked until a period PS2 `ee-gcc` toolchain
is used with function names from research `names.tsv` and `symbols/`.

Native sources were formatted with clang-format 20.1.8, LLVM style, recorded in
`.clang-format`. The texture decoder now separates header validation, packet
dispatch, register transitions, IMAGE decoding and final palette sampling.
Acceptance rules and pixels are unchanged: existing tests pass, and complete
pre/post decoded output streams from available private samples match byte for
byte. Evidence is included with this implementation, without an evidence-only
commit.
