# Return to Arms script built-ins: records, scene lifecycle, HUD, creatures, inventory, effects, vehicles, player interactions, scene utilities, audio and script control

`champions-script-builtins` is an authored portable C++17 translation of the
quest/profile variable, campaign-state, player-record, scene-lifecycle, menu/HUD, creature, inventory, object/effect/camera, vehicle, complete player-interaction, scene-utility, audio, script-control, sparse-memory, global-descriptor, memory-backed-object, call-descriptor, Host call-out, direct argument, native-binding, state-construction/setup and arbitrary-width signed-division contracts for **Return to Arms, SLUS_209.73 US v2.00**.
It depends on the existing `champions-amx`, `champions-core` and `champions-objects` modules and the standard library.
It implements all 191 table names.

Latest family: **signed truncating division**.

PROGRESS: 324 of 324 reference functions ported

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
Sections 1.10 and 3.3 establish scene transitions, dialog, saving and control gates.
Section 3.4 establishes the complete menu/HUD/medal-result family. Section 1.3
and pinned `ref_objects.NameTable` establish object lookup order. Section 3.7
establishes the complete 36-handler creature family. Section 3.6 and the pinned
reference establish the complete inventory/item-grant family. Section 3.8 and the pinned
reference establish the complete 19-handler other-object/effects/camera family.
Section 3.10 establishes the complete eight-handler vehicle family.
Sections 3.1 and 3.9 establish scene utilities and audio requests. Sections
1.6, 1.7 and 1.8 and the pinned reference establish deferred queues, post actions
and nested entry-point execution.
Pinned `ref_objects.props_find` documents case-sensitive property lookup with
ASCII lowering of the lookup key.
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

`Variables::natives()` supplies exactly 191 AMX bindings. Keep the instance alive
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
`StackGarbage`. `run_script` manages fresh/nested frames automatically.
`forget_string_slot()` explicitly starts an unknown slot when a direct host caller
crosses an external stack boundary that the module cannot track.

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
the newer direct `run_script` API can supply that execution when explicitly wired
by the owner. This existing callback boundary is unchanged. Historical vectors substitute this
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

## Scene lifecycle family (27 new table names, 30 reference units)

This run implements every handler in reference section 3.3, using section 1.10
for transition helpers and the pinned `dialog_start` packet contract:

| Entry points | Behavior |
|---|---|
| `newWorld`, `newWorldAlways`, `simpleNewWorld` | Shared network cooldown/HUD gate unless in a post action; close menus before writing the pending level; simple form stores orientation first |
| `newWorldPost`, `newWorldParameter`, `setEntrance` | Post-dialog/entrance strings; first case-sensitive `level=` property, then last matching `orientation=` value |
| `cutScene`, `startDialog`, `dialogRunning`, `forceMutexRelease` | Explicit resource, camera, dialog, packet-send and menu callbacks |
| `changeSwoop`, `startWithSwoopDone`, `leaveGame`, `exitToMainMenu` | Exact owned global writes |
| `endGame`, `saveGame`, `saveEndConfig`, `saveDone` | Explicit card/lobby services, player-status guard, ordered campaign completion writes and difficulty bits |
| `saveCheckPoint`, `loadCheckPoint`, `setSaveInhibit`, `getSaveInhibit`, `setAlternateDeath` | Callback requests and documented globals |
| `preloadResource`, `clearRecall`, `playersLoaded`, `pauseGameControl` | Explicit lump callback; live-count record traversal; exact lockout stores |

`scene_service(name, values)` carries raw low32 integers and arbitrary owned byte
strings, including the complete dialog packet. `find_resource`,
`local_player_slot` and `request_config_save` reuse the existing callbacks.
Missing callbacks refuse at the exact call site; no load, dialog, save or network
success is fabricated. Callable copies allow replacement and reentry. Argument
reads, live global reads and write/callback order follow the reference, including
mutations after camera update or menu close. Exceptions preserve preceding writes.

`object_properties` is an owned address-to-property-list projection for these
transitions only. Zero selected address means no object; unknown addresses and
null property lists fault separately. It does not implement the object world,
name table or memory-backed object constructors. `newWorldParameter` retains its
original property iteration list across callbacks, while `set_next_world` reads
the live selected address/list. Orientation uses decimal parsing modulo 32 bits
and a byte store, including repeated matches and long synthetic digit strings.

World helper buffer refusals preserve the reference's write prefixes: names of
112 bytes refuse before copying; absent orientation with a name of 95 bytes or
more refuses after the pending string write. Simple transitions have no such
helper limit. Long strings can overwrite neighboring owned identities; no field
capacity is invented. Dialog messages retain name bytes, 0xff alignment padding,
low-byte parameters, twelve raw camera bytes and little-endian low16 yaw/pitch.

The family retains slots 0x20, 0x80 (`startDialog`) and 0xd0 (`cutScene`) independently,
with all other handler slots invalidated before argument access. The direct
`new_world_no_gate` helper has no dispatcher invalidation, matching the reference.
Host helpers `net_gate`, `set_next_world` and `dialog_start` are exposed directly.
The alias counts as one additional table name and zero additional AST functions.

## Menus, HUD and medal results (18 new handlers, 25 reference units)

All handlers in pinned section 3.4 are implemented:

| Entry points | Exact behavior |
|---|---|
| `activateMissionMenu`, `activateStore`, `storeRunning` | Dialog/recall gates; offline active slot or network slot lookup; store <21 unsigned writes low16 before dialog query; live alive-list/count rereads after menu callbacks |
| `setInfo`, `addHelpMessage`, `addHelpMessageF`, `helpMessageActive` | Ordered string conversion and lookup; raw R5900 signed-integer-to-float formatting arguments; both symbolic `stack` format calls precede static-buffer copies; signed help byte |
| `showCompass`, `showBust` | First named object's live raw position after text lookup (missing object refuses); bust arguments read after lookup |
| `hudResetFadeScreen`, `hudFadeScreen`, `revealMap`, `showOnMap` | Explicit fade/reveal callbacks; fade returns the service word; map writes bit 43 with one eight-byte store for each creature among the first 99 name matches |
| `runMedalSuccess`, `runMedalFail` | Shared card/network/ack gates; success measures title/reward lines then draws lines in order 3, 2, 1; signed32 wrapping totals and truncating division; explicit formatting/drawing callbacks |
| `setupPlayerSel`, `startPlayerSel`, `playerSelRunning` | The first two return zero exactly as the contract; the third returns its explicit service result |

`medal_gate`, `medal_prompt` and `medal_pad` expose the complete shared helpers.
Every gate stores ticks=2; offline it stores paused=1 even before a busy-card
return. Local network players wait for card readiness and acknowledge; nonlocal
players with acknowledge may skip drawing while busy but still test the pad.
Confirm wins over cancel, copies the current level and writes orientation=255;
cancel calls `game_openMissionMenu`. Drawing callbacks can mutate state before
the pad read. The success handler converts all four strings with a 128-byte
frame even when a reward line is hidden. Other family strings use 160 bytes.

`scene_service` is also the explicit presentation/menu/card seam. Its values are
raw low32 words, owned byte strings, or a three-word array for compass position;
float arguments remain raw R5900 bit patterns. The string `stack` is the pinned
model's symbolic formatting buffer, not actual host memory. Text lookup, wide
formatting/copying, rendering, menu, card and network services are delegated;
missing callbacks raise `HostFault` at the original call site with preceding
writes retained. Return values are ignored or read exactly where specified.
`local_player_slot` remains the separate `net_getLocalPlayerSlot(0)` seam.
All callable bindings are copied before invocation, allowing reentry and replacement.

`object_names` reuses the existing exact `objects::NameTable`: its 512 probe
slots, case-insensitive byte lookup, insertion quote stripping, duplicate matches,
tombstones and limit-minus-one ordering are preserved. `named_objects` can own
stable identities; callers supplying pointers to the name table must keep their
objects alive. This family reads flags/positions/creature flags live from
`Variables::memory`, not the independent object's cached fields. `find`,
`find_first`, `need` and `alive` are exposed helpers. No grid, scene object
creation, selected-object resolver or complete object runtime integration is implied.

The new handler slots are 0x40 (bust), 0x50 (compass), 0x70 (success), 0xc0
(failure), 0x1c0 (map), and 0x440 (formatted help). `setInfo` shares existing 0x20;
`addHelpMessage` shares existing 0x30. Entry invalidates every unrelated slot,
including slots introduced by this family when entering older handlers.
`forget_string_slot()` clears every tracked slot. Refused addresses preserve a
known slot; conversion uses the current AMX bytes, and unknown slots refuse.

## Creature family (36 new handlers)

This run ports all of specification section 3.7, from `increaseEnemyRank` through
`countLiveObjects` in the reference's creature section. The added bindings are:

`increaseEnemyRank`, `charmMonster`, `rootMonster`, `unRootMonster`, `selectSetMonsterVal`, `setMonsterVal`, `selectSetCFlag`, `setCFlag`, `selectSetOFlag`, `setOFlag`, `setWeaponType`, `setMonsterRange`, `killMonster`, `deleteMonster`, `hideMonster`, `showMonster`, `moveMonster`, `followPlayer`, `stopFollowingPlayer`, `followPath`, `selectFollowPath`, `stopFollowingPath`, `selectStopPath`, `friendly`, `unFriendly`, `reapEnemies`, `checkMonstersLeft`, `getHitPoints`, `setHitPoints`, `generateMonster`, `monstersInTrigger`, `initScriptSetParams`, `setCreatureParams`, `startNPCAnim`, `NPCAnimAtEnd`, `countLiveObjects`.

Object flags, 64-bit creature flags, position, facing, root/death state, health,
monster-value fields, path radius/home, and NPC end bits use the same owned
`Memory`. All stores retain their original widths and ordering. Names use the
existing name table with the original 511-match creature and 127-match live-count
limits; selected calls resolve the raw identity at 0x4039f8 through `obj_at`.
Zero resolves to absent; an unknown nonzero identity refuses. Register objects in
`named_objects` even when they have no name, and keep them alive for the full
call. `GameObject::type_name` supplies exact class identities for generators,
monster triggers and the documented pointer-type branch of `reapEnemies`.
Base `GameObject` fields are not the source of flags/position; bytes are read live
from this module's memory. This is an explicit registry projection, not a complete
World/Session adapter or a port of overlay-backed `Obj`/`Mem` constructors.

`scene_service` supplies virtual calls (slot first, then raw object identity),
charm/music/grid-link operations, spawning, trigger counts, stat scaling and
animation services. `find_resource` resolves each NPC resource separately.
`object_query` supplies ordered object pointers for the box and grid enumerations,
with the exact raw box vectors/limit or grid callback identity. No grid membership,
destruction, spawning, AI, animation playback or default virtual result is
fabricated. Missing services raise `HostFault` at their call sites, retaining the
reference write prefix. Supplied callbacks may throw, reenter helpers or replace
themselves; calls retain the active callable and subsequent reads use live state.

Health conversion and the five percentage stat factors use the existing exact
raw-bit `Fpu` arithmetic, without host floating-point conversion. Generation
interns case-sensitive class strings in the 24-byte-stride table; signed counts,
count overflow, copies longer than an entry, and count-before-string write order
remain as documented. NPC mode -1 clears before resolving the resource and then
pushes/jumps/releases; other modes resolve first and start with raw 0.3 blend.

The new shared pointer slots are 0x220, 0x820, 0x830, 0x840 and 0x850;
`setHitPoints` and `NPCAnimAtEnd` share the existing 0x20. Multiple strings in a
handler share its slot, so a rejected later argument can reuse the most recently
accepted address. Numeric and older handlers also invalidate the new slots.

## Vehicle family (8 handlers, 9 reference units)

The complete section 3.10 family preserves the pinned contract, including its
unchecked slots and unusual field selection. The specification records no shipped
script calls to these handlers; this port is contract support, not gameplay proof.

| Entry points | Behavior |
|---|---|
| `putNPCInVehicle` | Convert both names first; attach the first NPC or zero only if the first vehicle exists |
| `putPlayersInVehicle` | Virtual slot 14 for the active slot unless -1, even if dead; then other living players in order; null/stale player identities refuse; count and active slot are reread |
| `entityRideVehicle` | Require vehicle, then entity; ask virtual seat slot 19 for position/facing; write three raw position words, then low16 facing plus the live angle argument; update grid last |
| `controlVehicle`, `holdVehicle` | Raw +0x410 and signed-int-to-float +0x2f0 writes; explicit hold service with raw on argument |
| `getPlayerVPosition`, `getVPosition` | Compare vehicle +0x90c but retain +0x34c across living players; named query uses +0x90c; software multiplication by 128 and truncating integer conversion |
| `getPlayerVDir` | Alive-list order; living player's +0x540 vehicle; strict +0x348 > 2 threshold; ordered software dot products with negated raw +0x468 vector and path-pointer +0xc vector; return 1, -1 or zero |
| `vehicle_of` | Direct helper: return zero for null/dead player, otherwise the raw vehicle word; stale player identities refuse |

Existing `scene_service` carries `Vehicle_attachNpcDriver`, `Vehicle_setHold`,
virtual slot 14 and `obj_updateGridCell`. The typed `vehicle_seat(vehicle, offset)`
callback represents virtual slot 19 and returns three raw position words and
integer facing. Its callable is retained across replacement/reentry. Missing
services refuse at the exact call boundary, preserving prior writes. No seat
geometry, vehicle engine or grid behavior is fabricated.

String slots follow the pin: shared 0x30 for NPC attachment, 0x40 for player
attachment, shared 0x70 for both seat names, and shared 0x20 for control, hold
and named position. Numeric queries invalidate all slots. The existing 160-byte
string buffer refusal and stale-pointer behavior remain intact.

### Reference-function accounting

The generator enumerates all top-level functions and direct class methods in the
pinned AST: 324 units. Only `_main`, the CLI demonstration/harness, is excluded.
Property getters and setters are separate declarations (setters use `@setter`
in the manifest). Constructors, descriptors and host adapters remain in the
denominator; a refused/unimplemented behavior contributes zero to the numerator.
The manifest retains the complete `reference_units` and `ported_units` ledgers.
The 261 fully implemented and tested units are 190 distinct `Game.n_*` handlers
(`newWorldAlways` aliases `newWorld`) plus:

- `GameState.queue_deferred`, `run_deferred`, `clear_deferred`, `reset_script_state`,
  `set_post_action`, `run_post_action`, `set_script` and `run_script`.
- `Game._distance3`, `_spell_effect`, `GameState.save_player_record` and
  `GameState.restore_player_record`.
- `Game._vehicle_of`.
- `Game._particle_def`.
- `Game._empty_bag`, `_drop_at`, `_give`, `_give_or_drop`, `GameState.player_obj`,
  and `GameState.rand`.
- `Game._creatures`, `apply_monster_val`, `apply_cflag`, `apply_oflag`,
  `_follow_path`, `_stop_path`, `_scale`, `_start_anim`, and `GameState.obj_at`.
- `Game._medal_gate`, `_medal_prompt`, `_medal_pad`, and `GameState.alive`,
  `find`, `find_first`, `need`.
- `GameState.net_gate`, `set_next_world`, `dialog_start` and `Game.new_world_no_gate`.
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
The total is distinct from the 191 built-in table names. The remaining 63
reference units are exactly the manifest ledger difference: memory, object, host,
argument and binding infrastructure, with no unbound table handlers.

## Inventory, item grants and ground drops (11 handlers, 17 reference units)

The complete section 3.6 family is now bound: `playerHasItem`, `onePlayerHasItem`,
`countQuestItems`, `removePlayerItem`, `removeAllItems`, `removeAllWeapons`,
`givePlayerItem`, `giveOnePlayerItem`, `givePlayerRandom`, `dropItem`, and
`selectDropItem`. The shared helpers `empty_bag`, `drop_at`, `give`,
`give_or_drop`, `player_object`, and `random_draw` port their six reference units.

The explicit `inventory_service` callback carries raw low32 words, owned byte
strings, three raw position words, or `ItemBlank`. The latter preserves exactly
the initialized portions of the model's temporary record: sixteen zero bytes,
two 0xff bytes and four 0xffff halfwords. `inventory_findItemBySubstring` returns
an index/bag pair; other services consumed as results return a word. Inventory
contents, descriptor parsing, item generation, weight calculation, equipment,
load recalculation, idle animation and ground-item allocation/construction
remain delegated. A missing callback refuses at its original call site; preceding
stores and draws remain. Callables are copied before invocation for replacement
and reentry. No inventory or item-generator implementation is fabricated.

The implementation preserves signed player indices (including negatives),
live player-count and medal-filter rereads, signed32 count accumulation, object
and death gates before optional string conversion, and the index -1 removal
sentinel. Bag emptying captures its count once per bag but checks entry zero
before each removal. Weapon removal clears its three equipped pointers only
after successful bag processing. Descriptor grants capture variant/equip at the
reference's argument-read sites and retain the signed definition bag and signed
inventory index when forming the wrapped entry address.

Capacity arithmetic uses the existing raw-bit R5900 `Fpu`, with strict `<`:
`givePlayerItem` additionally permits zero weight; `giveOnePlayerItem` does not.
Failed allocation skips position evaluation entirely. Nonzero allocation reads
the player's current object, or the live first alive-list entry when that
fallback was selected before allocation. Missing/stale identities refuse.
Named and selected ground drops read raw positions from the same owned memory.

`givePlayerRandom` stores the shared seed at 0x404464 for each draw, increments
`draws`, and performs 2 through 11 draws per player. It computes budget and level
through the exact raw-bit arithmetic, retains the special-level substring test,
resolves fixed descriptors from supplied memory only for difficulty 1 through 3,
and makes at most ten generation attempts. This follows the pinned Python model's
symbolic temporary-item boundary; the specification's original stack-record
bytes beyond `ItemBlank` are not a new owned item representation.

String slots are 0x20 for one-player/quest counts, 0x30 for total counts/removal,
0x40 for selected drops, 0x60 for named drops, 0xb0 for single-player grants and
0xd0 for all-player grants. They share the existing slots and invalidation rules,
including reuse after a refused address. The 160-byte frame boundary remains
an explicit refusal as in the reference.

## Other objects, effects and camera (19 handlers, 20 reference units)

This family implements all of section 3.8 and `Game._particle_def`:

| Entry points | Exact behavior |
|---|---|
| `trigger`, `autoPropSetState`, `autoPropGetState` | Ordered virtual calls on up to 99 named objects; auto-prop flag filtering and first-object signed state read |
| `startPropAnim`, `stopPropAnim`, `propAnimAtEnd` | State at object +0x54; first case-sensitive `lump=` property; clear/resource/push/end/cache ordering for mode -1; byte stop and halfword end bit |
| `deleteObjects`, `setNoCollide` | Destroy virtual slot 1 with 3 on up to 24 objects; first-object flag bit 0 |
| `objInPlayerTrig`, `findDistance`, `setContainerProps` | Explicit trigger-class query and contains virtual; horizontal software-float distance; exact AutoContainer class and bit 3 updates on up to 511 objects |
| `lightEffect`, `addExpandingPool`, `particleMorph` | Exact allocation sizes, argument conversions and constructor/decal callbacks; zero allocation skips construction; morph examines only each list's first object |
| `setParticle`, `addParticleOnObject` | Three documented definition identities; exact integer/float argument order, position offsets and velocity ×0x3a83126e; spawn slot -1 aliases 0 |
| `shakeCamera`, `setFarClip`, `setWaveDetail` | Medal-countdown camera gate, converted far distance, and normalized wave-detail word |

`scene_service` carries virtual, allocation, animation, effect, particle and camera
requests. `object_query` supplies `world_findObjectsByClass("7Trigger", 50)`;
its callback owns the original class enumeration and limit. `find_resource`
resolves prop animation. These existing callbacks remain required at the exact
call site; absence refuses without fabricated engine effects. Active callables
are copied before invocation, supporting replacement and reentry. Named lists
are captured before iteration, while flags and argument cells are read at the
reference's live sites after preceding callbacks.

Prop animation uses the existing owned `object_properties` projection. A null or
missing list has no property; nonnull lists first write the lowercased lookup key
back to its memory identity, with no terminator write. Missing `lump=` faults
thereafter, preserving any preceding animation clear and key write. The first
matching property wins, including an empty value. Property keys themselves are
case-sensitive. No resource content or engine animation execution is implied.

String/pointer slots reuse 0x20, 0x30, 0x40, 0x50, 0xa0, 0x110, 0x1c0,
0x1d0 and 0x840. `particleMorph` uniquely retains both 0x10f0 and 0x840;
`lightEffect` uses its 0x40 slot for a position-cell pointer. Accepted addresses,
refused-pointer reuse, unrelated-slot invalidation and 160-byte string refusals
match the reference. Particle indices outside 0..2 raise the pinned model's
explicit `HostFault`; this does not assert a bounds check in the retail handler.
All arithmetic uses the existing default software R5900 `Fpu`, with no host-float
or Godot dependency.

## Verification and reproduction

The committed synthetic corpus contains 78,002 state transitions across
200 operations. All previous 71,017 transitions are retained byte for byte.
Cases include packed/unpacked strings, ASCII case folding,
empty names, all signed clamp boundaries, signed32 overflow, zero/negative/short/
excess parameter counts, known/unknown rejected addresses, misalignment,
inaccessible arguments, 159/160-byte string boundaries, non-ASCII
refusal, equal-value/no-save profile writes, callback exceptions and callback
mutations. Authored tables span pages and exceed the nominal quest-table area;
those older cases test variable access only, not neighboring services.

Each transition compares complete sparse byte state, ordered writes, known
pointer slots, owned selected-object property projections (scene cases only),
the complete object name table (menu and creature cases), class identities
(creature cases),
ordered config-save, reward-script, local-slot, player-service and scene-service
events, the full
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

### Menu/HUD verification

The Release configure/build and both affected CTests passed on macOS arm64:
`script-builtins-api` and `script-builtins-exact` (32,384 complete transitions,
32.00 seconds together). The final full Release suite passed **102/102 tests in
683.34 seconds**, with no timeouts or reruns.
Independent regeneration reproduced both compressed vectors and manifest byte
for byte. All 13,381 previous transitions are an exact byte prefix. Independent
At that stage, AST enumeration confirmed 324 declarations with 130 ported
units and 102 remaining table names. The current ledger supersedes those counts.

New cases include all offline/network/local/card/ack/pad combinations; confirm
priority; title/line measurement, signed height/total overflow, reverse reward
line order, 128-byte medal frames and 160-byte other frames; callback failures
at successive services; live slot/count/list/position mutations; formatting call
order and raw float arguments; store unsigned bounds; all shared and new string
slots; signed help bytes, raw service results, duplicate names, tombstones,
quote stripping and the 99-match map limit. API cases also cover missing services
and write prefixes, callable replacement/reentry, name-query limit refusal,
null-object refusal and slot invalidation before inaccessible arguments.

The separate x64 Release build and both affected tests also passed under Rosetta
(61.95 seconds). `cmp` verified byte-identical complete arm64/x64 traces, SHA-256
`252e1c0367a2bc5512397c5818d1525beb9f2523162229b78d77bdc32521e703`. These are macOS measurements, not Windows execution.
The x64 commands are:

```sh
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
```

Windows execution, private game acceptance, original-code differential reruns,
actual text/render/menu/card services and Godot/Session integration remain
unverified. The research checkout stayed clean and read-only.

### Creature-family verification

At completion of the creature family, the synthetic corpus had **47,013 transitions across 164 operations**.
All 32,384 historical transitions form an exact byte prefix. Independent
regeneration reproduced both compressed vectors and the manifest byte for byte;
independent AST enumeration confirms 324 units, with 175 ported and 149 remaining.
At that stage the manifest retained 66 remaining table names; the inventory ledger
above supersedes those counts.

Both affected Release CTests passed on macOS arm64 (38.60 seconds) and x64
under Rosetta (81.89 seconds). `cmp` confirmed byte-identical complete traces,
SHA-256 `7a957aba72630af15423928a8bf38baf9f4620eadb87bcca90e7ad5f8ddfa6d6`.
Cases cover mixed creature/non-creature/dead/player flags; selected null/stale
identities; every monster-value selector; low-byte/halfword truncation; 64-bit
flag preservation; boss-meter write prefixes and live callback mutations;
root/death/path transitions; target reads per movement iteration; raw HP
patterns; signed generator counts, class interning and copies beyond a table
entry; callback exceptions/replacement; NPC clear/resource/start ordering;
pointer-type reap branches; and 512-slot name-table limits/tombstones.

An initial full-suite attempt encountered `No space left on device` and was
stopped. Task-owned large traces were removed, repetitive setup was reduced to
stateful boundary sequences, and the verifier now removes its owned temporary
decoded input after execution while retaining the actual trace. The final full
Release rerun passed **102/102 tests in 338.83 seconds**, with no timeouts
or test reruns. The earlier disk failures were infrastructure failures, not
C++ contract mismatches.

Windows execution, private game acceptance, original-code differential reruns,
real AI/grid/spawn/animation services, Godot/Session integration and Norrath
remain unverified. All public inputs are synthetic; the research checkout was
read-only throughout.

Additional reproducibility commands (all passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re --output work/script-builtins-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.tsv.gz work/script-builtins-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.json work/script-builtins-reproduced.json
```

### Inventory-family verification

Both affected Release CTests passed on macOS arm64 (49.18 seconds) and x64
under Rosetta (88.18 seconds on the final rebuild). The final callable-retention
regression also passed the arm64 API test before the full-suite run. The complete 71,017-transition traces match
byte for byte, SHA-256
`d302633ed56b30924fb093cf455ef1e6020b010828e71f2c4f074ae6b1785c39`.
All 47,013 historical transitions form an unchanged byte prefix. Independent
regeneration reproduced both compressed vectors and the manifest byte for byte.
At that stage, independent AST enumeration confirmed 324 reference units, with
192 ported, 132 remaining units and 55 unbound table handlers. The current
object/effects/camera ledger supersedes those counts.

The final full Release suite passed **102/102 tests in 334.85 seconds**,
including the rebuilt built-ins exact test (55.60 seconds), with no timeouts
or test reruns. Final arm64/x64 trace comparison again passed after the full suite.

New synthetic cases cover negative and out-of-range player slots, dead/null/stale
objects, overflowed item-count totals, bag count capture and live entry rereads,
index -1 and signed bag/index address formation, raw weight/capacity patterns,
zero-weight exception versus strict equality, medal filtering, equipment/reset
ordering, zero/nonzero allocations, fallback positions, shared-slot reuse and
160-byte refusals, callback exceptions/mutations/replacement/reentry, every
special-level difficulty, shared-seed wrapping, 1/10-attempt generation, and
missing-service write/draw prefixes. An initial build found a test-local variable
name collision and the first API check found the old 125-binding assertion;
both were corrected before the successful checks.

Windows execution, private game acceptance, original-code differential reruns,
actual inventory/item/ground-prop services, Godot/Session integration and Norrath
remain unverified. Public fixtures contain authored synthetic data only.
The research checkout was not edited.

Reproduction and comparison commands (all passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re --output work/script-builtins-items-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.tsv.gz work/script-builtins-items-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.json work/script-builtins-items-reproduced.json
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
```

### Other-object/effects/camera verification

Release configure/build and both affected CTests passed on macOS arm64
(73.08 seconds) and x64 under Rosetta (102.56 seconds). The complete
78,002-transition traces match byte for byte. `file` confirmed that the executed
binaries are Mach-O arm64 and x86_64 respectively; the Python wrapper metadata
reports its Darwin-arm64 host for both. Trace SHA-256:
`d36400f16e71cc4e674dbe04fdbcef5c83ddcb7c042a3efd2aea8dc28c0b7cc9`.
All 71,017 historical transitions are an unchanged 1,165,631,615-byte vector
prefix. Independent regeneration reproduced the compressed vectors and manifest
byte for byte. Independent AST enumeration confirms 324 units, 212 fully ported
and tested, 112 remaining, and 36 unbound table names. The new family adds 19
handlers and one helper; no refusal replaces supported reference behavior.

The final full Release suite passed **102/102 tests in 367.39 seconds**,
including the built-ins exact check (60.00 seconds), with no failures, timeouts
or reruns. The arm64/x64 trace comparison remained byte-identical.

New synthetic cases cover object-class and flag filtering; 24/99/511-object
lookup limits; first-object rather than later-creature selection; null/missing
props, first/empty/capitalized lump keys and exact key-write prefixes; animation
modes and exception ordering; horizontal raw-float distance patterns; container
selectors and collision flags; position-cell conversion and negative-pointer
failure before allocation; zero/nonzero allocation; all valid and invalid
particle slots; raw argument order and velocity scaling; countdown gating;
independent/shared pointer-slot reuse and string boundaries; query early returns;
callback exceptions, mutations, replacement and reentry.

Windows execution, original-code differential reruns, actual effect/camera/
animation/query services, Godot/Session integration and Norrath remain unverified.
Private acceptance measurements were skipped: `../../private-validation` is
unavailable on this machine, as specified by the task. No alternate private-input
location was searched. All committed inputs are synthetic, and the research
checkout was not edited.

Additional reproduction and architecture comparison commands (all passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re --output work/script-builtins-effects-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.tsv.gz work/script-builtins-effects-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.json work/script-builtins-effects-reproduced.json
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
```

### Vehicle-family verification

The corpus now contains **79,494 transitions across 210 operations**. All
78,002 historical transitions remain an exact 1,323,181,984-byte prefix.
Independent regeneration reproduces compressed vectors and the manifest byte
for byte. Independent AST enumeration confirms 324 reference units, 221 fully
ported and tested, 103 remaining, and 28 unbound table names. The nine new units
are eight handlers plus `_vehicle_of`; refusals of delegated host work do not
substitute for any reference-owned behavior.

Both affected Release CTests passed on macOS arm64 (75.90 seconds) and x64 under
Rosetta (123.72 seconds). The final additional API cases also passed on both
architectures. The complete arm64/x64 traces match byte for byte, SHA-256
`2428f50975ff6b632da941dbac739878a65d983ca69a1463623980c85361bbe7`.
`file` confirms Mach-O arm64 and x86_64 executed binaries.
Synthetic checks cover active-first/live-count interaction order, dead/null/stale
players, missing vehicles/NPCs, integer conversion and low16 facing overflow,
raw position-selection and direction thresholds, three-term cancellation,
alive-list skipping, packed/unpacked strings, shared-slot reuse, 159/160-byte
boundaries, missing services, callback exceptions/mutation/replacement/reentry,
and retained entity writes after grid failure.

Commands (all passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
ctest --test-dir build/native-x64 -R '^script-builtins-api$' --output-on-failure
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re --output work/script-builtins-vehicles-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.tsv.gz work/script-builtins-vehicles-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.json work/script-builtins-vehicles-reproduced.json
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
```

The final full Release run passed **102/102 tests in 357.70 seconds**, including
the built-ins exact check (76.43 seconds). No failures, timeouts or reruns were
needed. The final full-suite arm64 trace still matches the x64 trace byte for byte.

```sh
ctest --test-dir build/native --output-on-failure -j 3 > work/vehicle-full-ctest.log 2>&1
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Private acceptance measurements were skipped because `../../private-validation`
is unavailable on this machine; no alternate location was searched. Windows,
original-code differential reruns, actual vehicle/seat/grid services, Norrath and
Godot/Session integration remain unverified. The research checkout stayed clean
and was never edited. All public inputs are synthetic.

## Remaining player interactions, spell effects and record snapshots (nine handlers)

This run completes the remaining section 3.5 player family using the pinned
reference and specification sections 1.11 and 3.5. It adds all nine handlers and
four reference helpers (`_distance3`, `_spell_effect`, `save_player_record`,
`restore_player_record`). The earlier 25 player handlers retain their fixtures.

| Entry points | Exact behavior |
|---|---|
| `setBestActivePlayer` | Stores/returns 1 only when player 0 has no object or a dead object and the player count is exactly two; otherwise 0 |
| `closestPlayerDist` | Nearest listed nonnull player in 3D; raw ordered arithmetic and integer conversion; 100000 when none qualify |
| `hitPlayers` | Live-count alive-list traversal; strict radius; virtual damage slot 2 with no attacker, then reread player identity and push argument before raw speed and low16 direction writes |
| `hornOmen`, `planarSteel` | Live-count per-player allocations and spell callbacks; planar steel heals positive-HP records first; zero allocations still reach `creature_addSpell` |
| `playGotItemSound` | Stores the active-slot clamp but checks HP using its original value; conditionally switches voice recipient and calls voice 9 |
| `explodePotionsOnly` | Stores word 1 at 0x4f7128 |
| `savePlayer`, `save_player_record` | Capture medal slot; request copy identity; copy all 0x4c30 bytes after the callback; set saved flag |
| `restorePlayer`, `restore_player_record` | Capture slot, record and five live byte ranges before copy callback; copy full record, set respawn countdown, restore ranges in order, set four descending death countdowns, request alive-list rebuild, then clear saved flag |

The restore ranges are medals +0x4c0d (8 bytes), +0x4c15 (4), +0x4c19 (1),
+0x78 (24), and object pointer +0x43d8 (4). Reads are owned snapshots before each
bulk write, preserving overlap and low32 wrapping without host pointers.
`distance3` exposes distance/dx/dy raw words using the existing tested console
arithmetic; `spell_effect` exposes the shared constructor/size/heal body.

All delegated work uses the existing `scene_service` callback: `virtual` receives
slot 2, object, zero attacker lanes and raw damage; other services are `pool_new`,
`SpellHornOmenEffect_ctor`, `SpellPlanarSteelEffect_ctor`, `creature_addSpell`,
`player_voice`, `save_playerCopy` and `game_rebuildAlivePlayers`. The copy service
returns an owned memory address. Rebuilding the alive list is host-owned here,
exactly as the reference delegates it; no automatic host default is invented.
A missing service refuses at the call site with preceding writes retained.
Callback callable copies support self-replacement and reentry. Exceptions retain
all completed stores and prevent subsequent work.

`closestPlayerDist` shares slot 0x20; `hitPlayers` shares slot 0x60 with `dropItem`.
Both preserve refused-pointer reuse and the 160-byte conversion boundary. Missing
or stale required objects explicitly fault, while documented null-skipping and
empty-list paths remain successful. This adds no original engine integration,
spell implementation, persistent save format, or Norrath claim.

### Player-interaction family verification

The final full Release suite passed **102/102 tests in 346.23 seconds**, including
the built-ins exact check (93.71 seconds), with no failures, timeouts or reruns.
The final full-suite arm64 trace still matches the x64 trace byte for byte.

Independent regeneration reproduces the compressed vectors and manifest byte for byte.
The corpus contains **84,581 transitions across 220 operations**. All 79,494
historical transitions remain an exact 1,345,288,514-byte vector prefix.
Independent AST enumeration confirms 324 reference units, 234 fully ported and
tested, 90 remaining, and 19 unbound table names. Nine new handlers and four
helpers implement reference-owned behavior; delegated host work stays explicit.

Both affected Release CTests passed on macOS arm64 (**79.76 seconds**) and x64
under Rosetta (**131.17 seconds**). The complete traces match byte for byte,
SHA-256 `c126fb98aa4b6c81f6bf871381b2ca9f188180c38ea02588840f05debe18e645`.
`file` confirms executed Mach-O arm64 and x86_64 binaries. A test-fixture build
initially used an invalid default object constructor; it was corrected to the
required address constructor before these successful runs.

Synthetic cases cover null/dead objects, negative and empty counts, ordered
3D distances with raw extreme/denormal float patterns, strict radius boundaries,
post-damage object/argument/count rereads, low16 push direction, packed/unpacked
strings and slot reuse/clearing, 159/160-byte limits, zero/nonzero allocations,
healing before allocation, unclamped voice selection, full-record copies,
sparse bytes throughout records, all preserved ranges, medal-slot boundary cases (0, 1, 3, 255), overlapping/wrapping/zero copy
addresses, callback exceptions and mutation, self-replacement/reentry, missing
service write prefixes and saved-flag retention after rebuild failure.

Commands (passed unless an initial corrected failure is noted above):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure > work/interaction-arm-tests.log 2>&1
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4 > work/interaction-x64-build.log 2>&1
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure > work/interaction-x64-tests.log 2>&1
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re --output work/script-builtins-interactions-reproduced.tsv.gz > work/interaction-regeneration.log 2>&1
cmp native/tests/vectors/script_builtins.tsv.gz work/script-builtins-interactions-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.json work/script-builtins-interactions-reproduced.json
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
ctest --test-dir build/native --output-on-failure -j 3 > work/interaction-full-ctest.log 2>&1
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Private acceptance measurements were skipped because `../../private-validation`
is unavailable, as specified; no alternate location was searched. Windows,
original-code differential reruns, actual damage/spell/voice/save services,
Godot/Session integration and Norrath remain unverified. All public inputs are
synthetic. The research checkout was not edited.

## Scene utilities and audio (16 handlers)

Family selected for this run: **scene utilities and audio**, all remaining
non-script-control handlers in sections 3.1 and 3.9 of the pinned specification.
The eleven utility handlers and five audio handlers add sixteen reference units.
No reference-owned behavior is replaced with a refusal.

| Entry points | Exact behavior |
|---|---|
| `print`, `printVal`, `demoRunning` | Print converts then drops its string; the other two return zero without reading arguments |
| `getRand` | One existing shared-seed draw, including its low32 seed store and draw count |
| `getScaledTime` | Signed integer conversion, two ordered console multiplications by 60 and the truncated 1/60, then integer conversion |
| `sin1000`, `cos1000` | Existing pinned 16-bit angle math, multiplied by 1000 and converted with console arithmetic |
| `gamePaused`, `networkMode`, `levelLoading` | Pause/travel/cutscene OR; signed network byte; signed loading word |
| `getLevelTimer` | Unsigned counter conversion with the reference high-bit halve/round-odd/double path, then truncated 1/60 multiplication |
| `sound`, `soundSequence` | Convert both strings before resource lookup; sample playback only for nonzero resource; sequence playback even for zero, followed by freeze-enabled request on the returned handle |
| `startMusic`, `startAudio`, `endMusic` | Ordered level-music, level-stream and stream-stop requests with exact integer constants |

`print`, `startMusic` and `startAudio` retain shared slot 0x20. `sound` and
`soundSequence` retain shared slot 0x30 for both strings; a refused second pointer
reuses the first successful pointer. The existing 160-byte conversion refusal,
slot invalidation and callback exception behavior remain unchanged.
Resource lookup uses `find_resource`; every audio request uses `scene_service`.
Callback values are raw low32 words or owned strings. Missing services refuse
at their call site; zero-resource sample playback needs no audio callback.
Callable copies support replacement and reentry, and the second sequence request
uses the live callback after playback returns. This ports audio requests, not an
audio engine or actual playback.

### Scene-utility/audio verification

The corpus now contains **85,782 transitions across 237 operations**. All
84,581 historical transitions remain an exact **1,423,391,089-byte prefix**.
An independent AST enumeration confirms 324 reference units, 250 fully ported
and tested, 74 remaining units and three unbound table names. All sixteen new
handlers are covered; the count excludes the command-line `_main` harness.

The final full Release suite passed **102/102 tests in 492.92 seconds**, with
no failures, timeouts or reruns. The final full-suite arm64 built-ins trace still
matches the x64 trace byte for byte.

Both affected Release CTests passed on macOS arm64 (**80.74 seconds**) and x64
under Rosetta (**124.02 seconds**). `file` confirms Mach-O arm64 and x86_64
executables. Complete traces match byte for byte, SHA-256
`823df119c2a2380e558336b4dfae06c2a5c7a4a642bbaa14a806f840eaef93a0`.

Independent regeneration reproduced both compressed vectors and the manifest byte
for byte. All commands listed below passed after the corrected initial failures.

New synthetic cases cover signed integer time conversion and rounding boundaries,
unsigned timer high-bit conversion, wrapped angles and quadrant boundaries,
signed network bytes, pause/travel/cutscene width checks, shared RNG seeds,
packed/unpacked strings, shared/refused pointer slots, 159/160-byte boundaries,
zero/nonzero resources, exact playback/freeze request order, callback exceptions,
mutation, replacement and reentry, missing-service refusals and constant handlers
with inaccessible arguments. Initial fixture setup, string-offset and generator
reset-operation errors were corrected before the passing checks.

Commands:

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure > work/utilities-arm-tests.log 2>&1
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4 > work/utilities-x64-build.log 2>&1
build/native-x64/champions-script-builtins-tests --api
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure > work/utilities-x64-tests.log 2>&1
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re --output work/script-builtins-utilities-reproduced.tsv.gz > work/utilities-regeneration.log 2>&1
cmp native/tests/vectors/script_builtins.tsv.gz work/script-builtins-utilities-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.json work/script-builtins-utilities-reproduced.json
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
ctest --test-dir build/native --output-on-failure -j 3 > work/utilities-full-ctest.log 2>&1
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Private acceptance was skipped because `../../private-validation` is unavailable;
no alternate location was searched. Windows execution, original-code differential
reruns, actual audio playback, Godot/Session integration and Norrath compatibility
remain unverified. All public inputs are synthetic. No research file was edited.

## Script execution, deferred calls and post actions (three handlers, eleven reference units)

Family selected: **script execution, deferred calls and post actions**. This
completes all remaining table handlers and all eight lifecycle helpers in the
pinned reference. All reference-owned behavior in this family is implemented.

| Entry points | Exact behavior |
|---|---|
| `callScript`, `run_script` | Case-sensitive lookup in the selected machine; -1 for null name, absent machine or missing public; signed missing-stamp comparison and low32 frame+20 write; nested execution with explicit entry PRI; absent VM result becomes zero, including errors, while a HALT result survives its error code |
| `set_script` | Select owned AMX machine or null; register built-ins without loading it; running calls retain their machine across callback replacement |
| `addDeferedCall`, `queue_deferred` | Queue into the opposite half of the 2×64×0x50-byte buffer; store queued count before the string copy; silently drop at count >=64; no invented lower bound or name-length cap |
| `run_deferred` | Reread signed count and flip after every run; queue additions wait for the next tick; copy queued count, clear queued count, increment full low32 flip in that order |
| `clear_deferred`, `reset_script_state` | Three ordered counter resets; the latter also clears only the post-action buffer's first byte |
| `postAction`, `set_post_action`, `run_post_action` | Replace the pending name; snapshot and clear before execution; set flag=1 before running, flag=0 afterward; an escaping host exception retains flag=1 and the completed write prefix |

`set_script` accepts `shared_ptr<amx::Machine>`; the owner must load the machine
and keep `Variables` alive while its bound natives can run. `entry_pri` defaults
to zero as in the reference and is explicit because original entry PRI is
uninitialized. `run_script(std::nullopt)` represents a null name. Each actual
run gets fresh unknown string slots and restores every outer slot on success,
VM error, host exception or interpreter resource refusal. A missing entry
performs no slot reset. The three handlers share slot 0x20, including refused
pointer reuse and the established 160-byte conversion boundary. Direct queue
and post-action helpers have no such handler buffer limit and preserve overlapping
name writes and negative counter arithmetic.

The existing `run_reward_script` callback stays explicit and byte-compatible
with historical fixtures. An owner may wire it to `run_script`; this change does
not silently replace host-provided reward behavior. Saves, resource lookup,
presentation and engine work remain callbacks. No new engine defaults are added.
Deferred tasks/post actions require explicit host calls; no Session scheduler,
object-trigger dispatcher or Godot integration is implied.

### Script-control verification

The corpus contains **86,887 transitions across 251 operations**, adding 1,105
new-family transitions. All **85,782** historical transitions remain an exact
**1,440,181,375-byte prefix**. Independent full regeneration reproduces both the
compressed corpus and manifest byte for byte. Independent AST enumeration confirms
324 reference units, 261 ported, 63 remaining and zero unbound table names.

Synthetic checks cover real nested bytecode, initial PRI, null/missing/case-sensitive
entries, signed stamp comparisons and overflow, VM error versus HALT results,
outer-slot restoration, packed/unpacked strings, refused-pointer reuse, short
argument counts, 159/160-byte handler limits, negative queue counters, full
capacity and dropped entries, flip wrap, overlapping names, paused tasks,
queueing during a run, post-action replacement, callback count/flip mutation,
exceptions and machine replacement. Direct API checks also cover all outer slot
kinds, interpreter-budget exceptions and retaining the active machine when a
callback selects another one. Early fixture label/opcode spelling and runner
operation setup errors were corrected before the passing runs.

New-family direct checks passed on macOS arm64 and x64 under Rosetta with
byte-identical traces. Both full-corpus CTests passed on arm64 in **95.41 seconds** and x64 under
Rosetta in **141.74 seconds**. `file` confirms executed Mach-O arm64 and x86_64
binaries. The complete traces match byte for byte, SHA-256
`3c8feabc45608e063872db8e254fa9a5ccd2e67770163da20b615c3b97e742e3`.
The final full Release suite passed **102/102 tests in 513.80 seconds**,
including the built-ins exact check in **202.83 seconds**. No failures, timeouts
or reruns were needed. The final arm64 trace still matches x64 byte for byte.

Commands (all final invocations passed; scratch fixture setup failures are noted above):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure > work/control-arm-tests.log 2>&1
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4 > work/control-x64-build.log 2>&1
build/native-x64/champions-script-builtins-tests --api
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure > work/control-x64-tests.log 2>&1
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re --output work/script-builtins-control-reproduced.tsv.gz > work/control-regeneration.log 2>&1
cmp native/tests/vectors/script_builtins.tsv.gz work/script-builtins-control-reproduced.tsv.gz
cmp native/tests/vectors/script_builtins.json work/script-builtins-control-reproduced.json
ctest --test-dir build/native --output-on-failure -j 3 > work/control-full-ctest.log 2>&1
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

A streaming comparison against the saved pre-change compressed fixture verifies
the entire historical prefix; `work/control-prefix-proof.json` records its row
and byte counts. All public inputs are authored synthetic fixtures. Private
acceptance measurements were skipped because `../../private-validation` is
unavailable; no alternate location was searched. Windows execution, original-code
differential reruns, actual engine services, Godot/Session integration and Norrath
compatibility remain unverified. The research checkout was not edited.

## Sparse memory backing, typed accessors and global descriptors (22 reference units)

Family selected: **sparse memory backing, typed accessors and global descriptors**.
All nineteen `Mem` methods and all three `_Global` methods are implemented from
sections 2 and 5 of the pinned Python contract. The AST total remains 324;
`_main` is the only excluded command-line harness. No refusal counts as a port.

`Memory` now caches owned pages on first read or write. An optional `under`
callback receives the page base and 4096-byte request; changing it leaves cached
pages intact. The default remains zero-backed. Backing exceptions retain the
reference write-log prefix and any completed earlier pages. Public `page(index)`
returns a stable mutable page, matching `_page`; `nonzero_bytes()` reflects live
page aliases and retains the existing canonical sparse snapshot representation.
Short backing results preserve Python bytearray slice behavior rather than
inventing missing bytes. Reads and writes wrap addresses modulo 2^32; empty
writes produce no log entry. Bulk writes log once before requesting pages.

`load` returns an unsigned arbitrary-width `Integer` with little-endian magnitude;
`store` accepts arbitrary-width positive or negative magnitudes and reduces them
modulo the requested byte width. The convenience word/byte interfaces remain.
All unsigned and signed typed readers and width-limited stores are exposed.
`cstr` accepts an explicit limit (default 0x4000); `string` is the reference
`strcpy` byte-and-terminator operation. `Global` preserves address, kind, size,
documentation, class descriptor identity, signed/unsigned reads and exact-width
writes for all six documented kinds. It does not install new implicit engine
callbacks or reinterpret Return to Arms globals as Norrath state.

### Memory/global verification

The corpus contains **87,660 transitions across 261 operations**, adding 773
new-family transitions. A streaming comparison verifies that all 86,887 historical
rows remain an exact **1,456,933,380-byte prefix**. The proof is recorded locally
in `work/memory-prefix-proof.json`. The new-family corpus was executed twice from
its reset boundary for 1,546 transitions on each architecture; both matched the
reference and one another exactly.

Both full-corpus Release CTests passed on macOS arm64 (**169.28 seconds**) and
x64 under Rosetta (**247.83 seconds**). Direct API tests passed on both.
`file` confirms executed Mach-O arm64 and x86_64 binaries. Complete traces match
byte for byte, SHA-256 `fbafbf2d284a6c36658d2896750c97b3b9c8e03120a5da29ab5eb99ac306d8da`. Synthetic coverage
includes arbitrary positive/negative integer magnitudes beyond 64 bits, widths
0/1/2/4/8/9/17/33, page boundaries, address wrapping, all eleven typed accessors,
all six global kinds, string-limit boundaries, empty writes, mutable page aliases,
backing replacement and caching, short backing results and callback failure prefixes.
AST enumeration and the checked manifest confirm 283 ported units of 324, leaving
41; no delegated engine behavior or refusal contributes a new ported unit.

The final full Release suite passed **102/102 tests in 373.27 seconds**, with
no failures, timeouts or reruns. The final arm64 complete trace still matches
x64 byte for byte. The research checkout remains unchanged.

Commands (all final invocations passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
build/native-x64/champions-script-builtins-tests --api
python3 work/check_memory_prefix.py
build/native/champions-script-builtins-tests work/memory-family.tsv work/memory-family-arm.tsv
build/native-x64/champions-script-builtins-tests work/memory-family.tsv work/memory-family-x64.tsv
cmp work/memory-family-arm.tsv work/memory-family-x64.tsv
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Files changed: the existing module header/source, existing native test runner,
existing generator/verifier, compressed corpus/manifest, and this documentation.
No shared build plans, research files or sibling worktrees were edited.
Private acceptance measurements were skipped because `../../private-validation`
is unavailable; no alternate location was searched. Windows execution, original-code
differential reruns, real engine services, Godot/Session integration and Norrath
compatibility remain unverified. All public inputs are synthetic.

## Memory-backed objects and call descriptor metadata (13 reference units)

Family selected: **memory-backed object construction and accessors, plus call
descriptor metadata**. All twelve `Obj` methods/properties (including the three
property setters separately) and `X.__init__` are implemented from sections 3
and 4 of the pinned Python contract. No delegated behavior or refusal counts as
an added port. The AST total remains 324, excluding only `_main`.

`Obj` inherits the existing authored `objects::GameObject` defaults. Construction
binds the supplied Memory and type name without writing memory, clearing existing
fields, allocating from the World pool, drawing random values or registering an
object. Explicit live getters/setters expose flags at +0x0c (u32), position at
+0x10/+0x14/+0x18 (three raw u32 patterns), and facing at +0x38 (u16). Generic
`u`/`w` accept an explicit offset and width; stores accept arbitrary signed
integer magnitudes through the existing `Memory::Integer` representation.
`cflags`/`set_cflags` use the eight-byte creature field at +0xa8. All address
arithmetic wraps modulo 2^32. Position reads return fresh values; position stores
occur separately in component order, preserving prior writes if indexing or a
backing callback fails. Extra components are ignored like the reference.

A null Memory reproduces the property getters' zero results and setters' no-op
behavior, including short position inputs. Generic access and creature flags
require memory; the reference's missing-memory attribute failure maps to native
`HostFault`. Short position inputs map `IndexError` to `std::out_of_range` after
the same prefix. `repr` preserves the unsigned address's lower-case hexadecimal
spelling and all Latin-1 type-name bytes, including embedded NUL.
Memory must outlive its object views. Two views at one address observe each
other's writes and external Memory writes immediately.

`X` retains address, integer argument kinds, float and stack counts, result kind
and note, with the pinned defaults. It is metadata, not engine execution.
Existing built-ins, object registries, name tables and callback requirements are
unchanged. Inherited base fields are not synchronized with live property methods;
passing this view to `objects::World` does not establish a shared-memory bridge.
`GameState.new_object` and default Host/World integration remain on the remaining
list below, with public evidence available for a future family.

### Object/descriptor verification

The corpus now contains **89,866 transitions across 273 operations**, adding
**2,206 new-family transitions**. A streaming comparison proves all **87,660**
historical transitions remain an exact **1,465,188,802-byte prefix**; local proof
is in `work/object-prefix-proof.json`. Both new-family traces passed on macOS
arm64 and x64 under Rosetta and match byte for byte. Direct API tests passed on
both, including constructor defaults, unchanged preexisting fields, live aliases,
arbitrary signed widths, detached properties, exact descriptor metadata,
short-input store prefixes and a backing failure after the first position store.

The arm64 full-corpus test passed in **67.60 seconds**; the x64 full-corpus test
passed in **103.89 seconds**. `file` confirms executed Mach-O arm64 and x86_64
binaries. Complete traces match byte for byte, SHA-256
`846a5dce5d4c3676da4c3382176f6e679a44830fff51517319d418ee5e8074d1`.
Independent AST enumeration confirms 324 total units, 296 ported and 28 remaining;
no missing-memory fault or delegated engine service is counted as a port.

The final full Release suite passed **102/102 tests in 293.48 seconds**, with
no failures, timeouts or test reruns. The arm64 complete trace matches x64.

Commands (final invocations passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re > work/object-generation.log 2>&1
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4 > work/object-x64-build.log 2>&1
build/native-x64/champions-script-builtins-tests --api
python3 work/check_object_prefix.py
build/native/champions-script-builtins-tests work/object-family.tsv work/object-family-arm.tsv
build/native-x64/champions-script-builtins-tests work/object-family.tsv work/object-family-x64.tsv
cmp work/object-family-arm.tsv work/object-family-x64.tsv
ctest --test-dir build/native --output-on-failure -j 3 > work/object-full-ctest.log 2>&1
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure > work/object-x64-ctest.log 2>&1
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

An initial x64 build invocation failed because its CMake configure process was
still running; waiting for configuration and repeating that build resolved it.
There were no implementation or fixture failures. Existing module/header, tests,
generator/verifier, corpus/manifest and this page are the only changed files;
shared build plans and research files remain unchanged. Private acceptance was
skipped because `../../private-validation` is unavailable; no alternate location
was searched. Windows execution, original-code differential reruns, real engine
services, World/Godot/Session integration and Norrath compatibility remain
unverified. All public fixtures are synthetic.

## Host call-out dispatch and object-world service adapters (12 reference units)

Family selected: **Host call-out dispatch and object-world service adapters**.
All twelve methods of pinned `Host` (reference lines 562–625, spec section 7)
are implemented: construction, `call`, `default`, `_obj`, the three object
mutators, box/class/grid enumeration, virtual dispatch and alive-player routing.
The reference delegates these operations to `ref_objects.World` or
`GameState.rebuild_alive_players`; the native `Host::Game` supplies those exact
explicit delegates. This ports dispatch behavior without claiming the still
unported GameState construction, alive-player calculation or live Obj/World
integration. The callbacks must keep object identities alive and perform grid
walks synchronously. `Host`, its game adapter and delegates are caller-owned.

`call` appends an owned `(name, arguments)` log entry before resolving an override
or default method. Overrides win, including for otherwise unknown names; a null
handler falls back. Reentrant handlers can erase themselves, replace handlers
or grow the log while retaining their arguments. Direct methods and
`default_answer` do not append log entries. Object methods resolve the registry
identity before invoking their delegate. Virtual slot 1 calls the destruction
delegate with the supplied flags before returning zero; other documented slots
do not dispatch an engine operation. Grid walking returns each visited identity
in order, retaining duplicate visits and ignoring the opaque callback argument.
Box queries preserve three raw coordinate words and signed limit; class queries
preserve byte strings and signed limit. Alive-player routing invokes the supplied
state callback and returns no value.

All 96 EXT entries and ten virtual slots have the pinned default result shape:
zero for numeric/unused results, a fresh empty list for list results, `(-1,0)`
for inventory substring lookup, and `((0,0,0),0)` for the virtual vehicle seat.
An object-world method with Python implicit `None` maps to `std::monostate`;
this stays distinct from zero. Missing registry keys/unknown metadata raise
`std::out_of_range` (the Python dictionary lookup failure); missing delegates
raise explicit `HostFault`. All twelve Host units execute their specified
dispatch behavior; no replacement refusal is counted as a port. Engine effects
remain the existing callback boundary. Public argument variants retain raw words, owned byte strings, vectors,
object identities, lists, pair/seat results, the blank item record and opaque
arbitrary-width integers. Host is not automatically wired into `Variables`.

### Host verification

The corpus contains **90,763 transitions across 277 operations**, including
**897 added-family transitions**. Streaming comparison proves all **89,866**
historical transitions remain an exact **1,493,539,540-byte prefix**; local proof
is in `work/host-prefix-proof.json`. Independent AST enumeration confirms 324
reference units, 308 ported and 16 remaining, excluding only `_main`.

Both direct API suites passed, covering constructor defaults, supplied owner,
registry identities, missing keys, exact result shapes, log ordering, unknown
metadata, fixed arity, override precedence, callback failures, self-erasing
handlers/delegates, reentry, retained arguments and aliased call-name lifetime.
The explicit grid/run-list adapters also ran against an authored native World.
The focused synthetic trace passed **1,097 transitions** on both architectures
(the 897 new-family rows plus 200 reset rows to satisfy the existing runner's
minimum-corpus check). These focused traces match byte for byte.

Both full-corpus suites passed: arm64 **2/2 in 80.33 seconds**, x64 under Rosetta
**2/2 in 136.48 seconds**. `file` confirms executed Mach-O arm64 and x86_64
binaries. The complete traces match byte for byte. Complete trace SHA-256:
`01ebd63c56a013bfd46e587d3190a814d6a1f0b3f3352d7025995867cf5cf813`.

The final full Release suite passed **102/102 in 357.06 seconds**, with no
failures, timeouts or reruns. The final arm64 complete trace still matches x64
byte for byte. The research checkout remains unchanged.

Final commands (each final invocation passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re > work/host-generation.log 2>&1
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
python3 work/check_host_prefix.py
build/native/champions-script-builtins-tests work/host-only.tsv work/host-only-arm.tsv
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
build/native-x64/champions-script-builtins-tests --api
build/native-x64/champions-script-builtins-tests work/host-only.tsv work/host-only-x64.tsv
cmp work/host-only-arm.tsv work/host-only-x64.tsv
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Initial focused runs exposed a synthetic reentrant override that called itself;
it was corrected to call a distinct service. The focused runner also required
more than 1,000 rows; adding 200 reset rows to the ignored focused fixture met
that existing guard. Final fixtures and tests have no unresolved failures.

Changed files: the existing module header/source, existing native test runner,
existing generator/verifier, compressed corpus/manifest and this documentation.
Shared build plans, research files and sibling worktrees were not edited.
Private acceptance measurements were skipped because `../../private-validation`
is unavailable; no alternate location was searched. Windows execution,
original-code differential reruns, real engine services, live-memory World
integration, Godot/Session integration and Norrath compatibility remain
unverified. Public fixtures contain authored synthetic inputs only.

## Direct script argument access and native binding (9 reference units)

Family selected: **direct script argument access and native binding**. Implements
all six `Args` units and `GameState.natives`, `_bind`, `_missing` from pinned
reference lines 662–703 and 1268–1292. `Args` captures `params + 4`, reads cells
live regardless of pushed argument count, exposes low32 unsigned values and
console signed-integer-to-float conversion, and preserves successful string
addresses in the same owned slots already used by the built-ins. Refused
addresses reuse only initialized slots; detached adapters do not persist slots.
`particleMorph` has independent index-0/index-1 slots with no fallback. String
room checks occur after address resolution and string copying; failures retain
that write prefix. Native addresses use the existing signed-64-bit AMX domain;
unrepresentable adapter arithmetic explicitly refuses rather than overflowing.

`Variables::bind` filters slots before constructing the adapter or invoking its
supplied method, ignores the harness argument vector, and retains the active
method through reentry/self-erasure. `natives` returns fresh maps containing all
191 table entries through this binder. `missing` reproduces the reference
helper's specified fault without mutating slots. This counts implementation of
the defined refusal helper itself; no refused engine behavior counts as ported.
The old dispatcher now uses the same slot metadata and filtering operation.
The six state-construction/setup methods and `_tdiv` remain uncounted.

### Argument/binding verification

The corpus contains **93,090 transitions across 285 operations**, adding
**2,327 family transitions**. Streaming comparison confirms all **90,763**
historical transitions remain an exact **1,502,556,144-byte prefix**. Both focused
family traces pass and match byte for byte on macOS arm64 and x64 under Rosetta;
`file` confirms the executed Mach-O architectures. Direct API checks cover
captured base/live reads, count ignoring, signed conversion, string-limit failure
prefixes, detached frames, slot filtering, particle index fallback refusal,
callback self-erasure, fresh complete maps, and unchanged wide/absent callback
returns. Independent AST enumeration confirms 324 units, 317 ported and seven
remaining, excluding only `_main`.

Both full-corpus suites pass: arm64 **2/2 in 113.84 seconds**, x64 under Rosetta
**2/2 in 165.07 seconds**. Complete traces match byte for byte; SHA-256:
`f58e3de76dc5652e2cea59a812bc7d20e70ffa397a76ffc8f717a9e32b147d73`.
The final full Release suite passed **102/102 in 385.81 seconds**, with no
failures, timeouts or reruns. The built-ins full-corpus test passed again in that
suite in **89.56 seconds**, and its final trace still matches x64 byte for byte.
The read-only research checkout remains unchanged.

Final commands (all final invocations passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re > work/arguments-generation.log 2>&1
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
python3 work/check_arguments_prefix.py
build/native/champions-script-builtins-tests work/arguments-only.tsv work/arguments-only-arm.tsv
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
build/native-x64/champions-script-builtins-tests --api
build/native-x64/champions-script-builtins-tests work/arguments-only.tsv work/arguments-only-x64.tsv
cmp work/arguments-only-arm.tsv work/arguments-only-x64.tsv
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

An intermediate arm64 build failed to link because its source compilation read
the earlier callback type before a header edit completed. Recompiling that object
from the final sources resolved it. An initial generator run was stopped to
correct an authored table-name case before regenerating. These were development
invocations; final results are reported separately.

Changed files: the existing module header/source, native tests, generator/verifier,
compressed corpus/manifest and this page. No shared build plans, research files,
or sibling worktrees were edited.
All fixtures are authored synthetic inputs. Private acceptance is skipped because
`../../private-validation` is unavailable; no alternate location is searched.
Windows execution, real engine services, live-memory World integration,
Godot/Session integration and Norrath behavior remain unverified.

## State construction, host forwarding and object/player setup (6 reference units)

Family selected: **state construction, host forwarding and object/player setup**.
Pinned `GameState.__init__`, `ext`, `virtual`, `rebuild_alive_players`, `new_object`
and `quest_vars` (reference lines 742–820 and 896–906) establish this family.
The existing `ref_objects.World.home_cell`, `HandlePool.link`, `NameTable.add`
and `World.alive_players` contracts supply grid/name registration and flag-8
alive-player selection. The fixture manifest independently enumerates 324 AST
units, excluding only `_main`; these six bring the tested count to 323.

`Variables` now accepts shared memory, World and Host owners, creating each
missing owner independently. Supplied identities are retained, and an existing
Host game owner is preserved. Otherwise, the Host receives explicit object
resolution and alive-player delegates. Machine, entry PRI, slots, draw count
and next setup address retain their specified initial values; construction
writes no memory. Destruction detaches only this state's Host owner pointer.
Supplied worlds and objects referencing this state's memory/handles must not
be used after this state is destroyed. Memory-backed Obj identification is a
single-threaded lifetime registry; it leaves the independent World ABI unchanged.

`ext` forwards the entire argument/result variants without conversion.
`virtual_call` prepends slot and object address before forwarding. Host logging,
overrides, callbacks, failures and reentry retain the existing dispatch contract.
Other World services remain explicit delegates: standalone World lifetime and
query methods cannot treat these memory-backed setup objects as their own pool
allocations. This API registers setup objects and grid handles; it does not
establish a live-memory engine adapter.

`new_object` writes flags, three raw position words and low16 facing in order;
copies supplied properties; links into the specified grid or off-grid list;
registers explicit names followed by exactly lowercase `name=` properties;
then publishes the address registry entry. It consumes no random draws. Automatic
addresses start at 0x01000000 and advance by `(size + 15) & ~15`, including
negative signed sizes; explicit addresses do not advance the cursor. Duplicate
addresses preserve older linked/name identities while replacing the registry
entry. Failed insertion prefixes retain linked objects. Address identities use
the existing low32 domain; automatic cursor/size arithmetic outside the supported
signed64/address domain refuses explicitly instead of overflowing.

Alive rebuilding resolves every player pointer before any store, captures the
signed player count once, and writes living slots in ascending order followed
by the count. It retains unused tail cells and ignores HP. Setup and borrowed
memory-backed objects use live flag bytes; supplied ordinary World objects use
live base flags. A stale pointer refuses before writes. `quest_vars` repeatedly
uses the existing ordered index helper, including its established resource guard.

### Setup verification

The corpus contains **93,422 transitions across 290 operations**, adding
**332 appended rows**. Streaming comparison confirms all **93,090 historical
rows** remain an exact **1,525,831,117-byte prefix** of the generated fixture;
local proof is `work/setup-prefix-proof.json`. Focused arm64 and x64 traces each
pass **1,332 transitions** (the appended rows plus 1,000 unchanged reset rows
for the existing runner minimum) and match byte for byte. Both direct API suites
pass, and `file` confirms the executed Mach-O arm64 and x86_64 binaries.
Full-corpus suites pass: arm64 **2/2 in 88.23 seconds** and x64 under Rosetta
**2/2 in 127.49 seconds**. Complete traces match byte for byte; SHA-256:
`3e5b747f1784506456180f82e9d36dc6bb27630e704068cbb6006f5a56c117a1`.
The final full Release suite passed **102/102 in 496.17 seconds**, with no
failures, timeouts or reruns. The final built-ins trace test passed in **89.00
seconds** and still matches the x64 trace. All final command invocations below
passed; the research checkout remains unchanged.

Final commands:

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re > work/setup-generation.log 2>&1
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
python3 work/check_setup_prefix.py
build/native/champions-script-builtins-tests work/setup-only.tsv work/setup-only-arm.tsv
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
build/native-x64/champions-script-builtins-tests --api
build/native-x64/champions-script-builtins-tests work/setup-only.tsv work/setup-only-x64.tsv
cmp work/setup-only-arm.tsv work/setup-only-x64.tsv
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Development checks exposed an assertion inspecting the object back-link rather
than its grid node, another incorrectly using a standalone World ownership API
for a setup-created object, and an unregistered service name in a reentry test. They were corrected to inspect the actual
handle lists and call a documented service. The first generation was interrupted
while refining forwarding coverage; the final generation completed normally.

Changed files:

- `native/include/champions/script_builtins.hpp`
- `native/src/script_builtins.cpp`
- `native/tests/script_builtins_tests.cpp`
- `tools/generate_script_builtins_vectors.py`
- `tools/test_script_builtins_vectors.py`
- `native/tests/vectors/script_builtins.tsv.gz`
- `native/tests/vectors/script_builtins.json`
- `docs/SCRIPT_BUILTINS.md`

 No shared build plans or research files
were changed. All fixtures are authored synthetic inputs. Private acceptance was
skipped because `../../private-validation` is unavailable; no other location was
searched. Windows execution, original-code differential reruns, live-memory
World engine integration, Godot/Session integration and Norrath remain unverified.

## Signed truncating division (1 reference unit)

Family selected: **signed truncating division**. Pinned `ref_natives._tdiv`
(lines 80–83) divides absolute integer magnitudes and applies the operand sign
XOR, giving a quotient toward zero. `truncating_divide` exposes that behavior
using the existing little-endian `Memory::Integer` representation. Handwritten
binary long division uses bounded unsigned byte arithmetic; values do not pass
through fixed-width signed division or floating point. Inputs remain unchanged.
Leading zero bytes and signed-zero inputs are accepted; results have no leading
zero bytes and zero has an empty magnitude with a positive sign. A zero divisor
raises `std::domain_error`, the C++ equivalent of Python `ZeroDivisionError`.
This tests the defined error rather than substituting a refusal for division.

The existing generator appends exact Python-derived synthetic division vectors
and independently enumerates all top-level functions and class methods in the
pinned file. The only excluded unit is `_main`, its command-line harness;
property setters are separate units and aliases count once. The manifest and
verifier now account for all 324 units. Previous handlers and their arithmetic
are untouched, and historical vectors must remain an exact byte prefix.

### Division verification

The regenerated corpus contains **94,932 transitions across 291 operations**.
All **93,422 historical rows** remain an exact **1,533,501,299-byte prefix**;
`python3 work/check_division_prefix.py` records the streaming comparison and
independent manifest accounting in `work/division-prefix-proof.json`.
The **1,510 appended rows** include 1,509 division cases and one reset, with
100 specified zero-divisor exceptions. Focused arm64/x64 traces both pass and
match byte for byte. Direct API tests pass on both architectures, covering
8,995 small signed operand pairs, input preservation, 4,097-bit operands,
equal wide operands, signed zero, and empty/padded zero divisors. `file`
confirms executed Mach-O arm64 and x86_64 binaries (x64 under Rosetta).

Both full-corpus suites pass: arm64 **2/2 in 96.36 seconds** and x64
**2/2 in 143.41 seconds**, with no failures or timeouts. The complete traces
match byte for byte; complete trace SHA-256:
`5268f0302ef7b46698cb5862cc08a4f17f81f16871ecfefe8657a3bf1cf95126`.
The final full Release suite passed **102/102 in 413.39 seconds**, with no
failures, timeouts or reruns. The built-ins corpus passed again in **102.87
seconds**, and its final arm64 trace still matches x64 byte for byte.
The read-only research checkout remains unchanged.

Commands (all final invocations passed):

```sh
python3 tools/generate_script_builtins_vectors.py --research-dir ../../champions-return-to-arms-re > work/division-generation.log 2>&1
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-script-builtins-tests --api
python3 work/check_division_prefix.py
build/native/champions-script-builtins-tests work/division-only.tsv work/division-only-arm.tsv
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 --target champions-script-builtins-tests -j 4
build/native-x64/champions-script-builtins-tests --api
build/native-x64/champions-script-builtins-tests work/division-only.tsv work/division-only-x64.tsv
cmp work/division-only-arm.tsv work/division-only-x64.tsv
ctest --test-dir build/native -R '^script-builtins-' --output-on-failure > work/division-arm-ctest.log 2>&1
ctest --test-dir build/native-x64 -R '^script-builtins-' --output-on-failure > work/division-x64-ctest.log 2>&1
cmp build/native/core-traces/script-builtins-actual.tsv build/native-x64/core-traces/script-builtins-actual.tsv
shasum -a 256 build/native/core-traces/script-builtins-actual.tsv
file build/native/champions-script-builtins-tests build/native-x64/champions-script-builtins-tests
ctest --test-dir build/native --output-on-failure -j 3 > work/division-full-ctest.log 2>&1
git diff --check
git -C ../../champions-return-to-arms-re status --short
```

Changed files: the existing module header/source, native test runner,
generator/verifier, compressed vectors/manifest and this documentation page.
No shared build plans, research files or sibling worktrees were changed.
All fixtures use authored synthetic inputs.

## Exact remaining scope

**No reference functions or table names remain unported.** The complete pinned
Python contract is implemented and tested through 324 reference units and all
191 table names. Reference-delegated services retain explicit callbacks or their
specified defaults/refusals; this count does not include implementing their
external engine behavior.

Complete shared-object integration and object-triggered script dispatch remain
additional integration work. Inventory, vehicle, creature, audio, presentation,
persistence and reward services still need actual engine owners. Windows
execution, original-code differential reruns, live-memory World integration,
Godot/Session integration, Norrath compatibility and playable-scene support
remain unverified. Private acceptance measurements are skipped because
`../../private-validation` is unavailable; no alternate location was searched.
