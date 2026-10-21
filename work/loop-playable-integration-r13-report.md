STATUS: COMPLETE

Wave 13 of job playable-integration is done. `codex/fx-trig` is merged, and
every exact-vector test passes on arm64 and on x86_64 under Rosetta. The real-data
sweep stays at 108/111, and the Godot dev and export matrices pass.
Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch
`codex/playable-integration`, local commits only (no push, PR or issue).
`~/champions-re` was not touched. The disc copy was only read, and its counts stay
in the gitignored `work/private/`.

## Merge (start `eb583b8`)

| Branch | Tip | Merge commit |
|---|---|---|
| fx-trig | f9900c7 | 917a26f (no conflicts) |

fx-trig was branched from review-fixes `28e6cf0`, which is already in the branch.
After the merge, its tip is an ancestor of HEAD.

Callers: the emitter's `fx::exact::angle_cos_sin` is the only place that used the
correctly rounded stopgap, and it now calls `trig::cos_sin` (math_cos16 /
math_sin16, ps2 adder). Nothing in `native/` or `gdextension/` calls host
`cos` / `sin`. Glow, trail, decal, recipe (`offset_by_yaw`) and combat
(`block_sparks`) already used `core::cos16` / `sin16`. fx-trig added three
refusals: `draw_haze`, the block-sparks angle and the `process_damage` angles.
The two combat files only gained comments.

## (1) Exact vectors and trace hashes

- Only `native/tests/vectors/fx_exact.{tsv,json}` changed: 900 `emitter` rows and
  16 `trig` rows. No combat, session or other vector file changed.
- Research match:
  - Regenerating from pin `19f80a6` with
    `python3 -I tools/generate_fx_exact_vectors.py --out /tmp/w13-vec` gives a TSV
    byte-identical to the committed one, with `vectors_sha256` `67290583…30bb` and
    the same generator hash.
  - The pin's `ref_core.cos16` / `sin16` equal
    `~/Projects/champions-re-wt/research-fx` (`t3/research-fx` at `fe137cc`)
    `tools/ref_trig.py` on all 65,536 angles, with 0 mismatches.
  - `sin16(0)` is `0x34800000`, as `docs/spec/trig16.md` says.
- `fx-exact` passes on both architectures: 14,690 rows, 0 mismatches (trig 16/16).
  The x86_64 binary is Mach-O x86_64.
- No combat or effects trace hash changed:
  - Full `FULL_TRACE` is `8dc349c1…aa0c` in the dev run and all four exported runs.
  - `FULL_HOLY_STRIKE` and `FULL_SAVE_LOAD`, including the bridge presentation
    strings, match wave 12 line for line, in the dev run and in the 36,000-tick run.
  - The trace covers seeds and health. The emitter angle changes only emission
    positions and velocities, so there was nothing to explain.

## CTest

- arm64 `ctest --test-dir build/native -j 3 --output-on-failure`: **196/196**,
  643 s (`player-exact` 115 s).
- x86_64 Rosetta, `cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_OSX_ARCHITECTURES=x86_64`, then build and `ctest --test-dir
  build/native-x64 -j 3`: **196/196**, 1,002 s (`player-exact` 145 s).
- No test timed out, so nothing was rerun.

## (2) Real data, 111-level sweep

`work/run-slices-r13.sh` is the wave-12 slice script with r13 output names. It
ran four headless slices, 60 ticks per level, with the light and recipe JSON.
All slices exited 0 with 0 failed checks.

| | wave 12 | wave 13 |
|---|---|---|
| Loaded and played | 108 / 111 | **108 / 111** |
| Refused | 3, no ground at the story start | the same 3 |
| HUD / FNT / lights | 34/34, 8/8, 90 named + 18 default | the same |
| Meshes / textured | 173,142 / 173,097 | the same |
| Ready ticks / start markers | 6,372 / 95 | the same |

Every count is equal except four:
- `time.load_ms`, `time.play_ms`: wall clock.
- `audio.frames_pushed`: pumped once per process frame, so it follows the wall
  clock (2,553,876 → 2,299,636).
- `memory.static_peak_bytes`: +5,824 B out of 7.57 GB.

The exported app ran **12/12** levels with 0 failures and the same level counts
as wave 12. Its summary also has creature-preview counts that the wave-12
exported summary did not record.

## (3) Godot

- `python3 work/playable-wave13/run-builds.py`: all four universal extension
  builds pass (debug and release, main and texture). The native build was
  done separately.
- Dev matrix `run-godot.py`: **43/43**.
  - Full combat: 2,616 checks.
  - Combined FX: 1,876.
  - Wave-12 extensions: 40.
  - FX bridge refusals: 33.
  - creature-anim: 32 / 25 / 231.
  - `native_fx_visual` (GL): 4 runs, no refusals.
- Export:
  `Godot --headless --path . --export-release 'macOS universal' ../build/exports/wave13-final/OpenChampions.zip`
  (run from `godot/`), then `ditto -xk`.
- `run-export.py`: **16/16** on the first run.
  - The 36,000-tick 1280×720 Full run: 108,822 checks, 0 failures, 127 s. The
    flaky stale-clip check did not fire.
  - 960×720 and 800×600: 2,742 each. Exported headless: 2,616.
- App: `build/exports/wave13-final/Open Champions.app`.
  - ZIP SHA-256 `9574f43d…710c`.
  - Main dylib `12aceca2…f05e`. Texture dylib `858bf674…7c51`, unchanged from
    wave 12.
  - Both are packaged byte-identical, x86_64 + arm64.

## Commits

- `917a26f`: merge codex/fx-trig.
- `b841b8d`: docs/INTEGRATION.md wave-13 section.
- This report.

## Open / unverified

- The console's last bit of an add is still the research's open question. The
  port follows the ps2 model, the reference default.
- The fx-trig refusals stay refused: haze drawing, the block-sparks angle and
  `Creature_processDamage`'s four angle formulas.
- The research interpreter difftest was not repeated. The cross-check here is
  the Python reference on every angle.
- The three levels with no ground at the story start are still refused.
- Interaction was scripted, not manual play. No audio was heard. No native
  Intel, Windows or Linux hardware was used (x86_64 is Rosetta only).
