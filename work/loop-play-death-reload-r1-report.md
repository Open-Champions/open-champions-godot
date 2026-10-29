STATUS: IN PROGRESS

Assigned branch `codex/play-death-reload`, base `76c5b35e`.
Research verified at `0e3afdb8ce3cdf79746f535d8a2654db37c9e97b`,
`research-death-reload/docs/death-reload.md:12-62,75-97`; audit PASS.

Full CTest: PASS, 258/258 (881.13 s), recorded in `8748f4fc` after
the native fixes through `435e1740`. The test continuation does not rerun it
unless native code changes. No acceptance gate or fixture changed.
Automatic reload restores the checkpoint buffer, not a card slot or a guessed
spawn. Restart is the separate current-level act-lookup -1 branch. Whole-world
Creature/portal effects and continuous death reload remain unknown and refuse.

Inherited runtime review: b1753453 retained. It uses the actual death Flow and
save loader, compares the whole restored/current record, retains literal != -1
branching and refuses missing camera before player/request mutation. Class
reconstruction is being reviewed separately. Campaign reload now invokes that
loader on shared memory when an owner explicitly supplies a valid checkpoint;
there is no checkpoint synthesized from the dying world. After a request it
stops before another old-world tick because world reconstruction is Unknown.
Fresh acceptance results are recorded below separately from this bounded proof.

Review follow-up: DR-1 (missing Prop resource-name copies) and DR-2
(post-request old-world HUD gold credit) were fixed. Prop restores the three
bounded name fields; the shared World now stops immediately after a death
request, before HUD/frame/alive-list effects, and refuses continued ticking.
A real World/Flow/save regression retains pending credit 8 without adding 1 to
the restored record. A speculative clear of nonserialized Creature charm was
removed: nonzero constructor charm without the optional word now refuses.
Focused tests: ps2-save, ps2-class-restore, player-death-flow 3/3 PASS.
The full CTest after these source fixes passed as recorded below.

Full CTest (recorded by the orchestrator, 2026-10-28 ~00:10, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` on 435e1740 — **258/258 passed, 881.13 s**, no timeouts (log `work/death-reload/ctest-r1.log`). Strict Godot matrix, FULL_SAVE_LOAD, death probe, FULL_TRACE and both journeys for this revision are still PENDING.

## Test continuation, 2026-10-28

Started from clean `8748f4fca1897167c26bd515f98acde815e8ef3f` on the assigned
branch; ancestry to `76c5b35e` verified. The research checkout has advanced, so
the contract was read with `git show 0e3afdb8:docs/death-reload.md`, retaining
the audited pin rather than adopting later research. The named audit is PASS.
No runtime, test, fixture or baseline edits have been made in this continuation.
All raw logs/captures remain ignored under `work/death-reload/gates-r1-final/`.

Three Release-optimized debug-ABI Godot extensions were rebuilt with `-j 3`.
The strict dev matrix was launched with `--captures`, normal per-case isolation,
and the unchanged diagnostic checker. Its completed result was recovered below;
remaining private gates run serially.

The historical `ui_layout_smoke` four-argument diagnostic is already fixed in
the inherited `d5001c5f`: `godot/tests/ui_layout_smoke.gd:89-97` checks the
five-argument signature and absence of defaults without making an invalid
four-argument call. That file is unchanged from the integration base. No
allow-list entry or diagnostic exemption is introduced here.

### Gate 1: strict Godot development matrix — PASS

Recovered the completed continuation run from
`work/death-reload/gates-r1-final/matrix/results.json`: **54/54 PASS, 0 FAIL**,
including graphical capture cases. The matrix finished after all three extension
build logs reached their successful target completion. This is retained execution
evidence from the interrupted job on the unchanged native implementation through
`435e1740`, freshly inspected here; the matrix was not redundantly rerun.
A fresh invocation of the unchanged `godot_log.error_lines` checker across all
105 matrix logs found **0 diagnostic errors**. Completion markers and per-case
results remain available in the ignored logs. `ui_layout_smoke` is PASS; its
historical four-argument diagnostic does not recur because the inherited
`d5001c5f` fix is already present. Its script is unchanged against `76c5b35e`.
No source, baseline, fixture, allow-list or test was changed.

### Gate 2: FULL_SAVE_LOAD — PASS

The completed final matrix's `playable_wave_smoke_full_combat.log` records
**300/300 presents**, with identical uninterrupted and continued presentation
SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`.
Fresh structured extraction confirmed equality and the unchanged strict log
checker accepted the complete log. The driver completed **2,645 checks, 0
failures**, at 640 target ready ticks. This is synthetic full-combat save/load
continuation; it does not establish a retail death checkpoint or world reload.
The ordinary ObjectDB shutdown warning remains visible and is not an engine
error under the unchanged checker. Evidence is retained execution from Gate 1.

### Gate 3: FULL_TRACE — PASS, unchanged

The same completed strict matrix run records **36,402 bytes**, SHA-256
`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, renderer
`headless`. Fresh structured extraction matches both the exact requested size
and hash. No trace/baseline explanation or adjustment is needed. This retained
synthetic determinism gate is separate from the private campaign journeys.

### Gate 4: native death probe — PASS for bounded death/refusal

Fresh `build/native/champions-campaign-fight-probe --game <private-input>
--death-check --attack-every 0`: exit **0**, 600 ticks, 78 actors, no fight failure.
The actual diagnostic damage route produced dead state and zero HP; both timers
reached 0; task remained running in phase 1 with fade 128. Native clip started
and advanced 239 steps. XP/mana/Gold/bag/equipment comparisons passed, the same
dead body remained, and `reload_refused=1`. Log and exit metadata:
`work/death-reload/gates-r1-final/native-death-probe{.log,-result.json}`.
This passes the existing refusal probe, not a playable reload gate.

An initial invocation included `--counts-only`; that flag selects a separate
early-return diagnostic and did not execute the death check. Its exit 0 is
excluded from death evidence. The corrected command above has the actual
`death_check` completion line; no code or test change was necessary.

Root cause of the retained reload refusal: normal campaign initialization does
not call `Fight::bind_death_checkpoint`; its reload callback refuses before the
strict loader when `death_restore_bound` is false. Manual native save companions
are not the original checkpoint buffer. The audited contract at
`research-death-reload/docs/death-reload.md:23-42,91-97` (`0e3afdb8`) requires
restoring the real saved state and explicitly leaves real-level checkpoint
creation/restore and continuous recovery Unknown. No snapshot or spawn fallback
was fabricated to satisfy the gate.
