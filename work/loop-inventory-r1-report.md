STATUS: PARTIAL — the inventory audit fixes are implemented and verified for the researched first cell on each bag page. Further page-1/2 cell placement and live drop remain refused until their runtime contracts are supplied.

## Evidence and scope

- Branch `codex/play-inventory`, starting at `33fbf20`; research `t3/research-inventory-screen` at `fe9e910`.
- The prior research fixup INV-F1 found that the layer-5 draw's texture argument does not establish the underbar sprite, so it remains omitted. The subsequent `native-inventory-r1.md` audit found one HIGH and two MEDIUM issues addressed below.
- The selected read-only US v2.00 disc supplies inventory box art, small font, executable item/picture tables and picture-cache payloads at runtime. No game bytes or decoded images are tracked.

## Implementation

- The in-game Inventory action opens the inventory screen. Native frame data submits the observed 20-piece zero-state box sequence, 65 scrollbar strips, populated grid underlays, clipped item pictures, the reached category-dependent detail frame and small font text for the authored fixture. Imported item text remains hidden where its localized fields are unknown. The 640×480 inventory canvas scales to fit the in-game viewport. The UI loads assets from the selected disc and uses the existing native picture-name, icon decoder and font services.
- The menu uses the executed display-grid stride `page*0xbdd8` for hover and the separate `page*0x1208` counted-bag stride for the observed first-cell equip action. Pages 3–5 dispatch the stat sheet. Highlight tint gives equipped state precedence over blocked state.
- Importing a player record copies its 0x4c30 bytes into the menu machine and loads item definitions from the selected executable. Equip/unequip changes the native equipment pointer, and the Godot bridge writes the changed record back to the imported player. A no-disc built-in session keeps scalar menu state without claiming live item actions.
- A native test covers imported-record equip and unequip pointer changes; the rendered capture test covers box submission, underbar absence, hover image change, runtime icon association and imported-record unequip writeback.

## Audit fixes

- INV-R1-1: A copied player record does not contain the separate page-1/2 display grid. The frame now draws the researched first-cell bag record on each page, while the hover helper retains its separate `page*0xbdd8` address and the first-cell action uses `page*0x1208`. Additional page-1/2 cells remain explicitly unrendered with `inventory-page-grid` gap when counted; their display-grid state has not been supplied.
- INV-R1-2: The inventory frame submits both `inventry.lmp/mousecursor.tex` copies on layer 7 with the documented `0x80808080` and `0x34000000` tints. Godot paints them after lower-layer inventory content. The cursor resource loads in the rendered capture.
- INV-R1-3: A verified picture-cache miss alone uses the documented dagger placeholder. Missing index/data, failed lookup, truncated payload or decode/dimension failure paints a red `ICON REFUSED` label instead. The capture exercises a missing index and verifies a changed image.
- The native regression checks first-item underlay/detail frames on pages 1 and 2 and both cursor submissions. The rendered check captures both populated pages, verifies their associated picture loads, and captures the icon refusal. The underbar stays omitted and live drop still refuses before mutation.

### Audit verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j4`, and `cmake --build build/godot-extension -j2` passed on arm64. Focused `ctest --test-dir build/native -R '^(menus-exact|inventory-icon-assoc|messages-exact|player-host)$' --output-on-failure -j2` passed **4/4**. Full `ctest --test-dir build/native --output-on-failure -j3` passed **226/226** in 902.48 seconds.
- After updating the menu smoke's expected sprite count from 20 to 22, `python3 -u work/inventory-r1/run-godot.py` passed **54/54**. `FULL_TRACE` stayed **31,622 bytes**, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`.
- Rendered `menu_capture.gd` with the selected read-only disc passed **13 captures**, including first items on pages 1 and 2, the loaded cursor sprites, and the visible icon refusal. The final page and refusal images were inspected. They are host renders, not original-game pixel comparisons.
- `bash work/inventory-r1/run-slices.sh inventory-audit-final --scripts` completed four successful slices: **111/111** tried, loaded and played; **230** checks; **0** failures, refusals and script faults. These aggregate counts match `inventory-r1-final`.

## Verification

- Release CMake configure and full native build passed. The tested binaries and extension are arm64. Commands: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j3`; `cmake -S gdextension -B build/godot-extension -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=<local godot-cpp> -DGODOTCPP_TARGET=template_debug`; `cmake --build build/godot-extension -j2`.
- Focused `ctest --test-dir build/native -R '^(menus-exact|inventory-icon-assoc|messages-exact|player-host)$' --output-on-failure -j2`: 4/4 passed after the final native frame change.
- Godot menu smoke: 44 checks, 0 failures. Front-end smoke: 124 checks, 0 failures. Rendered `menu_capture.gd`: 10 captures; inventory base, hover and icon images were inspected under ignored `work/inventory-r1/screens/`. Command: `$GODOT --path godot --script res://tests/menu_capture.gd -- --game-folder="$DISC" --capture-dir="$OUT/screens" --cache="$OUT/cache"`, where `$DISC` is the selected read-only disc and `$OUT` is ignored `work/inventory-r1`.
- Scripted disc sweep: `bash work/inventory-r1/run-slices.sh inventory-r1-final --scripts` → four successful slices, 111/111 tried, loaded and played; 230 checks passed, 0 failures, 0 refusals and 0 script faults.
- Godot dev matrix: `python3 -u work/inventory-r1/run-godot.py` → 54/54 exit 0. The `scene_play_flow` fixture still logs its two pre-existing missing-`loaded` script errors; the `ui_layout_smoke` negative API call logs one expected argument error. `FULL_TRACE` is 31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, matching the baseline.
- Full `ctest --test-dir build/native --output-on-failure -j3`: 226/226 passed in 1,151.65 seconds, including `player-exact` and `netplay`. This run began before the final frame addition; the focused 4/4 check above was rerun after it.

## Refusals and limits

- INV-F1 underbar submission, general cosmetic box-variant formula, localized item names and modifier/requirement fields, running-frame selected icon pulse, the second item-detail frame path, stat-point input gate, and retail pixel parity remain unverified. Live drop recognizes the documented input but refuses before mutation until world placement and item prop construction are connected. Page-1/2 non-first-cell actions are refused because only the first-cell sender was executed.
- The rendered capture uses an authored synthetic imported slot and verifies native resource loading. It is not an original-game pixel comparison.
