# RTA spell publication kernel

Pre-extraction reconciliation on `codex/skills-spell-kernel`, base `60def02`.
The oracle is immutable research commit
`356add27d50e50972b2a671b31e8f4ec93e6778c`, not research HEAD
`a2b145b6b22f3c0451f861ed70faa1827f94bf09`. The existing range manifest's
16 reference hashes were checked against Git objects and working files before
editing; the four requested Python files have no diff from the pin at HEAD.

| Adapter | Pinned body | Ordered prefix outside the kernel |
|---|---|---|
| A `spell_base` | `ref_skills_a.Model.spell_base` (1807) | level byte, back=0, skill, owner; no temporary table |
| B `spell_base_ctor` | `ref_skills_b.Model.spell_base_ctor` (651) | level byte, temporary table, skill, back=0, owner |
| C `execute(SpellBase)` | `ref_skills_c.Base.spell_base` (506) | level byte, skill, owner, temporary table, back=0 |

All tails acquire exactly one shared draw, publish its **upper16 result** as a
four-byte private seed, then publish the final table. An aliased prefix can
change the shared seed before acquisition. Final-table publication must follow
private-seed publication, including overlapping fields. Private RNG operations
read the stored full seed, compute the existing low32 LCG step, publish that
full stepped state, and only then return its upper16 result. They do not increase
the shared draw count. Inherited sources are `ref_arena_monsters.Model.spell_rand`
(318), `cosmetic_draw` (328), and `ref_creature.Model.draw` (440), `obj_rand` (447).
B's own spell/cosmetic/third-stream bodies use the same publication sequence.

## Evaluation and refusal boundaries

A converts raw32 spell to wide Address before offset arithmetic. Its RNG seed
address precedes the read; constructor seed address is formed after the shared
draw. A's write checks Model history, records a masked/narrow-address observation,
then calls Memory.store. Thus an invalid span or exhausted Memory store budget
can leave a Model observation without a Memory store. Its history error is
`Skill write history limit`.

B checks spell/object offsets before RNG reads. Python evaluates the constructor
seed address before draw acquisition. The old C++17 Memory.store argument
expression does not prescribe argument order; Clang's observed baselines agree
with the selected ordering. Also, once B's temporary-table span (+0x14, width 4)
succeeds, the smaller seed offset (+0x10) cannot independently refuse: the base
is a fixed value, unaffected by callbacks. Explicitly checking the seed address
before the common tail therefore preserves every admitted/refused prefix while
matching Python evaluation order. The final check stays lazy after seed
publication. No Memory span check is moved ahead of the draw. B writes directly
to Memory, has no Model write sink, and increments shared draws only after a
successful shared store. Its offset error is
`address offset exceeds raw32 memory domain`. No checks migrate across mutations.

C widens raw32 execute arguments to Address; its private write checks Model
history, preflights Memory.load of the target span, records the masked Model
observation, then stores. Its shared draw deliberately has no write preflight:
read shared seed, history check, observation, store, increment. The shared draw
stays local. C's history error is `write history limit`. Pure wide address
arithmetic can be evaluated before draw without refusing; span validation stays
inside the subsequent write, after draw. No eager preflight is introduced.

Memory enforces its existing raw32 span and history limits. These are authored
safety domains, distinct from Python's wider integer/sparse-memory domain.
Fixtures must label this policy overlay and must not narrow the pinned bodies
or weaken their valid-input expectations. Complete bytes, ordered Memory stores,
Model histories, reads, returns/refusals and shared draw counts are observed per
adapter; histories across A/B/C are deliberately not equal.

The shared header owns only read/step/publication/result and
one-draw/seed/final-table ordering, using lazy callbacks. It reuses
`skill_kernel::lcg_step/lcg_result`, without owning addresses or a Model.
Other constructors with different interleaved stores, whole field initialization,
virtual/service dispatch, handle policy, spell formulas and lifecycles remain
local because their ordering/contracts are not identical. A's existing
`gameobject_ctor` refusal is untouched. There is no new RNG authority, gameplay
wiring or reference-function credit.

Module A:
PROGRESS: 195 of 196 reference functions ported

Module B:
PROGRESS: 239 of 239 reference functions ported

Module C:
PROGRESS: 175 of 175 reference functions ported

Private acceptance is skipped: `../../private-validation` is unavailable. No
alternate paths or private content are searched.

## Implementation and independent proof

All three requested adapters now use both header-only operations:
`rng_publication` (A `spell_rand`, B `next`, C `own_draw`) and
`constructor_tail` (A `spell_base`, B `spell_base_ctor`, C `SpellBase`).
B's existing shared/object/cosmetic/third-stream wrappers consequently use the
publication operation too; their counters and checked offsets remain local.
No public/private declaration changes were needed. Arithmetic/range/handle
headers and all historical generators/verifiers/fixtures/manifests are untouched.

The separate `skills_spell_kernel` corpus contains **2,006 observations**.
Generation imports verified Git-object Python bytes in a temporary directory,
never C++ output. The final fixture and manifest regenerate byte-for-byte.
The manifest preserves all 16 established evidence hashes. Requested identities:

| Pinned file | SHA-256 |
|---|---|
| `tools/ref_skills_a.py` | `6ab0b5f330fdadbdef74c5cbbccb7e9738742d4c3b8beae9fdedd10a66a2d5ae` |
| `tools/ref_skills_b.py` | `b71cbdd9e82cd72abb855b9f6ab99d11fb78a58613cb69d314a5041457bbdd95` |
| `tools/ref_skills_c.py` | `9e673475a0b89292e483075d76b2f277d1251ce820a0956053e68c83faa81e91` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |

Synthetic cases cover shared seed zero/max/high-bit/overflow, modular-inverse
private seeds stepping exactly to zero/max, repeated private draws, full state
versus upper16 constructor seed, and private/shared/field aliases. Thirty layouts
include every byte offset from shared RNG minus 24 through plus 4, plus an ordinary
object. Level zero/0x100/max cases retain neighboring bytes; ordered temporary
and final table stores remain distinct per port. Raw32 edge spans and B's wide
Address inputs preserve refusals, including A's draw-before-invalid-seed-store
prefix. Store budgets and remaining Model history room 0..9 cover every
constructor/private-sequence store boundary; delegated read failures 1..15 cover
initial reads and C's write preflights. Expected states retain the refusal text,
returned values, complete sparse bytes, ordered Memory stores, A/C Model writes,
shared draw counts and delegated read attempts (including the failing attempt).

Only the established native safety policy is overlaid on Python: raw32 span
limits, B's checked offsets, Model/Memory history budgets, and injected delegated
read failures. These refusal cases are safety-prefix checks, not claims about
original Python invalid-input behavior. Within the original/native common valid
domain, constructor and RNG bodies execute unchanged from the pin. C's shared
write bypasses its private-write preflight exactly as the existing adapter does.
No oracle body, historical expectation or refusal domain was weakened.

The first 1,934-case version passed against the unmodified production adapters
on arm64 and x64/Rosetta before extraction (trace SHA-256
`2ec353531320e7824b4c2beae0e8a03cdb3b8eb90ff7ccf727c4594f875c3295`).
It passed unchanged after extraction; 72 further standalone private-seed cases
then covered exact zero/max LCG wrap and aliased repeated private draws.

`--spell-host` injects exceptions at every ordered RNG read/address/write and
constructor draw/read/address/write boundary, checks exact completed callback
prefixes, full versus upper16 state, table publication and draw accounting.
Each original API test adds a direct constructor call whose shared-read delegate
observes the complete port-specific prefix and mutates the future private seed.
The final tail overwrites that mutation in order, followed by a private draw;
Model/Memory histories and absence of extra services are checked.

## Unchanged historical byte/hash baseline

Before production edits, all 13 files below were measured and hashed in
`work/skills-spell-kernel/baseline.json`. They still match both that snapshot and
the Git objects at base `60def02`. Historical prefix checks still pass.

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
| `skills_handle_kernel.json` | 2085 | `bb1649a8019bc5a1ca304d5f0fc90add775ba43ae750c9917ed79b121b4131b4` |
| `skills_handle_kernel.tsv.gz` | 61411 | `cd5ff69c81d5fae3b6a4fe12709ac1a62613056cfe3ee428eb3c9a1de785cab6` |
| `skills_range_kernel.json` | 1995 | `52189f58da076912caa50a74fefec156f60c4fbd5a758cf6da655535f40c54c5` |
| `skills_range_kernel.tsv.gz` | 363090 | `0265a0c5a7d32f2f94c230d79fdb4cc9ced56bfe689bbec04fae69c36ac4699b` |

| Decompressed historical corpus | SHA-256 |
|---|---|
| `skill_kernel` | `8939885e879fe935f6992fcf618731b35cefeb51c107eb65aff0b2c4ad0f876d` |
| `skills_a` | `0537634d9def4db8daa996cc441a7e0a12be449f3b2c2b24e55418f0cc116288` |
| `skills_b` | `519ffc8d27f9a5f0cc5cd643ab2b02b1423c9b93148c7985173547b3eba8fb0a` |
| `skills_c` | `1881bfb823b26fa03e971f20a4698fe0f329c0fbdbac60a6db21c335de0aad46` |
| `skills_handle_kernel` | `5eb0f29539a3c95a03bf62bae27073b0bd6f02c2ddda419605cd0ae9d4f85327` |
| `skills_range_kernel` | `cfe892c6692fd332cc5347acc5cc3b31c3db7ff7908ea9619bf895da1858dd67` |

## Exact observations and executed checks

| New artifact | SHA-256 |
|---|---|
| Plain spell corpus | `e3b874524ccd1a4502325ab38d4a0397e620112ccfa27526c68cbb5681d7d86d` |
| Compressed spell corpus | `677f7c7a7599565b4d9b30d14c57a9aef0dd3fa3bb3e13b710d2448fff006a59` |
| Spell generator | `c9d1ccd8c8777447bbd06cc70b0cf5d75598a751b6b8588ada8a70d76fd933a6` |
| Spell manifest | `4b80f13006798b57e90d54996622b818acd7b5cb76ae9bb37f9d73b5374c4926` |

Complete observation hashes match on arm64 and x64/Rosetta for all seven corpora,
including failure prefixes. Each architecture was independently compared to its
own pinned expected observations before comparing the full trace digests.

| Complete trace | Observations | SHA-256 (both architectures) |
|---|---:|---|
| Skills A | 149,569 | `6977791fa8f8edf43f12c2439dc17f2d38fe3db43e9ee6237008a4bba1451ee7` |
| Skills B | 174,352 | `8cc3de69a6ac850569dce235ca4e17e69bb357bf8336047b03c2ce083d961362` |
| Skills C | 149,648 | `3143476e0e3d0e81cfc96e873802ab8d4ac23e8f3344b7e92d43d08f3edfb682` |
| Arithmetic kernel | 11,136 | `ca225d495f76c1ee19185da6814b857a01547f2bc8e79e868f888ccfee97dc11` |
| Range kernel | 39,744 | `7fcebc8e84068a62bc5cc3fc8c286bc2fe48ddd501a85d6a1abc86f9e12c3c2e` |
| Handle kernel | 6,302 | `2ea1a8e4fe3da131c0f2f46a67eb9de03b7f1c5a20e593a1d328f0489be8429f` |
| Spell kernel | 2,006 | `7ca27ce01744e8a20f4dc926282f3d90863558c96d0bdb4e697725459b406dde` |

Pre-extraction Release affected baselines passed **12/12 on arm64 in 56.01 s**
and **12/12 on x64/Rosetta in 70.23 s**. Final affected checks passed
**14/14 on arm64 in 50.62 s** and **14/14 on x64/Rosetta in 68.03 s**.
Final review moved C's new API block from the refusal helper into `main`, so it
runs once. The corrected API test passed again on arm64 (0.16 s) and x64
(0.38 s); production code and corpus observations did not change. Both Release
builds passed without compiler warnings. File inspection confirmed
arm64 and x86_64 Mach-O executables; x64 execution here is through Rosetta,
not an Intel host. Boundary checks passed unchanged (37 Python tests), Python
compilation and whitespace checks passed, and research tracked status stayed
clean. No central CMake/census/trace-registry or timeout changes were needed.

After the interrupted dispatch, a fresh Release configure/build and affected
arm64 run passed 14/14. The pinned generator reproduced the new fixture and
manifest byte-for-byte, and its 2,006 observations passed. A fresh full Release
CTest run passed **150/150 in 945.12 s** under concurrent machine load; no
timeout reruns were needed. The 13 historical corpus files still match their
pre-edit sizes, hashes, decompressed hashes and base Git objects.

A further x64/Rosetta `-j 3` affected rerun passed 12/14 directly; A reached
CTest's 120 s limit and B's unchanged verifier hit its internal 120 s limit
under heavy concurrent load. B passed when rerun alone (86.42 s). A again
reached CTest's 120 s limit alone, then its unchanged verifier passed directly
without that outer cap: all 149,569 complete states matched, SHA-256
`6977791fa8f8edf43f12c2439dc17f2d38fe3db43e9ee6237008a4bba1451ee7`.
The earlier complete x64/Rosetta affected CTest run passed 14/14. These time
limits caused no observation mismatch and no test timeout or verifier was edited.

Exact verification commands (logs under ignored `work/skills-spell-kernel/`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-|skills-handle-kernel-' --output-on-failure -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skill-kernel-tests champions-skills-a-tests champions-skills-a-trace champions-skills-b-tests champions-skills-c-api-tests champions-skills-c-trace -j 4
ctest --test-dir build/native-x64 -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-|skills-handle-kernel-' --output-on-failure -j 3
python3 tools/generate_skills_spell_kernel_vectors.py --research-dir ../../champions-return-to-arms-re
cmake --build build/native --target champions-skill-kernel-tests -j 4
python3 tools/test_skills_spell_kernel_vectors.py --exe build/native/champions-skill-kernel-tests
cmake --build build/native-x64 --target champions-skill-kernel-tests -j 4
python3 tools/test_skills_spell_kernel_vectors.py --exe build/native-x64/champions-skill-kernel-tests
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-|skills-handle-kernel-|skills-spell-kernel-' --output-on-failure -V -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skill-kernel-tests champions-skills-a-tests champions-skills-a-trace champions-skills-b-tests champions-skills-c-api-tests champions-skills-c-trace -j 4
ctest --test-dir build/native-x64 -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-|skills-handle-kernel-|skills-spell-kernel-' --output-on-failure -V -j 3
python3 tools/generate_skills_spell_kernel_vectors.py --research-dir ../../champions-return-to-arms-re --vectors work/skills-spell-kernel/reproduced.tsv.gz
cmp native/tests/vectors/skills_spell_kernel.tsv.gz work/skills-spell-kernel/reproduced.tsv.gz
cmp native/tests/vectors/skills_spell_kernel.json work/skills-spell-kernel/reproduced.json
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native-x64 -R 'skills-[abc]-|skill-kernel-|skills-range-kernel-|skills-handle-kernel-|skills-spell-kernel-' --output-on-failure -j 3
ctest --test-dir build/native-x64 -R '^skills-a-exact$' --output-on-failure -j 1
ctest --test-dir build/native-x64 -R '^skills-b-exact$' --output-on-failure -j 1
python3 tools/test_skills_a_vectors.py --native build/native-x64/champions-skills-a-trace --vectors native/tests/vectors/skills_a.tsv.gz --manifest native/tests/vectors/skills_a.json --trace-dir work/skills-spell-kernel/r2-x64-a-traces
cmake --build build/native --target champions-skills-c-api-tests -j 4
ctest --test-dir build/native -R '^skills-c-api$' --output-on-failure
cmake --build build/native-x64 --target champions-skills-c-api-tests -j 4
ctest --test-dir build/native-x64 -R '^skills-c-api$' --output-on-failure
python3 -m py_compile tools/generate_skills_spell_kernel_vectors.py tools/test_skills_spell_kernel_vectors.py
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
file build/native/champions-skill-kernel-tests build/native-x64/champions-skill-kernel-tests
git diff --check
git -C ../../champions-return-to-arms-re diff 356add27d50e50972b2a671b31e8f4ec93e6778c -- tools/ref_skills_a.py tools/ref_skills_b.py tools/ref_skills_c.py tools/ref_core.py
git -C ../../champions-return-to-arms-re status --short
```

## Changed files and retained boundaries

Owned changes only: `native/include/champions/detail/skill_spell_kernel.hpp`;
the specified RNG/base-constructor sites in `native/src/skills_a.cpp`,
`skills_b.cpp`, `skills_c.cpp`; focused additions in
`native/tests/skills_a_tests.cpp`, `skills_b_tests.cpp`, `skills_c_tests.cpp`,
`skill_kernel_tests.cpp`; `native/skill_kernel_targets.cmake`;
`tools/generate_skills_spell_kernel_vectors.py`,
`tools/test_skills_spell_kernel_vectors.py`;
`native/tests/vectors/skills_spell_kernel.tsv.gz` and `skills_spell_kernel.json`;
this page and the small `docs/SKILLS_KERNEL.md` link.

No new reference function is ported. No spells are wired into gameplay. A's
constructor refusal, spell-specific formulas/lifecycles, service dispatch and
all unrelated modules are retained. Private acceptance, Norrath, Windows,
Intel-host execution, console fidelity and exported gameplay remain unverified.
No private files were accessed or searched elsewhere. Local commit only;
no push, PR or issue actions.
