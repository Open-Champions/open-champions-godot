# Opt-in Controller facing

`rta-session-facing-v1` connects the existing Session turning adapter to the
bounded RTA Controller and movement sandbox. Facing is disabled by default.
This is control composition around the already ported `Creature_turnToward`;
**no new reference function is ported and no port credit is added**. Original
`Player_move`, player/AI dispatch, animation/root motion, water, networking
services and original HUD composition remain outside this integration. Norrath
is unsupported.

## Immutable evidence and authored choices

Read-only evidence is `../../champions-return-to-arms-re`. The generator reads
immutable Git objects and verifies these existing pins before executing them:

| Source | Commit | SHA-256 |
|---|---|---|
| `tools/ref_creature.py` | `a0d89502ce1431375a6fc045b91a73039114e6b9` | `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7` |
| `docs/spec/creature-tick.md` | same creature commit | `bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e` |
| `tools/ref_core.py` | `41e847af5360724ab62d208594f8027ea3c1b686` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |

The unchanged [turning](SESSION_TURN.md),
[creature control](SESSION_CONTROL_CREATURE.md) and
[recovery](SESSION_RECOVERY.md) generators/manifests pin the other original
arithmetic, World, network, movement, HP and player bodies and snapshot helpers.
The new generator hash-pins those authored composition dependencies too. The
reference Python is MIT, copyright 2026 TrustlessMike and contributors.
Expectations execute original Python bodies, never native output.

`Config::facing` must explicitly contain `FacingPolicy::DirectionalTarget`,
`TurnInitial` and mode 0 (ordinary) or 1 (opposite). Only DryCreature is accepted.
The descriptor's raw rate, unsigned angular velocity and slow byte are supplied
inputs. The sandbox's rate `0x3f800000` is a **nonzero demonstration setup**, not
a claimed retail class statistic. Native callers can supply zero, negative and
other raw rates according to the existing turning body. Unsupported profiles,
policies, movement combinations, modes and budgets refuse during construction,
before replacing a running scene.

On each genuine ready frame, the existing priority-20 task reads imported
`World.pads[0].held`, subject to the existing release/input-frame cutoff:

| Ready input | Native axis velocity |
|---|---|
| POS_X only / NEG_X only | `0x42700000` / `0xc2700000` on X |
| POS_Y only / NEG_Y only | same positive / negative values on Y |
| Neither or both bits on an axis | raw zero on that axis |

For nonzero XY velocity, target is `(add(currentX,vX), add(currentY,vY),currentZ)`
using native RTA arithmetic. There is no normalization. Both axes cancelling,
neutral input and released buffered input make **no turn call**, even when prior
angular velocity is nonzero. The order is **turn, dry movement, damage edge,
optional health then mana recovery**. Input mapping, target construction and
this placement are authored host policy, not inferred original player behavior.
Turning's signed pre-turn result, spring/limit/slow rules, ordered stores and
failure publication remain in the existing adapter/body. Rate `+0xb0`, velocity
`+0x110`, facing `+0x38` and slow `+0x2fc` are never recomputed in GDScript.

## Ownership, observations and bounds

The Controller calls `enable_turning({1,1})` and binds its existing HP identity
through `bind_turner`. There is still one actor, World, handle pool, terrain Level
and HP Memory/store log. Both movement and turning borrow that Memory; recovery
borrows it too. Generation, address, allocation extent, base HP dispatch and
managed-overlay checks remain the existing Session checks. Movement imports
published World facing without overwriting angular velocity, rate or slow.
Recovery preserves these same actor bytes.

Complete Controller snapshots retain immutable setup/terrain, facing descriptor,
mode and the complete Session. The Session's existing 20-field turning snapshot
retains bindings, original setup, limits, phase/publication context, per-tick
counters, signed results, ordered writes/calls/events, draws and operation inputs,
results and completion flags. HP retains every authoritative sparse byte and
shared store exactly once. The dedicated observer composes the existing complete
creature/recovery and turning observers; legacy observers still refuse active
extensions instead of omitting them. Disabled legacy fixture contracts are unchanged.

The closed facing policy caps both facing alone and facing plus recovery at
**256 ready ticks**, retaining the existing 8,008 submission cap. Legacy Isolated,
DryCreature-only and recovery-only caps stay 2,000, 512 and 256. Turning adds at
most three stores counted in model writes/events and shared Memory log, plus one
operation: **10 entries per ready tick**. Existing movement/damage/scheduler is
at most 67; facing alone fits 96/tick. Existing recovery composition fits 128;
adding turning fits a conservative 160/tick. `256*160 + 2048 + 128 < 63488`
(the Session event-history cap) leaves the network tail and operation reserves.
Setup uses unlogged pokes; histories are never cleared to fit. Tests execute the
full chosen cap in all 16 arithmetic settings, with and without recovery.

No-ready, pending-level and `no_sim` paths do not execute the ready composition.
Pause/stop submit nothing; focus/dialog release drains buffered directions through
the existing cutoff. Reset constructs the same explicit initial state. Operation
failures retain executed stores, events, operation/result prefixes and published
facing, latch failure and refuse retry. Failed prefixes remain observable and
are deliberately not reloadable checkpoints.

## Native save formats

Active facing writes **replay v3** (`OCNSAVE\0`) and **indexed snapshot v5**
(`OCNSNAP\0`), with or without recovery. Both snapshot entry points select v5.
Replay v1/v2 and snapshot v1/v2/v3/v4 readers and byte layouts remain unchanged;
v3 retains its existing deferred-history continuation behavior and v4 retains
its eager recovery validation.

Replay v3 places one counted descriptor after max-ready-ticks: facing policy,
initial rate/velocity/slow, mode, then optional recovery Initial, encoded using the
portable Checkpoint codec. Its witness adds facing raw32 and angular velocity
raw16 before the existing optional recovery witness. There are no host pointers,
callbacks, struct dumps or original save bytes.

Snapshot v5 uses the existing four-section constructor/current/history/journal
layout. The closed constructor recreates one actor and all borrowed bindings.
The Session checkpoint appends turning counters/result, phase/publication/depth,
draw count and four histories. Constructor-owned identity, initial descriptor,
limits and fixed callback policy are not accepted as replacement pointers or
mutable bindings. Shared Memory restores through HP once. Validation checks
identity/generation, immutable rate/slow, World/raw facing and XY, phases/counters,
operation domain and frame order, policy mode, ordered store addresses/widths,
write/event equality, velocity/facing/result consistency with retained histories,
aggregate budgets and canonical section/journal/witness agreement. V5 owns and
validates all histories before scene publication, without executing the journal.
Corrupt, newer and unsupported files preserve the running scene.

Replay and direct snapshot tests continue from nonzero velocity, reversal,
queued input and pause, including recovery, and compare complete states after
continued commands. Independent lifecycle recipes also insert both save/load
forms mid-schedule: Python retains its uninterrupted state while native execution
replaces its owner with the loaded checkpoint.

## Sandbox and verification

The existing sandbox offers **Facing test (opt in)** and **Facing off / restart**.
Each starts a fresh run on the selected initial terrain and preserves the selected
movement/recovery policy and optional appearance. Read-only diagnostics expose
policy, raw facing/rate/angular velocity/slow, mode and last signed result.
The scene uses the existing native facing endpoints and `lerp_angle` solely for
presentation. Successful policy changes, reset, terrain replacement and load snap
both endpoints and clear elapsed debt. Fixed 60 Hz simulation and uncapped default
rendering remain unchanged. No assets, HUD or animation system was added.

`tools/generate_session_facing_vectors.py` and the narrowly named native helpers
produce compact lossless complete-state traces across all 16 FPU settings.
The corpus covers all 16 directional masks, neutral/release, slow 1/60/61,
negative rate, velocity wrapping, opposite mode, delayed input, ordering,
pause/stop/reset, both checkpoint forms, pending/no-simulation and failure/latch
prefixes, and complete final states at the full 256-tick cap. The API suite adds
invalid setup, stale generations, resource failures before/after completed turns,
refreshed-checksum corrupt histories/descriptors, and identical tick-indexed
complete observations at 30/60/144/240 FPS and an irregular uncapped schedule.

Executed results and exact commands are recorded below.
Private acceptance is skipped: `../../private-validation` is unavailable and no
alternate private location was searched. Windows, physical Intel Mac, clean-machine
installation, signing/notarization, sanitizers, original-game playability and
original-console interoperability are not established by this work.

## Executed local verification (2026-10-14)

Artifacts were built locally from this working tree before the local commit.
Host: arm64 macOS 27.2 (26B5101f), Apple M5 Pro, Godot 4.5.2. Native x86_64
executables ran under Rosetta on the same Mac. `file` confirmed both native
architectures and both slices of the debug/release GDExtensions.

- **528/528 independent exact cases passed on both architectures**. Final
  complete trace files compare byte-for-byte with `cmp`; both match expected
  SHA-256 `7d8f0b9ad4e938d145ac212b1dadbc0595189dad454a502205319b284827f1ab`.
  The corpus is 7,033,374 compressed bytes, 220,032,198 expanded bytes; maximum
  row is 864,733 bytes. Only the four small recipe columns reach native execution.
- **11,023 native API checks passed on each architecture**, including full-cap
  save continuation and per-tick complete render-pacing comparisons. The full
  continuation/failure/pacing observation FNV1a64 is `af14fc9a8ce04c8e` on both.
- The affected Release CTest run passed **20/20** in 272.15 seconds. The final
  full `-j 3` run passed **133/134** in 706.62 seconds: the new facing exact
  registration initially omitted its absolute fixture argument and failed before
  executing native recipes. This was fixed in the dedicated CMake registration;
  the isolated registered test then **passed in 133.74 seconds**. Thus **all
  134 distinct gates passed across the full run and corrected-gate rerun**;
  this is not a claim of one 134/134 invocation. No timeout was changed.
- LLVM 20 formatting, `git diff --check`, the existing 66-path simulation
  boundary and **34 boundary unit tests** passed. Existing turning, creature
  control and recovery compressed-manifest identities were independently checked
  unchanged, and their exact/transport regressions passed.
- Final real source-runtime facing smoke: **62 checks**, zero failures. Final
  exported release application: **62 headless facing checks**, zero failures.
  Exported recovery **40**, sandbox **124**, and frame-rate smoke checks also
  passed with zero failures.
- Final **exported GPU application: 77 checks**, zero failures, using macOS
  OpenGL 4.1 Metal/Compatibility. Its rendered captures were inspected for all
  four cardinal directions, opposite-direction reversal, near-zero wrap and
  shortest-path interpolation, pause, reset, loaded nonzero velocity and combined
  recovery state. The original capsule and existing optional synthetic triangle
  model route were used. Controls fit, diagnostics remain readable and terrain/
  health/model controls remain in the existing layout. Captures are under ignored
  `work/session-facing/visual/`; no new art or asset dependency was added.

These were scripted application interactions and actual rendered viewport captures.
Manual desktop keyboard/pointer and native file-picker interactions were not
independently verified. Neither the x64 native run nor a universal extension build
establishes x64 Godot execution, a physical Intel Mac or Windows execution.
Sanitizers and unavailable private acceptance were not run. No release was
published, and no original-game playability or full player/AI dispatch is claimed.

Commands (from this worktree; redirected logs and traces are in ignored
`work/session-facing/`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3 \
  -R '^(session-(turn|control|recovery|creature)|native-(save|snapshot)|frame-clock)'
python3 tools/generate_session_facing_vectors.py \
  --research-dir ../../champions-return-to-arms-re
python3 tools/test_session_facing_vectors.py \
  --exe build/native/champions-session-facing-trace \
  --trace work/session-facing/closed-arm64-complete-trace.tsv
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-session-facing-tests \
  champions-session-facing-trace -j 4
arch -x86_64 build/native-x64/champions-session-facing-tests
python3 tools/test_session_facing_vectors.py \
  --exe build/native-x64/champions-session-facing-trace \
  --trace work/session-facing/closed-x64-complete-trace.tsv
cmp work/session-facing/closed-arm64-complete-trace.tsv \
  work/session-facing/closed-x64-complete-trace.tsv
ctest --test-dir build/native --output-on-failure -j 3
# Above: 133 passes; new registration path failure, fixed then reconfigured.
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -R '^session-facing-exact$'
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
```

Universal bindings used the existing pinned sibling dependency cache:

```sh
cmake -S gdextension -B build/facing-godot-debug \
  -DGODOT_CPP_DIR="$PWD/../../worktrees/godot-health/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/facing-godot-debug -j 4
cmake -S gdextension -B build/facing-godot-release \
  -DGODOT_CPP_DIR="$PWD/../../worktrees/godot-health/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/facing-godot-release -j 4
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --editor --quit-after 120 --frame-delay 50
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --script res://tests/session_facing_smoke.gd
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --export-release 'macOS universal'
ditto -xk build/exports/macos-universal/OpenChampions.zip build/facing-export
'build/facing-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --script res://tests/session_facing_smoke.gd
'build/facing-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --script res://tests/session_facing_smoke.gd -- --facing-visual \
  "--facing-captures=$PWD/work/session-facing/visual"
'build/facing-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --script res://tests/session_recovery_smoke.gd
'build/facing-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --script res://tests/session_sandbox_smoke.gd
'build/facing-export/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --script res://tests/frame_rate_smoke.gd
```

Changed files are the Controller descriptor/glue and Session observation/checkpoint
hooks; native save version envelopes and checkpoint field codecs; dedicated
`session_facing*` API/trace helpers, fixtures and build registration; the facing
Python generator/comparator; thin Godot Controller/sandbox controls and smoke;
and this guide with small links in the creature-control and native-save guides.
Original movement/player/recovery bodies and historical corpora are unchanged.
