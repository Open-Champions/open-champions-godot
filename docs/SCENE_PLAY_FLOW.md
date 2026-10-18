# Scene entry and pause flow

`play_flow.tscn` is the authored host shell; it extends the existing sandbox.
The sandbox remains directly runnable and both exported smoke switches route to
it. Native simulation, 60 Hz accumulation, display interpolation, native save
formats and atomic terrain publication remain owned by their existing classes.
No original menu layout, spawn, game rule, asset pairing or art is invented.

Entry starts paused. Open a local WORLD through the existing atomic loader or
explicitly start the bounded diagnostic on practice ground. Opening menus,
returning to selection and resuming preserve the live controller. Only explicit
reset, successful level/load or explicit play start replaces native state.
Modal entry and focus loss release movement/damage and discard clock debt.
A successful reset/load stays paused for an explicit Resume. Diagnostics retain
the native 512-tick bound (256 for some optional policies) and show that limit.

The original sandbox methods and diagnostic switches remain available. Its
controls are expandable in the shell; the original branding, ground and static
visual are retained. Short windows scroll the information/navigation region.

Evidence: integration baseline `60def02263f2bcb9685cb06c5a0c7409ac5ccab4`,
`SESSION_CONTROL.md`, `WORLD_GEOMETRY.md`, `NATIVE_SAVE.md`, `FRAME_RATE.md`
and `GODOT_BROWSER.md`. Read-only research pin
`a2b145b6b22f3c0451f861ed70faa1827f94bf09`,
`docs/spec/movement-camera.md` SHA256
`3d4c9aa29a3e37e8054f834cabdf6eb392b69aeb856af1a8a4db1fd94b34f73b`
retains the 60 Hz/coordinate evidence; no original function was newly ported.
Reference-function coverage unchanged; client integration only.

## Optional component contract and test seam

The shell uses `ResourceLoader.exists/load` only for the three PLAN.md paths:
`scenes/level_library.tscn`, `scenes/scene_animation_preview.tscn` and
`ui/original_art/original_art_panel.tscn`. It validates the frozen method/signal
names before configuring a component. Missing/mismatched components give disabled
actions with the exact install path/reason; the baseline runs without them.
The library receives the existing atomic terrain callback and a pause/release
callback. Animation receives the inspector and actor, plus raw observations and
suspension; only `preview_changed(true)` hides the static actor. Accepted load,
reset, replacement appearance and exit clear the preview; menu toggles do not.
Art lives in the diagnostic information region and cannot replace the health
readout. Its controls pause before opening a picker. All placements are authored,
not assertions of original HUD composition. Pairing remains explicitly unverified.

`component_factory: Callable` is a pre-ready injectable lifecycle-test seam.
The named test's fixture is only under `tests/scene_play_flow*`; it implements no
parsing, native simulation or sibling feature. It validates shell arguments,
observations, suspension, refusal and teardown. These tests do NOT establish real
component acceptance. Production resolves only the exact frozen scene resources.

Run the real-extension suite:
```
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/scene_play_flow_smoke.gd
```
Optional `-- --scene-play-flow-captures=/absolute/ignored/work/directory` on a
graphical run captures entry, running, paused, failure and narrow-window states.
Keyboard events are dispatched using `Input.parse_input_event`; component events
are isolated lifecycle spies; save/terrain operations use the real native binding.

## Play capability boundary

The baseline has neither `get_play_capabilities()` nor `start_play_session()`.
The runtime worker's 2026-10-17 audit records an ownership blocker in
`objects_lifetime.cpp`: legacy World checkpoints reject frames above 2,000.
No continuation capability or byte format is available to integrate yet. This
shell deliberately retains the diagnostic cap and never resets it automatically.

When the methods exist, `available` gates explicit native start and
`history_policy` is displayed. Save choices come only from `save_formats` (or
`supported_save_formats`). The small adapter accepts an array of native
descriptors `{label, method}` for existing `save_file` / `save_snapshot_file`
entrypoints, or explicit `replay`, `snapshot`, `continuation` names. Unknown
formats/methods disable Save with an explanation. No continuation serializer,
version, byte interpretation or fake runtime is supplied here. The native owner
must confirm the final descriptor schema in integration; adapter schema tests
are not continuation-save acceptance.

A loaded new runtime must identify mode via `play_mode: bool`, `mode`, or
`session_mode` (`diagnostic`, `play`, `rta-play-session-v1`). Unknown mode disables
Save instead of relabelling it as legacy. The old bridge has no discriminator
and retains its known diagnostic mode. These are presentation adapters, not
changes to native save meanings. Optional diagnostic switches are unavailable
in play/unknown mode; the existing diagnostic scene remains directly runnable.

## End-to-end integration acceptance script

Run this after the owners release their scopes and the integration worker merges
the real components. Do not inject `component_factory` in this acceptance.

1. Build the combined native extension in Release with `-j 4`, import using the
   pinned Godot, run each worker's named real-extension suite, and export the
   actual combined application. Preserve the sandbox/browser smoke switches.
2. Open the export at 1280x720 and 800x600. Verify initial selection is paused,
   Tab/Shift-Tab/Enter navigate visible enabled controls, and explanatory
   capability/missing-art text and status remain readable (scroll if needed).
3. Confirm all three exact component resources loaded and their actions are
   enabled. Use the real level library to load two supported synthetic WORLD
   fixtures, switch back, cancel, and reject a malformed/missing/water/moving
   input. Record actual native terrain/position and mesh identity, not just a
   selected row. Failed replacement must retain the current session/appearance.
4. If/when owner-provided private levels become available, explicitly choose one
   through that same library and record counts only. Private data is absent for
   this branch: do not substitute synthetic acceptance for retail level proof.
5. With the real play runtime available, confirm the capability format schema,
   start once, hold/release WASD and the authored H health diagnostic. Advance
   beyond 512 ready ticks (and run the runtime worker's 36,000-tick soak). Assert
   absolute counters remain monotonic, no automatic reset/reseed occurs, and
   retained record counts obey the native policy. Until the runtime blocker is
   resolved this step is BLOCKED, not a reason to bypass the diagnostic limit.
6. Hold movement and H while opening pause, Frame rate, library and pickers;
   release focus, close/cancel, wait, then Resume. Check native position/HP stay
   frozen during modal time and no delayed direction/damage or clock catch-up
   happens. Opening/closing selection alone must preserve the native owner.
7. Save using every format actually advertised for the current native mode.
   Move, reload, and compare future states/RNG with an uninterrupted native peer.
   Successful load/reset snaps position/facing and stays paused. Corrupt/unknown
   save and unwritable destination must retain state and show the native error.
   Confirm legacy diagnostic replay/snapshot bytes remain unchanged.
8. Through the real animation picker, explicitly select supported synthetic
   VIF/ANM rows; compare two native-ready-tick samples against the native bridge.
   Observe geometry changes, freeze on pause, and retain selection across menu
   toggles. Failure/clear restores static appearance. Reset, equal-counter load,
   successful terrain and static-asset replacement clear/rebase the preview.
   Keep the cosmetic/unverified-pairing label visible; no inferred retail pairing.
9. Through the real original-art panel, select the documented image identities.
   Verify native-shaped pixels, 1x/2x scale, missing/error recovery and unchanged
   Session bytes; preserve diagnostic health/mana alongside the images. Original
   HUD composition and unavailable private visual fidelity remain unverified.
10. Return to selection, resume, explicitly reset the current ground, and enter
    archive tools. Check component/native/mesh release on navigation/exit. Inspect
    rendered entry, moving, paused, failure and resized states from this export.
    Record the actual platform/architecture, commit, commands and image paths.

## Local verification boundary

The shell's named suite uses the actual locally built extension for movement,
terrain publication, saves, interpolation and refusal. Optional component spies
only exercise adapter wiring. The original sandbox, facing, recovery, cache and
frame-rate suites remain unchanged. The exported graphical suite dispatches
keyboard events and invokes UI/component callbacks; captures come from the
actual exported viewport. This is scripted interaction, not manual desktop input.
The Mac debug export uses the universal Godot template with this worktree's
Release-optimized arm64 debug extension; it is not a validated universal release
package. Windows, Intel hardware, signing, private retail levels, real merged
components and play continuation beyond 512 ticks are not established here.

Verified locally on Apple Silicon macOS with Godot 4.5.2 `6ce3de25a` and
`godot-cpp` `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`: named headless suite
62 checks, exported graphical suite 69 checks, legacy sandbox 124, facing 62,
recovery 40, cache 30, frame-rate zero failures, exported browser 283. All passed.
The shell-owned archive visit returns through the legacy Movement test button to
paused shell selection; standalone sandbox launches and smoke routes are kept.

Final source/export execution logs and seven viewport captures are under ignored
`work/*-final.log` and `work/captures/`. The source and embedded debug extension
SHA256 both equal
`2d188dc834b28add3527621028002071676d87a0538896d4e26b4d3a9c35543c`.
The final round report records the full native CTest result separately.
