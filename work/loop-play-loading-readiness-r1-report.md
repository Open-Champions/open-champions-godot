STATUS: MORE — implementation in progress; pending full CTest.

Assigned base: 66e529727407dc7c0016cd286c36ac4b36649ba9.
Research: research-loading-readiness@9dd4904603ae0d8a46e57454cf5c84b1f25c8fd9,
`docs/loading-readiness.md`; audited PASS in `audits/research-loading-readiness.md`.

Implemented native one-pass observation, explicit offline peer/network-save bypass,
exact addressed store applicability, and conservative published equipment subset.
Unknown model/I/O publication, stock generation/RNG, online services and card
persistence retain explicit refusals. Full verification pending.

Gates: pending full CTest (this note committed before execution), Release build,
Godot dev matrix, both journeys, FULL_TRACE verification.

Review checkpoint (2026-10-27): pinned research and PASS audit reviewed. Fixed
scene attachment failure overriding controller readiness; added attachment and
launch-revocation regressions. Focused native script-command cases PASS 3/3
(0.42 s). Release native and main extension builds PASS with -j 4. Initial
Godot setup failed on absent auxiliary texture extension; building both auxiliary
extensions before retry. Pending full CTest; this checkpoint precedes execution.
