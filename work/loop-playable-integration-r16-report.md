STATUS: COMPLETE

Wave 16 of job playable-integration is done. `codex/water-draw` is merged.
Water tiles now draw the documented layer-3 surface: the set colour, FIX blend,
slope UVs and vertices, with the wave stepped on the simulation tick.

The real-data sweep stays **111/111** with scripts on and with scripts off.
CTest is 200/200 on arm64 and on x86_64 under Rosetta, both on the first
run. The Godot dev matrix is 45/45. The export matrix is 18/18 after one
rerun of the 36,000-tick run.

Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch
`codex/playable-integration`. Local commits only: no push, PR or issue.
`~/champions-re` was not touched. The disc copy was only read. Its counts,
logs, summaries and captures stay in the gitignored `work/private/`.

## Merge (start `685c6e6`)

| Branch | Range | Merge commit |
|---|---|---|
| water-draw | 037e26d..be0f580 (3 commits) | 679b7d7 |

There were two conflicts, and both were resolved by keeping both sides:

- `docs/REAL_DATA.md`: the wave-14/15 sections, then the water-draw section.
- `godot/tests/real_data_scene.gd`: the `--scripts` and `--water` usage lines.

No integration code fix was needed. `water-draw`, `level-draw` and
`real-world-adapter` passed right after the merge build, before the merge was
committed. The main extension dylib is byte-identical to wave 15. Only the
level-texture extension changed, because it now compiles `water_draw.cpp`.

## (1) Real data, 111-level sweep

`work/run-slices-r16.sh` ran four headless slices, 60 ready ticks per level,
with the light and recipe JSON. The load average was about 9 during the sweep.

| Run | Time | Tried / loaded / played | Refused | Failed checks |
|---|---|---|---|---|
| `r16 --scripts` | 3 min 39 s | 111 / 111 / 111 | 0 | 0 |
| `r16n` (no scripts) | 3 min 44 s | 111 / 111 / 111 | 0 | 0 |

Compared with wave 15 (scripts on) and wave 14 (scripts off):

- New counts:
  - `water.texture_ok` is 48 of 48 levels with water. 0 were refused.
  - Every decoded texture is 64×64. The aggregate shows 256 only because it
    sums the per-slice peaks.
- `levels.surfaces` is 174,457, one more than before. Meshes are unchanged at
  174,456.
  - The water mesh now has one surface per documented set.
  - One level, index 20, uses both sets. I found it with one headless run per
    level over indices 0–27.
- Every other deterministic count is equal. That includes the story ground
  (111 / 98), water (48 levels, 9,451 tiles, 4 entries, 252 of 3,024 ticks
  in water), meshes, textured meshes, 6,549 ready ticks and 119 checks.
- With scripts on, every script count equals wave 15: 111 attached,
  76 started, 41 waits, 37 completions, 0 faults, 17,398 objects, and the
  per-command levels and calls.
- Only `time.*`, `audio.frames_pushed` and the peak static memory differ. The
  memory rose by 26 MB, summed over the four slices.

## (2) CTest

| Arch | Result | Time | Notes |
|---|---|---|---|
| arm64 | **200/200**, first run | 772 s | `player-exact` 130 s, `skills-a-exact` 31 s |
| x86_64 (Rosetta) | **200/200**, first run | 1,246 s | `player-exact` 187 s, `monsters-1-exact` 82 s, `skills-a-exact` 50 s |

- The x86_64 build is `build/native-x64`, made with
  `-DCMAKE_OSX_ARCHITECTURES=x86_64`. The binaries are Mach-O x86_64.
- The new test is `water-draw`.
- The load average stayed between 6 and 17 for the whole wave. It never
  neared 50, so no wait was needed and no timeout occurred.
- The arm64 build had 0 warnings.

## (3) Godot

- `python3 work/playable-wave16/run-builds.py`: all four universal extension
  builds pass.
- The dev matrix `run-godot.py` is **45/45**, the same set as wave 15.
  - Full combat 2,616 checks, combined FX 1,876, wave-12 extensions 40, FX
    bridge refusals 33.
  - creature-anim 32 / 25 / 231. `native_fx_visual` ran 4 times with no
    refusals.
  - `script_commands_smoke` passed. `FULL_TRACE` is `8dc349c1…aa0c`.
- The export ran from `godot/` with `--export-release 'macOS universal'` to
  `build/exports/wave16-final/OpenChampions.zip`, then `ditto -xk`.
- `run-export.py` passed 17 of 18 in the matrix run.
  - The 36,000-tick 1280×720 Full run passed 108,822 checks with **1 failure**:
    "stale clip clears cosmetic owner and restores static mesh".
  - That check is the animation-picker refusal step, which rewrites the LMP
    and restarts the clip. water-draw changes nothing on that path. Its
    `FULL_TRACE` still matched.
  - Rerun alone, it passed **108,822 checks with 0 failures** in 152 s, with
    63 captures, so the export matrix is 18/18.
  - I found no timestamp cache in the scripts. The cause is not proven. The
    first run's log and captures are kept as `exported-full-16x9-first.log`
    and `captures-16x9-first/`.
  - Exported headless 2,616. 960×720 and 800×600 2,742 each, with 63
    captures each. Every `FULL_TRACE` is `8dc349c1…aa0c`.
- The exported app on real data (headless, gitignored summaries):
  - The 12-level set, with and without `--scripts`, equals wave 15 apart from
    timing and the new water-texture counts (5 decoded).
  - The three arena indices with `--scripts`: 3 / 3, equal to wave 15 apart
    from timing and the water-texture counts (3 decoded).
- App: `build/exports/wave16-final/Open Champions.app`.
  - ZIP SHA-256 `317bdd38…71b6`.
  - Main dylib `28342066…e95a`, unchanged from wave 15. Texture dylib
    `7ef534a2…927c`.
  - Both are packaged byte-identical, x86_64 + arm64.

## (4) Water captures

The captures were taken in a window at 1280×720 and are in the gitignored
`work/private/captures-r16/`. There is a play, creature and overview view for
each level, 12 PNGs in all, with 0 failed checks. I looked at all of them.
water-draw's own captures covered the first 3 water levels from index 0, so
this set adds index 20 and the first 3 water levels from index 28.

| Index | Tiles | Seen |
|---|---|---|
| 20 | 48, both sets | Bright orange sheets over a round pit and a side patch. Texture detail is visible up close. Overlapping tiles are brighter, because each tile blends on its own. |
| 28 | 37 | Small pale grey-green patches in the overview. None is in the play view. |
| 29 | 11 | One small grey-green patch in the overview. Not in the play view. |
| 30 | 772 | Many orange square sheets. Up close they are flat with faint texture and hard edges. One sheet draws over nearer rock, which is the documented no-depth-test behaviour. |

No ripples on any level, as expected while the claim seed is refused. The
windowed run from index 28 went on to play all 35 remaining water levels with
0 failures, and all 35 water textures decoded.

## Commits

- `679b7d7`: merge codex/water-draw.
- `1c02a95`: docs/REAL_DATA.md wave-16 sweep.
- `f647168`: clarify the memory delta.
- `0b9f283`: docs/REAL_DATA.md captures and exported runs.
- `4038ce6`: docs/INTEGRATION.md wave-16 section.
- This report.

## Open / unverified

- Ripples are still open. The claim-frame weights, the drawer's splash point
  and the simulation splash stay refused, so every drawn block is a flat zero
  grid. A research job is working on them.
- Overlapping tiles brighten where they overlap. I did not check whether the
  original shows the same overlap, though its documented blend implies it
  would.
- On index 20, I could not tell set 0 tiles from set 1 tiles by eye. The
  two-set finding comes from the surface count.
- The single 36,000-tick failure was not reproduced, and its cause is unknown.
- Level-option bit 8 (`filled_mask()`) is not turned on. The ±1000 screen clip
  is tested only natively.
- Interaction was scripted, not manual play. No audio was heard. No native
  Intel, Windows or Linux hardware was used (x86_64 is Rosetta only).
