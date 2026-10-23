STATUS: PARTIAL. Playthrough step 6 changes real levels from a walked-in Trigger and carries the player record. Exact original TriggerParams linking, normal PushTrigger use, gate menu/travel, and final portal free-spot arrival remain refused.

2026-10-23 | `codex/play-exits` from `d554959` | local implementation only; no push

## Implementation and evidence

- `level_flow::Trigger::run_exact` now models signed delay, inclusive x/y entry, lower player slot, recall suppression after slot reset, linked action-before-script dispatch, onOnly/offOnly precedence, timeout and needScriptHit rearming, and the five saved state bytes. Its callbacks accept the selected player; the live bridge binds the Trigger object and active player for its own script. Synthetic cases cover forced-off, delay, recall, failed script hit, and disabled state.
- `portal_arrival` is a bounded native rule for the researched rank-3 Portal/PortalHurt arrival. It invokes the caller's anyHeight free-spot search for each player and draws facing from the shared seed after each search. Its synthetic test checks search arguments, rank, slot order, seed, and facing. It is not connected to real-level loading.
- The Godot bridge schedules eligible Trigger records on 15 slow-list buckets and samples the native player's position on accepted ticks. Object scripts run through `Host::run_object_entry`, which binds the selected object and active player. A level with TriggerParams or a known unmodelled slow-list constructor refuses automatic dispatch; the UI reports that refusal. In-session trigger delay and occupancy carry on a return to a previously visited level.
- The playthrough finds the researched four-Trigger source from counts-only reads of the owner-supplied `objects.ob`, uses movement commands to enter a rectangle, and requires that entry before accepting a queued level change. It checks the carried campaign image and player record. It does not invoke the selected-script-action diagnostic.
- The final headless step 6 changed real levels after a walked entry: `walked_into=true`, `trigger_samples=224`, `trigger_dispatches=3`, `record_carried=true`, `campaign_carried=true`, `campaign_image_restored=true`, and `arrival_at_marker=true`. The marker is rank 2 and labelled `pre_search=true`; it does not establish the final original free-spot coordinate. The playthrough found three earlier entered Trigger edges with no level request and four explicit Trigger script refusals during its search. Later combat and native save/load steps remain refused, so the whole wrapper exits nonzero.

Evidence: `t3/research-exits-walkin` at `ba48fe5`, `docs/exits-walkin.md` (audited PASS with LOW EXW-1), plus the pinned object-system and level-flow contracts. EXW-1 is handled as observed: recall resets the found slot to -1 before suppressing the edge.

Changed files: `native/include/champions/level_flow.hpp`, `native/src/level_flow.cpp`, `native/tests/level_flow_tests.cpp`, `gdextension/src/session_controller.hpp`, `gdextension/src/session_controller.cpp`, `godot/scenes/real_data_play.gd`, `godot/tests/loop_playthrough.gd`, and this report.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 4`: PASS. `build/native/champions-level-flow-tests` is arm64 Mach-O.
- `ctest --test-dir build/native --output-on-failure -j 3 -R '^(level-flow-exact|script-commands|quests|script-builtins-api)$'`: PASS, 4/4.
- `ctest --test-dir build/native --output-on-failure -j 3`: PASS, 231/231; `player-exact` passed in 162.88 seconds, so no serial no-timeout retry was needed.
- `cmake --build build/godot-extension -j 4`: PASS, including the Godot development extension.
- `python3 work/play-exits/run-godot.py`: PASS, 54/54 Godot dev matrix. The full-combat smoke's `FULL_TRACE` is 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from the pre-change trace.
- `../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-supplied-disc> --cache=<local-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<local-summary>`: PASS, 111/111 candidates, loads and plays; 224 checks passed, zero failures. The disc remained read-only at 994 files.
- `python3 tools/run_loop_playthrough.py --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder=<owner-supplied-disc> --out work/play-exits/r2-verified-playthrough`: step 6 PASS in headless and rendered mode, each with the same carry/arrival results; rendered destination frame inspected. The overall wrapper exits 1 because later campaign-combat and native save/load steps still refuse. Ten rendered frames captured.

- `ctest --test-dir build/native --output-on-failure -j 3`: 230/231 passed; `player-exact` alone timed out at its 240-second limit under simultaneous suites in sibling worktrees. An isolated `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'` retry also timed out at 240 seconds. `/opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz` passed all 274,304 complete-state cases and transport refusals without CTest's fixed timeout. The full suite remains 230/231 at its configured timeout; the comparator result supports a timing-only cause under this load.

## Remaining boundary

The linked TriggerParams box query is not represented by this bridge, so any level containing such records is refused conservatively. The research does not enumerate every other constructor that advances the shared 15-list cursor; the bridge refuses known unmodelled cases and does not claim complete original scheduling across arbitrary levels. Normal nearest-object PushTrigger interaction and host/network prompts are not wired. JumpGate discovery, list persistence, eight-state menu input, 180-tick real-level travel, and destination gate construction are not wired. The real scene still starts at the documented marker before Player1's live free-spot search; exact terrain-dependent arrival and original facing cannot be claimed. Pending post actions, same-level reload, and complete level-state save/re-entry remain refused.

Private disc files were read only for acceptance. No names, decoded content, or game bytes are committed.

## Audit fixes

- PER2-1: The live Trigger reads the documented `t` property alone. A missing or empty `t` refuses automatic dispatch. The synthetic Godot smoke supplies both `script=absent` and `t=tick` and observes a non-missing walk-in dispatch; its alias-only case refuses walk-in.
- PER2-2: Each successful live Trigger entry dispatch records its object index only when that dispatch changes the pending level request to a queued destination. Playthrough step 6 now requires a geometric entry, a new queued-dispatch event for the entered record, and equality between that event's destination and the accepted pending request before applying the level change. The existing arrival and carried-record checks remain.

Verification after these fixes:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 4`: PASS. `cmake --build build/godot-extension -j 4` and the final `-j 2` rebuild: PASS.
- `ctest --test-dir build/native --output-on-failure -j 3 -R '^(level-flow-exact|script-commands|quests|script-builtins-api)$'`: PASS, 4/4.
- `../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/script_commands_smoke.gd`: PASS, including both Trigger property cases.
- `python3 work/play-exits/run-godot.py`: PASS, 54/54. Its `FULL_TRACE` is 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged.
- `../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-supplied-disc> --cache=<local-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<local-summary>`: PASS, 111/111 candidates loaded and played, 224 checks passed, zero failures or refusal kinds.
- `python3 tools/run_loop_playthrough.py --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder=<owner-supplied-disc> --out work/play-exits/audit-fix-final-playthrough`: step 6 PASS in headless and rendered runs with `walked_into=true`, `queued_by_walk=true`, one request-queueing Trigger dispatch, `record_carried=true`, `campaign_image_restored=true`, and `arrival_at_marker=true`; the rendered destination frame was inspected. The wrapper still exits 1 for the later combat and native save/load refusals. Ten rendered frames were captured. The disc remained 994 files before and after.
