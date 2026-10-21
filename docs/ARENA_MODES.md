# Arena modes

RTA US v2.00 versus (mode A) and wave arena (mode B), from research
`t3/research-arena` commit `17810bb0f9a3474fbeae9e86d339cb72ccd0f154`.
The prose is that checkout's `docs/arena-modes.md`. The executable model is
`tools/ref_arena_modes.py`. `tools/difftest_arena_modes.py` reported 302
passing checks against the original functions. Nothing in that round was
observed on a console.

`native/src/arena_modes.cpp` is the port. Floats are raw patterns from
`champions-core`. Recipe bytes stay caller arguments: the research reads them
from the executable and does not publish them, so this tree does not embed
them. The 13-name and 7-name tables are the strings printed in the research
prose.

## What runs

- Shared generator `seed * 0x19660d + 0x3c6ef35f`, draw `seed >> 16`.
- Score block: reset, kill credit, suicide, life loss, the mode B life grant,
  and the gold purchase (`150 / 100` becomes `50 / 200`, HUD word `0x80`).
- Overlay timer, glyph parameters, hermite, and the prompt fade step. The
  timer clamp that copies lives is the store in the reference. The difftest
  did not take that branch; a step that stays at or below 2.0 was executed.
- `arena_pickSpawnPoint` for both `a0 != 0` and `a0 == 0`, including the
  offline 240-unit candidate test and the camera-target fallback.
- Wave planning, ring step, monster level, intro lock, intermission limits
  `480` and `960`, kill limit, wipe, and the live-match player scan.
- Mode A map rotation and mode B record selection. Confirm uses
  `arena_levelName`. Cancel clears mode B and stores session exit 1.
- Mode A armour cut and the later player-only half. `combat::damage_apply`
  already applies both when `World::pvp_a` is set. This module exposes the
  same two formulas and does not re-enter that pipeline.

The run task exists only while mode B is set. Mode A alone never installs it.
Damage still reads mode A. The task does not advance the fade; a caller
supplies confirm and cancel as booleans. Button bit `0x100` in the difftest
is a fixture, not a documented mask. If both booleans are set, confirm runs
and cancel does not. That pair was not executed.

## Refusals

| Item | Why |
|---|---|
| Session word `0x4f7124` | Unknown. Facing treats it as an input. |
| Net message `+0xf4` | Unknown. |
| Empty-wave callback `0x3069e8` | The 128-frame gate was read. The callback was not run. A true gate is recorded and does not invent a living count. |
| Draw helper `0x307bbc` | Not run. |
| Draw task on every lockstep machine | Unknown. Fade step is the executed store. |
| Monster builder `0x3065fc` | Only the call (class, argument 0) was executed. |
| `arena_nextWave` body | Not run. The task records argument 0 or 1 and does not advance the wave. |
| SpellArena particles | Positions were stubbed. Flag bit and tint gate are implemented. |
| `player_respawn` body | Stubbed. The caller records spread 2 and radius 3, then refills the fixture's max HP and mana. |
| `arena_shutdown` body | Stubbed. Confirm records the call. |
| Saved player-record bytes | Not in this research. Confirm counts one restore request per active slot. |
| Retail recipe bytes | Not published. A missing record is a refusal, not a guessed table. |

`0x4f7124` is not derived here. Direct stores found by the research are zeroes.
The session writers and the net field stay unknown.

## Reproduce

```
python3 tools/generate_arena_modes_vectors.py \
  ~/Projects/champions-re-wt/research-arena \
  native/tests/arena_modes_vectors.inc
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4 --target champions-arena-modes-tests
./build/native/champions-arena-modes-tests
```

The generator checks the pinned SHA-256 of the research prose, reference,
difftest, logic test, and `tools/ref_core.py` before importing the reference.
`native/tests/arena_modes_contract.json` records those hashes.
