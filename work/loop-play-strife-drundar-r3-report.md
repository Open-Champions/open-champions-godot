STATUS: IN PROGRESS — PSD-R3-1 corrected; validation pending.

Supersedes the prior round-3 partial-state expectations. The validated NOT
and researched zero/nonzero predicate remain intact. Zero now records an
explicit unsupported-door-sequence refusal immediately after the persistent
read, before clear/cutscene/write dispatch. Incoming save inhibit and persistent
flag survive without rollback or fabricated completion. Nonzero completes
without effects. PSD-R1-1 and all other round-3 behavior remain unchanged.

Native and Godot regressions require unchanged inhibit/flag and absent effect
dispatch. Opcode replacement controls remain intact.

Pending full CTest: this note is committed before invoking the full suite.
Release build (-j 4), focused native, full CTest (-j 3), Mission 1 headless/
rendered, both journey variants and unchanged FULL_TRACE gates are pending.

PSD-R3-1 fix full CTest (recorded by the orchestrator, 2026-10-27 ~13:00, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` on the fix (3a695b08 + 6c6b9747) — **252/252 passed, 657.59 s**, no timeouts (log `work/mission1/r3-fix-ctest-full.log`). Mission-1 Godot checks, both journeys and FULL_TRACE for this revision are still PENDING and must be run by the next job before re-audit.
