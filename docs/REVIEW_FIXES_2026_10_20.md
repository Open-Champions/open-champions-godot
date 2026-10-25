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
| 13 | x86_64 verification (68e7016) | `fx-exact` fails on x86_64: 623 of 2,000 emitter rows differ in spawn-velocity bits | **Fixed** | Reproduced with `build/native-x64/champions-fx-exact-tests native/tests/vectors`. Cause: host libm cos/sin, not FMA or excess precision (`-ffp-contract=off` and SSE2 were already in force). Over all 65,536 angles, arm64 and x86_64 libm disagree on 18,327, and neither is correctly rounded (arm64 is off on 5,332, x86_64 on 16,888; `work/x64trig`). The committed vectors encoded arm64 libm. The port now uses `fx_trig.hpp` (double-double, no libm, correctly rounded on all 65,536 angles on both architectures); the generator uses `tools/binary64_trig.py` (exact decimal), which changed 395 emitter rows; 16 new `trig` rows cover every angle. `fx-exact`: 14,690 rows, 0 mismatches on arm64 and x86_64. |
| 14 | x86_64 verification (68e7016) | `unittest discover` cannot import `test_combat_runtime_2_vectors.py` | **Fixed** | It failed with a ValueError at import (`sys.argv` unpack). The CLI now lives in `main()`, as in the round-1 checker, and discovery runs a provenance test. Discovery: 715 tests OK, 385 skipped; `combat-runtime-2-exact` still passes. |

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

## Verification (macOS arm64 host, 2026-10-20)

- **Builds.** `python3 work/matrix/run-builds.py` (from the wave-11 runner): 10/10
  pass. Native Release, plus the universal arm64 + x86_64 main and level-texture
  extensions (debug and release), all `-j 4`. The x86_64 slice first failed on the
  SDK's legacy `finite()`; fixed in 6eb9adb.
- **Native tests.** `ctest --test-dir build/native -j 3`: 183/184 in the aggregate
  run (1143.9 s). `amx-validation-transport` hit its internal 5 s subprocess timeout
  under load and passed alone (0.13 s), so 184/184 pass.
  `tools/check_simulation_boundary.py`: pass.
- **Godot dev matrix.** 36/36 commands exit 0 with no `ERROR` lines, including
  `fx_bridge_refusal_smoke` (33 checks). The Full chain passes 2,614 checks, and the
  combined smokes all report 0 failures.
- **Creature animation.** `run_creature_anim_smoke.py`: `--freed` 25 checks, smoke 32,
  `--dispatch` 222; all pass.
- **Export.** macOS universal (`--export-release 'macOS universal'`); export matrix
  14/14 exit 0:
  - 36,000-tick 1280×720 Full chain: 108,820 checks, 0 failures;
  - 960×720 and 800×600: 2,740 each;
  - exported headless: 2,614;
  - every Full trace SHA-256 is `8dc349c1…0c`, including the save/load-continued run.
- **Rendered present counts.** In the exported rendered app the uninterrupted drawn
  run made 299 presents and the continued run 300. Their per-machine bridge states
  then differ by that one frame, so the comparison is reported, not asserted. Dev
  headless and dev rendered runs made 300 / 300, and their bridge states match.

- **x86_64 (Rosetta), after findings 13 and 14.**
  - `cmake -S native -B build/native-x64 -DCMAKE_OSX_ARCHITECTURES=x86_64 Release`, then
    `ctest -j 3`: 183/184 (1607.8 s). `player-exact` timed out at 240 s and passed alone
    in 226.3 s, so 184/184 pass, including `fx-exact` (0 mismatches), `fixture-save`,
    `playable-combat-fx` and `combat-runtime-2-exact`.
  - `python3 -m unittest discover -s tools -p 'test_*.py'`: 715 tests OK (385 skipped).
  - After the change, on arm64: the affected native tests 10/10; universal builds 10/10;
    `native_fx_smoke`, `fx_bridge_refusal_smoke` (33), `native_fx_visual` and the Full
    chain (2,614 checks, trace `8dc349c1…`) pass.

Not run: Intel / Windows / Linux hardware (x86_64 ran only under Rosetta), the Godot
export matrix after findings 13 and 14 (it ran before them), and private-file
acceptance (none of these fixes needs private data).
