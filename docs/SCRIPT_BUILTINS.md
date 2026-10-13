# Return to Arms script built-ins: variable subset

`champions-script-builtins` is an authored portable C++17 translation of the
quest/profile variable contract for **Return to Arms, SLUS_209.73 US v2.00**.
It depends only on the existing `champions-amx` module and the standard library.
It implements five of the 191 table names. It does not establish Norrath behavior,
engine integration, actual memory-card persistence or a playable game.

## Pinned evidence

Research commit: `356add27d50e50972b2a671b31e8f4ec93e6778c` in the read-only
`champions-return-to-arms-re` sibling. The documented contract is
[ref_natives.py](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_natives.py),
SHA-256 `5ba711f93299a6cdb684651ae5a3dda5c0ddc5370baeea3a54ff123f1cc44839`.
The [specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/script-builtins.md),
SHA-256 `2beea1e91b3f71770a3d2ecd963fa856b9ee78a08647a2e218c03c53678cc2b4`,
sections 1.1, 1.2 and 1.5 establish dispatch, string conversion and tables.
The research MIT license, copyright 2026 TrustlessMike and contributors, applies
to this translation; the repository LICENSE retains that attribution.

The pinned `difftest_natives.py` harness compares the research model with original
handlers in an instruction interpreter, including result/error words, writes,
call-outs and script state. The specification records 300 random quest-variable
sequences and 60 cases per table name. These are **upstream evidence**, not tests
rerun here, console measurements or live gameplay proof. Its SHA-256 is
`c5e3d4367db8919ce8dd73e106fd6b8e0510cc2889e801bdc17203e5eb83251b`.
The fixture manifest additionally pins the exact imported `ref_amx.py`,
`ref_core.py` and `ref_objects.py` files. Generation validates both the checked-out
files and their contents at the recorded commit before importing them, and disables
Python bytecode writes. No research file is edited.

## Supported contract

| Entry point | Behavior |
|---|---|
| `getv` | ASCII lower-case lookup; appends an unknown name with value zero; signed byte result |
| `setv` | Appends if unknown; clamps signed32 value to -128..127; returns zero |
| `addv` | Gets first (possibly appending), adds modulo 2^32, then stores a clamped value; returns the signed32 sum before clamping |
| `getpv` | The corresponding lookup/insertion in this machine's profile table |
| `setpv` | Clamps and writes; requests config save only for a new name or changed value; returns zero |
| `quest_get`, `quest_set`, `quest_clear_all`, `quest_by_index` | Direct host helpers: clear values without removing names; indexed reads retain table order and return absent for invalid indices |
| `profile_get`, `profile_set` | Direct host helpers, including explicit save=false behavior |

`Variables::memory` owns zero-backed sparse little-endian bytes. Quest and profile
table identities are 0x86da80 and 0x4f9030; no host pointer dereferences occur.
Snapshots retain every nonzero byte (all omitted addresses read zero) and the
ordered address/size of every write, including stores that do not change a value.
Appending writes only name/terminator/value, preserving whatever follows. There
is no invented table capacity or extra terminating write. Even an empty name
retains the reference's writes and end-marker behavior. Reads can mutate a table.

`Variables::natives()` supplies exactly five AMX bindings. Keep the instance alive
for the lifetime of those bindings. Bind them to an existing owned `amx::Machine`;
its dispatcher handles error reset, parameter location and the low32 return.
The callbacks ignore the argument vector and argument-count cell and read the
handler's required cells directly from `machine.params`. Too-few declared
arguments therefore still read the caller's live cells. The existing AMX dispatcher
may refuse inaccessible excess argument cells before reaching a handler, as the
pinned AMX model also does.

All five handlers share pointer slot 0x20. A successful `get_addr` updates the
known address before string conversion; a refused address reuses it. Conversion
reads current bytes at that address, not a cached string. An unknown slot raises
`StackGarbage`. `forget_string_slot()` explicitly starts an unknown slot for a
fresh/nested run or a call whose frame this subset cannot track. An owner must
perform this reset when crossing those boundaries; no script-run host is implied.

`request_config_save` is an explicit callback. A changed profile entry is written
before the callback, so exceptions retain that write prefix. If the callback is
missing, the module raises `HostFault` at that point; it does not claim a save.
Callback reentry into variable helpers and callback mutations are supported.
Strings of 160 or more bytes and non-ASCII variable names raise `HostFault`, as in
the reference. Unpacked AMX strings terminate on a zero cell; a nonzero cell with a zero low
byte can therefore produce an embedded NUL. The module preserves the pinned
model's raw name bytes and write sizes even for those synthetic inputs. This is
a model edge case, not new original-code validation of C-library handling. Memory strings refuse after 0x4000
bytes like reference `Mem.cstr`. Table walks have an authored one-million-entry
resource guard (`ResourceLimit`), not an asserted original capacity.

## Verification and reproduction

The committed synthetic corpus contains 1,392 state transitions across
17 operations. Cases include packed/unpacked strings, ASCII case folding,
empty names, all signed clamp boundaries, signed32 overflow, zero/negative/short/
excess parameter counts, known/unknown rejected addresses, misalignment,
inaccessible arguments, 159/160-byte string boundaries, non-ASCII
refusal, equal-value/no-save profile writes, callback exceptions and callback
mutations. Authored tables span pages and exceed the nominal quest-table area;
this tests variable access only, not neighboring services.

Each transition compares complete sparse byte state, ordered writes, known
pointer slot, ordered config-save events, the full AMX image and all exposed
scalar execution/debug registers relevant to dispatch (including error and params).
The API test additionally checks missing-callback write prefixes, reentry,
embedded-NUL writes and host-address overflow refusal, and actual `SYSREQ.C` bytecode execution through both setv
and addv. Public tests use authored inputs only, without game files or a research
checkout. The CTest wrapper validates fixture/reference/generator hashes and
coverage before running C++, then verifies the complete emitted trace independently
and records its SHA-256 and host architecture under `build/native/core-traces/`.

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
```

The Release configure/build commands above passed on macOS arm64. The full
`ctest --test-dir build/native --output-on-failure -j 3` run passed all **102 tests**
in 1,065.04 seconds without failures or timeouts. The original-code differential
harness and private game-input acceptance were not run.

The final API and exact-vector checks passed on both macOS architectures with
1,392 transitions. Their complete trace files are byte-identical, SHA-256
`e72abd3541a26e89f814b9920adf145b565870d3b4dad789cfc8e494a34ee97f`. Fixture regeneration also reproduced
the compressed vectors and manifest byte for byte. The x64 check used:

```sh
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCHAMPIONS_TEST_PLATFORM=Darwin-x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
```

Local validation covers macOS arm64 and a separately compiled macOS x86_64
executable run through Rosetta. Windows execution, private game acceptance,
original-code differential reruns and Godot application integration remain
unverified. The trace report distinguishes the supplied target platform from
the Python runner's host platform.

## Exact remaining scope

The manifest's `unsupported_natives` records all 186 unported names at the pin.
They are listed below in original table order. `natives()` does not bind them;
`dispatch()` explicitly refuses unsupported names. No default engine behavior is
invented for these calls.

`callScript`, `print`, `printVal`, `sound`, `soundSequence`, `lightEffect`, `simpleNewWorld`, `newWorld`
`newWorldAlways`, `newWorldPost`, `activateMissionMenu`, `activateStore`, `storeRunning`, `getScaledTime`, `setFarClip`, `startDialog`
`changeSwoop`, `startWithSwoopDone`, `dialogRunning`, `increaseEnemyRank`, `charmMonster`, `rootMonster`, `unRootMonster`, `selectSetMonsterVal`
`setMonsterVal`, `selectSetCFlag`, `setCFlag`, `selectSetOFlag`, `setOFlag`, `setWeaponType`, `setMonsterRange`, `killMonster`
`deleteMonster`, `hideMonster`, `showMonster`, `moveMonster`, `followPlayer`, `stopFollowingPlayer`, `followPath`, `selectFollowPath`
`stopFollowingPath`, `selectStopPath`, `friendly`, `unFriendly`, `startPropAnim`, `stopPropAnim`, `propAnimAtEnd`, `startNPCAnim`
`NPCAnimAtEnd`, `addExpandingPool`, `playerHasItem`, `onePlayerHasItem`, `removePlayerItem`, `givePlayerItem`, `giveOnePlayerItem`, `givePlayerRandom`
`givePlayerExp`, `givePlayerGold`, `takePlayerGold`, `checkPlayerGold`, `checkPlayerMinGold`, `playGotItemSound`, `setInfo`, `addHelpMessage`
`addQuest`, `removeQuest`, `hornOmen`, `planarSteel`, `particleMorph`, `healPlayer`, `leaveGame`, `endGame`
`clearRecall`, `startMusic`, `startAudio`, `endMusic`, `saveGame`, `saveEndConfig`, `saveDone`, `exitToMainMenu`
`reapEnemies`, `getNumPlayers`, `getRand`, `activePlayerIsDwarf`, `activePlayerIsElf`, `gamePaused`, `pauseGameControl`, `levelLoading`
`setParticle`, `addParticleOnObject`, `setNoCollide`, `shakeCamera`, `deleteObjects`, `hidePlayers`, `checkPlayerAlive`, `setBestActivePlayer`
`decayPlayerLights`, `checkMonstersLeft`, `forceMutexRelease`, `addDeferedCall`, `cutScene`, `postAction`, `newWorldParameter`, `putNPCInVehicle`
`putPlayersInVehicle`, `entityRideVehicle`, `controlVehicle`, `playersLoaded`, `holdVehicle`, `getPlayerVPosition`, `getVPosition`, `getPlayerVDir`
`setupPlayerSel`, `startPlayerSel`, `playerSelRunning`, `objInPlayerTrig`, `setEntrance`, `hudResetFadeScreen`, `hudFadeScreen`, `closestPlayerDist`
`hitPlayers`, `zeroPlayerMana`, `showCompass`, `showBust`, `getMaxPlayerHP`, `getHitPoints`, `setHitPoints`, `saveCheckPoint`
`loadCheckPoint`, `trigger`, `autoPropSetState`, `autoPropGetState`, `preloadResource`, `setSaveInhibit`, `getSaveInhibit`, `countQuestItems`
`countPlayersAlive`, `countLiveObjects`, `networkMode`, `getManaUsed`, `getHealthUsed`, `getLevelTimer`, `getEvil`, `setEvil`
`getMadeDecision`, `setMadeDecision`, `grantMission`, `getMissionComplete`, `setMissionAvailable`, `getMissionAvailable`, `grantGold`, `grantSilver`
`grantBronze`, `getMedal`, `generateMonster`, `monstersInTrigger`, `sin1000`, `cos1000`, `helpMessageActive`, `addHelpMessageF`
`revealMap`, `showOnMap`, `initScriptSetParams`, `setCreatureParams`, `explodePotionsOnly`, `runMedalSuccess`, `runMedalFail`, `setAlternateDeath`
`dropItem`, `selectDropItem`, `findDistance`, `setContainerProps`, `setWaveDetail`, `demoRunning`, `removeAllItems`, `savePlayer`
`restorePlayer`, `piercePlayerEar`, `playerAdjustStat`, `giveFullHealth`, `setPlayerFlag`, `getPlayerFlag`, `clearAllSkills`, `isClericOrWizard`
`refillPlayerMana`, `removeAllWeapons`

Beyond those handlers, shared object/name-table integration, selected-object and
active-player machinery, deferred queues, post actions, nested entry-point runs,
missing-entry throttling, level-transition/network gates and object-triggered
script dispatch remain unported here. Campaign flags/missions/medals, player
records, inventory, creatures, vehicle state, presentation and all associated
external callbacks remain outside this module. Connecting this subset to a
Session, a complete engine command table or gameplay needs separate work.
