STATUS: PENDING — M2H-1 implementation and regression validation in progress; pending full CTest.

Work stays on the assigned mission-2 branch. The current integration tip
`e6ed82e6` was merged locally to reuse its audited campaign lifetime helpers
and reconcile the seven existing integration conflicts. No integration worktree
was changed. Shared actor-lifetime reconciliation is extracted into one header
used by both registries; mission-2 additionally requires WorldAuthority identity.
Object serials are saved; adoption, registration, lookup and const enumeration
retire stale entries. No gameplay constants or class branch conditions change.

Evidence: research-mission2 ffff9a7, mission2-enemies.md constructor/base-object
chain and ref_mission2_enemies.py; the audit-requested campaign helpers and
research-campaign-combat 328b072 ref_objects/ref_core lifetime and pool reuse.
Synthetic regressions cover all four classes, real destroy/reallocate/adopt,
foreign ownership, serial mismatch, stale enumeration and fresh refusal/death state.

Pending: Release build, focused regressions, full CTest (-j 3, timeout retries
alone), affected Godot checks, headless/rendered journey and final trial merge.
Detailed logs remain ignored under work/mission2-r2.
