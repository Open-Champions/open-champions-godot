STATUS: COMPLETE

Wave 15 of job playable-integration is done. `codex/script-commands` is
merged. Real levels run their scripts when the user opts in, and the real-data
sweep stays **111/111** with scripts on. CTest is 199/199 on arm64 and on
x86_64 under Rosetta, both on the first full run. The Godot dev matrix is
45/45 and the export matrix is 18/18, including the 36,000-tick run.
Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch
`codex/playable-integration`, local commits only (no push, PR or issue).
`~/champions-re` was not touched. The disc copy was only read, and its counts,
logs and summaries stay in the gitignored `work/private/`.

## Merge (start `6081211`)

| Branch | Range | Merge commit |
|---|---|---|
| script-commands | 037e26d..2088ab1 (3 commits) | b2882e5 (no conflicts) |

The native, extension and Godot code merged as is. No integration fix was
needed. The level-texture extension does not compile the host, and its
release dylib is byte-identical to wave 14.

## (1) Scene wiring

`real_data_play` already carried the branch's opt-in wiring, and it works
unchanged on top of wave 14:

- A "Level script (opt in)" checkbox, or `--level-script` on the command line.
- When opted in, `load_real_level` reads the level's `script.scr` and
  `objects.ob` and calls `enable_level_script`.
- `start_play` reattaches the script after a reset. Reset and stop drop the
  host.
- When not opted in, the scene calls `disable_level_script` and no host
  exists. The controller pumps a ready tick into the host only while one is
  enabled. Counts never enter the play observation.

Check of the scene's own path (untracked `work/playable-wave15/scene_path_check.gd`,
three level indices, 30 steps each):

| Run | Levels attached | Levels started | Failures |
|---|---|---|---|
| With `--level-script` | 3 / 3 | 2 | 0 |
| Without the flag | 0 / 3 | 0 | 0 |

There were 0 faults. Without the flag, `script_command_counts` reported no
counts.

The sweep driver change is `8141634`. With `--scripts` it also records
`scripts.levels_started` and, for each command, how many levels executed or
refused it.

## (2) Real data, 111-level sweep with scripts

`work/run-slices-r15.sh r15 --scripts` ran four headless slices, 60 ticks per
level, with the light and recipe JSON. Two slices (28–56 and 84–111) were cut
short when a harness time limit stopped them. The machine's load average was
around 200 then, from sibling worktrees. Those two slices were rerun whole
with `work/playable-wave15/slice.sh`. All four final slices exited 0 with
0 failed checks.

- Tried / loaded / played: **111 / 111 / 111**. 0 refused for any reason.
- Every count outside the host equals wave 14 (story ground 111, marker 98,
  water 48, meshes 174,456 / textured 174,408, ready ticks 6,549, checks 119,
  and so on). Only `time.*`, `audio.frames_pushed` and the peak static memory
  (+0.4 MB) differ.

Script totals:

| | Count |
|---|---|
| Scripts attached | 111 |
| Scripts started | 76 |
| Reached a wait / completed | 41 / 37 |
| Faults / missing entries | 0 / 0 |
| Objects registered / refused | 17,398 / 0 |

The started, wait, completion and object counts equal the native measure.
`champions-script-commands-measure --game rta`, run from the integrated
build, reproduces the branch's numbers: 1,681 executed across 36 names and 529
refused across 27. The play run's call counts are higher because the session
resumes deferred names on all 60 ready ticks, while the measure stops on a
repeated deferred table. Polling scripts repeat their commands. Three names
appear only in play: `findDistance`, `closestPlayerDist` and
`stopFollowingPath`.

For each command, "Levels" is the number of levels whose script ran the
command at least once.

**Executed: 22,371 across 38 names**

| Command | Levels | Calls |
|---|---|---|
| `addParticleOnObject` | 9 | 5960 |
| `gamePaused` | 23 | 3222 |
| `addDeferedCall` | 41 | 2597 |
| `cos1000` | 5 | 2552 |
| `sin1000` | 5 | 2552 |
| `getRand` | 10 | 2161 |
| `getv` | 53 | 1222 |
| `countPlayersAlive` | 10 | 561 |
| `getScaledTime` | 7 | 533 |
| `callScript` | 24 | 263 |
| `setv` | 50 | 175 |
| `hideMonster` | 41 | 138 |
| `zeroPlayerMana` | 2 | 118 |
| `getHealthUsed` | 1 | 60 |
| `postAction` | 27 | 39 |
| `setSaveInhibit` | 27 | 27 |
| `getNumPlayers` | 14 | 25 |
| `showOnMap` | 4 | 24 |
| `setMissionAvailable` | 23 | 23 |
| `getEvil` | 16 | 19 |
| `findDistance` | 4 | 15 |
| `showMonster` | 15 | 15 |
| `closestPlayerDist` | 1 | 14 |
| `getMissionComplete` | 10 | 13 |
| `setPlayerFlag` | 3 | 9 |
| `addQuest` | 8 | 8 |
| `friendly` | 1 | 6 |
| `grantMission` | 4 | 4 |
| `getMadeDecision` | 3 | 3 |
| `increaseEnemyRank` | 1 | 3 |
| `clearAllSkills` | 2 | 2 |
| `getMissionAvailable` | 1 | 2 |
| `explodePotionsOnly` | 1 | 1 |
| `getPlayerFlag` | 1 | 1 |
| `moveMonster` | 1 | 1 |
| `removeQuest` | 1 | 1 |
| `setEntrance` | 1 | 1 |
| `startWithSwoopDone` | 1 | 1 |

**Refused: 1,375 across 28 names**

| Command | Levels | Calls |
|---|---|---|
| `showBust` | 9 | 502 |
| `countQuestItems` | 12 | 306 |
| `setMonsterVal` | 13 | 94 |
| `playerHasItem` | 16 | 91 |
| `onePlayerHasItem` | 2 | 72 |
| `removePlayerItem` | 4 | 47 |
| `setCreatureParams` | 11 | 45 |
| `rootMonster` | 4 | 30 |
| `setCFlag` | 3 | 23 |
| `cutScene` | 17 | 18 |
| `followPath` | 5 | 16 |
| `autoPropSetState` | 6 | 15 |
| `setParticle` | 13 | 14 |
| `deleteMonster` | 2 | 12 |
| `generateMonster` | 5 | 12 |
| `addHelpMessage` | 9 | 10 |
| `stopFollowingPath` | 1 | 10 |
| `savePlayer` | 9 | 9 |
| `setAlternateDeath` | 9 | 9 |
| `saveCheckPoint` | 8 | 8 |
| `setWeaponType` | 5 | 8 |
| `givePlayerItem` | 1 | 7 |
| `preloadResource` | 7 | 7 |
| `charmMonster` | 3 | 4 |
| `revealMap` | 3 | 3 |
| `removeAllItems` | 1 | 1 |
| `removeAllWeapons` | 1 | 1 |
| `startDialog` | 1 | 1 |

## (3) CTest

Native Release builds pass on both architectures.

**`skills-a-exact` timeout (`becea3e`).** I raised the limit from 120 s to
300 s in `native/skills_a_tests.cmake`. That is the only edit to shared CMake.
The justification:

- In wave 14 it ran 116 s alone under Rosetta, against the 120 s limit.
- Its arm64 time is about 22–28 s, so Rosetta runs it about five times slower.
- The vector script has no limit of its own.
- 300 s gives headroom similar to the other trace tests (240–900 s).
- This wave it took 47 s under Rosetta.

**arm64.** `ctest --test-dir build/native -j 3 --output-on-failure`:
**199/199**, 694 s (`player-exact` 147 s, `skills-a-exact` 28 s).

- My first attempt started while the load average was near 200.
- `player-exact` hit its 240 s limit there, with only 2 tests finished after
  14 min.
- I stopped that attempt and its orphaned test processes, then reran the full
  suite once the load fell. No single-test rerun was needed.

**x86_64 under Rosetta.** `cmake -S native -B build/native-x64
-DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64`, build, then
`ctest -j 3`:

- **199/199** on the first run, 1,076 s, no timeouts.
- `skills-a-exact` 47 s, `monsters-1-exact` 79 s, `player-exact` 145 s.
- The binaries are Mach-O x86_64.
- The three new tests (`script-commands`, `script-commands-measure-usage`,
  `script-commands-measure-empty`) pass on both architectures.

## (4) Godot

- `python3 work/playable-wave15/run-builds.py`: all four universal extension
  builds pass.
- Dev matrix `run-godot.py`: **45/45**. It is the wave-14 43 plus
  `script_commands_smoke.gd` (PASS) and `real_data_play.tscn -- --level-script`
  with no game folder (exit 0).
  - Full combat: 2,616 checks.
  - Combined FX: 1,876.
  - Wave-12 extensions: 40.
  - FX bridge refusals: 33.
  - creature-anim: 32 / 25 / 231.
  - `native_fx_visual`: 4 runs, no refusals.
  - `FULL_TRACE` is `8dc349c1…aa0c`.
- Export: run from `godot/`, `Godot --headless --path . --export-release
  'macOS universal' ../build/exports/wave15-final/OpenChampions.zip`, then
  `ditto -xk`.
- `run-export.py`: **18/18**. It is the wave-14 16 plus the same two script
  checks, exported.
  - The 36,000-tick 1280×720 Full run: 108,822 checks, 0 failures, 142 s.
  - Exported headless: 2,616. 800×600: 2,742, with 63 captures.
  - 960×720 stalled once in the matrix. Its window drew no frames, so it wrote
    0 captures and the headless-versus-drawn trace check failed. It sat near
    4% CPU, and I killed it after 1,179 s. Rerun alone, it passed 2,742
    checks with 63 captures in 18 s.
  - Every `FULL_TRACE` (headless, 16×9, compact and the 4×3 rerun) is
    `8dc349c1…aa0c`.
- Exported app on real data (gitignored summaries):
  - The 12-level set equals wave 14 apart from timing, both without and with
    `--scripts`.
  - With scripts: 12 attached, 6 started, 4 waits, 2 completions, 0 faults.
    656 commands executed across 19 names and 112 refused across 11.
  - The three arena indices: 3 / 3, equal to wave 14. Scripts attached,
    0 started (no level entry).
- App: `build/exports/wave15-final/Open Champions.app`.
  - ZIP SHA-256 `87423687…f019`.
  - Main dylib `28342066…e95a`. Texture dylib `0b93213e…9941`, unchanged.
  - Both are packaged byte-identical, x86_64 + arm64.

## Commits

- `b2882e5`: merge codex/script-commands.
- `8141634`: per-level command counts in the real-data sweep driver.
- `becea3e`: `skills-a-exact` CTest limit 300 s.
- `c034ce3`: docs/REAL_DATA.md wave-15 section.
- `9d813ff`: docs/INTEGRATION.md wave-15 section.
- This report.

## Open / unverified

- 28 commands stay refusals and return 0: combat, creature AI, audio, HUD
  text, items, cutscenes and paths. Scripts that branch on them (for example
  `playerHasItem`, `countQuestItems`) take the 0 path, which may not be what
  the original would do.
- Script effects stay in the host's own object, effect and level-flow worlds.
  Particles are counted and not drawn, `hideMonster` / `showMonster` /
  `moveMonster` do not move drawn meshes, and `setEntrance` does not change
  the play level.
- The post action runs once at load. The one-tick budget is shared across the
  deferred names of a tick, and the original has no instruction budget.
- The one stalled 960×720 run is attributed to the window not drawing (no
  frames, no captures). That was not proven, and the rerun passed.
- No captures of scripted play were taken this wave.
- Interaction was scripted, not manual play. No audio was heard. No native
  Intel, Windows or Linux hardware was used (x86_64 is Rosetta only).
