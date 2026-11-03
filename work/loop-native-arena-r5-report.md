STATUS: VALIDATION COMPLETE — arena Godot PASS; strict Godot dev matrix FAIL (inherited UI-layout diagnostic); FULL_TRACE UNCHANGED. Live match acceptance REFUSED.

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

## Final gate results

- Release configure/build (`-j 4`): retained PASS after review fixes; no native code changed in this continuation.
- Focused arena native runtime/refusal tests: retained 5/5 PASS, 3.19 s after review fixes.
- Full CTest (`-j 3`): retained **217/217 PASS**, 714.29 s, no timeouts; recorded in `035061f0` and verified against its log. Not rerun.
- Arena Godot: **113/113 checks PASS**, zero engine/script diagnostics.
- Strict Godot dev matrix: **FAIL**, 44/45 strict passes; all 45 exits 0. The inherited four-argument UI-layout error is retained, not filtered. Fifteen ObjectDB exit-leak warnings are separately recorded.
- Fresh FULL_TRACE: **PASS / UNCHANGED**, 36,402 bytes, SHA-256
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

Full CTest (recorded by the orchestrator, 2026-10-28 ~00:10, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` on 74fae130 — **217/217 passed, 714.29 s**, no timeouts (log `work/native-arena-r5-ctest-full.log`). That checkpoint left Godot and FULL_TRACE pending; the continuation below completes their execution and records the strict failure.

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

- Strict Godot dev matrix: **FAIL**, 44/45 commands pass the strict error gate;
  all 45 processes exited 0, 155.35 s total command time, run serially.
  Runner exit 1 correctly retains the sole engine error in `ui_layout_smoke`.
  Evidence: `work/native-arena-r5-verify-20261102/matrix-verified/results.json`,
  `matrix-summary.json` and `matrix-run.log` under that evidence directory.
  Root cause: `godot/tests/ui_layout_smoke.gd:84` deliberately calls
  `hud_text_glyphs` with four arguments; the binding at
  `gdextension/src/ui_layout_service.cpp:45` requires five (including tint).
  Both files are unchanged from `486d5769`. The engine diagnostic remains
  captured despite the script's PASS marker and exit 0. Per the task, it was
  neither filtered nor changed. No arena regression or in-scope code fix.
  15 ObjectDB exit-leak warnings are separately retained in the summary;
  the error gate checks exits and engine/script errors, not warning severity.
  Each runtime command used a fresh isolated custom user directory.

- Fresh FULL_TRACE: **PASS / UNCHANGED**, 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
  The matrix's fresh `combined-full-combat` run exited 0 without engine/script
  errors and compared headless, drawing-enabled and mid-fight saved continuations.
  `FULL_SAVE_LOAD` also matched trace and presentation hashes with 300/300
  presented frames. Evidence: `work/native-arena-r5-verify-20261102/full-trace-result.json`
  and `matrix-verified/godot-combined-full-combat.log` under that evidence directory.
  This unchanged fixture does not enter mode B and is not live-arena acceptance.

## Completion and evidence scope

The continuation began clean at `035061f0` on `codex/native-arena`, after
reviewing `git log 486d5769..HEAD`, status and this report. The checked-in
native, GDExtension, Godot, tool, fixture and baseline files remain byte-for-byte
unchanged relative to that revision (`git diff --exit-code 035061f0 -- native/
gdextension/ godot/ tools/`). Only this report changed in the continuation.
Godot 4.5.2 used the existing post-fix extension builds. All commands ran
serially; no native rerun, push or PR. The referenced logs and harness remain
local ignored evidence; no game inputs or private paths are added to commits.

Evidence root: `work/native-arena-r5-verify-20261102/`. The local runner is
`run.py` (`arena` and `matrix` modes); it executes the previous 46-command
coverage as the standalone arena gate plus 45 dev-matrix commands, retaining
all engine/script errors. Editor import is preparation for the arena gate and
also appears in the dev matrix. A script PASS marker or process exit 0 cannot
override a captured engine error. Fresh user directories isolate runtime state.
The FULL_TRACE result is extracted from the matrix's fresh full-combat run;
it is not a second or stale execution.

Checkpoint commits: arena gate `5af6f55e`, strict matrix `13938c50`,
FULL_TRACE `0ab757bb`. The final report commit completes the handoff.

The remaining strict failure is the inherited required-tint arity probe,
whose source and binding are cited above. Its correction is intentionally
not part of this task's instruction to note and preserve the diagnostic.
No arena gate failure called for a code fix or a new research interpretation.
The round-5 behavior remains bounded by the research-to-code map in
`docs/ARENA_MATCH.md:34-55`; the intro/intermission and terminal authority is
`research-arena-live@9a0c26a5`, `docs/spec/arena-live-scheduler.md:35-47,105-125`.
Non-null preload-factory destruction and saved-creature construction remain
refused; missing services, invalid tables/player domains and signed-overflow
boundaries also refuse. No destructor flags, tie ordering, successful loading,
natural victory, normal-input kills, newly spawned creature ticking, continuous
later-wave progression, displayed results or fixed rewards are inferred.
Rendered FX/camera/material smoke commands passed their assertions; this is
not interactive or exported-app live-arena acceptance.

## Complete dev-matrix ledger

Each row references `matrix-verified/godot-<command>.log`; strict results also
appear in `matrix-verified/results.json`. Warning counts are retained separately
from the engine/script error criterion. Total command time: 155.35 s.

| Command | Exit | Strict error gate | ObjectDB warnings |
|---|---:|---|---:|
| `import` | 0 | PASS | 0 |
| `archive` | 0 | PASS | 0 |
| `frame_rate` | 0 | PASS | 0 |
| `session_facing` | 0 | PASS | 0 |
| `session_recovery` | 0 | PASS | 0 |
| `session_sandbox` | 0 | PASS | 0 |
| `texture_upscale` | 0 | PASS | 0 |
| `ui_art_gallery` | 0 | PASS | 0 |
| `control_anim_cache` | 0 | PASS | 0 |
| `level_library` | 0 | PASS | 0 |
| `scene_animation_preview` | 0 | PASS | 0 |
| `scene_original_art` | 0 | PASS | 0 |
| `scene_play_flow` | 0 | PASS | 1 |
| `playable_wave` | 0 | PASS | 1 |
| `play_session_runtime` | 0 | PASS | 0 |
| `combined-play-short` | 0 | PASS | 1 |
| `inventory_icons_smoke` | 0 | PASS | 0 |
| `ui_layout_smoke` | 0 | FAIL — inherited arity diagnostic | 0 |
| `ui_layout_dynamic_smoke` | 0 | PASS | 0 |
| `combined-ui` | 0 | PASS | 1 |
| `combined-level` | 0 | PASS | 1 |
| `combined-animation` | 0 | PASS | 0 |
| `native-fx` | 0 | PASS | 0 |
| `combined-fx` | 0 | PASS | 1 |
| `default` | 0 | PASS | 1 |
| `level_texture_preview_smoke` | 0 | PASS | 0 |
| `level_textured_session_smoke` | 0 | PASS | 1 |
| `combined-textures` | 0 | PASS | 1 |
| `level_texture_material` | 0 | PASS | 1 |
| `native-audio` | 0 | PASS | 1 |
| `combined-full-combat` | 0 | PASS | 1 |
| `fx-camera-headless` | 0 | PASS | 0 |
| `fx-camera-rendered` | 0 | PASS | 0 |
| `native-fx-visual` | 0 | PASS | 0 |
| `fx-bridge-refusals` | 0 | PASS | 0 |
| `level-lights` | 0 | PASS | 0 |
| `combined-combat` | 0 | PASS | 1 |
| `wave12-combat-extensions` | 0 | PASS | 0 |
| `real-data-empty` | 0 | PASS | 1 |
| `creature-anim` | 0 | PASS | 0 |
| `creature-anim-freed` | 0 | PASS | 0 |
| `creature-anim-dispatch` | 0 | PASS | 0 |
| `combined-audio` | 0 | PASS | 1 |
| `script_commands_smoke` | 0 | PASS | 0 |
| `real_water_texture_smoke` | 0 | PASS | 0 |
