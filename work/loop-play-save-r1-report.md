STATUS: PASS — playthrough blockers #2 and #5 are fixed; unrelated playthrough blockers remain.

# Loop play-save r1 — 2026-10-22

Coordination: `codex/play-save` from wave 24 `2e57376`; local worktree only. The playthrough harness from `codex/playthrough` was brought in as `ab06ce0`. No push or PR. The user's read-only disc stayed at 994 files before and after the playthrough.

## Change and evidence

- The real-data client writes a small `.level` association for an authored `.ocplay` continuation. It records the selected level index and source hashes, plus hashes of the native save, player record, and campaign companion. Loading checks those associations, reloads that level's WORLD/TEX from the selected folder, rebuilds its textured display, then restores native session position, the existing player record and the authored quest campaign. A fresh scene without an in-memory texture association also loaded the save.
- `ChampionsSessionController` can reuse the verified level display during native continuation preparation. The ordinary synthetic save path still uses its existing terrain presentation. The campaign bridge exposes the existing native `script_commands::Host::save_campaign/load_campaign` format; it does not claim PS2 memory-card compatibility.
- `t3/research-menus` documents five normal pause entries and no Save entry (`docs/menus.md` section 6). The client keeps those five and labels F5 as a host shortcut to the documented in-game save screen. The host in-game Save action uses the same screen when the front end is active. `t3/research-saves` and `docs/FRONTEND.md` document that save-screen route and the player, quest, level and position fields.
- An open dialog or help box refuses a real-level save. Active world-object state and in-flight script execution are not serialized; the UI labels that world objects restart from level data. Norrath and PS2 slot round trips remain outside this native continuation.

Changed: `gdextension/src/session_controller.{cpp,hpp}`, `godot/scenes/{session_sandbox,play_flow,real_data_play}.gd`, `godot/ui/frontend_menus.gd`, `godot/tests/loop_playthrough.gd`. Added the playthrough harness and runner from `75cca7d` as `ab06ce0`.

## Verification

- GDExtension arm64 `template_debug` configure and `champions-archive-extension` build: pass. To keep load down, unchanged wave-24 objects and the unchanged level-texture/creature-animation extension binaries came from the same pinned source commit; the changed controller object was compiled and the extension relinked in this worktree. Godot 4.5.2 editor import: pass, no script errors.
- arm64 CTest on the unchanged wave-24 native sources in the playthrough build: **10/10** (`player-state`, five native-save cases, `player-host`, `player-exact`, `quests`, `frontend-exact`), zero failures. Command: `ctest --test-dir build/native -R '^(native-save|native-snapshot|native-save-access|native-save-partition|native-save-indexed|player-state|player-host|player-exact|quests|frontend-exact)$' --output-on-failure -j 2`.
- Godot dev matrix: **53/53** after staging the unchanged wave-24 creature-animation binary. The first pass had 50/53 because that optional binary was absent from the new worktree; all three affected cases passed on the isolated retry, and the complete second pass was 53/53.
- Read-only real-data sweep with scripts, four sequential slices, 60 ticks per level: **111/111 loaded and played**, 230 passed checks, zero failures/refusals; 111 scripts attached, 76 started, zero faults. It used the existing level-light and effect-recipe measurement inputs outside the checkout.
- `FULL_TRACE`: **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, 36,402 bytes; matches wave 24.
- Scripted playthrough headless and rendered: pause's five original entries remained, F5 opened Save, the file and companions were written, quit reached the main menu, and Load restored the same level, HP, gold, quest summary and position. Fresh-scene cold load also passed. The overall harness still exits 1 because the other reported combat, dialog-drawing and level-travel steps are unfinished. Rendered pause, Saved, and loaded-level screenshots were inspected; the loaded level uses its textured source data.

## Remaining

This branch fixes only blockers #2 and #5. Enemy combat, dialog drawing, character appearance and level travel remain separate playthrough work. The native continuation carries collision and movement state; the client reloads level presentation from the user's data. World-object state and active script/dialog execution are not restored by this authored format.

## Commands

```sh
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --target champions-archive-extension --parallel 2
/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
ctest --test-dir build/native -R '^(native-save|native-snapshot|native-save-access|native-save-partition|native-save-indexed|player-state|player-host|player-exact|quests|frontend-exact)$' --output-on-failure -j 2
python3 /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration/work/playable-wave21/run-godot.py
python3 tools/run_loop_playthrough.py --godot /Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder /Users/syndicatemike/Projects/open-champions/private-validation/rta-disc --out work/play-save/playthrough
```

The real-data sweep ran `godot --headless --path godot --script res://tests/real_data_scene.gd` with `--all --levels=0 --ticks=60 --scripts`, the same read-only folder, effect/light measurement inputs, and four sequential `--from/--to` slices: 0–28, 28–56, 56–84, 84–111. Its per-slice logs and count summaries are ignored under `work/play-save/sweep/` and `work/play-save/sweep-summaries/`.
