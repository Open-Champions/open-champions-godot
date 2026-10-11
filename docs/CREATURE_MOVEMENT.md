# Original RTA creature collision step

This standalone portable module implements `Creature_moveBy` (0x113c48), its water
bookkeeping, `Creature_settle` (0x119e38), the object-local random draw and the base
contact dispatch that these bodies reach. The behavior contract is the published
Python model, rather than a fresh interpretation of decompiled code.

The namespace is `champions::simulation::rta::creature_movement`. Its raw-memory
addresses describe the pinned RTA US v2.00 model. They are not host pointers,
retail-file fingerprints or evidence that Norrath shares these rules. It reuses the
bounded sparse memory implementation of [creature HP](CREATURE_HP.md).

## Contract and ownership

- Core: `tools/ref_core.py` at `41e847af5360724ab62d208594f8027ea3c1b686`,
  SHA-256 `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123`.
- Creature: `tools/ref_creature.py` at `a0d89502ce1431375a6fc045b91a73039114e6b9`,
  SHA-256 `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7`.
- Specification: `docs/spec/creature-tick.md` at that creature revision,
  SHA-256 `bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e`.
- Reference-map identity: `44e47186b857b04c9295b480cea89369af4697e2467e650aa6d20773f9ad8dfb`.
- Fixture policy: `rta-creature-movement-v1`.

These documents and models live in the
[research repository](https://github.com/Open-Champions/champions-return-to-arms-re).
The generator loads immutable Git objects and verifies every hash. It does not
import mutable research files. No research or game data is copied into this module.

The caller supplies raw creature/object memory and explicit terrain, object-grid,
water, resource and sound callbacks. Missing handlers refuse by default. A separate
reference-zero policy reproduces the model's absent-callback behavior for analysis.
This is not a runtime world implementation or an automatic object allocator.

Movement consumes root X/Y and yaw before the collision step, honors the signed
root timer, and preserves the order of contact callbacks, radius rereads and
position restoration. An entering-water draw updates the creature's own `+0x44`
seed; it does not advance the shared world seed. Base slot 6 sets the contact bit
for a player touch, while base slots 10 and 11 are empty. Subclass callbacks remain
explicit, including signed virtual-table this adjustment.

Simulation uses integer-backed RTA arithmetic throughout. No viewer binary64 path,
Godot physics, host normalization or wall-clock delta participates in this body.
Memory, callback histories and failures are observable. A failed operation retains
its executed prefix and does not add rollback, automatic retry or a failure latch.

## Supported boundary

Callback results retain owned integer, three-word vector, list, tuple or byte-buffer
values. The collected object list has at most 16 raw32 addresses. The initial slide
returns a hit integer and a normal vector. A no-hit result may retain an absent
normal; it is never consumed. A hit requires a vector. Water truth and terrain
height return integers. The module refuses unsupported shapes rather than inventing a result.

Raw-memory spans stay inside unsigned 32-bit address space; only the virtual this
adjustment wraps explicitly. Limits may be lowered for a caller, never raised past
1 MiB of sparse bytes, 65,536 history entries, 64 callbacks or 64 nested operations.
Callback values separately cap sequence length at 16, nesting at four levels, total
nodes at 64 and total byte buffers at 4 KiB. These are native resource policies,
not additional behavior attributed to the original game. Malformed graphs, shapes
and oversized work have native refusal tests outside the Python parity domain.

## Remaining integration

The [controllable scene](SESSION_CONTROL_CREATURE.md) now explicitly selects the
dry singleton creature policy. Its keyboard mapping remains authored; existing
native callers still default to the earlier isolated movement policy.
The [dry singleton Session adapter](SESSION_CREATURE.md) now binds this body to
the same owned World, terrain Level and HP Memory on ready frames. Its borrowed
Model constructor keeps the existing Memory and overlay by reference; the owner
must outlive the Model. Session retains that ownership internally. The adapter
uses fixed terrain/grid services and explicit authored base-table storage; it
does not install water or dynamic-contact services. The Godot bridge now selects
it for initial construction and terrain replacement, with a 512-ready-tick cap. Original `Player_move` is still a delegated callback in the published
player model. Animation root generation, water geometry, complete levels, original
player controls and Norrath behavior remain separate work. Exact model parity does
not establish a playable game or original console network compatibility.

## Validation

Local validation on macOS ARM64 used the frozen implementation and final typed
callback fixture:

- Full native build and all three affected CTest cases passed.
  The API executable reported **525 checks**; the independent trace matched
  **1,472 cases**, 46 recipes × two variants × all 16 arithmetic settings.
- Address/undefined sanitizers passed those API checks and all final exact and
  transport cases. Compile commands independently confirm all five linked runtime
  units and both entry points were instrumented. macOS leak detection was disabled;
  this is not a leak-detector result.
- Three final-source oracle generations produced byte-identical gzip and manifest
  pairs. Seven failure-injection tests cover atomic fixture publication. Callback
  tuple/list/buffer distinctions and the no-hit absent-normal result are retained
  in calls, returns and ordered events.
- The Python suite reported 345 tests, including 183 explicit native-path/platform
  skips; the dedicated CTest transport case supplies the native executable.
- LLVM 20 formatting passed all previously tracked `native/src` files and all four
  newly authored C++ files. GCC 15 syntax checks passed the runtime and both entry
  points. The simulation boundary checks all 52 supported source/header paths and
  rejects host floating-point or presentation dependencies.
- Independent reviews checked the body against the pinned contract and the oracle
  against the independently executed native replay. No unresolved semantic or
  provenance blocker remained.

Final generator SHA-256:
`f9ec68ce16c0ff870d6dd2794e56b48f8608431a9c91ec496b4116fe0c7a9f9c`.
Fixture gzip SHA-256:
`9f766a416619916519566d68ba7e5fb157c9883dbaef583bb909ba77b1dc9157`.
Manifest SHA-256:
`186064a2793c58a3375e924e3301ff2ef9dd6ddfbe71e1d8561b8396255e55bb`.
Expected/actual trace SHA-256:
`6b24421c853e915e12068af52d58befae4ad8841ae90407193ef4a5add9d23d7`.

The transport caps authored input at 32 MiB and one line at 64 KiB (the corpus is
6,114,217 bytes; its longest line is 5,888 bytes). It rejects partial/duplicated or
modified records, malformed identities, non-ASCII/CRLF input, oversized work,
missing files, write failures and output aliases. Fixture creation uses fsynced
sibling temporary files and publishes the manifest last. A failure between the
two replacements leaves a detectably stale pair; this is not a two-file transaction.

Platform CI requires all **74 native CTest cases** and **29 exact suites** on
Apple Silicon/Intel macOS, ARM64/x64 Windows and Linux x64. The comparison checks
actual output against Python-derived expectations, so identical wrong results on
all hosts fail. Missing hosts, changed model/spec hashes, stale source/manifest
identity and incomplete coverage also fail. Executed target-platform results belong
to the implementation PR and its exact source head; local results above do not
stand in for those runs.

No private retail asset is required or newly validated by this memory/body port.
The previously selected owner collection contained no recognized WORLD files;
terrain parsing and presentation still need independent suitable real-file evidence.
