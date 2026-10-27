STATUS: MORE — PLR2-1 fix verified; all requested revision gates complete; inherited UI diagnostic and live arrival refusals remain.

## Historical round-2 implementation and pre-fix gates

The following checkpoint history predates the independent PLR2-1 test review at
the end of this report. Pending statements in this history are superseded by
that review; the 718.00 s CTest result is pre-fix.

Assigned branch verified clean at 3278e618 before work.
Research pin: research-loading-readiness@d6f035fab6bb5cbe66f5c72a55a1a6aef05ee8df,
`docs/loading-arrival-r2.md`; research and round-1 audits PASS.

Full CTest: 251/251 PASS, 718.00 s, no timeouts. Godot: 62/63 public cases clean
(one inherited UI diagnostic), private disc case separately PASS. Journeys remain
67 headless / 68 rendered, exit 1 each; FULL_TRACE unchanged. Historical pending
notes below retain the committed pre-gate checkpoint chronology.

Implementation checkpoint: production deferred queue already has documented
VM-error continuation; regression executes HALT, bounds/division errors, missing
entry and native-fault stop through Host. Added memory-backed equipment lifecycle,
seven routes, bounded publication, explicit I/O/fixup services and matching model
pair output. Shared World explicit stores now count as runtime publication, while
sparse defaults and executable backing do not. Actual transformation services
remain unbound; no live model-readiness claim. Focused script-commands PASS (0.46 s).
Release native build PASS; initial added shared-World test needed its combat-runtime
link dependency, fixed without changing assertions. Full CTest still pending.

## Per-condition implementation and research evidence

All round-2 references below are to `research-loading-readiness` at exact pin
`d6f035fab6bb5cbe66f5c72a55a1a6aef05ee8df`. These are addressed Observed/Executed
contracts with synthetic original-function execution, not live arrival proof.

- Deferred (`docs/loading-arrival-r2.md:11-35`): retained the real Variables queue's
  result-discard behavior and added pinned source comments. It terminates the
  failing VM entry, continues to the following entry, promotes captured back count
  once, clears back and increments parity on return. Tests execute authored HALT
  4/27/-1, bounds/division errors and a missing entry, observe the following quest
  write and a self-deferred nonempty front after exactly one promotion. Existing
  loading observation requires the returned pass, not queue exhaustion.
- Fault/refusal (`docs/loading-arrival-r2.md:37-54`): an unbound SYSREQ native
  faults the real Host, does not reach the following quest write, does not promote
  counts/parity and cannot certify loading. Existing discarded-command refusal
  counters still invalidate loading completion. No actual arrival command census
  is invented; no unsupported command is granted a successful effect.
- Player/slots (`docs/loading-arrival-r2.md:58-73`): memory-backed publication
  stores player index +0x3c8; non-display-only (+0x462 == 0) publishes into
  record +0x43d8 at 0x4d8af0 + index*0x4c30. Selected-slot publication writes
  +0x43f0 + 4*slot. These bounded slices do not implement inventory selection,
  construction, conflicts, hooks or reconstruction. Client publication accepts
  only exercised indices 0/1, and the inherited 17-slot domain
  (`docs/loading-readiness.md:41-53`). Sparse missing bytes refuse. The shared
  World regression spawns an actual native combat player; explicit combat stores
  are visible to readiness without copying a fabricated player or empty inventory.
- Specialized routes (`docs/loading-arrival-r2.md:75-97`): all seven slot/cache
  routes and index*0x58 stride are implemented; tests exercise actual cache memory
  for both indices, pending and validated pointer outputs. Host scanner consults
  these routes under the documented environment and definition guards and trims
  four characters only from explicitly supplied formatter output. Formatter is
  unbound in ordinary play. Slot-3 selection still refuses. Generic lookup is
  unchanged/refused. PLR2-1 correction: servicing requires published current and request frames and
  current minus request >= 4, per `docs/loading-readiness.md:63-72` and
  `tools/difftest_loading_readiness.py:85-101`. The original absolute 3/4 controls
  failed to establish this interval. Wrap behavior is unestablished; current below
  request refuses rather than assuming rollover semantics.
- Submission (`docs/loading-arrival-r2.md:101-118`): explicit service entry accepts
  resolved metadata/file location and age/busy observations; neither metadata
  search nor a device result is synthesized. Implements three source-address
  selections, sector offset/size arithmetic, submitted short, model offset,
  completion-pointer storage/clear, request words and active byte. Actual transfer
  callback is mandatory and absent by default. Test observes all request fields,
  all source routes, young/busy suppression and no duplicate submission.
- I/O completion (`docs/loading-arrival-r2.md:120-128`): ordinary busy waits;
  error requests a bound retry and stays pending; idle/no-error writes completion
  and clears active. Streaming requires observed remaining bytes, waits above zero
  and completes at zero. Tests drive real runtime fields; missing retry/streaming
  observations refuse. Offline never completes local media work.
- Cache publication (`docs/loading-arrival-r2.md:130-138`): pending completion
  leaves both outputs zero. Completion requires both fixup services in documented
  order and usable resource validation before identity, valid short and outstanding
  clear are published. Completing call still has zero outputs; next matching
  lookup produces (base+offset,base). Default fixups/validation are unbound and
  refuse even with a completion byte or valid bit. Authored regression services
  transform owned fixture bytes; invalidated bytes revoke readiness. They are not
  evidence of actual resource transformations. Outstanding-cache identity check,
  publication coverage, supported-domain and pointer-overflow checks are explicit
  client safety restrictions, not new claimed original branch conditions.
- Header (`docs/loading-arrival-r2.md:140-144`): count/override selects the two
  documented identifier addresses; metadata/header contents remain unknown.

## Remaining refusals

Destination-specific deferred commands, slot-3 selection, generic model resolution,
actual formatter/header metadata, inventory/constructor effects, media transfer,
resource fixups and usable live models remain unverified/unbound. No empty-table
workaround, placeholder pair, offline I/O completion, ready-bit shortcut or success
fallback was added. Peer/store/save behavior is inherited from audited round 1.

## Verification in progress

Pending full CTest was committed first in `00107865`; implementation checkpoint
`5334d44e` retained the pending note before aggregate execution. Release native
configure/build and Release extension configure/build PASS (`-j 4`). Focused
builtins/commands: 5/5 PASS, 69.52 s; final commands after route controls: 1/1 PASS,
0.46 s. Full CTest uses `-j 3`; exact count/time and timeout reruns pending.
Initial shared-World test link failure was fixed by adding its actual combat-runtime
link dependency, preserving every assertion. Godot and journey gates running.

### Godot and trace checkpoint

Strict isolated aggregate: 58/61 PASS. Same three inherited/setup failures as
round 1: private loading-disc case without required input argument (exit 2),
native-FX capture's absent relative work directory (exit 0 plus PNG-save ERROR),
and `ui_layout_smoke` calling `hud_text_glyphs` with four arguments instead of five
(exit 0 plus engine ERROR). No diagnostic was suppressed. Supplied private input
and capture directory in isolated reruns: both PASS. Three additional rendered
identity/death/loading-present cases PASS. Final public total: **62/63 clean**;
private disc case separately PASS. No test/fixture/baseline was changed.
Evidence: `work/loading-r2-matrix/results.json`,
`work/loading-r2-additional/results.json`, associated logs.

`FULL_TRACE`: **36,402 bytes**, SHA-256
`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged.
`FULL_SAVE_LOAD`: 300/300, identical continued/uninterrupted presentation hashes.
Evidence: `work/loading-r2-matrix/playable_wave_smoke_full_combat.log:17-18`.

Headless journey writes result JSON at **67 steps**, unchanged from round 1.
Arrival step **37** has attached script, two loading frames, zero audio-handoff
errors, `loading_ready=false`, with the same deferred/equipment refusal. Final
step 67 is `player_death: blocked_death_reload`, 239 native clip steps, no respawn.
Six refusal kinds, zero captures. Rendered journey/full CTest still pending.

### Journey completion and visual boundary

Rendered journey: **68 steps / exit 1**, **43 captures**, six refusal kinds,
matching round 1 exactly. Arrival step **38** refuses after two loading frames;
script attached, `loading_ready=false`, zero audio-handoff errors. Headless arrival
is step 37 with the identical diagnostic:

`Deferred loading pass absent, faulted or refused commands. Equipment readiness unavailable: runtime player/slot/model publication or specialized I/O unbound.`

Both finish at `player_death: blocked_death_reload` (headless 67, rendered 68),
239 native clip steps, respawn false. Both result JSON files were written; neither
journey timed out. Unchanged wrapper exits 1 because both variants exit 1.
No additional peer/store/save barrier appeared. Both logs have zero SCRIPT ERROR
or engine ERROR diagnostics, with one ObjectDB exit-leak warning per variant.
Input file counts: **994 before / 994 after**, counts-only, not a content hash.
Evidence: `work/loading-r2-playthrough/run-summary.json`, both `result.json` files,
logs and `work/loading-r2-summary.json`.

Inspected actual rendered loading and destination captures. The loading-labelled
capture shows the preceding live view. The destination capture shows a stopped
session and disabled Resume. No playable handoff, live-model readiness or loading
art parity is claimed. Images remain ignored/private and are not committed.
Full combat fixture: **2,641 checks / zero failures**, separate from journey
acceptance (`work/loading-r2-matrix/playable_wave_smoke_full_combat.log:21`).

## Final gate results and disposition

2026-10-27 — assigned worktree/branch only. Requested implementation and all gate
runs completed. `STATUS: MORE` denotes unresolved live arrival/resource authority,
not an unfinished test job. No push, PR, process-name termination, research write,
main research checkout access or playable-integration write was performed.
No memory update was requested or performed.

- Release: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4` PASS. Release extension configure/build PASS
  (`cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release`,
  `cmake --build build/godot-extension -j 4`). Final native/extension binaries
  include the committed implementation. Evidence: `work/loading-r2-configure.log`,
  `work/loading-r2-build.log`, `work/loading-r2-extension-configure.log`,
  `work/loading-r2-extension-build.log`.
- Full CTest: `ctest --test-dir build/native --output-on-failure -j 3`:
  **251/251 PASS, 718.00 seconds**, no failures/timeouts, no isolated timeout rerun
  necessary. Evidence: `work/loading-r2-ctest.log:501-511`. Pending-full-CTest note
  was committed as `00107865` before this run, and retained at `5334d44e`.
- Godot strict public matrix: **62/63 clean** after explicitly recorded setup
  reruns and three additional rendered cases; inherited UI argument-count ERROR
  remains a failure. Private loading-disc gate separately PASS. Initial aggregate
  **58/61** is preserved, not overwritten or represented as an all-pass result.
- Full combat **2,641 checks / zero failures**; save/load **300/300**.
- FULL_TRACE **36,402 bytes**, SHA-256
  **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**,
  exact round-1 baseline; no explanation/change needed.
- Required unchanged playthrough wrapper ran headless and rendered to written
  results, neither timeout: **67 / 68 steps**, **exit 1 / 1**, **0 / 43 captures**,
  six refusal kinds each. Arrival refuses at **37 / 38** with unchanged deferred
  and equipment diagnostics; both finish **player_death: blocked_death_reload**.
  The new function-level contracts do not establish the missing actual commands
  or resource transformations, so no successful arrival is inferred.
- `git diff 3278e618..HEAD --check` and final worktree diff check PASS.
  Tracked additions contain authored code/fixtures and this report only; no game
  payloads, private identifiers, decoded content, images, raw logs or private paths.
  No existing assertions, tests, fixtures, baselines or diagnostic allowlists were
  weakened. Raw gate evidence remains ignored under `work/loading-r2-*`.

Local checkpoints: `00107865` pending gate note; `5334d44e` implementation and
native regressions; `cc54eba1` matrix/trace/headless evidence. This final report
commit records the completed aggregate/journey results. No playable campaign or
real transformed model acceptance is claimed.

## PLR2-1 revision checkpoint

Request-relative timing fixed; missing request-frame publication and backwards/
wrapped frame domain refuse. Production-runtime regressions use request 100 and
current 100/103/104, with submitted/completed I/O and bound fixups, checking no
early fixup, identity, valid-bit or outstanding-owner publication. Existing
assertions and baselines retained. Release build, full CTest (-j 3), Godot dev
matrix, both journeys and FULL_TRACE verification PENDING for this revision.
This pending note is committed before the full CTest run.

PLR2-1 fix full CTest (recorded by the orchestrator, 2026-10-27 ~12:15, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` on `960835de` — **251/251 passed, 665.72 s**, no timeouts (log `work/plr2-fix-ctest.log`). Godot dev matrix, both journeys and FULL_TRACE for this revision are still PENDING and must be run by the next job before re-audit.


## PLR2-1 independent test review (2026-10-27)

Started clean on assigned branch at `00cf5f51`; reviewed `960835de` against
PLR2-1 and its pinned references quoted in the audit. No native edits required.
The earlier absolute draw-frame 3/4 timing claim was incorrect: servicing must
use the published request frame at `0x4ef3f0` and published current frame at
`0x44fdd0`, with current minus request at least four. The fix enforces that
interval and refuses missing publication or the unestablished backwards domain.
Already-valid matching resources retain the existing validated cache-hit route.

Production regressions use request 100, current 100/103/104. At elapsed zero
and three, submitted/completed state and both bound fixups make early publication
observable; assertions retain empty fixup calls, submitted/completion state,
previous identity, invalid bit and outstanding owner. At four, both fixups run
in order, identity/valid/owner publish, outputs remain zero until the next match.
Absent request-frame publication separately refuses. No assertions were weakened.

Full CTest was not rerun: no native code changed. Inspected the recorded log on
the fixed revision: **251/251 PASS, 665.72 s**, no timeouts, as recorded by
`00cf5f51` in `work/plr2-fix-ctest.log`. The older 718.00 s result above belongs
to the pre-fix revision and is not evidence for `960835de`.

Fresh revision gates completed under ignored `work/plr2-verify-*`:

- Strict isolated Godot dev matrix with captures: **58/61 PASS**, exit 1.
  Failures: private disc case missing its required input, native-FX capture missing
  the isolated relative output directory, and inherited UI diagnostic. Isolated
  supplemental runs supply the input/output prerequisites: both setup failures
  PASS; rendered identity/death/loading-present cases also PASS (5/5 total).
  Final public coverage is **62/63 clean**; private disc separately PASS.
  Evidence: `work/plr2-verify-matrix/results.json`,
  `work/plr2-verify-additional/results.json` and corresponding logs.
- `ui_layout_smoke` remains a strict failure: `hud_text_glyphs` expected five
  arguments, called with four via `callv`. Confirmed the same four-argument call
  in base `3278e618:godot/tests/ui_layout_smoke.gd:84`. Exit zero does not make
  the diagnostic clean. No diagnostic allowlist or test was changed.
- FULL_TRACE **36,402 bytes**, SHA-256
  **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**,
  exact required baseline. FULL_SAVE_LOAD **300/300**, matching continued and
  uninterrupted presentation hashes; full combat **2,641 checks, zero failures**.
  Evidence: `work/plr2-verify-matrix/playable_wave_smoke_full_combat.log`.
- Unchanged `tools/run_loop_playthrough.py`: headless **67 steps / exit 1**,
  rendered **68 steps / exit 1**, both result JSON files written, neither timeout.
  **0/43 captures**, six refusal kinds each. Arrival **37/38**: script attached,
  two loading frames, `loading_ready=false`, zero audio-handoff errors, same
  deferred/equipment diagnostic quoted above. Each ends at
  `player_death: blocked_death_reload`, 239 native clip steps, respawn false.
  No additional peer/store/save barrier observed. Both logs have zero script or
  engine ERROR diagnostics and one ObjectDB exit-leak warning each. Input file
  count **994 before / 994 after** (counts only, not a content-integrity hash).
  Evidence: `work/plr2-verify-playthrough/run-summary.json`, both result JSON
  files and logs. Wrapper exit 1 truthfully retains the two journey refusals.
- Inspected the fresh rendered destination capture: stopped session with disabled
  Resume. No playable handoff or live resource/art acceptance is established.
  Captures, raw logs, private input and helper scripts remain ignored/uncommitted.

No native, Godot, fixture or baseline edits were needed. Only this report changed.
All requested execution is complete; STATUS MORE retains unresolved live arrival
and resource authority plus the inherited strict UI failure. No push or PR.
Final whitespace and staged-content checks PASS; no private data or paths added.
