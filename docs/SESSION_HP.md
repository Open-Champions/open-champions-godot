# Persistent base-creature health in Session

This RTA-only composition connects the existing base Creature damage body to
Session's owned object World. One combat storage Adapter persists across ready
frames and reads the **same** World handle pool through RawHandles. Original
objects, allocation serials and physical addresses remain separate identities.

This is an explicit host policy built from the pinned core, object, network,
combat and creature contracts. It does not recover an original attack task,
creature AI, derived/player dispatch, movement, collision, death, loot, or a
playable game. Norrath behavior needs independent evidence.

## Installation and execution

Enable the object runtime first, then install health during setup. Bind each
supported base creature by its current World allocation serial and address;
the actual allocation must have room for the documented fields. Initial HP,
creature flags and status values use integer bit patterns, never host floating
point. No generic GameObject is silently treated as a creature.

Register an explicitly named numeric scheduler task after priority-10
`runObjects`. That task supplies base `take_damage` inputs on actual ready
simulation frames. A render submission is not itself a damage tick. Pending
levels, missing remote input and `no_sim` retain health until a task executes;
held backlog and frozen object updates follow the existing scheduler semantics.
Frozen object updates do not automatically suppress an independently scheduled
damage task.

The raw view requires an encodable, quiescent graph. Calls during object-list
iteration refuse rather than inventing a raw iterator address. Normal post-run
operation, failure prefixes and diagnostic availability require their own tests.

## State ownership and failure

Between HP calls, World flags, position and frame are authoritative for shared
fields. Entry projects those fields into the persistent combat records. HP,
status, pending damage and timers retain their prior values. During the damage
body, raw memory is authoritative; success and failure publish scalar changes
before Session latches a failure. The fixed clear/link callbacks mutate the
existing pool. Arbitrary HP callbacks and mutable Adapter access are excluded.

Bindings are generation-qualified. Reuse of **any** registered address by a
replacement object refuses before projection, even when another creature is
the victim. This first gate does not remove or rebind combat records. Destroyed
objects and retained raw bytes remain diagnostic state.

The base HP body draws no randomness. World/shared network, Session combat, and
Adapter combat/cosmetic streams remain distinct. Full combat damage needs a
later explicit combat-stream authority contract; copying seeds does not prove
that two users share the same generator.

Snapshots copy Session, network rings, object state, health bindings, combat
records, sparse backing, attempted stores, ordered writes/calls, seeds and the
raw registry. Physical graph observation is separate from backing bytes. If a
failed graph contains an unsupported iterator, preserve its semantic World
state and registry with an explicit physical-view error. A wire hash alone
cannot establish health equality. Failure keeps completed effects and refuses
retry; there is no rollback.

## Verification contract

The independent generator loads five immutable Python model blobs, checks their
SHA-256 values before execution and invokes their original bodies. Authored
snapshot helpers are separately pinned. Each expected case runs twice from
fresh model instances. Native traces must match every generated state record,
with arithmetic cases under all sixteen configurations. Runtime snapshots retain
exact diagnostic wording. Cross-language replay compares physical-view availability
and the `physical_unavailable` category; wording of that error is normalized.
Semantic graph, registry, backing and scalar-publication errors remain compared.

Local checks include literal multi-frame health anchors, ready versus blocked
frames, scalar publication, object destruction/address reuse, real shared-pool
exhaustion, partial failure and owned snapshots. Five-host CI compares complete
actual traces against the committed Python expectations. Endpoint exchange is
separate from single-session parity; neither establishes compatibility with an
original PS2 client.

The committed corpus has 308 cases in eighteen scenario groups. Its uncompressed
size is 56,285,567 bytes, with a 592,480-byte maximum line, within the existing
64 MiB input and 1 MiB line budgets. It stores a complete initial snapshot and
lossless replacements for later states; the generator reapplies every replacement
and compares the reconstructed complete state. Three independent generation
processes produced byte-identical gzip and manifest files. The pinned models,
authored helpers, ordering evidence and specification are listed in
`native/tests/vectors/session_hp.json`.

Local Apple Silicon verification passed all 56 CTest groups: the 53 existing
groups in 317.04 seconds and three new groups in 123.86 seconds. The 255 API
checks include actual packet exchange between opposite endpoints, literal HP
and RNG anchors, active callback observations and failure prefixes. All 308
optimized native traces matched the independent Python expectations exactly:
`513a1280e7b95244dff06bbbb2632fb68cad55e00f72267d26d6e8bfa06d024e`.
Python discovery passed 216 tests with 93 explicit skips; dedicated CTest cases
supply native executables for their transport tests. The simulation boundary
check covered all 42 source/header files, and all eight changed C++ files passed
LLVM 20 formatting checks. AddressSanitizer and UndefinedBehaviorSanitizer
passed the 255 API checks with all 23 linked runtime source modules instrumented.
The fully instrumented 308-case replay also passed and produced byte-identical
traces to the optimized build and Python expectations. Leak detection is
unavailable on this macOS host.

GCC 15 syntax checks passed for the changed game-system translation units and
both new entry points. Cross-platform CI caught and corrected a missing direct
`<optional>` include that Apple's standard library had supplied indirectly.
