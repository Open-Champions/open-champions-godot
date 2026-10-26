STATUS: MORE — three audit fixes implemented; final gate measurements in progress. Unsupported loading prerequisites now refuse rather than handing off control.

2026-10-25 | `codex/play-loading` | base `b99bc949` | local commits only, no push

## Scope and evidence

- NLOAD-5: the production warm-up checks an explicit prerequisite state adapter, requiring boolean deferred completion, equipment pending, peer readiness and store completion. The released controller does not expose that adapter, so transitions explicitly call `_refuse_loaded_transition`. No offline no-work subset is asserted. Terrain reset and the 1,500-present refusal remain; equipment pending is no longer supplied as literal false. This deliberately prevents the previously unsupported hub-to-mission handoff.
- NLOAD-7: per-cell decode results are independent of resident mesh arrays. Required failed cells return their error through `loading_state` and the host readiness adapter; successful empty cells remain ready. Both synchronous decoding and asynchronous publication record the result. Eviction clears it. Failed cells outside the wanted window do not refuse the current window.
- NLOAD-8: supplied-draw service success uses the bridge error independently of `sound_tick`'s queued flag. The real-audio regression passes a valid no-queue packet and verifies sound/draw counters and an empty bank-error list.
- Authored from `research-loading@3ba61c7`, `docs/loading-transitions.md:95-105,139-149,197-230`, the named audit, and the existing native bridge return contract. No retail bytes or decoded content are tracked.

## Verification so far

- Release configure and native build passed: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`.
- Focused Godot tests passed before the additional host-propagation checks: loading display 55/55, real-audio 10/10. Godot 4.5.2, synthetic fixtures only.
- Full `ctest --test-dir build/native --output-on-failure -j 3`, development matrix, scripts-on 111-level sweep, and headless/rendered route probes are running. Gate figures will be appended after completion. No successful end-to-end route is claimed with the unsupported prerequisite adapter.

## Limits

Deferred/equipment/peer/store state services remain unsupported. A future supported host must supply checked completion/pending state, not elapsed-time or literal-negative substitutes. Original GPU parity, audible stream promotion and first-control timing remain unverified.
