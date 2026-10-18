STATUS: COMPLETE

Wave-9 native audio composition, full CTest and final exported graphical
verification are complete. All required checks pass with the documented gates.
Worktree: `/Users/syndicatemike/Projects/open-champions/worktrees/playable-integration`.
Branch `codex/playable-integration`; starting clean tip `149ebd7`.
Reference-function coverage unchanged; client integration only.

| Released branch | Source tip | Local merge | Owner status |
|---|---|---|---|
| codex/native-audio | bd7bd54 | df4633c | COMPLETE with documented gates |

Read the full owner `work/loop-native-audio-r3-report.md` and `docs/NATIVE_AUDIO.md`
before merging. Additive conflicts in native CMake, main extension CMake,
registration and ROADMAP retained all earlier modules and added audio.
No other branch was merged. The native decoder/mixer and owner test remain
byte-identical to bd7bd54; the bridge adds a read-only observation method.

## Composition

Every successful play ready tick emits an owned audio packet, including catch-up
batches and ordinary, combat and level sessions. Non-ready, paused, stopped,
failed and legacy diagnostic advances do not tick sound. The scene routes typed
ClipRole events before native `sound_tick(0)`; AudioStreamGenerator pulls never
advance policy, voices or simulation. Output is 800 frames per tick at 48 kHz,
with the owner's 16-tick FIFO bound. Fast headless/accelerated runs may drop old
PCM as documented; this is separate from exact event and simulation timing.

All VAG/VAT data is authored synthetic test tones. No user sound files were
supplied, searched for or read. Menu, level, ambient and explicit Broon boss
selection invoke the real native policies and documented names. Boss override
refusal is retained. The existing stationary combat carriers are not inferred
to be bosses; explicit scene controls and typed boss enter/leave hooks are used.
The initial level countdown of one, zero cosmetic draw, preview volume and sample
associations are client fixture choices, not recovered retail behavior.

Play has no render/audio-device authority. Entry-menu sound uses a separate
presentation clock only before a session starts. Pause freezes the mixer while
already queued device PCM drains. Reset/new play/continuation load restart the
cosmetic mixer at the saved native tick origin; no audio state enters saves or
changes their formats. Invalid loads retain the prior owners. Camera panning uses
the scene camera converted to native XY-floor/Z-up coordinates.

Native generated FatCyclops walk dispatch (state 1, entry 16) emits actual 0x4 at
tick 13 and 0x8 at tick 49 over authored animation bytes. The new headless native
check proves voice allocation and nonzero mixed PCM on exactly those ticks, plus
level music selection/start at native queue tick 4. It does not claim FatCyclops
AI or a walking scene actor. The scene's stationary Agony animation fixture
routes its real attack events with ClipRole::attack: two native voices on tick 37,
alongside the player's exact HP drop. An attack 0x4 is refused as a footstep.

Unknown queue flag meanings 0x40/0x100 stay stored/counted only. Intro/cutscene drift,
ducking, mute-for, delayed starts, original ANM sound mappings, group-volume tables,
nonzero VAT restart, SPU exhaustion, interpolation and hardware mix remain gated.
No reverb or new gameplay action dispatch is introduced. Earlier level, texture,
FX and combat refusals remain in place. See `docs/PLAYABLE_AUDIO.md`.

## Development correction

An initial combined test left the scene in animated-combat mode before inherited
refusal checks expected ordinary play. The test now restores a valid normal
continuation before those checks. No production simulation fix was needed.
`initial-combined.log` preserves that failure; `fixed-combined.log` is green.

## Evidence boundaries

Tests use real local C++/Godot modules and authored fixtures. Exported UI checks
invoke actual controls/signals and native methods; viewport captures are actual
rendered output. No manual keyboard/mouse interaction, heard speaker output,
physical Intel hardware, Windows/Linux execution, retail asset parity or complete
original game is claimed. x86_64 is Rosetta on this Apple Silicon host.

Owner research pins and numeric policies are recorded in `docs/NATIVE_AUDIO.md`
(audio a9593d2..834806e) and creature dispatch 4f9b057. Current local released code,
owner contracts and fresh tests are the evidence; fresh source-research execution
is not claimed. No private asset acceptance was possible or attempted.

## Build and targeted results

- Release native plus universal debug/release main and texture extensions: ten
  configure/build commands pass, all builds limited to `-j4`. Water root points
  to this integrated tree. `build-results.json` records exact commands/timing.
- Targeted native CTest: native-audio, playable-audio and animated-combat-play
  pass 3/3. Audio core has 185 checks; the new composition has 164 checks.
- Both audio executables also pass x86_64/Rosetta, with stdout identical to arm64.
  `architecture-results.json`, `arm64-*.log` and `x64-*.log` preserve the results.
- Python tooling: 714 tests, 385 expected skips, pass in 25.141 seconds.
- Development Godot matrix: 31/31 commands pass. Final combined headless check
  has 2,563 assertions; owner audio smoke has 18 checks and pushes 16,000 frames
  through real AudioStreamGeneratorPlayback.
- Four released native audio files and all 118 prior vector files are unchanged;
  see `source-audit.json`. No existing reference corpus was weakened.
- Local implementation commit: `634c0dc`; merge commit: `df4633c`.

Godot's isolated audio smoke and some combined runs intermittently print the
owner-known ObjectDB shutdown warning. `audio-exit-verbose.log` identifies an
AudioStreamGeneratorPlayback reference retained at immediate exit (three in the
combined verbose flow), after the
bridge detaches and player stops. This is recorded separately from passing
assertions/exit status; no claim of warning-free shutdown or heard output.

## Exported flow and captures

Pinned Godot 4.5.2 exported with `--export-release 'macOS universal'`; ZIP unpacked
locally using `ditto -xk`. All **22 exported commands pass**. The combined 1280×720
run reaches 36,000 ticks and continues after native save/load, with **108,758
assertions**, zero failures, stable warmed capacity and 6,650 peak retained
records. The complete exported process took 121.69 seconds (accelerated logical
time, not a ten-minute wall-clock soak). The 960×720 and 800×600 versions each
pass **2,678 assertions**. Exported combined headless passes **2,563**.

The flow retains real level-library loading, manual animation picker, original-art
HUD/inventory panels, fight/death/removal, saved-level gate travel, tick-37 damage,
FX bursts/decals and decoded/textured WORLD lighting. New audio captures are:

- `work/playable-wave9/ui-captures-16x9/audio-01-level-music.png`
- `work/playable-wave9/ui-captures-16x9/audio-02-boss-music.png`
- `work/playable-wave9/ui-captures-16x9/audio-03-loaded-continuation.png`
- `work/playable-wave9/ui-captures-16x9/audio-04-tick37-hit-voices.png`

Matching audio captures are in `ui-captures-4x3/` and `ui-captures-compact/`.
Earlier journey states, long-play, save/load, HUD, inventory, fight, gate, FX and
texture images remain in the same folders. Base flow captures are in
`captures-16x9/` and `captures-4x3/`. Actual images were inspected for visible
status/HP, actor rendering, textured lighting and compact layout; no image editing
or mocked screenshot was used. The native lighting pixel comparison changes
6,538 pixels in widescreen/4:3 and 2,984 in compact mode.

App: `build/exports/wave9-final/Open Champions.app`.
ZIP: `build/exports/wave9-final/OpenChampions.zip`.
ZIP SHA-256: `463fb7d4e87c9d7dae07827afe5896a50a9658ed7588836320b02922d4eabb70`.
Main universal extension SHA-256:
`0d5f132e58f985b6cea39c4745d30e8bdd88c90da456d0d84bd230e05b25ac17`.
Texture universal extension SHA-256:
`05264dc90fcbc1eaa184db9c33a16c5398565bb7bba069b14f3822e03958b2a0`.
Packaged libraries are byte-identical to their local built counterparts.
`export-artifacts.json` records sizes, SHA-256 values and `lipo -archs` results.

Reproduce from this worktree:

```sh
python3 work/playable-wave9/run-builds.py
python3 work/playable-wave9/run-validation.py
python3 work/playable-wave9/run-godot.py
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --export-release 'macOS universal' ../build/exports/wave9-final/OpenChampions.zip
ditto -xk build/exports/wave9-final/OpenChampions.zip build/exports/wave9-final
python3 work/playable-wave9/run-export.py
```

Interactive launch:

```sh
open 'build/exports/wave9-final/Open Champions.app'
```

Explicit exported headless composition:

```sh
'build/exports/wave9-final/Open Champions.app/Contents/MacOS/Open Champions' --headless --script res://tests/playable_wave_smoke_audio.gd -- --ready-ticks=640
```

No push, PR, issue, canonical-tree/source edit or protected-directory access.

The final capture set contains **200 PNGs**. The combined verbose teardown check
also passes 2,563 assertions and identifies three playback references at immediate
shutdown (`combined-exit-verbose.log`); the exact engine-side lifetime cause has
not been established. No generic assertion failure is hidden by this warning.


## Final full validation

Full Release CTest `ctest --test-dir build/native --output-on-failure -j 3` passes
**175/175**, zero failures, **599.87 seconds** (runner wall time 599.88 seconds).
`player-exact` passes in the aggregate in 112.01 seconds. No timeout or isolated
rerun was necessary. The suite includes all skills A/B/C and prior wave contracts,
plus native-audio and playable-audio. See `work/playable-wave9/ctest.log` and
`validation-results.json`.

`final-summary.json` reconciles all ten successful build commands, 31 development
Godot commands, 22 exported commands, native/Python results, warnings, artifacts
and 200 captures. `capture-manifest.json` records every image's path, dimensions,
size and SHA-256. `ancestry-audit.json` confirms all 16 requested released tips
from waves 1–9 are ancestors of the integrated local branch.

`docs/INTEGRATION.md`, `docs/PLAYABLE_AUDIO.md` and the original wave index record
the completed result and retained limitations. The only post-implementation
changes are documentation/reporting; tested executable and scripts are unchanged.

## Wave 10 — level-flow-2 — 2026-10-18

Read the complete owner r1 report, then merged released `2c57ba3` as `103658a`
without conflicts. It builds on the already integrated `1cbd5db`. Native contracts
now cover first exact `.world` in stored order, base/chunk lighting, lost recall
during cutscene expiry (network F+59/offline S+60), and exact `move_piece` fields.
Wave-5 host/bridge never access `Transient::recall_countdown` directly, so the new
`RecallCountdown` type requires no wiring edit. The stale refusal paragraph in
`PLAYABLE_LEVEL_FLOW.md` is corrected; scene scope remains the existing gate and
saved-level fixture, not newly claimed recall or moving-chunk visuals.

Release native and both universal main-extension templates build with `-j4`.
Focused `level-flow-exact`, `level-flow-water-adapter`, `level-play`: **3/3 pass**.
The integrated level-flow executable reports **10,123 exact checks**.
Existing Godot combined level flow: **1,631 assertions**, zero failures.
Full CTest: **175/175 pass**, 602.89 seconds, no timeouts or reruns. Commands/results/logs: `work/playable-wave10/`.
No new export/captures requested or produced; wave-9 exported evidence above is
unchanged. No retail/source-research execution, push, PR or issue. Reference-function
coverage accounting unchanged here; this round integrates the released contracts.
