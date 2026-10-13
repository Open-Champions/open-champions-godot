# Frame rate and original RTA pacing

Rendering is uncapped by default (V-Sync off, `Engine.max_fps = 0`). The
Frame rate button offers 30/60/120/144/240 FPS, display refresh or unlimited,
V-Sync off/on, adaptive when supported, and a rolling FPS/frame-time readout.
Preferences are stored in `user://frame_rate.cfg`, never in the repository.
Unknown display refresh leaves the cap unlimited. Refresh is rechecked while
that option is selected. Godot's integer FPS cap rounds display refresh to the
nearest integer. V-Sync or the platform driver can impose its own limit.

Godot 4.5 [DisplayServer documentation](https://docs.godotengine.org/en/4.5/classes/class_displayserver.html)
restricts adaptive V-Sync to supported displays and Forward+/Mobile renderers.
This project's Compatibility renderer therefore disables the adaptive option.
The [Engine API](https://docs.godotengine.org/en/4.5/classes/class_engine.html)
provides the presentation FPS cap. No renderer or asset pipeline was replaced.

## Evidence for the tick

Read-only research checkout: `../../champions-return-to-arms-re`, commit
`356add27d50e50972b2a671b31e8f4ec93e6778c`; the four consulted documents were clean.
This evidence concerns RTA **SLUS_209.73**, not Norrath or an unvalidated PAL
revision. The logical simulation step is **1/60 second (60 Hz)**:

- `docs/spec/movement-camera.md` §0 identifies the original raw `DT` as
  `0x3c888888`, below the nearest IEEE float to 1/60. Movement multiplies by
  that raw constant inside the original modeled operation. Host elapsed time
  never replaces it. The specification also defines facing as 65536 units per
  turn, zero along native +X and 0x4000 along +Y.
- `docs/mainloop.md` §§3,5,6 documents `tasks_run` (0x3226e4),
  `engine_frameStep` (0x19e3a8), and `gfx_endFrame` (0x19daf8).
  Full-rate mode runs one tick per displayed frame; end-frame waits for vblank.
  Half-rate mode holds a frame for two fields and normally runs two ticks from
  the same input sample, clearing button edges for the second tick. Full-rate
  missed fields slow the original simulation; there is no unbounded catch-up.
  After 32 consecutive late frames, or the terrain's 22-frame threshold, it
  switches to half rate. Three or more fields still yield at most two ticks.
  Network input readiness can yield zero ticks while drawing continues.
- `docs/spec/player-tick.md` identifies `Player_run` once per simulated tick
  for real players; the prediction copy is separate. Its timers and action
  frame counts belong to simulation, not to rendering.
- `docs/runtime.md` P8 finds no clock/hardware timer reads in its examined
  runtime range. That range does not establish an alternate gameplay cadence.

The native client's accumulator preserves the documented **logical 60 Hz step**;
it intentionally does not reproduce console slowdown or couple desktop render
frames to vblank fields. Exact NTSC display-field frequency was not measured on
console hardware here. Native movement arithmetic, scheduler order, RNGs, input
pipeline, histories and existing scene budgets remain unchanged. The old scene's
30 Hz physics submission rate was authored pacing, not evidence of original timing.

## Ownership, interpolation and input

`native/include/champions/frame_clock.hpp` is a Godot-independent presentation
clock, outside Session snapshots. It accumulates integer microseconds multiplied
by 60 against a one-million-unit threshold, preserving the fractional 1/60
cadence without rounding every tick to 16667 microseconds. Active slow frames
run every owed submission, including multiple ticks per render; no ticks or
elapsed time are silently dropped. The existing 512-ready-tick diagnostic budget
still stops the scene, now about 8.55 seconds after startup at normal pacing.

The scene samples monotonic `Time.get_ticks_usec()` in `_process`, rather than
Godot's potentially clamped delta or `_physics_process`. Input collection remains
local. It submits integer command bits to the thin bridge. H's queued edge is
retained until a submission occurs; held input is shared across catch-up ticks,
and native edge/pipeline rules still decide when damage executes. Identical
**tick-indexed inputs** have identical traces; inputs from a live keyboard can
naturally arrive at different boundaries on different machines.

The bridge caches the last two native actor poses and exposes them plus the
accumulator fraction. GDScript only interpolates their positions and shortest-path
facing for display, maps native X/Y/Z to Godot X/Z/-Y, and follows that display
with the camera. No interpolated values are passed to Session. Interpolation
introduces one fixed tick of presentation latency, without changing authority.
The current authored movement policy does not turn the actor; facing is projected
from World and never inferred from keyboard direction.

Reset, terrain-controller replacement, stop and pause collapse both poses and
clear presentation debt. Paused rendering remains free to run while authority
stays frozen. Resume and focus/dialog release discard old elapsed time rather
than catching up paused time. Future authoritative teleport/save-load paths must
call `snap_presentation()` immediately after publishing the discontinuity; there
is currently no general teleport command in this Controller. No distance threshold
or invented teleport heuristic changes gameplay.

A measured bottleneck was full World/Session snapshots used for scalar display
reads and the singleton isolation check. Small const projections now read the
same owned fields and all live retained actor identities (including actors absent
from the current registry), without copying the entire grid and histories. A
callback-free reference validator retains foreign/null registry, link, object
and iterator refusals; the raw-handle preflight runs the same validation without
materializing an unused snapshot. All
histories and complete snapshots remain available. These are observation changes,
not simulation-rule changes.

## Validation and Mac measurements

Measured 2026-10-13 on Apple M5 Pro, Mac17,9, 48 GiB RAM, arm64 macOS,
Godot 4.5.2 `6ce3de25a`, Compatibility renderer, Release-optimized debug ABI
extension. Four-second headless runs held the same authored +X command after a
0.5-second warm-up. Samples count actual process callbacks using monotonic time;
p95 includes simulation work and cap waiting. Each row was run sequentially,
though other project jobs and native tests were active on this Mac.

| Cap | Process FPS | Mean frame ms | p95 ms | Process elapsed s | Ready ticks | Simulation elapsed s | Ticks / real s |
|---|---:|---:|---:|---:|---:|---:|---:|
| Unlimited | 138.993 | 7.195 | 11.112 | 4.000193 | 240 | 3.999500 | 60.008 |
| 60 | 60.045 | 16.654 | 22.777 | 4.013683 | 240 | 3.999852 | 60.002 |
| 30 | 30.250 | 33.058 | 41.906 | 4.000036 | 239 | 3.970274 | 60.197 |

Tick counts are paired with the monotonic timestamps of completed scene updates,
not the earlier SceneTree measurement callback. This avoids mixing counters and
endpoints from different frames during catch-up. The count has less than one
fixed-tick phase uncertainty at these endpoints. The first Session submission
still waits for input readiness; measurements begin after warm-up. The 30 FPS
row demonstrates two submissions per render interval while authority retains
60 Hz. These are local observations, not isolated benchmark guarantees.

A fixed **120 `advance(1)` command** workload retained 119 ready ticks in both
versions: **11,396,540 us** with the original full-snapshot observation/isolation
path versus **610,488 us** with the final const projections and complete reference
validation. The baseline bridge already had pose caching but bypassed the new
accumulator via manual advancement; it used the old full-snapshot native reads.
The diagnostic command was `"$FPS_GODOT" --headless --path godot --script
"$PWD/work/native_measure.gd"` (and the same script with `--path
work/baseline-project` before optimization). That ignored script instantiates
`ChampionsSessionController`, times 120 calls to `advance(1)` using
`Time.get_ticks_usec()`, and reads `ready_ticks` afterward.
No simulation input, arithmetic, limit or history was removed. Host contention
was uncontrolled, so this is diagnostic evidence for the snapshot bottleneck,
not a general performance multiplier.

The new `frame-clock-traces` test compares complete Controller, Session, World,
network, terrain, HP and creature state **after every submission**, including
initial states, full histories and callback results. The same 60-tick movement/
health script runs at 30, 60, 144, 240 and irregular uncapped render intervals
(including 73,123-us stalls), under all **16 arithmetic configurations**. It also
checks exact one-second submission counts, fractional carry, pause/reset debt,
stop during catch-up and negative elapsed-time refusal. Scalar projections are
checked against the full snapshot. This is byte equality of complete serialized
states, not position-only comparison or a synchronization hash.

The real bridge smoke test verifies interpolation without authority feedback,
pause/resume debt, reset/snap behavior, the network startup delay, shortest-angle
wrap, all cap choices, readout toggling and isolated `user://` config save/reload.
Source and latest exported Mac executable pass it with zero failures. The latest
export also passes **70 scene checks** and **283 archive-browser checks**.

The exported application was inspected graphically: cap menu, V-Sync menu with
adaptive disabled, readout toggle, a visible approximately 60 FPS / 16.67-ms
readout after selecting the 60 cap, reset and held-D movement. Its uncapped
readout exceeded 200 FPS after the diagnostic Session had stopped; that is UI/
render-loop evidence only, not the active-simulation measurement in the table.
The readout fits above Health and the Frame rate button fits at the footer. Test
preferences were restored to unlimited/off/readout-hidden. The final inspection copy
used a distinct local app name and bundle identifier to avoid export collisions
and controlling other agents' apps. Its defaults were verified graphically.
The official universal export template was used with an arm64-only extension;
this verifies arm64 execution, not Intel support or a distributable Universal2
package.

Reproduce the executed build, tests, measurements and export with:

```sh
FPS_GODOT="$PWD/../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot"
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake -S gdextension -B build/godot-extension \
  -DGODOT_CPP_DIR="$PWD/../../open-champions-godot/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension -j 4
"$FPS_GODOT" --headless --path godot --editor --quit-after 120 --frame-delay 50
ctest --test-dir build/native \
  -R 'frame-clock-traces|session-control.*host|raw-handles-host|session-creature-host|session-terrain-host' \
  --output-on-failure -j 3
ctest --test-dir build/native -R '^session-terrain-host$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
"$FPS_GODOT" --headless --path godot --script res://tests/frame_rate_smoke.gd
"$FPS_GODOT" --headless --path godot --script res://tests/session_sandbox_smoke.gd
"$FPS_GODOT" --headless --path godot --script res://tests/frame_rate_measure.gd -- 0
"$FPS_GODOT" --headless --path godot --script res://tests/frame_rate_measure.gd -- 60
"$FPS_GODOT" --headless --path godot --script res://tests/frame_rate_measure.gd -- 30
# Isolate Godot's Mac scratch folder by using a unique staging app name.
# Source names, settings and export presets remain unchanged.
python3 - <<'PY_STAGE'
from pathlib import Path
import shutil
stage = Path('build/fps-export-project')
shutil.copytree('godot', stage, ignore=shutil.ignore_patterns('.godot'), dirs_exist_ok=True)
p = stage / 'project.godot'
p.write_text(p.read_text().replace('config/name="Open Champions"',
                                  'config/name="Open Champions FPS Verification"'))
p = stage / 'export_presets.cfg'
p.write_text(p.read_text().replace('org.openchampions.client',
                                  'org.openchampions.fps-scaling'))
PY_STAGE
mkdir -p build/exports/fps-isolated
"$FPS_GODOT" --headless --path build/fps-export-project \
  --editor --quit-after 120 --frame-delay 50
"$FPS_GODOT" --headless --path build/fps-export-project \
  --export-debug "macOS universal" "$PWD/build/exports/fps-isolated/OpenChampionsFPS.zip"
ditto -xk build/exports/fps-isolated/OpenChampionsFPS.zip build/exports/fps-isolated-unpacked
FPS_EXPORTED="$PWD/build/exports/fps-isolated-unpacked/Open Champions FPS Verification.app/Contents/MacOS/Open Champions FPS Verification"
"$FPS_EXPORTED" --headless --script res://tests/frame_rate_smoke.gd
"$FPS_EXPORTED" --headless --script res://tests/session_sandbox_smoke.gd
"$FPS_EXPORTED" --headless --script res://tests/archive_smoke.gd
```

Godot's shared Mac scratch directory collided with simultaneous agents' identically
named exports. `TMPDIR` did not isolate that folder on this Mac. Earlier ZIPs were
incomplete or contained another branch's pack and were rejected. The final staged
export's unique name avoids this collision; inspection confirmed the FrameRate
autoload and both new test scripts inside its pack before the three smoke checks.
The repository app name and presets were not changed for this workaround.

The targeted native run initially found a null-registry dereference and then an
exception-type mismatch; the final const reference checks preserve the original
refusal and the dedicated terrain regression passes. An incomplete first full
run caught the simulation boundary's disallowed `unordered_set` include. That
container was replaced with the already allowed `set`, the boundary check passed,
and the final full suite was restarted against the rebuilt final version.
Final full suite: **101/101 passed**, `ctest --test-dir build/native
--output-on-failure -j 3`, **1029.62 seconds**. The final frame-clock matrix
passed in 105.79 seconds. LLVM 20 formatting and `git diff --check` also pass.

Logs, baseline staging and exported binaries stay in ignored `work/` and `build/`.
No original game content or private-validation input was used.

## Remaining verification

Other operating systems and Intel Mac execution, cross-arm64/x64 state comparison,
high-refresh displays, driver-dependent adaptive/on/off behavior, and animation
playback interpolation remain unverified in this branch. Headless FPS measures
process callbacks and elapsed frame intervals, not GPU throughput or displayed
pictures. The scene is still an authored movement/health test, not a playable port.

## Changed files

- Pacing and trace proof: `native/include/champions/frame_clock.hpp`,
  `native/tests/frame_clock_tests.cpp`, and the additive test registration in
  `native/CMakeLists.txt`.
- Const observations and reference validation:
  `native/include/champions/{objects,raw_handles,session,session_control}.hpp`
  and `native/src/{objects_lifetime,raw_handles,session,session_control}.cpp`.
- Thin bridge: `gdextension/src/session_controller.hpp` and `.cpp`.
- Client: `godot/project.godot`, `godot/scenes/session_sandbox.gd`,
  `godot/ui/frame_rate.gd`, `godot/tests/frame_rate_smoke.gd`, and
  `godot/tests/frame_rate_measure.gd`, including the three new script UID files.
- Evidence and timing correction: this guide and `docs/SESSION_CONTROL.md`.
