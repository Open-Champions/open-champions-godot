STATUS: COMPLETE

Wave 17 of job playable-integration is done. `codex/audit-fixes`, `codex/quests` and `codex/arena` are merged, in that order. The documented arena replacements are wired in, and the real-data sweep is back to **111/111**, with scripts on and with scripts off.

CTest is **208/208** on arm64 and on x86_64 (Rosetta), both on the first run. The Godot dev matrix is **45/45** and the export matrix **18/18**, both on the first run. The flaky "stale clip" check has a proven cause and is fixed.

Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch `codex/playable-integration`, start `77d1260`. Local commits only: no push, PR or issue. `~/champions-re` was not touched. The disc copy was only read. Its counts, logs and summaries stay in the gitignored `work/private/`.

## Merges

| Branch | Tip | Merge | Notes |
|---|---|---|---|
| audit-fixes | bffec5f | d2e7a8b | Clean. The tree equals bffec5f. |
| quests | 265904e | 6d5033f | Conflicts in `script_commands.cpp`, ROADMAP and SCRIPT_COMMANDS. |
| arena | a9af2e4 | 764ddff | Clean. |

Resolving quests against the audit's script host (SC-1..7):
- Undocumented commands still go through `refuse()`, so a used refused answer stops the entry. The three commands that `quests::route_undocumented` routes are the exception. The `getRand` and stub gates come after.
- **Added `setCFlag` to the stub-read list** (limit 0x200). `ref_natives.n_setCFlag` at research-quests 7efd3ce tests creature flag 0x20 on each object it finds, which is the audit's own stub rule. The new test fails without the row and passes with it.
- `quests_tests` and `quests_measure` were ported from the removed `begin_level` to `run_prestart` / `adopt_objects` / `run_start`.
- The synthetic quest script now ends `ZERO.pri`, `RETN`, like a compiled void function. Before, its last refused command's answer was returned, which correctly stopped the entry.

## AntQueen, Skeleton and Soul: kept refused (evidence)

- `t3/research-combat` `tools/difftest_combat_round5.py` at faa7033 executes only these run bodies:
  - `AntQueen_run` 0x236f78, states 0/1: 101 of 607 instructions;
  - `Skeleton_run` 0x2a6864, states 0/1: 143 of 809;
  - the `Soul_run` 0x2a9870 phase gate: 126 of 1062.
- Each runs on a zeroed 0x800 block with a stub vtable (`SightRun.go`).
- No tool calls the constructors 0x236c70, 0x2a5af4 or 0x2a9478. `docs/monsters-2.md` marks them as Observed only.
- cr5's `spawn_class` built all three through the Skelight `spawn()`: size 0x3c8, base vtable, run and setState swapped in. That is the audit's CAI-4.
- The coordinator's new research agrees: `t3/research-combat-ctors` e9777eb, `docs/spec/combat-ctors.md` §1, says no tool on any branch had called them.
- They are not restored. The new ctor research, potions 1/2 (98c081a) and the music volume (22408a1) go to a separate native job.

## Wiring the documented replacements

- **Respawn pick.** `level_flow::arena_respawn` (previously `[[noreturn]]` refusing `ArenaRespawn`) now forwards the registered spawn table to `arena_modes::pick_spawn(flag 0)`.
  - It refuses an empty table, or a hole below count.
  - `level-flow-exact` replays the three generated flag-0 reference cases through it.
  - `champions-level-flow` links `champions-arena-modes` through a new guarded `native/arena_modes_library.cmake`. The level-texture extension also compiles `arena_modes.cpp`.
- **Arena mode A.** `arena_executable.{hpp,cpp}` is the measure tool's ELF reader moved into the library, plus `read_names` (13 and 7 entries, or refuse) and `mode_a_index`. They have synthetic-ELF tests, and the arena measure output is byte-identical.
  - The session gets `load_arena_tables(exe)` (counts only; names are kept in memory) and `set_terrain_start_objects(path, objects, level_name)`.
  - A level whose story snap has no ground starts with `arena_mode_a_start_ground` only when the executable's mode-A table lists it. That is the audit's suggested gate (§18.1). The start is published as `start.arena_mode` "A", pre-search, with no facing.
  - `real_data.gd` reads the game folder's `SLUS_209.73`.
- **Campaign.** `enable_level_script(..., carry_campaign)` can start a host from the previous host's campaign image. It is off by default, so sweep slices stay order-independent.
  - `script_command_counts().campaign` reports quest lines, mission bits, variables and events.

## (1) Real data, 111-level sweep (`work/run-slices-r17.sh`, 60 ticks)

| Run | Time | Tried / loaded / played | Refused | Failed checks |
|---|---|---|---|---|
| `r17b --scripts` (final tree) | 5 min 28 s | 111 / 111 / 111 | 0 | 0 |
| `r17bn` (no scripts, final tree) | 5 min 41 s | 111 / 111 / 111 | 0 | 0 |

- The final-tree runs equal the earlier `r17` / `r17n` runs on every count apart from timing.
- Against wave 16 these are unchanged: meshes, surfaces, textured meshes, water (48 levels, 9,451 tiles, 48 textures), 6,549 ready ticks, 119 checks, story ground 111, marker taken 98, and scripts attached / started / faults / objects (111 / 76 / 0 / 17,398).
- New counts:
  - `start.arena_mode_a` 3 and `start.pre_search` 98;
  - constant-light chunks 1,244 (base 0x80 on 108 levels, 0x40 on 3);
  - `arena.tables_read`.
- With scripts on:
  - wait / complete 41 / 37 → 14 / 28;
  - refusal stops 39 and stub refusals 102;
  - executed command calls 22,371 → 4,192;
  - `getRand` 2,161 executed → 5 refused.
- Fixed along the way (ac1b0d9): every level had listed the stale refusal "level light refused: loaded level name unknown". `fx.reset()` recorded it before the loader named the level. Every level did apply its light record. The final runs have an empty refusal list.

Measure tools on this tree: arena (identical to the arena branch), level-spawn (paired 111, cold 3, cold start floor 3), and quests. For quests:
- getv 244 → 142, setv 116 → 82;
- setCFlag 23 executed → 1 refused;
- quest variables 198 → 158;
- the chain and every table count are unchanged.

## (2) CTest

| Arch | Result | Time | Slowest tests |
|---|---|---|---|
| arm64 | **208/208**, first run | 692 s | `player-exact` 140 s |
| x86_64 (Rosetta, `build/native-x64`, Mach-O x86_64) | **208/208**, first run | 1,284 s | `player-exact` 179 s, `monsters-1-exact` 84 s |

There were no timeouts and 0 compiler warnings. The netplay worktree was running x86_64 tests at the same time. The load peaked at about 13.

## (3) Godot

- All four universal extension builds pass.
- Dev matrix `work/playable-wave17/run-godot.py`: **45/45** on the first run.
  - Full combat 2,620 checks, FX 1,876, animation 1,809, play-short 1,341, level 1,643.
  - Wave-12 extensions 22, FX bridge refusals 33, creature-anim 32 / 25 / 231.
  - `native_fx_visual` ran 4 times with no refusals. `script_commands_smoke` passed.
- **FULL_TRACE `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`** (21,990 bytes), the audit-fixes value.
- Export `--export-release 'macOS universal'` → `build/exports/wave17-final/`. `run-export.py` passed **18/18** on the first run.
  - The 36,000-tick 1280×720 Full run passed 108,826 checks with 0 failures and 63 captures.
  - Exported headless passed 2,620, and 960×720 / 800×600 passed 2,746 each. Every `FULL_TRACE` is `374e40c9…2035`.
  - ZIP `77822940…287c`. Main dylib `7684822c…3f6c`, texture dylib `c424e01b…0ab5`. Both are packaged byte-identical, x86_64 + arm64.
- Exported app on real data (60 ticks): 12/12, 12/12 with `--scripts`, and 3/3 arena levels with `--scripts`. 0 failures and 0 refusals. The only differences from wave 16 are the new counts above.

## (4) The flaky "stale clip" check: cause and fix (4e89ddb)

- **Cause.** The smoke wrote its fixtures to `user://playable-wave-smoke`. Every worktree and the exported app share one project name, so they share one `app_userdata/Open Champions/` folder.
  - A smoke running at the same time elsewhere could rewrite `cosmetic.lmp`. The picker then opened a file that already held the 512 clip.
  - The test's own 512 rewrite changed nothing, so the SHA-256 fingerprint still matched. The restart succeeded, and the check failed.
- **Fix.** The fixture directory is now per process (`OS.get_process_id()`) and is removed in `_finalize`. The play variant gets the same change.
- **Proof.** I ran four concurrent `combined-animation` runs, three rounds each:
  - before the fix, 4 of 12 failed: 2 "stale clip", and 2 "short DAT rejected" (the same race on another fixture);
  - after the fix, 0 of 12.
- Both matrices then passed on their first runs.

## Regressions kept

Each is an audit refusal with no documented replacement on these branches:
1. **Full fight without AntQueen, Skeleton and Soul** (CAI-4). Their constructors run only in research-combat-ctors e9777eb, which is for the new job.
2. **Own classes other than Gnaw and Skelight** refuse to spawn (CAI-4).
3. **Thrown potions 1 and 2** stop a Full session (CR4-1). Documented in 98c081a, for the new job.
4. **No level or boss music** until the music volume is supplied (AUD-1). The documented default is 0.5 (22408a1), for the new job.
5. **Level scripts:**
   - `getRand` is refused, because no session seed is bound (SC-4).
   - objects.ob records are stubs (SC-5), so creature, HP and type reads refuse. This now includes `setCFlag`.
   - A used refused answer stops its entry (SC-6).
   - Executed calls fell from 22,371 to 4,192.
6. **HUD overlay draws nothing** (HUD-1..7). The medal box needs `hud_drawMessageBox` and `menuList_draw`. Campaign state alone does not supply them.
7. The combat round-3/4 refusals, the removed env-pass and atlas guesses, and the other audit refusals are as `codex/audit-fixes` left them.

## Commits

- d2e7a8b, 6d5033f, 764ddff: the three merges.
- 7be097c: arena mode A, the respawn pick and campaign wiring.
- 4e89ddb: per-process smoke fixture directory (the stale-clip fix).
- ac1b0d9: stale level-light refusal cleared.
- 7f66a7b: LEVEL_FLOW, ARENA_MODES and QUESTS docs.
- 2eefef7: INTEGRATION.md wave-17 section and REAL_DATA.md wave-17 section.
- This report.

## Open / unverified

- The session's binder has no planted player count. The sweep's mission bits and quest lines therefore stay 0 within 60 ticks, while the measure tool plants one player and gets 41 available bits and 8 quest lines.
- No act tables are read in the session, so no act change runs there.
- The quest `clearAllSkills` / mission-offset contradiction is still open in research.
- No arena `Match` runs in the scene: waves, scores and respawn timing are not bound to Godot. Only the start and the respawn pick are wired, and the respawn pick has no session caller yet.
- The other `combined-level` flake that audit-fixes noted ("WORLD, preview and art owners survive long play") shares the same directory, so it is probably the same race. It did not occur this wave and was not reproduced.
- No native Intel, Windows or Linux run. No manual play.
