# Opt-in Controller animation-cache diagnostic

`rta-control-anim-cache-v1` is a closed authored Controller policy over the
already ported [Session cache adapter](SESSION_ANIM_CACHE.md). It is disabled by
default. It closes Controller command/save ownership, not animation rendering or
original player/AI dispatch. **No new reference function is ported or credited.**
Base: `codex/integration` at `c538b65175363d2ed232cc94ae46ed8b94b815eb`, containing
Session job `e1e29ba0f975e126aeeb3348c0f394e0553adeb1`. Research is read-only.
No decompiler output, retail bytes, original art or proprietary toolchain is used.

## Evidence

The independent generator reads immutable objects through the existing pinned
loaders, never research HEAD or native output:

| Source | Commit | SHA-256 |
|---|---|---|
| `tools/ref_anim.py` | `f516ac798bf28375554fe53ffb31b74802844136` | `6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8` |
| `tools/ref_core.py` | `41e847af5360724ab62d208594f8027ea3c1b686` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `docs/spec/animation.md` | `f2b996dded898f1b0b558b901cab875f775cfdc7` | `be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab` |

The immutable World/wire/combat/HP host pins, turning/recovery/terrain/creature
bodies and transitive authored serializers remain the existing ones. Their hashes
and the new generator's identity are recorded in
`native/tests/vectors/control_anim_cache.json`. Python separately states the
Controller glue, executes the existing original bodies and produces complete
initial state plus lossless deltas. It does not read a C++ expected-state file.
The reference Python is MIT, copyright 2026 TrustlessMike and contributors.

## Fixed policy and ownership

`Config::animation_cache` contains owned supplied Clip values, including their
bytes, words, name, skeleton metadata and events. Registration order establishes
ClipIDs 1..N. Equal values registered twice remain different identities. Reset
registers these same values afresh; there is no filename, host pointer, callback,
external asset lifetime or new random authority. RTA is the only admitted profile.
Both existing movement policies are supported, with optional facing/recovery.
The default Controller retains its original setup and observation/save contracts.

The existing actor, World, HP binding and one Session Runtime own all cache state.
Exactly one bound state has constructor-defined capacity and canonical cell IDs
1..capacity. No parallel clock tracks or pose state exist. Live selection is the
highest-index live cell **after advance**; count zero selects null. Only this live
cell is decoded on that ready task. Lower tracks still advance through Runtime.
Last sampled data remains a historical observation after stop; the UI labels its
sample track separately from the current selected track. No getter clears it.

| Host resource | Closed bound |
|---|---:|
| Ready ticks | 1..128 (explicit configuration) |
| Submissions | existing 8,008 |
| Supplied clips | 1..4 independent identities |
| Slots | 2..4 |
| State capacity / total canonical cells | 1..8 / 8 |
| Runtime states | 1 |
| Per-clip data / aggregate retained metadata | 1,024 / 16,384 bytes |
| Name / words / sound-time entries | existing 1,024 bytes / 512 words / 64 entries |
| Joints / clip length | 1..4 / 1..65,536 ticks |
| Decoder reads / LOOP subtractions per call | 1..256 / 1..256 |
| Session operations per ready task / retained | 1..10 / 1..1,024 |
| Commands retained for the run | 64, including consumed records |
| Commands on one input frame | 4; also bounded by scheduled operations |

The ordinary descriptor uses 2 slots, capacity 4, 10 operations/task, history
1,024 and key/LOOP budgets 256. Setup validates all clips and aggregate limits in
an unpublished owner. A conservative 128*(160+10)+2,048+128 is below Session's
63,488 aggregate-history bound, including movement, damage, turning and recovery.
Clip/key arithmetic remains the unchanged adapter/Runtime/decoder implementation.
Storage is bounded separately from the native save's existing 32 MiB file cap;
serialization refuses a file exceeding that cap rather than dropping histories.
Active-cache construction reserves 100,000 bytes for 10,000 ten-byte journal
records plus 4,096 witness bytes before publishing the Scene; the disabled
policy retains its original two-byte journal reservation.
Terrain retains its existing independent limits and save refusal boundaries.

## Tick-indexed commands and exact order

`queue_animation(kind, ClipID, frame)` admits only Start, Stop and Restart and
nonnull registered IDs. Frame zero means **the next local input frame** (`lif`)
at queue time. An explicit frame must be at least that frame and at most 8,008.
Commands own monotonic sequence IDs, intended frame, kind, clip, consumed and
completed flags. They may be queued while paused. Same-frame commands execute in
enqueue order; future-frame commands can be enqueued out of frame order. Each
command and the whole per-frame operation budget validate before queue mutation.
Queue/history storage never grows during render or read-only observation.

At the existing priority-20 post-runObjects ready task:

1. Increment the Controller's ready count, import frame/pad, and reserve the whole
   command prefix plus advance and one possible decode against call/history limits.
2. Existing optional directional turning, movement, damage-edge consumption and
   damage, then optional health/mana recovery execute in their existing order.
3. Commands for **this exact input frame** execute in sequence order. Start calls
   the Session start adapter with blend zero and `LOOP|KEEP` (`0x0201`). Stop calls
   stop with blend zero for that clip. Restart calls stop then start with those same
   arguments; these are the proven adapter bodies, not a rewritten lifecycle.
4. Advance the binding once with raw speed `ONE` (`0x3f800000`). Sound/root/yaw
   requests remain false. Select the highest live canonical cell and decode it once.
5. Return to normal submission completion. No cache event triggers HP or another
   effect. Events are observations only; there is no event-mutation command.

This ordering, selection and input mapping are authored host policy, not inferred
original `Creature_stepAnim` or player dispatch. Capacity replacement remains the
Runtime body's exact behavior. No automatic movement/attack selection, blending,
pose mapping, randomization, sound, ROOT/YAW or new cosmetic seed is introduced.
No-ready submissions and pause perform no animation work. Pause/release neutralize
movement input under the old cutoff but retain animation commands for their exact
frames. Stop is terminal; reset creates a fresh identical configuration. A queued
command is marked consumed before its first accepted adapter operation and completed
only after its final operation succeeds. Failure retains every executed prefix,
Runtime/decoder/history change and incomplete command/operation, latches Controller
and Session failure and never retries. Failed state is fully observable but remains
unreloadable, as in the existing save policy.

## Versioned bytes

Cache enabled writes **replay v4** (`OCNSAVE\0`) and **indexed snapshot v6**
(`OCNSNAP\0`). Both direct snapshot entry points select v6. Disabled writes still
select replay v1/v2/v3 and snapshot v1/v3/v4/v5 by the existing policy rules; all
legacy readers, old vectors and v1..v5 snapshot contracts remain unchanged.
Snapshot v2's existing reader remains available. Version is scoped by magic.

All fields are explicit little-endian, never object dumps. Replay's existing
24-byte header remains magic8, version u32, RTA-profile u32, total u32, CRC32 u32;
CRC uses the existing polynomial and treats the CRC field as zero. V4 body:

- Existing model, mul-bias, div-nearest and movement: four u8 fields; seed u32,
  maximum-ready-ticks u32.
- Descriptor byte count u32, then portable Checkpoint encoding of required
  AnimationCache, optional Facing, optional Recovery, in that order.
- Existing seven terrain limits (u32 each), supplied-terrain bool u8 and optional
  existing terrain body.
- Action count u32 (at most 10,000). Each action is kind u8, value u8, frame i64.
  Kinds 0..3 keep advance/pause/release/stop with frame zero. Kinds 4/5/6 queue
  Start/Stop/Restart, value is ClipID 1..4, frame is the resolved intended frame.
  Journal order implicitly owns command IDs. Reset clears both journal and cache
  command identity/history, as in the existing Scene reset.
- Existing position/vz/radius/HP, frame/counters/bools/error witness. V4 then adds
  cache-witness byte count u32 and Checkpoint encoding of selected TrackID,
  optional last event, optional last decoded sample. Existing optional facing and
  recovery witness fields follow in their existing order.

Checkpoint scalars retain the existing codec: integral values are 8 bytes,
including signed two's-complement values and raw32/raw16/raw8 values widened to
8 bytes; booleans are canonical u8; enums are 8-byte unsigned values; counts are
u32; optional values are bool then payload; arrays have no count. Strings are
u32 character count then one widened unsigned character per 8-byte value.

Animation descriptor order is clips vector, slots, capacity, calls/task, history,
key-record budget, LOOP-subtraction budget. Each Clip encodes name, data vector,
joint count, length, bind vector (four i16 values per row), depth vector, four
(seconds,mask) events, optional sound-time vector, sound group, joint mask, words
vector. Vector counts obey the above closed caps **before allocation**.

V6 uses the existing 56-byte indexed header and four independently CRC-checked
sections in order: constructor replay v4, current, retained histories, journal
replay v4. Header layout and section descriptors are unchanged from v5. Current
begins action count u32, history-container count u32, history counts u32[], current
byte count u32, current Checkpoint bytes, then the v4 continuation witness.
Constructor identity and immutable geometry remain separate from current state.

The Controller checkpoint appends cache fields after its existing current scalar
fields and before Session: next-command ID, selected-track ID, commands vector.
A command is ID, frame, enum kind, ClipID, consumed bool, completed bool.
The command vector is bounded current continuation state, not deferred history:
pending commands are immediately owned without reading/decoding elapsed ticks.

Session appends its cache checkpoint after existing recovery/turning fields:
HP binding ID/serial/address/generation, StateID, then Runtime; phase, depth,
aggregate/binding calls, advanced bool, optional last event, optional last decoded;
operations form one additional **history section** container. Session's outer
ready-task fence/publication/network/World/HP state stays in its existing fields.

Runtime order: limits (decoder six limits, states, cells, LOOP budget), decoder
snapshot, state count u32, then (StateID, pose token, count, capacity, flags,
ordered cell vector) per state. Decoder snapshot order: frame, cosmetic seed,
six limits; clips [(ID,Clip)]; tracks [(ID,clip,slot,weight,rate,life,life-rate,time,
flags,sound-index)]; slots [(index,clip,cursor,tick,owner,yaw,64 joint records)];
LRU vector. Each joint is pt, rt, position3, velocity3, quaternion4, qvelocity4.
All inactive cells/joints are encoded, not just live or sampled values.

A decoded sample is TrackID, SlotID, raw time, joints [(position3,quaternion4)].
An operation is kind string, binding, ClipID, TrackID, frame, raw parameter,
flags, event index, (seconds,mask), optional result, optional decoded, completed.
No current value is reconstructed by replaying a clip from zero.

## Restore and complete validation

Internal checkpoint hooks create unpublished decoder/Runtime storage, restore
canonical IDs directly, validate, then swap ownership. They do not call alloc,
touch, start, stop, advance or decode to reconstruct state. They validate immutable
limits/pool, canonical IDs and counts, cell ownership/uniqueness, clip shape,
aggregate storage, ID membership, slot indices and LRU permutation/cursor bounds.
A track-slot link and slot-owner link need **not** be reciprocal: valid stale
references remain exact. Foreign references are corrupt; stale references are not
silently repaired. Inactive contents and original LRU order remain unchanged.

The closed Controller/Session layer checks constructor clips/capacity/membership,
fixed flags/arithmetic domain, actor allocation/serial/address/HP generation,
phase/depth/fence, counters and network-frame ownership, pending command identity,
kind/frame/clip and per-frame work, selected live track, operation kind/arguments/
frame order/completion and scheduled command -> advance -> optional decode order.
It checks the selected sample against the current cell/slot with existing pure
raw joint reads under the saved FPU configuration (no decode/LRU mutation). A
bounded scan of declared start/stop ClipID topology validates active IDs/count and
selected decode shape; it never constructs the current Runtime from that scan.
It checks last event/sample against retained operations and command identities
against the journal **by inspection**, without executing gameplay or decoder
history. All histories remain separate from current state. V6 eagerly validates
and owns the bounded history sections before Scene publication; it does not make
resumption dependent on replay proportional to elapsed animation ticks.

CRC/length/profile/version, section bounds, truncation, extra bytes, excessive
counts, invalid booleans, unknown IDs and inconsistent state refuse before replacing
the caller's Scene. Failed Controllers retain the existing save refusal. The old
Session/Controller serializers require an explicit cache-schema gate and otherwise
refuse the active cache. Complete observers retain setup, all command records,
selected track, Session bindings/accounting/fences, complete Runtime/decoder state,
ordered histories, failure prefixes and original random ledgers. Getters are owned
copies and never advance time, allocate/touch a slot or decode.

## Diagnostic presentation and remaining gate

The existing sandbox has explicitly labeled Animation-cache diagnostic (opt in),
Cache off / restart, Clip start/stop/restart, raw track/sample-track/time/event and
raw joint values. Its authored one-joint velocity stream is synthetic diagnostic
data, not replacement game art. It uses the existing pause/reset/save/load, terrain,
movement/health/recovery/facing controls and fixed simulation/uncapped render clock.
Successful load/reset/policy replacement snaps presentation interpolation and
clears elapsed debt. Render uses cached observations; joint quaternions are never
applied to the capsule or a selected model.

Hierarchy, transform conventions, pose/model pairing, blending, skin transforms,
root motion, sound and original dispatch remain separate evidence gates. No
rendered-animation, gameplay-playability, Norrath or console-save compatibility
claim follows from raw joint diagnostics.

## Executed verification

Final results and commands are recorded below. Artifacts/logs/captures remain
ignored under `work/control-anim-cache/` and `build/`. Private acceptance is skipped:
`../../private-validation` is unavailable; no other private location, Documents or
Desktop was searched. Native x64 is Rosetta execution on Apple Silicon, not a
physical Intel Mac. Windows, x64 Godot execution, manual desktop keyboard/file
picker interactions, signing/notarization and sanitizers remain unverified.

The complete independent corpus is 96 cases (six recipes across all 16 arithmetic
settings), approximately 974 KiB compressed / 29 MiB complete trace. Lifecycle
covers three equal-valued distinct clip IDs, two-slot eviction, start/stop/restart,
changing raw joints, events, damage/turn/recovery composition and both reload
forms. Further cases cover paused pending commands, reset identity, accepted key
failure prefixes/no retry, task history preflight and the full 128-ready-tick cap.
Two fresh Python executions per case and a separate process regeneration reproduce
both canonical fixture files byte-for-byte. No native output seeds the oracle.

Arm64 and Rosetta x86_64 each pass **911 API checks** and all **96 exact cases**.
Final complete trace files compare equal; SHA-256:
`925f5a0b66e49863464ed97d9fbb4c9f4d81ed229093532d82dad2b42b7eb403`.
API checks include continuation after paused/pending and live saves, immutable
getters, bounded commands, failure latching/save refusal, reset, refreshed-CRC
corruption of IDs/LRU/cells/cursors/joint ticks/journal identity, valid supplied
nonreciprocal stale ownership (preserved through direct save/load/continue), and
complete per-submission traces under 30/60/144/240 FPS and irregular uncapped
render schedules. The closed command subset does not itself manufacture arbitrary
nonreciprocal owners; the direct-restore test supplies valid stale links explicitly.

Disabled-save compatibility is an independent **before/after writer comparison**,
not the new animation oracle: a local build from immutable base `c538b651` emits
192 replay/direct/indexed blocks covering Isolated, DryCreature, recovery and
facing at all 16 settings. All **360,326,752 bytes** match the new disabled writers.
Aggregate SHA-256:
`e24cf59c79c3653a755706c25dd8d1962d897ffc65edd199f4b35f60022823fb`.
The dedicated compatibility manifest records this base, original save-source hash,
probe source hash, count and exact identity; its CTest gate preserves the contract.
Historical compressed fixtures, generators, omission checks and timeouts are
unchanged. The new complete-state provenance suite passes three tests, including
field-by-field omission probes. The boundary is now 71 sources/headers, with
37 boundary tests; all pass. Python tooling passes **711 tests, 385 explicit skips**.
LLVM20 formatting and `git diff --check` pass.

Universal debug/release extensions built against the existing pinned godot-cpp.
The actual final macOS exported release app passes **30 headless cache checks** and
**34 GPU cache checks**, using OpenGL 4.1 Metal/Compatibility on Apple M5 Pro.
All four final captures (changing values, paused load, stopped and combined
recovery/facing) were inspected: controls fit and diagnostics remain readable.
These are scripted exported-app interactions, not manual desktop/file-picker
verification. Existing exported facing **62**, recovery **40**, sandbox **124**
checks and frame-rate smoke also pass with zero failures. Interpolation snaps
and invalid-load recovery are included. No raw joint quaternion is rendered.

Commands executed from this worktree (logs named by operation under
`work/control-anim-cache/`; tests run on local working sources before commit,
not remote CI or exact-head platform artifacts):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
python3 tools/generate_control_anim_cache_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_control_anim_cache_vectors.py --research-dir ../../champions-return-to-arms-re --output work/control-anim-cache/reproduced.tsv.gz
cmp native/tests/vectors/control_anim_cache.tsv.gz work/control-anim-cache/reproduced.tsv.gz
cmp native/tests/vectors/control_anim_cache.json work/control-anim-cache/reproduced.json
python3 tools/test_control_anim_cache_vectors.py --exe build/control-anim-cache-final/champions-control-anim-cache-trace --trace work/control-anim-cache/arm64-final.tsv
build/control-anim-cache-final/champions-control-anim-cache-tests
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-control-anim-cache-tests champions-control-anim-cache-trace -j 4
arch -x86_64 build/native-x64/champions-control-anim-cache-tests
python3 tools/test_control_anim_cache_vectors.py --exe build/native-x64/champions-control-anim-cache-trace --trace work/control-anim-cache/x64-final.tsv
cmp work/control-anim-cache/arm64-final.tsv work/control-anim-cache/x64-final.tsv
shasum -a 256 work/control-anim-cache/arm64-final.tsv work/control-anim-cache/x64-final.tsv
python3 tools/test_control_anim_cache_contract.py
python3 tools/test_control_anim_cache_legacy_bytes.py --exe build/native/champions-control-anim-cache-legacy-bytes
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
git diff --check
```

The auxiliary `build/control-anim-cache-final` uses the same Release configure and
build commands, targeting the dedicated tests/trace/legacy-bytes executables.
The baseline writer probe used `git archive c538b65175363d2ed232cc94ae46ed8b94b815eb
native LICENSE README.md` into ignored scratch, the identical probe source, then
`cmake -S work/control-anim-cache/legacy-base/native -B build/control-anim-cache-legacy
-DCMAKE_BUILD_TYPE=Release` and the `champions-control-anim-cache-legacy-bytes`
target. `cmp` of `legacy-base.bin` and `legacy-current.bin` passes. This copies
only this repository's authored baseline sources into ignored scratch.

```sh
cmake -S gdextension -B build/control-anim-cache-godot-debug -DGODOT_CPP_DIR="$PWD/../../worktrees/godot-health/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/control-anim-cache-godot-debug -j 4
cmake -S gdextension -B build/control-anim-cache-godot-release -DGODOT_CPP_DIR="$PWD/../../worktrees/godot-health/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/control-anim-cache-godot-release -j 4
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/control_anim_cache_smoke.gd
mkdir -p build/exports/macos-universal work/control-anim-cache/visual
../../worktrees/godot-health/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --export-release 'macOS universal'
ditto -xk build/exports/macos-universal/OpenChampions.zip build/control-anim-cache-export
'build/control-anim-cache-export/Open Champions.app/Contents/MacOS/Open Champions' --headless --script res://tests/control_anim_cache_smoke.gd
'build/control-anim-cache-export/Open Champions.app/Contents/MacOS/Open Champions' --script res://tests/control_anim_cache_smoke.gd -- "--cache-captures=$PWD/work/control-anim-cache/visual"
'build/control-anim-cache-export/Open Champions.app/Contents/MacOS/Open Champions' --headless --script res://tests/session_facing_smoke.gd
'build/control-anim-cache-export/Open Champions.app/Contents/MacOS/Open Champions' --headless --script res://tests/session_recovery_smoke.gd
'build/control-anim-cache-export/Open Champions.app/Contents/MacOS/Open Champions' --headless --script res://tests/session_sandbox_smoke.gd
'build/control-anim-cache-export/Open Champions.app/Contents/MacOS/Open Champions' --headless --script res://tests/frame_rate_smoke.gd
```

The first baseline configure needed the baseline LICENSE/README files, and the
first export needed its output directory; both preparation errors were corrected
before the successful comparisons/export checks. No runtime expectation was
changed to match native output. No test timeout or omission guard was weakened.

Changed files are limited to the named policy/header/source, internal checkpoint
codec/Runtime/decoder hooks, Session/control/save hooks; the thin Controller bridge
and existing sandbox plus its named smoke; dedicated API/trace/provenance/legacy
compatibility helpers and fixtures; additive CMake and boundary census entries;
this guide and two small links. Integration notes, skills code, original lifecycle/
decoding arithmetic, old fixture corpora and research are unchanged. Commits are
local only; no push, pull request or issue action is performed.

Full native verification: **145/145 PASS, 659.76 seconds**, no timeout or rerun.
The earlier pre-final full run also passed 144/144 in 750.66 seconds. After the
full run, the closing cache-only validation/journal-reservation audit added the
maximum ten-byte-journal check and tightened sample/event bounds. The final
Release rebuild and **4/4 cache gates pass in 32.29 seconds**; final API count is
911 on both architectures. Thus the final cache path has its own closing rerun;
the 145-test invocation predates that last audit. Commands for that closing gate:

```sh
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3 -R '^control_anim_cache-'
```

The final coherence audit adds refreshed-CRC corruption cases for a registered
but incorrect active ClipID, current time versus sample, and a changed finite
current joint value. All four cache gates pass in 31.78 seconds (911 API checks).
A pre-audit committed-source full run was deliberately interrupted when that
consistency gap was found; it is not reported as a completed run. Current data is
still restored directly; the new checks inspect ID topology and pure current-slot
samples without decoder, lifecycle or gameplay execution.

### Final frozen-source closing gate

Implementation commit **`9a57fa7efb79948f88d984636a7fcfa594c66256`** passes the
complete **145/145 CTest run in 616.94 seconds**, with zero failures, zero timeouts
and no isolated rerun. This is the final full run after the coherence audit and
supersedes the earlier full/targeted results for completion. The worktree was
clean during configure/build/test. Exact final commands:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release -DCHAMPIONS_SOURCE_COMMIT=9a57fa7 -DCHAMPIONS_TEST_PLATFORM=macos-arm64
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
```

Log: `work/control-anim-cache/ctest-verified-head.log`. Current-source arm64 and
Rosetta API runs pass 911 checks each; all 96 exact cases match the same independent
fixture and trace SHA-256 on each architecture. Both final universal extension
builds and the re-exported current application pass again: 30 headless and 34 GPU
cache checks. All four current exported captures were inspected. Final formatting,
`git diff --check`, historical fixture/source identity and ownership audits pass.
The final follow-up commit changes only these verification notes, not tested code.
