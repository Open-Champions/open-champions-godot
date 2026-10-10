# Experimental RTA session and replay host

The native session layer composes the separately tested network, script, item
and combat modules. It delegates input submission to modeled `net_main`, then
runs named, caller-supplied tasks only through its ready-frame callback. This
is an authored integration boundary. It does not recover original object
scheduling, player movement, damage-to-HP handling or engine script commands.
Presentation, collision, saves, sockets and a playable scene remain work.

## Ownership and arithmetic

`champions::simulation::rta::session::Session` owns its network machine, named
script machines, item definitions/records and stable item-to-combat arena.
It is noncopyable and nonmovable. The profile must be explicitly `rta`;
Norrath and unknown profiles refuse until independently validated. This label
selects pinned contracts, not an identified or playable game revision.

Tasks and AMX native functions are supplied trusted host code. Their executable
code and captured external state are outside the owned snapshot and replay
proof. Registration names/order, binding kinds and callable presence are recorded.
Declared native names are cached at load; snapshots do not parse a damaged
header to derive them. The raw VM image/header fields remain available after
a bounded callback corruption or truncation failure. No original
engine native is replaced with a silent success. Missing bindings and modeled
errors remain visible.

Gameplay values are integer/raw numeric patterns. The host selects an explicit
FPU contract for compound combat operations and restores the calling context.
Items retain their fixed arithmetic contract; network arithmetic uses its own
explicit context. Item/combat RNG state and caller-supplied network sync state
are separate domains. No implicit shared-global alias is invented.

## Failure boundary

Operational exceptions retain the modules' partial mutations and latch failure.
The failed host refuses subsequent operations rather than repeating an action
at an unadvanced frame. This policy is authored host behavior; it does not
change the pinned network model or add rollback. Reentrant submissions refuse.

Scripts have 100,000-instruction budgets per execution, a nesting limit of
16 and a submission frame-step cap of 32. The host bounds eight scripts to
16MiB of total images, sixteen tasks/messages, 256 definitions/items and
65,536 total history entries. Message kinds/payload lengths fit their wire
bytes; these are explicit wrapper refusals, not changes to the network model. Native/task callbacks are trusted executable
code and are outside the AMX instruction counter. The host is used serially;
concurrent operations on one session are not supported.

## Exact composition contract

The optional generator reads immutable Git objects from an explicit research
checkout and verifies each source SHA before execution. Ordinary public tests
use only committed authored vectors; they need no research checkout or game
content.

| Module | Pinned research commit |
|---|---|
| Core | `41e847af5360724ab62d218594f8027ea3c1b686` |
| Items | `14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512` |
| Combat | `7212ecfecbeae8dd2015f6c82897226f49a94695` |
| AMX | `5ad489a445d41612d0795c063e1ce67557d5c441` |
| Network wire | `54258f9d7e281ef07f0675040e82e6264f9386cf` |

The canonical five-entry reference map has SHA-256
`4eb13e02ec6c1237ce7660794fd7e8e2f5870f15bd13a08784719a917fd2b14a`.
The strict gate verifies that actual map and `rta-authored-session-v1` policy,
fixture and manifest identity, source commit and exact native result bytes.
The new replay suite joins the existing sixteen mandatory simulation suites.
Matching hosts alone are insufficient: each result must match independently
composed Python expectations.

The existing synchronization hash excludes item and script state. Full replay
observations therefore include owned network fields/rings/pads, script images
and mutable state, definitions/items, converted combat data, draw histories,
events and failure/configuration state. A matching wire hash does not establish
matching game state or original-client compatibility.

## Verification

The corpus has **768 independent Python-generated cases**, covering twelve
scenarios, sixteen FPU configurations and four seeds. The generator replays
each case in two separately constructed identical-role oracle replicas; these
are not opposite transport endpoints. A separate native integration test
exchanges real modeled datagrams between sessions at local slots zero/one,
with delivery delayed by one/two calls and one endpoint called every other
presentation iteration. Their item/VM/RNG projections match at twelve completed
ticks; transport-local state legitimately differs. The full single-session
state corpus remains the independent Python oracle.

All 218 native API/integration checks and 768 replay cases pass optimized and
with all composed modules instrumented by ASan/UBSan on Apple Silicon. Eight
public transport tests cover malformed/changed/truncated/duplicate corpora,
Unicode paths and prevention of input overwrite. Generic Python discovery
skips the executable-dependent cases when no native path is supplied; their
dedicated CTest entry supplies that path and runs all eight on every native host. All 35 existing CTest cases
pass, and the three new session cases pass. Twenty-three artifact/boundary tests pass. Root independent
regeneration produces identical compressed vectors and manifest bytes.

Vector SHA-256:
`968225887ce9ee54c49e479233b87d18b497c7f5cc074b328f88414589975262`.
Actual optimized/sanitized replay SHA-256:
`6aeed054ccb2dfaa4c73a36113921484fad514ff20b11b405ee5fbd9d01db4ec`.
The combined corpus has 1,409,415 exact result records per native host across
seventeen mandatory suites. Five-platform native execution results are
recorded in the implementation pull request checks. Existing Godot browser
exports serve as regression checks; this milestone adds no playable export. No original assets, private script images or
raw decompilation are part of this implementation.
