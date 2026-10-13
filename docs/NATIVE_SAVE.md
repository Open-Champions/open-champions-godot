# Authored native scene saves, version 1

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
Every authoritative mutation is behind the facade. It exposes copied observations
and snapshots, never a mutable Controller, Session, World, terrain or HP reference.
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
atomic file writing and save-picker presentation belong to a future host adapter.

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

Godot bindings/UI are intentionally not part of this native change. Saving an
existing Godot scene requires adopting this facade and exposing thin byte/file
bindings. This does not complete the visual scene's save/reload acceptance gate,
PS2 save compatibility or a playable-port milestone. Windows and Intel Mac
hardware execution remain separate from local arm64 and Rosetta checks.

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
