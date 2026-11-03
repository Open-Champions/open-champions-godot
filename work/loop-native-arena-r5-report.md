STATUS: IN PROGRESS — bounded implementation and review fixes committed; final gates pending. Live match acceptance REFUSED.

# Native arena round 5

Base: `486d5769` on `codex/native-arena`, initially clean. Local work only.
Research: `research-arena-live@9a0c26a5e6037043fa80fe55fa90a03dc0f619c4`,
`docs/spec/arena-live-scheduler.md:10-51,105-148`, and its referenced authored
mode contract and probes. Round-1 corrected PASS and round-2 PASS audits read;
native round-4 PASS and owner report read before implementation.

Implementation: `d85572de`; additive synthetic oracle, World runtime cases and
Godot table-boundary smoke. Research-to-code map: `docs/ARENA_MATCH.md`.
No previous vectors, tests, baselines or trace implementation were changed.

## Review findings addressed

- R5-1: a read-only native grid preflight refuses save-eligible actors before
  effect/preparation, intermission increment, actor removal or saved-pointer
  stores. The regression uses an actual World actor/grid and verifies retained
  counters, marker, pointer storage and grid contents. Existing callbacks retain
  their audited classification; no new destructor arguments are inferred.
- R5-2: the final `0x503890=1` store precedes the camera callback, matching its
  call delay slot; the earlier zero initialization is present. The callback
  regression reads the flag itself, beyond merely checking final state.
- R5-3: begin checks the player-record domain before any preload/counter write;
  five players with otherwise valid request inputs refuse without calls or
  state changes.
- Oracle provenance: generation reads hash-checked committed Git blobs at the
  pin, not the mutable research working tree. CTest verifies source hashes,
  synthetic recipes, row count and content digest before invoking the native
  runtime test. A mutated-copy negative check refused before launching a binary.

## Gates (pending note before execution)

- Release configure/build (`-j 4`): PASS after review fixes.
- Focused arena native runtime/refusal tests: 5/5 PASS, 3.19 s after review fixes.
- Full CTest (`-j 3`), count/time and isolated timeout retries: PENDING.
- Arena Godot checks and existing dev matrix: PENDING.
- Fresh FULL_TRACE: PENDING; required unchanged 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.

The initial pre-fix full run was interrupted after 1/217 completed tests when
review identified source fixes; it is not a gate pass. Its exact owned CTest
PID was verified by log handle, command and cwd before SIGINT (exit 130).
The pre-fix Godot run completed 46 commands with exit 0; diagnostic inspection
found its staging capture directory missing, so it is not counted as a clean
final matrix. The directory is prepared for the rerun without changing tests.

## Refusals retained

Natural victory, normal-input kills, later creature ticks, displayed results,
fixed rewards/collection, successful loading and signed-wave overflow are
unknown. Later-wave samples through 32765 do not prove continuous progression.
Missing construction/resources/records/services, ranking ties and unsupported
purchase paths must refuse. No live match acceptance or success fallback.

No game data/names or absolute private paths belong in tracked changes.

Full CTest (recorded by the orchestrator, 2026-10-28 ~00:10, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` on 74fae130 — **217/217 passed, 714.29 s**, no timeouts (log `work/native-arena-r5-ctest-full.log`). Arena Godot tests/matrix and FULL_TRACE for this revision are still PENDING.

## Test continuation checkpoints (2026-11-02)

- Arena Godot gate: **PASS**, `ARENA_BOUNDARY_SMOKE_PASS checks=113 failures=0`,
  exit 0 with zero captured engine/script diagnostics. Editor import also exited
  0 with zero diagnostics. Evidence:
  `work/native-arena-r5-verify-20261102/arena-verified/results.json` and
  `arena-verified/godot-arena-boundary.log` under that evidence directory.
  Runtime user storage was fresh and isolated per command. Initial harness
  directory-placement attempts were rejected by its own isolation check, not
  by arena assertions; Godot sanitizes parent-directory components. The final
  runtime used a fresh supported custom user-directory name; editor import
  does not create a game user directory. No game-source or test change.
