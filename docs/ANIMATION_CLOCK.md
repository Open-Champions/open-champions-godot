# RTA animation lifecycle and clock

The portable `champions-animation-clock` library implements the published
cache-free RTA animation timing contract. It owns track cells, clip identities,
weights, clocks and event results independently of Godot. It is not connected to
the controllable scene, and does not sample or render poses. The optional
[Session adapter](SESSION_ANIMATION.md) now owns clock state for explicit ready
tasks under a separately documented authored policy.

## Pinned behavior

The implementation follows these immutable research sources:

| Source | Commit | SHA-256 |
|---|---|---|
| `tools/ref_anim.py` | `f516ac798bf28375554fe53ffb31b74802844136` | `6b85385ceb9888be481d83e9a32d17a627d900060ffbb5e98ffc178b1a239af8` |
| `tools/ref_core.py` | `41e847af5360724ab62d208594f8027ea3c1b686` | `0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123` |
| `docs/spec/animation.md` | `f2b996dded898f1b0b558b901cab875f775cfdc7` | `be0bc9f02b9e9fbfac607bd909ccee94cf1f09342b2350cedbe274e56f8837ab` |

The sources live in the
[research repository](https://github.com/Open-Champions/champions-return-to-arms-re).
Public fixtures contain authored timing metadata; no retail clip streams or
assets are included. RTA tests do not establish Norrath behavior.

All 22 modeled operations are supported: clear, push, weighted push, chained
push, start, blend-in/out start, stop, persistent-track filtering, cache release,
state destruction, frame synchronization, randomized time, synchronization to the
lower track, jump/snap to end, identity/flag lookup, fade in/out, explicit weight,
state copy and advance. Cache release/destruction operate only in the admitted
cache-free domain.

The model's chained start executes `(below.time - length) + 2`. Nearby prose
says "two ticks after," although ordinary timing starts near two ticks before
the lower clip ends. The executable formula is preserved. The discrepancy is
reported without editing the research source or spec.

## State, arithmetic and lifetime

Each state owns 1–127 fixed track cells. Removal shifts cell contents and retains
inactive stale cells; clearing the active count does not erase them. Clip objects
have shared identity, a joint mask and four mutable event slots. Aliases share
event changes, while equal-valued distinct clips remain distinct. Clip arguments
are retained by value so cell shifts cannot change an aliased operation target.
Copied states share clip objects and copy every cell. Returned track pointers
refer to cells whose contents may change on subsequent operations; callers must
not retain them across resizing or destruction.

Timing and weights use raw32 integer patterns through the existing game
arithmetic and the caller's selected FPU context. No host floating point enters
this module. Cosmetic randomization uses the separate cosmetic stream and
consumes no draw for an empty state. Event masks combine in original order and
return a signed byte widened to an integer.

Supplied flags remain raw32 until an operation explicitly performs the model's
16-bit masking. This preserves mutable model inputs such as `65568`; silently
truncating them on input changes behavior. New-track writes, stop masking,
signed flag lookup and final advance normalization retain their original rules.

## Refusals and executed prefixes

ROOT/YAW, nonzero decoder or pose cache tokens, requested pose/root/yaw outputs
and sound execution are explicitly unsupported. Every operation checks the
complete cell array, including inactive cells, before mutation. These native
policy refusals are separate from original-model outcome evidence. Supplied
state shape and active clip presence must be valid.

LOOP advance uses repeated original subtraction stores, not remainder arithmetic.
An aggregate caller budget defaults to 65,536 subtractions, with a hard cap of
1,048,576. Exhaustion retains each completed subtraction and the original prior
state flags. Original `HostFault` outcomes also retain the executed prefix;
there is no rollback. These limits bound pathological raw arithmetic rather
than claiming an original runtime resource limit.

## Executed evidence

The independently generated corpus contains **4,224 cases**: 22 operations ×
12 variants × all 16 arithmetic settings. Each record submits complete initial
state and commands, then observes results, both states, every stale cell, shared
clip/event identities and system frame/cosmetic seed after each operation.
The generic native replay executes those inputs; expected columns do not drive
native state or operation dispatch.

The exact expected trace SHA-256 is
`0236b742e1466dbcbb06d58ac35aca4991a56a5f1aaf13015cdd7365963cba25`;
its FNV-1a64 is `824fa0fba95221b7`.

Executed local Apple Silicon checks:

- Optimized and ASan/UBSan replay each match every original Python expected
  byte for all 4,224 cases. All four linked runtime units were instrumented.
  Mac leak detection was disabled; no leak claim is made.
- Three fresh generator processes reproduce both fixture files exactly. Seven
  atomic publication failure tests pass.
- Eighteen API checks cover capacity endpoints, alias-safe removal, stale-cell
  lifetime, unsupported-service atomic refusal and retained LOOP failure prefixes.
- Twenty-one transport tests pass. `/dev/full` is unavailable on Mac and skipped;
  the actual file-size-limit write-failure test passes.
- The Python suite passes 460 tests with 252 explicit native/platform skips.
  The final core integration subset passes 92 tests, including 62 comparator
  tests and 27 simulation-boundary tests. These comparator unit fixtures are
  synthetic rejection checks, not execution on five physical hosts.
- GCC 15 strict C++17 syntax checks pass all three new translation units;
  LLVM 20 formatting passes all five owned C++ files. The simulation boundary
  covers 54 source/header paths, including both clock files.
- The shared Universal2 GDExtension build succeeds. Existing source-scene tests
  pass 70 checks and source-browser tests pass 283 checks. The clock library has
  no Session or GDExtension consumer; these checks establish the existing build
  graph remains usable, not scene animation playback.
- All 87 local CTest gates pass across the full regression and a fresh integrity
  rerun. The initial integrity test started before its new-suite fixture setup
  was complete and refused the missing animation provenance. The completed
  fixture setup passes on rerun; runtime expectations were unchanged. The full
  regression took 938.39 seconds and the integrity rerun took 8.26 seconds.

Independent runtime, replay and integration reviews found no unresolved blocker
in the admitted domain. The supplied Python flags initially exposed a native
u16 input truncation mismatch; correcting storage to raw32 made all original
expectations pass without changing the generator or oracle bytes.

CMake registers API, exact replay and transport gates. The five-host comparator
requires this 33rd suite alongside prior suites, exact Python results and pinned
reference/spec/generator provenance. Platform CI remains a separate gate until
actual artifacts from the final implementation head are inspected.

Original clip decoding, pose caches, sound services, root-motion generation,
event-to-gameplay dispatch and Session scheduling remain work. This standalone
module does not establish animation playback, retail playability or PS2 network
compatibility.
