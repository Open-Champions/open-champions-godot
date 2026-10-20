# Review fixes, 2026-10-20

Branch `codex/review-fixes`, based on `codex/playable-integration` at `ba23958`. Input:
the read-only review of the playable-integration delta (waves 1–11). Each finding was
first reproduced with a test that fails, or crashes, on the unchanged code; then it was
fixed. A finding that could not be reproduced is recorded with the evidence. Synthetic
fixtures only.

| # | Review severity | Finding | Outcome | Reproduction (unchanged code) → test |
|---|---|---|---|---|
| 1 | High | Uncaught C++ exceptions in `ChampionsFxBridge` | **Fixed** | `godot/tests/fx_bridge_refusal_smoke.gd`, one case per process. Five of the cited methods aborted Godot with exit 134, an uncaught `std::domain_error` / `fx::render::Refusal`: `spawn_particle`, `fx_spawn` and `run_emitter` via `exact::trunc`; `tick` via a paired trail with one entry and via the clock passing 0x7fff. Now 33 checks pass. |
| 1 | | …the same methods: `set_detail`, `add_trail_point`, `update_beam`, `add_decal` | False positive for throwing | They have no throw path, which the same cases confirmed. They still gained `{ok, refusal}` answers and input checks. |
| 1 | | …`shade_*` | No exception, but real UB | `shade_world` with a NaN position (and any light node) hits `static_cast<int>(NaN)`; UBSan `float-cast-overflow`, `work/ubsan/lighting_nan.cpp`. Fixed: refused at the boundary. |
| 1 | | Audit of every other binding | See below | 77 bound methods outside the FX bridge were audited. |
| 2 | High | Combat / level fixtures cannot be saved | **Fixed** | The bridge refused saving. [Fixture continuation v1](NATIVE_SAVE.md#fixture-continuation-v1) is now covered by `fixture-save` (70 checks) and `playable_wave_smoke_full_combat.gd` (mid-fight save and load through the scene). |
| 3 | Medium | `Effects::with_shared` forks g_randSeed | **Fixed** | `playable-combat-fx` `shared_chain`: a burning StaticFire on the player. Before the fix, the World rolled damage from the stale seed and the published seed was the model's fork, dropping the damage draws. Now one hand-off chain. |
| 4 | Medium | Create failures return handle 0 | **Fixed** | `fx_bridge_refusal_smoke` `create_failures` (6 failed before). Now `{ok, handle}` or a refusal. |
| 5 | Medium | Missing particle definitions dropped silently | **Fixed** | `native_fx_smoke`, `fx_bridge_refusal_smoke`. Missing definitions are counted by address and named by spawn / fx_spawn / emitters / draw and by the Full `effects` observation. The tick does not fail, so that drawing and headless machines stay identical. |
| 6 | Medium | `draw` reports ok:true with refusals | **Fixed** | `fx_bridge_refusal_smoke` `draw_ok`. `draw` and `NativeFxRenderer.present` answer `ok: false` with the runs they still made. |
| 7 | Medium | Combat `presentation:fx_spawn` never reaches EffectSimulation | **Fixed** | `playable-combat-fx` `missile_fx_spawn`: before, the note had 0 arguments and was counted as unrouted. The projectile notes now carry ref_combat_ranged's exact ids and positions, including the 0x40000 → id 8 branch the port lacked. |
| 8 | Medium | Creature-anim Callables without teardown | **Fixed** (partly false positive) | `godot/creature_anim/freed_callable_smoke.gd`. Godot 4.5 object Callables hold an ObjectID, so a freed target did not crash. The binding did keep the dead callback, refused with the misleading "service must return an integer", and accepted float / NaN raw values (audit U2). Now the dead callback is dropped and refused by name, `is_valid()` is checked before each call, Callables are released on destruction, and integer Variants are required. |
| 9 | Medium | Dry `.ocplay` omits host audio and FX | **Accepted** | The brief keeps the dry format and its tests unchanged. This is a documented presentation reset: actor, HP and shared RNG match after load. The Full fixture continuation does carry the effect world. |
| 10 | Low | Cosmetic LCG / row-4 flipbooks advance per `present()` | **Accepted** | This is the original: research effects.md 13.5 and 17 put beam drift on the live cosmetic seed once per drawn frame, and row 4 steps every call. It is per machine and not in the sync hash; headless and drawn traces are identical (`playable-combat-fx`, the Godot FULL_TRACE). |
| 11 | Low | Refused recipe constants only `push_warning` | **Fixed** | UI-only, with no automated failing test: the reason now stays on the Full option's tooltip and the capability line. |
| 12 | Low | Draw-slot glows queued without a draw | **Fixed** | `playable_wave_smoke_full_combat.gd` swaps in a camera the bridge refuses during a Holy Strike. With the old order the glow queue grew 0 → 3 in three frames; now it stays empty. |
| — | Audio note | Mix out-of-bounds with a tiny loop | **Fixed** | `work/ubsan/audio_loop_oob.cpp`: a one-sample loop at the top pitch read past the vector (libc++ hardening). `add_effect` now refuses loops or samples shorter than the largest pitch step (`native-audio` `short_loops`). VAG loops are 28-sample blocks. |
| — | Audio note | `animation_event` without try | **Fixed** | Wrapped, with non-finite positions refused. |
| — | Audio note | Docs say the device drains the native FIFO | **Fixed** | PLAYABLE_AUDIO.md: the main thread drains into the generator; `Playback` must never be called from the audio thread. |
| — | Note | Throw after `combat_ = std::move(next)` in `start_play_session` | **Fixed by construction** | The combat scene (session and effects) is fully built before publication. |
| — | Note | `snap_presentation` unguarded | **Fixed** | Answers the observation or a failure dictionary. |
| — | Note | Non-Full `fx_spawn` always `near=true` | **Accepted** | There is no `isNearCameraFocus480` contract for the presentation bridge; this is cosmetic only. |

Audit of the other GDExtension bindings (77 bound methods; agent-assisted read, each
fix then tested):

- `snap_presentation` could throw across the boundary. Fixed.
- `Playback::load_stream` dereferenced an empty `std::optional` after a VAT directory
  replacement, which libc++ hardening traps. Fixed, with the `native-audio`
  `replaced_directory` test.
- Audio positions are now refused when non-finite (U3).
- Creature-anim integer Variants (U2) and `catch (...)` in the dispatch `attempt`:
  fixed.
- Accepted with reasons:
  - U4: `advance_time` catch-up is bounded by every session's ready-tick cap.
  - U5: finite double above FLT_MAX into float (units 1e300) gives IEEE ±inf in
    presentation-only vectors; no bound is documented to pick.
  - The controller constructor's latent throw: built-in practice terrain only.
- Deferred, documented:
  - L1: GDScript signal handlers can re-enter the controller during a tick; values are
    re-read after `emit_signal`, so nothing dangles.
  - L2–L4: creature-anim callbacks driving another binding on a shared Environment
    mid-tick, and reference cycles through Callables that capture the binding.

The review's own "likely false positives" table was not re-investigated.

A flaky check was seen once in three runs:
`playable_wave_smoke.gd: stale clip clears cosmetic owner and restores static mesh`. It
rewrites an LMP and restarts the clip immediately, which looks timing-dependent. It is
unrelated to these changes and passed on the two reruns.
