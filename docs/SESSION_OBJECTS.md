# RTA session object adapter

The optional adapter connects the validated object runtime to the owned native
Session. It is restricted to the explicit RTA network profile. Legacy authored
Session tasks keep their existing behavior when object mode is disabled.
Object mode uses one priority-table dispatcher and refuses mixing it with those
legacy tasks. Derived behavior, movement, damage-to-HP handling and engine script
natives remain separate work.

## Contract and phase boundary

The independent oracle composes immutable published models:

| Model | Research commit |
|---|---|
| Core | `41e847af5360724ab62d208594f8027ea3c1b686` |
| Network wire | `54258f9d7e281ef07f0675040e82e6264f9386cf` |
| Objects | `0dd02ce416389dd440540119727d946636507703` |

The canonical three-model reference map hashes to
`7ecb261a35003527dcadc58c4ae55d3cfba8ade014bd53b0d6b7327fcbcb59d9`.
The fixture policy is `rta-session-objects-v1`. These models specify module
behavior; their connection is an explicit authored composition boundary.

The pinned network model supplies a simulation callback after peer inputs and
messages. The separately pinned [main-loop document](https://github.com/Open-Champions/champions-return-to-arms-re/blob/0dd02ce416389dd440540119727d946636507703/docs/mainloop.md)
records **Observed** frame assignment before peer inputs. That document has
SHA-256 `b5e0bacfb38dc56fcda79d11988cae76621597c740de7bb8bbea59f8ce6c0895`.
An optional native pre-frame hook implements this adapter phase only after the
pending-level skip. Its empty default preserves the published wire-model path.
This is not a claim that the Python wire model already implements that phase.
Between-tick frame restoration is not invented.

Before each supplied message callback, the adapter imports the current peer pad
words. Peers already processed have current input; later peers still have their
previous words. Scheduler entry imports those words again. Nested World access
retains in-progress pad and RNG mutations, including cleared edge words during
end-of-tick processing. Exactly one modeled `tasks_run(1)` runs per actual ready
callback. A held sample can still process a ready backlog; pending-level calls
can report positive available steps while actually running no World tick.

## Ownership and refusals

Session owns the World and its callback/task registrations. Guarded access is
valid only within the supplied call; trusted handlers must not retain a World
reference, manually start another scheduler pass or replace adapter-owned
callback wrappers. Reentrant submission from a World scope refuses. Snapshots copy owned value
state and record registered callback labels/presence and numeric task IDs; executable closures and external
captured state are outside the proof boundary.

An explicit shared seed initializes World and network synchronization state.
Combat randomness remains a separate existing domain. No global alias between
those streams is inferred. Synchronization positions and accumulator remain
supplied observations, with no artificial frame, movement or accumulator update.

Failures retain partial module mutations and the existing Session failure latch
refuses subsequent operations. Neither rollback nor an automatic retry is added.
Session pad words are unsigned 32-bit wire values. Wider or negative writes
refuse while retaining the actual World value and the low bits already published
to network state; those refusals do not claim additional Python behavior.
Missing handlers, unsupported registration and bounded resource exhaustion stay
visible. A full World snapshot is required for comparisons; equal wire hashes
can hide differences in object tags, schedules or other owned state.

## Verification boundary

The authored corpus has 1,024 cases: sixteen scenarios, sixteen FPU settings and
four seeds. Each Python case runs in two fresh same-role oracle replicas. Root
also regenerated every case in isolated processes; the complete compressed
fixture and manifest matched the primary generation byte for byte.

Uncompressed fixture SHA-256:
`fc35aefaf8d1b41fb5c621a0116902e9707f18018941947351920126acd9b469`.
Expected complete native trace SHA-256:
`73c60ef68861db545e771bbef37090d0798d90e5c6bf5b3e901f8f23f84c57f8`.

All 1,024 complete native replay records match those Python expectations in an
optimized build. All 286 API/integration checks pass optimized and with all eighteen composed
implementation modules under ASan/UBSan on Apple Silicon. The first 64 replay
records also match Python and optimized output exactly under those sanitizers;
the remaining replay records are outside the sanitizer execution claim. A
separate native test exchanges actual modeled packets between opposite endpoints
with delayed delivery and unequal presentation cadence. Their shared World
projections match at twelve completed ticks; transport-local fields differ.

All 44 local native CTest cases passed. The new public transport suite has eleven
checks for complete Unicode-path replay, incomplete/changed/duplicate corpora,
ASCII/LF requirements, same-file and hard-link safety, and output failures.
Platform-specific I/O cases explicitly skip where unavailable. Python discovery
ran 145 tests with 39 explicit native-binary/platform skips; dedicated CTest
entries supply the native tools. Thirty-three artifact/boundary/runner tests
passed, and the changed C++ files pass the repository's clang-format policy.

The platform gate requires nineteen game-system suites on macOS arm64/x64,
Windows arm64/x64 and Linux x64, plus five Godot regression exports. Each actual
trace must match the committed Python expectations, fixture, source commit and
pinned provenance. The combined suites contain 1,411,655 records per native host.
Matching peers or exports alone cannot establish gameplay.
No original assets or executable bytes are needed by the public tests. This
adapter does not establish a playable scene, original-console networking or
Norrath behavior.
