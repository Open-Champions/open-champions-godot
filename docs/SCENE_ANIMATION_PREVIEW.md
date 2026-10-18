# Scene actor cosmetic animation preview

`res://scenes/scene_animation_preview.tscn` is a Control that exposes
`configure(inspector, visual_parent)`, `show_picker()`, `observe(observation)`,
`set_suspended(bool)` and `clear_preview()`. Its `preview_changed(active)` signal
lets the hosting scene hide its own static visual while the cosmetic mesh exists
and restore that visual on any refusal or clear. The host retains actor position,
facing, collision and authoritative native state. The preview creates one
`MeshInstance3D` beneath the supplied visual parent.

The user opens a local GOB/LMP, selects explicit `.vif` and `.anm` **archive row
indices**, and presses **Use selected pair**. The picker retains the exact
`inspect_archive` fingerprint and the Return to Arms profile. Duplicate names
remain separate selections. Every pose is computed by the existing
`sample_skinned_model` native bridge, starting at tick zero. The bridge checks
fingerprints on every call, so a changed archive clears the mesh and reports an
error. The UI always says **pairing unverified**. Successful arithmetic does not
establish a retail resource association.

**Play** advances one authored cosmetic sample tick for each increase in the
host's native `ready_ticks` observation. Duplicate observations do nothing;
skipped observations advance by the exact counter difference, clamped to the
clip duration. The preview stops at that duration. **Pause** freezes it;
**Restart** samples zero and rebases the counter. Counter decreases, focus loss,
and suspension rebase without catch-up. The host must call `clear_preview()`
after a save load with potentially equal counters. These rules are a new client
presentation policy. They do not implement the game's animation state, clip
dispatch, blending, events, root motion, or looping flags. Keyboard direction,
filenames and actor reference IDs do not choose or switch clips.
**Hide picker** leaves the cosmetic mesh active so the actor remains visible
during movement. **Close preview** releases the mesh and restores the static
visual. Scene removal does the same.

The preview validates all returned meshes, including hidden meshes, before
publishing geometry. It draws only default-visible triangles, maps native Z-up
to Godot Y-up once, and uses an unlit, two-sided material without normals,
textures or equipment. The initial tick-zero sample fixes one cosmetic scale
and offset for the selected pair; later poses do not refit or move the actor
root/camera. Empty, hidden, malformed, nonfinite, stale, attachment and partial
body results are refused. Clearing drops the mesh reference and emits
`preview_changed(false)` to restore the host's original static appearance.

Evidence: the bridge and its supported position-only result are documented in
`SKINNING_BRIDGE.md` and `SKINNING_POSITIONS.md`. The research checkout at
`a2b145b6b22f3c0451f861ed70faa1827f94bf09` documents resource lookup
without validating a chosen pair in `docs/formats/anim-audio.md` §2.8. Its
`docs/spec/animation.md` and `tools/ref_anim.py` describe the original runtime
state/cache separately from this presentation policy. The research scene tap
is a live recompiled renderer and does not establish selected pair semantics.

Reference-function coverage unchanged; client integration only. Public tests
use authored VIF/ANM fixtures and cannot verify retail pairing or original
animation behavior. Private retail validation is absent on this machine.
