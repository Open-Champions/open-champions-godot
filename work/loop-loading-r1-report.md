STATUS: PARTIAL — the hub to mission loading screen, art fallback, frame sequence, and native entry-music decision run; exact GPU pixels, audible handoff, concurrent loading animation, and fully integrated player-control parity remain unverified.

2026-10-24–25 | `codex/play-loading` from `7ed3f21` | local only; no push

## Implementation and evidence

- `godot/real_data/loading_display.gd` uses the `t3/research-loading` `3ba61c7` alias/truncation/fallback and decodes `shot.tex` plus `LOADMISC.LMP` art from the chosen disc folder at run time. It renders the measured art/title and moving horizontal strips in the 640x448 design space. The start has three dark frames; the scene gates input, keeps the display across the synchronous loader and ten warm-up presentations, blanks six fields, then starts the 32-present colour ramp. Rendered timing advances on `RenderingServer.frame_post_draw`; headless timing advances on process frames. No asset bytes are tracked.
- `native/src/audio/native_audio.cpp` chooses the first case-sensitive `enttag` name in level-directory order, or queues a stream-2 stop when absent; the method preserves the stream on recall. `godot/real_data/real_audio.gd` invokes that policy through the native mixer for normal entry and services sound twice after the new level bank is installed.
- Existing level loading still binds world/terrain, textures, lights, sound bank, objects, script and spawn before `start_play`; the loading branch now withholds automatic simulation and input until its warm-up and blank-field boundary.
- Evidence: `~/Projects/champions-re-wt/research-loading/docs/loading-transitions.md`, audited research head `3ba61c7`. The loading art dimensions, alias and fade cases were executed or measured in the bounded research. Final GPU output and audible delivery were not established there.

## Verification

- Release native build and Godot extension build: passed on macOS arm64.
- Focused CTest `^(native-audio|playable-audio)$`: 2/2 passed. Synthetic Godot loading cases: 28/28 passed, including neutral colour after 32 completed presents.
- Read-only disc art selection: 111/111 selected textures decoded at documented dimensions; 12 used fallback; zero refusals.
- Bounded hub to mission to destination route: headless and rendered exit 0 with campaign image and player record carried; the route observed 11 loading animation frames before fade. Rendered loading, fade-start and arrival captures are under ignored `work/play-loading/route-rendered-r5/` and were inspected.
- Full arm64 CTest: 229/231 passed in the `-j 3` run. `player-exact` hit its fixed 240-second CTest limit; `session-facing-exact` hit its internal 600-second subprocess limit. The serial `player-exact` CTest rerun hit the same limit, then its exact command passed directly with 274,304 cases. The serial `session-facing-exact` rerun hit the same internal limit; an ignored local copy changed only its subprocess timeout to 1,800 seconds, then the unchanged comparison passed 528 exact cases (SHA-256 `7d8f0b9ad4e938d145ac212b1dadbc0595189dad454a502205319b284827f1ab`).
- Godot development matrix: 57/57 commands passed, including loading contract, disc acceptance, and route.
- Scripts-on sweep: 111/111 candidates loaded and played, 224 checks passed, zero failures, 111 scripts attached, zero script faults. The owner disc remained read-only at 994 files.
- Headless full-combat smoke: 2,620 checks, zero failures. `FULL_TRACE` 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from the `codex/play-route` baseline.

## Limits

- `scrollbar.tex` rotated ornament positions are only observed in the research instructions and lack an independently modeled geometry contract; this client draws the measured horizontal strips and does not invent the eight rotated positions.
- The host loader remains synchronous, so no renderer thread animates during its blocking work. Warm-up presently counts ten display presents; terrain-stream busy, equipment, peer barrier and store-stock waits are not fully integrated. The host request API does not expose the recall leg, though the mixer method preserves it when called. The 0.05-second direct-effect mute and audible stream promotion were not verified.
- The rendered art is a decoder and host-renderer result. It is not a captured original GPU frame; texture pixels, PS2 blend interpretation, and integrated first-control timing remain unverified.

## Commands

Run from this worktree; `<Godot>` is the pinned Godot 4.5.2 binary in the integration worktree, `<owner-disc>` is the read-only owner-selected disc folder, and `<work>` is this worktree's ignored `work/play-loading` directory. `<python3.14>` is the Python interpreter configured by CMake. The extended comparator copy under ignored `work/` differs only in `timeout=1800` versus `timeout=600`.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<pinned-godot-cpp> -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 2
ctest --test-dir build/native --output-on-failure -j 1 -R '^(native-audio|playable-audio)$'
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'
<python3.14> tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz
ctest --test-dir build/native --output-on-failure -j 1 -R '^session-facing-exact$'
<python3.14> work/test_session_facing_vectors_long.py --exe build/native/champions-session-facing-trace --vectors native/tests/vectors/session_facing.tsv.gz
<Godot> --headless --path godot --script res://tests/loading_display_cases.gd
<Godot> --headless --path godot --script res://tests/loading_disc_cases.gd -- --game-folder=<owner-disc>
<Godot> --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<work>/route-headless-r4 --route-only
<Godot> --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<work>/route-rendered-r5 --route-only --rendered
<Godot> --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<work>/sweep-cache --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<work>/sweep-summary.json
<Godot> --headless --path godot --script res://tests/playable_wave_smoke_full_combat.gd -- --ready-ticks=640
python3 work/play-loading/run-godot-matrix.py
```
