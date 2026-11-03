STATUS: PARTIAL — bounded gates PASS; continuous playable death reload REFUSED.

Test continuation completed 2026-10-29. All requested gate outcomes are recorded;
no in-scope runtime bug was found. This continuation changes only this report.

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
reconstruction remains bounded by the pinned research. Campaign reload invokes that
loader on shared memory when an owner explicitly supplies a valid checkpoint;
there is no checkpoint synthesized from the dying world. After a request it
stops before another old-world tick because world reconstruction is Unknown.
Gate outcomes are recorded below separately from this bounded proof.

Review follow-up: DR-1 (missing Prop resource-name copies) and DR-2
(post-request old-world HUD gold credit) were fixed. Prop restores the three
bounded name fields; the shared World now stops immediately after a death
request, before HUD/frame/alive-list effects, and refuses continued ticking.
A real World/Flow/save regression retains pending credit 8 without adding 1 to
the restored record. A speculative clear of nonserialized Creature charm was
removed: nonzero constructor charm without the optional word now refuses.
Focused tests: ps2-save, ps2-class-restore, player-death-flow 3/3 PASS.
The full CTest after these source fixes passed as recorded below.

Full CTest (recorded by the orchestrator, 2026-10-28 ~00:10, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` on 435e1740 — **258/258 passed, 881.13 s**, no timeouts (log `work/death-reload/ctest-r1.log`). The other gates were pending at that checkpoint; their completed outcomes follow below.

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
not call `Fight::bind_death_checkpoint`. The callback calls
`ps2_save::reload_checkpoint` (`native/src/campaign_fight.cpp:1062-1071`), whose
empty-buffer guard refuses before `load_impl` (`native/src/ps2_save.cpp:835-840`).
There is no `death_restore_bound` flag at this revision; this corrects the earlier
report's mechanism, without changing the recorded probe outcome. Manual native
save companions are not the original checkpoint buffer. The audited contract at
`research-death-reload/docs/death-reload.md:23-42,91-97` (`0e3afdb8`) requires
restoring the real saved state and explicitly leaves real-level checkpoint
creation/restore and continuous recovery Unknown. No snapshot or spawn fallback
was fabricated to satisfy the gate.

### Gate 5: completed campaign journeys — retained refusal evidence

Fresh inspection on 2026-10-29 of the completed `tools/run_loop_playthrough.py`
outputs under `work/death-reload/gates-r1-final/playthrough/` confirms:

- `headless/result.json`: **80 steps**; last step `player_death`, result
  `refused`, `saved_level_reloaded=false`. `loot_pickup` passed with inventory
  **10 → 11**, `picked_up=true`, `drop_produced=true`, `walked=true`,
  `walk_ticks=52`, zero retry kills and zero diagnostic retry placements.
- `rendered/result.json`: **81 steps**, 38 recorded screenshots; last step
  `player_death`, result `refused`, `saved_level_reloaded=false`.
  `loot_pickup` passed with inventory **10 → 11**, `picked_up=true`,
  `drop_produced=true`, `walked=true`, `walk_ticks=52`, zero retry kills and zero
  diagnostic retry placements.

Both runs record `rendered_pickup_verified=false`; the inventory transition is
verified state evidence, not a claim of rendered pickup verification. Both
`pickup_save_preflight` steps are `refused_as_expected` with existing files
preserved. These are retained completed executions, not new journey runs.
The step counts equal the user-supplied wave-36 comparison (80/81); that
comparison was not independently rerun. The ending refusal is recorded without
claiming a successful saved-level reload. Its pinned-contract assessment follows
in the final continuation section.

## Final continuation assessment, 2026-10-29

**Verdict: intended remaining refusal, not an in-scope runtime bug.** The precise
journey stop is earlier than the bounded native death probe:

1. Both journey results record `in_game_save=refused_as_expected` and
   `dead_state_refused=true`. The save guard refuses dead built-Creature state
   (`native/src/script_commands_objects.cpp:119-139`); the scene reports that
   refusal before writing the save (`godot/scenes/real_data_play.gd:409-414`).
2. Both record `load_and_carry=unavailable_save_refused`, `load_attempted=false`,
   and `cold_load=refused_as_expected`, `loaded=false`. No journey save or its
   five companion files exists in either output directory.
3. `godot/tests/loop_playthrough.gd:1957-1958` requires that saved-level load
   before entering the diagnostic lethal-hit sequence. Its failure takes
   lines 2000-2001 and records `player_death=refused`,
   `saved_level_reloaded=false`. Neither journey executes or verifies that
   lethal-hit sequence, the native reload callback, or a rendered death pose.
   The native probe in Gate 4 supplies the separate bounded death evidence.

At **`0e3afdb8`**, `research-death-reload/docs/death-reload.md:23-31` establishes
synthetic saved-record restore and a level request, with remaining callbacks
explicitly bounded. Lines **58-62** leave invalid/uninitialized checkpoints and
full level/object construction Unknown. Line **76** leaves complete Creature
motion, corpse tracks and world registration Unknown. Lines **91-97** expressly
leave real-level checkpoint creation/restore and continuous death-to-playable
recovery Unknown and require preserving `blocked_death_reload`. The named audit
is PASS and affirms that boundary. Removing the save guard or inventing a
checkpoint/fallback would exceed this research.

The actual campaign callback also retains that boundary: an explicitly bound
valid buffer can restore/request through the strict loader, but completed world
reconstruction and qualified-name matching refuse
(`native/src/campaign_fight.cpp:1076-1082`). After a successful request, further
old-world ticking stops at lines 1107-1115. This is not continuous recovery.
An independent read-only review at `6a8c03ab` reached the same verdict and
identified the report mechanism correction above; it ran no gates.

### Final gate accounting and provenance

- **Full CTest: PASS, 258/258, 881.13 s**, retained execution on `435e1740`,
  recorded by `8748f4fc`; final log summary inspected again.
- **Strict Godot matrix: PASS, 54/54**, recorded by `68de0217`; all 105 retained
  logs rechecked with unchanged `godot_log.error_lines`: **0 errors**.
- **FULL_TRACE: PASS, unchanged**, recorded by `4f74a440`; retained structured
  marker rechecked: **36,402 bytes**, exact SHA-256 recorded in Gate 3.
- **FULL_SAVE_LOAD: PASS**, recorded by `0e64eb9e`; retained marker rechecked:
  **300/300 presents**, equal hashes recorded in Gate 2; **2,645 checks,
  0 failures**. This remains synthetic continuation evidence.
- **Native death probe: PASS for bounded death/refusal**, recorded by `88dbd679`;
  retained exit metadata is **0**, and the actual `death_check` completion line
  confirms Gate 4's counts and `reload_refused=1`.
- **Headless journey: REFUSED/incomplete, exit 1, 80 steps**, pickup **10 → 11**.
- **Rendered journey: REFUSED/incomplete, exit 1, 81 steps**, pickup **10 → 11**,
  38 screenshots. Both final steps and reload flags are recorded in Gate 5.

For both journeys, `run-summary.json` confirms `result_written=true`,
`pickup_verified=true`, `log_errors=0`, and unchanged private-input file counts.
Fresh strict log inspection also found **0 errors** in each journey log. Every
`PLAYTHROUGH_STEP` and the final `PLAYTHROUGH_RESULT` exactly match the respective
`result.json`. Clean diagnostics do not make the refused journeys pass.
The retained rendered `07-save.png` visibly reports the Creature-state save
refusal; `09-loaded.png` shows the main menu, consistent with no successful load.

Result artifact SHA-256 values (raw outputs remain ignored):

- Headless: `1865f4eac2dc5e600bcf13bb418cc33cc6677f2b8e4c934fe92571912425cf5a`.
- Rendered: `8cc1ddbb666f459fd531b6f4f04a0e05ab597a50ecc9b880caaaef03d9a73f3f`.

### Remaining refusals and completion boundary

The journeys retain 27 headless / 28 rendered rows with the literal result
`refused`, including the final death prerequisite; they are not all death
regressions. Remaining categories include hub save/readback, portal scripts and
requests, mission-menu art/network/navigation, physical story travel and
destination/objective actions, level departure, object-state roundtrips and
revisit. Journal layout, normal combat/ability/construction coverage, and rendered
pickup verification also retain their existing limitations. Six refusal kinds
are recorded in each result's refusal map. No result, baseline or fixture was
changed to suppress these outcomes.

The requested test/report continuation is complete; playable reload remains
blocked on the documented reconstruction/checkpoint research above. The runtime,
Godot scripts, tools, tests, fixtures and baselines are unchanged from the tested
`435e1740` tree. No native change was needed, so no CTest, matrix, probe, trace,
save/load or journey execution was repeated. Fresh work here was evidence
inspection, source/contract comparison, log parsing and rendered capture review.
Only this report is staged with `git add -f`; private outputs stay ignored.
Journey evidence was committed first as `6a8c03ab`; this final report is a separate
local milestone on the same branch. No push or PR is part of this continuation.
