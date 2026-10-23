STATUS: COMPLETE — bounded native actor port, focused arm64 tests, Godot matrix, 111-level sweep and FULL_TRACE verified; one broad CTest timeout passed on a direct rerun.

## Scope and evidence

- Worktree `port-warboar`, branch `codex/port-warboar`, base `1cfddf0`. Implementation commits: `14c55f4`, `810d8fd` (constructor refusal cleanup).
- Evidence: `research-campaign-combat` at `328b072`, `docs/issues/campaign-combat-handoff.md` rounds 2–8, and `tools/ref_campaign_combat_r2.py`, `r4.py`, `r5.py`. All fixtures in the new tests are authored synthetic inputs.
- Added standalone C++17 `WarBoar`, `WarBoarSmall`, `Minotaur` actor types and a World-owned runtime. It resolves the documented 20/26 definition slots by name through `resources::Manager`, preserves one grid/run-list identity, writes documented boar fields and stat/HP offsets, dispatches the bounded boar tick, applies `Creature_takeDamage`, and forwards the researched death and loot-generator parameters. The seeded generator tests use `items::loot_generate_drops` with synthetic tables.
- No play-scene or session World wiring was changed.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: passed.
- `cmake --build build/native --target champions-campaign-actors-tests -j 2` and `ctest --test-dir build/native --output-on-failure -R '^campaign-actors$'`: passed 1/1 after final actor changes, including cleanup of a World object when a supplied service refuses.
- Arm64 Release `cmake --build build/native -j 4`: passed. An unrelated `level-flow-tests` translation unit took about 24 minutes to compile under shared machine load.
- Focused arm64 CTest: `ctest --test-dir build/native --output-on-failure -j 2 -R '^(campaign-actors|resource-manager|items-exact|item-generation-exact|combat-stats-exact|objects-runtime-(host|exact)|creature-hp-(host|exact))$'`: **9/9 passed**. `ctest --test-dir build/native --output-on-failure -j 1 -R '^level-flow-exact$' --timeout 900`: **1/1 passed**.
- Broader `ctest --test-dir build/native --output-on-failure -j 3 -E '^level-flow-exact$'`: **235/236 passed** in 3,108.46 seconds. The sole issue was `player-exact` hitting its 240-second CTest timeout under shared load. The isolated CTest retry with `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$' --timeout 900` also stopped at 240 seconds because the test's own TIMEOUT property takes precedence. `timeout 900 /opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz` then **passed**, reporting **274,304 exact complete-state cases and transport refusals**. The first aggregate timeout remains recorded; no aggregate 236/236 pass is claimed.
- Arm64 Godot debug extension builds: `cmake -S gdextension -B build/gdextension-debug -DCMAKE_BUILD_TYPE=Release -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64`, then `cmake --build build/gdextension-debug -j 4`; the same configure/build pair for `gdextension/level_texture` → `build/level-texture-debug` and `gdextension/creature_anim` → `build/creature-anim-debug`: passed.
- Wave 26 Godot development matrix: `python3 -u /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration/work/playable-wave26/run-godot.py` from this worktree: **54/54** exit 0. Combined full combat passed **2,620** checks, zero failures. `FULL_TRACE`: **36,402** bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, equal to the Wave 26 baseline.
- Scripted read-only disc sweep: `bash /Users/syndicatemike/Projects/open-champions/worktrees/play-enemies/work/play-enemies/run-sweep.sh` from this worktree, four sequential 60-tick slices: **111/111 tried, loaded and played**, 230 checks passed, zero failures, no refusal kinds. All slices exited 0. Only counts are recorded here.

## Explicit limits

- Minotaur AI tick has no executed state-machine reference in rounds 2–8 and refuses with `Minotaur_run: Unknown`. Its documented death `setState(0x14)` call is covered; the unmeasured remainder requires a service answer.
- Nonempty `Creature_ctor` properties, model hide-mask, relocated animation, terrain/init, hand writes, boar target transitions, loot eligibility and spawn/landing remain explicit service/refusal boundaries. Synthetic service answers are not campaign values.
- The round-7/8 Valor entry was assisted/scripted. No normal-route campaign entry, player survival, integrated fight or Gold credit is claimed.
