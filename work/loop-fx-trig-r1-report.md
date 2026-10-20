STATUS: COMPLETE

Native job fx-trig is local on `codex/fx-trig` in `worktrees/fx-trig`, branched from `codex/review-fixes` at `28e6cf0`. Not pushed.

The emitter cosine and sine are `math_cos16` / `math_sin16`: the degree-7 half-angle polynomial, then `cos 2x = 1 - 2 sin² x`, with the default ps2 adder. Each multiply, add and subtract truncates toward zero. There is no coefficient table and no host libm, so arm64 and x86_64 produce the same bits. `sin16(0)` is +2⁻²². The float32 result is promoted to binary64 before the rest of the emitter, which is how `ref_particles` continues after `_cs`.

## Evidence

FX research `t3/research-fx` at `fe137cc` (`docs/spec/trig16.md`, `tools/ref_trig.py`, `tools/difftest_trig.py`, `work/cursor-report.md`). That difftest is the research's result: all 65,536 angles under each of the four adders, plus the 64 haze pairs, 0 failures. This job did not re-run the interpreter. `ref_core.py` at the vector pin `19f80a6` is the same `cos16` / `sin16` body, so the generator stayed on that pin and patched `_cs` with `ref_core.cos16` / `sin16`.

The port's integer ps2 adder matches `core::cos16` / `sin16` on every angle. Trails, glows and the combat cosine already call that polynomial. Beam builders in `0x192a40`–`0x193fff` do not call either routine. The haze circle is computed by `haze_circle` (64 angles, truncation collapses them to 28 pairs). It is not stored in the repository.

## Refusals kept

- The angle passed into `Player_blockSparks`. The silent-0 body remains the earlier executed defence model (facing × 10). This job does not treat that multiply as a newly recovered angle. `trig::refuse_block_sparks_angle` throws.
- `Creature_processDamage`'s four formulas. `trig::refuse_creature_process_damage_angles` throws. The existing `process_damage` body does not invent them.
- Whether `particle_drawHaze` runs only for kind 3, and which screen axis receives the sine word. `draw_haze` refuses both. `particle_emitLists` still draws kinds 0..2.

The console's last bit of an add is still the research's open question. The port uses the ps2 model, which is the reference default. The other three adder models disagree with it.

## Files

- `ee6521a` — `fx_trig.hpp`, emitter / haze / refusal call sites, unit expectations, regenerated `fx_exact` vectors, generator. Removed `tools/binary64_trig.py`.
- `49d2492` — `docs/NATIVE_FX.md`.

Of 14,690 vector rows, 900 emitter rows and all 16 trig rows changed. Every other prefix is unchanged.

## Commands

Arm64 Release:

```
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
```

Build exit 0 (361 s). `ctest --test-dir build/native -j 3 --output-on-failure`: 184/184 passed, 696.61 s. No timeout, so nothing was rerun alone.

x86_64 Release, cross-compiled, tests under Rosetta (not native Intel hardware):

```
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 -j 4
```

Build exit 0 (202 s). `file` on `champions-fx-exact-tests` is Mach-O x86_64. `ctest --test-dir build/native-x64 -j 3 --output-on-failure`: 184/184 passed, 1110.18 s, including `fx-exact` (14,690 rows, 0 mismatches). No timeout.

Godot, after `cmake -S gdextension -B build/godot-extension` with the integration `godot-cpp` pin `e83fd090`, Release, `GODOTCPP_TARGET=template_debug`, then `cmake --build build/godot-extension -j 4` (exit 0). The archive extension landed in `godot/bin/`. Godot aborts if the level-texture library is absent; a gitignored link to the parent worktree's existing build supplied that file. It is not part of this change.

- `native_fx_smoke.gd`: exit 0, "native FX Godot smoke passed".
- `fx_bridge_refusal_smoke.gd`: 33 checks, 0 failures, exit 0.
- `native_fx_visual.gd`: headless uses the dummy renderer, so the viewport image is null. The same script with GL Compatibility wrote `work/native-fx-2-visual.png` (exit 0, 4 runs, no refusals).
- `playable_wave_smoke_full_combat.gd` headless: exit 0, 108,694 checks, 0 failures. Full trace SHA-256 `8dc349c19f9f14a9d759aeed60aa31a3e962dc4dab418a13a4fcec7f7de8aa0c`. Both drawing runs presented 300 frames and their bridge states matched.

## Private disc

The read-only disc has 994 files. This job counted them and did not copy names, bytes, or decoded content.

## Unverified

Native Intel, Windows and Linux hardware. The interpreter difftest was not repeated here. The three refusals above are intentionally not implemented. The synthetic visual capture is not a comparison against the original game.
