STATUS: PARTIAL — the audited branch and carry assertions now pass: good reaches its expected keyed destination with restored script, campaign and player state; evil reaches its expected keyed request and verifies the pending-post-action refusal. Live cutscene/camera binding, doors, combat, mission completion and hub return remain unverified; one full-suite native timeout remains unresolved.

2026-10-25 | `codex/play-blood1` from `7ed3f21` | local only; no push

## Evidence and implementation

- Research: `t3/research-blood1-scripts` `532f69c`, `docs/blood1-scripts.md`, and census `t3/research-blood1` `5107fcb`. The script has 21 entries and no enemy spawn, wake, boss, mission grant or direct hub return. The authored 140 hostiles are object records. The blood1 exits request `strife` or `drundar`; a conditional `exitzone` branch in the destination scripts grants mission 1 and requests `hub`.
- The Godot script-host adapter now exposes counts-only story state: objective/mission and choice bits, pending post action, command refusals, and an in-memory record census. It does not export object names, positions, game bytes or dialog text.
- The playthrough continues the existing hub route into blood1. It checks the documented 363 records, 21 entries, three arrival markers, one walk-in trigger, two keyed exits, two doors and 140 hostile placements. It now asserts a clean initial decision/evil state, the selected terminal's exact difficulty-1 choice bit, and the independently researched good→`strife` / evil→`drundar` request. It retains every earlier hub route/carry gate in `--blood1-only` success. It makes no synthetic spawn or kill.
- The scene now resets and records the script-attachment result for each level load, so a geometry-only load or stale carry flags cannot satisfy the test. The good destination gate requires script attachment, campaign image restoration, and matching player-record and decision/evil state across the transition. The evil boundary passes only when the expected destination stays queued, the post action is pending, the level stays in blood1, and the host explicitly refuses that transition for the pending post action.
- The Trigger and PushTrigger calls use the selected-object host action. Physical occupancy edges, prompt/proximity, live cutscene actor binding, camera keys, AutoProp collision and actor deletion are not claimed. The client labels the Trigger action as a diagnostic.

## Playthrough results

- Good branch, headless and rendered: initial decision/evil were zero; the good decision bit matched the documented difficulty-1 mask, the keyed request matched `strife`, and the destination script, campaign image, player record and decision/evil state all survived the transition. Mission 1 was still incomplete there, so the counter/demo completion branch and hub return were not executed.
- Evil branch, headless and rendered: initial decision/evil were zero; the evil decision bit matched the documented difficulty-1 mask and the keyed request matched `drundar`. The item/deferred post action remained pending after the unported door cutscene. The bounded runner now exits 0 only because it verifies the specific pending-post-action refusal and unchanged level; it does not claim the destination loaded.
- The first rendered capture was taken before the dialog draw task produced a box. The bounded probe now advances up to 300 displayed frames and records the draw state. The final rendered capture was inspected: the dialog box, speaker and body glyphs are visible. `choice_ready` was still false at the capture limit and the text was mid-reveal, so final layout is unverified; the following input loop nevertheless completed the decision.

## Verification

- Release arm64 native build and the three Godot extension builds were configured with the pinned Godot C++ checkout and completed. The large `level_flow_tests.cpp` translation unit finished after the initial testing attempt.
- Focused serial arm64 CTest `^(level-flow-exact|script-commands|quests|script-builtins-api)$`: **4/4 passed**; the level-flow and script-command executables are Mach-O arm64. An earlier combined run reached 3/4 only because the level-flow executable was not built yet.
- Full Release arm64 CTest `-j 3` on the audit-fix tree: **230/231 passed**. `session-facing-exact` passed in 335 seconds and `netplay` passed in 661 seconds. Only `player-exact` hit its 240-second CTest timeout; its required **serial** rerun also timed out at 240 seconds, with no value mismatch reported. The full native gate remains **unresolved for that one case**. The earlier pre-fix full run was 229/231, with both `player-exact` and `session-facing-exact` timing out; the latter passed in this new run.
- `python3 tools/run_loop_playthrough.py --godot <pinned-Godot> --game-folder <owner-disc> --out work/play-blood1/pb1-fix-final-good --blood1-only`: **headless 0, rendered 0**, 30 steps each, nine rendered captures, zero refusal kinds. The destination script-attached, campaign-restored, player-carried and decision-carried gates were true. The dialog and destination captures were inspected locally; the destination still uses a placeholder actor and the capture is dark. No image is committed.
- The same runner with `--out work/play-blood1/pb1-fix-final-evil --blood1-choice-evil`: **headless 0, rendered 0**, 30 steps each, nine rendered captures, one expected refusal kind in each mode. The evil post-action refusal gate was true and the destination attachment gates were false. The disc had **994 files before and after** each run.
- The matrix's headless Full-combat case passed **2,620 checks, zero failures** and printed `FULL_TRACE` **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to the `play-route` base report. This synthetic trace does not validate blood1 combat.
- Godot development matrix `python3 work/play-route/run-godot.py`: **54/54 commands exited 0** on the audit-fix tree; a separate final editor import exited 0. The `scene_play_flow` and `ui_layout_smoke` logs retain their baseline script/API errors despite exit 0; no other matrix log contains `SCRIPT ERROR` or `ERROR:`.
- Scripts-on read-only sweep, `Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<ignored-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<ignored-summary>`: **111/111 candidates tried, loaded and played; 224 checks passed, zero failed, zero refusal kinds, zero script faults** on the audit-fix tree. The aggregate retains **14 script refusal stops** and **15,577 unbuilt object selections** across all levels; this is not proof that those objects work. The disc still had **994 files** after the sweep.

All commands ran from this worktree. `<owner-disc>` is the read-only game-folder input, `<pinned-Godot>` is the local Godot executable, and `<godot-cpp-checkout>` is the pinned local Godot C++ checkout. Resolved paths and output stayed in ignored local logs under `work/play-blood1/`:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<godot-cpp-checkout> -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 2
cmake -S gdextension/level_texture -B build/level-texture -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<godot-cpp-checkout> -DGODOTCPP_TARGET=template_debug
cmake --build build/level-texture --config Release --parallel 2
cmake -S gdextension/creature_anim -B build/creature-anim -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<godot-cpp-checkout> -DGODOTCPP_TARGET=template_debug
cmake --build build/creature-anim --config Release --parallel 2
ctest --test-dir build/native --output-on-failure -j 1 -R '^(script-commands|quests|script-builtins-api)$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^level-flow-exact$'
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^session-facing-exact$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^(level-flow-exact|script-commands|quests|script-builtins-api)$'
python3 work/play-route/run-godot.py
```

## Remaining work

Port live cutscene resource binding and camera/key scheduling, AutoProp state/animation/collision and revisit restore, actor cleanup, the evil item/deferred action, the authored hostile constructors and combat, physical trigger/use dispatch, and the destination-script completion inputs. Re-run both branches and a continuous mission-complete return before claiming blood1 playable. The unrelated `player-exact` timeout also needs a successful run under adequate CPU capacity before the full native gate can be called green.
