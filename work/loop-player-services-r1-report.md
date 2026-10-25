STATUS: FIXED — all three player-services audit findings addressed; scoped arm64 CTest 17/17, Godot dev matrix 54/54, read-only level sweep 111/111, and FULL_TRACE unchanged.

# Player services round 1 — 2026-10-23

Branch `codex/player-services`, based on `84c12fb`. No World ownership or creature code changed. No push.

## Implementation

- `player_services::Bundle` borrows the decoded 111-level `world_queries::Level`. Slide, height, height-below, point height and blocking-flag callbacks call the existing terrain port. Water test and first-chunk height call the existing `creature_water::Service`, now backed on a committed real level by `real_world::WaterImage` built from that level's WORLD bytes. Dry disturbance returns zero; wet disturbance requires owned runtime grids. Unported terrain raycast refuses only its own callback with a label.
- The client captures the actual `Camera3D` yaw, position and four side normals after its presentation camera update, including the additional real-player framing step. The bundle calculates the documented screen footprint and 60-unit inset at the queried player height. A remote slot needs its own camera snapshot.
- Animation start, timed advance and root displacement call `creature_anim::Runtime`. They require a bound actor state and resolved clip resource; absent inputs refuse by callback and never use the Full fixture's authored zero root or flat service. The clip resolver takes the selected model's resource pointer; `t3/research-player-model` `7f13899` requires slot order from that model's animation lump, including repeated names. The new live field reader exposes signed weapon animation class `R+0x4a86` and 32-bit action state `P+0x58c`, with slot and record-owner checks. It reads each time so equip and action transitions are visible. This follows audit-PASSED `t3/research-player-anim` `cda95dc` and the ported animation runtime. The shared World actor/record and clip-resource registration are owned by the parallel job, so ordinary play does not yet invoke animation callbacks.

## Verification

- Arm64 native build: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build/native -j 4` passed. Focused `ctest --test-dir build/native -R '^player-services$' --output-on-failure`: **1/1 passed**. Synthetic authored tests cover terrain and water at known points, water-tree first height/disturb, a known four-plane camera, animation start and two advance ticks/root, and live weapon class/action changes.
- Counts-only headless probe of one selected private level: **1 loaded, 16 terrain queries, 16 floors, 0 water chunks, 0 callback refusals**. No game bytes or private paths are emitted by the probe itself.
- GDExtension, level-texture and creature-animation dev builds passed. Godot 4.5.2 dev matrix via `python3 /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration/work/playable-wave25/run-godot.py`: **54/54 exit 0**. A final-source `scene_play_flow_smoke.gd` rerun passed **65 checks, 0 failures** after the real camera capture change.
- Scripted read-only 111-level sweep via `bash /Users/syndicatemike/Projects/open-champions/worktrees/play-enemies/work/play-enemies/run-sweep.sh`: four slices all exited 0; **111 tried / 111 loaded / 111 played, 230 checks passed, 0 failures, 0 refusal kinds**. Disc file count: **994 before and after**.
- Full fixture `FULL_TRACE`: **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged. It is synthetic fixture evidence, not ordinary campaign combat.

## Native CTest scope and remaining boundary

Final scoped arm64 CTest: `ctest --test-dir build/native -j 3 --output-on-failure -R '^(player-services|world-queries-(host|exact|transport)|terrain-file-(host|exact|transport)|session-terrain-host|creature-water-host|creature-anim-(api|exact)|animation-decoder-(api|exact|transport)|real-world-adapter|level-flow-water-adapter|combat-runtime-host)$'`: **17/17 passed**. A broader 237-test run was interrupted during unrelated `native-save-partition`; a broader scoped run passed 27/28 before its long `combat-runtime-2-exact` vector test was interrupted. Both were under simultaneous repository test load. Neither is claimed as a full-suite pass.

The bundle is ready for the shared World to bind its live player actor state, memory reader and clip-resource resolver. That is the remaining integration gate for real-level animation and weapon/action observation; this branch does not claim a real campaign fight.

## Audit fixes

- **PSR1-1 (HIGH):** A wet `water_disturb` now refuses with a label until the owner explicitly binds runtime ripple grids, and it checks that every queried chunk has a grid pointer before any ripple write. Dry queries still return zero. This follows `research-wading/docs/spec/water-services.md` sections on the null grid and disturbance query; the real-level `WaterImage` supplies no runtime grids. The synthetic test covers dry, wet refusal, a falsely declared grid owner with a null pointer, and an explicitly supplied authored grid.
- **PSR1-2 (MEDIUM):** The Godot camera snapshot passes an absent mode because `Camera3D` has no verified source for the original follow-mode word. `camera_flag` refuses with a label until a live mode is supplied; yaw and frustum remain available. The native test covers absent mode and both documented boolean flag values from `research-combat-walk/docs/spec/movement-camera.md`.
- **PSR1-3 (MEDIUM):** Binding an actor invalidates its cached root displacement. Rebinding the animation runtime also clears stale actor and motion bindings. The native test requires a new advance after actor-state replacement, per `t3/research-player-anim` `cda95dc` and the `combat_runtime::Services` contract.

### Audit verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: passed. `cmake --build build/native --target champions-player-services-tests -j 4`: passed on arm64. `ctest --test-dir build/native -R '^player-services$' --output-on-failure`: 1/1 passed after the final edit.
- `ctest --test-dir build/native -j 1 --output-on-failure -R '^(player-services|world-queries-(host|exact|transport)|terrain-file-(host|exact|transport)|session-terrain-host|creature-water-host|creature-anim-(api|exact)|animation-decoder-(api|exact|transport)|real-world-adapter|level-flow-water-adapter|combat-runtime-host)$'`: 17/17 passed serially, no timeouts. The binaries inspected were Mach-O arm64.
- `cmake --build build/godot-extension --parallel 2`: passed. `python3 /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration/work/playable-wave25/run-godot.py`: 54/54 passed. The headless Full combat row passed 2,620 checks, zero failures, and retained `FULL_TRACE` 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- `bash /Users/syndicatemike/Projects/open-champions/worktrees/play-enemies/work/play-enemies/run-sweep.sh`: all four sequential slices passed. Counts only: 111 levels tried, loaded and played; 230 checks passed, zero failures, zero refusal kinds; 111 scripts attached and 76 started. The read-only disc held 994 files before and after.
- The final synthetic `player-services` binary passed **37 checks**. `cmake --build build/native -j 4` compiled the changed target and almost all other targets, but an unrelated `level_flow_tests.cpp` Release compile remained active after over 20 minutes under heavy shared load. I stopped only that build's exact PID; a full-build pass is not claimed. The focused target was rebuilt after the last test edit and the serial 17-test CTest was rerun successfully.

The original runtime camera mode, owned water ripple-grid lifetime, and shared World actor/clip registration remain unported boundaries. The Full trace is synthetic fixture evidence; the level sweep does not establish a campaign fight.
