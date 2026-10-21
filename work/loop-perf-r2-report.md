STATUS: COMPLETE. All ten single-level memory peaks are below 1 GB; native, Godot, sweep and trace gates pass.

# perf-2, round 2: terrain ownership and paced cell streaming (2026-10-21)

Branch `codex/perf-2` from wave 18 `0f3cafc` (the wave-19 report was absent).
Local commits: `a4d044c` (one streamed session terrain and retained global VAT),
`5ed72bb` (worker decode and staged lighting), `8e08514` (moving-piece lighting
after pose), and `2916653` (headless count parity). No push or PR. The private
disc was read-only. Only counts and timings are recorded here.

## Changes

- The game-folder stream now prepares and commits the collision session once.
  It validates the same geometry and preserves surface and triangle summary
  counts, but never builds the neutral terrain mesh that the textured display
  immediately replaced. The full neutral path remains for other callers.
- The one process-lifetime mixer keeps its global VAT after `end_session()`.
  The game-folder bridge installs the same read-only bank once, avoiding the
  roughly 108 MB copy on each reset. The mixer and its lifetime state are not
  recreated per level.
- During play, one nearest missing cell is decoded at a time on a worker.
  Mesh creation and FX lighting are paced on the main thread, with groups kept
  hidden until the whole cell is ready. A cell no longer in the focus window is
  discarded. The first window remains fully built before play. Headless runs
  keep synchronous one-cell-per-frame publication so lighting counts match.
- The level-library history and load signal still fire once for the game-folder
  path. A synthetic smoke covers worker order, atomic visibility and focus
  cancellation.

## Before and after on the same ten heaviest levels

The round-1 probe was copied to ignored `work/perf/probe.gd`. Each run used one
process per level, `real_data_play.tscn`, 1280×720 GL Compatibility on Apple M5
Pro, vsync off, 240 standing frames then 240 walking frames, and the same
level-lights JSON. `/usr/bin/time -l` supplied process peak footprint. Base
and after were both measured from this wave-18 source on this machine. Units:
load and play start in seconds; frame samples in milliseconds; peak footprint
in decimal GB. The rank is by real-level triangle count, not a private name.

| Rank | Triangles | Load s before → after | Play start s before → after | Stand median / p95 ms before → after | Walk median / p95 / max ms before → after | Peak GB before → after |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1,826,822 | 0.60 → 0.27 | 0.55 → 0.49 | 3.3 / 7.7 → 2.3 / 7.3 | 7.3 / 11.2 / 13.1 → 5.1 / 9.9 / 12.8 | 1.279 → 0.737 |
| 2 | 1,348,348 | 0.38 → 0.18 | 0.36 → 0.32 | 1.3 / 5.8 → 1.1 / 5.4 | 4.2 / 8.4 / 8.6 → 3.9 / 7.9 / 12.4 | 1.139 → 0.672 |
| 3 | 1,311,825 | 0.61 → 0.46 | 0.58 → 0.60 | 2.5 / 8.2 → 2.2 / 7.9 | 5.9 / 11.2 / 13.9 → 5.5 / 10.9 / 27.4 | 1.237 → 0.825 |
| 4 | 1,098,342 | 0.42 → 0.24 | 0.47 → 0.45 | 2.9 / 7.7 → 2.7 / 7.6 | 6.3 / 11.0 / 13.4 → 6.0 / 10.7 / 12.8 | 1.082 → 0.698 |
| 5 | 1,045,486 | 0.68 → 0.55 | 0.53 → 0.49 | 12.0 / 12.4 → 4.2 / 11.0 | 15.0 / 15.6 / 93.6 → 13.3 / 16.9 / 35.9 | 1.324 → 0.903 |
| 6 | 976,898 | 0.42 → 0.25 | 0.39 → 0.38 | 1.1 / 5.8 → 0.9 / 5.8 | 4.2 / 8.9 / 13.9 → 4.0 / 8.7 / 13.9 | 1.122 → 0.685 |
| 7 | 945,632 | 0.38 → 0.22 | 0.38 → 0.34 | 2.9 / 7.7 → 1.8 / 6.3 | 5.5 / 10.2 / 13.0 → 4.8 / 8.9 / 11.0 | 1.112 → 0.726 |
| 8 | 917,101 | 0.33 → 0.20 | 0.30 → 0.27 | 1.2 / 5.9 → 1.2 / 6.5 | 4.2 / 8.6 / 8.8 → 4.4 / 9.2 / 9.7 | 1.001 → 0.694 |
| 9 | 908,032 | 0.34 → 0.20 | 0.28 → 0.33 | 1.9 / 6.8 → 1.4 / 6.3 | 5.0 / 9.8 / 38.1 → 5.1 / 10.1 / 31.4 | 1.113 → 0.652 |
| 10 | 904,676 | 0.25 → 0.15 | 0.31 → 0.30 | 0.9 / 5.3 → 0.9 / 5.5 | 3.5 / 7.8 / 12.5 → 3.9 / 8.5 / 12.4 | 0.937 → 0.608 |

Ten-level sequence in one process: peak footprint was 1.073 GB after the
change. It remains above 1 GB in this separate workload. Scene replacement
overlap and allocator page retention are plausible contributors but were not
isolated. The requested one-process-per-level peaks are all below 1 GB.

## Water-mesh check

On rank 5, hiding the single water mesh cut submitted primitives from
1,413,284 to 23,684, but improved standing median only 11.905 → 11.448 ms
and walking median 14.500 → 14.136 ms in paired diagnostic runs. The water
mesh is not the main cause of this level's frame time, so its rendering and
visual output were left intact.

With streaming disabled during the walk as a diagnostic, rank 5 still reached
a 33.7 ms maximum. The final streamed walk reached 35.9 ms. The remaining long
frame is largely present without cell streaming; the measurements do not
establish it as a texture-cell decode stall.

## Verification

- Final single-level probes: all ten peaks 0.608–0.903 GB. The standing
  primitive maxima match baseline on every level. The rendered streaming path
  keeps nearest-missing selection and publishes no more than one cell per frame.
- `level_library_smoke`: 164 checks, zero failures.
- `level_texture_stream_smoke`: 31 checks, zero failures after the worker and
  focus-cancellation checks were added.
- `native_audio_smoke`: 20 checks, zero failures; `level_textured_session_smoke`:
  10 checks passed.
- Native arm64 CTest: 211/211 passed.
- Godot dev matrix: 48/48 passed after building the optional creature-animation extension.
- Final 111-level sweeps, scripts off and on: 111 tried/loaded/played in each,
  119 checks each, zero failures and zero refusals. All stable wave-18 counts
  and refusal maps match exactly across four slices. Elapsed time, memory
  counters and audio frame delivery varied with scheduling.
- Rendered 36,000-tick run: 108,826 checks, zero failures, 62 PNG captures,
  `FULL_TRACE` SHA-256
  `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`
  (21,990 bytes), identical to wave 18. Against the wave-18 1280×720 captures,
  59 PNG files are byte identical; the other three display generated temporary
  paths or the worktree directory. The rendered texture and continuation
  captures were inspected.

The probe and private sweep logs remain gitignored under `work/`. No private
bytes, names or decoded content are committed. Pixel captures of private
levels were not recorded under the counts-only disc rule; visual equivalence
is bounded by the unchanged draw counts, steady standing primitive counts,
synthetic visual tests and trace gates.
