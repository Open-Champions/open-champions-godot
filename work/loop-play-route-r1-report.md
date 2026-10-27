STATUS: PARTIAL — the selected hub PushTrigger action now opens a functional host mission menu and the first story choice reaches `blood1` with the player record and campaign image carried; physical PushTrigger proximity/use, exact arrival marker selection and combat remain refused.

2026-10-23 | `codex/play-route` from `d554959` | local implementation; no push

## Implementation and evidence

- `gdextension/src/session_controller.cpp` reads the 13 mission rows from the owner-selected executable at runtime using the documented US v2.00 table layout. A first row other than `blood1` refuses this bounded route. The script host already runs hub `preStart`/`start`, NPC dialog choices, ordered PushTrigger key/action scripts and campaign serialization; the adapter now exposes the native `missionMenu_run` state and pad/fade tick to Godot.
- `godot/scenes/real_data_play.gd` shows a labelled host mission panel when the script opens it. Selecting Mission 1 advances the native menu through its availability, row, story column, fade and state-8 pending-level write; only a `blood1` request is accepted. The existing level loader carries the player record and campaign image, with no added marker coordinate or private asset.
- `godot/tests/loop_playthrough.gd` now stays on the hub after the opening conversation, invokes the researched hub PushTrigger record 11 action, selects the first story mission, and checks `blood1` arrival and carry. The Valor fight is a separate diagnostic after the campaign-route assertion.
- `work/play-route/run-godot.py` records the 54-command Godot development matrix used for this branch and creates its ignored output directory.
- Source: `t3/research-story-route` `d56a70f`, `docs/story-route.md` and its two read-only audits. SRF-1's portal visual effect and SRF-2's exact `Player1` property census were not ported. The tested route does not assert a specific arrival marker or coordinate.

## Verification

- Release native build, Godot extension and its level-texture/creature dependencies: passed on macOS arm64. Focused CTest `^(script-commands|quests|script-builtins-api)$`: **3/3 passed**; focused `^level-flow-exact$`: **1/1 passed**. Full CTest: **231/231 passed** with `-j 3` (1,298.24 s under contention), including `level-flow-exact`, `quests`, `script-commands`, dialog and netplay. `champions-script-commands-tests` is Mach-O arm64.
- Godot 4.5.2 headless and rendered `res://tests/loop_playthrough.gd`: both full runs reported `mission_1_unlock=available`, `hub_trigger_mission_menu=opened`, `mission_1_story_request=blood1` with orientation `255`, and `blood1_arrival=arrived` with `campaign_carried`, `campaign_image_restored`, `record_carried` and mission 1 availability true. The final bounded `--route-only` runs exited 0 in both modes with name, class, level, XP, gold and raw maximum HP/mana preserved. The rendered menu and arrival captures were inspected. Both complete processes exit 1 on later Valor combat and save/load refusals.
- Scripts-on real-data sweep: **111/111 candidates loaded and played**, 224 checks, zero failures, 111 scripts attached, zero script faults. The owner-selected disc remained read-only; its file count was 994 after the run.
- Godot 4.5.2 dev matrix `python3 work/play-route/run-godot.py`: **54/54 commands exited 0**. The `scene_play_flow` and `ui_layout_smoke` logs retain their identical pre-existing script/API errors from `codex/play-exits`; exit codes alone do not clear those warnings. Full-combat headless: 2,620 checks, zero failures; `FULL_TRACE` **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to `d554959`.

Commands run from this worktree (the owner disc path is redacted as `<owner-disc>`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3 -R '^(script-commands|quests|script-builtins-api)$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^level-flow-exact$'
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<pinned-godot-cpp> -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 2
cmake -S gdextension/level_texture -B build/level-texture -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<pinned-godot-cpp> -DGODOTCPP_TARGET=template_debug
cmake --build build/level-texture --config Release --parallel 2
cmake -S gdextension/creature_anim -B build/creature-anim -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<pinned-godot-cpp> -DGODOTCPP_TARGET=template_debug
cmake --build build/creature-anim --config Release --parallel 2
python3 work/play-route/run-godot.py
Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<ignored-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<ignored-summary>
Godot --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<ignored-output> --route-only
Godot --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<ignored-output> --route-only --rendered
```

## Remaining boundary

The hub PushTrigger is invoked through the existing selected-object host action. Its normal position/use dispatch was not established by the research for this specific object, so this work does not claim a physical portal interaction. The generic level loader consumed the runtime entrance and records; the blood1-specific default marker and final coordinates remain unverified. The separate Valor probe still has zero live hostile actors and no handled attack. The later pause-save and load check retain their existing refusals.

## Audit fixes

- PR-1 (MEDIUM): `--route-only` now requires the opening dialog choice to complete, the native mission menu to be closed before the hub record-11 PushTrigger action, that action to return `ok`, and the native menu to be open afterward. The selected mission and arrival checks remain required. The result row records each condition, so an already-open menu or failed action cannot produce a passing route.
- Verified on macOS arm64: focused Release CTest `^(level-flow-exact|script-commands|quests|script-builtins-api)$` **4/4 passed**; `champions-script-commands-tests` is Mach-O arm64. Godot 4.5.2 dev matrix **54/54 commands exited 0**. Its existing `scene_play_flow` and `ui_layout_smoke` script/API errors remain present despite exit 0.
- Scripts-on sweep: **111/111 candidates loaded and played**, 224 checks passed, zero failures, 111 scripts attached and zero script faults. The owner disc remained read-only at 994 files. Full-combat headless: 2,620 checks, zero failures; `FULL_TRACE` **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from the pre-fix report.
- Bounded `--route-only` playthrough exited 0 headless and rendered. Both results recorded `dialog_choice_close=closed`, `hub_trigger_mission_menu=opened` with `hub_trigger_found`, `dialog_choice_completed`, `menu_was_closed`, `trigger_action_ok` and `menu_open` all true, followed by the first story request and arrival. The rendered menu and level-change captures were inspected.

Audit-fix commands (run sequentially by ignored `work/play-route/verify-audit.py`; `Godot` denotes `../integration/work/runtime/Godot.app/Contents/MacOS/Godot`, with the same `<owner-disc>`, `<ignored-cache>`, `<ignored-summary>` and `<ignored-output>` substitutions as above):

```sh
ctest --test-dir build/native --output-on-failure -j 3 -R '^(level-flow-exact|script-commands|quests|script-builtins-api)$'
Godot --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<ignored-output> --route-only
Godot --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<ignored-output> --route-only --rendered
Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<ignored-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<ignored-summary>
python3 work/play-route/run-godot.py
```
