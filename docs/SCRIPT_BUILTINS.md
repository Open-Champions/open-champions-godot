# Return to Arms script built-ins: records, scene lifecycle, HUD, creatures, inventory, effects and vehicles

`champions-script-builtins` is an authored portable C++17 translation of the
quest/profile variable, campaign-state, player-record, scene-lifecycle, menu/HUD, creature, inventory, object/effect/camera and vehicle contracts for **Return to Arms, SLUS_209.73 US v2.00**.
It depends on the existing `champions-amx`, `champions-core` and `champions-objects` modules and the standard library.
It implements 163 of the 191 table names.

PROGRESS: 221 of 324 reference functions ported

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

`Variables::natives()` supplies exactly 163 AMX bindings. Keep the instance alive
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
The 221 fully implemented and tested units are 162 distinct `Game.n_*` handlers
(`newWorldAlways` aliases `newWorld`) plus:

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
The total is distinct from the 191 built-in table names. The remaining 103
reference units are exactly the manifest ledger difference; among them are 28
unbound table handlers, plus host/object/string/script machinery and wrappers.

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

## Exact remaining scope

The manifest's `unsupported_natives` records all 28 unported names at the pin.
They are listed below in original table order. `natives()` does not bind them;
`dispatch()` explicitly refuses unsupported names. No default engine behavior is
invented for these calls.

`callScript`, `print`, `printVal`, `sound`, `soundSequence`, `getScaledTime`, `playGotItemSound`, `hornOmen`
`planarSteel`, `startMusic`, `startAudio`, `endMusic`, `getRand`, `gamePaused`, `levelLoading`, `setBestActivePlayer`
`addDeferedCall`, `postAction`
`closestPlayerDist`, `hitPlayers`, `networkMode`, `getLevelTimer`, `sin1000`, `cos1000`
`explodePotionsOnly`, `demoRunning`, `savePlayer`, `restorePlayer`

Beyond those handlers, complete shared-object integration, selected-object and
active-player machinery, deferred queues, post actions, nested entry-point runs,
missing-entry throttling, object-triggered
script dispatch remain unported here. Player snapshot save/restore, object-dependent player functions, inventory services,
actual vehicle services and complete creature-engine services remain outside this module. Actual presentation engines,
medal UI rendering and complete reward-script execution remain separate work. Connecting this subset to a
Session, a complete engine command table or gameplay needs separate work.
