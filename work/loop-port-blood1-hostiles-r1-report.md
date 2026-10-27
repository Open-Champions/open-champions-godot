STATUS: COMPLETE — bounded native blood1 hostile actors implemented and verified; 237/237 arm64 Release CTest, 54/54 Godot development matrix, two 111/111 sweeps, and unchanged FULL_TRACE. Live fight, landed damage and actual drops remain Unknown.

# Blood1 hostile actor port, round 1

Worktree `port-blood1-hostiles`, branch `codex/port-blood1-hostiles`, base `1cfddf0`. Code commits `f2107fa` and `fd25f67`. The pinned research is `t3/research-blood1` at `5107fcb` (`docs/blood1.md`, `tools/ref_blood1_enemies.py`, `tools/difftest_blood1_enemies.py`); the audit reported PASS. The interface mirrors the separate `port-warboar` actor runtime: a caller-owned `objects::World`, `resources::Manager`, typed actors and explicit service answers. There is no play-scene wiring.

## Implementation

- Added native `Kobold`, `KoboldTough`, `KoboldShaman` and `WarWraith` classes in `native/include/champions/blood1_actors.hpp` and `native/src/blood1_actors.cpp`, plus additive CMake entries and `native/tests/blood1_actors_tests.cpp`. Each actor gets one World object and resolves its documented class-definition entries by name through the resource manager. The class-owned fields, event setup, balance factors, bounded run/AI callbacks, null-attacker HP subtraction and state-20 death dispatch follow the reference models. Tests use authored synthetic archives and balance rows, with seeds 20973 and 7; a fresh WarWraith probe checks its exact seeded holder part.
- The follow-up commit refuses nonzero-attacker damage before any HP, flags or frame write, since the isolated HP model has no World-backed attacker bytes. Missing named resources, nonempty properties, unsupported AI decisions, unknown death remainder, drop generation and fade paths require service answers or throw a labelled `Unknown` refusal.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`: PASS, arm64 Release. The first heavily loaded build exited 143 near completion; the incremental rerun completed without a compile error.
- `cmake --build build/native --target champions-blood1-actors-tests -j 2`; `ctest --test-dir build/native --output-on-failure -R '^blood1-actors$'`: PASS after the final code edits.
- Relevant arm64 CTest: `ctest --test-dir build/native --output-on-failure -j 3 -R '^(blood1-actors|resource-manager|arena-monsters-exact|combat-damage-exact|combat-stats-exact|objects-runtime-.*|session-objects-.*|creature-hp-.*|combat-creature-.*|monsters-1-.*)$'`: **19/19 passed** after the attacker refusal. The exact fresh WarWraith seed assertion was added afterward and passed in the focused test.
- Final full `ctest --test-dir build/native --output-on-failure -j 3`: **237/237 passed** in 2,337.05 seconds, including `blood1-actors`; no timeout reruns were needed.
- Release `template_debug` builds for `gdextension` → `build/godot-extension`, `gdextension/level_texture` → `build/godot-level-texture` and `gdextension/creature_anim` → `build/creature-anim-template_debug`: PASS. Each used `cmake -S <source> -B <build> -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp` followed by `cmake --build <build> -j 4`, with the source/build pairs just listed. The Godot development matrix (`python3 -u work/blood1-hostiles/run-godot.py`, local ignored helper copied from the integration matrix) passed **54/54**. `FULL_TRACE` remained **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- Read-only disc sweep through `bash work/blood1-hostiles/run-slices.sh blood1-on --scripts` and `bash work/blood1-hostiles/run-slices.sh blood1-off` (local ignored helper): each four-slice mode passed **111/111 tried, loaded and played**, **230 checks**, zero failures, zero summary refusal kinds. The disc still contains **994 files**. These are counts only; logs and inputs remain ignored and uncommitted.

## Limits

The research established call sites with synthetic stubs. Landed player damage, a real kill transition, actual loot or Gold, and a native fight remain **Unknown**. The client still does not instantiate these classes in the play scene. `dead()` reports HP at or below zero; the caller must request the documented death state, and the downstream effects require supplied services. Final balanced HP and damage on a loaded blood1 placement are **Unknown**. No retail bytes, extracted assets or private data are committed; no push, PR or issue was made.
