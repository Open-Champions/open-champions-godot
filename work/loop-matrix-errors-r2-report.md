STATUS: COMPLETE

# Matrix errors: branch report and round 2 fixes

Scope: `codex/matrix-errors`, starting at `d5001c5f5919c7f0ea2d5c94272acc13b4f7c1b9`.
The assigned worktree was clean. The complete round 1 audit was read before edits.
This report also supplies the missing report for the original branch commit.

## Original branch change

Inspected `d5001c5f`: a shared stdout/stderr Godot diagnostic gate now protects
matrix, CI, packaging and private-driver commands, independently of exit status
and completion markers. Its existing fixture changes repair missing art fields,
inspect the registered five-argument UI API, stop dependent parsing after refused
texture export, and explicitly refuse headless pixel capture. No execution claim
from the original machine is assumed here; current gates are recorded below.

## Findings

- MATRIX-R1-1 addressed: anchored diagnostic envelopes accept only horizontal
  indentation, an optional numeric bracketed timestamp and ANSI SGR colours.
  SCRIPT ERROR, ERROR, USER ERROR, parser/compiler/shader markers, C++ annotations
  and assertion/null/bounds diagnostics still fail. No allow-list entry, scene
  exemption or category exemption was added. Regressions cover quoted messages,
  warning payloads, structured refusals containing diagnostic markers, and paths.
  Exact ERROR and SCRIPT ERROR samples were reproduced with Godot 4.5.2 using
  authored push_error/Nil-call probes; both occurred despite process exit zero.
- MATRIX-R1-2 addressed: each invocation owns a temporary copy of the project and
  import cache. Each case selects its own UUID-named Godot custom user directory
  through the staged override.cfg, independently of log placement. Nested creature
  checks receive a private generated project and the same explicit user-data
  setting for their editor and smoke subprocesses. Python context managers remove
  generated projects and user data on success or failure. Source project settings,
  application data and import caches are not modified. Fixed-name missing-file
  negative cases remain absent initially. Actual engine probes verified the exact
  OS.get_user_data_dir result, fresh missing files, fixture writes and cleanup in
  four runs (including two successive cases reusing one staged project).
  Regression tests exercise two invocations sharing --out and all three
  nested creature variants, including exception cleanup.
- MATRIX-R1-3 intentionally deferred as requested. At integration, reconcile the
  overlapping fixture fields to one declaration each, retain hud_view where used,
  and preserve the integration journey options alongside the diagnostic gate.
  No integration worktree was inspected or changed during this task.

## Additional fixture failure caught by the gate

The first full run passed 53/54 cases. Its only diagnostic was an engine ERROR
from the PNG resource saver:
`ERROR: Can't save PNG at path: <temporary project>/../work/native-fx-2-visual.png`.
Specifically, native_fx_visual tried to save to the hard-coded
res://../work/native-fx-2-visual.png path. The staged project's parent did not
contain that directory. The fixture ignored --capture-dir, printed the save
failure code, and still exited zero. This is a real save error exposed by the
new isolation and correctly rejected by the diagnostic gate, not a false positive.

The test now requires graphical rendering and an explicit --capture-dir, writes
its existing synthetic PNG there, and exits nonzero if saving fails. No rendered
values or production behavior changed. Focused editor + native_fx_visual rerun:
2/2 PASS; the saved 640x480 artifact was opened and inspected. The expected
synthetic occluder, sprites, line, trail and beam are present. The first run's
full logs and results remain under ignored work/matrix-r2-full/; the focused
rerun is under work/matrix-r2-capture-fix/.

## Verification

Platform: macOS arm64, official Godot 4.5.2 stable, OpenGL Compatibility renderer.
Official runtime ZIP SHA-512 matched the documented pin. godot-cpp was at the
pinned e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77 revision. All three GDExtensions
built successfully with Release optimization and template_debug ABI, at no more
than three compiler workers per build. Logs: work/build-extension.log,
work/build-level.log, work/build-creature.log.

- python3 -m unittest discover -s tools -p test_godot_matrix.py: 12/12 PASS.
- python3 -m unittest discover -s tools -p test_desktop_package.py: 27/27 PASS.
- Actual-engine isolation probes: 4/4 PASS, exact user directory, fresh missing
  save path, fixture write and cleanup checked (work/isolation-probe-*.log).
- Actual-engine diagnostic probe: exit zero with exactly two diagnostics caught:
  ERROR: synthetic diagnostic probe; SCRIPT ERROR: Invalid call. Nonexistent
  function 'missing' in base 'Nil'. These intentional synthetic errors are
  separate from the matrix results (work/real-diagnostic-probe.log).
- First full matrix, with --captures: 53/54 PASS, exactly one ERROR (PNG save
  failure above), no SCRIPT ERROR. No category/scene was waived.
- Focused fixture fix: 2/2 PASS, zero error diagnostics.
- Final full matrix (current continuation): 54/54 PASS, zero error diagnostics; exit 0.
- git diff --check: PASS.

The matrix command is python3 tools/godot_matrix.py matrix --godot
work/runtime/Godot.app/Contents/MacOS/Godot --out work/matrix-r2-final --captures.
It includes 48 editor/default/smoke/creature cases and six graphical capture
cases. ObjectDB shutdown WARNING lines remain visible; they are not ERROR
markers. Public synthetic checks establish tooling/fixture behavior on this
host, not retail gameplay, exports or cross-platform correctness.

## Boundaries

Only Python tooling, documentation and synthetic fixtures are eligible here.
No runtime or native source change, game-data import, private content, guessed
value, push or PR. Native Release build is not required by the conditional gate
because native source is unchanged; the Godot extension prerequisites are built
with Release optimization and template_debug ABI for this verification.


## Local commits and handoff

Implementation milestones: 163c6715 (anchoring and isolation), e1037295 (private
invocation cache), 37cdc2d5 (engine samples and all nested variants), 77f7f3eb
(capture fixture repair). This report is committed separately after final gates.
MATRIX-R1-3 is the sole intentionally deferred integration concern.

Coordination 2026-10-26: implementation remains on the assigned branch; no
sibling checkout, integration branch or remote was changed.

## Completion review (current continuation)

Re-read the complete audit, prior report, and all three interrupted-job commits.
The worktree was clean at 77f7f3eb. Inspection found both requested fixes complete;
no additional implementation change was needed. The classifier remains anchored,
with no allow-list entries or scene/category exemptions. Negative tests include
quoted diagnostics, diagnostic-bearing JSON refusals, warnings, and paths;
positive tests retain the actual Godot 4.5.2 push_error and Nil-call samples.
All three nested creature variants receive isolated generated projects and user
directories. Missing/malformed save checks remain in scene_play_flow_smoke.

Re-executed Python matrix tests: 12/12 PASS; packaging tests: 27/27 PASS.
The intentionally failed mocked matrix and diagnostic subprocesses are expected
unit-test assertions, not failures in the live matrix.
Two fresh actual-engine isolation probes: 2/2 PASS, verifying the exact user
directory, absent fixed-name fixture and missing-save paths, successful write,
and project/user-directory cleanup. Logs: work/isolation-final-{0,1}.log.
Earlier engine diagnostic/isolation probe logs were also inspected; their
historical results above are kept distinct from these current executions.

Final full matrix completed with exit 0: **54/54 PASS**, comprising 48
editor/default/smoke/creature cases and six graphical capture cases. All 105
retained engine/runner logs were scanned with the shared gate: zero diagnostics.
Thus there are no new engine/script errors to repair. scene_play_flow_smoke
reported SCENE_PLAY_FLOW_PASS checks=65 failures=0, retaining its missing-file
and malformed-file negative checks. native_fx_visual reported save result 0.
Its current PNG was opened and inspected: the synthetic occluder, coloured
sprites, line, trail, and beam are visible. Logs/results/captures remain under
ignored work/matrix-r2-final/. The earlier 53/54 run caught exactly the one PNG
save error documented above (duplicated in engine and runner logs), followed by
the 2/2 focused pass and this full pass; no errors were waived.

Native Release build: NOT REQUIRED, because no native source changed.
MATRIX-R1-3 remains deferred to integration, as requested. No runtime changes,
private inputs, sibling-worktree changes, pushes, or PRs. Final diff whitespace
check passed. The only continuation change is this completed report.
