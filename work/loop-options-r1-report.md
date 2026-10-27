STATUS: PASS — bounded options port; documented evidence limits remain.

# Options menu R1

Branch `codex/play-options`, base `7f6dbc2`. Contract: `t3/research-options-menu` at `603b2b7`, `docs/options-menu.md`, with the research-options-menu fix audit PASS. No retail bytes or decoded assets are tracked.

## Added

- Both observed option tables: 13 front-end rows and 14 pause rows, including the pause communications row with an Unknown destination. The controls action refuses the unverified sub-screen; the pause parent transition remains static evidence.
- Three 0..1 sliders using the executed 1/120 FPU step/timer, nine binary toggles, observed row anchors and frame calls. The disc frame textures and medium font load at run time. English words, slider bars and the state-14 prompt are visibly labelled Host.
- The documented 760-byte config image retains the controller, voice and profile regions; the twelve direct option words write at their researched offsets, including chat at +0xc4. Changed front-end options enter state 14: accept writes `user://frontend/options.img`; cancel commits only in memory. This is a host filename, not a claimed PS2 card filename.
- Music, effects and stereo reach the client's native mixer. Other values persist in local option state pending verified client application paths.

Changed: `native/include/champions/frontend.hpp`, `native/include/champions/frontend_layout.hpp`, `native/src/frontend.cpp`, `native/src/frontend_layout.cpp`, `native/tests/frontend_tests.cpp`, `godot/ui/frontend_menus.gd`, `godot/tests/frontend_smoke.gd`, `godot/tests/frontend_capture.gd`, `docs/FRONTEND.md`.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4` exposed a test helper declaration error, fixed locally. The resumed `cmake --build build/native -j 2` completed on arm64, including the large Release level-flow test translation unit.
- `cmake --build build/native --target champions-frontend-tests -j 1`; `ctest --test-dir build/native -R '^(frontend-exact|ps2-save|native-audio|music-volume-host|music-volume-exact|ui-layout-exact)$' --output-on-failure -j 1`: final focused set 6/6 passed on arm64.
- `ctest --test-dir build/native --output-on-failure -j 3`: 226/226 passed on arm64; 1,588.56 seconds under shared machine load, with no timeout retry needed.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR=<pinned local checkout> -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`; rebuilt the extension after the final source edits. Auxiliary Godot libraries were copied from the sibling `menus` checkout at the identical `7f6dbc2` base; source revision and copied hashes matched.
- `python3 -u work/options-matrix/run-godot.py`: final-code matrix 54/54 exit 0. The existing `scene_play_flow` fixture logged two script errors despite its zero exit; the front-end smoke passed 136 checks.
- `bash work/options-matrix/run-sweep.sh`: 111/111 attempted, loaded and played, 230 checks passed, zero top-level failures or refusal entries. Script diagnostics counted 14 refusal stops and 266 stub refusals; the sweep is not a full playthrough. Read-only owner disc; counts only.
- `frontend_capture.gd` with read-only game folder and output/cache under ignored `work/`: 24/24 captures. Inspected `05-options.png` and `19-pause-options.png` at 1280×960 after fixing bar paint order; both show all rows, disc frame/font art and three visible fills. The final capture asserts visible fill pixels on both lists. Images remain untracked.
- Synthetic Full combat: 2,620 checks, zero failures; `FULL_TRACE` 31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, matching the base. This is regression evidence, not full gameplay reachability.

## Remaining evidence limits

The controller configuration screen, executed pause parent transition and pad routing, pause communications destination, exact card config filename, localized text/value glyphs, brightness transfer curve, non-audio gameplay setters, and retail pixel parity remain unverified. The host labels or refuses these; it does not claim original behavior for them.
