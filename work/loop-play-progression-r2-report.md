STATUS: PENDING — PPR2-1 repair in progress; fresh native/Godot/journey gates pending. Ordinary campaign progression remains refused.

2026-10-28 audit repair, starting from 0bbcf15b. The full Round-2 audit
reports one HIGH finding (PPR2-1) and no other findings. The previous menu
lifetime claim and cancellation test were unsupported and are withdrawn.
Fresh Release build -j 4, full CTest -j 3 (timeouts rerun alone), strict Godot
development matrix, FULL_SAVE_LOAD, FULL_TRACE and both standard journeys
are PENDING. Historical results below are retained evidence only.

## Fresh repair validation (2026-10-29)

Coordination: resumed only codex/play-progression at d77c01a9; inspected the
complete 0bbcf15b..HEAD history, clean status, report, and full PPR2-1 audit.
The one HIGH finding is addressed by d77c01a9's conservative menu-open latch
and explicit Unknown refusal. Its 256 production Host/campaign/save controls
are already committed; they do not claim a researched termination threshold.
The old cancellation-completion claim remains withdrawn in this report.

PENDING before this run: Release configure/build -j 4; focused production
script-command tests; full CTest -j 3 with any timeouts rerun alone; Release
GDExtension rebuild; strict development matrix including graphical cases;
FULL_SAVE_LOAD 300/300 and unchanged FULL_TRACE; standard headless/rendered
journeys compared against 80/81 steps and pickup inventory 10 to 11.
The known inherited ui_layout_smoke four-argument diagnostic will remain a
strict failure if reproduced; no test or diagnostic filter will be weakened.
Fresh evidence directory: work/progression-r2/ppr2-1/2026-10-29/.
Earlier logs remain retained evidence, not results of this run.

Fresh native preflight PASS: Release configure 1.17 s; build -j 4, 2.91 s;
focused dialog/script-commands selection 5/5, 1.58 s CTest wall time. Commands:
`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`,
`cmake --build build/native -j 4`, and
`ctest --test-dir build/native --output-on-failure -R '(dialog|script-commands)' -j 1`.
The selected production script-commands target includes all 256 menu refusal
controls. Evidence: configure.log, build.log, focused.log and native-preflight.json
in the fresh evidence directory. Full CTest -j 3 is now running; later gates
remain PENDING. No source/test change was needed after reviewing d77c01a9.

Full native CTest PASS: 253/253, total test time 610.92 s, command
`ctest --test-dir build/native --output-on-failure -j 3`, started 2026-10-29 01:41
on the clean tree at be126413 (code identical to d77c01a9); no timeouts. Log:
work/progression-r2/ppr2-1/2026-10-29/ctest.log. The job that started it was cut
off by a provider limit; the orchestrator recorded the finished result. Still
PENDING: Release GDExtension rebuild, strict development matrix, FULL_SAVE_LOAD
300/300, FULL_TRACE, standard headless/rendered journeys vs 80/81 (pickup 10 to 11).

Release GDExtension rebuild PASS on a8a27aef: all three existing development
extensions rebuilt with Release optimization and template_debug ABI. Commands:
`cmake --build build/godot-extension --config Release --parallel 3`, then the
same command for build/level-texture-debug and build/creature-anim-debug.
Elapsed: 2.87 s, 0.24 s and 0.54 s; all exit 0. Evidence: extension-build.json
and each build-directory-named log in the fresh evidence directory. No native
source changed; the completed 253/253 CTest result remains applicable. Strict
matrix, save/load, trace and journeys remain PENDING.

Strict Godot development matrix PASS on 170fa711: 54/54, including all six
graphical cases; 124.65 s wall time, 124.53 s summed case durations.
Command: `python3 tools/godot_matrix.py matrix --godot <local-engine> --captures
--out work/progression-r2/ppr2-1/2026-10-29/matrix`. Godot version:
4.5.2.stable.official.6ce3de25a. All 105 case/engine logs were additionally
checked by the unchanged strict classifier: zero errors. ObjectDB shutdown
warnings remain visible. The known inherited ui_layout_smoke four-argument
diagnostic did not reproduce: existing ancestor d5001c5f replaced that invalid
call with required-tint signature assertions (godot/tests/ui_layout_smoke.gd:84-91).
No filter, allow-list, test or source change was made in this continuation.
Evidence: matrix.log, matrix-run.json, matrix-verification.json and
matrix/results.json in the fresh evidence directory. Save/load, trace and
journey verification remain PENDING.

FULL_SAVE_LOAD PASS: the freshly executed strict matrix's Full-combat case
recorded 300/300 presentation frames and matching uninterrupted/continued
presentation hashes cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573.
The exact counts, both hashes and absence of a skipped presentation comparison
were checked after the matrix commit; the complete case log has zero classified
errors. Evidence: full-save-load.json and matrix/playable_wave_smoke_full_combat.log
in the fresh evidence directory. This is synthetic mid-fight continuation, not
real-level reload or campaign completion. Trace and journeys remain PENDING.

## PPR2-1 repair (2026-10-28)

Reviewed starting commit 7df45143 and the interrupted +14/-7 source edit
against the complete Round-2 audit (one HIGH finding; no other findings).
Kept 7df45143's pending status and withdrawal of the false lifetime claim.
Kept the interrupted implementation because it restores menu_open as a
conservative admission latch and explicitly refuses bound hub openings and
stale choices with `mission menu admission refused: run-task termination is Unknown`.
Neither countdown sign nor destination publication clears admission suppression.
This is an explicit Unknown boundary, not a claim that the original task runs forever.

Authority for the unchanged supported contracts is
research-mission-unlocks@950b2538: docs/spec/mission-unlocks.md:46-84
(predicate table, ordering, variable reads), :88-118 (first/second choices),
:120-125 (running-menu/recall suppressors); tools/difftest_mission_unlocks.py:175-178
(recall byte). The same pin's docs/quests.md:176-197 describes opening,
the observed countdown prefix and destination publication; :193,357-358
explicitly leaves cancellation to zero untraced. No termination timing is added.

Replaced the unsupported cancellation-completion test with 256 production-path
controls over first/second offers, both choices, opening/stale-choice admission,
and live/replacement Hosts. Cancellation checkpoints include 127, 63, 15,
-1, -17 and -145 (the existing port's decrement-only arithmetic, not new
original-executable evidence). Exact zero is a labelled authored imported save
control: subtracting 16 from 127 does not naturally hit zero. Launch controls
cover states 5/6/7/8 before, at and after publication; reopen controls preserve
the old destination in states 0/1/2 and save/restore both before and after reopen.
Every control asserts the explicit reason, no predicate dispatch, no create-zero
reads, and byte-identical campaign saves after the refused attempt. Existing
positive VM grants, all-node predicates, signed variables and recall controls stay.
No fixture, baseline, journey runner or admission threshold is weakened.

Fresh Release/focused/full CTest, extension, strict matrix, save/trace and journeys
remain PENDING at this implementation checkpoint. Evidence will be retained under
work/progression-r2/ppr2-1/ and results committed after each gate group.

## Historical continuation before PPR2-1

2026-10-27. Test continuation on codex/play-progression, clean at 5e0a5825.
Round 2 started from c12ecc31. Implementation commits: 559bb17f, c48f4f2c,
a1c2ab87. This continuation changes this report only; native code, tests,
baselines, fixtures and the standard journey runner remain unchanged.

Authority: research-mission-unlocks@950b2538, docs/spec/mission-unlocks.md;
cursor-runs/audits/research-mission-unlocks.md records VERDICT: PASS.
Round-1 audit PASS remains retained. Research files were read only; this test
continuation is not a new research audit or original-executable comparison.

## Implementation evidence

559bb17f implements all 31 normal-mode predicates and bounded choice admission
from live binder/save state. Resource identity uses the complete runtime
predicate/public-offset structure and runtime-resolved variable strings.
Physical actor/interaction binding is not inferred. First/second offer choices
retain the supplied native VM and production quest writer; later actions refuse.
Recall and mission-menu suppressors use live state, and the existing mission
menu consumes campaign availability.

c48f4f2c introduced an unsupported menu-lifetime proxy, now withdrawn by
PPR2-1. Neither a nonpositive state-4 counter nor a published destination
proves run-task termination. The former cancellation test incorrectly called
17 decrement-only ticks completion and expected an admitted second offer.
a1c2ab87 adds first-offer menu/save and stale-admission cases; those remain.

The authored tests independently specify truth-node sets for all 31 predicates,
raw bits 0..3 and signed variable values -128,-1,0,1,2,127. They include 160
saved/replacement-host opening/choice cases across five difficulty rows,
individual active variables, changed state after opening, mismatched resources,
unknown later actions, create-zero reads and mission-menu/save publication.

Retained private read-only probes: exactly one matching supplied resource and
zero invalid bindings/state controls; 40 first/second choice paths across five
rows, raw bytes 0/2 and both choices produced 40 grants and zero invalid/stalled
paths. These use supplied resources with synthetic speaker/service binding,
production VM/binder and campaign saves. They establish constructed-state
binding/action evidence, not physical reachability or played completion.
Evidence stays ignored under work/progression-r2/: binding-probe.cpp,
binding-probe.log and action-probe.log. No extracted names, dialog or game bytes
are included in this report.

## Retained gates, inspected in this continuation

- Release configure/build PASS; focused dialog/script-commands 2/2 PASS, 1.18 s
  before the menu correction, then 2/2 PASS, 1.80 s after it. The first full
  CTest was interrupted for that correction (verified CTest PID, SIGINT, exit
  130); ctest-interrupted-menu-review.log remains an incomplete run.
- Full CTest recorded by 339b5630: 253/253 PASS, 679.23 s, no timeouts.
  Command: ctest --test-dir build/native --output-on-failure -j 3.
  Evidence: work/progression-r2/ctest-resume.log. Not rerun here.
- Independent predicate/admission review recorded by 97ba319b: no native or
  fixture change required. Focused dialog/script-commands 2/2 PASS, 0.96 s;
  GDExtension build -j 3 PASS. Evidence: test-focused.log and test-extension.log.
- Strict Godot development matrix recorded by 5e0a5825: 54/54 PASS, including
  six graphical cases; 136.58 s summed case durations. A read-only review of
  all 54 case logs and 51 engine logs found zero classified diagnostic errors.
  ObjectDB shutdown warnings remain; clean diagnostic classification does not
  mean warning-free output. Evidence: test-matrix.log and test-matrix/results.json.
- FULL_SAVE_LOAD is already present in that same completed matrix: 300/300
  presentation frames with identical uninterrupted/continued hashes:
  cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573.
  This continuation parsed the marker, checked both counts/hashes and the log
  diagnostics, and matched the passing matrix result. It did not repeat the
  combined smoke, which would also rerun the already-completed FULL_TRACE gate.
- Full combat: 2,645 checks, zero failures. FULL_TRACE remains 36,402 bytes,
  SHA-256 9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659.
  Retained Round-1/Round-2 markers match. This is recorded trace evidence, not a
  new raw-trace rehash. Evidence for both trace and save/load:
  work/progression-r2/test-matrix/playable_wave_smoke_full_combat.log.

All short evidence names above are relative to work/progression-r2/.
The synthetic save/load and trace gates do not prove physical campaign
completion, a second offer following played completion, or real-level reload.

## Predicate/admission review retained from 97ba319b

Round-2 independent code review (2026-10-27, candidate 339b5630)

Historical review (superseded for menu lifetime by PPR2-1). Reviewed commits 559bb17f, c48f4f2c,
a1c2ab87 against audited research-mission-unlocks@950b2538. Authority references
below all mean that revision's docs/spec/mission-unlocks.md (path:line).
The 31 predicate expressions are bounded Inferred contracts backed by original
execution, not exhaustive imported-state or played-campaign proofs (:37-44).

- native/include/champions/hub_offers.hpp: all 31 entries match :55-79.
  Active uses player 0/current difficulty, availability bit 0, completion bit 1,
  twelve signed nonzero variables (:46-48). Duplicate predicates remain duplicate
  nodes. Offer tests are !availability; pending tests require available, incomplete,
  zero variable. Nodes 4/5/6 stay false in normal mode (:58-59,81-84).
  All twelve runtime variable offsets match :57,61,63,65,67,69,71-73,75,77,79.
  Signed controls derive from :39-40,120-125; absent reads create zero (:84).
  Node-zero fallback is original dialog selection (:49-51), never a fallback grant.
  File-order node priority does not establish mission order or adjacency.
- Structural binding checks the complete predicate/public-offset sequence and
  both second-offer action offsets (:53-79,107-112); strings resolve at runtime.
  The first-offer writer remains original supplied VM execution (:88-92).
  native/src/script_dialog.cpp revalidates current choice/admission/state, admits
  first traversal 1/2/3 (:88-92), second node 7 (:99-112), and node-8 close
  without a grant (:113). Later actions refuse. native/src/script_commands.cpp
  admits only setMissionAvailable(1,1) or (2,1) for the respective action.
  Opening never writes mission availability; completion and other difficulty rows remain preserved.
- Recall and active mission-menu suppressors use live state (:120-125); recall
  address is tools/difftest_mission_unlocks.py:175-178. The prior review
  incorrectly accepted the implementation as menu-lifetime authority. At
  research-mission-unlocks@950b2538, docs/quests.md:193,357-358 leaves the
  cancellation boundary untraced; :197 proves destination publication only.
  No termination or physical binding is inferred from these observations.
- native/tests/hub_offer_host_tests.hpp drives production quest records and actual
  campaign save/load, independently expected truth-node sets, signed suppressing
  cases for each variable, recall/menu controls, state changes after opening,
  mismatched binding refusal, later-action refusal, and absent-zero behavior.
  First and second grants surface as selectable production mission-menu rows and
  survive replacement-host save/load. Tests use authored synthetic resources;
  existing ignored private probes corroborate supplied-resource binding and both
  real VM action paths, without claiming physical interaction.

## Fresh standard journeys at 5e0a5825

The unchanged tools/run_loop_playthrough.py ran headless and rendered serially
with the pinned Godot 4.5.2 runtime and the existing read-only input. No option
changed the journey, combat, pickup or refusal expectations. Command (private
paths supplied only at runtime):

```sh
python3 tools/run_loop_playthrough.py --godot <local-engine> --game-folder <read-only-input> --out work/progression-r2/final-journey
```

| Gate | Headless | Rendered |
| --- | --- | --- |
| Journey steps | 80 (baseline 80) | 81 (baseline 81) |
| Process exit | 1 | 1 |
| Classified log errors | 0 | 0 |
| Result JSON written | yes | yes |
| Refusal kinds | 8 (baseline 6) | 8 (baseline 6) |
| Captures | 0 | 38 |
| Pickup assertion | PASS, 10 to 11 | PASS, 10 to 11 |
| Walked pickup | yes, 52 ticks | yes, 52 ticks |
| Retry kills / diagnostic pickup placements | 0 / 0 | 0 / 0 |
| Last step | 80: player_death, refused | 81: player_death, refused |
| saved_level_reloaded | false | false |

The wrapper exits 1; these are completed non-green journeys, not passing
campaign acceptance. Evidence: final-journey.log, final-journey/run-summary.json,
and each variant's result.json and godot.log, all under work/progression-r2/.
The input file census remains 994 before / 994 after. File counts are the
runner's read-only census, not a byte-for-byte content integrity measurement.

Offer result: both runs show the existing first offer path (three choices
close the dialog; one mission bit and one quest line are added). Mission 1
is available at step 24 in both, exactly as in Round 1. No newly reachable
second or later offer is demonstrated. First-act completion/return remains
branch_unverified (headless step 57, rendered step 58), with no completed
destination, queued return or loaded hub; the conditional post-return offer
path is not reached.

Ordinary progression stops at the hub portal: step 25 records
refused autoPropSetState, and the later hub-trigger attempt records
refused trigger. The mission menu remains closed with no host panel.
The first-story request is not queued (headless step 40, rendered step 41).
The unchanged harness continues through its diagnostic destination/fight
branches; that continuation and its successful pickup do not establish travel
through the ordinary mission route. The fight's own diagnostic placement is
separate from the pickup's zero diagnostic retry placements.

Both runs retain all six prior refusal kinds and add two reporting contexts
(dialog confirmation and tick collection) for the same explicit boundary:
hub actions beyond the documented first/second offers are Unknown. This is
not a new successful offer or evidence for later writers. The difference is
recorded without weakening the harness or changing any baseline.

Real-level save/load remains refused: hub savepoint readback is incomplete;
pickup inventory/equipment reload and creature death restoration remain
unsupported. Existing-file preservation checks pass. The load_and_carry row
has load_attempted=false, so unchanged experience/level/gold/HP fields are
not a successful load. Cold reload, object roundtrip and revisit remain refused,
and both final player_death rows have saved_level_reloaded=false. These are
separate from the passing synthetic FULL_SAVE_LOAD fixture.

Visual inspection used fresh rendered captures: 04-dialog-prompt.png shows
an open dialog; 10-mission-menu.png shows the hub view with the later-action
refusal and no mission panel; 09-loaded.png remains at the main menu.
No game text/names or captures are committed. The harness still reports
rendered_pickup_verified=false; the inventory assertion is not promoted to
independent visual pickup verification. No presentation change was made.

## Remaining refusals

First-act completion, physical reachability, the full campaign prerequisite
and travel graph, later availability writers including missions 8/9,
demo-specific behavior and unsupported host services remain Unknown/refused
(research authority, lines 129-142). Dialog node priority is not mission order.
Constructed-state offers and native save composition do not remove those gates.

## Historical disposition (before PPR2-1)

All requested gate results are recorded. CTest, predicate/admission review,
Godot matrix, FULL_TRACE and the already-completed FULL_SAVE_LOAD are retained
evidence inspected in this continuation, not claimed as fresh reruns. Both
standard journeys were freshly executed and compared against Round 1's 80/81
steps; neither establishes a newly reachable offer or completed campaign.
No native change required repeating the earlier suites.

The only staged file is this report (explicit git add -f). Final diff/whitespace
and content-boundary checks passed. Tests, baselines, fixtures and pickup
assertions were unchanged; private inputs and captures remain uncommitted.
No sibling checkout was modified, no process was killed, and no push or PR
was performed.
