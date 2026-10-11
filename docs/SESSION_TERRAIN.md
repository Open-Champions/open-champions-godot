# Session-owned supplied terrain movement

The RTA Session now composes the verified terrain kernel with its existing object
World and ready-frame scheduler. An explicitly registered task supplies raw
velocity after runObjects, calls `move_isolated`, and observes the returned
position, vertical velocity and movement bits. Velocity is caller supplied;
this API does not infer the original game's player controls.

## Ownership and supported domain

`enable_terrain(data, limits)` deep-copies one supplied post-fixup Level during
setup. `bind_isolated_mover(serial, address)` selects one current owned World
allocation and returns a stable binding ID. Setup freezes at first submission.
No mutable Level pointer or caller-owned terrain state escapes through the API.

Before every move, the Session checks **exactly one live retained World actor**,
its current live registry identity and its allocated pool block. A second live
actor refuses even when stationary, hidden, far away, pending deletion or absent
from the grid. A reused address never silently selects a replacement generation.
The conservative isolation restriction is authored wrapper policy; the underlying
movement body remains the pinned Python model's no-neighbor subset.

Two network endpoints can each replicate the same sole actor. Network pads do
not require a GameObject for each peer. This domain supports genuine delayed
wire-frame tests while retaining the strict movement restriction.

## Ready ticks and partial state

Movement requires an actual ready task with priority greater than 10 after
runObjects returns. It runs at most once per ready tick, independently of
presentation submissions. Message callbacks, object callbacks, end-of-tick
callbacks and nested World access cannot use it. No-ready, pending and no-simulation
submissions do not invoke a movement task. A frozen runObjects that returns does
not itself suppress a separate authored post task.

The accepted call records its velocity and counter, executes the existing terrain
kernel, then assigns the actual GameObject position and vertical velocity. It
relinks the existing grid node through World.update_grid_cell, including same-cell
and off-grid transitions. Relinking allocates no handles; an exhausted free pool
is a valid movement case when the existing node is intact.

A query refusal retains completed terrain stamps while the actor position remains
unchanged. A grid refusal after assignment retains the computed position, vertical
velocity, terrain state and graph prefix. A later task or end-of-tick exception
also retains completed movement. The Session latches failure and refuses retry;
no rollback is implied.

When HP is enabled, World position and flags remain authoritative. Movement imports
those scalars into typed health records and projects the raw aliases on success
and after a post-position grid failure, preserving HP, status and other health
state. It does not publish an old adapter position over the moved World object.
Known capacity and generation restrictions are checked before terrain mutation;
secondary publication errors are observable without replacing a primary failure.

## Exact observations and evidence

The optional TerrainSnapshot retains the entire Level and Limits, binding,
policy, phases, depth, per-tick counter, last supplied velocity and computed result,
publication diagnostics and task guards. The existing complete Session, World,
network and optional HP snapshots retain their own authoritative state. Snapshot
values are owned copies, including shared-heightfield identity by index.

The dedicated oracle executes immutable SHA-verified original core, network,
object, terrain and optional health Python bodies. Supplied tasks, velocity
recipes, strict isolation and failure latching are separately identified as
`rta-session-terrain-v1`. Full initial states and lossless per-step deltas cover
observations; a wire hash alone does not prove terrain or actor state equality.

The four older Session trace formats reject any present terrain extension so
that they cannot silently omit it. Their disabled terrain bytes and fixture
identities remain unchanged. The new terrain trace is an independently complete
format, rather than a weaker version of an older format.

Local Apple Silicon verification covers 182 API checks and 235 full-state
Python-model cases in Release and with AddressSanitizer/UndefinedBehaviorSanitizer.
All 25 linked simulation source units and both new entrypoints were instrumented;
macOS leak detection was disabled, so these results make no leak claim. Three
independent complete Python generations produced identical gzip and manifest
bytes. All 16 documented arithmetic configurations participate in the movement
corpus, including terrain contact, grounded clamps, delayed peers, health aliases
and partial failures.

The full 65-case native regression run passed 63 cases initially. The two new
replay checks found a locally truncated generated fixture; restoring its exact
independently reproduced bytes let both checks pass without changing implementation
or expectations. The truncation cause was not established. All 50 older fixture
files remained byte-identical. The 268 Python unit tests passed with 138 explicitly
skipped native-path/platform cases; dedicated CTest transport tests exercise the
new native path. LLVM 20 formatting, GCC 15 syntax checks over seven changed
translation units and the 46-file simulation boundary check passed.

The actual trace SHA-256 is
`cf89a015a40f610c7eb68dd2fd17c946acc7f4e7a1c0f2ed18c3187e1394648f`.
Platform CI requires complete CTest runs and exact Python-derived traces on macOS
ARM64/x64, Windows ARM64/x64 and Linux x64, including this new suite. Local results
alone do not establish those platforms; their execution evidence belongs in the
pull request checks and reviewed artifact comparison.

## Remaining work

This gate uses supplied post-fixup terrain. Original level parsing, live piece
editing, dynamic actor contact, standing objects, touch callbacks, water, root
motion, Player_move and Creature_moveBy remain separate services. It does not
establish original stick mapping, a playable scene or original-console network
compatibility. Norrath behavior requires independent contracts and validation.
Original game content remains private and outside fixtures, source and exports.
