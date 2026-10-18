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
