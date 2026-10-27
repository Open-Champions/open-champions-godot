STATUS: MORE — implementation and acceptance gates pending.

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
