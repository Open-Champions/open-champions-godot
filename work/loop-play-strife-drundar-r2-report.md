STATUS: DONE — both PSD-R1 findings fixed; requested gates recorded.
Full mission gameplay remains blocked at the unchanged evidence boundary.

Pinned research remains 0d42d465276aeb768ad54316b34cb4dabe3a2c35.
PSD-R1-1: attachment checks Mission 1 preStart stop; refusal/fault/wait
prevents adoption, dialog, start and automatic restoration. Missing preStart
is not a refusal. Restoration remains an explicit native diagnostic only.
PSD-R1-2: door entry refuses the unknown predicate at 0xa78 before any
addressed dispatch or save-inhibit mutation. No polarity is inferred and
Contract::inspect does not claim to establish that unknown cell. Mutation
fixtures verify both changed and removed operations are refused by the
entry diagnostic (not rejected by the wider bounded static contract).

Regression: Godot production attachment checks no adoption/start/restoration
sites or counter changes after refused preStart and subsequent ticks.
Native mutation tests preserve save inhibit and all reached-site counts.

Pending full CTest (-j 3) checkpoint committed before invocation.
Release build and focused results, Godot headless/rendered, full journeys
and unchanged FULL_TRACE verification will be recorded after completion.
No push/PR; private inputs remain read-only and untracked.

## Fresh verification — 2026-10-27

- Release native configure/build PASS (-j 4); Release GDExtension build PASS
  (-j 4). Logs: work/mission1/r2-configure.log, r2-build.log, r2-extension.log.
- Focused mission1-scripts/script-commands: 2/2 PASS, 0.52 seconds.
  Direct mission fixture: 88 checks, zero failures. Initial new mutation
  fixture used invalid opcode zero and dereferenced the refused load; fixed
  to an authored replacement PROC and guarded the fixture contract access.
  Final focused and full runs use the corrected fixture (be527959).
- Mission 1 Godot headless: 46 checks / zero failures, 4.90 seconds.
  Rendered: 48 checks / zero failures, 5.19 seconds. Both exit zero.
  The former automatic Strife census/effect checks are replaced by explicit
  production-attachment refusal assertions: no objects adopted, no start,
  no restoration/visibility sites and no counter change on subsequent ticks.
  Native explicit restoration coverage is retained. Strife's automatic
  attachment intentionally no longer reaches its object census or visibility.
- Rendered destination capture inspected: terrain/HUD and capsule fallback;
  no visual parity claim. Only the non-stopped destination is captured by
  this probe; the stopped branch is checked through controller state.
- Full-combat headless: 2,641 checks / zero failures, exit zero (18.08 s).
  Rendered: 2,771 checks / zero failures, exit zero (21.87 s).
  FULL_TRACE unchanged in both: 36,402 bytes, SHA-256
  9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659.
  Headless save/load presentation matches at 300/300; rendered draws 299/300
  and correctly does not compare unequal-count presentation hashes.
- tools/run_loop_playthrough.py: headless 67 steps, rendered 68 steps
  (43 captures), six refusal kinds each; both write results and exit 1 at
  blocked_death_reload, matching the requested wave-33 boundary. Wrapper
  wall time 382.51 seconds. Read-only input count remains 994 before/after.
  These are preserved blocked journeys, not passing full gameplay acceptance.
- All final Godot logs have no SCRIPT ERROR or ERROR envelopes. Existing
  ObjectDB exit warnings remain disclosed. Gate outputs remain ignored under
  work/mission1/r2/. No baseline or journey-tool changes.
- Full CTest remains pending at this evidence checkpoint; pending note was
  committed in 439c1479 before its invocation. Final result follows.


## Final full CTest and delivery

Full CTest: ctest --test-dir build/native --output-on-failure -j 3.
252/252 PASS; exact real elapsed time 690.70 seconds. No timeouts and no
isolated timeout reruns required. Raw evidence: work/mission1/r2-ctest-full.log.
The pre-run pending checkpoint is 439c1479; corrected fixture is be527959.

Final diff whitespace check passes; branch remains codex/play-strife-drundar.
Only the four task source/test files and this forced-added report changed.
The optional untracked mission1_real.gd.uid remains untouched. All changes
are committed locally; no push/PR or prohibited worktree/process action.
No private payload, decoded identifier inventory, or private path is tracked.
Restoration is no longer an automatic attachment phase. The unestablished
0xa78 operation remains Unknown: all door entry diagnostics refuse regardless
of its bytes, rather than pretending a polarity check is evidenced. Existing
native explicit restoration tests remain; stopped automatic Strife attachment
does not claim object adoption or mission readiness.

Coordination 2026-10-27: PSD-R1-1 and PSD-R1-2 implementation complete;
Release, full native suite, both Mission 1 Godot variants, unchanged trace
and wave-33 67/68-step journey boundaries verified. Final report commit is
HEAD returned to the parent.
