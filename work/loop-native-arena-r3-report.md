STATUS: BLOCKED — bounded mechanics committed; the requested real arena match remains unimplemented, and the native test gate is not green.

# Native arena round 3

Implementation commit: `8c1476ae3d6285f3d6df8c274153e2a492427eed` (local only), on `codex/native-arena`,
base `b598e5b`. Evidence: `research-arena` `17810bb` and
`research-arena-match` `d4f58da6377f1011068ba80d3608dbe1e3149370`, including
round 11 and the PASS r9/r10/r11 audits. No seed-specific original probe was
rerun or promoted into native gameplay proof.

Added portable C++17 scoring/lives, recipe arithmetic, the native grid/saved
object callbacks, bounded intermission/end-task decisions, prompt fade and
complete saved-record restore. The Full World now connects base creature death
to score credit and player death to mode-B life loss. Synthetic coverage:
426 reference cases / 573 checks; 19,908 host checks. See `docs/ARENA_MATCH.md`.

## Gates (macOS arm64)

- Release native build: PASS.
- Focused CTest: 4/4 PASS in 0.29 s.
- Full CTest `-j 3`: **214/216 PASS**, 3131.28 s, exit 8. `player-exact`
  reached its 240.07 s limit; `session-facing-exact` reached its internal
  600 s subprocess limit. No data mismatch was reported by either timeout.
- Required isolated retries, unchanged limits: **player-exact TIMEOUT again**
  at 240.08 s (exit 8); **session-facing-exact PASS** in 488.68 s.
  Thus 215 unique cases passed; the player gate remains unresolved.
  Player sources/vector harness are unchanged from the base; its link closure
  is player/creature-HP/core, and Release `-O3 -DNDEBUG` was verified.
- Three Release `template_debug` Godot targets rebuilt from the committed code:
  PASS. Current-binary dev matrix: **45/45 PASS**. Zero script errors; the
  UI-layout smoke emits its expected wrong-arity diagnostic, as on the base.
- Private level regression: **111 tried / 111 loaded / 111 played**, **119 checks
  PASS**, zero failed checks, zero script faults; all four slices exited 0.
  Disc file count: **994 before / 994 after**. These are level regressions,
  not mode-B matches.
- `FULL_TRACE`: **36,402 bytes**, unchanged SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
  This existing synthetic fixture does not enter mode B.
- Playthrough: **BLOCKED**, command exited 2. Both
  `tools/run_loop_playthrough.py` and `godot/tests/loop_playthrough.gd` are absent
  from this worktree. No sibling progression implementation was imported.

## Remaining refusals

The base round-2 report already refuses a live mode-B match. This round does
not supply original Player/Goblin/holder/resource construction, the complete
original scheduler, wave construction, or Godot mode-B wiring. The World frame
runner still refuses PvP. Original r11 proves assisted kills and one bounded
wave handoff; it does not prove normal-input kills, later creature ticks,
a natural completed match, displayed results, real reward collection or a
successful next-level load.

Wave/engine/saved-record services remain explicit. Missing handlers, nonzero
purchase-candidate counts, and unestablished qsort tie order refuse. The audited
saved-object probes establish destructor target/self but not the flags; the
native adapter therefore requires that service instead of guessing flags.
No fixed Goblin gold/XP award or final mode-B victory condition was invented.

## Commands and files

- `python3 tools/generate_arena_match_vectors.py /Users/syndicatemike/Projects/champions-re-wt/research-arena-match`
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`
- `ctest --test-dir build/native -j 3 --output-on-failure -R '^(arena-match-host|arena-match-exact|combat-runtime-2-host|combat-runtime-3-host)$'`
- `ctest --test-dir build/native -j 3 --output-on-failure`
- `ctest --test-dir build/native -j 1 --output-on-failure -R '^player-exact$'`
- `ctest --test-dir build/native -j 1 --output-on-failure -R '^session-facing-exact$'`
- Godot source/build pairs: `gdextension` / `build/godot-extension`, `gdextension/creature_anim` / `build/creature-godot`, `gdextension/level_texture` / `build/godot-level-texture`. Each configured with `-DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp` and built with `--config Release --parallel 2`. Final targets: `champions-archive-extension`, `champions-creature-animation`, `champions-level-texture-preview` respectively.
- `python3 work/native-arena-r3/run-godot-current.py` (45 existing dev commands)
- `python3 work/native-arena-r3/run-sweep.py` (four `--all --levels=0 --ticks=60 --scripts` slices: `0/28`, `28/56`, `56/84`, `84/111`)
- `python3 tools/run_loop_playthrough.py --godot /Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder /Users/syndicatemike/Projects/open-champions/private-validation/rta-disc`

Changed: arena-match header/source; combat runtime API/world/services/defence
adapters and internal declarations; additive native CMake targets; synthetic
host/vector tests and provenance generator/verifier; arena documentation,
additive roadmap note and this report.

No private bytes, extracted assets or decoded game content is tracked.
No push, PR, issue or process-name kill.
