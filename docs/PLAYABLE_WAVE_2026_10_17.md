# Playable scene integration — wave 9, 2026-10-18

STATUS: COMPLETE

Wave 9 adds synthetic native audio on play ready ticks and typed animation events.
Full CTest passes **175/175**; the exported 36,000-tick plus continuation flow passes
**108,758 assertions**, with 200 captures. All 16 requested released tips from
waves 1–9 are integrated locally. See [PLAYABLE_AUDIO.md](PLAYABLE_AUDIO.md) and
[INTEGRATION.md](INTEGRATION.md#playable-integration-wave-9--native-audio--2026-10-18)
for current evidence, commands, source limits and the retained shutdown warning.
The local round report is `work/loop-playable-integration-r9-report.md`.

## Preserved wave-4 acceptance

Wave 4 adds the explicit player/two-Skelight combat fixture while retaining normal
bounded play and continuation saves. Full CTest passed **163/163**. The final
export passed the **36,000-tick** combined flow and exact fight/death/removal checks;
116 final captures are retained. Combat saves remain explicitly unsupported.
See [PLAYABLE_COMBAT.md](PLAYABLE_COMBAT.md) for the policy and
[INTEGRATION.md](INTEGRATION.md#playable-integration-wave-4--opt-in-combat--2026-10-18)
for merge commits, fresh results, hashes and capture locations. The local round
report is `work/loop-playable-integration-r4-report.md`.

## Preserved wave-3 acceptance

STATUS: COMPLETE

Wave 3 is complete locally: both released branches are merged, the supported
HUD and inventory picture paths are wired, CTest passed 160/160, and the actual
exported combined flow passed through 36,000 ticks and save/load continuation.

## Wave 3 — HUD layout and inventory pictures — 2026-10-18

Starting point: clean wave-2 handoff
`65901c8fe9d913983f7d0a4fbe299f48f56e6161`, `codex/playable-integration`.
Read each released final work report and API document before merging immutable
source commits, one at a time, using `git merge --no-ff --no-edit`.

| Branch | Released source | Local merge |
|---|---|---|
| inventory-icons | d13ec1c73e76695ed94502bf2dddff719b2968e9 | c4823bb |
| hud-layout | 5e5fb9355a83bb2f09c26f57d19babd2b43585b6 | 96341ef |

Inventory conflicted only at additive native CMake registrations: retain the
play-session targets and inventory targets. HUD conflicted only in the extension
core's source list: retain inventory plus layout/text/dynamic sources. All other
registrations merged automatically. No portable native source/header was edited;
`work/playable-wave3/native-source-audit.json` verifies both incoming implementations
byte-for-byte. All 108 historical vector files still match `60def02` exactly.
No canonical integration or source worktree was changed.

Scene implementation commit: `56df452` (local only).

### Supported scene integration

The original-art panel now contains a native-positioned status view. It calls
`ChampionsUiLayoutService` for slot origin, animation-frame selection, health/mana
fill arithmetic, source rectangles and destination clips, and
`ChampionsUiDynamicLayoutService.painter_indices` for submission ordering.
The authored offline singleton view uses local slot zero. It displays the top
112 logical rows of the 640×448 canvas with the documented 640×480 square-pixel
mapping. This is a clearly labelled status-region preview inside the art panel,
not a claim of complete original-screen layout or final PS2 framebuffer parity.

The exact native records place the local status frame at **(8,36)**, health at
**(27,46)** and available mana at **(27,63)**. Health 96/100 ends at X **122**;
ready tick 7 selects **bar_red01.tex**. Native painter order draws the frame
before the bars. Bar sources use the native 100×8 logical rectangle within
supported 104×8 transfer pages; the existing 160×46 woven status image is retained.
Nearest texture sampling, destination scissoring and tint modulation are Godot
presentation; the native compositor remains the separate parity oracle.

An explicit **Load matching HUD animation set** action loads all 23 selected
red/blue animation frames and the status frame from the chosen source. Missing
or duplicate identities refuse; failed decoding restores the prior selection.
The individual-image picker/gallery remains usable. No missing frame is replaced
with another animation frame or generated art. The catalogue's new names follow
the native 00–10 family; existing identities and shape rules remain unchanged.

Only observed, finite, nonnegative values with positive maxima produce bars.
The dry play policy has no usable mana maximum, so **play mana is omitted**, with
an explanation. The integration test also feeds a separate real native recovery
observation to prove the mana mapping, then restores the live play observation.
Neutral internal arguments obtain unaffected frame geometry when a bar is absent;
that bar is removed before submission, and no fabricated value is displayed.
Experience, cooldown, flash, player names/levels, glow and other unobserved gates
are not invented. All panel observations remain read-only.

The scene also exposes **Inventory pictures…**. Its modal selects a local HDR,
local DAT and an exact picture name. Native code parses the index, resolves the
sector span, decodes pixels and returns grid footprints/origins. The host checks
file spans, reads only the requested DAT sectors, applies the explicit native
display-alpha policy, and places the resulting texture in an authored **8×8
inspection grid** with at most 64 pictures. The 32×34 cell geometry and icon
origins come from native placement; first-fit cell selection is authored UI.
HDR and individual payload reads have explicit 8 MiB inspection limits.

Failed HDR/DAT/name/packet/range operations retain the current grid. No substitute
placeholder art is fabricated. The native item-name builder, original 64-slot
streaming cache/placeholder policy and gold bottom-anchor rule are merged and
tested, but this manual inspector does not impersonate those callsites. No item
ownership, item tables, viewer-body selection, equipment slots, asynchronous
original cache schedule or original inventory screen layout is claimed.

Inventory opens paused, blocks Resume while modal, and preserves its read-only
pictures across continuation load. An actual compact export exposed a dialog
extending below the window. The grid now scrolls and wrapping labels have bounded
width, keeping Close reachable. The initial test palette was almost transparent;
the final **test-only** palette is opaque so decoded pixels are visibly inspectable.
No art was generated or added to the normal product flow.

### Evidence and commands

Pinned module contracts remain in `HUD_LAYOUT.md` and `INVENTORY_ICONS.md`:
HUD base layout at `5af769188dfbd47e15693f5041ae5d6d90292bc9`, dynamic/text/parity
at `0b73858def976bf494c2b2803339aa54be814a62`, inventory association at
`ad8c34ee7e4591a29fed361322bed1e085403752`. Their SHA-256 source pins remain
unchanged. The pinned public HUD layout and catalogue metadata were read with
`git show` from the documented research-ui checkout only. No private assets or
protected private research checkout were accessed.

The new `godot/tests/playable_wave_smoke_ui.gd` extends the real production-flow
acceptance fixture. It adds exact native HUD record checks, a complete synthetic
HUD archive, explicit inventory HDR/DAT UI selection, independently expected
RGBA pixels, cell placement, read-only authority, refusal ownership, modal
pause and load retention. It uses all real production resources and native
services, with no component injection. The underlying play fixture gained a
small overridable panel hook and an expected-art count; its original diagnostic
and play checks remain intact.

Build commands (all Release, four build jobs):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
# For target=template_debug and target=template_release:
cmake -S gdextension -B "build/mac-$target" \
  -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET="$target" \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build "build/mac-$target" --config Release --parallel 4
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
ctest --test-dir build/native --output-on-failure -j 3
```

Full Release CTest **passed 160/160 in 652.66 seconds**, exit 0, with `-j 3`.
`player-exact` passed in the full run in **117.69 seconds**. No test timed out
and no isolated rerun was needed. All four new inventory/layout tests, all
15 skill/kernel checks and all play-session cases passed in the combined tree.

Native and both universal extension builds passed. Boundary checker passed with
74 selected simulation paths and 38 tests; the new rendering/parser modules do
not expand that simulation census. Python discovery passed 712 tests, 385 skipped.
Full exact commands, durations and logs: `work/playable-wave3/build-results.json`,
`validation-results.json`, `boundary.log` and `ctest.log`.

The pinned Godot 4.5.2 executable and godot-cpp
`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77` were reused read-only. All 21 import,
headless/default commands passed, including every previous scene/runtime smoke,
`inventory_icons_smoke`, `ui_layout_smoke`, `ui_layout_dynamic_smoke`, and the
new combined UI flow. The final changed UI was rerun in the actual export.
See `godot-results.json`, `combined-ui-final.log` and `export-results.json`.

```sh
GODOT=../integration/work/runtime/Godot.app/Contents/MacOS/Godot
"$GODOT" --headless --path godot --editor --quit-after 120 --frame-delay 50
"$GODOT" --headless --path godot --script res://tests/playable_wave_smoke_ui.gd \
  -- --ready-ticks=640
mkdir -p build/exports/wave3-final
"$GODOT" --headless --path godot --export-release 'macOS universal' \
  "$PWD/build/exports/wave3-final/OpenChampions.zip"
ditto -xk build/exports/wave3-final/OpenChampions.zip build/exports/wave3-final
'build/exports/wave3-final/Open Champions.app/Contents/MacOS/Open Champions' \
  --resolution 1280x720 --script res://tests/playable_wave_smoke_ui.gd -- \
  --ready-ticks=36000 --capture-dir="$PWD/work/playable-wave3/repro-16x9"
```

For 4:3/compact layout acceptance, use `960x720` or `800x600`,
`--ready-ticks=640`, and separate capture directories. Normal real-time entry:
`build/exports/wave3-final/Open Champions.app/Contents/MacOS/Open Champions`.
All fixture bytes and captured art are synthetic. Execution is Apple M5 Pro,
arm64 macOS 27.2, OpenGL 4.1 Metal Compatibility; universal binaries also contain
x86_64, but physical Intel and Windows were not executed.

### Final exported results

All **17** final exported commands passed. The embedded release dylib matches
this worktree's locally built release extension byte-for-byte. Exported component
checks: animation 42, original-art 63, scene shell 64, combined diagnostic 68,
runtime 24, browser 283 and sandbox 124; the inventory and both layout service
smokes also passed. New combined UI headless: **1,377 checks**, zero failures.

| Graphical flow | Size | Ready ticks before save | Checks | Failures | Wall time |
|---|---|---:|---:|---:|---:|
| HUD/inventory + full play | 1280×720 | 36,000 | 72,142 | 0 | 121.38 s |
| HUD/inventory + play | 960×720 | 640 | 1,422 | 0 | 5.40 s |
| HUD/inventory + play | 800×600 | 640 | 1,422 | 0 | 5.84 s |
| Legacy diagnostic | 1280×720 | cap 512 | 102 | 0 | 2.97 s |
| Legacy diagnostic | 960×720 | cap 512 | 102 | 0 | 3.00 s |

The full flow was entry → level library → two WORLD choices → Start play →
animation preview → positioned art/HUD → inventory pictures → 36,000 ready ticks
→ focus/pause → save → changed state → load → identical continuation through
**36,257 ticks** → refusal cases → reset → return/re-entry. Retained observations
peaked at **6,650 records**, capacity stayed at **8,512 slots** after warm-up,
and the continuation was **2,979,244 bytes**. Art/inventory operations never
changed native authority. Loading clears cosmetic animation and restores
collision state, retains selected art/pictures, and does not invent a WORLD
source/mesh association. Repeated stress health inputs still follow the existing
diagnostic policy; negative health is not passed as a valid HUD bar input.

These are deterministic production UI-handler/fixed-step tests, not ten minutes
of manual mouse play or original retail-game acceptance. All retained-state and
future-state assertions remain from the wave-2 acceptance, with new panel checks.

Final actual viewport captures: **98**, excluding preliminary inspection images:
22 each under `work/playable-wave3/ui-captures-16x9/`, `ui-captures-4x3/` and
`ui-captures-compact/`; 16 each under `captures-16x9/` and `captures-4x3/`.
Each combined UI folder includes `acceptance.json`. Inspected representative
final states include:

- `ui-captures-16x9/ui-01-positioned-hud.png`: native-positioned status region,
  real health observation and explicit missing-mana note.
- `ui-captures-4x3/ui-02-inventory-grid.png`: ten visible native-decoded pictures,
  native grid spacing, explicit selected files and reachable Close.
- `ui-captures-compact/ui-03-inventory-refusal.png`: DAT range refusal while the
  existing grid remains visible and the complete modal fits 800×600.
- `ui-captures-compact/01-entry.png`: inventory menu action and wrapped navigation.
- `ui-captures-16x9/12-loaded-continuation.png`: known play mode, continuation
  choice and restored collision-ground ownership after load.
- Additional saved states include library, animation, art picker, 36,000-tick
  movement, pause/save, continued movement, corrupt save, stale clip and reset.

The preview compact export additionally caught and verified the dialog fit fix;
`work/playable-wave3/visual-inspection.md` distinguishes those from final evidence.
No new visual defect remained in the inspected final states.

ZIP: `build/exports/wave3-final/OpenChampions.zip`, SHA-256
`bb479d7c30a27adab8df38505ea4968303c1a879eaeaca99ef064d32ac2f0361`.
Local/embedded release extension SHA-256:
`ce0777949fdb9c4998d3faa2fad3ecdc36495ca282bf2bd02e2d283b1fabc870`.
Exact app/binary sizes and universal architectures: `export-artifacts.json`.
Earlier wave exports/evidence remain preserved.

### Boundaries and changed files

Integration edits are in `godot/scenes/play_flow.gd`, the original-art
catalogue/panel and new `hud_view.gd`, new `godot/ui/inventory_icons/` controls,
and the `playable_wave_smoke*` test hook/fixtures. Native work is inherited
unchanged from the two released branches. Documentation updates cover this report,
`INTEGRATION.md`, `HUD_LAYOUT.md`, `INVENTORY_ICONS.md` and `SCENE_ORIGINAL_ART.md`.

Full original HUD visibility gates, text/font selection, gameplay inventory,
original screen associations, field/dither/final-present parity, retail acceptance,
Windows and physical Intel execution remain unverified. Private game files are
absent and were not sought elsewhere. No new original gameplay rules, art or
asset association were invented. **Reference-function coverage unchanged;
client integration only.** No push, PR, issue, agent, scheduler, canonical checkout
write or protected-worktree access.

---

The following preserves the earlier wave-2 and wave-1 handoffs. Their historical
limitations are superseded only where wave 3 explicitly establishes new behavior.

# Playable scene integration — 2026-10-17/18

STATUS: COMPLETE

Both waves are integrated locally. Full Release CTest passed **156/156**; the
actual exported play flow reached 36,000 ticks and resumed identically through
36,257 after save/load. The original 512-tick cap remains only in diagnostics.

## Wave 2 — 2026-10-18

Starting from clean wave-1 handoff `7a23f0e0cf44abb3fbf42245f10fc1d57a6b5407`,
read the released runtime's r4 report and its explicit World checkpoint scope
extension, then merged `a01d260dc30203d4456fbe5dee28d3c60c0374e9` locally with
`git merge --no-ff --no-edit`. Merge commit: `d26d562`. Scene integration commit:
`1a51907324579c3d034de633dca2e2777d7caeff`. No source/canonical worktree changed.

| Wave | Branch | Authorized source | Local merge |
|---|---|---|---|
| 1 | scene-level-library | 95a6dda | 2341a79 |
| 1 | scene-animation-preview | 2606de2 | 4fd8c43 |
| 1 | scene-original-art | 1af101e | 719cf45 |
| 1 | scene-play-flow | 2f0a072 | 47dce83 |
| 1 | skills-spell-kernel | 3ef9b9a | be912f4 |
| 1 | skills-a-ctor | c094b96 | 20de046 |
| 1 | creature-water-2 | 23699af | 8e199a6 |
| 2 | session-play-runtime | a01d260 | d26d562 |

Only boundary-census files conflicted in wave 2: keep water source/header and
play policy header, deriving **74** paths. Preserve water boundary coverage and
all additive CMake registrations. Strict arithmetic flags/platform gates remain.
All 108 original corpus files are byte-identical to `60def02`; spell-kernel's
new corpus is the only addition. All 16 runtime native/bridge source, header and
target files match `a01d260` exactly (`work/playable-wave2/runtime-source-audit.json`).
No original numeric algorithm or runtime retention policy was altered here.

### Scene behavior and integration fixes

The production Start play action calls `start_play_session()` on the selected
terrain and observes `session_mode=play`. Recognize the exact advertised
`play-continuation-v1` format as native `save_file`, with a visible
**Continuation (current state)** selector. The Save picker defaults to `.ocplay`
in play mode; Load accepts both `.ocplay` and `.ocnsave`. Native magic/version
validation remains authoritative. Diagnostic Replay/Snapshot formats retain
previous meanings, and unknown capability formats still disable Save.

Refresh capabilities after explicit recovery/facing/cache diagnostic changes;
the UI explains why those configurations cannot start play. Default construction
and explicit diagnostic sessions retain their original 512-tick cap. The old
smoke suites now expect the released runtime while retaining their diagnostic
limit, legacy-save and missing-component cases.

New `godot/tests/playable_wave_smoke_play.gd` extends combined-test fixture/helpers,
not the production implementation. It uses the real shell and the three real
resource paths with no injected components. It exercises two native WORLDs,
Start play, native-tick cosmetic animation, exact native art pixels, bounded
retention, focus/pause, save/load, unchanged future states, refusal ownership,
level replacement, reset and return/re-entry. Unknown/corrupt saves, unsupported
water/moving WORLDs, failed art and stale clips remain explicit refusals.

### Exported acceptance

Actual local **release export**, Apple M5 Pro, macOS 27.2, Godot
`4.5.2.stable.official.6ce3de25a`, OpenGL 4.1 Metal Compatibility. godot-cpp pin
`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77` was read-only; the universal extension
was built locally. Embedded release dylib is byte-identical to the local build.
Both app and dylib contain arm64/x86_64 slices; actual execution here is arm64.

| Exported combined run | Native ready ticks before save | Checks | Failures | Wall time |
|---|---:|---:|---:|---:|
| Play, 1280×720 | 36,000 | 72,099 | 0 | 114.84 s |
| Play, 960×720 | 640 | 1,379 | 0 | 5.96 s |
| Play, 800×600 | 640 | 1,379 | 0 | 5.74 s |
| Diagnostic, 1280×720 | original cap 512 | 102 | 0 | 3.49 s |
| Diagnostic, 960×720 | original cap 512 | 102 | 0 | 3.50 s |

The long run retained at most **6,650 records**; **8,512 capacity slots** remained
unchanged after the 256-tick warm-up. It passed every per-tick bound, never reset
or stopped, and reported the retired history as unavailable. The continuation
was **2,979,244 bytes**, with independent OCPLAY magic and the existing 32 MiB cap.
After pausing and saving at tick 36,000, the original owner produced 257 future
observations, crossing two more retirement boundaries. Loading the file and
repeating the same inputs matched every authoritative observation through
**tick 36,257**. Allocation capacity is measured separately, and the documented
terrain-label/history-materialization fields are presentation metadata; neither
is mistaken for gameplay authority in the equality comparison.

Real animation and art owners survive the soak. Loading intentionally clears
the cosmetic selection, restores the static figure and retains read-only art.
Saves restore collision state, **not the original WORLD mesh/source association**.
The synthetic animation ends at its explicit eight-tick fixture duration; no
original asset pairing or animation dispatch is inferred. Repeated health-test
inputs produce negative raw HP under the unchanged diagnostic policy; integration
adds no invented death/progression or health clamp.

Interactions used production UI signals and input handlers plus deterministic
fixed-step submissions: ten **logical** minutes, not ten wall-clock minutes or
manual mouse play. Inspected actual exported captures across 16:9, 4:3 and
800×600: entry, library, animated mesh, art picker/pixels, pause, continuation
picker, restored collision state, continued movement and refusal states. No new
visual integration defect was found; long content remains scrollable.

Evidence: `work/playable-wave2/visual-inspection.md`, `export-results.json`,
`export-artifacts.json`, and `capture-inventory.json`. All **89** viewport captures
are local ignored evidence: 19 each under `play-captures-16x9/`,
`play-captures-4x3/`, `play-captures-compact/`; 16 each under `captures-16x9/` and
`captures-4x3/`. Each play capture folder includes `acceptance.json`.

Representative capture paths (relative to this worktree):
- `work/playable-wave2/play-captures-16x9/01-entry.png`
- `work/playable-wave2/play-captures-16x9/02-library.png`
- `work/playable-wave2/play-captures-16x9/05-animated-play.png`
- `work/playable-wave2/play-captures-16x9/07-art-panel.png`
- `work/playable-wave2/play-captures-16x9/09-long-play.png`
- `work/playable-wave2/play-captures-16x9/10-paused.png`
- `work/playable-wave2/play-captures-16x9/11-save-dialog.png`
- `work/playable-wave2/play-captures-16x9/12-loaded-continuation.png`
- `work/playable-wave2/play-captures-16x9/13-continued.png`
- `work/playable-wave2/play-captures-4x3/14-continuation-refused.png`
- `work/playable-wave2/play-captures-compact/06-art-picker.png`

Runtime owner evidence is reused only for its unchanged standalone 36,000-tick
native/Rosetta retention, RSS and failure-matrix measurements, as the brief allows.
See `PLAY_SESSION_RUNTIME.md` and the runtime's r4 report. This integration's
exported 36,000-tick run and all results above are fresh local execution; no
sibling binary was used as integration proof.

### Builds and regression commands

Run from the repository root (a checkout of `codex/playable-integration`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
python3 tools/check_simulation_boundary.py .
python3 -m unittest discover -s tools -p test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
```

For each `target` in `template_debug template_release`:

```sh
cmake -S gdextension -B "build/mac-$target" \
  -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET="$target" \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build "build/mac-$target" --config Release --parallel 4
```

Full Release CTest **passed 156/156 in 741.58 s**, exit 0, using `-j 3`.
`player-exact` passed in the full run (142.08 s); no timeout or isolated rerun
was needed. The helper nevertheless read a stale wave-1 LastTestsFailed.log and
started an unnecessary isolated player rerun; it was stopped and the helper was
corrected to consult that list only after a failing full run. The successful
full-suite log/exit code is the acceptance evidence. All 15 skills checks,
including A/B/C and spell-kernel, passed, together with the four
new play-session cases (boundary, runtime and both independent exact windows).
Final native evidence: `work/playable-wave2/ctest.log` and
`validation-results.json`.

Native Release build **passed**; both universal extension builds **passed**.
Boundary checker **passed**, with **74** selected paths and **38** passing tests.
Python discovery **passed: 712 tests, 385 skipped** in 36.323 s.

Godot import and all affected existing/new suites passed against the local
extension. Exact argument arrays/exit codes: `work/playable-wave2/godot-results.json`.
The first new combined short test had two incorrect expectations, then passed
**1,341 checks** after correcting them (`godot-combined-play-short-final.log`).
At ready tick 640, the last retired boundary is exactly 512; also, both bridge
save aliases intentionally write OCPLAY in play mode, rather than exposing the
portable legacy snapshot API. No runtime behavior was changed to satisfy tests.
The original failed output is retained, not silently replaced.

```sh
GODOT=../integration/work/runtime/Godot.app/Contents/MacOS/Godot
"$GODOT" --headless --path godot --editor --quit-after 120 --frame-delay 50
# Each script below used --headless --path godot --script res://tests/<name>.gd:
# archive_smoke, frame_rate_smoke, session_facing_smoke,
# session_recovery_smoke, session_sandbox_smoke, texture_upscale_smoke,
# ui_art_gallery_smoke, control_anim_cache_smoke, level_library_smoke,
# scene_animation_preview_smoke, scene_original_art_smoke,
# scene_play_flow_smoke, playable_wave_smoke, play_session_runtime.
"$GODOT" --headless --path godot --script res://tests/playable_wave_smoke_play.gd \
  -- --ready-ticks=640
"$GODOT" --headless --path godot --quit-after 120
"$GODOT" --headless --path godot --export-release 'macOS universal'
ditto -xk build/exports/macos-universal/OpenChampions.zip build/exports/wave2-final
```

The actual exported app separately passed animation **42**, art **63**, shell
**64**, combined diagnostic **68**, runtime **24**, sandbox **124**, and archive
browser **283** checks; default entry launch also passed. Full exact commands,
including `--sandbox-smoke-test` and paired `--browser-smoke-test --smoke-test`,
are in `work/playable-wave2/export-results.json`.

Interactive entry (normal real-time pacing):

```sh
'build/exports/wave2-final/Open Champions.app/Contents/MacOS/Open Champions'
```

Reproduce the complete exported deterministic soak and captures:

```sh
'build/exports/wave2-final/Open Champions.app/Contents/MacOS/Open Champions' \
  --resolution 1280x720 --script res://tests/playable_wave_smoke_play.gd -- \
  --ready-ticks=36000 --capture-dir="$PWD/work/playable-wave2/repro-16x9"
```

Repeat at `960x720` or `800x600` with `--ready-ticks=640` and a distinct capture
directory for layout/interaction verification. Use `playable_wave_smoke.gd`
without `--ready-ticks` to reproduce the legacy diagnostic flow.

Export ZIP `build/exports/macos-universal/OpenChampions.zip`:
SHA-256 `51297fce6a7e997a09a7662f8f7dbe922582e0cc026cf30ebe06d032cd116037`.
Local/embedded release extension:
SHA-256 `422c9fffbc495730fbd9f69c9de64da9e0d26306e5a6232d5a2b0b96cb59887a`.
Wave-1 unpacked app and its evidence remain at `build/exports/wave1-final/` and
`work/playable-wave/`; the standard macOS ZIP path now contains wave 2.

### Remaining product boundaries

The integration acceptance is for the explicitly authored singleton dry
movement/health play policy and synthetic fixtures. Diagnostic-only optional
recovery/facing/cache configurations remain unsupported in play mode. Finite
native resource/numeric limits still apply; bounded histories do not promise an
infinite session. No original combat/progression, enemy dispatch, online play,
Norrath behavior, retail WORLD acceptance, original HUD layout or automatic
asset association is claimed. Private validation is absent and was not sought
elsewhere. Windows and physical Intel execution are unverified; no integration
Rosetta execution is borrowed from the runtime owner's standalone evidence.

The component and runtime documents retain their immutable source pins; the
runtime contract is based on `a2b145b6b22f3c0451f861ed70faa1827f94bf09` and the
seven reference-function SHA pins in `PLAY_SESSION_RUNTIME.md`. The wave-1
supplemental skill/water pins below remain unchanged. **Reference-function
coverage unchanged; client integration only.** No push, PR, issue, scheduler,
new agent, canonical-worktree write or protected research-worktree access.

---

The following is the preserved historical wave-1 handoff. Its pending-runtime
limitation is superseded by wave 2 above.

# Playable scene integration, wave 1 — 2026-10-17

STATUS: MORE

Coordinator explicitly split the original five-component brief into two waves.
Wave 1 includes the four scene components plus three completed native jobs.
`session-play-runtime` is not merged; the 512-ready-tick diagnostic cap is an
expected limitation. No long-session or retail-playability claim is made.

Worktree: a local checkout of branch
`codex/playable-integration`. Clean starting HEAD:
`60def02263f2bcb9685cb06c5a0c7409ac5ccab4`. Each source worktree was inspected
read-only and clean at its exact authorized tip; its final `work/` report was read.
No canonical/source worktree, research client, PR73, push, PR or issue changed.

## Serial local merges

| Branch | Source tip | Local merge |
|---|---|---|
| scene-level-library | 95a6dda | 2341a79 |
| scene-animation-preview | 2606de2 | 4fd8c43 |
| scene-original-art | 1af101e | 719cf45 |
| scene-play-flow | 2f0a072 | 47dce83 |
| skills-spell-kernel | 3ef9b9a | be912f4 |
| skills-a-ctor | c094b96 | 20de046 |
| creature-water-2 | 23699af | 8e199a6 |

Commands: `git merge --no-ff --no-edit codex/<branch>`, in table order.
All seven merges were clean. Git automatically combined disjoint Skills-A
constructor and spell-kernel edits. Existing strict numerical flags, target
platform gates and historical vector/manifest bytes were preserved.
Full hashes: ignored `work/playable-wave/merge-audit.json`.
Historical corpus byte audit: `work/playable-wave/historical-corpus.json`.

## Integration-only changes

- Add the water source/header to the explicit simulation-boundary census,
  increasing its actual union from 71 to 73. Add rejection coverage for host
  arithmetic/presentation dependencies in those files. Wave 2 must retain this
  additive registration when it merges its own census additions.
- Make the shell's existing absent-component smoke case explicitly return null
  through its existing factory seam. This preserves the missing-component
  assertions after the real components are installed. Its other factory cases
  still cover adapter lifecycle; the new combined suite uses production paths
  with **no injected components**.
- Add `godot/tests/playable_wave_smoke.gd` for the merged real-extension flow.
  Synthetic WORLD/VIF/ANM/TEX fixtures are generated only in its isolated test
  directory. It drives UI signals and shell input; screenshots are from the
  actual exported graphical viewport, not a browser or a mocked renderer.

## Evidence sources and boundaries

The four scene components retain research pin
`a2b145b6b22f3c0451f861ed70faa1827f94bf09`; the spell kernel retains
`356add27d50e50972b2a671b31e8f4ec93e6778c`; constructor/water supplements use
`2502e2bfac58203ac0174fccd7e0953b36d06439`. Exact contract hashes and supported
refusal domains remain in `SCENE_LEVEL_LIBRARY.md`, `SCENE_ANIMATION_PREVIEW.md`,
`SCENE_ORIGINAL_ART.md`, `SCENE_PLAY_FLOW.md`, `SKILLS_SPELL_KERNEL.md`,
`SKILLS_A.md` and `CREATURE_WATER.md`. Integration changes no original algorithms.

Reference-function coverage unchanged; client integration only. Included source
branches establish their own standalone function counts; these are not gameplay
percentages. Water remains an opt-in supplied-grid service, not Session water
support. Cosmetic pairing and diagnostic art placement remain explicitly authored.
Private validation is unavailable; no alternative locations were searched.
Windows, physical Intel execution, Norrath, original HUD fidelity, retail asset
pairing, combat/progression and complete retail levels remain unverified.

Additional integration fixes found by actual export inspection:

- Animation picker Play was disabled whenever the shell paused for that picker.
  Play/Restart now retain the user's explicit playback intent while suspended;
  `observe()` still refuses to advance cosmetic time until native session Resume.
  The picker reports `waiting for session resume`. The combined test presses the
  visible enabled button before hiding the picker, then verifies native-tick pose
  advancement. It never invokes a hidden Play control to bypass this interaction.
- Give the animation panel an opaque background and small content margins so
  underlying shell text cannot overlap its labels.
- Give the art picker's wrapping status label the row list's existing 680-pixel
  minimum width. Before first layout, zero-width wrapping expanded the dialog to
  over 2,000 pixels, placing Close offscreen. The combined graphical test asserts
  the dialog fits its window; the re-exported Close control is visible.

These are presentation/assembly fixes only. No native numerical body, retention
algorithm, original-art pixels, save schema or gameplay behavior was changed.

## Save and test interpretation

The initial combined test expected all four submissions to be ready ticks; the
existing startup handshake instead yields three. It also compared presentation
metadata as if it were saved authority. These test assumptions were corrected
against the unchanged sandbox/save tests and bridge. The exact comparison now
excludes only `terrain_name`, `terrain_note`, `terrain_mode` and
`save_history_pending`; all authoritative observations, raw32 state and counters
must match. Both replay and indexed snapshot continuation are tested.

Existing legacy saves reconstruct collision ground, not the selected WORLD
render mesh/source association. Their loaded display is explicitly labelled
`Restored collision ground`; the original WORLD can be reselected through the
library. Indexed snapshots can retain deferred historical validation; a later
save attempt can materialize it even when filesystem writing fails. That flag
is diagnostic metadata, not a changed live game state. These limitations are
recorded, not hidden by rewriting the still-owned runtime/save implementation.

The combined journey covers actual entry, enabled production components, two
WORLD geometries and return, native movement, visible picker playback, exact
sampled corners, two native-shaped synthetic UI images at 1x/2x, pause/input
release, replay/snapshot saves, changed state, identical continuation, failed
WORLD/art/clip/save ownership, the expected cap, reset, selection/re-entry,
focus loss and teardown. Existing shell tests also cover archive exit/return and
keyboard navigation. No physical mouse or native OS file picker was operated:
scripted UI signals and shell input drove the export.

## Commands and local evidence

From this worktree (all exit 0 unless the final CTest section says otherwise):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
ctest --test-dir build/native --output-on-failure -j 3
```

Native build passed. Boundary checker: 73 unique existing paths; 38 tests pass.
Python tooling: 712 tests, OK, 385 skipped. Packaging unit-test mocks do not count
as platform execution. Native full-run result is recorded in the closing section.

Both extension variants were built here, without borrowing a sibling binary:

```sh
for target in template_debug template_release; do
  cmake -S gdextension -B "build/mac-$target" \
    -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" \
    -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET="$target" \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
  cmake --build "build/mac-$target" --config Release --parallel 4
done
```

Godot is `../integration/work/runtime/Godot.app/Contents/MacOS/Godot`, verified
`4.5.2.stable.official.6ce3de25a`; read-only godot-cpp pin is
`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`.

The full headless batch passed: bounded editor import, archive, frame-rate,
facing, recovery, sandbox, texture-upscale, UI gallery, control-animation-cache,
level-library, animation-preview, original-art, scene-play-flow, combined wave
and default launch. Exact argument arrays, elapsed times and statuses:
`work/playable-wave/godot-results.json`. Logs: `godot-*.log` in that directory.
Final changed scene scripts were rechecked in the actual release export.

```sh
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --editor --quit-after 120 --frame-delay 50
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --script res://tests/playable_wave_smoke.gd
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --export-release 'macOS universal'
ditto -xk build/exports/macos-universal/OpenChampions.zip build/exports/wave1-final
```

Export logs and final execution argument arrays are `export-final.log` and
`export-results.json`; hashes/architecture slices are `export-artifacts.json`.
The embedded release extension equals the locally built release extension byte
for byte. Both executable and library have arm64/x86_64 slices; only arm64 was
executed, using macOS OpenGL 4.1 Metal Compatibility on Apple M5 Pro.

Reproduce the graphical combined run (use 960x720 and a different capture folder
for 4:3; start the executable without test arguments for normal entry):

```sh
'build/exports/wave1-final/Open Champions.app/Contents/MacOS/Open Champions' \
  --resolution 1280x720 --script res://tests/playable_wave_smoke.gd -- \
  --capture-dir="$PWD/work/playable-wave/captures-16x9"
```

Actual export headless routes: animation 42 checks, art 63, shell 62, combined
68, sandbox 124, browser 283, plus normal default launch: all pass. The browser
route requires both `--browser-smoke-test` and `--smoke-test`; an initial manual
invocation omitted the latter and was terminated, then corrected successfully.
The final recorded batch uses the correct route and exits normally.

Final graphical combined runs: 102 checks at **1280x720** and 102 at **960x720**,
zero failures. Captures: `work/playable-wave/captures-{16x9,4x3}/`, 16 PNGs each:
entry, library, level, animation picker/zero/four, art picker/1x/2x, pause, loaded
save, WORLD/art/clip/save refusals and expected cap. Inspected actual mesh changes,
shaped synthetic pixels, legible picker/error text, reachable Close and menu
controls, and narrow/wide scrolling. These are synthetic diagnostic visuals.

A final narrow exported stopped-state capture exposed a stale OptionButton
minimum-width cache after save formats were rebuilt while its menu was hidden.
The shell now reserves width from the actual advertised labels plus control
padding. The graphical regression verifies that the selected format is legible;
no supported method, format selection or serialization behavior changed.

Source integration commit: `cb1052bd5bc09a3160e963a330d73825c5b04743`. Final runtime/scripts in the export
match that source; final handoff documentation is committed separately.
Final ZIP SHA-256: `7bb025e1b79920aae1f7fd916b6eb24ba174964376e78d84927ebda5a4e919a9`.
Local/embedded release dylib SHA-256: `86fd38306973527599bf4cf6768aa0d184bd5f6ebf5a13f6246ce165a35e830d`.
Source subtree IDs and all 48 capture hashes: `work/playable-wave/source-capture-provenance.json`.

The brief's additional **800x600** layout also passed the same 102-check actual
export flow. Sixteen further PNGs are in `work/playable-wave/captures-800x600/`;
log: `exported-combined-800x600.log`. Entry, art Close, save failure and capped
session controls remain readable/reachable with scrolling. No code change was
needed for this additional size. Total final captures: 48 across three sizes.

## Full native sweep

`ctest --test-dir build/native --output-on-failure -j 3`: **151/152 passed**,
1167.79 seconds, exit 8. The only failing result was `player-exact`, which hit
its unchanged 240-second limit under host contention (240.04 seconds). Every
other native test passed, including all merged Skills-A/B/C and shared-kernel
checks, the constructor supplement, water and boundary tests.

Full log: `work/playable-wave/ctest.log`; merged skills breakdown:
`work/playable-wave/merged-skills-results.json`. The prescribed isolated retry
is `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'`;
it **passed 1/1 in 196.75 seconds**, exit 0. No timeout, corpus or numerical
flag changed. All 152 named CTests have passing evidence across the original
full run and this isolated rerun; the original full command remains 151/152,
not a fully green sweep. Retry log: `work/playable-wave/ctest-alone-player-exact.log`.
