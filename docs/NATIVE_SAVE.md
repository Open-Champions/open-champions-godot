# Authored native scene saves

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
