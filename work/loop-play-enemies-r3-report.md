STATUS: BLOCKED — the `valor` playthrough loads, but the 78 WarBoar records still cannot become live combat actors; step 4 does not pass.

# Play enemies implementation round 3 — 2026-10-22

Local branch `codex/play-enemies`. Merged wave 25 head `6ba8afe` as `f7e561d` before editing. Research: `t3/research-campaign-combat` at `e4c4f40`, `docs/issues/campaign-combat-handoff.md` rounds 2–4, with the round-4 audit PASS. Round-3 R3-1 was corrected at `977a3f2`: valid model/class pairs and zeroed debug switches establish bounded starting-grant requests, but parser, inventory and equip answers remain unknown. RTA US v2.00 private inputs were used read only, and this report contains counts only.

## Change and boundary

- The factory now names the WarBoar resource/shared-combat refusal for its pinned constructor `0x2ba03c`. It keeps factory allocation and file order and leaves these records unbuilt. The playthrough exposes the count of unbuilt WarBoar and Minotaur records, the created player's model/class/import status, the item-load gate, and unmodelled constructor draws alongside its existing fight outcomes.
- Wave 25 brought selected balance table parsing, level scale and monster stat copying into this branch. The existing isolated native combat code has damage and Gold countdown/touch/HUD credit bodies. Those parts are not wired to factory objects in the client; no fixture actor, weapon value, animation answer or Gold source was substituted.
- Round 4 now traces boar clip relocation and the configured attack's event masks (1 at loop index 18, 2 at 42) and a Gold-capable Minotaur death call. Its seed-20973 Gold spawn request is a **synthetic-seed** result, not a campaign drop. The handoff still leaves the native resource manager, hide mask and resource-backed tick, shared script-host/combat identity and restore, actual equipped item, and a live Gold pickup/credit unestablished. The WarBoar class drop call has zero Gold chance. These missing service answers prevent a faithful constructor, tick and fight. The original handoff's portable contracts cannot be claimed as a live port at this boundary.

## Observed result

Headless and rendered strict step 4 both entered `valor` and reported **16 built / 175 unbuilt** level objects, of which **78 unbuilt** take the WarBoar constructor and **6 unbuilt** take the Minotaur constructor. The created player record reports model 0, class 0, imported false; it supplies no equipped item. The factory recorded 78 WarBoar service refusals, one Player1 faithful-seed refusal, one item-load gate refusal and 175 unmodelled constructor draws. Both had zero live enemies, J unhandled, no damage, death or drop, and gold 500→500; step 4 remains `blocked_unported_combat`. The rendered fight capture shows terrain and the practice player marker, with no enemy actor. Both variants wrote 22 steps; the rendered run wrote 11 screenshots. The read-only disc file count stayed 994 before and after. The later load/save probe separately refuses the scene's terrain surface limit.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`: **PASS**, arm64 Release.
- `ctest --test-dir build/native --output-on-failure -j 3 -R '^(level-objects|script-commands|combat-runtime-2-host|play-session-runtime)$'`: **4/4 PASS** on arm64.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`; `cmake --build build/godot-extension --parallel 4`: **PASS**.
- `python3 /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration/work/playable-wave25/run-godot.py`: **54/54 PASS**. Its isolated Full fixture reports `FULL_TRACE` 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from wave 25; this is not a campaign fight.
- `bash work/play-enemies/run-sweep.sh`: **111/111 tried, loaded and played**, 230 checks passed, zero failures and zero refusal kinds; four sequential slices, 60 ready ticks per level.
- `ctest --test-dir build/native --output-on-failure -j 3`: **233/233 PASS** on arm64 Release (733.25 s).
- `python3 tools/run_loop_playthrough.py --godot /Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder /Users/syndicatemike/Projects/open-champions/private-validation/rta-disc`: headless and rendered both exit 1 for incomplete playthroughs, with results above. `git diff --check` and `git diff --cached --check`: **PASS**.

No push or PR.
