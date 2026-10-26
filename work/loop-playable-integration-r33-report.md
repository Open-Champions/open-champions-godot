STATUS: RUNNING — four audited tips merged and committed; pending full CTest. Wave 33 validation is in progress.

# Playable integration wave 33 — 2026-10-26

## Scope, evidence and merges

Worked only in `worktrees/playable-integration`, branch `codex/playable-integration`, starting clean at wave-32 `3550e1bf`. Read wave 32 first, shared/repository rules, contributing guide, roadmap and all four requested PASS audits before merging. Private disc inputs remain read-only; raw logs/captures stay ignored under `work/playable-wave33/` and `work/private/w33-*`. No push, branch creation, PR, or process-name termination.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-blood1` | `b2911263` | `play-blood1-r1-fix.md` | `d0772e2d` | `godot/scenes/real_data_play.gd`, `godot/tests/loop_playthrough.gd`, `tools/run_loop_playthrough.py` |
| `codex/play-loading` | `5f60c336` | `native-loading-r1-fix2.md` | `30601cc6` | `godot/scenes/real_data_play.gd`, `godot/tests/loop_playthrough.gd` |
| `codex/port-blood1-hostiles` | `e37f806f` | `port-blood1-hostiles-fix2.md` | `2e491a56` | `native/CMakeLists.txt`, `native/simulation_targets.cmake` |
| `codex/port-minotaur` | `65f6cb05` | `port-minotaur-r2.md` | `8572a4bc` | none |

All four were full `git merge --no-ff` in the requested order, each committed immediately after conflict resolution. No cherry-pick or omitted tip.

## Conflict resolutions

- Retained marker-record, mission-art, physical-use, gate and route/carry checks alongside Blood1 story census, exact choice/destination checks and attachment state. Kept the integration wrapper output default and added both Blood1 options. Selected script actions remain labelled diagnostics.
- Composed loading display/signals and input lock with researched mission-menu, hub services, player/death presentation. Route carry requires loading readiness as well as script attachment and all previous player/campaign checks. Existing level-change row and incoming fade captures remain. Missing prerequisites stop the native session; no bypass adapter was invented.
- Kept both campaign actor and Blood1 actor libraries/tests. Blood1 uses the existing archive binding selector in extension builds, avoiding a second archive implementation. No fixture, baseline or check was weakened.
- Minotaur merged cleanly with one owns_live_object/register_actor/retire_stale implementation. Shared-World pointer/serial/alive guards and construction/hit/held-part refusals remain intact.
- Follow-up `264150c3` awaits the now-asynchronous loading transition in all retained journey probes, including walk-in, destination and hub-return paths.

## Gates and evidence

Candidate runtime revision: `264150c3` (recovery starts at `6dbc3369`, report-only descendants). Full CTest is DONE: 250/251 parallel in 2300.12 s; netplay timeout passed alone in 162.98 s, as recorded below. Full CTest was not rerun during recovery.

- Recovery native Release build: PASS, `cmake --build build/native -j 4` (resume-native-build.log).
- Focused Blood1 actor gate: **1/1 PASS**, 0.02 s, `ctest --test-dir build/native --output-on-failure -R '^blood1-actors$'`. Exercises Kobold variants/WarWraith, 320 original-backed targeted frames, call order/height/turn/hit/completion and explicit refusals. No fixture regenerated or weakened.
- Native AI probe: PASS, 78 boars, 600 ticks, 3 moved/wandered, 2 chased, 2 attacked, 35 state changes, 13 attack entries, 14 hit events, 0 failures. Diagnostic placement only.
- Native death probe: PASS, 78 actors, 600 ticks, dead/zero HP retained, countdowns zero, phase 1/fade 128, 239 native death-clip steps, record retention and reload refusal passed. Diagnostic hit only.
- Recovery evidence: ignored `work/playable-wave33/resume-{native-build,blood1-actors,boar-ai,death-probe}.log`. All four universal extension/library configure/build pairs PASS (8/8 exit 0), Release, arm64;x86_64, parallelism 4; `build-results.json` and `resume-extension-builds.out`. Remaining Godot/private/journey gates pending fresh recovery execution.

## Full scripted journey

Pending fresh headless/rendered execution; compare against wave 32's 53/54 steps without relaxing acceptance. Native AI/death diagnostic probes above pass; those do not establish the full journey.

## Exact command families

Same drivers/method as wave 32, output directory changed to wave 33, build parallelism 4, full CTest parallelism 3. Additional focused loading, actor-identity, combat-move and death cases run against the merged candidate. Private paths remain only in ignored drivers/logs.

## Remaining boundaries

The audited loading prerequisite adapter is absent and must refuse. Original physical interaction, gate lifecycle, unsupported actor services, Gold/ground pickup, checkpoint/reload, trajectory/pixel parity and cross-platform execution remain bounded as in wave 32 and the incoming audits. Fresh gate results are pending; B1H-7 is not yet closed by this record.

## Full CTest result record

Executed on runtime candidate `264150c3` after pending record `4fd70467`. Exact output tail:

```text
        Start 244: campaign-fight-boundaries
249/251 Test #241: minotaur-class .......................   Passed    0.25 sec
250/251 Test #242: minotaur-stats-exact .................   Passed    0.39 sec
251/251 Test #244: campaign-fight-boundaries ............   Passed    0.22 sec

99% tests passed, 1 tests failed out of 251

Label Time Summary:
fx             =   4.89 sec*proc (5 tests)
native-fx-2    =   4.89 sec*proc (5 tests)

Total Test time (real) = 2300.12 sec

The following tests FAILED:
	201 - netplay (Timeout)
Errors while running CTest
```

Isolated rerun (recorded by the orchestrator, 2026-10-26 15:15, after the job's provider stopped): the full run's only failure was `netplay` (Timeout) while machine load was ~290. Rerun alone at load ~11 with `ctest --test-dir build/native -R '^netplay$' --output-on-failure -j1`: **Passed, 162.98 s**. Full CTest therefore stands at **250/251 in the parallel run (2300.12 s), with the single timeout passing in isolation**; no assertion failures. Remaining wave 33 gates (Godot matrix, synthetic combat, FULL_SAVE_LOAD, FULL_TRACE, sweeps, probes, playthrough) are still pending.
