# Return to Arms script built-ins: variables, campaign and player records

`champions-script-builtins` is an authored portable C++17 translation of the
quest/profile variable, campaign-state and player-record contracts for **Return to Arms, SLUS_209.73 US v2.00**.
It depends on the existing `champions-amx` and `champions-core` modules and the standard library.
It implements 44 of the 191 table names.

PROGRESS: 75 of 324 reference functions ported

It does not establish Norrath behavior,
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
Sections 1.11 and 3.5 document player records, health/mana, gold, flags and skills.
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

`Variables::natives()` supplies exactly 44 AMX bindings. Keep the instance alive
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

## Player-record vitals, economy, flags and skills (25 new handlers)

| Entry points | Exact behavior |
|---|---|
| `getNumPlayers`, `countPlayersAlive`, `checkPlayerAlive` | Signed global counts; check either positive signed16 respawn countdown or the count of positive raw-float player HP |
| `activePlayerIsDwarf`, `activePlayerIsElf`, `isClericOrWizard` | Unchecked active model lookup; constant zero elf result; unsigned player <4 and player <= signed count for classes 1/4 |
| `getMaxPlayerHP`, `healPlayer`, `giveFullHealth` | One-based bounded maximum-HP conversion; resource/sound calls before living-only refill; unconditional refill |
| `zeroPlayerMana`, `refillPlayerMana` | Zero or copy raw maximum mana in live-count player order |
| `givePlayerExp`, `playerAdjustStat` | Explicit experience/inventory/message callbacks; stat 4..8 only; reward byte captured before repeated calls |
| `givePlayerGold`, `takePlayerGold`, `checkPlayerGold`, `checkPlayerMinGold` | Signed division toward zero; low32 sum/subtraction; ordered uncapped then capped gold stores; zero floor for taking; signed sum/minimum |
| `setPlayerFlag`, `getPlayerFlag`, `piercePlayerEar` | Modulo-32 flag masks with byte stores; -1 captures count for flag writes; unsigned piercing slot accepts -1 or 0..3 |
| `clearAllSkills` | Medal-player byte chooses record; exact 8/1/2/4-byte reset stores in reference order, including 48 skill slots and 15 ability entries |
| `hidePlayers`, `decayPlayerLights`, `getManaUsed`, `getHealthUsed` | Low-byte visibility store; pinned default software R5900 light arithmetic and directional clamp; signed16 usage reads |

`player_service(name, raw32_arguments)` supplies `sound_playOnObject`,
`player_giveExp`, `inventory_onStatPoint`, and `invmsg_sendStatPoint`.
`find_resource(lump, entry)` supplies `res_find`; strings come from the owned
memory at the documented reference identities. Missing callbacks refuse at the
call site; they never simulate successful engine work. Each active callable is
copied before invocation to support replacement and reentry. Exceptions retain
preceding writes. Experience loops reread the argument cells and player count;
stat adjustment captures arguments and reward player first. Numeric handlers
invalidate both shared string slots before reading arguments, even on failure.
Direct flag and HP-count helpers are exposed alongside the existing host helpers.
No object lookup, inventory implementation, audio engine, save/restore or scene
adapter is supplied by this family.

### Reference-function accounting

The generator enumerates all top-level functions and direct class methods in the
pinned AST: 324 units. Only `_main`, the CLI demonstration/harness, is excluded.
Property getters and setters are separate declarations (setters use `@setter`
in the manifest). Constructors, descriptors and host adapters remain in the
denominator; a refused/unimplemented behavior contributes zero to the numerator.
The manifest retains the complete `reference_units` and `ported_units` ledgers.
The 75 fully implemented and tested units are the 44 `Game.n_*` handlers plus:

- `s8`, `s16`, `s32`, through the existing signed conversions exercised by vectors.
- `GameState.rec`, `_lower`, `_var_slot`, `var_get`, `var_set`, `vars_clear_all`,
  `var_by_index`, `profile_get`, `profile_set`.
- `GameState.mission_byte`, `mission_is_available`, `mission_set_available`,
  `mission_is_complete`, `mission_set_complete`, `mission_grant_medal`,
  `mission_get_medal`, `quest_add`, `quest_remove`.
- `GameState.player_set_flag`, `player_get_flag`, `count_players_with_hp` and
  `Game._refill`.
- `GameState._difficulty_bit`, `get_evil`, `_set_choice`, `set_evil`,
  `get_made_decision`, `set_made_decision`.

Other class contracts, including overlay-backed `Mem`, are not counted as complete
merely because this subset owns byte storage or supplies a narrower adapter.
The total is distinct from the 191 built-in table names. The remaining 249
reference units are exactly the manifest ledger difference; among them are 147
unbound table handlers, plus host/object/string/script machinery and wrappers.

## Verification and reproduction

The committed synthetic corpus contains 11,583 state transitions across
67 operations. All previous 7,477 transitions are unchanged byte for byte.
Cases include packed/unpacked strings, ASCII case folding,
empty names, all signed clamp boundaries, signed32 overflow, zero/negative/short/
excess parameter counts, known/unknown rejected addresses, misalignment,
inaccessible arguments, 159/160-byte string boundaries, non-ASCII
refusal, equal-value/no-save profile writes, callback exceptions and callback
mutations. Authored tables span pages and exceed the nominal quest-table area;
those older cases test variable access only, not neighboring services.

Each transition compares complete sparse byte state, ordered writes, known
pointer slots, ordered config-save, reward-script, local-slot and player-service events, the full
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

The Release configure/build and both affected CTests passed on macOS arm64,
including 11,583 complete state transitions. A separately compiled macOS x86_64
executable passed API and exact checks through Rosetta. The two complete traces
are byte-identical, SHA-256
`5cfc289f7280b27e3b35edaa4003da3f6e489b3bfdf825e64dedd62bd4d6e981`.
Independent regeneration reproduced the compressed vectors and manifest byte for
byte; all 7,477 previous transitions remain unchanged. The first wrapper rerun
started during regeneration and saw a stale generator digest; another exposed
the getter/setter ledger naming collision. After regeneration and distinct setter
identities, both affected checks passed.

The x64 validation commands were:

```sh
clang++ -std=c++17 -O2 -arch x86_64 -I native/include native/src/script_builtins.cpp native/src/core_arithmetic.cpp native/src/amx_image.cpp native/src/amx_host.cpp native/src/amx_execute.cpp native/tests/script_builtins_tests.cpp -o work/script-builtins-x86_64
work/script-builtins-x86_64 --api
python3 tools/test_script_builtins_vectors.py --native work/script-builtins-x86_64 --vectors native/tests/vectors/script_builtins.tsv.gz --manifest native/tests/vectors/script_builtins.json --trace-dir build/native/script-builtins-x64 --source-commit 41a2801 --platform Darwin-x86_64
cmp build/native/core-traces/script-builtins-actual.tsv build/native/script-builtins-x64/script-builtins-actual.tsv
```

The full Release `ctest --test-dir build/native --output-on-failure -j 3`
run completed in 2,104.28 seconds: 99/102 passed. Its three failures were the
pre-fix getter/setter accounting check and timeout-only failures in
`objects-runtime-transport` and `session-objects-transport`. The corrected
built-ins API/exact rerun passed with the final 11,583-transition corpus.
Isolated `objects-runtime-transport` passed in 78.36 seconds and isolated
`session-objects-transport` passed in 342.79 seconds. All 102 registered tests
therefore passed across the full run and successful reruns; the initial parallel
run itself retained three failures.

```sh
ctest --test-dir build/native -R '^objects-runtime-transport$' --output-on-failure
ctest --test-dir build/native -R '^session-objects-transport$' --output-on-failure
```

Windows execution, private game acceptance, original-code differential reruns and
Godot/Session integration remain unverified. The research checkout stayed clean
and read-only.

## Exact remaining scope

The manifest's `unsupported_natives` records all 147 unported names at the pin.
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
`playGotItemSound`, `setInfo`, `addHelpMessage`, `hornOmen`, `planarSteel`, `particleMorph`, `leaveGame`, `endGame`
`clearRecall`, `startMusic`, `startAudio`, `endMusic`, `saveGame`, `saveEndConfig`, `saveDone`, `exitToMainMenu`
`reapEnemies`, `getRand`, `gamePaused`, `pauseGameControl`, `levelLoading`, `setParticle`, `addParticleOnObject`, `setNoCollide`
`shakeCamera`, `deleteObjects`, `setBestActivePlayer`, `checkMonstersLeft`, `forceMutexRelease`, `addDeferedCall`, `cutScene`, `postAction`
`newWorldParameter`, `putNPCInVehicle`, `putPlayersInVehicle`, `entityRideVehicle`, `controlVehicle`, `playersLoaded`, `holdVehicle`, `getPlayerVPosition`
`getVPosition`, `getPlayerVDir`, `setupPlayerSel`, `startPlayerSel`, `playerSelRunning`, `objInPlayerTrig`, `setEntrance`, `hudResetFadeScreen`
`hudFadeScreen`, `closestPlayerDist`, `hitPlayers`, `showCompass`, `showBust`, `getHitPoints`, `setHitPoints`, `saveCheckPoint`
`loadCheckPoint`, `trigger`, `autoPropSetState`, `autoPropGetState`, `preloadResource`, `setSaveInhibit`, `getSaveInhibit`, `countQuestItems`
`countLiveObjects`, `networkMode`, `getLevelTimer`, `generateMonster`, `monstersInTrigger`, `sin1000`, `cos1000`, `helpMessageActive`
`addHelpMessageF`, `revealMap`, `showOnMap`, `initScriptSetParams`, `setCreatureParams`, `explodePotionsOnly`, `runMedalSuccess`, `runMedalFail`
`setAlternateDeath`, `dropItem`, `selectDropItem`, `findDistance`, `setContainerProps`, `setWaveDetail`, `demoRunning`, `removeAllItems`
`savePlayer`, `restorePlayer`, `removeAllWeapons`

Beyond those handlers, shared object/name-table integration, selected-object and
active-player machinery, deferred queues, post actions, nested entry-point runs,
missing-entry throttling, level-transition/network gates and object-triggered
script dispatch remain unported here. Player snapshot save/restore, object-dependent player functions, inventory,
creatures, vehicle state, presentation
and their external callbacks remain outside this module. Medal-result UI and
complete reward-script execution remain separate work. Connecting this subset to a
Session, a complete engine command table or gameplay needs separate work.
