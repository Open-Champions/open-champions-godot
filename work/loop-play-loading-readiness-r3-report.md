STATUS: DONE — round-3 review PASS; exact-tree CTest 251/251 retained; strict Godot dev gate recorded (inherited UI diagnostic remains FAIL); FULL_TRACE unchanged; headless/rendered journeys 67/68 match baseline with arrival refused. Normal-route command order and starting-equipment mapping remain Unknown.

## Final round-3 review (2026-11-02)

Reviewed the clean `d766b5e4` tree against research
`2550d3473dafc3609bbecbed2ec365bf7a658c60` and its PASS round-3 audit.
**VERDICT: PASS — no remaining in-scope correctness finding.** This closes
the bounded round-3 implementation/review task; it does not establish playable
arrival or a wholly clean Godot matrix. This checkpoint supersedes the earlier
pending notes below. No native code, tests, fixtures, baselines or helpers changed
in this review, so no gates were rerun.

The complete implementation delta from `4dbee736` is confined to the equipment
header, equipment implementation, authored runtime tests and this report. All
new original fields and branches have the following pinned evidence; publication,
overflow and rollback restrictions remain explicitly client-authored:

| Implementation | Evidence at research `2550d34` |
| --- | --- |
| `native/src/loading_equipment.cpp:20-29`: nonzero word `+0x1c` skips; relative word `+0x10`, short `+0xa = 0xa7fb`, guard `-1`; unsigned short `+8`, bit zero / `0x100` early exits | `docs/loading-arrival-r3.md:11-20`; `tools/difftest_loading_arrival_r3.py:23-34` |
| `native/src/loading_equipment.cpp:30-47`: table first-word relocation; four signed bytes in left/top/right/bottom order, inclusive cells, `-1` terminator, four-byte cell stride; accumulator `0x48`, six plus signed product divided by 128, low halfword at `+6` | `docs/loading-arrival-r3.md:22-29`; `tools/difftest_loading_arrival_r3.py:35-46` |
| `native/src/loading_equipment.cpp:50-60`: signed count `+0x26`, nonpositive fallback one, signed width `+0x12` plus one, consecutive words from `+0x28`, second resource's own base and no guard | `docs/loading-arrival-r3.md:31-40`; `tools/difftest_loading_arrival_r3.py:50-59` |
| `native/src/loading_equipment.cpp:179-200`: first fixup then second, publication only after validation, completion returns zero outputs, next matching lookup avoids another relocation | `docs/loading-arrival-r3.md:49-55`; `tools/difftest_loading_arrival_r3.py:139-170` |

Only the round-2 missing-fixup refusal is replaced. An absent validator, a
half-bound override, unpublished inputs and relocation overflow still refuse;
validator false/exception restores the planned resource bytes and retains the
pending cache. The rollback does not promise to undo arbitrary adapter/validator
side effects. There is no default successful production validator. The Host
scanner and Godot prerequisite bridge retain unavailable-state propagation
(`native/src/script_commands.cpp:1239-1327`,
`gdextension/src/session_controller.cpp:1794-1824`). Normal-route ordered commands
and starting-equipment association remain unknown exactly as required by
`2550d34:docs/loading-arrival-r3.md:65-69,93-106`.

Test review: `native/tests/script_commands_tests.cpp:1490-1692` uses authored
non-game bytes and identities through the real Host-owned Runtime: 252 paired
controls, three table shapes, six sparse early exits, rejecting/throwing validator
retries and 14 incomplete/overflow/traversal refusals. Whole-buffer comparisons,
unchanged matching rescans, pending-cache checks and preflight write-history checks
remain intact. The test is registered at `:1989`; existing deferred-command and
unavailable-equipment refusal controls at `:1213-1487` are unchanged.

Recorded gates were cross-checked, not executed again:

- `8e99daae`: exact-tree CTest log confirms **251/251 PASS, 616.04 s**.
- `ec8d941b`: strict matrix **58/61 PASS**, supplemental **5/5 PASS**;
  the inherited UI argument-count diagnostic remains a strict failure. Public
  coverage remains **62/63 clean**, private disc separately PASS. No diagnostic
  is waived and the failed raw aggregate remains retained.
- `d7f04e75`: FULL_TRACE remains **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- `7a5df5ab` / `d766b5e4`: retained result hashes match all three recorded
  journey JSON files; their logs contain **67 / 67 / 68** step records. The
  rendered run retains **43** captures. First-destination and portal arrivals
  remain refused, and both variants end at the death/reload refusal.

All **11/11** entries in `work/r3-verified-gate-manifest.json` still match their
recorded byte counts and SHA-256 hashes. `git diff 6b066acb d766b5e4` contains
only this report. No fresh visual inspection or original-executable probe is
claimed by this review. Only this report is staged; no payloads, private input
identities/paths, logs or captures are committed.

## Final continuation checkpoints (2026-10-29)

Started clean at `ec8d941b` on the assigned branch. No native, Godot, fixture or
baseline edits. Completed gates are retained rather than rerun.

1. **FULL_TRACE verified unchanged:** **36,402 bytes**, SHA-256
   `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
   Read the exact-source strict-matrix output at
   `work/r3-verified-matrix/playable_wave_smoke_full_combat.log:18` and its PASS
   row in `results.json`; cross-checked `work/r3-verified-trace.json`.
   All eleven source/wrapper/helper/engine/extension entries still match
   `work/r3-verified-gate-manifest.json` byte counts and SHA-256 hashes.
   Only this report differs from that gate's `8e99daae` source tree.
   This is verification of the retained execution, not a new matrix run.

## Ordered continuation gates (2026-10-29)

Started clean at `8e99daae`; `git diff 6b066acb HEAD` contains report-only
changes. The retained exact-tree CTest is **251/251 PASS, 616.04 s** in
`work/r3-verified-ctest.log`; no native code changed and CTest was not rerun.
The extension dry-run schedules no compilation/linking. Source, wrapper, helper,
engine and extension hashes are retained in ignored
`work/r3-verified-gate-manifest.json`.

1. **Strict Godot dev matrix recorded:** raw **58/61 PASS**, exit 1,
   125.77 s summed case time (`work/r3-verified-matrix/results.json`).
   Failures are the private-disc argument prerequisite, native-FX output-directory
   prerequisite, and inherited `ui_layout_smoke.gd:84` diagnostic:
   `hud_text_glyphs` expects five arguments but receives four through `callv`.
   This remains a strict failure despite engine exit zero; no filtering or
   allowlist changes. Explicit supplemental setup/rendered runs are **5/5 PASS**,
   6.01 s (`work/r3-verified-additional/results.json`), checked from every row
   rather than the helper's exit code. Public coverage is **62/63 clean**;
   private disc separately PASS. Captured stdout/stderr and separate engine logs
   were inspected; the inherited UI diagnostic remains unresolved.
   The three creature-driver cases use their existing fixed scratch project;
   other matrix cases use copied projects with fresh per-case user data. All
   cases ran serially. Native-FX capture was separately retained and checked
   (save result zero, nonempty PNG) in `work/r3-verified-fx-capture/` because the
   existing script ignores the capture-directory argument and its temporary
   output otherwise disappears. No source test or baseline was changed.

## Recovery disposition (2026-10-28)

Resumed at `7f87b053` with only this report, `native/src/loading_equipment.cpp`
and `native/tests/script_commands_tests.cpp` modified. Read their complete diff
before other work. Retained both code/test edits unchanged: rollback protects
the established once-only model relocation on validation failure, and authored
controls strengthen early-exit, traversal and refusal coverage without changing
any baseline. Research was read with `git show 2550d34:<path>`; the research
checkout's later HEAD and uncommitted round-4 work are outside this task.

The retained full run `work/r3-final-ctest.log` is **251/251 PASS, 868.43 s**,
with no timeouts. Both changed source files at `6b066acb` match
`work/r3-retained-source-sha256.json`; their timestamps precede the run. However,
that two-file manifest was written after the run, and is not a contemporaneous
whole-tree/build-input manifest. The previous no-op build and timestamps support
consistency but do not prove exact tested-tree identity. This supersedes the
previous report's stronger attribution. The 868.43-second run remains retained
historical evidence and is not the final exact-tree gate.

### Current continuation checkpoint (2026-10-28)

Started clean at `6b066acbe5b8639dbbbae488237fb6f33a077f2c`. Reviewed the recovery
code against `2550d34:docs/loading-arrival-r3.md:11-55` and the PASS research audit.
Rollback is a client transaction restriction preserving once-only relocation,
not a claim about an original rollback branch. Existing authored false/throwing
validator controls exercise refusal, byte restoration, retry and matching rescan.

**Exact-tree native CTest: DONE — 251/251 passed, total 616.04 s** (log
`work/r3-verified-ctest.log`, started 2026-10-28 19:40, finished 19:50). The run
was started by a continuation job that was then cut off by a provider limit; the
orchestrator recorded the finished result. Tree identity: clean working tree at
`333fcfe8`, whose code is identical to `6b066acb` (`git diff 6b066acb 333fcfe8`
touches only this report). `make -n` on `build/native` issues no compile or link
commands, so the tested binaries were current with the committed sources. This
supersedes citing the earlier working-tree run (`work/r3-final-ctest.log`).
Remaining: Godot dev matrix, headless/rendered journeys vs 67/68, FULL_TRACE.

## Round-3 test review and repair

Started clean on `codex/play-loading-readiness` at
`7f87b05367772113a1df3bd1e1086fabdf284eee`. Reviewed implementation `4ba375b5`,
citation correction `83f5dff5`, recovery record `00eff96a`, and the retained full
CTest record. All edits and execution are owned by this worktree.

Research pin: `research-loading-readiness@2550d3473dafc3609bbecbed2ec365bf7a658c60`.
The read-only round-3 research audit is PASS. Research was inspected at that
commit; original-executable probes and private resource sweeps were not rerun.

The code's original fields and branches trace to the following pinned evidence:

- `docs/loading-arrival-r3.md:11-16`: first-resource nonzero word `+0x1c`
  returns unchanged; otherwise relocate word `+0x10`, write short `+0xa = 0xa7fb`
  and guard word `+0x1c = -1`.
- `docs/loading-arrival-r3.md:18-20`: unsigned flags at short `+8`; bit zero set
  or bit `0x100` clear returns after header work.
- `docs/loading-arrival-r3.md:22-29`: relocate the table's first word, read signed
  inclusive rectangle endpoints, stop at first byte `-1`, relocate each cell,
  accumulate from `0x48` with six plus signed `(rows * 0x800 * columns) / 128`
  truncated toward zero, then store the low halfword at `+6`.
- `docs/loading-arrival-r3.md:31-40`: second resource uses its own base; signed
  count byte `+0x26 <= 0` becomes one; signed width byte `+0x12` plus one selects
  `max(0, count * width)` consecutive words from `+0x28`. This transformation has
  no relocation guard and must not run twice on the same published pair.
- `docs/loading-arrival-r3.md:49-55`: first then second fixup before cache
  identity/valid/outstanding publication, zero outputs on completion, and
  the next matching lookup returns the pair without another transformation.

Usable-resource validation, publication/bounds/overflow checks and the repair below are client restrictions,
not invented original branch conditions. Round-2 request-relative timing remains:
published request/current frames, elapsed at least four, with missing or backwards
frame state refused. Explicit paired adapter overrides remain available; an absent
validator or half-bound override refuses. No default successful validator exists.

### PLR3-1 — MEDIUM, repaired

Both parent inspection and the independent first review found the same defect:
validator false/exception left transformed resource bytes while the cache stayed
invalid and outstanding. The Host scanner catches the refusal, so a later lookup
relocated the model again. An executed, authored Host-runtime probe reproduced
initial word `0x20` at model base `0x180200` becoming `0x180220`, then `0x300420`.
Evidence: `work/r3-test-fix-before.log`; the probe and its bytes remain ignored.

Built-in fixups now snapshot both preflighted write plans before applying either.
If validation rejects or throws, those resource bytes are restored and the refusal
propagates. The same probe now retains word `0x20`, guard zero and invalid cache
after both attempts (`work/r3-test-fix-after.log`). Successful validation still
publishes once; next-match output behavior is unchanged. This enforces the
once-only rescan requirement at `2550d34:docs/loading-arrival-r3.md:31-37,49-55`.

Rollback is deliberately bounded: it restores planned resource bytes, not arbitrary
validator side effects. Transformation/restoration records remain in `Memory::writes`;
all restored fields were already published. Explicit adapter callback behavior is
unchanged. Preflight refusals, which apply no writes, preserve write history exactly.

Existing 252 paired resource controls and four incomplete/overflow controls remain.
Added controls drive the same real Host-owned loading Runtime with authored bytes:

- Three table shapes: immediate terminator, two rectangles and 3,000 rectangles
  exercising low-halfword accumulator truncation; whole-buffer comparison and
  unchanged next-match rescan.
- Six sparse-field early-exit controls: guarded texture, all three header-only
  flag routes, and model widths below `-1`; unused downstream fields stay unpublished.
- False-returning and throwing validators: two refusals each, whole-resource byte
  restoration and pending-cache invariance, then successful publication and a
  matching rescan without double relocation.
- Ten malformed traversal controls: absent table/terminator, truncated rectangle/
  cell data, table/cell relocation overflow, table address-space overflow, and
  missing/overflowing later model words after earlier writes have been planned.
  Together with the four existing cases, all 14 assert unchanged write history.

Independent second review: PASS with the rollback limits above; no remaining
correctness finding. It inspected source only and did not execute the gates.
No existing assertion, fixture, baseline, diagnostic allowlist or test was weakened.

## Final revised-code execution

The retained pre-repair CTest log was verified: **251/251 PASS, 675.65 s**,
`work/r3-resume-ctest.log`, recorded by `7f87b053`. That result is historical for
this repair. Native code changed for PLR3-1; the earlier repaired-code run took
868.43 seconds. The final exact-tree gate is the 616.04-second run recorded above.

- Release native configure/build: PASS, `-j 4`; final focused `script-commands`:
  **1/1 PASS, 0.89 s**. Logs: `work/r3-test-fix-configure.log`,
  `work/r3-final-native-build.log`, `work/r3-final-focused.log`.
- Release extension configure/build: PASS, `-j 4`, including the repaired code.
  Logs: `work/r3-test-extension-configure.log`, `work/r3-final-extension-build.log`.
- Full CTest on the retained repaired code: **251/251 PASS, 868.43 s**;
  `work/r3-final-ctest.log`; no timeouts.
- Fresh strict isolated Godot matrix: **58/61 PASS**, exit 1, 166.95 s.
  Three failures: private disc case missing its required input argument,
  native-FX capture missing the isolated relative output directory, and the
  inherited UI diagnostic. Evidence: `work/r3-final-matrix/results.json`.
  Explicit supplemental runs supplied the two setup prerequisites and ran rendered
  identity/death/loading-present cases: **5/5 PASS**, 9.57 s, with fresh per-case
  user data (`work/r3-final-additional/results.json`). Final public coverage:
  **62/63 clean**; private loading-disc gate separately PASS. The raw failed
  aggregate is retained. No diagnostic was suppressed or test changed.
- Fresh headless journey: DONE — 67 steps (matches the 67-step headless baseline), at
  `d7f04e75` (code identical to `6b066acb`); output `work/r3-completion-playthrough/headless/`,
  result.json SHA-256 029d97c8212c46fb91e60f1466d692d5d69617bc9706c599df09255eae5dcb86;
  godot.log has 67 PLAYTHROUGH_STEP lines. Arrival still refuses on the normal route:
  step 37 first-destination arrival = refused (steps 36-41 marker records, arrival, level change,
  initial choice state, record census and objective all refused); step 55 `portal_arrival`
  = refused_expected ("Gate travel refused: campaign hub/GateList lifecycle, paused menu
  task, and destination constructor seed/blocking grid are unavailable."). Last step 67
  `player_death` = blocked_death_reload. The run was started by a job that was cut off by a
  provider limit; the orchestrator recorded the finished result.
- Fresh rendered journey: DONE — a complete wrapper run (tools/run_loop_playthrough.py)
  at `7a5df5ab` (code identical to `6b066acb`) wrote both variants to
  `work/r3-rendered-completion-playthrough/`: headless 67 steps (exit 1, 6 refusal kinds,
  result.json SHA-256 9da1861bb2b5d034da4829b93e9e5ad07ef90e17a2eedfb275cca6778af92a36) and
  rendered 68 steps (macOS renderer, 43 screenshots, exit 1, 6 refusal kinds, result.json
  SHA-256 958cfad7c910b8a440b7457c736b25179a36b761df6ff3ca3571c4b4b00091db) — both match the
  67/68 baseline. Private-input file count 994 before and after. Rendered arrival still
  refuses at step 38 first-destination arrival = refused and step 56 `portal_arrival` = refused_expected
  (same gate-travel refusal text as headless); last step 68 `player_death` =
  blocked_death_reload. The wrapper was started by a job that was cut off by a provider
  limit; the orchestrator recorded the finished result.
- Fresh FULL_TRACE: **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  unchanged. FULL_SAVE_LOAD: **300/300**, identical continued/uninterrupted
  presentation hashes. Full-combat fixture: **2,641 checks / zero failures**.
  Evidence: `work/r3-final-matrix/playable_wave_smoke_full_combat.log:17-21`.

The unchanged journey wrapper is `tools/run_loop_playthrough.py`. An ignored
launcher supplies a fresh copied project and fresh `user://` per process using
the existing isolation helper; the source project/settings and user saves are
not modified. Gate helpers and raw logs/captures remain ignored. Preliminary
`work/r3-test-*` Godot/journey runs overlap the repair and are not final-code evidence.

Inspected the fresh rendered loading and destination captures. The loading-labelled
capture still shows the preceding live view; the destination shows a stopped session
with disabled Resume. This does not establish a playable handoff, live equipment
models or loading-art parity. Images remain ignored/private and are not committed.

## Remaining acceptance boundaries

`2550d34:docs/loading-arrival-r3.md:65-69` leaves actual starting-item generation,
slot/header/resource mapping, formatter/metadata/media transfer and renderer
validation unknown. `:93-106` leaves the normal-route deferred queues, ordered
commands and native-call census unknown. Generic transformations establish neither.
The runtime retains explicit refusals; no empty inventory, arbitrary passing
resource, successful unsupported native, offline I/O completion or ready-bit
fallback replaces these missing inputs. Full campaign arrival is not established.

The inherited `ui_layout_smoke.gd:84` four-argument `hud_text_glyphs` call is also
present at `3278e618`. Its engine diagnostic is a strict gate failure even when
Godot exits zero; it must not be suppressed or counted clean.

No game payloads, private payload identities/paths, captures or raw logs are staged.
No research files, other worktrees, credentials, baselines or durable memories are
changed. No push, PR or process-name termination is performed.

## Retained implementation checkpoints

`4ba375b5` implemented the two researched transformations and the initial runtime
fixtures. `83f5dff5` retained and clarified the pinned citations. `00eff96a`
recorded recovery and pending gates. `7f87b053` recorded the pre-repair full CTest
result cited above. Their earlier pending notes are superseded by this report;
the repaired revision requires the fresh execution recorded here.
