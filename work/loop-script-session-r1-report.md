STATUS: COMPLETE

All four causes in the wave-17 report are fixed on `codex/script-session`, which branches from c42ad2d (wave 17). Wave 18 had not landed: `codex/playable-integration` had merges after c42ad2d, uncommitted edits and no r18 report.

With scripts on, the 111-level sweep stays at **111/111** and executed command calls go from **4,192 to 8,456**. With scripts off it stays at **111/111** and matches wave 17 on every count except timing.

CTest on arm64 is **210/210**. 209 passed in the full run. `player-exact` timed out at 240 s while the sweep ran beside it, then passed alone in 158 s.

The Godot dev matrix is **45/45**. **FULL_TRACE did not change** (`374e40c9…2035`).

All commits are local. Nothing was pushed and no PR or issue was opened. The disc copy was only read, and its counts and logs stay in the gitignored `work/private/`. `~/champions-re` was used only to add the research worktree and to read its `SLUS_209.73` and `disasm/`.

## Research first (`t3/research-script-session`, 6841b8a)

The worktree `~/Projects/champions-re-wt/research-script-session` branches from `main`. I used `tools/ee_emu.py` and followed rules-champions-re.txt. The commit adds `docs/script-session.md` and four tools, and corrects one line of `docs/objects.md`.

- **Seed.** `getRand` draws `g_randSeed` 0x404464. Nothing reseeds it when a level starts.
  - The `gxref` listing shows 232 stores to it in 125 functions. Only these are not draws:
    - 0x322a04 stores the COP0 cycle counter, once per offline session;
    - 0x13dd0c in `net_startSync` stores 0 for a net game;
    - two save/restores around the prediction copy.
  - `game_loadLevel` only copies the seed into the cosmetic seed.
  - So for an offline game, no fixed value is "the original's" seed.
- **Single player.** `g_numPlayers` is 1 and `g_difficultyIdx` is 1. Both are the executable image values, and also what the offline setup stores (0x323a98 and 0x1e18b0; difficulty per research-items `script-items.md` §3).
- **Object factory.** `ref_level_factory.py` models `world_objectFactory` 0x159c94:
  - 206 name tests, in the order captured from the running original;
  - the rows that build no object;
  - the ten property fall-backs and the light choice.
  - `difftest_level_factory.py` ran 1,117 name cases, 552 property cases and **every record on the disc** (111 levels, 17,398 records, 1,795 distinct pairs), with **0 mismatches**.
  - Of those records, 15,901 build an object and **1,497 never become objects**. In every level these include the six plain light records and many `TestLocation` markers.
  - **Correction:** the Skeleton `red`/`blue` sub-tests can never match, so every Skeleton record gets variant 0.
- **`Creature_init`** (0x10a230) is now fully specified by `ref_creature_init.py`. `difftest_creature_init.py` ran 400 cases on each of seeds 20973 and 7, with 0 mismatches, and a deliberately changed constant fails it. It never stores +0x0c, +0x228 or +0x231 itself.
- 662 research unit tests pass.

## Native changes

| Commit | What |
|---|---|
| ee1adc4 | **(4)** `level_spawn_measure` now reads LMPs with `readLmp` (u32 count) through `tools/level_spawn_lmp.hpp`, the same fix as NET-3 (b06c4e5). `level-flow-exact` gets an authored lump with count word 0x00010001: the u16 reading accepted it, `readLmp` refuses it. On the disc every count is unchanged, plus `lmp_refused 0`. |
| ce4cc2d | **(1)–(3)** seed policy and session stream, single player, the object factory and built creatures (below). Also counts refusal stops per command. |
| 07ce211 | The real-data sweep reports the new object, seed and build counts. |
| 66e30fb | `champions-script-commands-measure` gets `--bare`, `--no-seed`, `--no-players` and `--no-objects`, used for the attribution below. |

**(1) Seed.** `bind_seed` now takes a `SeedPolicy`:
- `Exact` is the default and keeps the audit rule: `getRand` is refused once any record was not constructed here.
- `OfflineSession` answers from the session's generator. Records the original would construct but the host does not are counted in `load_draws_unmodelled`.

The controller uses `OfflineSession` with a single stream:
- When the script attaches, the stream starts at the play session's configured `shared_seed`.
- The load's draws come first: the built objects' `GameObject_ctor`, then `getRand` in `preStart`/`start`.
- `start_play_session` starts the control World from where the load left the stream. The World then creates the player, as the original creates players after `start`.
- The stream is unpaired, and `getRand` refused again, if the World draws on its own, a fixture session starts, or the control session is replaced. A script attached under a fixture binds no seed.
- This adds a read-only `shared_seed()` to `session_control::Controller` and `native_save::Scene`.

**(2) Objects.** `champions-level-factory` follows the research table. Its strings are read from the owner's executable, which `load_arena_tables` now keeps.
- With the executable, the host registers only the records the factory builds. Every built object takes a pool block in file order, so the pool fills where the original's would.
- Five creature classes are built: SmallSpider, Gnaw, Mosquito, Scorpion and Badger. Each goes through `GameObject_ctor` (one shared draw), then the ported `monsters_1` constructor running on the executable's image.
- Fields that no ported code computes are not copied into the host's memory: the hit points and the other `Creature_setStats` results, the height and draw byte, the hide mask, the serial and the string pointers.
- `getHitPoints` is therefore refused on a built creature.
- A record still becomes a stub (a placeholder) when `Creature_init` would call into the class or run a script: `dead`, `initScript`, the MiniBoss bit, or a weapon element.
- Every other constructor is a stub, counted per constructor address (93 constructors).
- Not built, because their constructors create extra objects: Skelight, Zek and Horsemen.

**(3) Players.** `start_single_player` stores 1 and 1, and the controller calls it when a script attaches.

**Tests:**
- `level-factory`: the 206-row table plus 1,126 cases with expected results from the pinned research reference.
- `level-objects`: 336 authored records. For every built object, each byte and the shared seed match the pinned research constructors (294 built, 42 stubs).
- `script-commands`: the offline seed policy, single player, and the factory deciding what gets registered.

## Verification

| Check | Result |
|---|---|
| Native build, Release, arm64 | 0 warnings |
| CTest arm64 (`ctest -j 3`) | 209/210 in 17 min 22 s. `player-exact` timed out (240 s) beside the sweep; it passed alone in 158 s, as did both measure tests |
| GDExtension, all four universal builds | pass |
| Godot dev matrix (`work/script-session/run-godot.py`, the wave-17 runner) | 42/45 on the first run. The three creature-anim smokes failed only because their extension was not yet built in the new worktree. After building it they passed: 32, 25 and 231 checks. `script_commands_smoke` PASS |
| FULL_TRACE | `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035` (21,990 bytes), the same as wave 17: the Full combat fixture binds no level script |
| Sweep `ss1 --scripts` | 111/111/111, 119 checks, 0 failures, 0 refusals, 8 min 7 s |
| Sweep `ss1n` (scripts off) | 111/111/111. Equal to `r17bn` on every count except `time.*`, `audio.frames_pushed` and the memory peak. Those three already varied between wave-17 runs (`audio.frames_pushed` was 2.29 M, 2.64 M, 2.60 M and 2.80 M across r17/r17b/r17n/r17bn) |

## Executed calls: 4,192 → 8,456 (+4,264), and why

The sweep runs 60 ticks per level. The +4,264 calls break down as:

| Command | Δ | Cause |
|---|---|---|
| `addParticleOnObject`, `cos1000`, `sin1000` | +1,136 each | Two more levels run their per-tick particle loop, because they no longer stop at `getRand` (levels running it: 1 → 3) |
| `getRand` | +367 (was 5 refused) | the seed binding |
| `getv` +242, `addDeferedCall` +120, `gamePaused` +119 | | Polling scripts now get past their old stop and keep queueing work |
| `callScript` +3, `setv` +2, `getNumPlayers` +1, `hideMonster` +1, `showMonster` +1 | | One more level each. `showMonster` now runs on a built Gnaw instead of stopping on a placeholder |

At load (the measure tool, with no ticks) I took each part out in turn. Executed calls: 581 (`--bare`), 798 (all three parts), 581 (`--no-seed`), 798 (`--no-players`), 650 (`--no-objects`).
- The seed causes almost all of the gain.
- The objects add the rest, through the Gnaw `showMonster` and the scripts behind it.
- The player count adds no calls. It changes where 11 scripts stop: `countQuestItems` 11 → 1 and `onePlayerHasItem` 2 → 13. Both are item commands, which this job does not implement.

Other changes in the sweep:
- Refusal stops 39 → 35. The load-time measure (38 → 34) shows the same structure:
  - the five `getRand` stops and the Gnaw `showMonster` stop are gone;
  - one more script stops at `generateMonster`;
  - the item-command stops go from 13 to 14.
  - `followPath` gains one refusal whose answer is not used, so it is not a stop.
- Wait / complete 14 / 28 → 15 / 30.
- Stub refusals stay at 102: fewer placeholders, but more script runs reaching `hideMonster`.
- Objects registered 17,398 → 15,901.
- Quest lines 0 → 8, mission bits 0 → 32 available / 13 complete. Both come from the player count.
- Quest variables 158 → 160, from scripts that run further.

## What remains

- **Policy call for you.** `OfflineSession` answers `getRand` even though 15,577 records' load draws are not made here. My reasoning: an offline session seeds from a clock, so no fixed original value exists. Netplay must use `Exact`, and keeping the audit's stricter rule here is a one-line change in `enable_level_script`.
- `Player1_ctor` with its draws, the first level's 223 item-loader draws, and 93 unported constructors.
- Hit points, because `Creature_setStats` is not ported.
- Skelight, Zek and Horsemen, whose constructors create weapon holders or mounts.
- The quest campaign stores mission rows 0..4, so difficulty 5 has no row.
- No x86_64 CTest, no export matrix and no manual play were run.
- Stops on item commands (`playerHasItem` 15, `onePlayerHasItem` 13, `countQuestItems` 1) belong to the native-items job.
