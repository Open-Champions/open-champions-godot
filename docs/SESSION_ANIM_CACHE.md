# Owned Session animation cache policy

The separately opt-in [Controller animation-cache policy](CONTROL_ANIM_CACHE.md)
adds ready-frame commands, replay v4 / direct snapshot v6 and a synthetic diagnostic.

`rta-session-anim-cache-v1` is disabled by default. It composes the already
ported cache-bearing RTA Runtime with Session's existing World and HP identities.
It earns **no new reference-function port credit**. The task base is
`e5e64903472ab3a63361328d0eb02ae056a05287` on `codex/integration`, which includes
animation-runtime `f20a87d` and the completed Session-facing work. The released
Session ownership permits this adapter; earlier integration test results describe
those earlier sources and are not verification of this adapter. Integration notes
and sibling implementation bodies are unchanged.

## Immutable evidence

Read-only research: `../../champions-return-to-arms-re`. The generator verifies
immutable Git blobs before executing them, with no dependence on research HEAD.

| Source | Commit | SHA-256 |
|---|---|---|
| `tools/ref_anim.py` | `f516ac798bf28375554fe53ffb31b74802844136` | `6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8` |
| `tools/ref_core.py` | `41e847af5360724ab62d208594f8027ea3c1b686` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `docs/spec/animation.md` | `f2b996dded898f1b0b558b901cab875f775cfdc7` | `be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab` |

The existing Session-animation generator pins World, wire, combat and HP bodies
and ordering evidence. The new generator checks that generator, the Runtime
snapshot helper and each transitive host helper by SHA-256. All pins and the
separately authored input/host policy are recorded in
`native/tests/vectors/session_anim_cache.json`. Existing manifests, generators and
fixture bytes are preserved. The executable chained-clock formula remains
authoritative where prose differs; this adapter neither changes nor derives it.
Public inputs are authored synthetic clips. No decompiler output, retail bytes,
private assets, original dispatch derivation or new reference implementation enters
this adapter. Python expectations never read C++ output.

## API and authored order

Enable the existing object World and HP, then explicitly call
`enable_animation_cache(slot_count, AnimationCacheLimits)`. Before first submit,
register supplied clip bytes/name or an explicit bounded Clip value with
`add_animation_cache_clip`, and bind an HP identity/capacity using
`bind_animation_cache`. Every returned clip ID remains independent even when its
values equal another registration. Sharing an ID shares mutable event slots.
Each bound state retains its HP ID, World serial/address and adapter generation.
There is exactly one owned Runtime, one decoder registry and the existing World.
There are no clock-track mirrors or escaping Runtime references.

The ready-task surface is intentionally limited:

- `start_animation_cache` returns a canonical TrackId (zero for null clip).
- `stop_animation_cache` uses the original stop/cache-release/compaction behavior.
- `advance_animation_cache` returns the signed-byte event mask widened to int32.
- `set_animation_cache_event` explicitly changes one of four shared event slots.
- `decode_animation_cache` requires a selected live cell of the selected binding,
  calls Runtime `animtrack_decode`, then `joint_pos` and `joint_quat` for every
  declared joint at that cell's current raw32 time. It returns owned raw32 values,
  the selected track, decoder slot and sample time.

The demonstrated animation command order is **start -> advance -> decode**,
then advance/decode on later ready ticks and explicit stop/restart. The independent
trace consumes selected HP events after advance; the consumer-failure API test
consumes them after successful decoding. Consumer placement is supplied explicitly.
This is authored host policy, not original `Creature_stepAnim` ordering. Programs
may also decode before advance, decode repeatedly, mutate events before advancing,
or stop/restart during an admitted ready task. The adapter does not force one
command sequence or choose clips automatically.

Operations require the existing post-runObjects ready-task fence, priority >10,
quiescent World/HP/gameplay scopes, current actor allocation/serial/address/
generation and an unsigned32 World frame. Each operation imports that frame
without incrementing it or writing it back. Each state advances at most once per
actual ready tick. Accounting resets only at ready-task entry, not on render
submission. Pending-level, delayed packets, missing remote input and `no_sim`
retain time, decoder work, LRU and counters. A frozen World can skip object
execution while independent ready tasks still execute. Randomization is absent;
the unused Runtime seed remains zero and the existing random ledgers are unchanged.
No second cosmetic stream has been integrated.

## Bounds and failure policy

These are native host limits, separately recorded from original behavior:

| Resource | Default | Hard maximum |
|---|---:|---:|
| Decoder slots (explicit enable argument) | 2 | 1,024; minimum 2 |
| Per-clip input bytes | 64 MiB | 64 MiB |
| Aggregate retained clip bytes and metadata | 128 MiB | 128 MiB |
| Clip identities | 1,024 | 1,024 |
| Runtime states | 1,024 | 1,024 (also constrained by existing HP bindings) |
| Aggregate state cells / canonical tracks | 65,536 | 65,536 |
| Capacity per bound state | 8 | 127; minimum 1 |
| Declared joints per clip / all records per slot | 64 | 64 |
| Decoder key reads per decode, including lookahead | 65,536 | 1,048,576 |
| LOOP subtractions per advance, aggregate across tracks | 65,536 | 1,048,576 |
| Accepted cache operations per actual ready tick | 64 | 64 |
| Retained cache operations | 4,096 | Session's 63,488 event-history cap |

Runtime and decoder budgets are reused intact. LOOP retains every repeated
subtraction store. Key-work charging keeps the original decoder stores preceding
the next refused delta read. There is no modulo shortcut or budget retry.
Deterministic setup refusals publish no partial clip/state/adapter. The host reserves
its binding record before Runtime state allocation. A true allocation failure
follows Runtime's documented unassociated-cell prefix contract; it does not invent
a published StateId. Registration freezes at the first submit, including one
without a ready tick. The cache-free and cache policies reject simultaneous
activation before publishing a second adapter.

Operational preflight refusals publish no accepted operation. An accepted native
failure retains all Runtime/decoder mutations, frame import and counters plus an
incomplete operation record; Session latches failure. Last event/decoded fields
reset when their respective operation is accepted, and are set on success. A
later authored consumer failure retains the completed decode and any executed HP
prefix. No retry, rollback or implicit event consumer is added.

## Complete observations and save boundary

`Session::snapshot()` deep-copies the entire Runtime: both limit sets, every clip
byte/name/word/bind/depth/event/sound/mask value, state IDs and fixed cell membership,
all active and inactive track fields, every slot and all 64 joint records, original
slot owners, cursor/tick/yaw and complete LRU order, imported frame and unused seed.
It also copies binding identities, per-state/per-tick accounting, last decoded
values/events, every operation input/result/decode/completion, phase/depth and
fence state. Existing Session config/failure/network/World/HP and random history
remain in the same complete observation. Snapshots and returned decoded values
retain ownership after subsequent live mutation or Session destruction. Observation
never allocates a decoder slot, touches LRU or executes animation arithmetic.
Inactive fields and nonreciprocal stale links are retained, never repaired.

The dedicated `session_anim_cache_trace.hpp` serializer includes complete Session
base fields and every cache field. Initial observations plus lossless deltas retain
all state, including failures. Legacy Session serializers explicitly refuse an
active cache policy. The closed Controller Session checkpoint explicitly refuses it before
writing Session bytes. Replay v1/v2/v3 and snapshot v1..v5 formats are unchanged.
A generic Session snapshot is an observation, **not a restorable native save**.

## Validation

Final local Release validation passes **141/141 CTest cases in 650.47 seconds**,
with `-j 3`, no timeout and no isolated rerun. This includes the unchanged clock,
decoder, Runtime, cache-free Session animation, turning/facing/recovery, Controller,
native save/checkpoint and simulation boundary regressions. Both Release builds
and all 441 new API checks pass on arm64 and x86_64. The latter executes under
Rosetta on this Apple Silicon host, not a physical Intel Mac.

All **496 independent cases** (31 scenarios, all 16 settings) match every expected
byte on both architectures. Their complete trace files compare equal and share
SHA-256 `908bf4c164f3e6c1f00feac3e875e4c4f1b4d0127ae35195d7fe99aa8345521a`,
FNV-1a64 `144c248d9869a6b6`. Three fresh final-source generator processes reproduce
both canonical files byte for byte. Four provenance tests pass, including **112**
unanimous synthetic omission probes with refreshed artifact hashes, and six
atomic generation tests pass. The dedicated transport suite passes 20 tests
with one unavailable macOS `/dev/full` skip; the actual POSIX output-size failure
check passes. No sanitizer or leak-detection result is claimed.

The Python tooling suite passes **707 tests with 385 explicit skips**. The boundary
checker passes its 68-file census and 36 boundary tests. LLVM20 formatting and
`git diff --check` pass. Historical corpora, manifests, generators and standalone
animation implementation bodies are byte-for-byte unchanged. The new required
suite makes the exact registry 39 suites; prior suite contracts and omission
checks remain intact. Older integrity tests receive only true census updates.

Actual local reports use `local-session-anim-cache`, with `macos-arm64` and
`macos-x64` labels. They are final-source local worktree results, not remote
exact-head artifacts or evidence for the previously tested integration base.
Earlier failures concerned an API-test frame assertion and the omission-test
reader's handling of byte-range deltas. Both were corrected before the passing
full run; production arithmetic and reference expectations were not adjusted to
match native output.

Exact executed commands (all final gates PASS):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release -DCHAMPIONS_SOURCE_COMMIT=local-session-anim-cache -DCHAMPIONS_TEST_PLATFORM=macos-arm64
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCHAMPIONS_SOURCE_COMMIT=local-session-anim-cache -DCHAMPIONS_TEST_PLATFORM=macos-x64
cmake --build build/native-x64 --target champions-session-anim-cache-tests champions-session-anim-cache-trace -j 4
ctest --test-dir build/native-x64 -R '^session-anim-cache-(api|exact)$' --output-on-failure -j 1
cmp build/native/core-traces/session-anim-cache/actual.tsv build/native-x64/core-traces/session-anim-cache/actual.tsv
shasum -a 256 build/native/core-traces/session-anim-cache/actual.tsv build/native-x64/core-traces/session-anim-cache/actual.tsv
file build/native/champions-session-anim-cache-trace build/native-x64/champions-session-anim-cache-trace
build/native/champions-session-anim-cache-tests
python3 tools/generate_session_anim_cache_vectors.py --research-dir ../../champions-return-to-arms-re
python3 tools/generate_session_anim_cache_vectors.py --research-dir ../../champions-return-to-arms-re --output work/session-anim-cache-regen-1.tsv.gz
python3 tools/generate_session_anim_cache_vectors.py --research-dir ../../champions-return-to-arms-re --output work/session-anim-cache-regen-2.tsv.gz
python3 tools/test_session_anim_cache_contract.py
python3 tools/test_session_anim_cache_generation.py
python3 -m unittest discover -s tools -p 'test_*.py'
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
/opt/homebrew/opt/llvm@20/bin/clang-format --dry-run --Werror native/include/champions/session_anim_cache.hpp native/include/champions/session.hpp native/src/session.cpp native/tests/session_anim_cache_tests.cpp native/tests/session_anim_cache_trace.hpp native/tools/session_anim_cache_trace.cpp native/tests/session_snapshot_trace.hpp
git diff --check
git diff --exit-code -- native/tests/vectors native/src/animation_runtime.cpp native/src/animation_decoder.cpp native/src/animation_clock.cpp tools/generate_animation_runtime_vectors.py tools/generate_session_animation_vectors.py
```

Canonical/reproduction comparisons checked each of `.tsv.gz` and `.json` with
Python byte equality. Logs and actual traces remain ignored under `work/` and
`build/`; the complete native log is `work/session-anim-cache-ctest-full.log`.
The x64 API/exact pair passes in 72.42 seconds (exact: 69.65 seconds). The full
arm64 run contains all five new suite gates; the earlier targeted run's corrected
provenance test is superseded by that complete passing run.

Changed files are limited to:

- `native/include/champions/session_anim_cache.hpp`, `session.hpp` and
  `native/src/session.cpp`: public value domain, one owned adapter, ready fences,
  operations, observation/history accounting and explicit checkpoint refusal.
- `native/tests/session_anim_cache_tests.cpp`, `session_anim_cache_trace.hpp`,
  `native/tools/session_anim_cache_trace.cpp`, `native/tests/vectors/session_anim_cache.{json,tsv.gz}`
  and the one refusal guard in `native/tests/session_snapshot_trace.hpp`.
- `tools/generate_session_anim_cache_vectors.py` and
  `tools/test_session_anim_cache_{contract,generation,trace}.py`: independent
  composition, complete-state omissions and atomic transport/publication.
- Additive registrations in `native/CMakeLists.txt`, `simulation_targets.cmake`,
  `session_anim_cache_targets.cmake`; suite/provenance/expectation additions in
  `tools/simulation_contracts.py`, `core_trace_contract.py`,
  `test_compare_core_traces.py`; true census updates in
  `tools/check_simulation_boundary.py`, `test_core_simulation_boundary.py`,
  `test_core_animation_runtime_contract.py`, `test_core_session_turn_contract.py`
  and `test_creature_turn_contract.py`.
- This page and small links in `SESSION_ANIMATION.md` and `ANIMATION_RUNTIME.md`.

The independent corpus covers changing joints and the authored HP consumer,
shared/distinct identities and event mutations, minimum-pool eviction/reuse,
stop/restart/compaction/inactive fields, backwards sampling, repeated decoding,
pause/negative time, loop/end behavior, retired/reused actors, setup refusals,
operation/history/LOOP/key budgets, no-ready paths, later consumer failure,
decoder failure prefixes and immutable snapshots under all 16 configurations.
Provenance and omission probes use synthetic artifacts and do not claim actual
remote-platform execution. Local x64 evidence uses the available Rosetta route.

## Next gate

A separately bounded Controller/save/presentation policy must consume this verified
adapter, specify live-track selection and ready-command ownership, serialize and
validate the entire cache/binding/failure/history domain, and restore its canonical
IDs/LRU/stale cells without guessed repairs. It must keep existing save formats
unchanged or explicitly version a new schema. Rendering must consume already-owned
joint observations without mutating decoder state. Separately evidenced pose/model
mapping, hierarchy/quaternion conventions, blending and model/skin transforms are
required before an animated scene claim.

These joint observations are not blended animation, rendered gameplay, original
attack dispatch or playable-client proof. No Controller, native save, Godot, ROOT/
YAW, pose token, sound, automatic clip selection, original player/AI dispatch,
water, online or Norrath behavior is added. Private acceptance measurements are
skipped because `../../private-validation` is unavailable; no alternate location
was searched. Physical Intel Mac, Windows and exported animated-client behavior
remain unverified by this local adapter task.
