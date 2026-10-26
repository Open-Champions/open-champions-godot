STATUS: COMPLETE WITH CTEST TIMEOUT LIMIT — all five B1H audit findings fixed in `1b40e38`; the full native aggregate was 235/237, and both timed-out vector commands passed in isolated follow-up checks.

# Blood1 hostile actors, audit fix round 2

Worktree `port-blood1-hostiles`, branch `codex/port-blood1-hostiles`, starting tip `6930cdc`. Audit `port-blood1-hostiles-r1.md` reported VERDICT FIX (B1H-1 through B1H-5, all HIGH). The pinned behavior evidence remains `t3/research-blood1` at `5107fcb`, especially `docs/monsters-2.md`, `docs/spec/creature-tick.md`, and its reference models. This remains a native World runtime with no play-scene wiring.

## Fixes

- **B1H-1:** Bound actors to their World object's serial and address. The World death callback retires entries on normal destruction; lookup and construction also retire stale entries after direct `World::destroy`. Checked `emplace` before returning. A destroy/reconstruct test reuses the address, reads and ticks the replacement, and checks both destruction routes.
- **B1H-2:** State-6 and no-target state-7 transition requests now require an explicit animation-completion answer and fire only when it is true. Hit-event dispatch is independent. Tests compare running, completed, targeted and no-target ticks, including the requested next state (Kobold 5; WarWraith 3).
- **B1H-3:** WarWraith active ticks now update fade, creature flags `0x54`, and object flag `1` from the pre-state fade. The prefix turn runs only at fade >= 99. Tests assert post-tick fields at fade 100/98, across the 21 threshold, and toward chase fade 60. Unmodeled dead-particle fade states still refuse.
- **B1H-4:** Death requires a `Creature_filterState` refusal answer before state or drop effects. An accepted state-20 request clears four documented timers before either drop callback. Tests cover nonzero timers, filter refusal, missing filter, and timer values observed by the drop service.
- **B1H-5:** State-0 ticks require the caller's class deadline answer and compare it with the World frame. Tests cover missing timing, at-deadline idle and after-deadline wander request for all four classes. The runtime does not synthesize an unproved deadline.

Changed files: `native/include/champions/blood1_actors.hpp` (callback and tick contracts), `native/src/blood1_actors.cpp` (lifecycle, state, fade and timing behavior), and `native/tests/blood1_actors_tests.cpp` (synthetic regressions). This report is the only additional tracked file in round 2.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`: PASS, arm64 Release.
- `ctest --test-dir build/native --output-on-failure -R '^blood1-actors$'`: **1/1 passed** on both reference seeds 20973 and 7.
- Affected arm64 selection: `ctest --test-dir build/native --output-on-failure -j 3 -R '^(blood1-actors|resource-manager|arena-monsters-exact|combat-damage-exact|combat-stats-exact|objects-runtime-.*|session-objects-.*|creature-hp-.*|combat-creature-.*|monsters-1-.*)$'`: **19/19 passed** (315.66 seconds).
- Full `ctest --test-dir build/native --output-on-failure -j 3`: **235/237 passed** in 3003.81 seconds. `player-exact` hit its 240-second CTest timeout and `session-facing-exact` hit its internal 600-second subprocess timeout; neither yielded an assertion mismatch.
- Serial timeout follow-up: `ctest --test-dir build/native --output-on-failure -j 1 -R '^session-facing-exact$'` **passed 1/1** in 330.90 seconds. `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'` again hit the fixed 240-second test timeout. The same pinned player vector command, `/opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz`, passed **274,304 exact complete-state cases and transport refusals** under a 900-second external limit. The full CTest aggregate remains a reported timeout result, not a clean 237/237 pass.
- `cmake --build build/godot-extension -j 4`, `cmake --build build/godot-level-texture -j 4`, and `cmake --build build/creature-anim-template_debug -j 4`: PASS. `python3 -u work/blood1-hostiles/run-godot.py`: Godot development matrix **54/54 passed**. The synthetic `FULL_TRACE` is 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- `bash work/blood1-hostiles/run-slices.sh blood1-on-r2 --scripts` and `bash work/blood1-hostiles/run-slices.sh blood1-off-r2`: private-disc read-only sweeps **111/111 tried, loaded and played** in each mode; each yielded 230 passed checks and zero failures. Private input count: 994 files. Only counts are reported.

## Limits

The new death-filter callback supplies the caller's global refusal decision; the native death path implements the documented timer effects only for an accepted state-20 request. Idle deadlines also require caller answers. Actual landed damage, live fight, player survival, loot and Gold remain **Unknown** under the pinned synthetic research. No retail bytes or extracted assets are tracked; no push, PR or issue was made.
