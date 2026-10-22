STATUS: PASS for playthrough blocker #6's documented script exit path; automatic walk-in triggering and gate travel remain refused.

2026-10-22 | `codex/play-exits` from wave-24 `2e57376` | local implementation only

## Added

- Exposed the real level's `Trigger`, `PushTrigger`, `JumpGate`, `Portal`, and `PortalHurt` records to the play scene. The user can select a documented `Trigger.t` or `PushTrigger.action` in the menu. Ordered `key` and `action` properties run with the selected object and active-player script context. `shop`, `offOnly`, portal use, and JumpGate prompts remain explicitly refused where this host lacks their conditions or interaction.
- Routed offline `newWorld`, `newWorldAlways`, `simpleNewWorld`, `newWorldParameter`, and `setEntrance` through the existing script binder. Queued changes from those commands or dialog `newWorldPost` reach the scene boundary. The next level comes from the selected game folder; no destination is guessed. Existing campaign image, quest variables, and live player record carry into its new script host. Entry placement uses the documented previous-level and entrance marker path.
- Brought the scripted playthrough into this worktree and extended step 6 to exercise a real level action and assert level index, player record, and campaign image carry. The test searches real records without committing proprietary level names or contents.

Evidence: sibling `champions-return-to-arms-re/docs/script.md` for scene commands and offline `net_closeLocalMenus`; its `docs/gameplay.md` for trigger and push-trigger script context; local `docs/LEVEL_FLOW.md`, `docs/DIALOG.md`, and `native/src/level_flow.cpp` for entry markers, carry, and refused gate arrival.

## Verification

- Release build: `cmake --build build/godot-extension --config Release --parallel 2` passed. The level-texture and creature-animation extensions also built in Release.
- Arm64 CTest: `ctest --test-dir build/native --output-on-failure -j 3 -R '^(script-commands|level-flow-exact|quests|script-builtins-api)$'` passed **4/4**. `file` confirmed the script-commands test executable is Mach-O arm64.
- Godot dev matrix: **54/54 commands exited 0** after building the creature-animation dependency. `scene_play_flow` and `ui_layout_smoke` still print their pre-existing internal script/API errors despite exit 0; identical messages were present in the wave-23 baseline logs.
- Final real-level sweep: **111 candidates, 111 loaded, 111 played**, 224 checks passed, 0 failures, 0 refusal kinds. The command used `res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<ignored-work-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<ignored-work-json>`; Godot exited 0.
- Final headless and rendered `res://tests/loop_playthrough.gd`: step 6 reported `changed`, different source/destination level indices, `campaign_carried=true`, `campaign_image_restored=true`, and `record_carried=true`. The rendered transition screenshot was inspected in the play scene. Both full playthrough runs exit 1 due later existing combat/save/load refusals; they do not establish a complete campaign playthrough.
- Final `res://tests/playable_wave_smoke_full_combat.gd -- --ready-ticks=640`: exit 0, `FULL_TRACE` 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- `git diff --check` passed. Private game files were read only for acceptance; all private logs, caches, and screenshots remain ignored under `work/play-exits/`.

## Remaining boundary

The menu action is a diagnostic way to invoke an object script. Automatic player entry into a Trigger rectangle and normal proximity/use dispatch are not connected to this Godot play loop. JumpGate travel and portal anyHeight arrival are refused by the existing native contract. Same-level reload is refused. Thus the verified result is a real cross-level transition and carry, not an unassisted retail-style exit interaction.
