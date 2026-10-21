STATUS: STILL OPEN — mounted Horsemen need an exact linked Nightmare object path.

# Creature statistics and level script construction, wave 21

Base: wave-20 `8bdd705` on local branch `codex/creature-stats` (the final
wave-20 report commit was merged after the implementation began). Research is local
branch `t3/research-creature-stats` in its own worktree; no push or PR.

## Implemented

- `Creature_setStats` now uses the selected level's executable row and
  `monsters.bal` percentages. The executable's balance hash and lookup were
  run in `ee_emu.py` and difftested against an authored reference on seeds
  20973 and 7. The already executed combat statistic model supplies hit
  points, armor, damage, attack and experience. Skelight's temporary monster
  level override and the current level-name global reach that calculation.
- The factory builds Skelight, Zek and on-foot Horsemen using their executed
  constructor models. Their embedded weapon holder slot-1 body was separately
  executed and difftested on both holder tables. Factory variants now map the
  four on-foot rows to Horsemen and the four mounted rows to the Rider form.
- Player1 construction reads the selected WORLD for four starting spots. The
  binder now registers its externally owned objects with the world query,
  which lets spatial placement inspect them. A fresh level script advances
  the shared seed by the 223 first-level item resource draws before preStart.
  Godot and the counts-only measure pass the level name, balance data, and
  WORLD to the host.
- Refused commands still stop an entry when their answer is used.

AntQueen, Skeleton, and Soul files were not changed.

## Executed research

In `t3/research-creature-stats`, the selected combat, object-marker, item-load,
arena-monster, balance-lookup, holder-attachment and Nightmare-constructor
seams were run with `tools/ee_emu.py` and reference models on seeds 20973 and
7. The new balance model matched 300/300 cases per seed; each holder table
matched 300/300 per seed. Nightmare construction plus direct state-zero stores
matched 100/100 authored fresh-pool cases per seed, with the already researched
Creature base and external services delegated. The research Python suite passed
652 tests. Research commits: `67ca002`, `2752339`, `3469927`, `eaebc00`.

## Acceptance counts

Four-slice, counts-only 60-tick real-data sweeps on the read-only selected disc:

| Metric | Wave 20 scripts on | This branch scripts on | This branch scripts off |
| --- | ---: | ---: | ---: |
| Levels tried / loaded / played | 111 / 111 / 111 | 111 / 111 / 111 | 111 / 111 / 111 |
| Passed checks / failed checks | 230 / 0 | 230 / 0 | 230 / 0 |
| Failed levels / refusal list | 0 / empty | 0 / empty | 0 / empty |
| Built records | 324 | 503 | 0 |
| Unbuilt records | 15,577 | 15,398 | 0 |
| Registered objects | 15,901 | 15,901 | 0 |
| Refusal stops | 16 | 16 | 0 |
| Stub refusals | 265 | 260 | 0 |
| Unmodelled load draws | 15,577 | 15,398 | 0 |

Thus 179 more factory records are built, with the same number fewer unbuilt
records and unmodelled construction draws. The remaining four mounted
Horsemen are explicitly counted under `mounted Horsemen needs Nightmare_ctor`.
The 111-script counts-only measurement also had zero script faults and zero
object adoption refusals; it reported 503 built and 15,398 unbuilt.

## Verification

- Focused native `level-objects`, `script-builtins-api`,
  `script-builtins-exact`, and `session-objects-exact` passed. The authored
  fixture checks exact Gnaw and Skelight statistics, embedded holder bytes,
  on-foot Horsemen and Zek construction, and the Rider refusal.
- Both macOS GDExtension template variants and both level-texture variants
  built. The creature-animation extension built; its three smokes passed
  32, 25, and 231 checks.
- Godot dev matrix (`python3 work/creature-stats/run-godot.py`): 51/51 passed
  on the final libraries. Its headless Full-combat smoke passed 2,621 checks
  with zero failures.
- Native arm64 Release CTest (`ctest --test-dir build/native --output-on-failure -j 3`):
  216/216 passed, zero failures, 737.35 seconds.
  `file build/native/champions-level-objects-tests` confirms a Mach-O arm64
  executable; the CMake cache has `CMAKE_BUILD_TYPE=Release`.
- The headless Full-combat `FULL_TRACE` remains 21,990 bytes, SHA-256
  `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`.
  These changes apply to the opt-in real-data script host; the synthetic Full
  combat scenario does not enter it, so its trace is unchanged.

## Still open

The Nightmare constructor's bounded stores and call order are researched, but
the binder does not yet create and link the mounted Horsemen's second object.
The researched handle layout and the mount's later lifecycle must be connected
and difftested before those four Rider records can be marked built. The
variant-three skeleton lump load is currently a delegated presentation
service; its resource side effects are not modeled in the script host.

## Commands

All commands below were run locally, with counts-only artifacts under ignored
`work/private/` or `work/creature-stats/`:

```text
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(script-builtins-api|script-builtins-exact|session-objects-exact|level-objects)$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
python3 work/creature-stats/run-builds.py
cmake --build build/mac-template_debug --parallel 4
cmake --build build/mac-template_release --parallel 4
python3 work/creature-stats/run-godot.py
work/run-slices-creature-stats.sh csfinal --scripts
work/run-slices-creature-stats.sh csfinaln
./build/native/champions-script-commands-measure --game rta <owner-selected-rta-disc>
```

The research commands were the selected `tools/difftest_combat.py`,
`tools/difftest_objects.py`, `tools/difftest_arena_monsters.py`, and
`tools/difftest_items.py` quick routes plus the two new balance and holder
drivers and the Nightmare constructor driver, each with `--seed 20973` and
`--seed 7` against the owner executable. Research tests used
`python3 -B -m unittest discover -s tools -p 'test_*.py' -q`.
