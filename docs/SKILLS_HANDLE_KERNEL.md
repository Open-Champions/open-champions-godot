# RTA shared handle primitives

Reconciliation recorded before extraction from integration base
`4a5b8f528c76d09b14d014fdb95dedf30be7044d`, using immutable research
`356add27d50e50972b2a671b31e8f4ec93e6778c`. All 16 existing range/kernel
manifest evidence hashes and `docs/objects.md` were verified against Git objects
and read-only working files. Current research HEAD is not the oracle.

| Candidate | Pinned Python evidence | Selected boundary |
|---|---|---|
| B `node_take`, C `handle_take` | B `_node_take`, C `_h_take` | Read free head; if nonzero read successor and publish it; then clear exactly the 16-bit tag at +0x10 |
| B `node_free`, C recycle tail of `handle_unlink` | B `_node_free`, C `_h_unlink_free` | Read prior free head, store into node, then publish node as free head |
| B `handle_link`, C `HandleLink` | Both `handle_link` methods | Two separate takes, front insertion, back insertion, then front object, front partner, back object zero, back partner |

The common surface has injected ordered reads, writes, address arithmetic and
per-port insertion; it owns no state or pool. Offsets are evaluated only at their
original store points. B keeps checked `offset` and direct Memory stores; C keeps
wide Address expressions and its `write` service (pre-read, Model history entry,
then Memory store). Widths, refusal messages and partial effects remain local.
Exhausted takes return zero; later insertion and field stores still run, including
admitted address-zero writes. No new allocation failure behavior is introduced.

B `node_push` caches the first pointer before storing to the node; C insertion
rereads the head after that store (and again for its backlink destination).
Aliased node/head cells expose a difference. B `node_unlink` caches prev/next,
then rereads next after a prev write. C repeats its reads in conditions and write
arguments. B `partner_drop` has its own read schedule. These are retained verbatim,
not generalized. Clear and iteration loops, pointer validation, traversal limits,
history bounds, stack callbacks/services, and A's external handle callbacks stay
in their adapters. No undocumented behavior is reconciled.

The larger `ref_core.HandlePool` candidate owns Python Node objects, initializes
a pool, and refuses exhaustion/unsafe clear inputs. Its `_take`, `_release`,
`_push` and `_unlink` illustrate the layout but are not interchangeable with
these sparse-memory B/C contracts. No owning pool, exhaustion refusal, iterator
rule or safe-clear policy is imported from that candidate.

Baseline identities are recorded in ignored
`work/skills-handle-kernel/baseline.json` before source changes. Private acceptance
is skipped because `../../private-validation` is unavailable; no alternate paths
are searched. This is reusable native spell support, not gameplay integration.

Skills A:
PROGRESS: 195 of 196 reference functions ported

Skills B:
PROGRESS: 239 of 239 reference functions ported

Skills C:
PROGRESS: 175 of 175 reference functions ported

A's constructor refusal remains unresolved. No reference-function credit is added.

## Implementation and independent tests

Both B and C now call header-only `skill_handle_kernel::take`, `recycle`, and
`paired_link`. C has private `handle_insert` and `handle_recycle` adapters; B's
existing helpers remain. No whole Model or generic handle pool is introduced.

The separate corpus has 6,302 full-state observations authored by the pinned
Python B/C methods. Its only policy overlays model existing native Memory store
budgets and C's preflight/Model write-history boundary. It never consumes C++
output. It covers empty/one/two-node pools, nonempty lists, paired and partnerless
removal, reused nodes, repeated link/clear, head/back/free-head/node aliases,
16-bit tags and neighboring bytes, and store/Model history limits 0..50 (every
write boundary of the longest sequence). Cyclic Python clear runs only with a
finite injected store budget. Successful direct operations have empty callback
records, which are asserted; the unchanged historical/API tests preserve stack
services and callback records separately.

The explicit alias regression uses head `0x4001` over node `0x4000`, whose
successor is `0x4100`. B caches `0x41` and writes a backlink at `0x45`; C's reread
sees zero after writing the node and skips that backlink. Each is checked against
its own oracle. No cross-port equality is assumed. Focused host tests inject
failures at every take/recycle/paired orchestration read, offset, insertion and
write boundary and assert exact partial prefixes. Actual-port tests retain
post-successor wide-tag refusal, C's Model-before-Memory failure logs, cyclic clear
history limits and C's cyclic iteration refusal.

Fixture and manifest regenerate byte-for-byte with the same public inputs.

| New identity | SHA-256 |
|---|---|
| Plain handle corpus | `5eb0f29539a3c95a03bf62bae27073b0bd6f02c2ddda419605cd0ae9d4f85327` |
| Compressed handle corpus | `cd5ff69c81d5fae3b6a4fe12709ac1a62613056cfe3ee428eb3c9a1de785cab6` |
| Generator | `861259a0a8823292eff4ae14966ae6171cd1921966ea974e23d0e42d0e4c605c` |
| Complete arm64/x64 handle trace | `2ea1a8e4fe3da131c0f2f46a67eb9de03b7f1c5a20e593a1d328f0489be8429f` |

## Unchanged historical identities

All eleven files below match both the pre-edit byte/hash baseline and Git objects
at integration base `4a5b8f5`. Historical generators/verifiers/manifests were not
edited. All historical prefix identities remain enforced by their harnesses.

| File | Bytes | SHA-256 |
|---|---:|---|
| `skill_kernel.json` | 1904 | `aa5b58a9c508992a86e3d98586dab0a2fb058a4eebc8480189fc9632d940d1a2` |
| `skill_kernel.tsv.gz` | 280078 | `189a30efb721f6a2b4a384c2a32d35c960890abdab7f2f1a27dc5bb9029a518d` |
| `skills_a.json` | 14732 | `3babd70494a15f0597a4f21afda6e60efb4e7afd4a378fe11b8c61734e754884` |
| `skills_a.tsv.gz` | 98800516 | `4db2684e8c717fa33aa01a40d8ebae4aa1623868e630a2b3e6d271b46f2f4c28` |
| `skills_b.json` | 22221 | `56f4e158147350c26fe0dacb2c0894d98081ce2ff3154231dd958f2fae10e841` |
| `skills_b.tsv.gz` | 72776656 | `d4ba43e90707da6d8d20c6f2c170c36c718a1f899bd76f963561c75f774eea8c` |
| `skills_c.json` | 1202 | `bb6dd018c2f9b2c98212e9742aeefb41b71c39b57ab202b611e154ce997be4a5` |
| `skills_c.tsv.gz` | 259348725 | `1b063a7631b188b6e30203ca5fd8b6e21f9c9c270f4f3ecb392f567a60e51fb7` |
| `skills_c_evidence.json` | 781 | `b624c166601b5af6a8f3bb989f24d2dcbaca6e96c4e1e37584e46d2756416320` |
| `skills_range_kernel.json` | 1995 | `52189f58da076912caa50a74fefec156f60c4fbd5a758cf6da655535f40c54c5` |
| `skills_range_kernel.tsv.gz` | 363090 | `0265a0c5a7d32f2f94c230d79fdb4cc9ced56bfe689bbec04fae69c36ac4699b` |

| Decompressed corpus | SHA-256 |
|---|---|
| `skill_kernel` | `8939885e879fe935f6992fcf618731b35cefeb51c107eb65aff0b2c4ad0f876d` |
| `skills_a` | `0537634d9def4db8daa996cc441a7e0a12be449f3b2c2b24e55418f0cc116288` |
| `skills_b` | `519ffc8d27f9a5f0cc5cd643ab2b02b1423c9b93148c7985173547b3eba8fb0a` |
| `skills_c` | `1881bfb823b26fa03e971f20a4698fe0f329c0fbdbac60a6db21c335de0aad46` |
| `skills_range_kernel` | `cfe892c6692fd332cc5347acc5cc3b31c3db7ff7908ea9619bf895da1858dd67` |

| Complete observation trace (arm64 and x64/Rosetta) | SHA-256 |
|---|---|
| Skills A | `6977791fa8f8edf43f12c2439dc17f2d38fe3db43e9ee6237008a4bba1451ee7` |
| Skills B | `8cc3de69a6ac850569dce235ca4e17e69bb357bf8336047b03c2ce083d961362` |
| Skills C | `3143476e0e3d0e81cfc96e873802ab8d4ac23e8f3344b7e92d43d08f3edfb682` |
| Prior arithmetic kernel | `ca225d495f76c1ee19185da6814b857a01547f2bc8e79e868f888ccfee97dc11` |
| Prior range kernel | `7fcebc8e84068a62bc5cc3fc8c286bc2fe48ddd501a85d6a1abc86f9e12c3c2e` |

## Commands and results

Logs are under ignored `work/skills-handle-kernel/`; binaries under `build/`.
Before production edits, the full Release build and affected baseline passed
**10/10 in 69.97 seconds**. This is an affected baseline, not a claim that the
combined integration base full suite had previously passed.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-' --output-on-failure -j 3
python3 tools/generate_skills_handle_kernel_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-|skills-handle-kernel-' --output-on-failure -V -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skill-kernel-tests champions-skills-a-tests champions-skills-a-trace champions-skills-b-tests champions-skills-c-api-tests champions-skills-c-trace -j 4
ctest --test-dir build/native-x64 -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-|skills-handle-kernel-' --output-on-failure -V -j 3
ctest --test-dir build/native -R 'skills-handle-kernel-' --output-on-failure -V
ctest --test-dir build/native-x64 -R 'skills-handle-kernel-' --output-on-failure -V
python3 tools/generate_skills_handle_kernel_vectors.py --research-dir ../../champions-return-to-arms-re --vectors work/skills-handle-kernel/reproduced.tsv.gz
cmp native/tests/vectors/skills_handle_kernel.tsv.gz work/skills-handle-kernel/reproduced.tsv.gz
cmp native/tests/vectors/skills_handle_kernel.json work/skills-handle-kernel/reproduced.json
cmake --build build/native --target champions-skills-c-api-tests -j 4
ctest --test-dir build/native -R '^skills-c-api$' --output-on-failure
cmake --build build/native-x64 --target champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R '^skills-c-api$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/generate_skills_handle_kernel_vectors.py tools/test_skills_handle_kernel_vectors.py
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
file build/native/champions-skill-kernel-tests build/native-x64/champions-skill-kernel-tests
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

The affected scoped suites passed **12/12 on arm64 in 73.30 s** and
**12/12 on x64/Rosetta in 104.60 s**, with identical complete historical and
prior-kernel traces, including all refusal prefixes. After moving the C API
regression to main, its API test passed again on arm64 (0.19 s) and x64 (0.45 s).

The original 3,130-case corpus was expanded to 6,302 to cover later stores in
repeated sequences; final exact checks passed on both architectures (2/2, arm64
0.46 s, x64 0.72 s). A scratch regeneration matched both final fixture and
manifest. Boundary checks passed unchanged (37 Python tests). Research remained
clean/read only. Mach-O files confirm arm64/x86_64; x64 execution is Rosetta, not
an Intel-host test.

Two new harness defects were corrected: a supposed clear cycle became finite
when recycling rewrote its node (the fixture now aliases the free head), and B's
new assertion block initially preceded its local assertion macro. C's alias block
was subsequently moved from the refusal helper to a single main API check. No
production mismatch or historical expectation change was involved.

The required final arm64 Release sweep passed **148/148 in 759.12 s**. It
included all three unchanged historical corpora, both prior kernel corpora, the
final 6,302-case handle corpus and final API checks. No test failed or timed out;
no isolated timeout retry or out-of-scope dependency repair was required. The
combined integration base full suite had no documented pre-edit result; the
10/10 pre-edit affected baseline is distinguished above from this executed full
post-extraction sweep. Final Release builds had no compiler warnings.

## Changed files and remaining boundaries

Owned changes only: the new detail header; selected handle call sites in
`native/src/skills_b.cpp` and `skills_c.cpp`; C's private header declarations;
focused B/C API checks and `native/tests/skill_kernel_tests.cpp`; dedicated test
registrations in `native/skill_kernel_targets.cmake`; new
`tools/generate_skills_handle_kernel_vectors.py` and
`tools/test_skills_handle_kernel_vectors.py`; `skills_handle_kernel.json` and
`skills_handle_kernel.tsv.gz`; this page and the small `SKILLS_KERNEL.md` link.
No central target/census/registry, research, integration notes or unrelated
modules changed.

This is RTA native infrastructure only. A's callback and constructor refusal,
spell constructors/RNG/formulas/lifecycles, stack services, Session/control/save,
Godot, original rendering/dispatch, online behavior and gameplay remain outside
this extraction. Private acceptance, original game content, Norrath, Windows,
Intel-host execution and console fidelity are unverified. No private inputs were
searched or accessed. Local commit only; no push, PR or issue actions.
