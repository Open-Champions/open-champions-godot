# Creature hit-point updates

The standalone `champions::simulation::rta::creature_hp` module ports the complete
`Creature_takeDamage` virtual slot 2 (`0x116638`) from the published RTA US v2.00
Python model. It uses the existing raw-pattern arithmetic and 16-bit-angle
`atan2_16`. No host floating-point arithmetic or presentation dependency enters
the simulation target.

The immutable contract is research commit
`a0d89502ce1431375a6fc045b91a73039114e6b9`, `tools/ref_creature.py`
(SHA-256 `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7`),
with `docs/spec/creature-tick.md` section 12.1. Its core dependency is the already
validated model at `41e847af5360724ab62d208594f8027ea3c1b686`. The fixture
manifest pins both source identities and the specification independently.

## Behavior and ownership

The function skips inert creatures. Otherwise it writes the visual arc first.
IgnoreDamage creatures then clear five timers in the original order and return.
Normal damage stamps the frame and accumulates flags. With an attacker and
positive HP, it computes the angle, clears the last-attacker handle, resolves
projectile or effect ownership, and links the resulting nonzero raw address.
Finally it subtracts the amount from the HP value currently in memory and adds
it to the current pending amount. Negative damage and nonpositive HP follow the
model; no invented clamp or live-object validation is added.

Handle callbacks operate on the same owned sparse little-endian memory. Their
mutations are visible to subsequent reads. Exceptions propagate and retain all
changes already made; this standalone method has no rollback or failure latch.
The logs retain model writes, memory stores and ordered callback activity. The
unsigned 32-bit virtual addresses are authored memory identities, never host
pointers. Missing bytes read as zero without inserting entries.

The Model cannot be copied or moved; Memory value copies remain available.
Trusted callbacks must not destroy the executing Model. The registered callable
is copied before invocation to permit self-replacement; keep authoritative callback
state in Model memory or stable reference captures. Mutable by-value callable
identity is outside the state comparison.

Production callers must supply handle handlers. `RequireHandlers` is the default;
`ReferenceZero` explicitly enables the reference model's missing-handler behavior.
It does not implement the original handle lists. Resource and address refusals
are authored host safeguards, independently tested rather than attributed to
the unbounded Python memory model. Hard caps are 1 MiB of sparse bytes,
65,536 entries per history and for combined model events, 64 callback bindings
with names of at most 128 bytes, and 64 nested damage calls. Callers may select
stricter Limits. Memory widths are 0–8 bytes with checked unsigned 32-bit spans;
there is no external memory overlay. These guards apply to runtime operations;
trusted callers must manage public callback and history containers within the
API contract.

## Exact validation

`tools/generate_creature_hp_vectors.py` loads and hash-checks the immutable models
through Git. Public fixtures contain only authored memory and inputs. Two fresh
Python executions per case must produce identical complete memory, write and call
traces. Native execution reconstructs those cases and independently computes the
results under all sixteen arithmetic configurations. CI requires the actual
result bytes to equal the pinned Python expectations on Apple Silicon and Intel
macOS, ARM64 and x64 Windows, and Linux x64.

The corpus has **2,048 cases**: 32 scenarios, sixteen FPU settings and four
variants. Independent complete regeneration produced identical fixture and
manifest bytes. All 2,048 actual native result records match the oracle both
optimized and with the creature runtime, core arithmetic and core math compiled
under ASan/UBSan on Apple Silicon. The 250 focused API checks pass in both builds.
This includes ordered partial changes on callback exceptions, stale raw targets,
source precedence, zero/missing memory and independently tested host limits.

Uncompressed fixture SHA-256:
`ae97551469054f1b14e5c58f75fc6544245a8498b2c1908a8b7c03ea3dc724ab`.
Actual complete native trace SHA-256:
`09cf373e46a71dfb6bcb333cb17359164228b11ee63d332839626ac4d27aa99d`.

All 47 local CTest cases passed. Python discovery ran 160 tests with 50 explicit
native-binary/platform skips; dedicated CTest supplies the binaries. The 37
artifact, boundary and runner checks passed. The new transport suite ran ten
checks on macOS, with its unavailable `/dev/full` device check explicitly
skipped. Unicode paths, hard links and actual file-size-limited output failure
were exercised. The four new C++ files pass the repository formatting policy.
The platform gate now mandates twenty game-system suites, totaling 1,413,703
records per host, and checks provenance and exact expected bytes as well as peer
equality. Platform CI results are recorded on the implementation pull request.

## Remaining integration

The function is independently callable. The
[owned combat adapter](COMBAT_CREATURE.md) now supplies stable records and
persistent storage through an explicit composition. Session installation and
shared object lifetime/handle backing remain work.
Session currently owns a combat RNG, without a combat World or stable creature
record arena. Object snapshots reject foreign object pointers, while this method
can retain stale raw attacker addresses. The next adapter must prove stable record
bindings, partial-state publication and a shared handle backend; it must not
replace those addresses with null or invent an alive check.
The standalone combat callback remains caller-supplied; installing the owned
adapter connects its base Creature dispatch to HP updates. Death
processing, movement, subclass behavior, loot and experience call-outs remain
separate work. This RTA contract establishes neither Norrath behavior, a playable
scene nor compatibility with an original console peer.
