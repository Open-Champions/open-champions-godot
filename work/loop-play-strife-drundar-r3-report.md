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
