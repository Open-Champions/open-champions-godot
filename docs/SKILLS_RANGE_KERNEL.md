# RTA range arithmetic and signed virtual-this kernel

Reconciliation written before extraction on `c538b65175363d2ed232cc94ae46ed8b94b815eb`.
Evidence is immutable research commit `356add27d50e50972b2a671b31e8f4ec93e6778c`;
all 16 evidence hashes in the existing kernel/port manifests were verified against
both Git objects and read-only working files. Current research HEAD is not a pin.
The latest integration proposal selects this boundary, not whole range Models.

| Selected expression / C++ call site | Exact pinned Python method | Proven common surface / order |
|---|---|---|
| A `Model::skill_stat_range`, B `Model::stat_range` pointer overload, C `execute(Operation::Range)` fixed scale | A `Model.skill_stat_range` lines 404–423; B `Model.skill_stat_range` lines 615–638; C `Base.skill_stat_range` lines 449–475 | raw32 `mul(value, spread)` |
| Same three randomized scale sites | Same methods; A `frac`, B `Model.unit` (lines 354–356), C inline draw fraction | raw32 draw integer converted by `from_int`, multiplied by `TWO_M16`; then `mul(value, fraction)`, then `mul(product, spread)` |
| Same three low/high combine sites | Same methods' ordered `(sub, add)` loops | raw32 `sub(value, scaled)` for low, `add(value, scaled)` for high; no reassociation or fused operations |
| A `skill_dispatch` (including `first_skill_call`) | A first `Model.skill_call` lines 238–248; final override lines 2808–2820 delegates to it | low32(pointer + sign-extended low16 entry) |
| A `table_call`, `vcall`, `spell_dispatch` adjustment sites | A `Model.table_call` lines 250–259, `Model.vcall` lines 230–236, creature `Model.table_call` lines 422–426 | same adjustment only; body recognition and read order stay local |
| B `virtual_call`, generic `table_call` | B `Model._virtual` lines 294–307; `Model.table_call` delegates generic calls to creature `Model.table_call` | same adjustment; explicit `offset` validation remains before reads in wrappers; direct `virtual_call` historically accepts an Address and publishes its low32 residue |
| C `invoke`, `table_call` | C `Base.skill_call` lines 208–220; arena `Model.table_call` lines 308–318, with creature `Model.table_call` lines 422–426 fallback | same adjustment after C's raw32/slot validation; signed `half` reader remains local |

The kernel accepts already available raw32 arithmetic operands, a draw value
already obtained by the adapter, or pointer residue and already-read adjustment
bits. It neither loads tables nor calls callbacks. Its pointer input is in the
adapter's existing admitted domain; it does not enlarge or validate that domain.
For the proven adapter domain, `value = from_int(slot10_result)` (signed raw32
integer conversion), `spread` is any raw32 float pattern, and `draw` is an
already acquired integer in 0..65535. Outputs are raw32 float patterns:
`fixed = mul(value, spread)`;
`random = mul(mul(value, mul(from_int(draw), TWO_M16)), spread)`;
`low = sub(value, scaled)` / `high = add(value, scaled)`.
Each nested operation completes separately under the current FPU configuration.
No float-to-int conversion or predicate is part of the common surface. The raw
primitive checks also exercise arbitrary `value` patterns independently.
`adjust_this(pointer, bits)` publishes `(pointer + s16(bits)) mod 2^32`;
upper bits of the already-read entry are ignored, with no table-address wrap.

A's raw32 object API, B's checked wrapper offsets/direct virtual low32 publication,
and C's Address/raw32 refusal remain distinct. Signed16 extremes are -32768..32767.
All integer arithmetic wraps only at the existing low32 publication point.

Each range calls slot 10 first and converts its signed32 result. Fixed ranges
make no draws. Randomized ranges make probe draw then bound draw, low before high
(four total), with no common RNG authority introduced. A/C select rounding with
`le(ZERO, probe)`; B selects negative rounding with `lt(probe, ZERO)` before its
second draw. Unordered/NaN choices, to-int conversion and clamp checks remain in
the adapters. A/C publish a pair only on successful return. B stores each
unclamped result immediately and may store 1 next; low/high/seed/input aliases
therefore remain observable before the next draw. No memory reads, callback
arity, callback replacement, external dispatch, slot bounds, exceptions/prefixes,
per-skill formulas, lifecycle, statistics/tier or LCG behavior are shared further.
C's inline CreatureEnemy adjustment is retained: it is a per-skill service path,
outside the selected generic virtual dispatch sites. Spell construction and
handle/service ownership await separate reconciliation.

## Baselines and validation

Before source edits, nine historical fixture/manifest files were hashed with
byte lengths and decompressed SHA-256 in `work/skills-range-kernel/baseline.json`.
The original three complete-state corpora and existing kernel corpus are immutable.
Executed results and final identities follow after validation.

## Unchanged historical identities

| File | Bytes | SHA-256 |
|---|---:|---|
| `skills_a.json` | 14732 | `3babd70494a15f0597a4f21afda6e60efb4e7afd4a378fe11b8c61734e754884` |
| `skills_a.tsv.gz` | 98800516 | `4db2684e8c717fa33aa01a40d8ebae4aa1623868e630a2b3e6d271b46f2f4c28` |
| `skills_b.json` | 22221 | `56f4e158147350c26fe0dacb2c0894d98081ce2ff3154231dd958f2fae10e841` |
| `skills_b.tsv.gz` | 72776656 | `d4ba43e90707da6d8d20c6f2c170c36c718a1f899bd76f963561c75f774eea8c` |
| `skills_c.json` | 1202 | `bb6dd018c2f9b2c98212e9742aeefb41b71c39b57ab202b611e154ce997be4a5` |
| `skills_c.tsv.gz` | 259348725 | `1b063a7631b188b6e30203ca5fd8b6e21f9c9c270f4f3ecb392f567a60e51fb7` |
| `skills_c_evidence.json` | 781 | `b624c166601b5af6a8f3bb989f24d2dcbaca6e96c4e1e37584e46d2756416320` |
| `skill_kernel.json` | 1904 | `aa5b58a9c508992a86e3d98586dab0a2fb058a4eebc8480189fc9632d940d1a2` |
| `skill_kernel.tsv.gz` | 280078 | `189a30efb721f6a2b4a384c2a32d35c960890abdab7f2f1a27dc5bb9029a518d` |

| Plain corpus | Complete observations | SHA-256 |
|---|---:|---|
| `skills_a` | 149,569 | `0537634d9def4db8daa996cc441a7e0a12be449f3b2c2b24e55418f0cc116288` |
| `skills_b` | 174,352 | `519ffc8d27f9a5f0cc5cd643ab2b02b1423c9b93148c7985173547b3eba8fb0a` |
| `skills_c` | 149,648 | `1881bfb823b26fa03e971f20a4698fe0f329c0fbdbac60a6db21c335de0aad46` |
| `skill_kernel` | 11,136 | `8939885e879fe935f6992fcf618731b35cefeb51c107eb65aff0b2c4ad0f876d` |

## Extracted surface and exact coverage

Four stateless inline primitives extend the existing header: `range_scale`,
`range_random_scale`, `range_combine`, `adjust_this`. All three selected range
adapters and generic virtual routers use them. No public declarations, simulation
translation units, central registration, dependency or gameplay dispatch changed.
Existing statistic/tier/LCG primitives and per-skill formulas are untouched.

The separate `skills_range_kernel.tsv.gz` and JSON manifest contain **39,744**
independent observations. `generate_skills_range_kernel_vectors.py` checks all
16 immutable source/specification/executed-note hashes before importing the
pinned Python Models and core. Expected outputs never come from native output.
Authored Memory wrappers inject read failures and the existing native store-history
budget boundary; they do not change reference range arithmetic or virtual dispatch.
Normal checks require only the new fixture and generator integrity hash.
`test_skills_range_kernel_vectors.py` uses the existing executable's separately
named `--range-trace input output` option; historical transport remains unchanged.

Coverage spans all 16 FPU configurations, fixed/randomized ranges, signed32
statistic-result extrema, zeros, adjacent half-rounding patterns, NaNs, infinity,
subnormals and raw arithmetic operands not reachable through statistic conversion.
Independent raw checks include draw values 0, 1, 32767 and 65535. Probe and bound
use separate ordered draws, including a spread of two with opposite probe/bound
signs. Six layouts cover ordinary outputs, either/both outputs on the shared seed,
statistic input addresses and coincident outputs. Budgets 0..7 expose immediate
unclamped/clamp writes and failures between output stores and subsequent draws.
The unchanged historical kernel corpus additionally reads the aliased statistic
inputs through their original statistic body (the new range corpus supplies a
synthetic callback result). No attempt is made to unify these publication forms.

Signed16 adjustments cover 0, 1, 32767, -32768 and -1 at zero, small and high
raw32 object addresses, including wrapping adjusted `this`. Virtual cases test
failures at table/entry/body reads, callbacks throwing before/after an own draw,
self-replacement with a second call, and forced external dispatch of a recognized
statistic body. Every observation compares return/refusal message, full Model
write history (A/C), Memory store history, ordered callback arguments, shared draw
count and complete sparse memory including both seed words. B has no Model write
sink. Its Memory histories remain distinct. Existing host checks retain per-port
player callback arity and reader self-replacement checks; focused API additions
assert A's pre-read slot refusal, B's table-before-slot-check and checked-offset
refusal, B's admitted wide direct-virtual pointer residue, and C's pre-read
address/host-slot refusal. No wider pointer or slot domain was introduced.

Changed files: `native/include/champions/detail/skill_kernel.hpp`, the selected
range/virtual-adjust expressions in `native/src/skills_{a,b,c}.cpp`, focused API
checks in `native/tests/skills_{a,b,c}_tests.cpp`,
`native/tests/skill_kernel_tests.cpp`, `native/skill_kernel_targets.cmake`, the
two new range tools and two new range fixture/manifest files, this page and a
small link in `SKILLS_KERNEL.md`. No historical vectors/generators/manifests,
port headers, integration notes or other agent's files were edited.

## Executed verification

Pre-extraction arm64 Release baseline passed **9/9 in 54.93 s**. After extraction,
the affected suite passed **10/10 in 61.71 s** on arm64 and **10/10 in 84.27 s**
on x64 through Rosetta. After expanding the new gap cases, all three kernel cases
passed again: arm64 **3/3 in 0.82 s**, x64 **3/3 in 1.56 s**. There were no exact
observation mismatches. An initial API-test compile failed because a B host-check
block was inserted in the trace function; it was moved into `host_checks` and
both architectures rebuilt successfully. This was a test placement error.

Exact build/generation/test commands (logs under ignored
`work/skills-range-kernel/`; build outputs under `build/`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-|skill-kernel-' --output-on-failure -j 3
python3 tools/generate_skills_range_kernel_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target champions-skill-kernel-tests -j 4
python3 tools/test_skills_range_kernel_vectors.py --exe build/native/champions-skill-kernel-tests
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-' --output-on-failure -V -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skill-kernel-tests champions-skills-a-tests champions-skills-a-trace champions-skills-b-tests champions-skills-c-api-tests champions-skills-c-trace -j 4
ctest --test-dir build/native-x64 -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-' --output-on-failure -V -j 3
cmake --build build/native --target champions-skill-kernel-tests -j 4
ctest --test-dir build/native -R 'skill-kernel-|skills-range-kernel-' --output-on-failure -V
cmake --build build/native-x64 --target champions-skill-kernel-tests -j 4
ctest --test-dir build/native-x64 -R 'skill-kernel-|skills-range-kernel-' --output-on-failure -V
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_range_kernel_vectors.py tools/test_skills_range_kernel_vectors.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
file build/native/champions-skill-kernel-tests build/native-x64/champions-skill-kernel-tests
cmp build/native/core-traces/skills_a_actual.tsv build/native-x64/core-traces/skills_a_actual.tsv
shasum -a 256 build/native/core-traces/skills_a_actual.tsv build/native-x64/core-traces/skills_a_actual.tsv
```

Before edits, an inline Python check recorded byte lengths and SHA-256 of the nine
historical files and SHA-256 of each decompressed corpus. For every manifest pin,
it checked `sha256(git show PIN:path)` and `sha256(research/path)` against the
recorded digest. After extraction, a second inline check compared every byte length,
compressed/file hash and decompressed hash to that snapshot: **all nine unchanged**.
Python compilation and whitespace checks passed; research tracked status stayed
clean. The new manifest separately pins its generator, sources and corpus hashes.

## Remaining boundaries and progress

This is native RTA numeric consolidation only. It adds no skills to gameplay,
no reference units, constructors, handle/service ownership or Session/scene wiring.
The original callback interfaces and failure prefixes remain local. Unsupported
private acceptance measurements were skipped because `../../private-validation`
is unavailable; no other locations were searched. No private assets, decompiler
output, retail bytes or proprietary toolchains were imported. Norrath, Windows,
Intel-host execution, console fidelity, visual/exported gameplay and original
engine integration remain unverified. x64 execution here uses Rosetta on Apple
Silicon. All work stays local; no push, PR or issue action.

Module A
PROGRESS: 195 of 196 reference functions ported

Module B
PROGRESS: 239 of 239 reference functions ported

Module C
PROGRESS: 175 of 175 reference functions ported

A's unrelated `gameobject_ctor` refusal remains unchanged and blocked on its
existing constructor evidence; B/C remain complete as standalone contract ports.

New plain corpus SHA-256: `cfe892c6692fd332cc5347acc5cc3b31c3db7ff7908ea9619bf895da1858dd67`.
New compressed corpus SHA-256: `0265a0c5a7d32f2f94c230d79fdb4cc9ced56bfe689bbec04fae69c36ac4699b`.


The existing complete-trace route checked every reference token on both
architectures and reported identical observation hashes:

| Trace | arm64 and x64 SHA-256 |
|---|---|
| A (149,569 states) | `6977791fa8f8edf43f12c2439dc17f2d38fe3db43e9ee6237008a4bba1451ee7` |
| B (174,352 states) | `8cc3de69a6ac850569dce235ca4e17e69bb357bf8336047b03c2ce083d961362` |
| C (149,648 states) | `3143476e0e3d0e81cfc96e873802ab8d4ac23e8f3344b7e92d43d08f3edfb682` |
| Historical kernel (11,136 states) | `ca225d495f76c1ee19185da6814b857a01547f2bc8e79e868f888ccfee97dc11` |
| New range/virtual (39,744 states) | `7fcebc8e84068a62bc5cc3fc8c286bc2fe48ddd501a85d6a1abc86f9e12c3c2e` |

The final required arm64 Release full sweep passed **142/142 in 822.63 s**
(`ctest --test-dir build/native --output-on-failure -j 3`). All historical skills
and both kernel corpora passed again inside that sweep. No test failed or timed
out, so no isolated timeout retry was needed. Final Release builds had no compiler
warnings. The explicit A complete-trace `cmp` also passed; both files have the A
observation hash above. All port-manifest evidence entries were checked directly
again (A: 8, B: 7, C: 7; 16 unique pins) and match the new manifest.
