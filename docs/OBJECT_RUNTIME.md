# Return to Arms object runtime

The portable `champions-objects` library ports the published object-system Python
contract. It supplies base-object lifetime, names, grids, activity queries,
run-list order, task-table mutation, reaping, visibility, supplied level records
and start-marker rules. These game rules also run through the optional
[Session adapter](SESSION_OBJECTS.md), which owns the World and connects ready
network frames to its scheduler. Derived class logic and engine callbacks remain supplied inputs.

## Contract and provenance

The implementation follows immutable research
[`ref_objects.py`](https://github.com/Open-Champions/champions-return-to-arms-re/blob/0dd02ce416389dd440540119727d946636507703/tools/ref_objects.py)
and its [object-system specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/0dd02ce416389dd440540119727d946636507703/docs/spec/object-system.md).
Source SHA-256: `541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2`.
It reuses the shipped core contract at `41e847af5360724ab62d208594f8027ea3c1b686`,
SHA-256 `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123`.
The two-entry canonical reference map hashes to
`5b5c69873f8a42e0a184d22e21b0d49cc449ceece3c1e95686bca52b1df22c12`.

`tools/generate_objects_vectors.py` reads and verifies those Git objects,
executes authored scenarios against the Python models, and emits reproducible
asset-free expected vectors. The native trace runner independently executes the
same inputs and compares its own resulting state. The registry checks the whole
reference map and `rta-objects-runtime-v1` fixture policy. Platform comparisons
require complete coverage and the expected raw records, not merely equality
between hosts.

The research specification reports comparisons with an instruction interpreter
against the original executable. This port's evidence is Python-contract parity;
it does not repeat or extend that original-executable proof or establish console
behavior in the last floating-point bit. Norrath remains independently unvalidated.

## Ownership and callback boundaries

Worlds have stable heap-backed grids and owned object storage, including retired
allocations. The grids avoid the default Windows stack limit. Allocation
serials distinguish different objects that reused a virtual pool address. Handle
nodes and list heads retain modeled numeric identities; host pointers do not enter
trace records. Worlds cannot be copied or moved. Snapshots copy values, not
closures; shared property and class-definition aliases receive stable first-seen identities.
Completed iterators are released when the graph no longer references them.
Active, failed and stale referenced sentinels remain owned. Serial counters are
trace instrumentation, not newly recovered retail globals.

The default callback policy requires supplied handlers where modeled code invokes
derived object execution/destruction, frozen-state evaluation, resource resolution
or end-of-tick work. Explicit `ReferenceBase` selection enables the Python model's
base object and stub semantics for authored tests. It does not implement derived
classes, terrain, draw tasks, resource loading, engine natives or gameplay.
Terrain and factory answers are explicit callbacks. Supplied record and
class-definition structures must stay stable during a callback; unsupported
structural replacement is refused. Ordinary object, list and task mutation
remains part of the modeled execution. Handlers are copied for invocation so they
can replace their registered slot safely. Mutable gameplay state must live in World objects or explicitly owned
host state referenced by the handler; value captures are immutable inputs.
Mutable state kept solely inside a copied C++ functor is not preserved between
calls. Executable closures and captured external state remain outside the
snapshot and parity proof.

Gameplay operations use the raw-pattern core arithmetic under its selected FPU
configuration. Viewer arithmetic and Godot dependencies are excluded by the
simulation boundary test. Standalone calls use the caller's core configuration;
the Session adapter scopes it explicitly and restores the caller's configuration.

## Explicit native bounds

The wrapper permits up to 65,536 retained object allocations, owned extra list
heads and retained iterator records each, with a million steps per traversal.
Use a fresh World at the allocation-history boundary. The modeled task table has
32 entries. Authored record builds are bounded to 32,767 records, 32,767 bytes per
record and 16 MiB overall; property/text storage also has explicit bounds. Extra
list-head IDs start at `0x90000000`. A sphere-result list must be owned by its
World. Capacity and ownership refusals are native policy, separately checked
from Python expectations; they do not assert retail behavior outside that scope.

Callback exceptions propagate and retain prior mutations. The World itself does
not roll back or automatically retry. The Session adapter uses the existing
failure latch rather than resubmit a partially completed tick.

## Verification

The implementation includes 286 API, ownership, mutation and failure checks and
1,216 exact Python-generated state records. Both pass in optimized builds and
with the complete object module under AddressSanitizer and UndefinedBehaviorSanitizer.
The vectors cover 19 scenarios, four input seeds and all 16 core FPU configurations.
An independent regeneration reproduced the committed compressed fixture and
manifest byte for byte. Uncompressed fixture SHA-256:
`324d0a956ee08ec409ab5742348021c526dba3657240936fabcf123a5b1ef8ee`.
Expected native trace SHA-256:
`610447d8816f84d94f4b9466aeac16852352d1915ab54848909eeb0c39404a3c`.

Local verification covered all 41 native CTest cases. After fixing replay output
failure handling, all five affected session/object cases were rebuilt and passed
again; the other 36 had already passed. Python discovery passed 130 tests with
28 explicit skips for tests requiring a native binary or platform-specific I/O.
The dedicated native transport cases supply those binaries. Output errors,
malformed or incomplete inputs, duplicate cases, Unicode paths, same-file and
hard-link output refusals are exercised. Both replay tools now check each write
and requested final flush, including the POSIX file-size-limit failure path.
The simulation boundary and trace-integrity checks passed, and the C++ changes
pass the repository's clang-format configuration.

The platform gate requires all five native hosts (macOS arm64/x64, Windows
arm64/x64 and Linux x64), all five Godot regression exports, and complete exact
trace comparison against each fixture's pinned expectations. It contains 18 game
system suites, including this object runtime. CI results are linked in the
implementation pull request; unexecuted platform checks are not local evidence.

No assets, original code dumps or executables are part of the public vectors.
A passing object model or session test does not yet provide a controllable game,
original-client networking or Norrath compatibility.
