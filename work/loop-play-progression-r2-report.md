STATUS: MORE — bounded round-2 implementation reviewed; campaign journey remains refused.

Final test-subagent evidence is appended below; earlier PENDING entries are historical checkpoints.

2026-10-27. Round 2 starts from c12ecc31 on codex/play-progression.
Authority: research-mission-unlocks@950b2538 docs/spec/mission-unlocks.md;
research audit PASS. Round-1 audit PASS retained.

Full CTest is PENDING; this note precedes its invocation. Release build,
Godot dev matrix, FULL_SAVE_LOAD, unchanged FULL_TRACE, and unchanged standard
headless/rendered journey gates are pending. No campaign completion, physical
reachability, later offer actions or full graph acceptance is claimed.

Candidate milestone (gates still pending): implemented the 31 normal-mode
predicates and bounded choice admission using live binder/save state, identified
by runtime dialog/public structure and runtime-resolved variable strings. No
physical binding inferred. Native original choice actions retain their own VM
and production quest writer; later actions refuse. Recall/menu inputs now track
live state. Menu rows consume resulting campaign availability as before.

Focused Release candidate: dialog + script-commands 2/2 PASS, 1.18 s.
Independent expected truth-node sets cover all 31 predicates, raw bits 0..3
and signed variable values -128,-1,0,1,2,127. 160 saved/replacement-host
opening/choice cases cover five existing campaign difficulty rows. Tests also
cover each active variable, changed state after opening, mismatched resources,
unknown later actions, create-zero reads and mission-menu/save publication.
A counts-only private read-only binding probe found exactly 1 matching supplied
resource and 0 invalid bindings/state controls. This is constructed-state
binding evidence, not physical reachability or actual played completion.

Menu-lifetime correction: Campaign's menu_open flag persists after cancellation;
the dialog admission bridge now uses the existing Godot menu's active-lifetime
condition (no pending destination, not completed state-4 fade). A production
cancel-and-reopen regression passes. Focused native 2/2 PASS, 1.80 s.
The first full CTest was intentionally interrupted during review of this edge
case (verified worktree CTest PID only, SIGINT, exit 130). It is not a completed
gate. Its log remains ctest-interrupted-menu-review.log. Full CTest is PENDING
again before the corrected candidate's invocation; no test/baseline was weakened.

Additional private read-only probe: actual supplied script and matched dialog,
synthetic speaker/service binding, 40 first/second choice paths across five
supported rows, both initial raw bytes 0/2 and both choices: 40 grants, zero
invalid/stalled paths. Production native VM and binder, with campaign save state.
Source/log are ignored under work/progression-r2/binding-probe.cpp and
action-probe.log. No physical NPC or first-act completion claim follows.

Resume at c48f4f2c: pinned research/audit reviewed; first-offer menu/save and
stale-admission regressions added. Full CTest PENDING before invocation.

Full CTest (recorded by the orchestrator, 2026-10-27 ~20:10, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` — **253/253 passed, 679.23 s**, no timeouts (log `work/progression-r2/ctest-resume.log`). Godot dev matrix, FULL_SAVE_LOAD, FULL_TRACE and both journeys for this revision are still PENDING.

Round-2 independent code review (2026-10-27, candidate 339b5630)

No native or fixture changes required. Reviewed commits 559bb17f, c48f4f2c,
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
  Opening never writes; completion and other difficulty rows remain preserved.
- Recall and active mission-menu suppressors use live state (:120-125); recall
  address is tools/difftest_mission_unlocks.py:175-178. Menu lifetime uses the
  existing production menu machine (research-quests@7efd3ce docs/quests.md,
  section 4), with cancel-and-reopen regression. No physical binding is inferred.
- native/tests/hub_offer_host_tests.hpp drives production quest records and actual
  campaign save/load, independently expected truth-node sets, signed suppressing
  cases for each variable, recall/menu controls, state changes after opening,
  mismatched binding refusal, later-action refusal, and absent-zero behavior.
  First and second grants surface as selectable production mission-menu rows and
  survive replacement-host save/load. Tests use authored synthetic resources;
  existing ignored private probes corroborate supplied-resource binding and both
  real VM action paths, without claiming physical interaction.

Verification boundary: full native CTest remains the completed 253/253, 679.23 s
recorded in 339b5630 (ctest-resume.log); intentionally not rerun because no native
code changed. Fresh focused dialog/script-commands: 2/2 PASS, 0.96 s
(test-focused.log). Fresh GDExtension build -j 3 PASS (test-extension.log).

Remaining refusals: first-act completion, actual physical reachability, complete
campaign graph, later availability writers including missions 8/9, demo-specific
behavior, and unsupported host services remain Unknown/refused (:129-142).
Constructed-state offers and native save composition do not prove those paths.
No tests, baselines, fixtures, game inputs or mission-order rules were changed.

Fresh synthetic Full combat: 2,645 checks, zero failures. FULL_SAVE_LOAD passes
300/300 with identical continued/uninterrupted presentation hashes. FULL_TRACE
is unchanged: 36,402 bytes, SHA-256
9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659.
Evidence: test-matrix/playable_wave_smoke_full_combat.log. These fixtures do not
prove physical campaign completion or a second offer after played completion.

Fresh strict Godot development matrix: 54/54 PASS, including six graphical
cases; command: python3 tools/godot_matrix.py matrix --godot <Godot> --captures
--out work/progression-r2/test-matrix. Logs pass diagnostic checks as well as
exit-status checks. Summary: test-matrix.log and test-matrix/results.json.
