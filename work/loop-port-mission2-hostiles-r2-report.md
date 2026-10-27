STATUS: MORE — M2H-1 addressed; native and affected Godot gates pass; journey remains non-clean and integration trial merge conflicts.

Assigned branch only; started clean at 55f9de2c. Reviewed 089b3282,
2c332b0b, 55f9de2c and the r1 audit before validation. No push or PR;
no integration worktree changes.

Fix and evidence

The prior implementation needs no additional code change. actor_lifetime.hpp
contains the single implementation of owns_live_object, retire_stale and
register_actor, extracted from the audited integration campaign helpers.
Both campaign and mission-2 registries call it; there is no parallel copy.
Reconciliation checks live World membership, pointer, saved serial and alive
state before accepting an occupant. Mission-2 additionally validates authority
identity. Adoption and lookup retire stale entries; const enumeration filters
stale actors before campaign combat can enumerate them.

Pinned evidence: research-mission2 ffff9a7, docs/mission2-enemies.md
constructor/base-object chain and tools/ref_mission2_enemies.py;
research-campaign-combat 328b072 ref_objects construction/destruction and
ref_core pool reuse. Identity reconciliation is a host safety contract from
the audited integration implementation, not a new retail gameplay claim.
No gameplay values or class conditions changed in this continuation.

Regression review: all four classes destroy/unregister and really reallocate
at the same pool address with a different serial. Fresh adoption has no old
constructor/AI/death refusal or death flags, and executes fresh AI/death paths.
Foreign ownership, authority mismatch, saved serial mismatch despite a matching
replacement pointer, stale find/const enumeration and successful readoption
are covered. Existing 16 tick / 162 call expectations are unchanged.

Native gates (Release, arm64)

- Pending full CTest note committed first: 0089efbf.
- Rebuild `cmake --build build/native -j 4`: PASS, 2.73 s.
- Focused CTest `-j 3`: 7/7 PASS, 17.16 s.
- Direct mission-2 suite: 512 checks PASS.
- Full `ctest --test-dir build/native --output-on-failure -j 3`:
  252/252 PASS, exact CTest real time 1137.13 s (runner wall 1137.15 s).
  No timeout; no isolated rerun needed. Existing limits unchanged.
  Initial unrelated host load exceeded 120; no unrelated process was modified.
- Clean full-suite milestone committed: f35d70f9.

Affected Godot gates

Release main extension configure/build `-j 4`: PASS (0.61 / 37.58 s).
Headless editor import: PASS (4.73 s). Selected checks:
- Enemy presentation identity, headless: 22 checks, zero failures, 0.37 s.
- Scene play flow: 65 checks, zero failures, 2.36 s.
- Playable wave, 640 ready ticks: 1341 checks, zero failures, 5.15 s.
- Enemy presentation identity, rendered compatibility: 22 checks,
  zero failures, 0.68 s.
- Mission-2 real-data four-level probe: 163 actors, zero failures, 7.98 s;
  aggregate class counts 66/66/30/1.
Scene flow, playable wave and real-data probe emit ObjectDB exit-leak warnings;
exit zero does not establish leak-free teardown. Duplicate static-library linker
warnings also remain. No script-error diagnostic in these selected checks.

Journey gate

Ran `python3 tools/run_loop_playthrough.py` with the established read-only
private input and local Godot, default headless and rendered variants.
Overall exit 1, wall 1532.62 s.
- Headless: exit 1, final result written, 67 steps, six refusal kinds.
  Enemy fight remains blocked: no damage, kill, XP, loot or Gold gain;
  3000 swing cycles ran. Trigger/portal and death reload remain refused.
  ObjectDB exit-leak warning remains. This differs from the older r1
  one-kill journey and is not promoted to gameplay success.
- Rendered: exit 124 at the runner's unchanged 900-second limit, no final
  result file; 58 logged steps through passing combat moves, 35 captures.
  No script-error diagnostic before timeout. Inspected walking and fight-start
  captures: player and level geometry render; UI labels unsupported services
  and equipment fallback. Complete fight/death output is unverified.
- Input file counts: 994 before / 994 after. These are counts, not a
  content-hash immutability claim. No journey gate was weakened or bypassed.
- A brief read-only headless process sample while the fight ran showed active
  session ticks and snapshot-reference validation; no registry deadlock was
  established. No performance change is part of this bounded audit fix.

Trial merge

Earlier trial against 8a5e0f22 exited zero, conflict-free. Integration advanced
while gates ran. Final `git merge-tree --write-tree` against integration
`44646165a1ef8e030728c6f522e2660ae5a59792` and HEAD exited 1.
Content conflicts: `native/src/campaign_fight.cpp`, `native/src/campaign_fight_probe.cpp`.
No real merge or integration checkout was performed; conflict resolution is
left to integration ownership. The shared lifetime helper itself has no
reported textual conflict. This result supersedes the earlier clean trial.

Completion boundary

M2H-1 implementation and requested lifetime regression coverage are complete;
no extra implementation commit was needed beyond the reviewed prior commits.
All requested gates were executed. Full journey success and conflict-free
integration are not established. Existing loaded AI/death/pickup/portal
refusals remain intact. This continuation changed only this report.
Detailed logs and captures stay ignored under work/mission2-r2; no private
input content, decoded resource names or private input paths added to commits.
