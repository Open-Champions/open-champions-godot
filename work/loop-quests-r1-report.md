STATUS: PARTIAL — the opening quest, runtime journal, documented notice host, and hub-to-mission-1 route work; a live mission-1 turn-in and the separate mission-2 hub action remain unverified.

2026-10-24 | `codex/play-quests` from `7ed3f21` | local only; no push

## Implementation and evidence

- The base already routed the documented RTA quest commands through the script host and carried campaign state between levels. This change adds a read-only `quest_journal()` view of player 0's active title/body localization keys, with the original `NoQuests` key for an empty list. It exposes mission-1 completion and mission-2 availability separately from the quest list.
- The Godot quest page uses the selected read-only disc's `inventry.lmp/parchment.tex` and font data at run time; Q and the scene action open it. Text is clipped to the documented `(0,119)-(320,440)` rectangle. The first rendered capture showed a doubled parchment; decoding the woven art removed that visual defect. Images are only under ignored `work/quests/`.
- The synthetic native script test exercises `setv`, `setMissionAvailable`, `addQuest`, `removeQuest`, `grantMission`, `newWorld('hub')`, then a *separate* mission-2 availability action. The playthrough checks 0→1 journal entries after the opening NPC choice, the journal's runtime art/font, and the existing hub menu → `blood1` route. It also checks that mission 2 did not unlock merely from that route.
- Evidence: `t3/research-quest-log` `03c2304`, `docs/spec/quest-log.md` and its passing audits; command model from `t3/research-quests` `7efd3ce`; route base `7ed3f21`. The text notice/help and compass host paths existed at the base. A quest row alone does not imply a HUD objective; no automatic objective was invented.

## Verification

- macOS arm64 Release native build passed. Focused CTest `^(script-commands|quests|script-builtins-api|level-flow-exact)$`: **4/4 passed**. Full CTest (`-j 3`): **231/231 passed**, zero failures, 1,604.17 seconds.
- Godot 4.5.2 dev extension and the level-texture/creature-animation extensions built. Godot development matrix: **54/54 commands exited 0**. The `scene_play_flow` and `ui_layout_smoke` logs retain the same pre-existing script/API errors despite exit 0. Full-combat headless: **2,620 checks, zero failures**; `FULL_TRACE` **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from the route base.
- Bounded `loop_playthrough.gd --route-only` passed headless and rendered: opening dialog reached and closed, 0→1 journal entries after its choice, Q opened the runtime journal, mission 1 was available, the selected hub PushTrigger host action opened the native mission menu, and the story request arrived at `blood1` with campaign image and player record carried. Mission 2 stayed locked. The second rendered journal capture was visually inspected. The dialog visual probe still reported zero box sprites/glyphs while the script choice path completed.
- Scripts-on real-data sweep over the read-only owner disc: **111/111 tried, loaded, played**, **224 checks passed**, zero failures, 111 scripts attached, zero script faults. The disc still contained 994 files afterward; no extracted bytes, text or images are tracked.
- The first focused CTest attempt ran before the native build produced the level-flow test executable: 3 passed, 1 Not Run. The completed-build rerun passed 4/4. The first Godot editor import stopped because the level-texture extension binary had not yet been built; after both helper extensions were built, the import and scene parse passed with no script errors.

Commands run from this worktree (`<owner-disc>` is the read-only owner folder; all output paths are ignored `work/`):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 1 -R '^(script-commands|quests|script-builtins-api|level-flow-exact)$'
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<pinned-godot-cpp> -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 2
cmake -S gdextension/level_texture -B build/level-texture -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<pinned-godot-cpp> -DGODOTCPP_TARGET=template_debug
cmake --build build/level-texture --config Release --parallel 2
cmake -S gdextension/creature_anim -B build/creature-anim -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<pinned-godot-cpp> -DGODOTCPP_TARGET=template_debug
cmake --build build/creature-anim --config Release --parallel 2
Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
Godot --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<ignored-output> --route-only
Godot --path godot --script res://tests/loop_playthrough.gd -- --game-folder=<owner-disc> --out=<ignored-output> --route-only --rendered
Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<ignored-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<ignored-summary>
python3 work/play-route/run-godot.py
```

## Remaining boundary

- The rendered journal uses the observed parchment, text clip and title/body keys through the current font path, but exact original glyph placement, wrapping and retail pixel parity need an original rendered capture. Its current line spacing follows real font measurements and is an implementation choice, not a proved original value.
- The synthetic turn-in test proves command bookkeeping and a queued hub request, not the shipped `exitzone` demo/counter branch in live combat. The separate hub action's live dialog reachability and mission-2 selection remain unverified. The route still invokes the selected hub PushTrigger action through the host; physical proximity/use dispatch and exact arrival marker are not proved.
- `setInfo` and `addHelpMessage/F` render through the existing timed HUD and modal help paths. `showCompass` remains subject to the existing documented camera/menu gate and its unresolved mesh/key evidence. Which script shows an objective during a real mission remains unknown.
