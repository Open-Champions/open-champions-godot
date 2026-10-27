STATUS: MORE — implementation checkpoint; full CTest and Godot/journey gates PENDING.

2026-10-27. Assigned branch started clean at 4dbee736. Research verified at
2550d3473dafc3609bbecbed2ec365bf7a658c60; round-3 audit PASS.

Implemented built-in first/second equipment-resource transformations, retaining
explicit paired callback overrides for adapters. All new original fields and
branches cite research-loading-readiness@2550d34:docs/loading-arrival-r3.md:
11-29 (guard/header/flags/rectangle table/accumulator), 31-40 (signed count/width,
model relocation), 49-55 (ordered fixups before publication; next-call outputs).
Publication and address-overflow checks are client restrictions. Both resource
write plans are preflighted before either is applied, preventing partial-resource
refusals from poisoning the texture guard. Authored tests drive Host's actual
loading Runtime; no game bytes or payload names are included.

UNKNOWN/refused: actual starting-item generation, slot/header/resource mapping,
formatter/metadata/media transfer and renderer validation (research :65-69), and
normal-route deferred command order (:93-106). These are not replaced by an
arbitrary resource, default validation, or successful arrival fallback.

Focused Release build PASS (-j 4); script-commands PASS, 1/1, 0.61 s.
Initial build exposed an ADL name collision with std::apply; renamed the helper.
Full Release CTest (-j 3): PENDING; record exact count/time and isolated timeouts.
Release extension build, strict Godot dev matrix, both journeys and FULL_TRACE:
PENDING. Known inherited ui_layout_smoke four-argument diagnostic remains a gate
failure, not an allowed diagnostic. No fixtures/baselines/tests weakened.
