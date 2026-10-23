STATUS: PARTIAL — MENU-1, MENU-2 and MENU-3 audit fixes are complete and verified. The documented front-end art was already present at 2cf6668; in-game stats and skills use the disc's small font. Full retail menu art remains refused where the research lacks resource identities, positions, colours or fonts.

2026-10-22 coordination: implementation on `codex/menus` in the menus worktree only. Local commit only; no push or PR. No files in `~/champions-re` were changed.

## Evidence and scope

- `t3/research-menus-layout` at `29c90a5`: executed front-end title, logo, frame and list submissions. Its ART-1..3 corrections are included in this pinned head. The separate `t3/research-menus-art` branch/worktree was absent from local refs and the configured remote.
- `t3/research-menus` at `099900e`: in-game stats/skills use font word `0x4f7104` (small); inventory scrollbar strips are executed. Item info and pause frame origins, card rows and other sprite identities remain unknown.
- `t3/research-ui-text-gaps` at `523b3db`: the disc's font page and glyph path.
- The existing `godot/ui/frontend_view.gd` already loads documented title, logo, frame pieces and fonts from the user's disc at run time. Its main and pause captures were visually checked. No extracted bytes or rendered images are tracked.

## Changes

- `godot/ui/game_menus.gd`: load the small font from the selected game folder at run time; use it for the documented stats, level-up and skills text. The first implementation kept medium-font host copy for other screens and painted the layer-4 scrollbar strips; see Audit fixes below. Redraw only when a font becomes available.
- `godot/scenes/play_flow.gd`: give the in-game menu access to the selected real-data owner.
- `godot/tests/menu_capture.gd`: require the small font to load in the rendered disc capture; let the viewport container manage its size.

## Verification

- `ctest --test-dir build/native -R '^(ui_font|ui-layout-exact|ui-font-page-exact|ui-layout-dynamic-exact|menus-exact|frontend-exact)$' --output-on-failure -j 3`: **6/6 passed** on arm64 Release.
- `cmake --build build/level-texture-template_debug -j 4`: passed. The prior local binary was stale and produced a missing-method error during the first captures. The rebuilt, ignored local binary removed that error in the final captures and sweep.
- Godot dev matrix: `python3 work/menus-art-r1-final/run-godot.py` (results in `work/menus-art-r1-final/`): **54/54 exit 0** with the rebuilt extension. `scene_play_flow` still logs two script errors from its existing injected art fixture lacking a `loaded` property (`play_flow.gd:542`); that site was present at `2cf6668` and its test still exits 0. No other matrix log has a script error.
- Scripted disc sweep: `bash work/run-slices-r18.sh menus-art-r1b --scripts`: **111/111 tried, loaded and played; 230 checks passed; 0 failures**. The final four logs have no script errors.
- `FULL_TRACE` from the final matrix's `playable_wave_smoke_full_combat.gd -- --ready-ticks=640`: **31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**, matching the branch baseline.
- Graphical Godot captures from the user's disc: `frontend_capture.gd` produced **21/21** and `menu_capture.gd` produced **8/8** after the binary rebuild. Main, pause, save and in-game stats images were inspected under `$OUT`. They remain ignored; none was staged.

Capture commands (set `$GODOT` to the local Godot executable, `$DISC` to the read-only game folder, and `$OUT` to an ignored capture directory):

```sh
"$GODOT" --path godot --script res://tests/frontend_capture.gd -- --game-folder="$DISC" --capture-dir="$OUT/frontend" --cache="$OUT/cache"
"$GODOT" --path godot --script res://tests/menu_capture.gd -- --game-folder="$DISC" --capture-dir="$OUT/game" --cache="$OUT/cache"
```

## Audit fixes

- **MENU-1:** `research-menus/docs/menus.md:112-117` establishes the two scrollbar strip positions and layer 4, but records no `ui_drawRect` colour. The `0x80808080` in `native/src/menus.cpp` is not colour evidence for those strips. Godot now leaves all native menu rects unpainted until their visual contracts are traced. The geometry and layer remain available in the native frame data; the item hit targets remain unpainted.
- **MENU-2:** Replaced private absolute paths in this report with `$GODOT`, `$DISC` and `$OUT` placeholders. The current capture outputs remain ignored and untracked.
- **MENU-3:** Renamed `configure(owner)` to `configure(play_flow)` so it does not shadow `Node.owner`. Godot now draws in-game text only for stats, level-up and skills, whose small font is traced. Inventory item names and save `index N` remain in native state but are not painted because their font is not traced. The host panel says so.

Audit-fix verification (arm64 Release, this worktree):

- Relevant CTest: `ctest --test-dir build/native -R '^(ui_font|ui-layout-exact|ui-font-page-exact|ui-layout-dynamic-exact|menus-exact|frontend-exact)$' --output-on-failure -j 3` → **6/6 passed**.
- Full CTest: `ctest --test-dir build/native --output-on-failure -j 3` → **226/226 passed** in 1,007.52 seconds; `player-exact` passed on the first run. The test binaries are arm64.
- Godot dev matrix: `python3 -u work/menus-art-audit/run-godot.py` → **54/54 exit 0**. `menu_smoke` passed 36 checks and `frontend_smoke` passed 124 checks. The existing `scene_play_flow` fixture still logs script errors, as noted above.
- Combined Full combat: **2,620 checks, zero failures**; `FULL_TRACE` is **31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**, unchanged.
- Scripted disc sweep: `bash work/menus-r3/run-slices.sh menus-art-audit --scripts` with the existing authored recipe and lighting inputs → four successful slices; **111/111 tried, loaded and played; 230 checks passed; zero refusals**. The earlier round's sweep script pointed at missing local fixture files and logged a JSON parse error despite its 111/111 count; this run uses the available fixture paths.
- Rendered menu screenshot checks: `frontend_capture.gd` → **21/21**; `menu_capture.gd` → **8/8**. Inspected main, pause, save, inventory, stats and skills images under `$OUT`. Inventory and save no longer paint text with an untraced font; stats and skills still render with the loaded small font. Captures are not pixel-parity proof.

## Remaining refusal

The save screen is explicitly labelled **HOST LAYOUT** because the research does not establish real card row data or a fully identified cursor. Inventory icons and item-info frames, the pause frame origin, class art, background movie, and undocumented highlight positions/colours cannot be reconstructed without additional executed evidence. The captures prove the documented art and font paths render on this macOS development build; they do not establish retail pixel parity or other platform rendering.
