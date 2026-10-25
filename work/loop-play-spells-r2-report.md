STATUS: PARTIAL — Disease Bolt ranks 1–20 cast in the authored Full combat fixture with researched float mana costs and outgoing damage stats. Projectile deletion and the other class spell routes remain unported.

# Player spells, implementation round 2 — 2026-10-24

## Evidence and changes

Branch `codex/play-spells` continues from `947c002` without rewriting history. The implementation uses audited `t3/research-spells` `46b340a`, `docs/spec/player-spell-pipeline.md`, and the PASS read-only audit `cursor-runs/audits/research-spells-r2.md`. That research executes original slot-5 and player-tier stat getters for ranks 1–20 and separately exercises Disease Bolt's projectile and damage call. It does **not** establish original whole-session selection-to-HP parity.

The opt-in Full fixture now accepts Disease Bolt ranks 1–20. The assigned record carries the requested rank through `Player_run` admission, animation and cast-helper path. Slot 5 uses the documented float32 step and the PS2 arithmetic model, preserving the executed original mana bits at ranks 1, 2, 5 and 20. The projectile carries the rank to stat 0 and the existing `damage_apply` bridge. The Godot controller accepts an optional rank; the fixture journal records and replays it. Older zero-valued journal entries still mean rank 1. Invalid ranks refuse before journaling. Changed files are the combat runtime/play and fixture-save headers/sources, GDExtension session controller, native spell/save tests, and Godot spell smoke.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 4`: passed on arm64. The GDExtension build passed.
- `ctest --test-dir build/native -R '^(play-spells|fixture-save)$' --output-on-failure`: 2/2 passed. The spell test checks exact slot-5 mana debit bits and outgoing stat-0 damage at ranks 1, 2, 5 and 20, plus fixture spawn/hit/HP loss. The save test checks rank-2 replay and rejection of rank zero without changing the save.
- `godot/tests/play_spells_smoke.gd` through the local Godot runtime: passed rank-1 and rank-2 selection, mana debit, spawn and hit. `python3 work/play-spells-matrix-r5/run-godot.py`: 54/54 development checks passed.
- `ctest --test-dir build/native --output-on-failure -j 3`: 232/234 passed; `player-exact` exceeded its 240-second CTest timeout and `session-facing-exact` exceeded its internal 600-second timeout during the loaded aggregate run. `ctest --test-dir build/native -R '^player-exact$' --output-on-failure -j 1`: 1/1 passed in 167.25 seconds. `ctest --test-dir build/native -R '^session-facing-exact$' --output-on-failure -j 1`: 1/1 passed in 203.56 seconds. All 234 native cases therefore passed either in the aggregate or alone; the aggregate itself did not pass without timeouts.
- `bash work/run-slices-r21.sh spells-r2-on --scripts` and `bash work/run-slices-r21.sh spells-r2-off`: each completed four slices at 111 tried / 111 loaded / 111 played, 230 passed checks, zero failures. Scripts-on recorded 163 internal stub refusals, 15,472 unbuilt objects, and seven refusal stops; scripts-off does not report those script-mode counts. The private disc had 994 files before and after; only counts were used or reported.
- `FULL_TRACE` stayed 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from round 1 because ranked selection is opt-in after the established scenario.

## Remaining gates

- Disease Bolt run sets expiry to zero for ground contact and for object contact after its hit latch. Its audited run contract calls deletion at expiry, but the audited research does not document the skill-specific `DiseaseBolt_dtor` body at 0x3ccfd4. The native path refuses deletion rather than inventing handle cleanup or pool behavior. Survival at +299, deletion at +300, and both collision-end cases are **unverified in the native fixture**.
- The audited class matrix maps 75 castable entries across seven trees, with final cross-function routes labelled Inferred. This change does not compose their selection, `canUse`, cost, cast, object run/end and damage paths into the Full runtime. One cast per class is **not verified**; only Disease Bolt's Shadowknight skill ID 4 is exercised here. The fixture assignment is synthetic and does not establish tree level or prerequisite eligibility.
- The fixture's HP loss is native integration evidence. Original whole-session selection-to-HP parity, live campaign casts and campaign enemy hits remain **unverified**; this branch has no live campaign hostile for that acceptance gate. Exact final victim HP depends on the separate damage pipeline and defender state.
