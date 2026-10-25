# Session animation timing and gameplay events

The separately opted-in [Session animation cache policy](SESSION_ANIM_CACHE.md)
connects the owned Runtime to ready tasks and complete raw32 joint observations.

The optional RTA Session adapter owns animation clip metadata and track state for
existing health-bound actors. An explicitly authored ready task starts, stops or
advances the original cache-free animation clock and can consume its signed event
result through an existing gameplay operation. This connects timing to retained
gameplay state without requiring presentation, decoded poses or retail assets.

## Composition policy

`rta-session-animation-v1` is an authored host policy. It admits operations only in
a ready task after `runObjects` has completed, with priority greater than 10 and
without nested World, HP or animation operations. It does not reproduce original
`Creature_stepAnim` placement inside derived object execution. A frozen World can
skip object execution while its explicit ready tasks still run. Delayed packets,
warmup, pending-level and other submissions that produce no ready tick do not
advance the clock or reset its per-tick counters.

The caller registers clips and binds existing HP identities before first submit.
Session copies each clip into a stable numbered registry: equal-valued clips are
distinct, while tracks referring to one ID share its four mutable event slots.
Runtime operations use raw32 timing and the existing caller-selected game FPU.
Clip ID zero is an explicit null start/stop argument; unknown nonzero IDs refuse.

The World remains the frame and object-random authority. Each accepted animation
operation reads the current unsigned32 World frame without writing it back.
This adapter makes no random draw. Animation randomization is not exposed until
one authoritative cosmetic stream can be bound; an unused zero seed is retained
in observations rather than creating a second live random authority.

Returned events are signed-byte values widened to integers, including negative
masks. The caller decides whether and how to consume them. There is no automatic
damage dispatch, sound execution or retry. The test policy explicitly applies a
null-attacker HP hit for selected event bits; this is authored composition, not
evidence of a retail attack or script schedule.

## Ownership, bounds and failure prefixes

Bindings retain the existing World serial, allocated Creature pool address and HP
adapter generation. Current registry ownership, alive state and allocation are
checked before clock mutation. Retired actors, reused identities and frames
outside unsigned32 refuse. Setup freezes at first submit. The defaults and hard
caps are 64 clips, 16 bindings, 512 stale track cells and 64 calls per ready tick;
each binding advances at most once in that tick. LOOP uses at most 65,536 original
subtraction stores per advance, retaining the standalone clock's executed prefix.

Complete snapshots copy clip metadata, all active and stale cells, binding
identities, limits, counters, last events, phase and operation history. Retained
snapshots cannot change when shared live events are later edited. Legacy trace
serializers refuse an active animation extension instead of silently omitting it.

Original clock failures retain partial stores and an incomplete operation record.
Later authored consumer failures retain the completed clock result and any HP
writes already executed. Session latches the failure; it does not roll back or
retry. Aggregate Session history and the adapter's cell/work bounds remain
explicit host policies.

ROOT/YAW, decoder and pose caches, pose output and sound execution are refused.
There is no Godot, controller, file-loader or animation-root integration in this
adapter. Existing controllable-scene visuals are unchanged. RTA evidence does not
validate Norrath or establish playable retail behavior or PS2 networking.

## Independent model contract

The generator executes immutable original Python bodies for clock arithmetic,
network readiness, World scheduling, pools, handles and HP. It uses the same
frozen Session-HP helpers and their checked hashes. The authored command DSL
submits clip metadata, ready-task programs and packet bytes; expected output does
not determine native execution. Complete initial observations and lossless deltas
retain every exposed subsystem and failure prefix.

The six model pins, spec and ordering evidence, helper hashes and composition
policy are recorded in `native/tests/vectors/session_animation.json`. The clock
uses `ref_anim.py` at `f516ac798bf28375554fe53ffb31b74802844136`
(SHA-256 `6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8`)
and the existing raw arithmetic contract. The executable chained-clock formula
is preserved; the previously reported prose discrepancy remains unchanged.

## Executed evidence

Executed local Apple Silicon evidence:

- All 320 cases match every original Python expected byte in optimized and
  ASan/UBSan replay. The corpus contains 20 scenarios across all 16 FPU settings.
  All 27 linked Session runtime units are sanitizer-instrumented; no leak-detection
  claim is made.
- Three independent generator processes reproduce both fixture files exactly.
  Seven atomic publication failure tests pass.
- All 450 API checks pass in optimized and sanitizer builds. They cover ownership,
  phase and ready-tick fences, shared and distinct clip identities, stale cells,
  failure prefixes, frame limits, history bounds and unchanged random ledgers.
- All 90 CTest gates pass in one full run (370.69 seconds), including the legacy
  exact replays and the 22-test animation transport suite. The unavailable Mac
  `/dev/full` test skips; the actual POSIX file-size-limit failure test passes.
- The Python suite passes 498 tests with 274 explicit native/platform skips.
  The integration subset passes 101 tests: 68 comparator, 30 boundary and three
  wrapper checks. Identical omitted state across all hosts still refuses.
- LLVM20 formatting checks pass all ten changed C++ files. GCC15 strict C++17
  syntax checks pass the runtime, API harness and final generic replay.
- The universal2 GDExtension builds and existing source scene/browser checks pass
  70/283 checks. There is no new animation presentation or visual claim.
- Independent runtime, replay and integration reviews found no admitted-domain
  blocker. All 184 unrelated backup hashes remain unchanged.

Expected trace SHA-256:
`5d661e2084da06cc727dc847050ccc292adadefc370f580db8cde166bc002b09`;
FNV-1a64: `069ac9cd98dbe1ec`. Local reports use the explicit
`local-uncommitted-session-animation-a460` label; they are not final-head CI
artifacts. The strict registry now has 34 suites and 1,425,963 records per host.
New exact-head native/Godot platform checks and actual artifact inspection remain
required before merge. Their pending state is not a completed platform claim.
