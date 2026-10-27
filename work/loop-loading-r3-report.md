STATUS: MORE — three audit fixes implemented and locally committed. Route acceptance is blocked by unsupported loading prerequisites; full CTest is still running.

2026-10-25 | `codex/play-loading` | base `b99bc949` | local commits only, no push

## Scope and evidence

- NLOAD-5: the production warm-up checks an explicit prerequisite state adapter, requiring boolean deferred completion, equipment pending, peer readiness and store completion. The released controller does not expose that adapter, so transitions explicitly call `_refuse_loaded_transition`. No offline no-work subset is asserted. Terrain reset and the 1,500-present refusal remain; equipment pending is no longer supplied as literal false. This deliberately prevents the previously unsupported hub-to-mission handoff.
- NLOAD-7: per-cell decode results are independent of resident mesh arrays. Required failed cells return their error through `loading_state` and the host readiness adapter; successful empty cells remain ready. Both synchronous decoding and asynchronous publication record the result. Eviction clears it. Failed cells outside the wanted window do not refuse the current window.
- NLOAD-8: supplied-draw service success uses the bridge error independently of `sound_tick`'s queued flag. The real-audio regression passes a valid no-queue packet and verifies sound/draw counters and an empty bank-error list.
- Authored from `research-loading@3ba61c7`, `docs/loading-transitions.md:95-105,139-149,197-230`, the named audit, and the existing native bridge return contract. No retail bytes or decoded content are tracked.

## Verification

- Release configure and native build passed: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`.
- Godot extension Release build passed: `cmake --build build/godot-extension --config Release --parallel 2`.
- Focused Godot 4.5.2 tests: loading display **57/57**, real-audio **10/10**. The injected-failure loading test exits **1**, reporting **57/58**. Synthetic fixtures only. The audio case also verifies the countdown advances while stream-0/2 queued counts stay unchanged.
- Development matrix: **57/58 commands exited 0**; only `loading-route` exits **1** on the explicit prerequisite refusal. Separately, the unchanged `scene_play_flow` fixture emits two script errors concerning a missing `loaded` property despite exiting 0 with 65 checks; it is not claimed diagnostic-clean and is outside this patch.
- Read-only loading-art acceptance: **111/111** selections, **12** fallbacks, **0** refusals. Full combat smoke: **2,620** checks, **0** failures. `FULL_TRACE`: **36,402 bytes**, SHA-256 **9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659**, unchanged.
- Scripts-on sweep: **111/111** levels loaded and played, **224** checks passed, **0** failures, **111** scripts attached, **0** script faults. The relative summary destination did not exist under the Godot project; the same counts-only summary was recovered from stdout into ignored `work/play-loading/r3/sweep-summary.json`. Generated cache was moved from the project-relative output into ignored `work/play-loading/r3/sweep-cache`.
- Headless and rendered route probes both exit **1**, with **one explicit prerequisite refusal each**, **loading_ready=false**, **2 loading frames** and **0 audio handoff errors**. The rendered probe produced **9** captures. Inspection of its loading/arrival captures confirms a stopped session, not a playable handoff or verified fade. Captures remain ignored; no game images are tracked.
- Full `ctest --test-dir build/native --output-on-failure -j 3` is still running; final counts will follow. No successful end-to-end route is claimed.

## Commands

From this worktree, `<Godot>` is the existing Godot 4.5.2 executable in the integration worktree; `<owner-disc>` is the authorized read-only disc folder. Private output is ignored.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
cmake --build build/godot-extension --config Release --parallel 2
<Godot> --headless --path godot --script res://tests/loading_display_cases.gd
<Godot> --headless --path godot --script res://tests/loading_display_cases.gd -- --inject-failure
<Godot> --headless --path godot --script res://tests/loading_audio_cases.gd
python3 work/play-loading/run-godot-matrix.py
<Godot> --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=work/play-loading/sweep-cache-r3 --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=work/play-loading/r3/sweep-summary.json
<Godot> --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<absolute-work>/r3/route-headless --route-only
<Godot> --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<absolute-work>/r3/route-rendered --route-only --rendered
```

## Limits

Deferred/equipment/peer/store state services remain unsupported. A future supported host must supply checked completion/pending state, not elapsed-time or literal-negative substitutes. Original GPU parity, audible stream promotion and first-control timing remain unverified.
