STATUS: COMPLETE

Wave-8 synthetic composition, full CTest and exported graphical verification
are complete. Local worktree:
`/Users/syndicatemike/Projects/open-champions/worktrees/playable-integration`.
Branch `codex/playable-integration`; clean starting tip `eec5d71`.
Reference-function coverage unchanged; client integration only.

| Released branch | Immutable source tip | Local merge | Owner status |
|---|---|---|---|
| codex/level-textures | 973782d76c6a95e31fd267b06df0c6ffbc423fc1 | c34004e | PARTIAL; retained gates |

Read the complete owner r3 report and `docs/LEVEL_TEXTURES.md` first. The single
native CMake conflict retained both native-FX and level-texture test additions.
No other branch was merged. The matching texture extension is built locally in
both universal templates; the native decoder/binder and C++ bridge are unchanged.

## Composition

The real play scene now accepts a matching TEX after an explicitly chosen WORLD.
Native texture decompression, PSMT8 palette decoding and WORLD material binding
supply owned images, geometry and UVs. The selected WORLD/TEX pair is decoded
and the native collision candidate prepared before publishing. Invalid TEX
keeps both the existing native state and lit textured terrain resource. Revisit
re-reads only the previously selected paths; no asset association is inferred.

The shared terrain builder retains the owner's decoded texture/shader behavior.
The play shader consumes native-FX vertex RGB, preserving texture alpha, cutout,
reverse/additive blending, clamp/bilinear addressing and layer priority. Lighting
uses the actual released `shade_world` model and existing synthetic fixture
lights. No point-light formula was added. Original WORLD light entities are not
loaded; the scene identifies its lighting inputs as synthetic.

For chunks explicitly marked non-dynamic, the released base-texture pass stays
visible; special signed constant-colour composition remains gated. Environment
mapping, console depth ordering and automap tile-code mapping remain disabled.
The two 0x46 row choices remain explicit helper inputs; full model 0x46 decoding
is still refused. No source mapping, material constant or retail value is guessed.

Native save bytes and authority are unchanged. A bounded in-memory association
retains decoded presentation for eight successful textured saves, keyed by the
selected path plus SHA-256 of the written bytes. An exact same-session load
prepares that presentation before native commit. Unknown copies, changed bytes,
evicted entries and new application runs restore neutral collision display.
Explicitly select a WORLD/TEX pair to restore presentation in those cases.
The path-pair cache is also bounded to 128 entries. These are authored client
ownership rules, not original game behavior.

## Test and evidence boundaries

The combined check extends the complete wave-7 journey: long ordinary play,
continuation/save/load, HUD/inventory, combat/death/removal, 180-tick gate and
saved level return, tick-37 animated damage/FX, then textured lighting and
ownership checks. All components use the real locally built native modules.

A byte-identical copy of the owner's authored 16-KiB native texture fixture is
included under `godot/tests/fixtures/` for explicit exported checks. Production
selection never loads it automatically. No retail data was available, searched
for, read or committed. The check verifies the actual decoded 40×20 texture,
UV/position retention, all three native light colours, real shader modulation,
failed-pair atomicity, reset, library revisit and save association isolation.

The graphical comparison temporarily replaces only the mesh colour array with
white, captures the same decoded surface, then restores the native colours.
Geometry, texture bytes and UVs are unchanged. Camera/actor visibility changes
are inspection-only. The owner's separate material pixel check remains in the
actual exported app to guard blend/cutout/layer behavior.

Source pins are inherited from the released owner's `LEVEL_TEXTURES.md`, including
research-levels `18e1a34d92f7b7ad1c305a949785ff5d0d57e702`. That research object is
not in the allowed canonical research clone; a fresh research execution/hash
reconfirmation is not claimed. Released implementations, committed contracts,
authored fixtures and actual local validation are this integration's evidence.
No private retail, full-frame console, physical Intel, Windows/Linux execution,
original environment mapping or full playable-game completeness is claimed.

## Corrections while validating

An initial script import found a duplicate save method while composing the
shell. The hook now extends the existing save-capability dispatch rather than
adding another implementation. An initial continuation assertion compared
allocation capacity, which is explicitly not saved authority. It now uses the
existing `play_state` comparison, while bounded capacity is still checked by
the inherited soak. No native save or simulation algorithm was altered.
Diagnostics remain in `import-initial.log` and `textures-initial.log` under
`work/playable-wave8/`. Final logs are separate. Development graphical checks
and captures live under `dev-captures/`, excluded from final exported totals.

Contract and UI/ownership details: `docs/PLAYABLE_LEVEL_TEXTURES.md`.

## Completed build and exported checks

- Release native and both main/texture extension templates build with `-j4`:
  ten successful configure/build commands. Both extensions are arm64/x86_64.
  Water root remains this integrated tree. See `build-results.json`.
- Python tooling: **714 tests, 385 expected skips**, pass. The existing 80-file
  portable simulation boundary remains unchanged; no new simulation code.
- Godot import/default/headless/material matrix: **29/29 commands pass**.
  The final exported combined headless check passes **1,896 assertions**,
  including the added changed-save-path/hash guard.
- Actual exported commands: **21/21 pass**, including owner texture preview,
  inherited Session and graphical material pixel checks, plus all earlier
  scene/runtime/HUD/inventory checks.
- Exported 1280×720 journey: **72,723 checks**, 36,000 ready ticks and the
  inherited identical continuation through tick 36,257, plus all later fixtures.
  Peak retained records 6,650; warmed allocation capacities remain stable.
- Exported 960×720 and 800×600 journeys: **2,003 checks each**, 640 ready ticks
  plus the complete continuation/combat/level/animation/FX/texture checks.
- Real rendered pixels changed by native vertex lighting: **6,538** at 16:9
  and 4:3, **2,984** at 800×600, against the temporary white-colour reference.
  Native colours are restored afterward. Captures inspected at all three sizes.
- x86_64/Rosetta decoder test: **1,067 checks**, pass (CTest 0.22 seconds).
  This is not physical Intel execution. Graphical execution is arm64 macOS.
- Source audit: **8/8** released texture native/header/test/fixture/bridge/build
  files unchanged; **7/7** prior native-FX/header/bridge files unchanged;
  **118/118** prior vector files unchanged. The packaged fixture copy is identical
  to the released authored binary. No oracle or generator was rewritten.

Implementation: `757a372`. Merge: `c34004e`.

## Capture paths and reproduction

**188 final exported captures** live in the five current capture folders under
`work/playable-wave8/`. Development captures are excluded. Key paths:

- `ui-captures-16x9/texture-01-native-lit-world.png`
- `ui-captures-16x9/texture-02-lit-colour-restored.png`
- `ui-captures-4x3/texture-03-loaded-lit-continuation.png`
- `ui-captures-compact/texture-01-native-lit-world.png`
- `ui-captures-16x9/fx-03-death-gore2.png`
- `ui-captures-compact/fx-04-animated-hit-tick37.png`
- `ui-captures-4x3/ui-01-positioned-hud.png`
- `ui-captures-4x3/ui-02-inventory-grid.png`
- `ui-captures-16x9/12-loaded-continuation.png`
- `ui-captures-16x9/13-continued.png`
- `ui-captures-16x9/level-04-arrived-aab.png`
- `ui-captures-16x9/level-05-restored-aa.png`

Interaction is driven by scripted UI signals and native calls in the actual
exported app; graphical viewport captures show real meshes and textures.
No browser or private inputs were used. `run-builds.py`, `run-validation.py`,
`run-godot.py` and `run-export.py` retain exact commands. Export uses pinned Godot
4.5.2 with `--export-release 'macOS universal'`, locally unpacked by `ditto -xk`.
The final executable runs `res://tests/playable_wave_smoke_textures.gd` at the
three resolutions, with `--ready-ticks=36000` or `640` and a capture directory.

Application: `build/exports/wave8-final/Open Champions.app`.
ZIP: `build/exports/wave8-final/OpenChampions.zip`.
Hashes and universal architecture checks: `export-artifacts.json`.
All logs/results are under `work/playable-wave8/`; no push, PR or issue.

## Final validation and artifact identity

`ctest --test-dir build/native --output-on-failure -j 3`: **173/173 PASS**,
**605.46 seconds**, no timeouts or isolated reruns. `player-exact` passed in
115.05 seconds within the full run. All skills A/B/C and prior session/combat/
level/animation/FX checks remain green; the merged level-texture check passes
all 1,067 native cases. Final results: `validation-results.json`, `ctest.log`,
`final-summary.json`; capture hashes/dimensions: `capture-manifest.json`.

Final ZIP size: 67,282,498 bytes; SHA-256:
`df05d1a58ad687c308290ab92201b0e6376f0a1c8620ae43d4e4c8cba0d5d522`.
Texture extension SHA-256:
`05264dc90fcbc1eaa184db9c33a16c5398565bb7bba069b14f3822e03958b2a0`.
Main extension SHA-256:
`582bbc45183a35e29e7f25f1afbd7a583d7895443046d38f1bb4f599256ae309`.
Each embedded extension is byte-identical to its locally built universal release
library. No remote publication, issue/PR, protected checkout modification,
private asset search or delegated work. All remaining gates are listed above;
none blocks this requested synthetic integration scope.
