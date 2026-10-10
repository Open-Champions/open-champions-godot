# Shared RTA creature handle backing

This authored adapter connects the existing object World handle pool to an
explicit raw-memory view for the published base Creature HP function. It uses
World's actual pool; it does not allocate a parallel graph. Simulation retains
the pinned game arithmetic and stays independent of presentation.

## Contract and scope

The immutable core, object lifetime and creature HP models are the behavior
contract. The address registry and readonly memory view are authored integration
policy, not a recovered original pointer allocator. Known object identities and
opaque numeric targets remain distinct. Physical raw addresses are separate from
allocation generations and semantic snapshot identities.

Handle operations use the existing pool algorithms. Registered head and node
fields read the current graph, so a World destruction is visible on the next
memory read, even inside a damage callback. Direct raw writes that intersect a
managed graph field refuse before mutating backing memory. A store keeps its
attempted-write log, and the HP function retains its preceding write/event
prefix. Scalar HP fields and uncovered node padding use ordinary sparse backing.
Reads never materialize absent backing bytes.

A complete observation distinguishes semantic World state, address registration,
actual sparse backing, effective raw graph bytes and HP callback/write history.
Released node fields and the two padding bytes are retained. Failure to allocate
the second node in a paired link keeps the first allocation's mutations; the
adapter adds no rollback or retry.

The first supported domain excludes raw iterator addresses, arbitrary corrupted
link pointers and conflicting retired/live head aliases. Opaque object words are
retained without fabricating actor objects or replacing them with null. Typed
actor access requires a known owned identity.

## Immutable provenance and observations

The reference map SHA256 is
`13a979a1fc02c0ddefadc26efbf2b047adbd3199c7d5e94d20d18a1f10ca3967`:

| Model | Pinned research commit | SHA256 |
|---|---|---|
| Core | `41e847af5360724ab62d208594f8027ea3c1b686` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| Objects | `0dd02ce416389dd440540119727d946636507703` | `541a5d45e5121d7561dde71bb95a75269472e1dc9585498b413d39af2452e1e2` |
| Creature | `a0d89502ce1431375a6fc045b91a73039114e6b9` | `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7` |

The generator loads SHA-verified immutable Git blobs. Both dependent models use
the same verified core module. Complete World observation reuses the existing
authored object snapshot helper, SHA256
`39d6b7385eeeba2a6a431aae8dfeb573d94c7c801a16bca45672f90b17f6b6be`,
checked before import and enforced by the trace gate. The generator and the
native runner are independently authored; no native output feeds expectations.

The corpus has 258 cases across 24 scenarios. Seven HP compositions cover all
16 arithmetic configurations and two variants; arithmetic-independent handle
cases use the PS2 configuration and two variants. Each case runs twice with
fresh Python models, and two full generations plus a fresh root regeneration
are byte-identical. Cases use actual modeled World allocations requested at
`0x400`, the full 6,096-node World pool and the original construction grid pairs.

Every result contains the complete World state with reconstructible default
suppression, the explicit registry, complete node fields, opaque object words,
actual sparse backing, registered effective head reads, HP writes/calls, store
attempts and callback events. Suppressing known default graph entries does not
remove their state. Physical node addresses and known allocation serials are
recorded separately. Node offsets 18 and 19 remain ordinary sparse padding.

Exact trace SHA256:
`fdebedeeb31485a38dd9a5bde14c01c6f1f170edc3b6a5c6b55937c7871ae5a3`.
The corpus FNV1a64 is `37d934a45a894e7f`.

## Native API and resource boundaries

`RawHandles(World&, Memory&)` borrows World and memory and uses exactly
`World.handles`. Register object heads or World-retained external cells explicitly.
Implicit World heads are cached for pointer encoding; unregistered head addresses
are ordinary backing bytes, outside the claimed raw graph view. Raw `link` keeps
opaque object words; `link_known` explicitly supplies a known World object.
Repeated identical head registration is idempotent; changing its metadata refuses.
Address reuse requires an explicit dead-and-empty generation transition.

The overlay covers the 18 modeled bytes of every pool node, the free-head word
and explicitly registered four-byte heads. Managed-byte reads fail after World
or bridge destruction; a retained external lifetime token cannot make a destroyed
World valid. The bridge destructor deactivates its token without touching memory.
Explicit detach requires memory to remain alive and restores ordinary backing
access. Operations are synchronous; this API does not claim concurrent mutation.

Registry capacity is bounded at 65,536 head records, with labels of 1–128 bytes.
Registered heads cannot overlap the pool, free-head word, frame word or another
physical head. Unsupported tag values, foreign pointers, cyclic chains,
self-partners, free-node primaries/partners and inconsistent unlink relations refuse before
unsafe traversal. A supported paired-link exhaustion still performs the first
allocation before the second fails. Distinct same-head partner errors retain the
original core error point and partial prefix.

The runner accepts a regular fixture file of at most 64 MiB and lines of at most
1 MiB. It checks exact case identity, coverage and a pinned computed-result digest,
ASCII/LF input, duplicate rows, path aliases and output failures. Public fixtures
contain authored synthetic state only.

## Verification

Local validation passed on Apple Silicon macOS:

- 131 independent native API checks and all 258 complete reference cases pass
  optimized and ASan/UBSan. Every one of the eight used runtime modules and both
  entry points are instrumented. Optimized and sanitized actual trace bytes are
  identical to the committed Python expectations.
- All 53 CTest cases pass (359.22 seconds), including existing core, World,
  Session, HP and combat traces. The three affected cases were rerun after the
  final malformed-state guard and pass (28.11 seconds).
- Python discovery passes 198 tests with 78 explicit native/platform skips;
  dedicated CTest supplies those native binaries. Fourteen native transport
  checks pass; unavailable `/dev/full` is explicitly skipped on macOS.
- 27 artifact/provenance tests, 17 simulation-boundary tests, LLVM20 formatting
  for all ten touched C++ files and the 42-file simulation dependency check pass.

Platform CI is required on Apple Silicon/Intel macOS, ARM64/x64 Windows and
Linux x64. Its gate covers 22 suites and actual trace bytes plus committed
fixture/model/helper provenance. A passing export is separate from gameplay;
current platform evidence is linked from the pull request after execution.
Implementation and this local evidence are banked in the same commit.

## Remaining engine work

Session-owned HP/handle snapshots and ready-frame invocation follow this gate.
Derived/player damage, death/loot dispatch, movement, collision and a playable
scene remain separate work. This RTA composition does not establish Norrath
behavior or original-console peer compatibility. Original content stays private.
