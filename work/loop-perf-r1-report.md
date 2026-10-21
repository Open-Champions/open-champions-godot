STATUS: PARTIAL. 60 fps at 1280×720 is met on all 10 heaviest levels. Peak footprint is under 1 GB on 5 of 10 (0.92–1.21 GB, against 1.9–4.1 GB before). Every exact test and FULL_TRACE are unchanged.

# perf, round 1: real-level frame time, memory and load (2026-10-21)

Branch `codex/perf`. It started from `codex/playable-integration` `c034ce3`
and merged its wave 16 (`77d1260`) in `37bf27f`. Never pushed. Counts and
timings only. No names, bytes or decoded content leave the private disc copy.

## Takeover review (previous Cursor agent: 1 commit, 8 uncommitted files)

- Commit `6533150` (nine-cell bind) was kept and corrected in `755bf2b`:
  - `texture_cell_size` (+0x68, 0 → 960) and `cell_id_of` (`row × 100 + column`,
    `floor + 50`) match research `docs/level-flow.md` 13.1 and
    `docs/formats/world.md`. `cell_id_of` now divides in float, as `floorf` does.
  - **Invented:** the window order "columns focus+1, focus, focus−1", which
    the test called "documented". The research gives only "3 × 3 cells in 9
    slots, nearest missing cell first". The order is now plain row-major and
    says so.
- The uncommitted work was saved to gitignored `work/prev-agent/`, then
  rewritten rather than committed:
  - Its atlas merged blended surfaces across draw order, which changes
    transparent compositing.
  - Cross-cell draw order used per-cell indices.
  - "Large level" thresholds (`cells > 9 or triangles > 50000`) were invented.
  - It skipped the library's second session commit, which risked the
    simulation.
  - A debug `print` was added to `playable_wave_smoke.gd`, another job's test.
- Kept from it: the moving-piece test uses chunk flag 0x20, which is
  documented in `world.md` ("D: exactly the pieces' chunks").

## What changed

1. **Cell streaming** (`cd860e5`). New files: `level_texture_stream.cpp`,
   `scenes/level_texture_stream.gd`. Opt-in via `stream_cells`, set by the
   game-folder scene only.
   - Keeps the 3 × 3 cells around focus = camera position + forward × 600
     (`camera_updateFocus` 0x18a520), in nine slots.
   - Reads the nearest missing cell first, one per frame. A non-resident cell
     is not drawn.
   - Two client choices are labelled as such: "nearest" is the squared
     row/column distance, and a full set of slots reuses the farthest cell.
2. **Batching.**
   - Per cell, order-independent batches (opaque, darken, cutout; no
     environment pass; not a moving piece; UVs in [0, 1]) share atlas pages
     with a one-texel clamped border. This gives the same texels as
     clamp-to-edge bilinear.
   - Blended, environment and moving-piece batches keep the full loader's
     runs. They are ordered across the resident window by
     `level_texture::draw_slots`, which works from geometry alone.
   - Checked on the largest disc level: `draw_slots` equals `bind_world` for
     the whole level, and `bind_cells` for all 148 windows (0 mismatches).
3. **FX relight** (`cd860e5`, `27f01dd`).
   - A moving model used to get a new unshaded material copy on every frame,
     which recompiled its shader: about 50 ms per walking frame. Copies are
     now reused while their source is unchanged.
   - World chunks are shaded in one call. `shade_world_many` gives the same
     per-vertex colours as `shade_world`, checked exactly on 2,000 random
     vertices.
   - The transform and normal matrix are computed once per surface.
4. **Lighting hold during the swap** (`27f01dd`). In play mode, every
   game-folder load after the first lit the neutral 1.8 M-triangle session
   display vertex by vertex, and then freed it. That was the sweep slowdown:
   12–22 s per level. Lighting is now held during the swap. The textured
   display is lit on the next frame, with the new level's light record. Before
   this change it got the previous level's record.
5. **Audio memory** (`084e078`). The game-folder mixer is freed before the next
   one copies the roughly 100 MB global bank.
6. **Docs** (`0741651`): `docs/LEVEL_TEXTURES.md`, new section "Streamed
   display and frame cost".

## Before → after, the 10 heaviest levels

Setup:
- `real_data_play.tscn` at 1280×720, GL Compatibility on an Apple M5 Pro,
  vsync off, level-lights JSON.
- One process per level. Base and after runs were interleaved per level so
  both saw the same machine load.
- Idle is 240 play frames; walk is 240 frames holding forward.
- Footprint is `/usr/bin/time -l` "peak memory footprint" of the whole process.
- Probe: gitignored `work/perf/probe.gd`. Data: `work/perf/final-*`.

| Level (rank) | Triangles | Load s | Play start s | Idle median / p95 ms | Walk median / p95 / max ms | Draw calls | Terrain nodes | Peak footprint GB |
|---|---|---|---|---|---|---|---|---|
| 1 | 1,826,822 | 2.07 → 0.40 | 7.61 → 0.35 | 27.4 / 28.4 → 1.9 / 5.7 | 82.1 / 84.7 / 87 → 4.2 / 7.9 / 9 | 59 → 37 | 5,068 → 274 | 4.09 → 1.17 |
| 2 | 1,348,348 | 1.28 → 0.27 | 4.60 → 0.26 | 11.3 / 12.0 → 1.0 / 4.3 | 64.4 / 66.9 / 76 → 3.0 / 6.0 / 7 | 102 → 29 | 2,544 → 12 | 2.26 → 1.03 |
| 3 | 1,311,825 | 1.24 → 0.42 | 4.55 → 0.39 | 14.3 / 14.9 → 1.9 / 5.7 | 67.7 / 71.7 / 78 → 3.9 / 7.2 / 8 | 75 → 39 | 2,672 → 126 | 1.99 → 1.13 |
| 4 | 1,098,342 | 1.09 → 0.29 | 4.07 → 0.32 | 17.5 / 19.7 → 1.8 / 5.3 | 72.0 / 74.8 / 83 → 4.0 / 7.3 / 8 | 63 → 46 | 2,960 → 261 | 2.55 → 0.97 |
| 5 | 1,045,486 | 1.43 → 0.48 | 4.39 → 0.35 | 41.7 / 43.9 → 5.0 / 9.0 | 97.7 / 105.3 / 113 → 10.2 / 11.0 / 53 | 61 → 62 | 3,656 → 330 | 2.45 → 1.21 |
| 6 | 976,898 | 1.36 → 0.29 | 4.28 → 0.26 | 16.5 / 17.8 → 0.8 / 4.0 | 69.6 / 72.9 / 82 → 2.8 / 5.7 / 6 | 51 → 25 | 4,314 → 18 | 2.27 → 0.99 |
| 7 | 945,632 | 1.23 → 0.27 | 4.60 → 0.26 | 25.8 / 27.5 → 1.8 / 5.0 | 80.7 / 101.3 / 111 → 3.9 / 6.8 / 7 | 244 → 52 | 4,637 → 103 | 3.14 → 0.98 |
| 8 | 917,101 | 0.91 → 0.23 | 2.99 → 0.21 | 4.2 / 9.1 → 1.0 / 4.4 | 60.5 / 65.1 / 99 → 3.0 / 6.1 / 7 | 40 → 26 | 1,694 → 13 | 2.16 → 1.01 |
| 9 | 908,032 | 1.02 → 0.27 | 4.59 → 0.23 | 21.6 / 24.5 → 1.5 / 4.8 | 79.5 / 84.2 / 88 → 3.9 / 7.1 / 28 | 63 → 35 | 5,071 → 119 | 2.16 → 0.98 |
| 10 | 904,676 | 0.82 → 0.19 | 3.03 → 0.23 | 4.7 / 9.6 → 0.8 / 4.2 | 63.2 / 66.3 / 73 → 3.0 / 6.1 / 7 | 32 → 23 | 1,570 → 9 | 1.92 → 0.92 |

**Long sweep (the same 10 levels in one process):**
- Load per level after the first: 9.9–26.8 s → 0.29–0.57 s.
- Godot static peak: 1.67 → 0.36 GB.
- Process peak footprint: 4.74 → 1.42 GB.
- Node and object counts per level equal the single-level runs, both before
  and after, so no nodes accumulate.
- RSS rises from 1.33 GB to about 1.65 GB over the first five levels, then
  stays flat.

**111-level headless sweep** (`work/perf/run-slices.sh`, no scripts),
compared with wave-16 `r16n`:
- Every deterministic count is equal, with 0 failures and the same refusals.
- Differences are display counts and timing only:
  - meshes 174,456 → 6,133;
  - textured meshes 174,408 → 6,085;
  - drawn claimed pieces 52 → 14 (only resident cells draw);
  - world lighting calls 4,774,659 → 100,557;
  - total load 613 s → 49 s;
  - static peak per slice 2.53 → 0.70 GB.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, then
  `cmake --build build/native -j 4`: pass. `champions-level-texture-tests`
  passes 1,081 checks.
- `ctest --test-dir build/native --output-on-failure -j 3`: **200/200** in
  691 s. No reruns were needed.
- Extensions rebuilt (arm64, `template_debug`, Release): archive,
  level-texture and creature-anim.
- Godot dev matrix (wave-16 list plus 2 new smokes): **47/47 exit 0**.
  - Every existing smoke has the same check count as wave 16.
  - New: `level_texture_stream_smoke` 27 checks, `fx_shade_batch_smoke` 12
    checks.
- FULL_TRACE `8dc349c1…aa0c` is unchanged:
  - headless 640 ticks;
  - rendered 1280×720, 36,000 ticks with captures: 108,822 checks,
    0 failures.
- The flaky stale-clip check did not fire.

## Not done / limits

- **Peak footprint is still 1.01–1.21 GB on 5 of 10 levels.** Steady state
  measured on rank 5 is about 0.93 GB. The rest is outside this job's files:
  - The load prepares the session terrain twice: library, then texture
    commit. The controller also keeps its own neutral display copy, about
    125 MB plus 66 MB of video memory. That is in `session_controller.cpp`
    and the library flow.
  - The audio bridge copies the 108 MB global VAT into native memory on every
    reset.
  - Skipping the second commit would save memory, but it was not done because
    it touches simulation flow.
- **Frame spikes.** Walk max is 53 ms on rank 5 and 28 ms on rank 9, when a
  new cell is read and lit during play. Medians and p95 are under 11.1 ms.
  Rank 5 is still the slowest (about 5 ms idle). It draws 1.41 M primitives,
  probably the single wave-16 water mesh, which spans the whole level and so
  is never culled. That mesh belongs to water-draw / integration.
- **Not documented, so marked as client choices:** the original slot reuse
  order and the "nearest" metric.
- Only the game-folder scene streams. The fixture paths and other callers
  keep the full bind.
- Not run:
  - exported app;
  - x86_64 / Rosetta native tests (no native simulation code changed);
  - Windows or Linux;
  - manual play.
