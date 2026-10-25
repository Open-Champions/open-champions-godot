# RTA Skill arithmetic kernel

The [spell publication kernel](SKILLS_SPELL_KERNEL.md) shares RNG publication
and constructor tails while retaining the three ordered initialization adapters.

The [handle kernel](SKILLS_HANDLE_KERNEL.md) shares proven B/C take, recycle and
paired-link ordering while retaining their alias-sensitive adapters.

The subsequent [range and signed virtual-this kernel](SKILLS_RANGE_KERNEL.md)
reconciles their shared numeric primitives while retaining each port's adapters.

Reconciliation prepared before extraction, against base `873fcb4` and immutable
research commit `356add27d50e50972b2a671b31e8f4ec93e6778c`. Every evidence digest
in the three manifests was checked against `git show <pin>:<path>` and the
read-only working file. Current research HEAD was not used as evidence.

| Candidate / existing C++ entry | Exact pinned Python contract | Shared surface / retained adapter |
|---|---|---|
| A `skill_stat` | `ref_skills_a.Model.skill_stat` | Signed16 first/base, raw32 step, low32 signed delta conversion; zero-level exit and signed indexed addresses remain local |
| A `skill_stat_tier2` | `ref_skills_a.Model.skill_stat_tier2` | Positive signed cap and negative-first fallback; one tier-2 first read, local fallback call and rounding |
| B `stat_impl`, `get_stat` | `ref_skills_b.Model._stat`, `skill_get_stat` | Same raw arithmetic; adapter validates index 0..4 (even at zero level), checks signed delta before base/step reads, and rounds with `lt(value, ZERO)` |
| B `get_stat_tier2` | `ref_skills_b.Model.skill_get_stat_tier2` / `_stat` | Same signed cap/fallback; retains repeated selected first read, index checks and oversized-delta refusal |
| C `tier`, execute `Stat` / `Tier2` | `ref_skills_c.Base._tier_value`, `Base.skill_get_stat`, `Base.skill_get_stat_tier2` | Same arithmetic/cap/fallback; signed indices, low32 delta and repeated first read remain local; rounding uses `le(ZERO, value)` |
| A `draw`, `spell_rand`, `lcg` | `ref_creature.Model.draw`, `ref_arena_monsters.Model.spell_rand`; `ref_skills_a.lcg` | Low32 LCG step and upper16 result only; A write-history checks/stores and draw count remain local |
| B `next`, `draw` | inherited `ref_creature.Model.draw`; `ref_skills_b.Model.spell_rand`, `cosmetic_draw`, `seed3_draw`; inherited `obj_rand` | Same step/result; direct Memory stores (no Model write sink), shared-only count remain local |
| C `draw`, `own_draw` | `ref_creature.Model.draw` / `Model.obj_rand`, `ref_arena_monsters.Model.spell_rand` / `Model.cosmetic_draw` | Same step/result; caller-selected seed address, Model history checks/stores and shared-only count remain local |

Selected common input domain: raw32 patterns, sign-extended signed16 first/base,
raw32 step and low32 delta. B's accepted subset additionally requires index 0..4
and delta <= INT32_MAX. No address, dispatch, range, lifetime or memory ownership
is transferred to the kernel. It uses existing integer-backed core arithmetic and
the caller's current FPU configuration, with no state or configuration changes.

Sequence retained: adapter validates its domain; level zero returns before reads
(except B index validation); signed cap is applied; tier-2 first is read to select
fallback; selected first is read as before; below-first returns before base/step;
B checks delta before further reads; base and step reads stay in each adapter;
base conversion and step conversion/multiply retain their nested adapter
expressions; the shared sum adds their raw results; the original rounding predicate
and integer conversion remain local. No eager memory reads are introduced.
C's pinned Python `_tier_value` evaluates step/conversion before reading base;
its pre-existing C++ expression instead nests base and step as function arguments.
This extraction does not broaden the memory domain or unify read-failure policies:
those expressions remain in C's adapter. A/C `le` versus B `lt` rounding is
intentionally retained, including unordered values.

Range, caster, mana, callbacks, signed pointer adjustments, supplied-code
recognition, temporary vtables, paired handles and all skill-specific formulas
remain unchanged. B writes each unclamped bound immediately, followed by a clamp
store only when below 1; aliases can affect later seed/input reads. A/C return pairs. A retains its extra player-tier
argument. All private/global streams retain their original separate addresses,
store widths/order, history sinks and exception prefixes. No new RNG authority,
gameplay wiring or reference-function credit is introduced.

A: PROGRESS: 195 of 196 reference functions ported

B: PROGRESS: 239 of 239 reference functions ported

C: PROGRESS: 175 of 175 reference functions ported

A's unrelated `gameobject_ctor` refusal remains intact. Private acceptance is
skipped: `../../private-validation` is unavailable; no alternate search is made.

## Extracted surface and tests

`detail::skill_kernel` is header-only: `capped_level`, `tier2_fallback`, `delta`,
`statistic_base`, `statistic_step`, `statistic_sum`, `lcg_step`, `lcg_result`.
All three statistic/tier paths use these primitives. All shared/private LCG
wrappers use the common step/result (A's public `lcg` delegates too). Separating
base conversion, scaled step and sum preserves the adapters' original expression
structure rather than moving memory reads into a batch. Rounding remains local:
A/C use existing `round_half_away`; B uses its existing `round(value, lt(...))`.
The boundary checker now covers the new header: 67 enumerated sources/headers.
No new library, dependencies, public API, hidden seed or Godot code is needed.

`skill_kernel_tests.cpp` provides host adapter checks and an input-only trace
mode. `test_skill_kernel_vectors.py --generate` verifies all immutable pins before
importing the references, disables research bytecode writes and authors the
separate `skill_kernel.tsv.gz` corpus. Normal checks need only that fixture.
The **11,136** independent observations cover all 16 FPU configurations, signed
first/base/level extrema, low32 delta overflow versus B refusal, signed indices
versus B's 0..4 domain, negative tier-two firsts, signed caps, skipped zero-level
reads, NaN/infinity/subnormal patterns and conversion limits, shared/private RNG
transitions, and six range output layouts (normal, either/both seed aliases,
statistic memory aliases and identical outputs). A/C pair returns and B's stores
remain different. Complete sparse bytes and Memory store histories are compared;
A/C Model write counts and shared draw counts are compared separately. C's shared
draw is reached through its existing SpellBase API, also retaining its distinct
constructor stores. All expectations come from the pin plus the existing explicit
native refusal domains; the generator never reads C++ output.

Host checks add throwing memory delegates for zero-level/below-first skipped
reads and B's early delta refusal, per-port player callback arity and replacement,
callback exceptions after shared/private draws, and range history exhaustion after
partial writes/draws. They assert the original exception messages and the differing
Model/Memory histories. The unchanged historical suites additionally check every
complete return/refusal, callback argument/order, store width/order, Model history,
seed/draw count and transport-tampering case from all completed families.

## Unchanged historical identities

| Module | Complete observations | Plain corpus SHA-256 |
|---|---:|---|
| A | 149,569 | `0537634d9def4db8daa996cc441a7e0a12be449f3b2c2b24e55418f0cc116288` |
| B | 174,352 | `519ffc8d27f9a5f0cc5cd643ab2b02b1423c9b93148c7985173547b3eba8fb0a` |
| C | 149,648 | `1881bfb823b26fa03e971f20a4698fe0f329c0fbdbac60a6db21c335de0aad46` |

All historical prefix hashes enforced by the existing harnesses still pass.
The seven original compressed corpora/manifests/evidence files are byte-for-byte
unchanged from the pre-edit snapshot (SHA-256):

- `skills_a.json`: `3babd70494a15f0597a4f21afda6e60efb4e7afd4a378fe11b8c61734e754884`
- `skills_a.tsv.gz`: `4db2684e8c717fa33aa01a40d8ebae4aa1623868e630a2b3e6d271b46f2f4c28`
- `skills_b.json`: `56f4e158147350c26fe0dacb2c0894d98081ce2ff3154231dd958f2fae10e841`
- `skills_b.tsv.gz`: `d4ba43e90707da6d8d20c6f2c170c36c718a1f899bd76f963561c75f774eea8c`
- `skills_c.json`: `bb6dd018c2f9b2c98212e9742aeefb41b71c39b57ab202b611e154ce997be4a5`
- `skills_c.tsv.gz`: `1b063a7631b188b6e30203ca5fd8b6e21f9c9c270f4f3ecb392f567a60e51fb7`
- `skills_c_evidence.json`: `b624c166601b5af6a8f3bb989f24d2dcbaca6e96c4e1e37584e46d2756416320`

The separate kernel corpus plain SHA-256 is
`8939885e879fe935f6992fcf618731b35cefeb51c107eb65aff0b2c4ad0f876d`.
Its arm64/x64 observation SHA-256 is
`ca225d495f76c1ee19185da6814b857a01547f2bc8e79e868f888ccfee97dc11`.

## Executed verification

Release baselines before extraction passed **7/7** unchanged skills checks on
arm64 (100.23 s) and x64 through Rosetta (146.10 s). After extraction the arm64
scoped suite passed **9/9 in 96.48 s**; x64 through Rosetta passed
**9/9 in 148.35 s**. Exact commands follow; output logs are
bounded under ignored `work/skill-kernel/`. Build outputs/traces stay in `build/`.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-' --output-on-failure -j 1
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skills-a-tests champions-skills-a-trace champions-skills-b-tests champions-skills-c-api-tests champions-skills-c-trace -j 4
ctest --test-dir build/native-x64 -R 'skills-[abc]-' --output-on-failure -j 1
python3 tools/test_skill_kernel_vectors.py --generate --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R 'skills-[abc]-|skill-kernel-' --output-on-failure -V -j 1
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-skill-kernel-tests champions-skills-a-tests champions-skills-a-trace champions-skills-b-tests champions-skills-c-api-tests champions-skills-c-trace -j 4
ctest --test-dir build/native-x64 -R 'skills-[abc]-|skill-kernel-' --output-on-failure -V -j 1
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
python3 -m py_compile tools/test_skill_kernel_vectors.py tools/check_simulation_boundary.py tools/test_core_simulation_boundary.py
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

The initial new host harness compilation failed on A's required constructor
service argument and C's private `draw`; the harness was corrected to use the
existing A constructor and C SpellBase API. These were test-harness errors, not
production mismatches. No historical generator or expectation was edited.

Boundary checks passed (35 Python tests), as did Python compilation, whitespace,
immutable-pin checks and byte-for-byte historical identity checks. Research tracked
status stayed clean. Executables were verified as arm64 and x86_64 Mach-O; x64
execution here is through Rosetta, not an Intel-host test.

After the final input/output-file guard was added to the new runner, its x64
binary was rebuilt and the two kernel checks passed again (**2/2 in 0.59 s**):

```sh
cmake --build build/native-x64 --target champions-skill-kernel-tests -j 4
ctest --test-dir build/native-x64 -R 'skill-kernel-' --output-on-failure -V
```

Both architectures produced identical complete historical observation hashes:

| Module | Native complete trace SHA-256 (arm64 and x64) |
|---|---|
| A | `6977791fa8f8edf43f12c2439dc17f2d38fe3db43e9ee6237008a4bba1451ee7` |
| B | `8cc3de69a6ac850569dce235ca4e17e69bb357bf8336047b03c2ce083d961362` |
| C | `3143476e0e3d0e81cfc96e873802ab8d4ac23e8f3344b7e92d43d08f3edfb682` |

Only the new kernel header, dedicated test registration, new kernel/adaptor
runner/helper/fixtures, common call sites in the three sources, one additive
CMake include, the kernel boundary path/census checks, this page and four-line
notes in each skills page changed. Public headers, original API tests, original
trace transports, original generators/manifests/corpora and integration notes
remain unchanged.

## Remaining integration boundaries

This is a verified native arithmetic foundation only. Dispatch, memory services,
range outputs, constructors/vtables, handle lifetimes, presentation callbacks and
individual formulas remain owned by their ports. It introduces no new skill,
no reference-function credit and no Session or scene integration. A's missing
`gameobject_ctor` evidence still blocks its last function. Private acceptance,
real game content, console fidelity, Norrath, Windows, Intel-host execution and
Godot/exported gameplay remain unverified. No private inputs were accessed or
searched for; research stayed read only. Local commit only; no push or PR/issue.

The required final arm64 Release full suite passed **136/136 in 682.51 s**.
All three historical skills corpora and both new kernel cases passed again inside
that sweep. No final scoped/full-suite test failed or timed out; no isolated timeout
retry was needed. Both final Release builds passed without compiler warnings.
