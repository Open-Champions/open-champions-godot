STATUS: PARTIAL — all six audited defects have bounded fixes; original GPU pixels, audible handoff, the missing cosmetic random input, and complete peer/equipment/store readiness remain unverified or refused.

2026-10-25 | `codex/play-loading` | local commits only; no push

## Audit fixes

- NLOAD-1/2/3: Each of the three start presents retains the five recorded layer-6 sprite submissions at (0,0) with `0x80606060`. Both start and normal retained records paint in reverse submission order. The sprite adapter maps tint channels through /128, matching the existing HUD adapter; the later six blank fields stay separate.
- NLOAD-4: The mixer exposes a loader operation with exactly two sound updates, even without a level bank, and no ambient poll or draw-frame advance. The level countdown begins at 60. Session frames resume the internal draw cadence and stream-0 poll. When the 60th poll needs a cosmetic random draw unavailable from the native session packet, it refuses that ambient decision without mutating it; later sound service continues and a supplied draw can resume ordinary polling. No literal draw value is substituted.
- NLOAD-5: Warm-up observes the terrain stream's pending thread, pending cell, and missing wanted cells; it resets the count to 10 while busy and refuses an unresolved stream at the 1,500-present escape. The documented pending-equipment floor is modeled and tested, but this offline host has no equipment queue, peer barrier, or store-stock adapter to observe.
- NLOAD-6: The loading display test accumulates failures, prints passed/total, and exits nonzero on any mismatch. An injected failure exited 1 with 42/43; the ordinary run passed 42/42.

## Verification

- Release arm64 native and Godot extension builds passed. Focused `native-audio|playable-audio` CTest: 2/2. Synthetic loading audio: 8/8.
- Godot development matrix: 57/57 commands exited 0, including loading contract, loading audio, disc art, and route. Read-only art acceptance: 111/111 selections, 12 fallbacks, zero refusals; owner disc file count remained 994.
- Scripts-on sweep: 111/111 levels loaded and played, 224 checks, zero failures, 111 scripts attached, zero script faults. Full combat smoke: 2,620 checks, zero failures. `FULL_TRACE`: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to the prior branch baseline.
- Hub-to-destination route: headless and rendered exited 0 with zero route refusals; 15 and 49 loading frames respectively, zero audio handoff errors. The rendered start, loading, fade and arrival captures were inspected under ignored `work/play-loading/route-rendered-r6/`; none is tracked.
- Full arm64 CTest (`ctest --test-dir build/native --output-on-failure -j 3`): 230/231 passed. `player-exact` hit its fixed 240-second timeout under load; `session-facing-exact` passed in 322 seconds. The isolated serial `player-exact` CTest rerun passed 1/1 in 202 seconds.

## Limits

- The synchronous host loader cannot animate during its blocking call. The eight rotated loading sprites have no modeled geometry contract. Captures show the host-decoded art, not verified original GPU pixels or PS2 blend output.
- Audible stream promotion and the direct-effect mute interval are unverified. The full 60-frame ambient branch needs the original cosmetic random input. Pending-equipment, peer and store work have no host services; they are not claimed as completed by elapsed time. Integrated original first-control timing remains unverified.

## Commands

From this worktree, using the pinned Godot 4.5.2 executable at `../integration/work/runtime/Godot.app/Contents/MacOS/Godot` and the read-only owner-selected game folder:

```sh
cmake --build build/native -j 4
cmake --build build/godot-extension --config Release --parallel 2
ctest --test-dir build/native --output-on-failure -j 1 -R '^(native-audio|playable-audio)$'
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'
<Godot> --headless --path godot --script res://tests/loading_display_cases.gd
<Godot> --headless --path godot --script res://tests/loading_display_cases.gd -- --inject-failure
<Godot> --headless --path godot --script res://tests/loading_audio_cases.gd
python3 work/play-loading/run-godot-matrix.py
<Godot> --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=work/play-loading/sweep-cache-r2 --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=work/play-loading/sweep-summary-r2.json
<Godot> --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<work>/route-headless-r6 --route-only
<Godot> --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<work>/route-rendered-r6 --route-only --rendered
```
