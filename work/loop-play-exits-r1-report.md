STATUS: PASS for audit fixes PE-1..PE-4 and playthrough step 6; automatic walk-in triggering and gate travel remain refused.

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

## Audit fixes

- **PE-1:** The diagnostic menu action now refuses `PushTrigger` records with `once`, `shop`, or `offOnly`. It refuses a `Trigger` with any re-arming property and, conservatively, every `Trigger` on a level containing a `TriggerParams` record because the host does not model rectangle linking, entry edges, recall countdown, or re-arming state. Refused entries are no longer advertised as script actions. The button remains a bounded diagnostic, not a normal position/use interaction.
- **PE-2:** Exit classification now compares the parsed class directly with the documented plain names. Leading digits are not stripped.
- **PE-3:** The scene remembers a refused level request and does not queue the same request again on each observation tick. Attaching a new script resets that refusal, and a distinct request can be tried.
- **PE-4:** A change with a pending post action is refused before replacing the script host, since that action should run in the new level and this host cannot carry it. The `save_campaign` image carries the campaign's player records and quest variables. It does **not** carry the `level_flow::State` gate list, level states, save buffer, gate travel, recall state, or retained spawn state. The selected entrance is used for this transition's placement, but the old `g_entranceName` is not retained for a later change. These are still unsupported cross-level state paths.

Evidence: `research-levels/docs/gameplay.md` trigger and push-trigger sections; `research-levels/docs/level-flow.md` §§7.4, 9; `research-script-session/docs/script.md` post-action section; local `native/src/quests.cpp` campaign save schema and `native/src/script_commands.cpp` campaign bridge. No inferred retail values were added.

Audit-fix verification (macOS arm64, 2026-10-22):

- `cmake --build build/godot-extension --config Release --parallel 2`: passed; rebuilt the archive extension with the exit guards. `file build/native/champions-script-commands-tests` identified an arm64 executable.
- `ctest --test-dir build/native --output-on-failure -j 3 -R '^(script-commands|level-flow-exact|quests|script-builtins-api)$'`: **4/4 passed**.
- `python3 work/play-exits/run-godot.py`: Godot dev matrix **54/54 exited 0**. The full-combat row retained `FULL_TRACE` SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` at **36,402 bytes**.
- `Godot --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder=<owner-disc> --cache=<ignored-work-cache> --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary=<ignored-work-json>`: **111 candidates, 111 loaded, 111 played; 224 checks passed, 0 failures, 0 refusal kinds**.
- `python3 tools/run_loop_playthrough.py --godot <local-Godot> --game-folder <owner-disc> --out work/play-exits/audit-playthrough`: step 6 was **changed** in both headless and rendered modes, with campaign image and player record carried. The rendered transition screenshot was inspected. Both complete playthrough processes still exited 1 on the later combat/save/load refusals stated above; step 6 itself passed. The read-only disc retained **994 files** before and after.
- `git diff --check`: passed. Logs, screenshots and cache are under ignored `work/play-exits/`.
