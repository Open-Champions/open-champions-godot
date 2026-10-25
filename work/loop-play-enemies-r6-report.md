STATUS: PARTIAL — class-6/model-11 creation and the r5 death-gate correction are verified; the native/Godot client still has no live Valor combat actor, handled J, damage, death or Gold credit.

# Play enemies implementation round 6 — 2026-10-23

Branch `codex/play-enemies`, starting at `9b04e61`; research pin `t3/research-campaign-combat` `328b072`. The owner's selected RTA US v2.00 disc was read-only. No game bytes or decoded content were added to the repository.

## Implemented

- The ordinary playthrough now selects model 11 from the existing character-creation screen. The documented model mapping yields class 6. The created record is checked and reported at the character step, and the Valor step reports the same class/model pair. No club or attack statistic was copied from the assisted emulator capture. The native player record has no equipped-item record on this route, so the capture's 38 attack and 19–24 damage are still unestablished for this client session.
- The step-4 death gate now matches an actor by handle and requires that same actor's positive prior HP to be nonpositive and its dead state observed afterward. It does not accept a missing or replacement actor.

## Audit fixes (r5)

- PE5-1: the r5 audit found that `death_drop_observed` had no observation producer. The gate uses the audit's bounded same-handle HP/dead alternative. A real death/drop event cannot be emitted until the ordinary play session owns and ticks real actors; no such event is synthesized from a fixture or an absent actor. Drop production and AutoContainer Gold remain separate acceptance checks.

## Remaining implementation boundary

The ordinary session still creates a separate `DryCreature` controller. Its level factory resolves all 46 named WarBoar/Minotaur resources, then refuses the class tick/shared actor runtime. The factory World and `combat_play` Full fixture World have separate owners. The current route does not bind the ported Player tick, equipped item, WarBoar/Minotaur constructors and run functions, target grid, damage, loot, AutoContainer or HUD credit into one World. The handoff establishes individual calls and an assisted kill, but no complete normal-route values for this client's equipment, target/animation services, or Gold source. A constructor shell or fixture combat result cannot count as this fight.

## Valor playthrough and visual inspection

The post-change headless playthrough entered Valor (level index 99) with class 6/model 11. Step 4 reported 16 objects built overall, 175 unbuilt, 78 unbuilt boar-family records, six unbuilt Minotaurs, no actor observations, unhandled J, and Gold 500→500. The run exits 1 because the journey is incomplete. The rendered run wrote 11 screenshots and had the same combat outcome. I inspected `05-fight-attempt.png`: the class-6 model and terrain are visible, with no boar. The read-only disc file count was 994 before and after.

## Verification

- `git diff --check`: PASS.
- `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/loop-play-enemies-r6/creation-probe`: expected exit 1; headless and rendered each wrote 24 steps; no fight. The later direct headless rerun after the death-gate edit had the same scoped step-4 outcome.
- `ctest --test-dir build/native --output-on-failure -j 3 -R '^(resource-manager|player-state|level-objects|script-commands|play-session-boundary|play-session-runtime|frontend-exact)$'`: 7/7 PASS on the existing Release arm64 build. `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'`: 1/1 PASS in 148.61 seconds. An optional broader `-j 3 -E '^player-exact$'` run was interrupted after four passing cases because other concurrent full-suite jobs made it heavily contended; it is not reported as a full-suite pass.
- `python3 /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration/work/playable-wave25/run-godot.py`: Godot 4.5.2 dev matrix 54/54 exit 0. Its synthetic `FULL_TRACE` stayed 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`; this is a fixture regression check, not campaign-combat proof.
- `bash work/play-enemies/run-sweep.sh`: 111/111 levels tried, loaded and played; 230 checks passed, zero failures or summary refusal maps. Seven script entries stopped on refused answers. This sweep covers level loading and bounded session behavior, not a real fight.

No push or PR.
