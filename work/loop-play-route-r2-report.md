STATUS: PARTIAL — route, Godot matrix, 111-level sweep, FULL_TRACE and focused CTest passed; full CTest remained 229/231 after two timeouts.

2026-10-23 implementation; 2026-10-24 report finalized | `codex/play-route` at base `7ed3f21` | implementation `1786cf0`; local only; no push

## Implementation

- The RTA script host now lets the documented `trigger` built-in reach its virtual call. It records only slot 3, argument 0 on the named hub `portal` object. The target method and visible effect are unverified, so the host does not emulate either. A different slot, object or argument still refuses.
- The GDExtension exposes the parsed level's `Player1` records with source indices and property lists. The route asserts the complete blood1 census at indices 227, 287 and 298, with the exact documented properties. It keeps the existing arrival placement; actual marker selection remains unverified.
- The playthrough checks zero portal requests before mission 1 is offered, one after the hub dialog unlocks it, and zero on repeat. It requires these checks, the three exact markers, the menu request, and carried player/campaign state for `--route-only` success.

Evidence: `t3/research-story-route-r3` `902b352`, `docs/story-route.md` and its PASS audit, especially the guarded `gatewaytrigger` execution and exact blood1 archive census. The prior round-1 route report records the existing menu and arrival boundary.

## Verification

- macOS arm64 Release native and Godot development extension builds passed. Focused CTest `^(level-flow-exact|script-commands|quests|script-builtins-api)$`: 4/4 passed.
- Bounded route playthrough: headless and rendered exited 0. Both report before-offer 0, offered 1, repeat 0 portal requests and 3/3 exact marker records. The rendered mission menu and blood1 arrival captures were inspected.
- Full arm64 CTest `-j 3`: **229/231 passed** in 2,681.71 seconds. Only `session-facing-exact` (600-second subprocess timeout, reported Failed) and `player-exact` (240-second CTest Timeout) failed under shared machine load. `netplay` passed after 754.12 seconds. Isolated `session-facing-exact` passed in 452.91 seconds; isolated `player-exact` again timed out at 240.11 seconds. The direct player vector command without CTest's time cap passed **274,304 exact complete-state cases and transport refusals**. The failed aggregate and isolated CTest timeout remain recorded separately from that direct pass.
- Godot 4.5.2 development matrix: **54/54 commands exited 0**. Its full-combat headless smoke passed 2,620 checks with zero failures; FULL_TRACE 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from round 1. The previously noted `scene_play_flow` and `ui_layout_smoke` script/API warnings remain in those two logs despite exit 0.
- Scripts-on 111-level sweep: **111 tried / 111 loaded / 111 played**, 224 checks passed, zero failures, 111 scripts attached, zero script faults. Its 14 script refusal stops and 267 stub refusals match the earlier route baseline; the owner disc remained read-only.

Commands run from this worktree (`<owner-disc>` is the read-only owner disc path; `<out>` is ignored work output):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake --build build/godot-extension --config Release --parallel 2
ctest --test-dir build/native --output-on-failure -j 3 -R '^(level-flow-exact|script-commands|quests|script-builtins-api)$'
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^session-facing-exact$'
/opt/homebrew/bin/python3.14 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz
python3 work/play-route/run-godot.py
Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<out> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<out>
Godot --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<out> --route-only
Godot --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<out> --route-only --rendered
```

## Remaining boundary

The route invokes the researched Trigger record through the selected-object host action; physical walk-in dispatch is not established. The original portal target's virtual method and rendered effect, the blood1 `Player1_ctor` marker choice, final spawn coordinates, and combat are not established by this route. This change records the bounded request and archive markers without claiming those effects.
