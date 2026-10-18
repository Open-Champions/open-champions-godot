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
