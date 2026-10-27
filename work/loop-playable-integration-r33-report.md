STATUS: PENDING — loading wait liveness fix and truthful partial readiness adapter committed; focused regressions pass; fresh matrix/journeys/trace running. Unsupported deferred/equipment/peer/store readiness still refuses.

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
- Recovery evidence: ignored `work/playable-wave33/resume-{native-build,blood1-actors,boar-ai,death-probe}.log`. All four universal extension/library configure/build pairs PASS (8/8 exit 0), Release, arm64;x86_64, parallelism 4; `build-results.json` and `resume-extension-builds.out`. Fresh Godot/private/journey outcomes follow below.

- Recovery Godot dev matrix: **61/61 exit 0**, comprising the inherited 54 plus seven loading/identity/combat/death cases. Full synthetic combat: **2,641 checks, 0 failures**. `FULL_SAVE_LOAD`: **300/300**, identical continued/uninterrupted presentation hashes. `FULL_TRACE`: **36,402 bytes**, SHA-256 **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, unchanged. Evidence: `resume-godot-matrix.out`, `guard-matrix/godot-results.json`, `godot-combined-full-combat.log`. This is synthetic/fixture coverage; full journey outcomes follow below.

- Recovery private sweep `w33-on`: **111/111 tried, loaded, played; 230 checks, 0 failures, 0 top-level refusal kinds**. Four 60-tick slices; private disc read-only. Internal unsupported-service counters remain separate. Counts-only summaries: ignored `work/private/w33-on-s{0,28,56,84}.json`; log `resume-sweep-w33-on.out`. Full journey outcomes follow below.

- Recovery private sweep `w33-off`: **111/111 tried, loaded, played; 230 checks, 0 failures, 0 top-level refusal kinds**. Four 60-tick slices; private disc read-only. Internal unsupported-service counters remain separate. Counts-only summaries: ignored `work/private/w33-off-s{0,28,56,84}.json`; log `resume-sweep-w33-off.out`. Full journey outcomes follow below.

- Focused fresh matrix counts: loading display **57/57**, loading audio **10/10**, enemy identity **22/22 headless and rendered**, campaign moves **33/33**, death UI **11/11 headless, 13/13 rendered**. Fresh matrix logs contain **0 SCRIPT ERROR** entries; **18 logs retain ObjectDB exit-leak warnings**.
- **B1H-7 CLOSED for gate-report completeness**: integrated Blood1 hostile native paths were executed by the focused actor gate (including the 320 original-backed targeted frames), the inherited full native result is recorded without rerun, all four extension/library builds pass, 54 inherited Godot cases plus seven additions pass, both 111-level sweeps pass and FULL_TRACE is unchanged. This does not claim live Blood1 hostile scene binding, placement or landed damage; those remain outside the actor gate's evidence.

- Fresh full journey: headless **67 steps / exit 1**; rendered **36 logged steps / exit 124 at 900 s**, no final rendered result. Both variant outcomes recorded below; journey acceptance BLOCKED. Wrapper counted **994 disc files before/after**.

## Full scripted journey

Fresh recovery execution of `tools/run_loop_playthrough.py` used the same headless/rendered driver and its unchanged **900-second per-variant timeout**. Evidence stays ignored in `work/playable-wave33/resume-playthrough/`, `resume-playthrough.out` and `resume-followup.out`.

- **Headless: 67 steps, exit 1**, result JSON written, six refusal kinds, zero captures, no SCRIPT ERROR. It reaches `player_death: blocked_death_reload`; no crash or timeout. The inherited 53 steps gain 14 Blood1 continuation/boundary records. Those new records are refused/unverified after the loading failure, not successful Blood1 gameplay.
- **Rendered: exit 124 at 900 seconds**, **36 logged steps**, **21 PNGs**, **no result JSON**, zero SCRIPT ERROR. Last logged step is `mission_1_story_request: blood1`; only `10-start.png` of the loading-state captures is written. This does not pass wave 32's 54-step rendered journey. Earlier ignored 67/68-step wave-33 logs were not substituted for this fresh run.
- Wrapper exit 1; private disc files **994 before / 994 after**. Read-only inputs; no private payload or capture tracked.
- **Confirmed prerequisite blocker:** headless Blood1 arrival reports two loading presents, `loading_ready=false`, record carry false and mission availability false. `_loading_prerequisite_state()` finds no controller adapter for deferred script/equipment/peer/store readiness; `_refuse_loaded_transition()` stops the session. The check/refusal handler are identical to audited incoming `play-loading` tip `5f60c336`; the missing adapter is already an explicit incoming boundary. Blood1 destination/completion/hub-return are unverified. No readiness values or success fallback were invented.
- **Rendered stall, exact renderer cause Unknown:** the live process stayed in main-loop/SceneTree processing and frame delay after the start capture. `_wait_loading_present()` awaits post-draw signals during three start presents; this helper is identical to `5f60c336`. Static-view missing post-draw signals are a suspected mechanism (also documented by the existing capture helper), but the display queues redraw from its post-draw callback, so this is not a proven renderer root cause. An attempted window-state inspection was denied macOS assistive access (-25211); it was not bypassed. Another worktree's rendered process was observed and left untouched; interference is unproven. The initial loading PNG was visually inspected; no original-client pixel parity is claimed. Process sample remains ignored in `resume-rendered-sample.txt`.
- Headless physical walk-in: **92 sources**, **91 attempts**, **81 navigation stalls**, **6 entered-without-change**, no queued-by-walk success. PushTrigger/Portal retain expected refusals. Combat moves pass **22/22**.
- Headless fight: **78 actors**, **2,778 boar state changes**, **1,321 attack entries**, **1,185 hit events**; labelled weakest-enemy placement after failed physical approach. Native damage kills one enemy, grants **2,100 XP** and one level. One loot call yields **0 items**, **8 shared RNG draws**, unchanged Gold; `blocked_unported_combat` retains missing automatic-container Gold source/gain. No pickup or ordinary-route combat parity claim.
- Carried/cold loads restore their bounded state; mismatched display refuses. Final diagnostic death has zero HP, retained dead record, countdowns zero, phase 1/fade 128 and **239 native death-clip steps**; checkpoint/reload stays refused. No rendered fight/death acceptance is claimed because the fresh rendered journey never reaches them; separate rendered identity/death UI fixtures pass in the matrix.

No integration-level conflict fix was indicated by the confirmed prerequisite refusal. The presentation stall is on unchanged incoming loading code and its exact cause is unresolved; changing that implementation or inventing its adapters is outside this test-only integration-conflict scope. Full journey acceptance therefore remains BLOCKED.

## Exact command families

Same drivers/method as wave 32, output directory changed to wave 33, build parallelism 4, full CTest parallelism 3. Additional focused loading, actor-identity, combat-move and death cases run against the merged candidate. Private paths remain only in ignored drivers/logs.

## Remaining boundaries

The audited loading prerequisite adapter is absent and must refuse. Original physical interaction, gate lifecycle, unsupported actor services, Gold/ground pickup, checkpoint/reload, trajectory/pixel parity and cross-platform execution remain bounded as in wave 32 and the incoming audits. All requested remaining gates were executed and recorded above. B1H-7 is closed for integrated gate-report completeness, including explicit Blood1 actor-path exercise; live Blood1 hostile placement/combat is not established. Full journey blockers are the missing audited loading readiness adapter and unresolved rendered presentation stall. No test, fixture or trace baseline changed.

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

Isolated rerun (recorded by the orchestrator, 2026-10-26 15:15, after the job's provider stopped): the full run's only failure was `netplay` (Timeout) while machine load was ~290. Rerun alone at load ~11 with `ctest --test-dir build/native -R '^netplay$' --output-on-failure -j1`: **Passed, 162.98 s**. Full CTest therefore stands at **250/251 in the parallel run (2300.12 s), with the single timeout passing in isolation**; no assertion failures. At that CTest checkpoint the remaining gates were pending; their completed recovery results and journey blockers are recorded above.

## Recovery completion

2026-10-26: requested remaining gates executed with milestone commits; no full CTest rerun. Recovery changes only this tracked report. All four audited tips and `264150c3` remain ancestors. `git diff --check 6dbc3369..HEAD` passes; raw logs/captures stay ignored. No push, PR, branch creation, private-disc write, sibling-worktree edit, or process-name termination.

## Loading implementation follow-up: diagnosis (2026-10-26)

Verified clean `codex/playable-integration` at `7aa582aa`. Wave 32 already ended both full journeys with exit 1 (53/54 steps), ending at `player_death: blocked_death_reload`. The unchanged wrapper returns 1 for any nonzero variant exit or missing result; wave 33's exit 1 alone is not a regression. The rendered 900-second timeout with no result is a regression.

Confirmed liveness defect: `_wait_loading_present()` directly awaits `RenderingServer.frame_post_draw` without a competing deadline. `_advance_present()` requests the next redraw only after a post-draw signal, so it cannot recover a renderer that stops emitting that signal. This wait has no refusal path even while SceneTree processing continues (as the retained process sample reports). The precise OS/window condition that suppressed the old run's signals remains Unknown; no accessibility bypass or unrelated process operation is justified. A host deadline must report refusal, never count a missing signal as a present.

Pinned `research-loading@3ba61c7`, `docs/loading-transitions.md:95-105,197-209,228-230`, and PASS `native-loading-r1-fix2.md` require deferred calls, equipment readiness, applicable peers and four-store stocking. Script attachment is observable but is not deferred-call completion. No exposed runtime equipment/peer/store completion service establishes these gates; offline mode does not waive store work. Implement only observable state and retain explicit unknown prerequisite refusal. Host wall-clock deadlines are client liveness policy, not invented retail timing constants.

### Implementation checkpoints

- `5f75971f`: every start/loading/warm-up/blank wait now requires an actual renderer post-draw observation, polled from SceneTree frames rather than resuming the transition inside a render callback. Missing signals refuse after a five-second **host liveness policy** (not a retail timing value). The refused request stops the session and retains a logged/displayed reason. No missing signal is counted and no start/blank present is skipped. Headless still uses its existing process-frame diagnostic contract.
- `6c37e4b8`: partial host adapter reports the existing script-attachment result separately; deferred completion, equipment-pending, peer readiness and four-store completion stay explicitly unavailable, with `ok=false`. No boolean success fallback, offline waiver or attachment-to-deferred equivalence. Journey records now retain the loading refusal reason.
- Fresh native build PASS. No native source changed, so full CTest is not required by this brief. `ctest -R loading` found no native tests (not a pass claim); focused loading tests are the Godot cases. New waiter/adapter regressions: headless **15/15**, rendered **16/16**, exit 0. They distinguish stale/missing/new signals, bound missing-signal refusal, disconnect observers after either outcome, preserve unknown prerequisites for both attachment states, and observe an actual forced renderer draw. Force-draw is only in the test, using the existing journey-capture mechanism. Full matrix, focused existing loading cases, both full journeys and FULL_TRACE are pending in ignored `fix-*` logs.

Follow-up verification checkpoint: pinned research checkout is exactly `3ba61c74856828197403892ff8169a6d78182965`, with no loading-document delta. Render-disabled regression now passes **17/17 rendered**. It reproduces live SceneTree processing without post-draw signals, then restores rendering and verifies an actual renderer signal. Controller inspection (`enable_level_script` / `Host::run_start`) additionally confirms attachment `ok` does not prove deferred completion: the former ignores `run_start()`'s result, while the latter can return before its deferred pass on refusal/fault. Thus the partial adapter deliberately cannot certify `deferred_complete`. Fresh matrix at checkpoint **24/24 exit 0** (of 61); known `ui_layout_smoke` glyph-call argument error persists. Full headless journey has reached its Blood1 refusal boundary and continues diagnostics; final results pending.

### Fresh gate checkpoint

Fresh dev matrix **61/61 exit 0**, including existing loading display **57/57** and loading audio **10/10**. New present/partial-adapter suite **15/15 headless, 17/17 rendered**. FULL_TRACE **36,402 bytes / `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, unchanged. Combat **2,641 checks / 0 failures**; save/load **300/300**, matching continued/uninterrupted presentation hashes. Native build passed with `-j 4`; no native changes and no full CTest rerun. Evidence: ignored `fix-matrix/`, `fix-matrix.out`, `fix-native-build.log`, `fix-loading-present-{headless,rendered}.log`. Headless full journey completed **67 steps / exit 1**, result written, six refusal kinds, zero captures; explicit unsupported loading readiness remains the boundary. Rendered full journey is pending.

Rendered checkpoint: fresh run passed the old 36-step stall, reached Blood1 arrival and the explicit prerequisite refusal, then passed **22/22 combat-move checks** (58 logged steps at checkpoint). Both `10-start.png` and `10-loading.png` were produced. Inspected actual start and deferred loading captures: start art is visible; the loading capture has already reached the underlying scene after readiness refusal, so that filename is not proof of a loading-phase image. No original-client pixel parity, successful loading handoff or post-refusal Blood1 gameplay is claimed. The production waiter does not force draws or synthesize post-draw signals. The final full rendered result remains pending.
