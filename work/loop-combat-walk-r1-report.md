STATUS: done — both causes found and fixed. Research executed both bodies (seeds 20973 and 7, 0 mismatches). Netplay 2/3/4 peers pass with save/load, arm64 equal to x86_64. CTest 212/212 on arm64 and on x86_64 (one Rosetta timeout passed on its rerun alone). Godot 48/48. Sweep 111/111 with scripts on and off. FULL_TRACE unchanged.

# combat-walk round 1: walking after an attack, and a walker beside an armed creature

Branch `codex/combat-walk` (worktree `~/Projects/open-champions/worktrees/combat-walk`). It starts at the wave-18 verified head `0f3cafc`; wave 19 is not recorded yet. It merges `codex/netplay` `c60759e` (`2a6d3e8`), because the netplay fixture lives there. The one conflict, in `native/CMakeLists.txt`, is resolved exactly as integration resolved it in `52133a3`. Research branch `t3/research-combat-walk` (worktree `~/Projects/champions-re-wt/research-combat-walk`, from `main`, merges `t3/research-combat` `faa7033`). Nothing was pushed and no PR was opened. `~/champions-re` was touched only by `git worktree add`. The private disc was only read, and only counts are reported.

## Problem 1: "after the first swing the player never walks again"

**Cause: the fixture's input, not the player state machine.** The netplay attack script pressed attack once every 12 frames from frame 24 to the end of the run. In `Player_run` 0x1a5030, state 1 (melee) ends when one of these holds:
- the stick is past 0.8 (`0x3f4ccccc`) and `g_simFrame - player +0x448 >= 39.3` (`0x421d3332`), which with the stick held is **40 ticks after the current swing started**;
- `+0x470 × 60` ticks have passed (96 for these classes);
- `+0x450` is set.

A press after the swing window (22.02 `0x41b028f5` minus the haste trim, divided by speed) chains the next swing (`melee_next_swing` 0x1aac9c), which **restarts +0x448**. So a press every 12 frames chains swings forever, and the player stands from the first swing to the end of the run. The original does exactly this.

Evidence:
- Research reference first (`ref_combat_full.FullRuntime`, stick held, one press at tick 30): state 1, then state 2 at tick 70, then it walks. The native World gives the same sequence and the same positions (printed at float precision) through `World::stick` / `press_attack`.
- Native netplay path probe (`work/cw/net_probe.cpp`): one press at frame 24, then walk: the player moves on to the last tick. The mash script never moves again.
- **Executed against the original:** research `difftest_combat_walk.py` part 1. The original `Player_run` runs in ee_emu through difftest_player's rig, and every tick ref_player runs first and must be equal: calls, stores, seed, exit. Six melee classes, three runs each, seeds 20973 and 7, 13,633 / 13,488 ticks compared, 0 mismatches.

| Property | What the original did |
|---|---|
| `walk.once` | ends 40 ticks after the press; then `Player_move` mode 0 every tick |
| `walk.mash.stop` | ends 40 ticks after the last swing start; then mode 0 |
| `walk.mash.12` | never leaves state 1; restart gaps 12 and 24; no mode 0 |
| `walk.release` | ends at 96 ticks |

There was nothing to fix in the native `Player_run` port. The fixture now walks after attacking.

A second detail came out of this: `combat_play::Session::press_attack(false)` makes a pressed edge with no held bit. The neutral state starts a swing from the held word (pad +8, `ref_player.Tick.state_neutral`), so that call starts no swing. `pad_apply` never produces such a pad. The new Full fight test therefore uses a held press for one tick.

## Problem 2: a walker beside an armed Skelight reaches world_findFreeSpot

**Cause:** the body was already ported. `World::find_free_spot` 0x14acf8 landed in wave ≤18 (`7a99a74`, from research-levels `13fafa4`, 11 pinned cases). `codex/netplay` branched before that commit, and its world never supplied the two terrain services the body needs. So after the merge the search refused with `world_findFreeSpot terrain service missing`; the probe hit it at sim tick 1,038. The chain: a Skelight pinned by a walker makes no progress, so `Creature_moveTo` 0x114510's stuck recovery (300 clock ticks, then a side-step ending) calls `world_findFreeSpot(position, 72, radius, 0)`.

**Fix:** each netplay peer now answers `terrain_blocked` / `terrain_height` from its own level copy, using `world_queries::terrain_test_flag1` (0x183b48) and `terrain_height` (0x183820). These are the same kernels `combat_play` already feeds the Full fixture. `Session::free_spot_searches(peer)` counts the searches. load() replays the journal, so the count is recomputed after a load.

**Research.** The search was already executed by `difftest_objects.py` `load.marker` (Player1_ctor's arguments). Rerun here, it passes on both seeds: 896 / 907 searches, and every adder model.

I also executed it with the creature caller's arguments (`difftest_combat_walk.py` `free_spot`). It runs on the original object world, with radius 16 for players and 12 for creatures, some objects not solid, and 15 % crowded steps. The terrain is the composed runtime's flat floor. The point, the ok byte, the terrain calls, the seed and difftest_objects' full world check must all be equal.

| Seed | Searches | More than one try | No spot found | Mismatches |
|---|---|---|---|---|
| 20973 | 400 | 296 | 92 | 0 |
| 7 | 400 | 221 | 61 | 0 |

The research `FullRuntime` also refused the call-out (tick 396 in a walking fight). The new `ref_combat_walk.WalkRuntime` composes `ref_objects.World.find_free_spot` on the runtime's own world and seed, and it ran 3,000 ticks of that fight.

## Tests extended

**Netplay fixture (`native/tests/netplay_tests.cpp`).** There are now four inputs:
- `Walk`;
- `Mash`, the old attack script;
- `Swing`: one press at frame 24, then walk;
- `Combo`: presses at frames 24, 36, 48 and 60, then walk.

New checks:
- `attack_then_walk`: Mash never moves after tick 140. Swing and Combo walk on to tick 399.
- Three new long runs with a live creature, each saving and loading at the midpoint. Every peer and the replay must make the same searches, and every walking run with a creature must search. The players must walk on almost every tick from 200 on (Swing).
- The tick lengths stay under the iterator-retention cap. That cap is unchanged: 65,536 / (4 + players + creatures) is 9,362, 8,192 and 7,281.

**Full fight (`native/tests/combat_free_spot_tests.cpp`, CTest `combat-free-spot-host`).**
- `attack_then_walk`: a held press while walking. State 1 starts at tick 10 and ends at tick 50, 40 ticks after the swing start. The player takes no step during the swing, then steps every tick and covers more than 100 units. A press every 12 ticks keeps state 1 with no step, and the swings chain at least 4 times.
- `walk_beside_skelight`: 1,500 ready ticks walking ±x past the Skelight at x = 80. It makes 8 searches, all (72, 12, 0), with no refusal, and the player still walks at the end.
- Both runs stop at 120 ticks, because the fixture's two Skelights kill a player who stays near them at about 147 ticks (the existing party-wipe refusal).

## Verification

| What | Result |
|---|---|
| Native build arm64 (`build/native`, Release) | exit 0 |
| Native build x86_64 (`build/native-x64`, Release, `-DCMAKE_OSX_ARCHITECTURES=x86_64`, Mach-O x86_64 under Rosetta) | exit 0 |
| CTest arm64 `-j 3` | **212/212**, 1,167 s (wave 18's 211 plus the merged `netplay`, 177.5 s) |
| CTest x86_64 `-j 3` | 211/212 in 2,115 s. The only failure was `player-exact` timing out at 240 s (wave 18 needed 180 s for it; the machine was under load and it runs under Rosetta). **Rerun alone it passed in 228.8 s, so 212/212.** That test does not touch anything this branch changes. `netplay` passed in 288.8 s and `combat-free-spot-host` in 3.0 s |
| `champions-netplay-tests` arm64 and x86_64 | both exit 0, 183 checks, **outputs byte-identical** (`diff` empty) |
| `champions-netplay-level-measure` (arm64, private disc, counts only) | exit 0; disc_files=994, worlds_decoded=119, lmp_refused=0; 2/3/4 peers 10,000 / 9,000 / 8,000 ticks, mismatches=0, save_load=1, synced=1, moved_ticks 9,846 / 8,874 / 7,893 |
| Godot extensions | 5 builds exit 0: mac template_debug/release universal, level-texture debug/release universal, creature-anim debug |
| Godot dev matrix (`work/cw/run-godot.py`, a copy of wave 18's) | **48/48** exit 0. Every smoke's checks/failures count equals wave 18 (`diff` empty), e.g. full combat 2,620, scene_play_flow 65 |
| 111-level sweep, 60 ticks, scripts on (`cw1`) and off (`cw1n`) | **111/111** tried, loaded and played both ways; 0 failures, 0 refusals. Every count equals `r18` / `r18n` except `time.*`, `memory.static_peak_bytes` and `audio.frames_pushed`, which follow wall-clock time and load |
| FULL_TRACE | `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes: **unchanged** |
| Research `difftest_combat_walk.py`, seeds 20973 and 7 | 5/5 PASS each |
| Research `difftest_objects.py -k marker`, seeds 20973 and 7 | 4/4 PASS each |
| Research unit tests (`python3 -m unittest discover -s tools -p 'test_*.py'`) | 883 OK (6 new in `test_ref_combat_walk.py`) |

### Netplay determinism (arm64 = x86_64)

Every long run saves at its midpoint, loads, and must equal the uninterrupted run through the end. Every run is synced. Short 2/3/4-peer sessions also check save stability and the continuation.

| Peers | Ticks | Creature | Input | Searches | FNV-1a 64 |
|---|---|---|---|---|---|
| 2 | 10,000 | no | walk | 0 | ff00f5feb1f313eb (= r2) |
| 2 | 10,000 | yes | mash | 0 | 851dedc83c64092d (= r2) |
| 3 | 9,000 | no | walk | 0 | e00a5850977e534 (= r2) |
| 4 | 8,000 | no | walk | 0 | cc5d9f8362ff7036 (= r2) |
| 2 | 9,000 | yes | swing then walk | 10 | b5e9de2acec916bf (new) |
| 3 | 8,000 | yes | walk | 15 | 7b420205f0c95c44 (new) |
| 4 | 7,000 | yes | walk | 9 | 10d24357f588e8bf (new) |

The four r2 digests are unchanged. None of those runs reaches a search: the mash run's creature dies by tick 66, and the other three have no creature.

### FULL_TRACE

It is unchanged, and that is expected:
- No native simulation body changed. The netplay change only adds services and a counter to `netplay.cpp`, which Godot does not link.
- The new tests only add checks.
- `combat_play`'s Full fixture already supplied the terrain services.

## Commits

**open-champions-godot `codex/combat-walk`:**
- `2a6d3e8` merges `codex/netplay` `c60759e` for the netplay fixture.
- `37505cd` Netplay: answer world_findFreeSpot's terrain tests; walk after attacking (`native/src/netplay.cpp`, `native/include/champions/netplay.hpp`, `native/tests/netplay_tests.cpp`, `native/tests/combat_free_spot_tests.cpp`, `docs/NETPLAY.md`, `docs/COMBAT_RUNTIME.md`).
- This report.

**champions-re `t3/research-combat-walk`:**
- `e84c54a` merges `t3/research-combat` `faa7033`.
- `a10fe26` adds `tools/difftest_combat_walk.py`, `tools/ref_combat_walk.py` and `tools/test_ref_combat_walk.py`.
- `e20760a` adds `docs/spec/combat-walk.md` and a row in the `docs/combat-runtime.md` index.

Logs (gitignored): `work/cw/logs/`, `work/cw/godot/`, `work/private/cw1*-aggregate.json`. Research logs: `research-combat-walk/work/combat-walk-*.log`.

## What stays open

- The netplay attack cadence is authored. The fixture now mixes walk, mash, swing and combo, but no human rhythm is claimed.
- On a real level, the search's terrain tests are the native `world_queries` kernels. Research did not re-execute those two kernels in this round; they come from the world-queries port. `difftest_combat_walk` used the flat floor.
- The Full fixture's two Skelights kill a player who lingers near them (about tick 147 when walking in −y, 786 when idle). This is the existing party-wipe refusal and was not changed.
- The iterator-retention cap still bounds every long Full run.
- From r2, still open: Internet sockets, the lobby and rendezvous, ghost prediction, the original net-game path, and `net_isLocalSlot` answering 1 for every slot.
