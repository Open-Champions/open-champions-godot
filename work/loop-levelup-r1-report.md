STATUS: PASS — XP, level-up and stat spending are implemented and verified within the documented native/Godot client boundary.

## Evidence and scope

- Implemented from `t3/research-levelup` at `576b41c`, including the PASS audits in `research-levelup.md` and `research-levelup-fix.md`. The selected US v2.00 executable supplies the 81 XP thresholds and class formula words at run time. Tests use authored synthetic thresholds and inputs.
- This branch starts at `ed433fc` and changes only the `play-levelup` worktree. No private game bytes, decoded content or capture images are tracked. No push or PR.

## Implementation

- The native player ledger now grants XP through the documented threshold loop, including the old-level skill-point bands, three stat points per level, class HP/mana recalculation, alive-only refill, level-80 cap and separate acknowledged level. It exposes the documented solo kill-XP arithmetic and class-0 mana recovery for a future live combat caller. The existing combat model remains the owner of multiplayer sharing.
- The level-up stats page sends rows 0–3 only under the documented screen argument, edge mask, panel gate, timing and stats-half conditions. The message handler updates the imported player record and recalculates resources. Equipment mutations refuse further XP or stat changes until effective stats are recalculated from the player service.
- Godot shows live level, XP, HP/mana, stats and point balances and submits the documented XP bar. Skill spending remains refused and labelled because the skill-screen sender conditions are Unknown.

Files changed: `native/include/champions/menus.hpp`, `native/src/menus.cpp`, `native/tests/menus_tests.cpp`, `gdextension/src/menu_service.cpp`, `godot/ui/game_menus.gd`, `godot/scenes/play_flow.gd`, `godot/ui/original_art/{catalogue,hud_view}.gd`, `godot/ui/play_hud.gd`, `godot/tests/{menu_smoke,play_hud_smoke,levelup_capture}.gd`, and this report.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 2`: passed on arm64. `cmake --build build/native --target champions-menus-tests -j 1`: passed after the last native test edit.
- `ctest --test-dir build/native --output-on-failure -j 3`: **226/226 passed** against the full arm64 Release build in 1,340.91 seconds. No timeout rerun was needed.
- `ctest --test-dir build/native -R '^(menus-exact|combat-runtime-exact|combat-runtime-2-exact)$' --output-on-failure -j 1`: **3/3 passed** (144.79 seconds). After adding a kill-cap assertion, `ctest --test-dir build/native -R '^menus-exact$' --output-on-failure -j 1` passed again; `champions-menus-tests` reports **167 checks**.
- `cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<local godot-cpp> -DGODOTCPP_TARGET=template_debug` and `cmake --build build/godot-extension -j 2`: passed. Focused `menu_smoke.gd` after final extension rebuild: **47 checks, zero failures**.
- Final Godot dev matrix: `python3 -u work/levelup-r1/run-godot.py` → **54/54 exit 0** after the final extension rebuild. Combined full combat passed **2,620 checks**, zero failures; `FULL_TRACE` was **31,622 bytes**, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, matching the baseline.
- Scripted disc sweep: `bash work/levelup-r1/run-slices.sh levelup-r1-final --scripts` → **111/111 tried, loaded and played; 230 checks passed; zero failures**. The four slices exited 0. Private inputs were read-only; only counts are reported here.
- Rendered capture: `levelup_capture.gd` with the selected disc and authored synthetic slot passed the level-up, HUD draw, record writeback and stat-spend interaction checks. The image was inspected at `work/levelup-r1/stats-after-levelup.png` and remains ignored.

The focused Godot commands used `$GODOT` for the local engine, `$DISC` for the selected read-only game folder and `$OUT` for ignored `work/levelup-r1`:

```sh
"$GODOT" --headless --path godot --script res://tests/menu_smoke.gd
"$GODOT" --path godot --script res://tests/levelup_capture.gd -- --game-folder="$DISC" --cache="$OUT/capture-cache" --capture="$OUT/stats-after-levelup.png"
```

## Remaining boundary

Live campaign enemies are not yet wired to the kill-XP API. The skill-screen sender remains Unknown and skill spending is refused. The rendered capture proves this development client path, not retail pixel parity.
