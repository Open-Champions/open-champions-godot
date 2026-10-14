# Opt-in Session player recovery

`rta-session-recovery-v1` composes two **existing** player bodies with the owned
Session and bounded sandbox. Recovery is disabled by default. No additional
reference function was ported; port-player's existing **94/94** count is unchanged.

| Integration path | Existing body | Evidence |
|---|---|---|
| Health | `player::Tick::regen_health`, `Player_regenHealth` at `0x1b3b90` | Independent ready-frame traces, dead/copy gates, signed bonuses, passive levels, caps, pending delivery and a literal health failure prefix |
| Mana | `player::Tick::regen_mana`, `Player_regenMana` at `0x1b3900` | Independent ready-frame traces, class-0 query/gain behavior, health-before-mana gating, caps, pending delivery and a literal mana failure prefix |

This is an authored recovery composition, not full `Player_run`, original player
input, damage dispatch, potion inventory/use, active skills/spells or a playable
port. Original player bodies and corpora are unchanged. Norrath is unsupported.

## Evidence and independent oracle

The immutable research revision is
`356add27d50e50972b2a671b31e8f4ec93e6778c`. Git-object reads, rather than the
checkout's current HEAD, verified the hashes already recorded in PLAYER_TICK.md:

| Source | SHA-256 |
|---|---|
| `tools/ref_player.py` | `ee8750f778e520158243abfec0887633bd97d69b7274ae752dfa2531f297bc3d` |
| `tools/ref_core.py` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `docs/spec/player-tick.md` | `ba83f8672626cc5625ae108dd2838b187de06d8d37c54cdb57afae3cc71e3f37` |

The original Python contract is MIT, copyright 2026 TrustlessMike and contributors.
The existing player loader also verifies the pinned differential-harness source;
this job does not execute that harness or claim original executable validation.

`tools/generate_session_recovery_vectors.py` pins its authored dependencies and
composes the unchanged Python recovery bodies with the existing independently
pinned Session/HP/creature/control oracle. All source and helper identities are in
`native/tests/vectors/session_recovery.json`. The original network, World,
movement, HP and arithmetic bodies execute; only host setup, task placement,
HP projection and the explicitly closed services below are authored glue.
Expectations never come from C++ output. Every case runs twice from fresh state.
The final corpus has **400 cases**. Complete initial observations plus lossless
deltas retain all later states;
every generated delta is reapplied and checked before publication.

The corpus covers all 16 arithmetic configurations, repeated ready ticks,
positive/negative/zero/dead health, signed passive/bonus fields, signed/zero
pending amounts and rates, rate overshoot, caps, class-0 mana, copy gates,
health becoming zero before mana, movement/damage/recovery order, delayed input,
pause, release, reset and stop. Direct Session oracle cases additionally cover
pending levels, `no_sim`, health/mana memory-limit failures and refusal of a
latched retry, under all 16 arithmetic configurations. Native API checks separately
cover invalid setup, aliasing, generation/address reuse, duplicate calls,
resource failure prefixes, complete checkpoint continuation and rendering pace.
The body-prefix anchors are independently obtained from the pinned Python model:
health writes `0x42480a3d`, then attempted `0x424c0a3d`; mana's first attempted
write after the three health stores is `0x41a00a3d`. Native host failure/latch
semantics also have complete independent Python failure-state traces.

## Closed setup and ownership

`Session::enable_recovery(hp_id, Initial)` requires setup phase and one current,
live, isolated World allocation with room through actor offset `+0x477`.
The binding retains HP ID, World serial/address and Adapter generation. It
rechecks them before each call; a freed/reused address never binds implicitly.
There is one Session, World, handle pool, terrain Level and HP sparse Memory.
The player Tick borrows `hp.adapter.model.mem` and dies before that owner.

The supported helper domain is explicit:

- Slot 0 and its original record address `0x4d8af0`, extent `0x4c30`.
- Supplied class table indices 0 through 4. This is a conservative host domain,
  not a newly inferred count of game classes. Class, maxima, initial resources,
  pending amounts/rates and signed bonuses are caller-provided raw values.
- Exactly 48 supplied passive masks and signed level bytes. The original helper
  scans 48 rows; **the passive table is not sentinel-terminated**. Unused rows
  are openly initialized to zero. Negative levels clamp through the existing
  helper, without a skills-a/b/c dependency.
- A supplied empty rank list `(-1, 0)`, with the original `-1` terminator, and
  17 explicitly empty equipment slots. Nonempty/unterminated ranks or equipment
  refuse. Thus Regeneration-stat and item callbacks are provably unreachable.
- Named `NoActiveSpells`: the supplied nonzero Blessed type token is an opaque
  authored query result, never a pointer to constructed RTTI. The only accepted
  find-spell query is the configured actor/token pair and returns no spell.
  Blessed-stat, Regeneration-stat and other delegated services have no handler.
  The Tick uses `RequireHandlers`; `ReferenceZero` is never enabled.

`Initial::copy_at_setup` is an authored World spawn input for testing the
reference copy gate. Thereafter World flags remain authoritative. No original
prediction/copy construction is claimed. Pending amounts are supplied recovery
already awaiting delivery, not an implemented potion item/use action.

Setup checks all spans, backing/overlay aliases, existing HP/raw reservations,
World allocation arenas, managed handles, generations and sparse capacity before
publishing setup bytes. The record, class table and actor helper tail receive
named raw reservations. The helper inputs remain immutable and are checked on
entry and checkpoint restoration. Unsupported domains produce explicit errors.
Resource/allocation exceptions retain any completed prefix and latch Session
failure; there is no automatic repair or retry.

## Authority and ready-task order

| Boundary | Authoritative state and authored projection |
|---|---|
| Between calls | World owns flags, position and frame. Existing HP Adapter owns base-creature HP. Player record owns mana and pending amounts; descriptor owns immutable helper inputs/maxima/rates. |
| Entry | Existing HP import/project publishes shared scalars into the same Memory. Base HP is copied to player-record `+0xac`. Prior damage therefore cannot be overwritten by a stale player-record HP cache. |
| Body | `regen_health(actor)` runs first, then `regen_mana(actor)`. Raw player record is authoritative, including health read by mana's dead gate. Original writes/calls remain ordered and logged. |
| Success or body failure | Completed record HP at `record+0xac` is copied to actor `+0x228`, then the existing Adapter publishes typed/scalar state. Mana/pending prefixes stay in the same Memory. Projection uses unlogged pokes, as existing HP imports do; these are host cache transfers, not original player stores. |

The two HP fields are distinct storage. Their projection is authored policy and
is **not evidence of original Player damage dispatch**.

`recover_player` is allowed once per actual post-runObjects ready tick, in an
explicit task with priority greater than 10. The Controller's existing priority-20
task performs movement, consumes/applies the health-test damage edge, then runs
health and mana recovery. This authored placement lets recovery observe the
completed same-tick health test; only health-before-mana comes from `Tick::top`.
It does not reproduce the surrounding original Player task or damage dispatch.

No-ready input, pending levels and `no_sim` do not execute recovery. Frozen
runObjects can still return to an independently scheduled task, as in the
existing Session policy. Release drains old commands neutrally while subsequent
ready ticks can recover. Pause and stop submit no simulation. Reset creates the
same explicit setup afresh. Operational failure retains body prefixes and
publication diagnostics, latches the Session/Controller and refuses retry.

Complete observations retain policy, initial descriptor, binding, phase/depth,
per-tick counters, callback descriptors, ordered writes/calls/events, draws and
locals, plus the full surrounding Session, World, network, HP backing/store log
and terrain/movement histories. Fixed callback captures contain only immutable
actor/token values already represented by the descriptor/binding. There is no
mutable callback state outside snapshots. Unused Tick locals, named callbacks
and external-helper sets remain constructor constants behind the closed API;
neither recovery body mutates them. Legacy formatters refuse an active
recovery extension instead of silently omitting it; disabled traces are unchanged.

The closed Controller has a 256-ready-tick recovery cap. At most 14 recovery
stores (42 store/write/event history entries) plus two calls (four entries),
the existing 67-entry movement/damage/scheduler bound, and a conservative reserve
fit within 128 entries per tick. `256*128+128` fits the existing aggregate history
budget. No history is cleared to make this bound work. Legacy Isolated and
DryCreature limits remain 2000 and 512 when recovery is disabled.

## Native saves

Recovery writes **replay v2** (`OCNSAVE\0`) or **indexed snapshot v4**
(`OCNSNAP\0`). Existing replay v1 and snapshot v1/v2/v3 readers and byte layouts
are unchanged. A recovery snapshot request uses v4 through either snapshot entry
point; active recovery is never dropped into a legacy representation.

Replay v2 adds a counted, explicitly encoded Initial descriptor after the
existing max-ready-tick field, and appends mana/pending-health/pending-mana raw32
values to the continuation witness. Descriptor scalar fields use the existing
portable Checkpoint codec, in the declared `checkpoint_fields(Initial)` order.
There are no dumped structs, host callbacks, pointers or original save bytes.

Snapshot v4 retains the four-section indexed layout and constructor envelope,
with recovery phase/counters/locals and three player histories appended to the
Session checkpoint. Raw Memory is serialized once through HP. Restore uses the
same closed constructor, restores state directly and validates canonical values,
helper inputs, HP projection, histories, witnesses and section integrity.
**V4 validates and owns its complete bounded checkpoint before scene publication**,
without executing the action journal. This includes corrupt history/journal
sections. Legacy v3 still loads current state and continues actions without
reading retained histories; it is not forced through replay or v4 validation.

Header/version/profile, setup, current-state and historical validation failures
leave the existing bridge scene intact. Failed operational prefixes remain
observable but are deliberately not reloadable checkpoints, matching legacy
save policy. Paused state, input cutoff, queued damage, delayed network input,
pending amounts, immutable setup and all diagnostic histories survive valid
save/load/continue. Complete native observations are compared against uninterrupted
execution after every continued command, under all 16 arithmetic configurations.

## Sandbox and verification boundary

The existing sandbox has **Recovery test (opt in)** and **Recovery off / restart**
buttons. Enabling starts a fresh authored run on the selected initial terrain:
health 50/100, mana 20/50, pending health 10 at rate 1, pending mana 6 at rate 0.5,
class index 1, empty equipment/ranks/passive rows and NoActiveSpells. These numbers
are explicit test setup in `Initial`, not inferred statistics. Resource/pending
values and configured maxima are read-only native observations in the existing
diagnostic text, including after loading a different supported setup.

Movement, H health tests, terrain selection, fixed 60 Hz submissions and display
interpolation remain in place. Recovery never runs from a render callback.
The same tick-indexed schedule at 30/60/144/240 render FPS has matching complete
native authoritative states; Godot independently checks pacing observations.
Reset, opt-in/out, successful terrain/load publication and pause snap interpolation.
All recovery formulas stay in the existing native player bodies.

Public checks use only synthetic data. `../../private-validation` is unavailable;
private acceptance was skipped, without searching elsewhere. Windows, physical
Intel Mac, clean-machine installation, signing/notarization, original-game
playability and original-console interoperability are not established here.
Expansion requires separately specified equipment/statistics ownership, nonempty
rank/stat services, active spell services, inventory-use flows and original
player/damage/control dispatch contracts. No skills, monsters, messages, rendering
or research-repository changes are prerequisites for this closed test policy.

## Executed local verification

Verified 2026-10-14 on arm64 macOS 27.2 (26B5101f), Godot 4.5.2. X64 native
executables ran under Rosetta on this same Mac. Both native binaries were checked
with `file`; both Godot extension variants contain arm64 and x86_64 slices.

- Full Release CTest run: **132/132 passed**, 735.07 seconds. This includes the
  unchanged player, Session HP/control/creature/save and frame-rate regressions.
- After the final failure-corpus, checkpoint-validation and observation changes,
  the affected CTest run passed **20/20**, 303.72 seconds. The final recovery API
  executable passed **1,366 checks** on both architectures, with identical full
  continuation/failure-observation FNV1a64 `a83fc779c59995c3`.
- All **400** complete independent oracle cases matched on arm64 and x64.
  `cmp` found their actual traces byte-identical; both match expected SHA-256
  `5bff819778bd762c5ed62f73fe70318518941636af7300bcc2f8fe1d018734df`.
  The committed gzip is 2,551,333 bytes; expanded expectations are 93,694,084
  bytes, maximum line 263,147 bytes. Native execution receives only the 400
  small recipes, never expected observations. No histories are discarded.
- Simulation boundary: all 66 registered paths passed; **34** boundary tests
  passed. LLVM 20 formatting and `git diff --check` passed.
- Real editor-runtime and exported release-app recovery checks: **40 checks**,
  zero failures each, including opt-in dispatch, read-only values, pause/reset,
  both save formats, delayed continuation, corrupt/newer-save atomic refusal,
  interpolation reset, active recovery across terrain replacement and
  30/60/144/240 FPS observations. Existing exported
  sandbox: **124 checks**, zero failures; frame-rate smoke: zero failures.
- Actual exported GPU application: **23** visual-harness checks, zero failures,
  using macOS/OpenGL 4.1 Metal on Apple M5 Pro. Its own rendered viewport PNGs
  were inspected for disabled/enabled recovery, recovered/paused values, reset,
  loaded pending input and continued save state. Text is readable, controls fit
  and the existing ground/actor presentation remains intact. Captures and the
  synthetic checkpoint are under ignored `work/session-recovery/visual/`.
- A separate graphical exported startup ran for 120 frames and exited normally.
  Verbose engine shutdown reported unclaimed StringNames; this is not leak-test
  evidence. No application/script error was reported by the visual harness.

Desktop computer-use inspection initially failed with `cgWindowNotFound` (-10005)
and a subsequent tool timeout. The exported GPU harness supplied the rendered
application-state evidence instead. Manual desktop pointer/keyboard handling and
native file-picker interactions were not independently verified in this run.
Windows and physical Intel-Mac execution, private acceptance and sanitizers remain
unverified here. The existing synthetic browser/scene assets were used; no original
game assets or private files were accessed.

Commands run from this worktree (all final commands below passed):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 3 \
  -R '^(session-(recovery|control|hp|creature)|native-(save|snapshot)|frame-clock)'
python3 tools/generate_session_recovery_vectors.py \
  --research-dir ../../champions-return-to-arms-re
python3 tools/test_session_recovery_vectors.py \
  --exe build/native/champions-session-recovery-trace \
  --trace work/session-recovery/arm64-verified-trace.tsv
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-session-recovery-tests \
  champions-session-recovery-trace -j 4
arch -x86_64 build/native-x64/champions-session-recovery-tests
python3 tools/test_session_recovery_vectors.py \
  --exe build/native-x64/champions-session-recovery-trace \
  --trace work/session-recovery/x64-complete-trace.tsv
cmp work/session-recovery/arm64-verified-trace.tsv \
  work/session-recovery/x64-complete-trace.tsv
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
```

The sibling cache supplies the already pinned tools; no dependency or game-data
installation was added:

```sh
cmake -S gdextension -B build/recovery-godot-debug \
  -DGODOT_CPP_DIR="$PWD/../../worktrees/godot-health/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/recovery-godot-debug -j 4
cmake -S gdextension -B build/recovery-godot-release \
  -DGODOT_CPP_DIR="$PWD/../../worktrees/godot-health/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/recovery-godot-release -j 4
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --editor --quit-after 120 --frame-delay 50
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --script res://tests/session_recovery_smoke.gd
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --script res://tests/frame_rate_smoke.gd
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --script res://tests/session_sandbox_smoke.gd
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --export-release 'macOS universal'
ditto -xk build/exports/macos-universal/OpenChampions.zip build/recovery-export
'build/recovery-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --script res://tests/session_recovery_smoke.gd
'build/recovery-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --script res://tests/frame_rate_smoke.gd
'build/recovery-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --script res://tests/session_sandbox_smoke.gd
'build/recovery-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --verbose --quit-after 120
'build/recovery-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --script res://tests/session_recovery_visual.gd -- \
  --capture-dir="$PWD/work/session-recovery/visual"
```

Iteration-only failures were a header-only FrameClock link-target mistake,
a test snapshot-field typo, a GDScript inference error and a stale object from
overlapping early incremental builds. They were corrected before final evidence;
no failing runtime assertion remains. Raw local command outputs and architecture
traces are retained under ignored `work/session-recovery/`.

Changed-file groups: Session recovery policy/header/source and ready/checkpoint
hooks; Controller configuration/observations; native-save v2/v4 plumbing; thin
bridge and sandbox diagnostic controls; dedicated API/oracle/visual tests and
fixtures; minimal build/boundary registration; this guide and the two small
links in SESSION_CONTROL_CREATURE.md and NATIVE_SAVE.md.
