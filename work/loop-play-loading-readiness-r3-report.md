STATUS: MORE — interrupted PLR3-1 repair retained after pinned-research review; full native CTest verified for the retained code. Godot/journey evidence reconciliation in progress; normal-route arrival remains refused.

## Recovery disposition (2026-10-28)

Resumed at `7f87b053` with only this report, `native/src/loading_equipment.cpp`
and `native/tests/script_commands_tests.cpp` modified. Read their complete diff
before other work. Retained both code/test edits unchanged: rollback protects
the established once-only model relocation on validation failure, and authored
controls strengthen early-exit, traversal and refusal coverage without changing
any baseline. Research was read with `git show 2550d34:<path>`; the research
checkout's later HEAD and uncommitted round-4 work are outside this task.

The retained full run `work/r3-final-ctest.log` is **251/251 PASS, 868.43 s**,
with no timeouts. Both source SHA-256 digests match the retained source manifest;
implementation/test object and executable timestamps follow their last source
edits and precede this run. A recovery `cmake --build build/native -j 3` passes
without compilation or linking. Only report text changes during recovery, so
this full CTest applies to the unchanged implementation/test content committed
here; the older 675.65-second run is not substituted for it. Evidence:
`work/r3-retained-source-sha256.json`, `work/r3-retained-full-LastTest.log`,
`work/r3-recovery-verified-noop-build.log`. A fresh independent source review is
running; existing Godot and journey results below are being reconciled.

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
  this repair. Native code changed for PLR3-1; the required fresh full run is
  the 868.43-second run verified above.

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
- Fresh headless/rendered journeys: PENDING; `work/r3-final-playthrough/`.
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
