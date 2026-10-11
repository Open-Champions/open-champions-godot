# Session-owned dry creature movement

The RTA Session can now run the original modeled `Creature_moveBy` body against
its existing World, terrain Level and persistent health memory. An explicitly
registered ready task supplies raw velocity. The Godot movement test now selects
[this API through its explicit DryCreature controller policy](SESSION_CONTROL_CREATURE.md).
Keyboard mapping remains authored; the native Controller retains its earlier
isolated-object policy as the default for existing callers.

## Contract and ownership

The composition policy is `rta-session-creature-v1`. The original movement body
comes from `tools/ref_creature.py` at
`a0d89502ce1431375a6fc045b91a73039114e6b9`, SHA-256
`debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7`.
Its scalar arithmetic uses the pinned [core contract](CORE_SIMULATION.md).
The oracle also executes the immutable original network, object and terrain
models. Exact source, specification and authored-helper identities are recorded
in [the fixture manifest](../native/tests/vectors/session_creature.json).

This adapter adds explicit ownership, binding, isolation and failure policies;
those policies are not attributed to the original game. Ready-frame assignment
also uses the separately identified ordering evidence in `docs/mainloop.md` at
`0dd02ce416389dd440540119727d946636507703`.

Setup requires the existing [object World](SESSION_OBJECTS.md),
[HP adapter](SESSION_HP.md) and [terrain Level](SESSION_TERRAIN.md).
`enable_creature_movement()` installs four fixed services.
`bind_creature_mover(hp_id, initial)` binds one HP generation and initializes
explicit root X/Y, root yaw, root timer, phase and inert fields.
The standalone movement Model borrows the **same Memory object** as HP; there is
no copied backing image, second World, second handle pool or second terrain Level.
The standalone borrowed constructor requires its Memory owner to outlive it;
Session enforces that destruction order internally.

`CreatureInitial.base_table` is a required, nonzero caller-supplied address for a
128-byte authored dispatch region. Binding checks it against World pool arenas,
managed handle storage, globals, existing raw reservations and backing bytes.
Only the zero adjustments and published base entry tokens for slots 6, 10 and 11
are written; untouched table bytes remain absent from sparse backing. The raw
vtable pointer and World vtable agree. This is explicit host setup, not an original
constructor implementation or a claim about a retail table address.

## Supported calls and order

`move_creature(hp_id, raw_velocity)` is allowed once per actual ready tick, in an
explicit task with priority greater than 10 after runObjects returns. No-ready,
level-pending or no-simulation submissions do not manufacture movement calls.
A frozen runObjects may return and allow the separate authored task to execute.
Nested movement, message/object callbacks and simultaneous isolated-mover binding
are refused. Setup freezes on the first submission.

Each call revalidates the current allocation and generation and requires exactly
one live retained World actor, including off-grid and hidden objects. It requires
a base nonplayer creature, unchanged authored dispatch, representable scalar
fields and the dry `cflags & 0x100000` domain. A recycled address never silently
selects another object. The actual collector is still used; only self or an empty
result is accepted. These conservative restrictions do not establish dynamic
contact behavior.

World flags, position, radius, height, vertical velocity, angle and frame are
imported into the shared raw memory before the original body runs. HP, status,
root fields, phase and timer retain their existing backing authority. The fixed
services follow the body's conditional call order:

1. `world_slideMove` calls the existing Level slide kernel with the body's mask 3,
   writes its returned position into shared backing and publishes it to World.
   A no-hit absent normal stays absent.
2. `obj_updateGridCell` relinks the actual existing World grid node only when the
   body requests it; there is no unconditional second grid update.
3. `world_collectObjectsInBox` queries that same World after raw position is
   published. Contact processing retains the original base dispatch behavior.
4. `terrain_heightBelow` queries the same Level. Gravity and grounded behavior
   remain in the original creature body, rather than running another movement
   kernel afterward.

Root consumption, yaw changes, timer changes and final XYZ/vertical velocity are
published without discarding completed HP state. Dry movement does not draw from
or mirror the object's `+0x44` seed, the shared World seed or combat seeds. Session
arithmetic configuration governs the operation; no host float, wall-clock delta
or Godot physics value feeds the simulation.

## Failures, bounds and observations

Preflight refuses unsupported identity, water, dispatch or resource domains.
Once execution begins, a failure retains completed writes, Level stamps, grid
changes and callback history. Raw scalar publication is attempted on failure as
well as success; a secondary publication error is observed separately without
replacing the primary error. Session then latches failure and refuses retry.
There is no rollback or implicit repair.

The existing Memory, Level and Session limits still apply. Movement reserves 128
aggregate history entries before entering its bounded body; the shared Memory
store log is counted once through HP. Per-tick call counts reset on actual ready
ticks. Native-only malformed-state and resource refusals are tested separately
from finite Python-model parity.

The optional `CreatureMovementSnapshot` includes binding and initial values,
policy, phase, depth, per-tick count, last velocity/result, publication diagnostic,
limits, borrowed-memory identity, callback labels/presence, external overrides,
writes, calls, ordered events, successful typed callback results, draw count and
task guards. The complete surrounding snapshot retains Session/network, World and
pool graphs, HP backing and store log, and the full Level. Tuple, list and absent
callback values remain distinct. Older complete trace formats reject an active
movement extension rather than silently omitting it.

## Evidence and remaining work

The final authored corpus contains **281 full-state cases**, with movement
recipes covering all **16 arithmetic configurations**. It includes root/timer
consumption, dry gravity, slide and grid changes, delayed ready frames, frozen
objects, HP aliases, retained failures and actual packet exchange between two
native endpoint replicas. Full initial observations and lossless per-step deltas
are compared with independently executed, hash-pinned Python models; expectations
are not obtained from native output.

The final exact trace SHA-256 is
`4210779cc41e8fe0c86845822d1511e70482d0332c00cc9aca8a8986b6c7d0b0`.
Confirmed local Apple Silicon evidence:

- Optimized and AddressSanitizer/UndefinedBehaviorSanitizer runs each produced
  **281 actual output records** matching the Python expectations byte for byte
  and the exact trace hash above. Both configurations passed **382 Session API
  checks** and **27 borrowed-Memory API checks**.
- All **26 linked runtime units and three entrypoints** were instrumented in the
  sanitizer runs. Leak detection was disabled; these results make no leak claim.
- Three independent final generator runs produced identical manifests and gzip
  fixtures. All **seven atomic-publication failure tests** passed.
- The Python suite reported **371 tests**, including **198 explicit skips** for
  native-path/platform cases. Dedicated native transport checks are separate.
- LLVM 20 formatting passed all tracked `native/src` files and the owned C++
  additions. GCC 15 syntax checks passed five changed translation units. The
  simulation boundary covers **52 paths**; all **26 boundary tests** passed.
- Three independent reviews found no unresolved implementation, observation or
  model-provenance blocker.

All **78 local CTest cases** passed across the full regression and the corrected
API rerun. The initial API assertion expected the original callback name as the
wrapper phase; correcting that test expectation to `slide` left runtime behavior
unchanged. The optimized and sanitizer exact/transport cases also passed.
Final platform CI remains a separate gate: it must identify the tested
implementation head and actual artifacts. Local results and Python fixture
identity do not stand in for those runs.

Original `Player_move` and input mapping, animation-generated root motion, Session
water services, derived creatures, dynamic contacts and Norrath remain unvalidated.
The standalone module's water and settle bodies do not imply that these services
are installed here. No game assets are required by the synthetic corpus, and this
native integration does not establish a playable game or original-console network
compatibility.
