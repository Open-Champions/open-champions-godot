STATUS: PARTIAL — the documented front-end art was already present at 2cf6668; the in-game stats and skills now use the disc's small font and undocumented item boxes are no longer painted. Full retail menu art remains refused where the research lacks resource identities or positions.

2026-10-22 coordination: implementation on `codex/menus` in the menus worktree only. Local commit only; no push or PR. No files in `~/champions-re` were changed.

## Evidence and scope

- `t3/research-menus-layout` at `29c90a5`: executed front-end title, logo, frame and list submissions. Its ART-1..3 corrections are included in this pinned head. The separate `t3/research-menus-art` branch/worktree was absent from local refs and the configured remote.
- `t3/research-menus` at `099900e`: in-game stats/skills use font word `0x4f7104` (small); inventory scrollbar strips are executed. Item info and pause frame origins, card rows and other sprite identities remain unknown.
- `t3/research-ui-text-gaps` at `523b3db`: the disc's font page and glyph path.
- The existing `godot/ui/frontend_view.gd` already loads documented title, logo, frame pieces and fonts from the user's disc at run time. Its main and pause captures were visually checked. No extracted bytes or rendered images are tracked.

## Changes

- `godot/ui/game_menus.gd`: load the small font from the selected game folder at run time; use it for the documented stats, level-up and skills text. Keep the existing medium-font host copy for other screens. Draw only the executed layer-4 scrollbar strips, leaving synthetic item hit boxes out of the canvas. Redraw only when a font becomes available.
- `godot/scenes/play_flow.gd`: give the in-game menu access to the selected real-data owner.
- `godot/tests/menu_capture.gd`: require the small font to load in the rendered disc capture; let the viewport container manage its size.

## Verification

- `ctest --test-dir build/native -R '^(ui_font|ui-layout-exact|ui-font-page-exact|ui-layout-dynamic-exact|menus-exact|frontend-exact)$' --output-on-failure -j 3`: **6/6 passed** on arm64 Release.
- `cmake --build build/level-texture-template_debug -j 4`: passed. The prior local binary was stale and produced a missing-method error during the first captures. The rebuilt, ignored local binary removed that error in the final captures and sweep.
- Godot dev matrix: `python3 work/menus-art-r1-final/run-godot.py` (results in `work/menus-art-r1-final/`): **54/54 exit 0** with the rebuilt extension. `scene_play_flow` still logs two script errors from its existing injected art fixture lacking a `loaded` property (`play_flow.gd:542`); that site was present at `2cf6668` and its test still exits 0. No other matrix log has a script error.
- Scripted disc sweep: `bash work/run-slices-r18.sh menus-art-r1b --scripts`: **111/111 tried, loaded and played; 230 checks passed; 0 failures**. The final four logs have no script errors.
- `FULL_TRACE` from the final matrix's `playable_wave_smoke_full_combat.gd -- --ready-ticks=640`: **31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**, matching the branch baseline.
- Graphical Godot captures from the user's disc: `frontend_capture.gd` produced **21/21** and `menu_capture.gd` produced **8/8** after the binary rebuild. Main, pause, save and in-game stats images were inspected under `work/private/menus-art-r1/`. They remain ignored; none was staged.

Capture commands (the path variables below are the exact paths used):

```sh
G=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot
D=/Users/syndicatemike/Projects/open-champions/private-validation/rta-disc
W=/Users/syndicatemike/Projects/open-champions/worktrees/menus/work/private/menus-art-r1
"$G" --path godot --script res://tests/frontend_capture.gd -- --game-folder="$D" --capture-dir="$W/frontend-final" --cache="$W/cache"
"$G" --path godot --script res://tests/menu_capture.gd -- --game-folder="$D" --capture-dir="$W/game-final2" --cache="$W/cache"
```

## Remaining refusal

The save screen is explicitly labelled **HOST LAYOUT** because the research does not establish real card row data or a fully identified cursor. Inventory icons and item-info frames, the pause frame origin, class art, background movie, and undocumented highlight positions/colours cannot be reconstructed without additional executed evidence. The captures prove the documented art and font paths render on this macOS development build; they do not establish retail pixel parity or other platform rendering.
