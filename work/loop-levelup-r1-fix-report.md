STATUS: MORE — NLU-7/NLU-8 fixed; Release build, targeted tests, Godot matrix and sweep pass. Full CTest is running. The requested FULL_TRACE baseline does not match this branch's measured/documented baseline. Earlier results below are historical.

## NLU-7 / NLU-8 follow-up (2026-10-25)

- NLU-7: companion version 4 saves/restores the actual `R+0xee` sender-half byte. Versions 1–3 remain readable but close a restored level-up page because they lack that byte. Native coverage saves an open stats page, exhausts every point through `press_stat`, restores, inspects the half byte, and compares actual point/stat changes with a fresh-ledger restore. Saved exhausted skills halves stay blocked; a legacy version-3 level-up page closes.
- NLU-8: native adoption, live-record adoption and companion restoration reject earned levels outside 1–80 before mutation when XP rules are active. Installing XP rules validates the current earned level before enabling them; XP awards validate it before changing XP. Refusal assertions inspect full record/snapshot equality, imported-record ownership, usable threshold/state reads, and refused service adoption/restoration at level 81.
- Evidence: `research-levelup@576b41c:docs/spec/levelup.md:28` (81-entry table and level domain), `docs/spec/levelup.md:45` (sender gate), `tools/ref_messages.py:1120` (exhaustion switches half), `tools/ref_combat.py:1715` (XP/recalculation order). Companion versioning is authored, not a retail format claim.
- Files changed for these findings: `native/src/menus.cpp`, `native/tests/menus_tests.cpp`, `godot/tests/menu_smoke.gd`, this report. No push, PR, process-name termination or private-data changes.
- PASS: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`; `build/native/champions-menus-tests` — **250 checks**. `git diff --check` passes.
- PASS: `cmake --build build/godot-extension -j 4`; `python3 -u work/levelup-r1/run-godot.py` — **54/54 commands**, **177.77 seconds**, menu smoke **55 checks / zero failures**. This reruns the prior fix-2 Godot gate.
- PASS: `bash work/levelup-r1/run-slices.sh levelup-r1-fix3 --scripts` — **4/4 slices exit 0**, **111 tried / 111 loaded / 111 played**, **230 checks / zero failures**. Private input was read-only; only aggregate counts are reported. This reruns the prior fix-2 sweep gate.
- TRACE MISMATCH: matrix full-combat execution measures **31,622 bytes**, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, matching the historical report and `docs/INTEGRATION.md`, but not the requested **36,402 bytes / `9c7922b2…c659`**. No full requested hash or reproducing command was supplied; trace generation and unrelated combat code remain unchanged.
- RUNNING: `ctest --test-dir build/native --output-on-failure -j 3`, logged locally in `work/levelup-r1/fix3-ctest.log`. Final figures will replace this entry. This reruns the prior fix-2 full native gate.

## NLU-6 follow-up (2026-10-25)

- Companion version 3 snapshots current/max mana, class, stagger and load capacity, plus the owned authored object's armour, five resistances and four bonus bytes. Load restores these fields directly without recalculation, refill or resource clamping. HP retains the existing exact snapshot restore.
- Version 1/2 remain readable but cannot authorize XP/stat recalculation because their missing dependent state cannot be reconstructed exactly. Missing/unsupported linked-object snapshots and existing equipment/effective-stat invalidation likewise remain mutation refusals; no object state is invented.
- Native assertions cover level 6 (maximum mana 17), level 7 (18), then load back to 17; save/change/load and fresh-ledger restores inspect actual record/object fields, depleted current resources, capacity, stagger, armour, resistances and bonus bytes. Both legacy formats and a missing-object snapshot refuse later XP/stat reconstruction.
- Evidence: `research-levelup@576b41c:tools/ref_combat.py:1715` and `research-levelup@576b41c:tools/ref_combat.py:1843` (recalculation/refill boundaries and dependent writes), and `research-ui@3e39392:docs/hud-layout.md:165` (record mana source), as cited by NLU-6 in `native-levelup-r1-fix.md`. This is an authored companion format, not a retail-save format change.
- Changed files: `native/src/menus.cpp`, `native/tests/menus_tests.cpp`, this report. No other audit finding is changed. Local commits only; never pushed.
- Executed: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`; `build/native/champions-menus-tests` — all pass, **221 menu checks**. Broader gates follow below when complete.

## Audit fixes

This report supersedes the r1 report at `bbeb226` after the `native-levelup-r1.md` FIX audit.

- NLU-1: The HUD gets a separate XP interval indexed by acknowledged level `R+0x58`. Native and Godot tests cover a pending level before and after acknowledgement. Earned-level thresholds still govern awards.
- NLU-2: XP awards and stat spends call the researched recalculation for the supported authored object path, including effective stats, bonus bytes, armour, load capacity and resources. A detached record takes the documented effective-stat-only branch. Linked imported objects, equipment/figurine bonuses and unsupported object state refuse before spending points or awarding XP.
- NLU-3: Companion version 2 preserves effective stats, offsets, object status and invalidation. Version 1 is loaded with recalculation invalid. An equipped-character round trip and an equipment-change round trip are tested.
- NLU-4: Only an explicitly identified imported dry session reads ledger HP/mana/XP for its HUD. A combat fixture retains its active observation; the Godot smoke checks bar fills with a coexisting imported ledger.
- NLU-5: XP, kill XP and stat spending roll back native memory on refusal. Native tests compare the entire player record before and after refused XP/stat transitions.
- A refused HUD layout clears the prior painter order. A rendered level-up capture exercises the selected executable's table with an authored synthetic detached record, then spends a stat under the documented gate.

## Evidence and boundaries

The implementation follows `t3/research-levelup@576b41c` (`docs/spec/levelup.md`, `docs/spec/combat.md`, `tools/ref_combat.py`) and the PASS research audits, plus the acknowledged-level HUD interval in `t3/research-ui@3e39392` (`docs/hud-layout.md`). The selected executable provides its XP table and class formulas at runtime. Synthetic tests use authored inputs. No private game bytes, proprietary names or decoded content are tracked. The private disc was read-only and is reported through counts only. No push or PR.

Live campaign enemies still need to call the exposed kill-XP API. Skill spending remains refused because the skill-screen sender conditions are Unknown. A linked imported character with an absent player-object snapshot remains viewable, but XP and stat mutation refuse until that state can be recalculated. The capture covers the documented detached branch and development-client rendering, not a retail gameplay playthrough.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j4`: passed on arm64. `build/native/champions-menus-tests`: **185 checks passed**.
- `ctest --test-dir build/native --output-on-failure -j3`: **224/226 passed** in 3,227.69 seconds. The two failures were timeouts under heavy concurrent load, not assertion mismatches. `session-facing-exact` passed alone via `ctest --test-dir build/native -R '^session-facing-exact$' --output-on-failure -j1` in 530.71 seconds.
- `player-exact` timed out at its fixed 240-second CTest property both in the full run and alone. The exact pinned vector command passed **274,304 cases** under a 1,200-second outer allowance. The same test passed via isolated CTest in 325.22 seconds after temporarily raising only its generated build-file timeout to 900 seconds; the original 240-second generated setting was restored immediately afterward. The default parallel CTest invocation therefore remains 224/226, while all 226 test bodies have passed across the full run and isolated retries.
- `cmake --build build/godot-extension -j2`: passed. `python3 -u work/levelup-r1/run-godot.py`: **54/54 commands passed** in 274.9 seconds; menu smoke **51 checks, zero failures** and play HUD smoke passed.
- `bash work/levelup-r1/run-slices.sh levelup-r1-audit2 --scripts`: all four slices exited 0; **111/111 levels tried, loaded and played; 230 checks passed; zero failures**. Only aggregate counts were read from the private sweep summaries.
- The combined full-combat Godot command reported `FULL_TRACE` **31,622 bytes**, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, matching the prior branch baseline.
- `levelup_capture.gd` with the selected read-only game folder and an authored synthetic detached slot: **one threshold transition, one stat spend, image save OK**. The rendered stats page was visually inspected at `work/levelup-r1/audit-levelup-stats.png`; the image remains ignored.

Additional timeout and capture commands (the local engine and selected disc paths were supplied as `$GODOT` and `$DISC`; the latter is intentionally omitted from tracked evidence):

```sh
ctest --test-dir build/native -R '^player-exact$' --output-on-failure -j1
timeout 1200 /opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz
python3 work/levelup-r1/run-player-extended.py  # ignored local helper: generated CTest timeout 240 -> 900 -> 240
"$GODOT" --path godot --script res://tests/levelup_capture.gd -- --game-folder="$DISC" --capture="$PWD/work/levelup-r1/audit-levelup-stats.png" --cache="$PWD/work/levelup-r1/audit-capture-cache"
```

Changed files: `native/include/champions/menus.hpp`, `native/src/menus.cpp`, `native/tests/menus_tests.cpp`, `gdextension/src/menu_service.cpp`, `godot/scenes/play_flow.gd`, `godot/ui/original_art/hud_view.gd`, `godot/tests/levelup_capture.gd`, `godot/tests/menu_smoke.gd`, `godot/tests/play_hud_smoke.gd`, and this report.
