# Combat updates owned creature HP

The RTA `combat_creature::Adapter` connects the verified combat damage callback
with the verified base `Creature_takeDamage` body. It owns a combat World, stable
combat records and sparse creature memory. A later hit can observe the HP written
by an earlier hit. Pending damage, accumulated flags, hit angle/frame and visual
arc persist in backing memory. No host floating-point arithmetic enters this
composition.

The contract combines immutable `ref_core.py` at `41e847af5360724ab62d208594f8027ea3c1b686`,
`ref_combat.py` at `7212ecfecbeae8dd2015f6c82897226f49a94695` and
`ref_creature.py` at `a0d89502ce1431375a6fc045b91a73039114e6b9`.
Creature behavior is specified in `docs/spec/creature-tick.md` section 12.1 in
the research repository. The fixture manifest independently pins those sources,
the specification and this authored composition policy.

## Storage and dispatch

An explicit binding associates each stable owned record with a nonzero numeric
32-bit address, a distinct allocation generation, projection choice and dispatch
choice. There is no removal or address reuse. Base Creature dispatch requires the
documented scalar projection. Custom dispatch can opt out of that layout and
supply its own handler. This selection is explicit; the adapter does not infer
virtual methods from an RTTI label.

At each base HP callback, the adapter projects flags, position, creature flags,
HP, five status timers and the supplied world frame. It publishes the scalar
fields back for all projected records after normal return and partial failure.
Synchronization happens for every actual HP callback, including reflected hits
and DOT's accumulated hit followed by its ordinary zero hit. Player state and
`stamp_f0` are unrelated fields and are not mapped onto Creature state or hit frame.

Raw memory is authoritative during the supplied HP callbacks. Callback wrappers
publish it before and after callback execution. Nested damage begins from the
current published state. Direct edits to projected typed fields during a raw
callback are outside the supported contract; use raw memory for those changes.
Pointer observations and unprojected record fields remain explicit host data.

The projected domain requires flags and world frame representable as unsigned
32-bit values, exactly three position components and five status values. Wider
flags and negative frames are explicit adapter refusals. Record spans, raw-region
reservations and the frame word must not overlap. Caps cover 1,024 owned entries,
1,024 raw regions, 64 operation/callback levels and the HP module's memory/history
limits; callers can select stricter limits. Combat's public lists and arbitrary
callback execution remain trusted and caller-managed. Mutable World/Model views
must not replace the installed dispatch/callback wrappers or destroy the running
adapter. There is no general callback CPU/allocation guarantee.

## Handle boundary

Owner, effect-source and last-attacker handles require caller-supplied backing
and handlers. The before-base hook can publish handle state changed by combat
status effects. The after-base hook can expose derived observations. Neither
hook invents an object behind an unknown address. A null head, a nonzero node
with a null target and a stale nonzero target remain distinct.

Public tests use authored handle recipes to check the composition. These recipes
are not the original handle allocator. The adapter has no shared object-World
handle pool, destroy semantics, iterator clearing or original address-reuse
behavior. Numeric regions and record identities must be registered consistently;
trusted raw-memory callers must preserve that domain. A separate pool cannot
establish shared World lifetime compatibility.

## Verification

The corpus has **1,152 cases**: 36 recipes, sixteen FPU settings and two variants.
Each case runs twice in fresh Python model instances. Independent root regeneration
produced byte-identical gzip and manifest files. All native cases and **281 API
checks** pass both optimized and under Apple Clang ASan/UBSan, with all eight used
runtime modules and both test/trace entry points instrumented. Eight separately
authored pinned-model anchors also verify the main branches.

Actual complete native trace SHA-256:
`a364b400cf47df4452e3c62121c4ff2c585d349e315b3102db55c7e7738cb3e1`.
Uncompressed vector SHA-256:
`d4425665efe04c86a3113259ac08aadc6927487b2d8cbb67380e483797ff7118`.

All **50 local CTest cases passed** in 360.14 seconds. Python discovery ran 178
tests with 63 explicit native-binary/platform skips; dedicated CTest supplies the
native executables. The 42 provenance, fixture and simulation-boundary checks pass.
New transport verification ran twelve checks on macOS with one explicit unavailable
`/dev/full` skip; Unicode paths, aliases and real output-limit failure were exercised.
The four new C++ files pass LLVM 20.1.8 formatting. The simulation boundary now
covers 40 compiled sources/headers; no host floating-point or presentation types
enter it.

Platform CI mandates 21 suites, totaling **1,414,855 records per host**, with
actual result bytes checked against independent expectations as well as peer
agreement. Platform run links and final results are recorded on the implementation
pull request. The evidence and implementation are one commit.

The Python generator composes the pinned reference models rather than deriving
new game behavior. Native execution must independently reproduce complete owned
records, numeric bindings, sparse bytes, ordered writes/calls and both RNG
histories under all sixteen FPU configurations. Cross-platform CI requires the
actual result bytes and provenance to match on both Mac architectures, both
Windows architectures and Linux x64.

## Remaining work

Session installation, shared object lifetime/handle backing, movement, derived
creature/player dispatch, death, loot and a controllable scene remain separate
work. RTA model composition does not establish Norrath behavior, a playable game
or compatibility with an original console peer. Original content stays private.
