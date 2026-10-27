STATUS: PENDING — M2H-1 implemented; Release and focused checks pass; pending full CTest.

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

Verified so far:
- Release configure/build: PASS (`cmake -S native -B build/native
  -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j 4`).
- Mission-2 actor suite: 512 checks PASS (formerly 457); existing 16 ticks /
  162 call expectations remain unchanged.
- Focused CTest: 7/7 PASS in 36.50 seconds (mission-2, campaign actors,
  adoption, Minotaur, fight boundaries, death flow and rewards).
- Trial merge against integration `7aa582aa`: exit 0, no conflicts.

Pending: full CTest (-j 3, timeout retries alone), affected Godot checks,
headless/rendered journey and final trial merge against the then-current tip.
Detailed logs remain ignored under work/mission2-r2.

Continuation checkpoint: implementation review confirms one shared lifetime helper
implementation and all requested regression cases. Pending full CTest rerun
with `ctest --test-dir build/native --output-on-failure -j 3` before final gates.
