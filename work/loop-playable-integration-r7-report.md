STATUS: COMPLETE

Wave-7 synthetic client integration, full suite and final exported flow are
verified. Local-only work on `codex/playable-integration` in
`/Users/syndicatemike/Projects/open-champions/worktrees/playable-integration`.

| Released branch | Source tip | Local merge | Owner status |
|---|---|---|---|
| codex/native-fx | 2a4a3a146a9f3f75dfdf3bbdcd08f32feeeab361 | 8d9463a | PARTIAL, retained refusals |

Read the full owner r2 report and `docs/NATIVE_FX.md` before merging. The
GDExtension CMake and registration conflicts retained both sets of additions.
Waves 1–6 remain in ancestry. The water root remains the integrated tree.
Native audio has not been merged.
Reference-function coverage unchanged; client integration only.
The owner contract pins research source `d7922ef96e9fc3b812a4f3ce441478e93f3f44ff`;
its file hashes and unresolved evidence are retained in `docs/NATIVE_FX.md`.

## Implemented composition

The play scene uses real native model/WORLD vertex lighting, additive particle
MultiMeshes and floor decals. The released native FX and lighting algorithms,
JSON parser and Godot bridge remain unchanged. The GDExtension FX target now
uses the same strict floating-point options as its native target. Renderer
adaptations convert native Z-up coordinates, use inverse-transpose normals,
preserve material overrides and current native skinned poses, cache static
WORLD lighting, and restore original resources when cosmetic owners reset.

Owned hit/death observations from the combat World feed a per-ready-tick Godot
signal, including each tick inside catch-up batches. No rendering callback
advances combat or consumes shared RNG. A native hit produces the documented
300-particle spawn-0 burst and a red splat; the death queue produces a burst and
a full-detail pool. Blood settings 0/1/2 retain the native gating. This red
material and choice of death effects are explicitly authored fixture policies,
not claims about original Skelight/Agony materials or unknown death recipes.

The animated fight proves tick 37 delivers two actual hits, health 100 → 84,
600 native particles and two decals. The previous fight still proves player
retaliation, death and next-tick removal; FX does not duplicate removal events.
The exact shared seed remains 1140036074. Paused/no-ready calls are inert.

No user definitions JSON was supplied. Default definitions and both sheets
are explicitly synthetic. An explicit pause-menu JSON file selection validates
on a temporary native bridge before replacing cosmetic resources; failure is
atomic. The successful picker check uses an authored synthetic JSON file. No
retail value, definition file or private asset was read or committed.

Glow, trail and beam primitives remain disabled. Body parts, disintegration,
ice-death recipes, original atlas/projection gaps and kind-2 per-vertex alpha
remain refused as documented by the owner. Ordinary play retains its long
continuation/save path; separate combat/animation/level fixture limits remain.
See `docs/PLAYABLE_FX.md` for the exact event, clock, lighting and file contracts.

## Corrections during verification

Retained diagnostics include an initial C++ compile error from treating an
integer damage result as a floating-point word; the observation filter now uses
its actual integer type. Initial Godot runs exposed null-metadata semantics and
a static-lighting timing issue: failed save loads must preserve the current
terrain resource. Guarded metadata reads and synchronous WORLD lighting at
observation publication fix these. Native skinned poses replace the lighting
source cache correctly; a tick-38 regression assertion guards against freezing
the first pose.

Initial graphical evidence showed a stale footer claiming effects were omitted.
The play footer now describes active synthetic lighting/effects; the existing
diagnostic footer remains on its original path. The synthetic FX disclosure is
visible above the viewport. Initial captures/logs are retained separately in
`work/playable-wave7/initial-export/`; they are excluded from final totals.

## Evidence boundary

All interactions use scripted scene UI signals/native calls in the actual
exported macOS application, with graphical viewport captures. Native modules,
not mocks, run the combined journey. The long ordinary soak keeps its existing
authored diagnostic-damage policy, including negative HP; it does not claim
original death behavior. The separate native combat fixture supplies that
proof. Synthetic red rectangles/white particle sheets are intentionally not
original art. No retail playability, full-engine completeness, audio playback,
Windows/Linux execution, console/emulator parity or physical Intel run is claimed.

## Final capture paths and commands

Current captures are under `work/playable-wave7/`, relative to this worktree:

- `ui-captures-16x9/fx-02-hit-gore2.png` — actual hit burst and splat.
- `ui-captures-16x9/fx-03-death-gore2.png` — burst and visible red death pool.
- `ui-captures-16x9/fx-02-hit-gore0.png` and `fx-03-death-gore1.png` — gates.
- `ui-captures-compact/fx-04-animated-hit-tick37.png` — native poses, HP 84,
  floor splat and the visible synthetic-resource disclosure at 800×600.
- `ui-captures-16x9/fx-05-updated-skinned-pose.png` — next native pose, lit.
- `ui-captures-16x9/fx-06-lit-world-closeup.png` — real decoded WORLD triangle,
  native vertex colours; only the test camera moves for inspection.
- `ui-captures-4x3/animation-04-manual-cosmetic-tool.png` — separate preview.
- `ui-captures-4x3/ui-01-positioned-hud.png` and `ui-02-inventory-grid.png`.
- `ui-captures-16x9/12-loaded-continuation.png` and `13-continued.png`.
- `ui-captures-16x9/level-04-arrived-aab.png` and `level-05-restored-aa.png`.

Full entry/library/save/load/refusal captures are in `captures-16x9/`,
`captures-4x3/` and the three `ui-captures-*` folders. Prior draft exports under
`initial-export/` and `disclosure-export/` are excluded from current totals.
The WORLD label now distinguishes synthetic lighting from omitted original
WORLD object/light/effect chunks.

The owner's standalone visual script also passes with the integrated renderer:
`Godot --path godot --script res://tests/native_fx_visual.gd`. Inspected
`work/native-fx-visual.png` shows blend/additive overlap; `work/native-fx-decal.png`
shows the floor decal. Its log is `work/playable-wave7/native-fx-visual.log`.
These two development captures are separate from the exported-flow count.

Reproducible commands and exact arguments are retained in:
`run-builds.py`, `run-validation.py`, `run-godot.py`, `run-export.py`, and their
JSON results under `work/playable-wave7/`. All builds use this worktree's sources
and locally produced extension. The release export is made with the pinned
Godot 4.5.2 application, `--headless --path godot --export-release 'macOS universal'`,
then unpacked locally with `ditto -xk`. The runner invokes the exported executable
with `--script res://tests/playable_wave_smoke_fx.gd`, graphical `--resolution`
and `--ready-ticks=36000` / `640`; UI signals and native methods drive interaction.

Implementation commit: `cc5ae86`. Merge commit: `8d9463a`.

## Native and headless validation

- Release native configure/build `-j4`: pass. Both matching GDExtension templates
  build universal arm64/x86_64 with strict numeric options and integrated water
  root. Commands and timings: `build-results.json` (six successful commands).
- Full `ctest --test-dir build/native --output-on-failure -j 3`: **172/172 pass,
  624.42 seconds**. No failures/timeouts; no isolated rerun required.
  `player-exact` passed in the full run in 112.49 seconds. All skills A/B/C,
  session, combat, level, animation and native-FX checks remain green.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: **714 tests, 385
  expected skips**, pass; 80-file portable boundary remains unchanged.
- x86_64/Rosetta: native FX, combat-play and animated-combat-play **3/3 pass**
  in 2.10 seconds. Native and matching extension also compile for both arches.
  This is not physical Intel execution.
- Godot import/default/headless matrix: **25/25 commands pass**. The final
  combined rerun after the WORLD-label/close-up addition passes **1,867 checks**.
  Earlier diagnostics remain separate; no errors in the final runs.
- Owner source audit: **8/8** audited native/header/bridge/document files match
  released `2a4a3a1`; **118/118** pre-wave-seven vector files match `420e563`.
  No golden observation or generator was changed. See `source-audit.json`.
- Development blend/additive/decal graphical script: exit 0, captures inspected.

Evidence: `ctest.log`, `python.log`, `validation-results.json`,
`godot-results.json`, `godot-combined-fx-closeup.log`, `x64-ctest.log`,
`native-fx-visual.log` under `work/playable-wave7/`.

## Final exported result

- **18/18 final exported commands pass**. The full 1280×720 graphical journey
  passes **72,687 checks**, including **36,000 ready ticks**, pause/save/load,
  identical continuation through tick 36,257, all prior combat/level/animation
  checks and the new FX/lighting checks. Peak retained records: 6,650; warmed
  allocation capacities remain stable. Final long command: 103.71 seconds.
- 960×720 and 800×600 each pass **1,967 checks**, including 640 ready ticks and
  continuation plus the combat, gate, exact-frame animation and FX fixtures.
- **179 current exported captures**; manifest includes dimensions and hashes.
  Inspected actual final WORLD close-up, tick-37 compact health/poses/splat,
  death burst/pool, manual preview, HUD and inventory output. The capture
  sequence includes all earlier journey states and refusal paths.
- Actual final app: `build/exports/wave7-final/Open Champions.app`.
- ZIP: `build/exports/wave7-final/OpenChampions.zip`, 67,041,811 bytes.
  SHA-256: `5115597c41ac1bcc9addac53e8a166bdc765af1604e8089d0edeabb1e0f4b980`.
- Universal release extension SHA-256: `582bbc45183a35e29e7f25f1afbd7a583d7895443046d38f1bb4f599256ae309`.
  Embedded and locally built release dylibs are byte-identical; both are
  arm64/x86_64. Graphical execution is Apple Silicon macOS.

Final metadata: `export-results.json`, `export-artifacts.json`,
`capture-manifest.json`, `final-summary.json`. No push, PR, issue, retail-value
commit, private-asset search, protected checkout write or delegated agent.
The released FX module's unsupported features remain documented and disabled;
there is no unresolved blocker to this requested synthetic integration scope.
