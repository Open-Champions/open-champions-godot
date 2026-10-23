# Authored native scene saves

The separately opt-in [Controller animation-cache policy](CONTROL_ANIM_CACHE.md)
adds ready-frame commands, replay v4 / direct snapshot v6 and a synthetic diagnostic.

Opt-in [Session recovery](SESSION_RECOVERY.md#native-saves) uses separate replay v2
and snapshot v4 envelopes; the legacy byte contracts below remain unchanged.

`champions::simulation::rta::native_save::Scene` is a closed native save/reload
facade over the existing bounded `session_control::Controller`. It uses the same
Session constructor, one-peer network pipeline, World, movement, collision and
HP services. It accepts both existing controller policies, including the scene's
explicit `DryCreature` policy. Select that policy and its <=512-ready-tick cap
explicitly; the existing Controller default remains Isolated.

This is **an authored native schema, not PS2 save import**, not an interpretation
of a memory-card file, and not console save round-trip compatibility. No original
save structures, game bytes, assets or inferred constants are used. RTA here
selects the documented native contracts; it does not identify a retail revision.
Norrath and unknown profiles refuse.

## State capture and restoration

The latest continuation is [resumable indexed snapshot v3](#resumable-indexed-snapshot-v3).
Replay v1 and direct snapshot v1 remain unchanged, and indexed v2 remains readable.
The v3 range loader and resumed actions do not read or decode retained histories.
Complete historical observations and saving explicitly validate and materialize
those records; these operations still cost proportional to retained state.
Earlier dated continuations below record the prerequisites and their former limits.

Version 1 is a replay checkpoint, rather than a raw memory or struct dump. Its
complete state representation is the initial configuration, optional supplied
terrain with all limits, and ordered controller operations since the last reset.
Every authoritative mutation is behind the facade. It exposes copied observations, immutable setup values and complete snapshots,
never a mutable Controller, Session, World, terrain or HP reference.
It does not capture an arbitrary Controller already running outside this facade.
Adopting the facade must happen when the controller is constructed.

`load(bytes, "rta")` validates the file and builds a **fresh Controller and fresh
Session**, replays the bounded operations, and checks a redundant continuation
witness. Only a successful load returns ownership. A caller can then replace its
current scene; failures cannot mutate that scene. Restoring callbacks through the
known controller constructor preserves their ownership and fixed registration
order. No executable callback, host pointer or captured external state is read
from the file. Reload cost grows with the retained ready ticks and histories;
this is not a constant-time direct snapshot restore.

The representation recreates all state exposed by complete Controller/Session
observations, including:

- configuration, arithmetic mode/bias/rounding, movement policy and tick cap;
- actor serial/address, mover/HP identities, current held input, queued health
  edges, release cutoff, counts, pause/stop state and diagnostics;
- actual network frames, pads, rings, peer state, synchronization seeds,
  bookkeeping, outbox and retained network histories;
- World allocation/handle pool, grids, scheduler, actor scalar fields and
  separate shared, object-local and combat random authorities;
- HP typed records and shared sparse backing, handles, store/call/event logs;
- terrain limits, samples, grid references, cached rectangles, query stamps,
  piece/path metadata and movement results;
- the active creature adapter's root/timer/phase fields, binding, callback
  arguments/results, per-tick bookkeeping and complete histories.

A reset creates the same fresh state as Controller reset, then discards the old
operation stream. Pause/release/stop are recorded in order, including their
interaction with delayed wire input and pending damage edges. Complete histories
are recreated, not cleared or approximated. Operationally failed prefixes cannot
be saved: arbitrary resource/runtime exceptions are not treated as reproducible
checkpoint inputs. Invalid command bits and journal-cap refusals occur before
mutation. The existing movement/health code and Session are unchanged.

The scene currently owns an authored health-test edge; this schema preserves it
and its pending input identity. It adds no new trigger system or original player
behavior. Animation, texture/model decoding, rendering, camera, audio, wall clock,
selected appearance files and presentation input collection are outside this
schema. Arbitrary scripts, remote peers, custom callbacks, turning or animation
Session extensions are outside the closed controller domain.

## Byte contract

All multibyte integers are explicitly **little-endian**. Signed fields use
fixed-width two's-complement encoding, decoded without implementation-defined
unsigned-to-signed narrowing. Raw numeric patterns remain u32; there are no host
float conversions, ABI-dependent sizes, padding, pointers or serialized functions.
No compression, optional extension blocks or ignored trailing bytes exist.

| Offset | Field | Encoding |
|---|---|---|
| 0 | Magic | 8 bytes: `4f 43 4e 53 41 56 45 00` (`OCNSAVE` plus zero) |
| 8 | Schema/behavior version | u32, exactly 1 |
| 12 | Profile | u32, exactly 1 (explicit native RTA contracts) |
| 16 | Total byte length, including header | u32 |
| 20 | Integrity | u32 CRC32 of the entire file, treating bytes 20..23 as zero |
| 24 | Payload | Fields below, in order |

CRC32 uses the reflected IEEE polynomial `0xedb88320`, initial state
`0xffffffff`, final complement. It detects accidental corruption; it is not an
authentication signature. Callers independently supply the expected profile.
Changes to the default terrain, setup, controller behavior, arithmetic or replay
semantics require a new version or an explicitly retained version-1 implementation.
Changing the runtime while retaining this version is not a supported migration.

Payload fields, with no alignment gaps:

1. Arithmetic model u8 (`Ps2=0`, `Ps2g1=1`, `Chop=2`, `Nearest=3`), multiply bias
   boolean u8, division-nearest boolean u8, movement policy u8 (`Isolated=0`,
   `DryCreature=1`), shared seed u32, ready-tick cap u32.
2. Seven terrain limits, each u32, in this order: heightfields, elements, grid
   cells, aggregate samples, aggregate references, path cells, query work.
3. Supplied-terrain boolean u8. If false, version-1 Controller practice terrain
   is selected, preserving `supplied_terrain=false`. If true, encode the complete
   initial `LevelData` as described below. Custom limits remain effective either way.
4. Operation count u32, then two bytes per operation: kind u8, value u8.
   Kind 0 is advance (value command mask 0..31), 1 is set-paused (0 or 1),
   2 is release-input (value 0), 3 is stop (value 0). Reset starts a fresh journal
   and therefore has no operation code.
5. Redundant continuation witness: XYZ raw u32[3], vertical velocity u32,
   radius u32, HP u32, frame i64, submissions u32, ready ticks u32, damage events
   u32, paused/stopped/failed boolean u8 each, error byte length u32, error bytes.
   A failed witness refuses. Every witness byte must match fresh replay.

Supplied initial terrain encoding:

1. Stamp u32, then terrain-grid x0/y0/width/height i64 each.
2. Grid cell count u32; per cell, reference count u32 followed by i64 references.
3. Element count u32; per element, i64 fields in order: index, min_x, min_y,
   max_x, max_y, heightfield index, flags, x, y, z, cosine, sine; then stamp u32
   and cached min_x/min_y/max_x/max_y raw u32 each (116 bytes per element).
4. Heightfield count u32; per field, x0/y0/width/height/lo/hi/offset-label i64
   each, sample count u32, signed i16 samples.
5. Piece-reference count u32 and i64 references; path-grid x0/y0/width/height
   i64 each; path-cell count u32 and u32 cells.

All terrain fields are retained, including uninterpreted provenance/path metadata.
Vector position, rather than an Element index label or host address, preserves
terrain identity. Existing `world_queries::Level` validation checks dimensions,
references, coordinates and aggregate sizes before scene construction.

## Bounds and refusals

Files are at most 32 MiB. There are at most 10,000 operations since reset; capacity
is reserved before gameplay begins, and the cap refuses before mutation. Initial
terrain must leave headroom for the entire action budget and witness. Existing
controller caps still restrict meaningful replay to 8,008 submissions and at most
2,000 Isolated or 512 DryCreature ready ticks. No journal can reset repeatedly to
multiply that work. The underlying Session history limit remains in effect.

Terrain limits cannot exceed the existing native hard caps: 65,536 heightfields
and elements; 1,048,576 grid/path cells; 4,194,304 samples and references;
16,777,216 query-work units. Counts are checked against both their configured
aggregate budgets and remaining bytes before allocation. Signed dimensions and
all references then pass the existing terrain validator. A 32 MiB file cap also
bounds storage below combinations of every individual maximum.

Short header, bad magic, wrong version/profile, length/checksum mismatch,
noncanonical booleans, unknown enums/commands, oversized counts, malformed terrain,
truncated fields, extra bytes, failed replay and inconsistent witnesses all
refuse with `native_save::Error`. There is no migration, salvage, silent clamping
or replacement of the caller's existing scene. The byte API performs no disk I/O;
file writing and save-picker presentation belong to the Godot host adapter below.

## Evidence and exact proof

The implementation is authored from [SESSION_CONTROL.md](SESSION_CONTROL.md),
[SESSION_CONTROL_CREATURE.md](SESSION_CONTROL_CREATURE.md),
[SESSION_HOST.md](SESSION_HOST.md), [SESSION_CREATURE.md](SESSION_CREATURE.md),
the native Controller mutation surface and complete Session observations.
The existing pinned Python-contract bodies and fixture manifests referenced by
those documents establish the simulation behavior; no original save format is
claimed or needed. Public save tests use authored terrain and command fixtures.
Private files were not read.

`native/tests/native_save_tests.cpp` compares complete state using the same
lossless Controller field list and test-only Session/creature formatters as the
existing Python-oracle traces. It does not merely compare XYZ, HP or the witness.
For both movement policies, all 16 arithmetic configurations and both default and
supplied terrain (64 runs), it:

1. Runs 13 submissions, establishing **N=12 actual ready ticks**, with a pending
   damage edge and delayed network input at the checkpoint.
2. Saves bytes, reloads into a new owned Session, checks byte-identical resaving
   and exact complete state against an uninterrupted Controller.
3. Continues commands, including pause/resume and release-input, comparing every
   resulting complete snapshot and retained history. It reloads again mid-run.

A longer DryCreature run saves at N=511 ready ticks, reloads and reaches exactly
the existing 512-tick stop with a complete state comparison. Separate checks cover
paused/manual-stop/limit-stop saves, reset compaction, full action-cap refusal,
every truncation point and one-bit mutation of every byte in a small save,
re-checksummed malformed headers/enums/counts/terrain/witnesses, independent CRC32,
wrong caller profiles and preservation of the current scene after load refusal.

## Godot save/load host

The thin `ChampionsSessionController` now owns `native_save::Scene` from
construction, including when terrain is replaced. All native mutations use the
facade. `save_file(path)` writes the unchanged replay-v1 bytes through Godot
FileAccess (including Unicode paths), flushes and closes a sibling
`path + ".ocnsave.tmp"`, then renames it to the selected destination. A preexisting
temporary file refuses without deleting it; write/rename failures clean up only
the temporary file created by this call. This is a single-writer host operation,
not a concurrent-writer locking protocol or a power-loss durability guarantee.

`prepare_save_file(path)` caps file reads at 32 MiB, requires the native RTA
profile independently of the file header, reloads into a fresh facade, and
requires the scene's DryCreature policy. It prepares display meshes and copied
observations before publishing a candidate token. The existing
`commit_terrain_file(token)`/`cancel_terrain_file(token)` transaction is shared by
terrain and save candidates. The UI first builds the candidate display subtree,
then commits native ownership and replaces the display. Invalid files, unsupported
displays and cancelled/stale tokens preserve the current authority and ground.
Reset invalidates prepared candidates.

The movement/health scene adds **Save…** and **Load…** filesystem pickers for
user-chosen `.ocnsave` files. Its format selector offers **Replay (smaller file)**
and **Snapshot (faster load)**; replay remains selected by default. The choice
affects saving only. Load accepts either format regardless of this choice.
Opening a picker pauses and releases controls using
the existing scene policy; cancellation leaves that paused scene intact. Load
restores the saved pause/stop state and clears presentation keyboard state without
releasing or changing restored native pending input. Successful saves do not
advance the simulation. Appearance remains presentation-only.

The native schema retains collision terrain, not WORLD rendering triangles or
selected appearance files. Load explicitly displays **Restored collision ground**
with the existing **collision-plane illustration** label when that terrain meets
the bounded flat display contract. It refuses other display shapes, rather than
claiming to restore source WORLD geometry. The original native terrain and
simulation histories remain exact. PS2 save import, Norrath and a playable-port milestone remain unsupported.
The separate direct snapshot format is described below; replay v1 and its vectors
remain unchanged.

`godot/tests/session_sandbox_cases.gd`, run by the existing headless scene harness,
uses authored fixtures only. It checks the real binding, real scene save/load
callbacks, Unicode paths, v1 header, restored observations, pending delayed
movement/health continuation against an independent owner, overwrite, temporary
file collision, malformed input, failed writes, modal controls, and supplied
terrain restoration. Existing scene checks still cover movement, health, reset,
stop, terrain transactions, mesh ownership and appearance isolation. Private
validation files were not read.

### Godot host verification (2026-10-13)

On Apple Silicon macOS with pinned Godot 4.5.2 and pinned godot-cpp, configure/build
and first-import startup passed. Headless scene tests passed **101 checks, zero
failures** and emitted `SESSION_SANDBOX_SMOKE_PASS`; archive regression smoke
passed **283 checks, zero failures**. The native-save target passed all **1,899
assertions**, with the unchanged save/trace FNV1a64 `b4ff091ff3007c5d`. The running graphical debug application was
inspected: visible Save/Load controls, both filesystem pickers, a successful save
to ignored `work/visual.ocnsave`, and successful load showing the labelled restored
collision ground. No exported application, Windows, x64 extension, physical Intel
Mac or PS2 save checks were executed in this continuation.

The initial full Release CTest run passed **100/101** cases.
`session-combat-transport` hit its 600-second Unicode-path subprocess timeout
while other worktrees were testing on the same host; no trace mismatch was
reported. The isolated rerun **passed in 271.51 seconds**, so all 101 cases
passed across the initial run and this required timeout rerun. No test limits
or native behavior were changed.

Exact commands (all passed unless explicitly stated otherwise):

```sh
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/../../open-champions-godot/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_sandbox_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --path godot
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^native-save$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3 # 100/101; one timeout
ctest --test-dir build/native -R '^session-combat-transport$' --output-on-failure
/opt/homebrew/opt/llvm@20/bin/clang-format --dry-run --Werror gdextension/src/session_controller.cpp gdextension/src/session_controller.hpp
git diff --check
```

An initial extension build failed on an ambiguous Godot String concatenation;
it was corrected before the successful build and runtime checks. Replay native
source and test vectors were not changed.

### Local verification receipt (2026-10-13)

Apple Silicon macOS, AppleClang 21, Release build. All commands below passed
against the final runtime implementation:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^native-save$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-native-save-tests -j 4
ctest --test-dir build/native-x64 -R '^native-save$' --output-on-failure
/opt/homebrew/opt/llvm@20/bin/clang-format --dry-run --Werror native/include/champions/native_save.hpp native/src/native_save.cpp native/tests/native_save_tests.cpp
git diff --check
```

The full arm64 CTest run passed **101/101** cases, including the final native
save executable. Native-save checks passed **1,899 assertions** on both arm64
and x86_64 (executed locally under Rosetta). The identical combined serialized
save-byte/complete-state-trace FNV1a64 is `b4ff091ff3007c5d`. `file` independently
identified the two executables as arm64 and x86_64 Mach-O binaries. This is local
architecture evidence, not Windows or physical Intel-Mac execution evidence.
The new source/header also passed the existing simulation-boundary validator
with those two paths added to its census for this local check.

An initial test-only namespace collision caused a compile failure and was fixed
before these successful gates. Superseded x64 test runs were stopped after
source changes; the final x64 run completed successfully. No timeout rerun was
needed. No Godot/export/private-input/PS2-save compatibility checks were run.

## Direct native snapshot, version 1

`Scene::save_snapshot()` writes a separate `OCNSNAP\0` file. `Scene::save()` still
writes the original replay-v1 file. `Scene::load()` accepts either magic with an
independently required RTA profile. The Godot host adds `save_snapshot_file(path)`
using the same temporary-file transaction; the existing Load picker accepts both
formats. The Save picker writes the format selected in the scene, defaulting to
unchanged replay v1.

Snapshot loading constructs the same fresh closed Controller and callback graph,
then replaces owned value state directly. It executes **zero retained commands
and zero ready ticks**. It restores pending input, network codec/rings/pads,
World links, retained completed iterator identities, RNG state, HP typed values
and sparse memory with its attached handle overlay, terrain query caches and all
HP/movement/network/Session histories. Fixed setup, pool metadata, binding tables,
policy, immutable geometry and callback registration come from the versioned
constructor and embedded initial configuration. These fields cannot mutate
through the closed facade. This is not a general Session deserializer.

The operation journal is retained solely for unchanged replay-v1 resaving and
future replay checkpoints. The embedded replay witness must agree with restored
state; the direct block must also reencode byte-for-byte before ownership can be
published. Neither comparison runs gameplay. CRC32 detects corruption, not a
malicious author's intentional alternative valid state or forged histories.

### Snapshot byte contract

The 24-byte header has the same little-endian version/profile/length/CRC32 fields
as replay v1, but magic is `4f 43 4e 53 4e 41 50 00` (`OCNSNAP` plus zero), and
its independently numbered snapshot version is exactly 1. Payload:

1. u32 replay-envelope byte count, followed by an unchanged complete replay-v1
   file. Its configuration, initial terrain, journal and witness are validated;
   its operations are **not executed**.
2. u32 direct-state byte count, followed by the closed Controller state below.

Direct-state integral values and enums use eight little-endian bytes, regardless
of C++ field width; signed values use two's complement with checked narrowing.
Booleans use one canonical byte. Dynamic vector/map/string counts use u32;
arrays have fixed documented native dimensions. Optional values use a boolean
followed by the value when present. Map keys are strictly increasing. Strings
encode unsigned character values as eight-byte integers; combat text retains
char32 values. No C++ struct layout or host pointer is encoded.

The direct block has the following fixed group order. Within each group, the
field order is specified by the named handwritten `checkpoint` function and
[checkpoint field codec](../native/include/champions/detail/checkpoint_fields.hpp):

1. Controller input, damage edges, cutoff, submission/ready/damage counts,
   pause/stop state, phase and diagnostic.
2. Session start/completed-run/task state, phase, separate combat Seed and
   draws, Session events, then all callback-free network Machine value fields
   in declaration order (including all four peers, histories and outbox).
3. World actor mutable scalars, shared RNG, cursors/frame/menu bookkeeping,
   task cursor, iterator serial, periodic pool-free count, pads, then sorted complete link records and
   free-list identity. Link references use the existing World snapshot IDs,
   including object serial/address and bit63 iterator IDs.
4. HP adapter's mutable record fields, combat frame/events/seeds/draws, sparse
   backing and store log, then HP model writes/calls/events/draws. HP phase
   and per-tick calls follow.
5. Current terrain stamp and each element's stamp/cached rectangle; terrain
   phase/calls and optional last velocity/result.
6. For DryCreature only: movement phase/calls, optional velocity/bits, callback
   results, writes/calls/events/draws. Callback Value records retain kind,
   signed scalar, raw vector, recursive items and buffer bytes.

The outer file and direct codec each enforce 32 MiB. Dynamic direct collections
are bounded to 65,536 entries, strings to 4,096 characters, and recursive callback
values to the existing depth-four/64-node contract. Existing aggregate Session
and memory history limits are rechecked. Unknown/dangling/missing/duplicate graph
IDs, next-chain cycles, invalid free lists, noncanonical booleans, numeric
narrowing, invalid phases/counts/input cutoffs, inconsistent memory projections,
wrong versions/profiles, corrupt/truncated/trailing data and witness mismatches
refuse. A failed load destroys only its fresh candidate. Snapshot saving can
refuse a replay-valid scene whose combined direct state exceeds the file budget.

### Cost and remaining strict timing requirement

The direct path removes reconstruction work proportional to simulated play time.
Its remaining cost is **O(serialized state)**: exact historical observations and
the unchanged replay journal grow with play time and must still be read,
allocated, checked and copied. The current API materializes all histories eagerly.
Consequently this does **not** establish literally play-time-independent load
latency. Meeting that stronger requirement while retaining exact complete
histories still needs a design for history storage and validation whose load
work is independent of history length. A continuation-only save would change
historical trace equivalence and is outside the proof provided here.
Do not describe this implementation as constant-time loading.

The concrete remaining work is in the load/storage contract, rather than another
binding. `Scene::load(const Bytes&, ...)` checks the whole-file CRC, decodes the
journal, eagerly restores every history collection and reencodes the entire
direct block for canonicality. Godot first reads the entire file. The host now uses a copied, history-free
`Scene::setup()` for terrain preparation, and movement/health observations read
owned scalar values directly. File reading and direct decoding still grow with
retained history. Reducing replay work or moving the same work ahead
of a timer does not satisfy the strict requirement. A future format/API must
separate bounded continuation state from retained historical observations, with
owned deferred history storage, explicit validation/failure behavior and exact
trace comparisons after observation and continuation. Replay v1 and current
snapshot v1 must remain readable and unchanged. No histories are discarded in
the current implementation, and no deferred-history format is implemented here.

The implementation is handwritten from the existing native mutation surfaces and
contracts listed above. No sibling generated code, private input, console save
structure or new game constant was used. Both native policies and every arithmetic
configuration remain supported in this closed domain. Godot remains explicitly
DryCreature. Original saves, arbitrary Session extensions and Norrath remain
unsupported.

### Direct-snapshot verification (2026-10-13)

On Apple Silicon macOS, the focused `--boundary` run compared full replay/direct
state at N=63 and N=64, across the original periodic pool-free-count update.
The focused `--long` run compared complete retained state at N=12 and N=511,
and compared the next ready tick including the 512-tick stop. Both passed.
An earlier full direct test exposed the missing periodic pool-free-count field;
it was added from `World::run_objects`, not inferred from a witness.

Observed sequential load timings for the same authored fixtures (microseconds;
a busy shared host, single samples rather than a stable benchmark):

| Actual ready ticks | Replay load | Direct load | Replay bytes | Snapshot bytes |
|---|---:|---:|---:|---:|
| 12 | 825,072 | 414,470 | 146 | 2,849,584 |
| 63 | 4,245,973 | 519,757 | 248 | 3,041,534 |
| 64 | 4,008,321 | 351,149 | 250 | 3,044,272 |
| 511 | 20,355,402 | 226,839 | 1,144 | 4,718,846 |

The gain comes from skipping simulation replay. The substantially larger snapshot
stores full state/history explicitly; these samples do not prove constant-time
loading or performance on another machine. Complete trace equivalence, rather
than timing thresholds, gates the tests.

The final pinned Godot 4.5.2 headless scene run passed **105 checks, zero failures**
and `SESSION_SANDBOX_SMOKE_PASS`, including the new snapshot binding and actual
scene load callback. The archive regression smoke passed **283 checks, zero
failures**. The 32 simulation-boundary tests, boundary validator, formatting and
`git diff --check` passed. No private input was read. No new visual control was
added; the existing save/load presentation remains as previously verified.

Replay tests passed **1,899 assertions** on arm64 and locally emulated x86_64,
with the unchanged serialized-save/complete-state digest `b4ff091ff3007c5d`.
The replay test source and existing vectors were not changed.

Exact commands used (final builds/checks passed; superseded attempts are noted below):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(native-save|native-snapshot)$' --output-on-failure
build/native/champions-native-snapshot-tests --malformed
build/native/champions-native-snapshot-tests --boundary
build/native/champions-native-snapshot-tests --long
ctest --test-dir build/native --output-on-failure -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-native-snapshot-tests champions-native-save-tests -j 4
ctest --test-dir build/native-x64 -R '^(native-save|native-snapshot)$' --output-on-failure -V
ctest --test-dir build/native-x64 -R '^native-snapshot$' --output-on-failure -V
file build/native/champions-native-snapshot-tests build/native-x64/champions-native-snapshot-tests
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/../../open-champions-godot/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_sandbox_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
/opt/homebrew/opt/llvm@20/bin/clang-format --dry-run --Werror native/include/champions/detail/*.hpp native/include/champions/{native_save,session_control,session,objects,combat_creature,creature_hp}.hpp native/src/{native_save,session_control,session,objects_lifetime,combat_creature,creature_hp}.cpp native/tests/native_snapshot_tests.cpp gdextension/src/session_controller.{cpp,hpp}
git diff --check
```

Intermediate configure/build/test runs failed on a namespace lookup, an overly
strict singleton-World check, a callback-result index check, a stale extension,
a test that compared presentation terrain labels, and the missing periodic
pool-free count. Final review added checked enum-underlying narrowing and its
malformed-input case (19 focused malformed-input checks passed). They were corrected; superseded/duplicate runs were interrupted
and are not counted as final passes. The final arm64 snapshot suite passed **2,071 assertions**, covering 64 complete
state traces, pause/resume, release, stop/reset, byte-identical replay resaving,
repeated direct resaving, malformed counts/headers/graph links, caller-profile
refusal and unchanged live ownership. Its combined snapshot-byte/complete-trace
FNV1a64 is `9220d814d5f83b89`. The final x86_64 run under Rosetta also passed **2,071 assertions** with the
identical digest. `file` identified distinct arm64 and x86_64 Mach-O executables.
The final Release full CTest run passed **102/102 cases** with `-j 3` in
2,070.72 seconds. No timeout rerun was needed. The earlier partial full run was
superseded by the final enum-underlying-range guard and is not counted as this
full-suite receipt.

This is local arm64 and Rosetta evidence, not Windows, a physical Intel Mac,
a newly exported application, PS2 save compatibility or a playable-port test.


Files changed in this continuation:

- `native/include/champions/native_save.hpp`, `native/src/native_save.cpp`:
  separate snapshot API/format and replay decoding without execution.
- `native/include/champions/detail/checkpoint.hpp`,
  `native/include/champions/detail/checkpoint_fields.hpp`: checked portable codec.
- Headers and sources for `session_control`, `session`, `combat_creature`,
  `creature_hp`, and `objects` (`objects_lifetime.cpp`): private owned-state hooks.
- `native/tests/native_snapshot_tests.cpp`, `native/CMakeLists.txt`:
  independent complete-state tests and additive target registration.
- `gdextension/src/session_controller.hpp`,
  `gdextension/src/session_controller.cpp`,
  `godot/tests/session_sandbox_cases.gd`: snapshot file binding and headless proof.
- `tools/check_simulation_boundary.py`,
  `tools/test_core_simulation_boundary.py`: include both save formats and internal
  codec headers in the simulation boundary.
- `docs/NATIVE_SAVE.md`: format, evidence, commands, costs and remaining work.

## Snapshot picker continuation (2026-10-13)

The scene's Save action can now select either native format, using the existing
thin bindings and the same user-chosen file picker. Replay remains the default;
switching back produces the unchanged replay-v1 bytes. The synthetic scene test
now routes snapshot saving through the actual `file_selected` signal, and checks
unchanged native observations, byte-identical overwrite, temporary-file refusal
and failed-write ownership preservation. No native simulation, format, replay
test source or vector changed in this continuation.

On Apple Silicon macOS, pinned Godot 4.5.2 source-project and standalone debug
export scene tests each passed **111 checks, zero failures**, with
`SESSION_SANDBOX_SMOKE_PASS`. The archive regression smoke passed **283 checks,
zero failures**. The standalone debug app was inspected graphically: both
format choices were legible, the filesystem Save picker wrote
`work/visual-snapshot-ui.ocnsave` with `OCNSNAP\0` magic, and the Load picker
restored the labelled collision ground. The executable template is Universal 2,
but this local export contains an **arm64-only debug extension**; it is not a
distributable universal package or release-export proof.

The first ZIP export ran an older scene with 70 checks and is excluded from this
receipt. The final export to a distinct `.app` destination passed all 111 checks
and is the application used for visual verification. Other worktrees were
exporting/testing on the same host; no other worktree was modified.

The full Release native CTest run passed **102/102 cases** with `-j 3` in
**1,168.10 seconds**, with no timeouts or reruns. This includes the replay and
direct-snapshot suites; their unchanged exact trace digests are
`b4ff091ff3007c5d` and `9220d814d5f83b89`, respectively. This continuation reran
native tests on arm64 only; the earlier Rosetta receipt remains separate.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
```

Commands for the final Godot checks (passed):

```sh
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/../../open-champions-godot/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_sandbox_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --export-debug 'macOS universal' "$PWD/build/exports/save-ui/VerifiedSave.app"
'build/exports/save-ui/VerifiedSave.app/Contents/MacOS/Open Champions' --headless -- --sandbox-smoke-test
'build/exports/save-ui/VerifiedSave.app/Contents/MacOS/Open Champions'
git diff --check
```

Only `godot/scenes/session_sandbox.gd`,
`godot/tests/session_sandbox_cases.gd` and this page changed. Evidence is the
existing authored native contracts and save APIs documented above, plus the
executed synthetic tests and graphical inspection; private inputs were not read.
The strict play-time-independent load requirement remains unfinished for the
specific eager-history/validation reasons documented above. No new Windows,
x86_64 extension, release export, Norrath or PS2 save evidence is claimed.


## Bounded observation and setup continuation (2026-10-13)

Steps 1 and the direct-v1 implementation already existed at the start of this
continuation (`7e2637a`, `e07c5ab`, `f33780e`). The remaining strict requirement is
history-independent loading; the format picker did not need another replacement.
This change removes two avoidable full-history copies along that path:

- `Controller::observation()` now reads actor position/velocity/radius, typed HP,
  frame and failure directly from its owned Session through a private, read-only
  Controller projection. No callback executes and no mutable alias escapes. All
  Controller counters, policy and pause/stop/error fields retain their existing
  values. `Scene::save()` and load witness comparisons use this projection.
- `Controller::setup()` / `Scene::setup()` return copied configuration, initial
  terrain including limits, and the supplied-terrain flag. These inputs cannot
  grow with play time. Godot construction and save-candidate preparation use this
  setup instead of a complete Session snapshot. Cost remains proportional to
  selected initial terrain, independently of retained histories.

The existing complete snapshot API, history retention, simulation mutations,
replay-v1 and snapshot-v1 byte layouts and replay vectors remain unchanged.
Evidence is the authored Controller/Session/HP ownership and observation contracts
in [SESSION_CONTROL.md](SESSION_CONTROL.md),
[SESSION_CONTROL_CREATURE.md](SESSION_CONTROL_CREATURE.md),
[SESSION_HOST.md](SESSION_HOST.md) and [SESSION_HP.md](SESSION_HP.md), plus their
native implementation. No original save behavior, private bytes, new game
constant or sibling generated output was used.

Every complete-state comparison in the snapshot suite additionally checks every
field of the small observation and immutable setup against the full snapshot.
This covers both policies, all 16 arithmetic configurations, both terrain choices,
pending input, pause/resume, release, stop, reset and restored continuation. The
new `native-save-access` CTest case checks copied setup isolation and unchanged
replay bytes at 12 and 511 ready ticks, and compares projections through control
operations and the 512-tick boundary. Timing output is informational; exact state
and bytes gate correctness.

### Remaining work

**Step 2 is still incomplete under the strict load-time requirement.** The direct
format still eagerly reads, CRC-checks, allocates, decodes and canonically reencodes
all historical observations and the replay journal. `Session::history()` and
per-service budgets use actual container lengths; callback result indices refer
to complete call histories. A deferred-history implementation must preserve these
counts and indices during continuation, retain owned historical bytes, define
when deferred corruption is reported without publishing misleading complete
observations, and compare complete traces after materialization and subsequent
ready ticks. The current load API and Godot file reader still consume the whole
file. Faster small accessors do not remove that work or establish constant-time
load latency. No deferred-history format/API is added in this continuation.


### Verification receipt

Apple Silicon macOS, AppleClang 21, Release. The focused native run passed replay,
direct snapshot and bounded-access cases. Replay retained all **1,899 checks**
and `b4ff091ff3007c5d`; the expanded snapshot suite passed **5,663 checks** with
unchanged complete-state/serialized-byte digest `9220d814d5f83b89`. The snapshot
check count increased because both read-only projections are checked at every
complete-state comparison; the corpus and hash inputs were not changed.

The same authored DryCreature fixtures were measured before editing the runtime
and after rebuilding it. Each cell is elapsed microseconds for **ten reads**:

| Ready ticks | Old small observation | New small observation | Old complete snapshot for setup | New copied setup |
|---|---:|---:|---:|---:|
| 12 | 144,324 | 1 | 102,097 | 24 |
| 511 | 79,178 | below 1 | 86,724 | 15 |

These are single batches on a shared busy host, not stable throughput benchmarks
or total file-load measurements. A printed zero is below the microsecond timer's
resolution. Complete snapshots after the change still cost 55,169 and 107,570
microseconds for the same batches: their full retained state was deliberately
preserved. No timing threshold gates a test.

The pinned Godot 4.5.2 extension build, bounded editor startup, scene harness
(**111 checks, zero failures**, `SESSION_SANDBOX_SMOKE_PASS`) and archive harness
(**283 checks, zero failures**) passed. The simulation-boundary validator, its
32 unit tests, formatting and whitespace checks passed. No new visual UI was
introduced; no graphical or exported application was run in this continuation.

Exact commands (logs are under ignored `work/`; each command below passed):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --target champions-native-snapshot-tests -j 4
build/native/champions-native-snapshot-tests --access-cost # before runtime edit
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(native-save|native-snapshot|native-save-access|session-control|session-control-creature)$' --output-on-failure -V
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/../../open-champions-godot/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_sandbox_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
/opt/homebrew/opt/llvm@20/bin/clang-format --dry-run --Werror native/include/champions/{session,session_control,native_save}.hpp native/src/{session,session_control,native_save}.cpp native/tests/native_snapshot_tests.cpp gdextension/src/session_controller.cpp
git diff --check
```

The focused regex selected the three save cases; Controller Python-oracle and
transport regressions belong to the full CTest gate. Public inputs were authored
synthetic fixtures only. Windows, physical Intel Mac, x86_64 Godot extension,
release exports, original PS2 saves and Norrath remain unsupported or unverified
by this continuation.

Files changed: `native/include/champions/{session,session_control,native_save}.hpp`,
`native/src/{session,session_control,native_save}.cpp`,
`native/tests/native_snapshot_tests.cpp`, additive `native/CMakeLists.txt`,
`gdextension/src/session_controller.cpp` and `docs/NATIVE_SAVE.md`.

The final x86_64 Release build, executed under Rosetta on the same Apple Silicon
host, passed all three save cases: **1,899 replay checks**, **5,663 snapshot
checks**, and the bounded-access case. Both exact digests matched arm64:
`b4ff091ff3007c5d` and `9220d814d5f83b89`. `file` identified distinct arm64 and
x86_64 executables. This is local architecture evidence, not a physical Intel
Mac or Windows receipt. These commands passed:

```sh
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-native-snapshot-tests champions-native-save-tests -j 4
ctest --test-dir build/native-x64 -R '^(native-save|native-snapshot|native-save-access)$' --output-on-failure -V
file build/native/champions-native-snapshot-tests build/native-x64/champions-native-snapshot-tests
git diff --quiet -- native/tests/native_save_tests.cpp native/tests/vectors
```

The final full arm64 Release CTest run passed **103/103 cases** with `-j 3` in
**763.84 seconds**. No timeout or isolated rerun was needed. This includes both
Controller policies' complete Python-oracle and transport cases, the unchanged
replay suite, the expanded direct-snapshot suite and the new bounded-access case.
The required final command passed:

```sh
ctest --test-dir build/native --output-on-failure -j 3
```

This continuation is **STATUS: MORE**: the tested accessor work is complete, but
strict history-independent file loading is still the remaining step-2 work
described above. No save-format or vector change is claimed.

## Internal checkpoint history partition (2026-10-13)

The next storage prerequisite now exists in the handwritten checkpoint codec.
`detail::Checkpoint::Storage::Split` produces separate current-state and
historical-record streams. Its paired-stream constructor restores both through
the same field validators. This is an **internal representation**, not a new
file format, public Scene load API, or deferred-history implementation.

Explicit `history(...)` field groups cover Session events and combat seed draws,
network outbox/log, combat events and seed/cosmetic draws, HP memory store logs,
HP writes/calls/events, and creature callback results/writes/calls/events.
Current RNG states, scalar draw counters, pending input, network rings, sparse
memory values, terrain caches, World links and completed iterator identities
remain in the current-state stream. Historical records still affect aggregate
resource limits and callback indices; both streams are decoded before the
existing domain, memory-projection, callback-index and budget checks complete.
No continuation executes against empty or approximate histories.

The combined streams share the existing 32 MiB codec budget. Each must be fully
consumed. A history-codec exception restores the active stream before propagating
the failure. The default inline codec emits the original field order exactly;
replay v1, snapshot v1 and all existing vectors remain unchanged.

The new `native-save-partition` case uses synthetic fixtures, both policies,
all 16 arithmetic configurations and both default/supplied terrain (64 runs).
It restores the unchanged snapshot-v1 direct block, splits it, reloads both
streams into a fresh Controller, compares complete traces against replay
reconstruction, and rejoins to the exact original direct bytes. It checks pending
movement/health continuation, pause/release/resume, stop and reset, plus explicit
DryCreature checkpoints at 63, 64 and 511 ready ticks, including continuation
through the 512-tick cap. Truncated/trailing/missing/swapped streams, combined
byte-budget refusal and exception stream ownership have separate checks.

Evidence is the same authored Controller/Session/World/HP contracts cited above,
their mutation surfaces, and the existing exact save and creature trace tests.
No original format constant, private input or sibling generated source was used.

**Step 2 remains incomplete.** Separating records does not make the current
loader bounded in play time. The public loader still CRC-checks the whole file,
reads the replay journal, eagerly decodes all histories and canonically reencodes
them; Godot still reads the whole file. The next implementation needs owned
deferred storage and a range-reading/integrity contract, while preserving history
counts, callback indices and explicit corruption failures during observation and
continuation. This partition supplies the field boundary for that work without
changing the two supported formats or claiming a load-time improvement.

## Indexed snapshot v2 and range loading

This continuation finishes the pending internal history partition and adds a
separate **snapshot version 2**, retaining `OCNSNAP\0` magic. Replay v1, direct
snapshot v1 and all committed vectors remain unchanged. `save_snapshot()` still
writes direct v1; `save_indexed_snapshot()` writes indexed v2. The Godot
**Snapshot (faster load)** selection now writes indexed v2. Load accepts all three
formats, while replay remains the default save selection.

`Scene::load_source(shared_ptr<const Source>, "rta")` owns a serial range reader.
For indexed v2 it reads only the fixed header, initial setup and current state.
It constructs the known callback graph, restores actual current values, validates
the domain, sparse-memory projection, history counts and aggregate budget, and
checks the current continuation witness. It executes no journal operations or
ready ticks. Small `observation()` and copied `setup()` do not materialize records.
The fixed network buffers, bounded World identities and current sparse values are
decoded; cost depends on these and selected initial terrain, not retained logs.

The Scene retains the owned source, validated current bytes, historical counts
and the two deferred section descriptors. No mutable Controller, Session or
historical-container alias escapes. `history_pending()` explicitly reports that
historical integrity and callback indices have not yet been checked. A complete
`snapshot()`, any save, or the first mutation materializes into another fresh
owner before publication: read and CRC-check histories and journal, decode paired
streams, check all existing callback indices and budgets, reencode current values,
counts and histories canonically, and reproduce the exact embedded replay bytes.
Only success replaces ownership. Failure throws `native_save::Error` before
gameplay mutation or a complete snapshot is returned. Repeated failures preserve
the pending state. Reset explicitly discards it and returns to the initial setup
without reading deferred sections. Completed materialization releases the source.

The Godot adapter retains a FileAccess handle instead of loading the whole file
into a byte array. It uses the same candidate/display transaction and reports
`save_history_pending` separately from gameplay values. The visible status adds
**Save data checked before next action.** until materialization succeeds. File
length changes, short reads and section integrity failures refuse. A source may
refer to externally modified storage: CRCs check every section actually consumed;
they neither authenticate files nor promise protection from hostile replacement
with newly checksummed content. Save-file renames continue to use the existing
single-writer temporary-file transaction.

`load(const Bytes&, ...)` also accepts indexed v2, but copies the caller's buffer
into owned storage. That convenience API, reading a full file before calling it,
and the compatibility loaders for both v1 formats still cost proportional to
file size. The Godot v2 file path uses `load_source`, not that convenience API.

### Indexed byte contract

All integers remain explicit little-endian. The total file cap is 32 MiB.

| Offset | Field |
|---|---|
| 0 | 8-byte `OCNSNAP\0` magic |
| 8 | u32 snapshot version, exactly 2 |
| 12 | u32 explicit native RTA profile, exactly 1 |
| 16 | u32 total byte length |
| 20 | u32 CRC32 of the 56-byte header, with this field zeroed |
| 24 | Four section descriptors, each u32 length followed by u32 section CRC32 |
| 56 | Four adjacent sections in descriptor order; no gaps or trailing bytes |

Each section CRC covers every byte of that section, without zeroing any internal
field. The reflected polynomial, initial state and complement match replay v1.
Descriptor lengths must partition the exact file length before allocation.

1. **Setup:** an unchanged replay-v1 envelope for the initial Scene. It must have
   zero actions and exactly match a fresh constructor's save bytes. It never
   executes supplied actions.
2. **Current:** u32 history-container count (at most 32), u32 count per container,
   u32 current-stream byte length, the partitioned checkpoint current stream,
   then the existing continuation witness. Container counts follow checkpoint
   field traversal order. Each is at most 65,536; their aggregate and the current
   shared-draw counter must fit the existing Session history budget. Memory-log
   counts also respect their configured limit. Both count list and current stream
   must be completely consumed. Histories are not represented by empty logs in a
   published complete observation.
3. **History:** the checkpoint partition's historical stream, including the
   original vector counts, records and callback indices.
4. **Journal:** the exact unchanged replay-v1 save, including configuration,
   terrain, operations and witness. Its descriptor is bounded by setup size plus
   the existing action allowance and witness headroom. Inner integrity and exact
   canonical equality are checked during materialization.

The fixed constructor, profile refusals, movement limits and behavior-version
boundaries remain the same. No PS2 format, original save constant, private bytes
or decompiler output supplies this authored container.

### Exact proof and remaining cost

The existing Controller/Session/World/HP contracts cited above and their closed
mutation surfaces supply the evidence. New public tests use synthetic setup and
commands, both policies, all 16 arithmetic configurations and default/supplied
terrain. They compare complete indexed state against replay reconstruction,
byte-identical resaving of both v1 formats, materialization followed by delayed
movement and health, pause/release/resume, stop, reset, and source ownership.
Separate checkpoints cover zero ready ticks, 63/64, 511 and the 512-tick stop.
Corrupt setup/current data refuses immediately; corrupt deferred data refuses
before mutation or a complete observation. Reset can discard a corrupt pending
source. Range-read assertions prove no history or journal bytes are consumed by
initial loading or small accessors. Read-work comparisons gate the bounded
current-state envelope; elapsed times are informational.

**Step 2 remains incomplete for history-independent time to resume gameplay.**
This implementation makes initial indexed file loading bounded, but deliberately
decodes and validates *all* retained records before the first action. That action,
a complete observation or saving can still take longer after more play. The next
step is deferred historical containers with exact logical lengths and callback
indices during continuation, so new records can append without eagerly decoding
the prefix. Corrupt deferred records must still fail explicitly when consumed;
complete trace comparisons must cover later materialization. No constant-time
first action, save, full-history observation or power-loss durability is claimed.

### Indexed continuation verification receipt

Apple Silicon macOS, AppleClang 21, Release. The final full native run passed
**105/105 cases** with `-j 3` in **463.83 seconds**, without a timeout or isolated
rerun. Final arm64 and x86_64 save cases produced the same exact digests:

| Case | Checks | FNV1a64 |
|---|---:|---|
| Replay v1 | 1,899 | `b4ff091ff3007c5d` (unchanged) |
| Direct snapshot v1 | 5,663 | `9220d814d5f83b89` (unchanged) |
| Internal partition | 1,455 | `b849525e60bea01c` |
| Indexed snapshot v2 | 3,155 | `6ad888c5b13781ce` |

The new partition digest includes additional zero-tick and stopped-boundary
fixtures; no existing vector or v1 digest input was altered. `file` identified
separate arm64 and x86_64 Mach-O executables. x86_64 ran under Rosetta on this
Apple Silicon host; this is not physical Intel Mac or Windows execution.

A scratch C++17 measurement used the same authored DryCreature command sequence,
configuration and initial terrain for both formats, through owned in-memory
range readers on the final runtime. It measured initial loading separately from
the first indexed action, then checked byte-identical replay saving after that
action. Each timing is one read on a shared busy host, in microseconds; this is
not a disk throughput benchmark or timing threshold:

| Ready ticks | V1 load | V2 initial load | V1 bytes read | V2 initial bytes read | V2 first action |
|---|---:|---:|---:|---:|---:|
| 12 | 65,823 | 50,842 | 2,845,552 | 2,804,761 | 62,414 |
| 511 | 84,490 | 56,236 | 4,546,490 | 2,804,753 | 85,555 |

The committed indexed case independently asserts four initial range reads, no
history/journal access, no extra reads from small accessors, and bounded read
work across boot, 12, 63/64, 511 and the stopped 512-tick checkpoint. These
structural assertions and exact traces gate correctness, not noisy timings.

Pinned Godot 4.5.2 configure/build and bounded editor initialization passed.
The final scene harness passed **119 checks, zero failures** and emitted
`SESSION_SANDBOX_SMOKE_PASS`; archive regression smoke passed **283 checks, zero
failures**. The running debug application was inspected graphically: Snapshot
selection, save and load pickers, successful save to ignored
`work/indexed-visual.ocnsave`, restored collision ground, readable pending-data
notice, and successful resume with that notice removed. No export was run.

The existing shared dependency CMakeLists and Git ignore metadata were
iCloud-evicted and their reads stalled. A fresh checkout of the *same* pinned
godot-cpp commit was fetched into ignored `work/deps/godot-cpp-save`; the official
Godot 4.5.2 universal ZIP was fetched into ignored `work/`, SHA-512 checked against
the existing release checksum and unpacked under `work/runtime-local`. Existing
shared dependencies and metadata were not overwritten. Native ownership and
staging were verified with Git tree/index plumbing, which does not read the
unavailable ignore file. Initial test-only dispatch placement and scratch-harness
include errors were fixed before the final checks.

Exact final commands (all passed; logs under ignored `work/`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-native-save-tests champions-native-snapshot-tests -j 4
ctest --test-dir build/native-x64 -R '^(native-save|native-snapshot|native-save-partition|native-save-indexed)$' --output-on-failure -V
cmake -S gdextension -B build/godot-extension-local -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp-save" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension-local --config Release --parallel 4
work/runtime-local/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
work/runtime-local/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_sandbox_smoke.gd
work/runtime-local/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
work/runtime-local/Godot.app/Contents/MacOS/Godot --path godot
work/indexed-load-cost
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
/opt/homebrew/opt/llvm@20/bin/clang-format --dry-run --Werror native/include/champions/detail/checkpoint.hpp native/include/champions/detail/checkpoint_fields.hpp native/include/champions/native_save.hpp native/src/native_save.cpp native/src/session.cpp native/src/combat_creature.cpp native/src/creature_hp.cpp native/tests/native_snapshot_tests.cpp gdextension/src/session_controller.cpp
git diff-files --check
file build/native/champions-native-snapshot-tests build/native-x64/champions-native-snapshot-tests
```

The boundary validator and all 32 boundary unit tests passed. Replay tests and
all vector files were checked unchanged. Changes are confined to
`native/include/champions/detail/{checkpoint,checkpoint_fields}.hpp`,
`native/include/champions/native_save.hpp`,
`native/src/{native_save,session,combat_creature,creature_hp}.cpp`,
`native/tests/native_snapshot_tests.cpp`, two additive `native/CMakeLists.txt`
test registrations, `gdextension/src/session_controller.cpp`,
`godot/scenes/session_sandbox.gd`, `godot/tests/session_sandbox_cases.gd` and this
page. Public inputs were synthetic; private validation files were not read.
Windows, physical Intel Mac, x64 Godot extension, release exports, PS2 saves,
Norrath and history-independent first continuation remain unverified or unsupported.

This continuation is **STATUS: MORE** for the first-action cost described above.


## Resumable indexed snapshot v3

This continuation completes the remaining step-2 requirement for the bounded
movement/health scene. `save_indexed_snapshot()` now writes **version 3**;
the Godot Snapshot selection uses that API. Replay v1 bytes, direct snapshot v1
bytes, the replay test source, all committed vectors and the v2 reader remain
unchanged. The v2
compatibility path still materializes before its first mutation.

V3 uses the same 56-byte header, four sections and per-section CRCs as v2, with
version 3 at offset 8. Its Current section adds exactly one field before the v2
fields: **u32 retained journal action count**, at most 10,000. That count is covered
by the Current section CRC and is checked against the actual journal when that
section is consumed. History, Setup and Journal use the existing encodings.
A new version is necessary because action-cap enforcement must not require
reading the old journal before resumed play. No original game constant is added.

Loading constructs the known callback graph and restores actual current values
exactly as v2 does. Historical vectors are private suffix containers inside the
closed facade, with separate reserved prefix counts. Session aggregate budgets,
HP memory stores, HP and movement writes/calls/events, callback results and the
journal all count their retained prefixes when checking capacity. New movement
callback indices include the old call count. RNG state and draw counters, pending
input, sparse memory, World identities, terrain caches and all other current
values continue normally; no old commands or ready ticks execute.

`advance`, pause/resume, release and stop do not access the retained Source.
Their work does not grow with the pre-load history. Small observations/setup
remain history free. Reset discards both prefix and suffix and constructs fresh
state without consuming old storage. This closed domain does not expose partial
historical vectors as complete observations or mutable aliases.

On a complete `snapshot()` or any save, materialization first restores and validates
the original prefix in a fresh owner, including CRCs, canonical bytes, callback
indices, action count and replay witness. It then directly overlays the current
values and appends the owned suffix vectors and operations. It executes **zero
replayed operations**, including for the suffix. Final current-state bytes and
witness must match the running authority. Only a successful merge replaces the
owner and releases its source; failures preserve the running values and suffix.
The resulting complete histories and replay/direct-v1 saves are exact.

Deferred historical corruption does not invalidate independently checked current
state or stop resumed movement. It explicitly refuses complete observations and
all saves when those records are consumed. Repeated failures preserve current
play; reset can discard them. The binding reports `save_history_pending` in both
small observations and advance results, and the visible status says **Historical
save data not yet checked.** until successful materialization. This validation
boundary is deliberate; current-state acceptance is not whole-file integrity.
CRCs do not authenticate malicious, freshly checksummed input.

Load cost remains proportional to selected initial terrain and bounded current
state. It is independent of retained historical length, rather than a universal
constant-time claim across different terrain sizes or hardware. File-backed Godot
loading uses range reads. `load(Bytes, ...)` still copies the supplied whole
buffer, and legacy formats, complete historical observations and saving still
scale with their serialized size. Those convenience/compatibility and observation
costs are not claimed as bounded in play time.

### Evidence and verification (2026-10-14)

Evidence is the existing authored Controller/Session/World/HP and dry-creature
contracts cited above, their closed mutation surfaces and checkpoint field list.
Public inputs are authored synthetic terrain and command sequences. No private
inputs, sibling generated source, console save structures or invented original
behavior were used. Private acceptance measurements were skipped because the
specified private-validation directory is unavailable; it was not searched for
elsewhere.

The expanded indexed suite covers both policies, all 16 arithmetic configurations
and default/supplied terrain. It compares exact complete traces and unchanged
replay/direct-v1 bytes after multiple deferred movement/health operations, then
after pause/release/resume, stop and reset. Checkpoints include 0, 12, 63, 64, 511
and 512 ready ticks. Source instrumentation requires **zero historical or journal
reads during resumed actions**, and initial read work remains bounded across
these checkpoints. Separate tests cover every per-service prefix reserve,
exhausted journal refusal before mutation/read, append capacity and exception
ownership, v2 compatibility, retained source ownership and corrupt deferred
storage refusing every complete-save API without replacing the running authority.

The same preexisting scratch measurement was run before runtime editing and
rebuilt afterward against the final runtime, using identical synthetic DryCreature
setup and commands. Times below are microseconds, one sample on a shared host;
read counts and exact traces, rather than timing thresholds, gate correctness.

| Ready ticks | V2 first action before | V3 first action after | V3 initial bytes read | V3 first-action bytes read |
|---|---:|---:|---:|---:|
| 12 | 59,196 | 8,011 | 2,804,765 | 0 |
| 511 | 69,859 | 5,849 | 2,804,757 | 0 |

The large bounded Current section includes the existing fixed network buffers;
this change trades deferred source ownership and small prefix counters for removing
history I/O and decoding from resume. It does not reduce total snapshot size or
claim a disk-throughput, power-loss-durability or general gameplay benchmark.

Final verification commands (Release; logs under ignored `work/resume-*.log`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-native-snapshot-tests --resume
ctest --test-dir build/native --output-on-failure -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-native-save-tests champions-native-snapshot-tests -j 4
ctest --test-dir build/native-x64 -R '^(native-save|native-snapshot|native-save-partition|native-save-indexed)$' --output-on-failure -V
cmake -S gdextension -B build/godot-extension-local -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp-save" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension-local --config Release --parallel 4
work/runtime-local/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
work/runtime-local/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_sandbox_smoke.gd
work/runtime-local/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
mkdir -p build/exports/resume-save
work/runtime-local/Godot.app/Contents/MacOS/Godot --headless --path godot --export-debug 'macOS universal' "$PWD/build/exports/resume-save/VerifiedResume.app"
'build/exports/resume-save/VerifiedResume.app/Contents/MacOS/Open Champions' --headless -- --sandbox-smoke-test
work/indexed-load-cost
work/resume-load-cost
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
/opt/homebrew/opt/llvm@20/bin/clang-format --dry-run --Werror native/include/champions/{native_save,creature_hp,creature_movement}.hpp native/include/champions/detail/checkpoint.hpp native/src/{native_save,session,creature_hp,creature_movement,combat_creature}.cpp native/tests/native_snapshot_tests.cpp gdextension/src/session_controller.cpp
file build/native/champions-native-snapshot-tests build/native-x64/champions-native-snapshot-tests
git diff --check
git diff --quiet -- native/tests/native_save_tests.cpp native/tests/vectors
```

The pinned Godot 4.5.2 source-project scene suite and actual standalone debug export
each passed **124 checks, zero failures**, with `SESSION_SANDBOX_SMOKE_PASS`.
Archive regression smoke passed **283 checks, zero failures**. The boundary
validator, all **32** boundary unit tests, formatting and diff/vector checks passed.
The focused final native `--resume` run passed **156 checks**.

Initial attempts exposed an invalid zero-stride count read, missing test codec
field declarations, a missing generic append-refusal overload, and the advance
binding's old hardcoded deferred-status default. These were corrected before the
final gates. The first export attempt lacked its output directory; after creating
it, export and exported scene smoke passed. No failed attempt is counted as a pass.

Files changed are confined to:

- `native/include/champions/{native_save,creature_hp,creature_movement}.hpp` and
  `native/include/champions/detail/checkpoint.hpp`: private prefix reserves and
  checked direct suffix append support.
- `native/src/{native_save,session,creature_hp,creature_movement,combat_creature}.cpp`:
  versioned range load, continued budget/index accounting and atomic history merge.
- `native/tests/native_snapshot_tests.cpp`: complete trace/byte equivalence,
  source-work, v2 compatibility, journal and per-service capacity regressions.
- `gdextension/src/session_controller.cpp`, `godot/scenes/session_sandbox.gd` and
  `godot/tests/session_sandbox_cases.gd`: accurate deferred integrity status
  through resumed play and executed scene checks.
- This page. No shared CMake, simulation target, roadmap or CI file changed.

Graphical application selection through computer use timed out twice, so no new
graphical inspection is claimed. The debug export uses the existing universal
Godot template with an **arm64-only debug extension** and is not a distributable
universal/release package. Windows, physical Intel Mac, x64 Godot extension,
release exports, Norrath, PS2 save import and private acceptance remain unverified
or unsupported. The scope is still a bounded movement/health test, not a playable
Champions port.

The final arm64 full Release run passed **105/105 tests** with `-j 3` in
**2,910.56 seconds**. No timeout or isolated rerun was needed. Its save cases
passed **1,899 replay**, **5,663 direct-v1**, **1,455 partition** and **5,193
resumable-v3** checks. Replay and direct-v1 digests remain `b4ff091ff3007c5d`
and `9220d814d5f83b89`; the partition digest remains `b849525e60bea01c`.
The new resumable complete-trace digest is `9f94e5b26ea1a807`.

The x86_64 Release run under Rosetta passed the same replay, direct-v1 and
partition cases and exact digests. Its indexed case initially timed out without
a trace mismatch. The required isolated rerun **passed in 853.95 seconds**, with
all **5,193 checks** and the identical `9f94e5b26ea1a807` digest. Thus all four
x86_64 save cases passed across the original run and isolated rerun. No runtime,
fixture, optimization or timeout setting was changed between those runs.
`file` independently identified arm64 and x86_64 Mach-O test executables. This is
local Rosetta evidence, not physical Intel Mac or Windows execution.

```sh
ctest --test-dir build/native-x64 -R '^native-save-indexed$' --output-on-failure -V
```

This continuation is **STATUS: COMPLETE** for the two requested native-save steps:
the user-chosen-file Godot UI and headless tests are in place, and indexed v3
resumes without work proportional to retained histories while preserving exact
complete replay state when materialized. The platform, content and integrity
limits above remain explicit.

Active [Controller facing](SESSION_FACING.md) uses replay v3 and snapshot v5,
retaining the existing legacy and recovery save contracts.

## Fixture continuation v1

Review fixes, 2026-10-20 (`codex/review-fixes`; finding 2 of the playable-integration
review). Before this, the opt-in combat fixtures (Combat, Animated, Full with its
`EffectSimulation`) and the two-level gate fixture refused every save. They now save as
`fixture-continuation-v1` through `fixture_save::CombatScene` and
`fixture_save::LevelScene` (`native/include/champions/play_fixture_save.hpp`). The dry
`.ocplay` continuation and every OCNSAVE envelope above are unchanged.

Like replay v1 this is a checked replay, not a struct dump. The combat World holds raw
links, iterators and service callbacks, so its memory is not written. The facades own
the fixture, and every operation goes through a journal: refused operations, and the
Full fixture's attach/detach and draw-slot calls (the effect half's own state depends on
them), are recorded too. Loading builds a fresh fixture, replays the journal, and
requires both the witness and the canonical re-encoding to match byte for byte. A
refused operation must refuse again on replay.

| Offset | Field | Encoding |
|---|---|---|
| 0 | Magic | `4f 43 46 49 58 54 00 00` (`OCFIXT`) |
| 8 | Version | u32, exactly 1 |
| 12 | Profile | u32, exactly 1 (RTA); the caller passes `"rta"` |
| 16 | Total length | u32 |
| 20 | Integrity | CRC32 of the file with bytes 20..23 read as 0 (the OCNSAVE polynomial) |
| 24 | Kind | u8: 0 Combat, 1 Animated, 2 Full, 3 Level |

Then, in order:

1. Combat kinds only: the selected terrain, in the OCNSAVE encoding (seven limits, then
   the supplied-terrain fields; `native_save::terrain_codec`).
2. Full only: recipe-constants identity, u32 byte length and u32 CRC32 of the
   `fx_skill_recipes` JSON text. The constants themselves are never written. Loading
   requires the same identity.
3. Journal: u32 count, then per operation u8 kind, u8 refused (0/1), u32 value, and for
   the level's confirm/request a u32 length plus at most 64 name bytes. Kinds: 0 advance
   (command), 1 pause (0/1), 2 release input, 3 stop, 4 hit event (slot << 24 | mask),
   5 press attack (held), 6 release attack, 7 block (held), 8 Holy Strike, 9 attach
   (1 drawing, 0 headless), 10 draw slots, 11 gate open, 12 gate confirm, 13 level
   request.
4. Witness: u32 length, then the observation (position, HP, max HP, radius, facing,
   frame, submissions, ready ticks, damage events, paused / stopped / failed, error
   text) and the shared seed, plus:
   - combat: every actor's slot, handle, present / dead, position, HP and state, and
     the Full effect counts including the unrouted names;
   - level: the flow (level, generation, travel ticks, restored count, gate / trigger
     / menu flags, marker, resolved spawn, saved levels, restored bytes).
5. Presentation: u8 flag; if 1, a u32 length and an opaque block. The level writes 0.

During replay, a stand-in EffectWorld receives what the attached world received. The
per-machine EffectWorld itself cannot be replayed: the simulation's cosmetic draws and
the drawn frames' cosmetic draws (beam drift, row-4 flipbooks) interleave. The Godot
host therefore puts `ChampionsFxBridge.state_bytes()` in the presentation block. That
block holds the exact EffectWorld snapshot (`fx_world_save.hpp`, magic `OCFXWLD`
version 1) and the bridge's emitters, light-node keys, tick and stop reason.

The snapshot covers:
- the particle pools, with resident definition slots stored as definition addresses;
- definitions, trails, beams, the glow queue, decals and growers;
- level light, light effects and linked nodes;
- the cosmetic and libc generators, fx_spawn's made flags and every counter.

binary64 and raw float values are stored as their bit patterns. `load_world` decodes
into a scratch world, requires the canonical re-encoding, and only then overwrites the
target. The host's `near_camera_focus` callback is not state.

Godot host: `get_play_capabilities().save_formats` lists
`{id: "fixture-continuation-v1", method: "save_file"}` for these fixtures.
`prepare_save_file` dispatches on the magic and validates the presentation block;
`commit_terrain_file` publishes the scene and restores the bridge. The block is
restored again to a bridge attached before the next ready tick, because the play scene
rebuilds its FX node after a load.

Verification (`fixture-save`, 70 checks; `playable_wave_smoke_full_combat.gd`):
- A Full fight is saved on tick 174, with Holy Strike cast on 170 and in flight, and a
  drawing world presented twice on even ticks. Continued, it gives a byte-identical seed
  and HP trace and a byte-identical effect world.
- The level fixture saved mid gate travel continues byte-identically. It is saved on
  step 90; the gate was confirmed on step 17 and travel lasts 180 ready ticks.
- In Godot, the same scenario saved and loaded through the scene's Save / Load gives the
  uninterrupted trace's SHA-256 `8dc349c1…0c` (the wave-11 value) and an identical
  bridge state.
- Mutation checks: skipping the presentation restore, or ignoring the journaled attach,
  each fails a check.

Limits:
- The witness is observable state, not a digest of World memory.
- Cost grows with the journal (fixtures stop at 4,096 ticks; draw slots add one entry
  per drawn frame).
- A bridge mutation made from GDScript outside the controller is captured only through
  the bridge snapshot, not replayed.

## `.fight` companion (valor fight, 2026-10-23)

When a real-data session has a shared-World player (the first valor fight,
`native/src/campaign_fight.cpp`), `real_data_play.gd` writes a `path + ".fight"`
JSON companion next to `.player`, `.campaign` and `.level`. The `.level`
manifest's `fight_sha256` binds it (empty when the session has no shared-World
player). It is not part of the native image and is not an original save format.

| Field | Type | Meaning |
|---|---|---|
| `position` | 3 numbers | The player object's native world position (`+0x10..+0x18`, display floats). |
| `facing` | integer | The player's 16-bit facing (`+0x38`). |
| `hp_raw` | integer | The player's current hit points as the raw 32-bit float pattern (`+0x228` and the record's `PLAYERS+0xac`). |

Restored on load (`fight_restore_player`): position and facing are written to
the player object and its grid cell, and `hp_raw` to the object and the record.
**Not saved or restored:** enemies, loot and every other active object (they
are rebuilt by the level factory), the player's animation state, and any
inventory or equipment (none is modelled; see below).

Load refuses, with a visible status and no state written, when the companion is
absent or its SHA-256 differs from the manifest, is longer than 4096 bytes, is
not a JSON object with a three-number `position` and numeric `facing` and
`hp_raw`, has a non-finite position, a facing outside 0..65535, or `hp_raw`
outside 32 bits or not a finite positive float no larger than the character's
max hit points. The position is not checked against the level's bounds (no
bounds contract is bound in the host). The manifest is not signed, so a
self-consistent edit of companion and manifest is accepted if it passes these
checks.

### Limits of the fight session that these files do not hide

- **Starting equipment: `refused_unknown`.** The research does not establish
  the item a new class 6 / model 11 character has equipped on the normal route
  (`player_giveStartingItems` 0x1acc78 is not ported). The player keeps weapon
  class 5 (unarmed) with no equipment as a host stand-in, counted in
  `fight_refusals` and reported as `fight_starting_equipment` /
  `fight_player_weapon_class_source = "unarmed_fallback_standin"`. Damage,
  `hp_change` and `enemy_damaged` in this mode are not campaign evidence.
- **Camera inputs are host-sourced.** `net_getCamYaw` is the Godot sandbox
  camera's yaw, so movement is relative to the host camera, not
  `Camera_getYaw`. `net_getCamFlag` is 1 only while a script dialog is open;
  the inventory, shop and cutscene camera modes are not covered. Both are
  counted in `fight_refusals`.
- **Boar sight is refused.** The `force` argument of slot 25
  (`Creature_canSeeTarget`) is not documented for WarBoar, so the call is not
  made and each boar's state-0 AI refuses with a label. The state-0 entry age
  behind `+0xf4` (entry + 180) is also Unknown; the registration frame stands
  in only to decide when that refusal applies.
- `fight_place_player` (and the probe's `--near` / `--weak`) are diagnostics,
  not original routines; ordinary play never calls them.
