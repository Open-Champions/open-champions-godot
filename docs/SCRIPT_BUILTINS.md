# Return to Arms script built-ins: variables and campaign state

`champions-script-builtins` is an authored portable C++17 translation of the
quest/profile variable and campaign-state contracts for **Return to Arms, SLUS_209.73 US v2.00**.
It depends only on the existing `champions-amx` module and the standard library.
It implements 19 of the 191 table names. It does not establish Norrath behavior,
engine integration, actual memory-card persistence or a playable game.

## Pinned evidence

Research commit: `356add27d50e50972b2a671b31e8f4ec93e6778c` in the read-only
`champions-return-to-arms-re` sibling. The documented contract is
[ref_natives.py](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/tools/ref_natives.py),
SHA-256 `5ba711f93299a6cdb684651ae5a3dda5c0ddc5370baeea3a54ff123f1cc44839`.
The [specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/script-builtins.md),
SHA-256 `2beea1e91b3f71770a3d2ecd963fa856b9ee78a08647a2e218c03c53678cc2b4`,
sections 1.1, 1.2 and 1.5 establish dispatch, string conversion and tables;
section 1.11 and section 3.2 establish campaign state and its 14 handlers.
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

`Variables::natives()` supplies exactly 19 AMX bindings. Keep the instance alive
for the lifetime of those bindings. Bind them to an existing owned `amx::Machine`;
its dispatcher handles error reset, parameter location and the low32 return.
The callbacks ignore the argument vector and argument-count cell and read the
handler's required cells directly from `machine.params`. Too-few declared
arguments therefore still read the caller's live cells. The existing AMX dispatcher
may refuse inaccessible excess argument cells before reaching a handler, as the
pinned AMX model also does.

All five variable handlers share pointer slot 0x20. A successful `get_addr` updates the
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

## Campaign-state family (14 new handlers)

| Entry points | Exact behavior |
|---|---|
| `getEvil`, `setEvil` | Record 0 returns the difficulty bit mask; setting zero does nothing, nonzero sets the byte bit in the first player-count records |
| `getMadeDecision`, `setMadeDecision` | Same mask read from the decision byte; the setter always sets it without reading any argument |
| `grantMission`, `getMissionComplete` | Completion bit 1, at the current difficulty; granting updates all four records even when player count is zero |
| `setMissionAvailable`, `getMissionAvailable` | Availability bit 0; setting copies bit 0 of the argument to the first player-count records; reading uses record 0 |
| `grantBronze`, `grantSilver`, `grantGold` | For each player with a lower signed medal byte, write reward-player byte, invoke reward scripts in gained-level order, then store the requested medal |
| `getMedal` | Request this machine's first local player slot, then return that player's signed medal byte |
| `addQuest` | Append title/text in player order; stop the entire call at the first full list or duplicate title |
| `removeQuest` | Replace every matching title/text with the nine-byte placeholder; compact after the last match and decrement count once |

The same owned `Memory` now exposes little-endian 32-bit reads/writes and player
record addressing (base 0x4d8af0, stride 0x4c30). Global player count is 0x4d8ae0,
difficulty 0x4f719c, network byte 0x402791, and reward-player byte 0x4f7149.
Mission, medal, choice and quest offsets come from the pinned reference, not a
new save schema. Mission numbers >=13 return/refuse the operation as specified;
negative mission/difficulty/slot values keep low32 address arithmetic. Difficulty
shift counts are masked modulo 32, with only the low byte stored. No speculative
0..3 player-count, 0..12 lower mission bound or 32-byte quest-string limit is added.
Quest writes can overlap adjacent fields, exactly as the reference models them.

Host helpers expose mission availability/completion with an explicit difficulty,
medal grants/threshold queries, quest insertion/removal, and choice masks. The
completion helper preserves the special value -1 and otherwise copies bit 0.
The medal helper bypasses local-slot lookup only with a nonzero flag in a network
game; lookup occurs even before rejecting a mission >=13. Loops implemented as
Python `range` capture player count; medal grants and quest insertion reread it.

`local_player_slot` represents `net_getLocalPlayerSlot(0)` and must be supplied
when required. `run_reward_script(name)` is an explicit boundary for the existing
`script_runByName` service. It receives `firstBronze`, `firstSilver`, or `firstGold`
in reference order; its return value is ignored. Missing callbacks raise
`HostFault` at the call site, preserving preceding writes. The callback owns
nested execution, missing-entry behavior, and saving/restoring outer string slots;
this family does not implement those services. The generator substitutes this
same explicit seam in the Python model, rather than claiming nested execution
coverage. Callbacks can reenter helpers, mutate state, replace themselves or
throw; subsequent iterations read the reference's live state.

`addQuest` shares slot 0x30 between both string arguments. `removeQuest` shares
slot 0x20 with the original five variable handlers. On entry, a handler clears
all slots except its own, and numeric handlers clear both, as `GameState._bind`
specifies. A refused pointer retains that handler's previous known slot; an
unknown slot raises `StackGarbage`. Conversion still uses the 160-byte handler
buffer boundary. `forget_string_slot()` now clears both slots.

## Verification and reproduction

The committed synthetic corpus contains 7,477 state transitions across
38 operations. The first 1,392 original transitions are unchanged byte for byte.
Cases include packed/unpacked strings, ASCII case folding,
empty names, all signed clamp boundaries, signed32 overflow, zero/negative/short/
excess parameter counts, known/unknown rejected addresses, misalignment,
inaccessible arguments, 159/160-byte string boundaries, non-ASCII
refusal, equal-value/no-save profile writes, callback exceptions and callback
mutations. Authored tables span pages and exceed the nominal quest-table area;
those older cases test variable access only, not neighboring services.

Each transition compares complete sparse byte state, ordered writes, known
pointer slots, ordered config-save, reward-script and local-slot events, the full
AMX image and all exposed
scalar execution/debug registers relevant to dispatch (including error and params).
The campaign cases cover zero/negative player counts, signed mission and difficulty
boundaries, count-word aliasing, signed medal bytes, ordered rewards, callback
exceptions/mutations, threshold/flag queries, duplicate/full/negative-count quests,
overlapping strings, and slot invalidation. The API test additionally checks
missing-callback write prefixes, reentry,
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

The Release configure/build commands passed on macOS arm64. Both API and exact
checks passed on macOS arm64 and separately compiled macOS x86_64 through Rosetta,
with 7,477 complete transitions. Their trace files are byte-identical, SHA-256
`08a183f511af60225724405eb26197f4ea69100bd0c517cc1666b9cc389a8621`.
Independent regeneration reproduced both the compressed vectors and manifest byte
for byte, and the original 1,392-transition prefix also remained byte-identical.
The original-code differential harness and private game-input acceptance were
not run. The full Release `ctest --test-dir build/native --output-on-failure -j 3`
run completed in 2,852.71 seconds: 100/102 passed; only `session-creature-transport`
and `session-turn-transport` exceeded their 600-second subprocess limits.
The isolated `session-creature-transport` rerun passed in 178.18 seconds.
The isolated `session-turn-transport` rerun passed in 118.90 seconds. Thus all
102 registered tests passed across the full run and its isolated retries; the
initial parallel run itself retained the two timeout failures.

```sh
ctest --test-dir build/native -R '^session-creature-transport$' --output-on-failure
ctest --test-dir build/native -R '^session-turn-transport$' --output-on-failure
```

The x64 check uses:

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

The manifest's `unsupported_natives` records all 172 unported names at the pin.
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
`hornOmen`, `planarSteel`, `particleMorph`, `healPlayer`, `leaveGame`, `endGame`, `clearRecall`, `startMusic`
`startAudio`, `endMusic`, `saveGame`, `saveEndConfig`, `saveDone`, `exitToMainMenu`, `reapEnemies`, `getNumPlayers`
`getRand`, `activePlayerIsDwarf`, `activePlayerIsElf`, `gamePaused`, `pauseGameControl`, `levelLoading`, `setParticle`, `addParticleOnObject`
`setNoCollide`, `shakeCamera`, `deleteObjects`, `hidePlayers`, `checkPlayerAlive`, `setBestActivePlayer`, `decayPlayerLights`, `checkMonstersLeft`
`forceMutexRelease`, `addDeferedCall`, `cutScene`, `postAction`, `newWorldParameter`, `putNPCInVehicle`, `putPlayersInVehicle`, `entityRideVehicle`
`controlVehicle`, `playersLoaded`, `holdVehicle`, `getPlayerVPosition`, `getVPosition`, `getPlayerVDir`, `setupPlayerSel`, `startPlayerSel`
`playerSelRunning`, `objInPlayerTrig`, `setEntrance`, `hudResetFadeScreen`, `hudFadeScreen`, `closestPlayerDist`, `hitPlayers`, `zeroPlayerMana`
`showCompass`, `showBust`, `getMaxPlayerHP`, `getHitPoints`, `setHitPoints`, `saveCheckPoint`, `loadCheckPoint`, `trigger`
`autoPropSetState`, `autoPropGetState`, `preloadResource`, `setSaveInhibit`, `getSaveInhibit`, `countQuestItems`, `countPlayersAlive`, `countLiveObjects`
`networkMode`, `getManaUsed`, `getHealthUsed`, `getLevelTimer`, `generateMonster`, `monstersInTrigger`, `sin1000`, `cos1000`
`helpMessageActive`, `addHelpMessageF`, `revealMap`, `showOnMap`, `initScriptSetParams`, `setCreatureParams`, `explodePotionsOnly`, `runMedalSuccess`
`runMedalFail`, `setAlternateDeath`, `dropItem`, `selectDropItem`, `findDistance`, `setContainerProps`, `setWaveDetail`, `demoRunning`
`removeAllItems`, `savePlayer`, `restorePlayer`, `piercePlayerEar`, `playerAdjustStat`, `giveFullHealth`, `setPlayerFlag`, `getPlayerFlag`
`clearAllSkills`, `isClericOrWizard`, `refillPlayerMana`, `removeAllWeapons`

Beyond those handlers, shared object/name-table integration, selected-object and
active-player machinery, deferred queues, post actions, nested entry-point runs,
missing-entry throttling, level-transition/network gates and object-triggered
script dispatch remain unported here. Other player-record functions, inventory,
creatures, vehicle state, presentation
and their external callbacks remain outside this module. Medal-result UI and
complete reward-script execution remain separate work. Connecting this subset to a
Session, a complete engine command table or gameplay needs separate work.
