# Explicit Session turning

An HP-bound actor can now invoke original `Creature_turnToward` from an explicitly
registered post-`runObjects` ready task. Previously only the standalone operation
was available. This connects the existing stable World/HP identity and shared
memory to the original spring, clamp, slow and facing stores. The task order is
authored Session policy; it does not establish original player or AI dispatch.

`enable_turning` enables a bounded adapter, `bind_turner` supplies a turn rate,
angular velocity and slow byte for an existing HP identity, and `turn_toward`
returns the original signed pre-turn angle. A task must have priority above 10
and execute on an actual ready frame after object processing. Each binding may
turn once per ready tick; the supplied aggregate limit is at most 64. Terrain,
movement callbacks and a singleton World are unnecessary for this operation.

The adapter borrows the HP owner's authoritative Memory and store log. It imports
only the selected World XY into both typed HP XY and raw XY, preserving typed,
raw and World Z independently. It imports World facing, then publishes only raw
facing back to that actor after success or an executed failure. It does not
project every actor, rewrite the raw global frame, reset a random stream or
replace the shared HP object. Angular velocity, slow and turn rate persist in
that same memory. Destruction releases the borrowing turn model before its owner.

Binding checks identity, generation, scalar ranges, managed overlay conflicts and
unique byte capacity before writing. Calls preflight aggregate history capacity,
preserve ordered original stores and accepted-operation inputs/results, and latch
the Session after failure. An underlying shared Memory-log failure retains the
original attempted Model write/event without inventing a successful Memory store.
Complete copied snapshots include all 20 declared turning fields, including
stale bindings, counters, histories, outputs and failure/publication context.
Older complete observers explicitly refuse active turning rather than omitting it.

The immutable behavior contract is `ref_creature.py` at
`a0d89502ce1431375a6fc045b91a73039114e6b9` (SHA256
`debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7`),
with `ref_core.py` at `41e847af5360724ab62d208594f8027ea3c1b686` (SHA256
`0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123`).
The manifest separately pins original combat, objects, netwire, ordering/spec
evidence and unchanged snapshot helpers. Python executes those original bodies;
the native runner interprets the supplied input DSL independently.

The canonical 264 cases cover 16 arithmetic/interleaving groups across all 16 FPU
settings and 8 structural groups once. They observe HP before/after turning,
multiple actors, persistent raw rates, delayed/held/no-simulation readiness,
destruction/reuse and failure prefixes. The network fixture exercises an original
remote packet producer and local ready machine; it does not prove agreement
between two complete peer worlds. Three fresh generations reproduce both fixture
and manifest bytes, and seven atomic publisher tests pass.

Optimized and ASAN/UBSAN actual native traces match every original Python expected byte:
SHA256 `3ebe11635a9ae54c218ca397d7535e1723b4e6d3f4af95fb3db2a94a26c4ce04`,
FNV1a64 `b070379068af4fe1`. All 27 linked runtime units are instrumented;
422 API checks pass in both builds, including exact unique-byte binding capacity
and a shared HP store-log failure prefix. Leak detection is disabled on this Mac,
so no leak claim is made. The 20 transport tests pass in 270.486 seconds, with one
explicit unavailable Mac facility skip and an actual POSIX file-size failure. The 613 Python tests pass
with 343 explicit skips; 118 core integrity tests include every turning snapshot
cell omitted from unanimous refreshed synthetic reports. GCC15 strict syntax,
LLVM20 formatting and the 56-path simulation boundary pass. Fresh universal2
debug and release bindings build; the actual selected debug binding passes all
70 source scene and 283 browser checks. All 100 distinct CTest cases pass in one
full 2036.23-second regression run, including all prior published traces, asset
checks and the new turning gates.

Local reports use an explicit uncommitted source label; final-head
platform checks and independently inspected actual artifacts remain merge gates.

The native CI job limit increases from 60 to 90 minutes because PR69's Intel PR
job reached its former limit after 77 passing tests, and a same-head retry passed
all 97. The added exact/transport checks retain every existing gate.

No controller, scene, automatic AI/player, animation-cache, ROOT/YAW extraction,
pose or sound integration is added. Water services, moving pieces, dynamic
contacts and animation-generated root motion remain separate evidence gaps.
This is RTA evidence, not Norrath validation, retail playability or PS2 networking.
