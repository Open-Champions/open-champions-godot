STATUS: BLOCKED — the documented mode-B first wave needs a creature class that the native combat World cannot construct.

# Arena match implementation round 1 (2026-10-21)

Worktree: `~/Projects/open-champions/worktrees/arena-match`, branch `codex/arena-match`, based on `0548440` (wave-21 head, with wave-20 `8bdd705` in its ancestry). No code was changed, pushed or put in a PR. The only repository commit is this report.

## Established boundary

- `t3/research-arena` `docs/arena-modes.md` sections 3–4: mode A is versus, with a kill limit and **no monster wave**. The run task and three-wave recipe are installed only when mode B is set. With both words set, the task runs but mode A scoring, rotation and recipe skip apply. The existing play scene's mode-A fallback only selects a ground start; it does not establish a match.
- `t3/research-items` `49bc578`, `docs/script-items.md` sections 5.2 and 6: the builder at `0x3065fc` takes a class from the executable's 30-entry table, then calls `world_createObject`, scale/stat services, `SpellArena` and class state 0. `game_scanCm1Vars(0)` returns the cached `0x4f7110` word; the front end rescans this machine's profile. The native library ports both, but the play session calls neither for a match.
- `native/src/combat_runtime_own.cpp` `World::State::spawn_class` can construct Skelight, Gnaw, AntQueen, Skeleton and Soul; an unknown run table or unexecuted constructor refuses. AntQueen/Skeleton still have refused states described in the combat-ctors report. The Full combat scene is an authored fixed fixture, not a runtime that can instantiate and run every class from an arena recipe.
- Counts-only, read-only check of the owner's RTA US v2.00 executable: the first recipe calls for **2 monsters in wave 0, 3 in wave 1 and 4 in wave 2** with one active player. All 9 calls use classes outside the five constructible classes above. No private bytes, class names, or decoded content were saved or printed. The check used the file-backed ELF mapping and the published recipe/class table addresses, then applied the native `spawn_budget` and `recipe_groups` rules.

## Why the match stops

Entering one of the three arena floors is already possible via the mode-A start fallback. A full offline monster match requires mode B and a new World/Session adapter that supports the recipe's actual creatures, object factory, `Creature_scaleStats`, `SpellArena`, wave-clear callback, player death/respawn and end-of-match restore/shutdown. The research executes the builder as a standalone contract but does not establish the missing creatures' complete constructors, run/state dispatch or death path. The arena-modes contract explicitly refuses the empty-wave callback, `arena_nextWave` body, `player_respawn` body, saved record layout and `arena_shutdown` body. The documented mode-A rotation can be computed by `Match`, but cannot be presented as a playable monster match.

Per `rules.txt`, undocumented behavior was not guessed or replaced with a synthetic encounter. At the first spawn of mode B wave 0 the match must refuse at the creature constructor, before several waves, scoring from kills, respawn, or end flow can be verified. This is earlier than the AntQueen/Skeleton state boundary. Research for the actual recipe creatures and refused callbacks, with `tools/ee_emu.py` difftests, is necessary before an exact end-to-end client can be implemented.

## Verification

- `git merge-base --is-ancestor 8bdd705 HEAD`: passed.
- Public source and research contracts inspected. Private executable used read only for the counts above.
- No native CTest, Godot dev matrix, new arena smoke, 111-level sweeps or FULL_TRACE was run. With no implementation changed, those baseline checks could not validate the requested match. In particular, a smoke claiming several real waves would be false at the first unsupported spawn.

Files changed: this report only. The shared coordination log received one dated status line. No new research worktree was created; `~/champions-re` was untouched.
