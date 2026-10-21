# Quest and mission state

`champions-quests` is the campaign record for Return to Arms. Research is
`t3/research-quests` at `7efd3ce`: `docs/quests.md`, `tools/ref_quests.py`,
`tools/test_ref_quests.py`, `tools/difftest_quests.py`, and the campaign
operations in `tools/ref_natives.py`. The policy is `rta-quests-v1`. The
hashes of those files are in `native/tests/quests_contract.json`.

The retail experience table, the retail act names and the retail mission
level names are not in this tree. Callers pass the thresholds and the act
and mission rows. Public tests use authored tables and rows.
`champions-quests-measure` reads the real ones from the owner's executable
at run time and prints counts only.

## Record

Four player records. The quest list is eight 32-byte title and text pairs
and a count. `quest_add` stops the whole call at the first player whose list
is full or already has the title. It copies like `strcpy`: the string and its
terminator, and the bytes after the terminator stay. `quest_remove` overlays
the nine-byte placeholder on every match, slides once from the last match,
and drops the count by one. A string that would run past its 32-byte field,
or a stored field with no terminator, is refused. On the console those would
write or read into the neighbouring field.

Mission bytes are five rows of thirteen missions, one row per difficulty
1..5: `+0x4bbf + d × 13 + m`, that is +0x4bcc..+0x4c0c, ending where the
medal bytes start (research-items `49bc578` `docs/script-items.md` section 3).
`g_difficultyIdx` is 1 in the executable image, and a new campaign starts
there. Row 0 would lie on the skill pairs and the countdown bytes, so a
difficulty outside 1..5 is refused for every mission read and write, the hub
name and the menu. Bit 0 is available
and is written on the first `g_numPlayers` records. Bit 1 is complete and is
written on all four records. Value −1 sets that bit. Any other value stores
its bit 0. Mission 13 and above is not written. A negative mission index is
refused: that store lands before the mission row, and this model does not
own the neighbouring field.

Medals are one signed byte per mission, not per difficulty. The grant runs
`firstBronze`, `firstSilver` and `firstGold` for each level gained, then
stores the requested level. Evil and decision are one bit per difficulty in
two bytes. `setEvil(0)` does nothing. `setMadeDecision` always sets the bit.
Script flags are one byte at the player record; the bit number is taken mod
32 and bits 8–31 do not change the byte.

Gold adds the amount divided by the player count (C division toward zero)
as a signed 32-bit sum that wraps, then caps at 99999999, as
`n_givePlayerGold` does. A player count of zero does not enter the loop.
Experience uses the caller-supplied 81 thresholds. The cap is the last entry
minus one. A dead record still gains the level and the points and keeps both
pools. A negative amount lowers experience and does not lower the level. An
experience sum outside a signed 32-bit word is refused; the research did not
run that case.

Quest variables are a lower-case name and a signed value byte. A name byte
of 0x80 or more is refused (`ref_natives._lower`). `vars_get` appends an
unknown name with 0. `vars_clearAll` zeros the values and keeps the names.
A new character has mission 0 available and complete on difficulties 1..5
(`player_initNewCharacter`, research-items section 3).

An item with flag bit 39 is removed on an act change unless its name
contains `bloodstone` or `shardof`, or is exactly `spiritrunes`, `emptybag`,
`fullbag`, `pickaxe` or `twistedkey`. The comparison is case-sensitive.
Equipped-pointer fixups from `inventory_removeItem` are not applied: that
body was stubbed in the research.

## Menu, hub and acts

The menu states 0–8 are `missionMenu_run`. One button is applied per tick.
`tick_menu` takes the three calls the menu makes into other systems as
`MenuInputs`: a local chooser with the pause menu open, `net_isSlotActive`,
and the `hud_stepFade(4)` value. The defaults are the offline values the
research ran with. State 6 waits until the fade value reaches 128.
`open_menu` stores the chooser and state 0 and nothing else; the research
does not say the open clears the other menu words, and state 0 rewrites the
row, column, pause flag and counter. The medal the helpers read is record
0's: every executed case had the local slot at 0. State 4 subtracts 16 from
the counter and stays in state 4. The research did not follow that countdown
to 0, and a two-button chord was not tested. The draw is not this module.

`game_getHubName` returns `hub2` when this difficulty's bit is set in both
campaign bytes and mission 1 is complete. Otherwise it returns `hub`.

`find_act` and `Campaign::act_changed` are `save_findAct` and
`save_actChanged` as `ref_quests` states them: the current act first, then
acts 0..n−1, a non-empty prefix of the pending level, case-sensitive.
`change_level` (`quests_flow.hpp`) does not make a second decision. Level
flow's `begin_change` makes it, the campaign takes level flow's act, and the
campaign's clear (quest lists, flag-39 items, quest-variable values) runs
exactly when level flow resets the act: the act changed, the card latch is
clear and no death task is running. Level flow's prefix match also accepts
an empty prefix. The retail table has none (`difftest_quests` checks, and the
measure below counts 0), so the two rules agree on real data.

## Script host

Documented quest commands run the binder. After each of those commands the
host copies the binder record into the campaign: quests, missions, medals,
evil, decision, flags, level, experience, skill and stat points, hp, max hp,
mana, max mana (+0xac, +0xb0, +0xc0, +0xc4, the words `difftest_quests`
plants) and gold. `setCFlag` and `activateMissionMenu` run as well.
`givePlayerExp` runs only after `set_xp_table`; it copies the binder record
in first, so a level gained refills hp and mana from the live maxima.
`setCFlag` stores creature bits 0 and 26 on the host's own objects. Bit 0
counts a boss-music request and runs `creature_startBossMusic` on the host's
private `audio::Playback`, which needs a supplied music volume when the
creature names a boss track; without one that call is refused
([SCRIPT_COMMANDS.md](SCRIPT_COMMANDS.md)). Other bits are counted and store
nothing.

`activateMissionMenu` opens when the recall byte is 0. This host has no
dialog machine, so the dialog query returns 0. That is the missing machine,
not a claim that a dialog is closed on the console.

`Host::tick_mission_menu(button, inputs)` runs one menu tick. When state 0
pauses, it stores 1 in the binder's `g_paused` word 0x41b4f4. The state-8
tick copies the chosen level into the binder's pending name 0x41b6d0 with
0xff at 0x41b720 (docs/quests.md section 4), and into level flow's pending
request with `ORIENT_NONE` (0xff). The next `change_level` loads that level.

These stay refused. Their bodies are not in the pinned quest contract:

| Command | What is missing |
|---|---|
| `countQuestItems`, `playerHasItem`, `onePlayerHasItem` | the item-name compare inside `inventory_countItemByName` |
| `givePlayerItem`, `givePlayerRandom` | the descriptor parse and the bag insert |
| `removePlayerItem`, `removeAllItems` | the equipped-pointer fixups |

## Saves

`Campaign::save` writes a typed image, magic `OCQUEST1`, version 2: its
mission bytes are rows 1..5. A version-1 image held rows 0..4 and no row 5,
so it is refused. No saved file carries this image yet (see below). It is
not the PS2 card image. `save_build` was not re-run. `Host::save_campaign`
copies the binder record and quest variables into the campaign first and
returns that image. `Host::load_campaign` replaces the campaign and writes
its records and the whole quest-variable table (names, values, then an
empty name) back into the binder. The menu, bags and caller tables travel in
the image. The Godot session save (a replay of controller operations,
NATIVE_SAVE.md) is owned by the session job and does not carry this image
yet. Loading refuses a bag larger than the 64 inventory slots
(0x1200 / 0x48, docs/ui.md) and refuses overrun counts that are not original
capacities.

## Vectors

`tools/generate_quests_vectors.py --research-dir R --tsv T --contract C`
checks the research HEAD and imports the reference. Expected values come only
from `ref_quests.py` and `ref_natives.py`. Every campaign case runs at difficulty 1,
the image value, unless it names another of 1..5; record dumps are rows 1..5
(+0x4bcc). The first 261 lines are the round-1 cases, including a row-5 case
(mission 12 is +0x4c0c, the byte before the medals). Round 2 appends:

- `difftest_quests.py`'s case structure on authored data: 37 helper cases of
  12 helpers, every menu path and launch chain (story, medal, bonus, evil,
  evil row without an evil level, pause, inactive slot, close countdown,
  state 3), 10 hub cases, 12 experience cases, act lookups and changes,
  17 flag-39 bags, and the act clear over two records with a third left alone.
  The authored rows have the documented shape: rows 2, 9 and 11 with no medal
  or bonus level and zero flag words, row 5 with an evil level.
- `test_ref_quests.py`'s movement-and-launch case, step for step.
- Seeded runs (seed 20973): 48 quest-list runs and 48 mission, medal, choice,
  flag and gold runs against `ref_natives`, compared as full record bytes;
  16 quest-variable runs; 200 random helper rows; 720 hub bit patterns;
  60 experience cases on two authored tables; 40 menu runs on random rows.

11,324 lines. The test fails when the line count differs from the contract.

## Measure

`champions-quests-measure --game rta DIR` reads the TEX-paired lumps as the
level-script measure does and plants one player in the binder's player-count
word. It prints counts only.

1. Each level in a fresh host: command counts, campaign events, and record
   0's campaign fields after the run.
2. When the folder holds the executable and its SHA-256 equals
   `difftest_quests.ELF_SHA` (US v2.00): the mission rows, act tables and
   experience thresholds at the research addresses, read through the ELF's
   load segments and counted.
3. The real menu over every row and column, evil off and on, every mission
   available and complete with a gold medal.
4. One campaign carried through every level in directory order (not a
   play-through order): load the saved image, change level through level
   flow with the real acts, run, save.

Private run, 2026-10-21, the owner's RTA US v2.00 folder (read-only):

| Measure | Count |
|---|---|
| Levels / scripts loaded / started / faults | 111 / 111 / 76 / 0 |
| Executed: addQuest, removeQuest, getv, setv | 8, 1, 244, 116 |
| Executed: setMissionAvailable, getMissionAvailable, grantMission, getMissionComplete | 23, 2, 4, 13 |
| Executed: getEvil, getMadeDecision, setPlayerFlag, getPlayerFlag, setCFlag | 19, 3, 9, 1, 23 |
| Refused: countQuestItems, playerHasItem, onePlayerHasItem, removePlayerItem, removeAllItems, givePlayerItem | 34, 18, 119, 47, 1, 1 |
| setCFlag bit 0 / bit 26 / other | 1 / 22 / 0 |
| After the runs: quest lines, available bits, complete bits, quest variables (non-zero) | 8, 41, 22, 198 (78) |
| Executable revision confirmed | 1 |
| Mission rows / level pointers / pointers naming a level on the disc | 13 / 33 / 33 |
| Rows with no side columns (zero flag words, no medal level) | 3 |
| Acts / act rows / empty prefixes / rows naming a level on the disc | 13 / 86 / 0 / 85 |
| Disc levels inside an act / in none | 74 / 37 |
| Experience steps that do not fall | 80 of 80 |
| Hub names that are disc levels | 2 of 2 |
| Menu launches / naming a disc level / crosses refused | 60 / 60 / 12 |
| Chain: levels run, act changes, act resets, quest lines cleared | 111, 18, 18, 6 |
| Chain: save round trips, faults | 111, 0 |

The 13 acts, 86 act rows and three closed rows equal the research's own
counts. No menu, medal, gold or experience command ran in these level runs. The
host runs the level entry points and deferred calls only, and those calls
were not reached from them. The research places them inside conversation
branches and medal blocks (docs/quests.md sections 2 and 7).

The earlier measure counted nine `clearAllSkills` bytes as mission bits.
Research-items `49bc578` established that these bytes lie in row 0, outside
the mission rows 1..5. The current bridge does not count or mirror them.

## Mission rows 1..5 (2026-10-21)

`codex/small-fixes` moves the campaign's 65 mission bytes from rows 0..4
(+0x4bbf) to rows 1..5 (+0x4bcc..+0x4c0c), as research-items `49bc578`
section 3 documents. `codex/items` starts the host's binder word
`g_difficultyIdx` (0x4f719c) and the campaign at 1. The two changes are
independent: this one changes only the quests module, its bridge, measure,
vectors and tests; items changes the host. Merged together, the mission
commands write row 1 and the campaign mirrors it.

Wave 22 measures this combined implementation on the owner-selected disc:
`champions-quests-measure` counts 23 available and 4 complete bits after the
level runs. The `w22on` scene sweep counts the same 23 and 4; its wave-21
baseline counted 32 and 13 because nine `clearAllSkills` bytes from row 0
were included. `quests::IMAGE_DIFFICULTY` is the code's image value;
`script_items::DIFFICULTY_IMAGE` aliases it, and `Host` writes it once at
construction.

`champions-quests-measure`, same disc copy, wave 19 (`63c40ab`) → this
change alone → this change merged with `codex/items` (trial merge on
wave 18 `0f3cafc`; items does not yet merge cleanly into wave 19, for
reasons unrelated to this change):

| Measure | Wave 19 | Rows 1..5 | Rows 1..5 + items |
|---|---|---|---|
| After the runs: available / complete bits | 41 / 22 | 0 / 0 | 23 / 4 |
| Chain: quest lines cleared | 6 | 7 | 7 |
| Chain end: quest lines, available / complete bits | 0, 17 / 14 | 1, 17 / 6 | 1, 17 / 6 |

- Wave 19's 41 / 22 counted row 0: the 23 `setMissionAvailable` and 4
  `grantMission` calls ran at binder difficulty 0 and landed there, and
  `clearAllSkills`' 0xff pairs added 9 / 9 on each of two levels. Row 0 is
  no longer counted.
- Without items, the per-level hosts still run at binder difficulty 0, so
  those commands write row 0 and the campaign shows none of them. With
  items they land in row 1: 23 / 4, exactly the executed calls.
- The chain starts from a fresh campaign, which now holds difficulty 1, and
  `load_campaign` writes it to the binder. At difficulty 0 the chain's
  `getMissionComplete` read the cleared skill pairs as complete missions, so
  scripts took other branches.
- This change alone leaves every executed and refused command count, every table count and every menu
  count is unchanged.

The integrated host starts a single-player game with `g_numPlayers` 1 and
`g_difficultyIdx` 1 (`start_single_player`, docs/SCRIPT_COMMANDS.md). The
campaign holds exactly rows 1..5, including the last row beside the medals.

## After the audit's script-host fixes (wave 17, 2026-10-21)

Integrated on `codex/playable-integration` with `codex/audit-fixes`
(SC-1..7, docs/SCRIPT_COMMANDS.md). Three things change what the quest
commands reach:

- An entry enters in the loader's order, preStart, the objects.ob records,
  then start (`run_prestart`, `adopt_objects`, `run_start`;
  `begin_level` is gone).
- A refused answer that the script uses stops that entry (`refusal_stops`).
  The quest commands that stay refused (item counts, grants and removals)
  therefore end their entry where they used to return 0 and go on.
- `setCFlag` tests the creature flag 0x20 on each object it finds
  (`ref_natives.n_setCFlag`), so it joins the stub-read list: every
  objects.ob record is an unconstructed stub, and a `setCFlag` that finds one
  is refused (`stub_refusals`).

The synthetic host test script ends `ZERO.pri`, `RETN`, as a compiled
function with no return value does, so its last refused command does not
stop it.

Same private run, same tool (`champions-quests-measure`):

| Measure | Round 1 | Wave 17 |
|---|---|---|
| Scripts loaded / started / wait / complete | 111 / 76 / 41 / 37 | 111 / 76 / 14 / 28 |
| Executed: getv, setv, getEvil, setPlayerFlag | 244, 116, 19, 9 | 142, 82, 11, 8 |
| Executed / refused: setCFlag | 23 / 0 | 0 / 1 |
| Refused: countQuestItems, playerHasItem, onePlayerHasItem, removePlayerItem | 34, 18, 119, 47 | 1, 15, 12, 37 |
| setCFlag bit 0 / bit 26 | 1 / 22 | 0 / 0 |
| After the runs: quest lines, available bits, complete bits, variables (non-zero) | 8, 41, 22, 198 (78) | 8, 41, 22, 158 (56) |
| Chain: act changes, resets, lines cleared, round trips, faults | 18, 18, 6, 111, 0 | 18, 18, 6, 111, 0 |

addQuest, removeQuest, the mission commands, getMadeDecision and
getPlayerFlag are unchanged. Every executable-table and menu count is
unchanged.

The Godot session's script host carries this campaign. `script_command_counts`
reports it under `campaign`, and `enable_level_script(script, objects, true)`
starts the next level's host from the previous host's campaign image. The
real-data sweep leaves that off, so each level's counts do not depend on load
order. The act tables are not read in the session, so no act change runs
there.


## After the items port (2026-10-21)

`codex/items` (research-items `49bc578`, [SCRIPT_ITEMS.md](SCRIPT_ITEMS.md))
supersedes the "stay refused" table above for the item commands:

- `countQuestItems`, `playerHasItem`, `onePlayerHasItem`, `removePlayerItem`,
  `removeAllItems` and `removeAllWeapons` now run. `removePlayerItem` uses the
  substring rule: the entry whose definition name occurs inside the text,
  and the last match wins.
- `givePlayerItem` and `giveOnePlayerItem` run only with the item tables and
  the bound session seed. The host binds neither yet, so both still refuse
  there.
- `givePlayerRandom` stays refused.
- The host starts `g_difficultyIdx` (`0x4f719c`) and the campaign at 1, the
  executable image's value. `clearAllSkills` still writes 0xff into
  `+0x4bbf..+0x4bc7`, outside the mission rows. Mission commands read and
  write row 1 (`+0x4bcc`). The quests measure and campaign array hold rows
  1..5, including row 5 (`+0x4c00..+0x4c0c`); `seed_new_character` marks
  those same rows.

`champions-quests-measure` on the same disc copy, wave 17 → items:

| Measure | Wave 17 | Items |
|---|---|---|
| Scripts wait / complete | 14 / 28 | 22 / 33 |
| Executed getv, setv | 142, 82 | 160, 86 |
| Executed / refused: countQuestItems | 0 / 1 | 21 / 0 |
| Executed / refused: playerHasItem | 0 / 15 | 16 / 0 |
| Executed / refused: onePlayerHasItem | 0 / 12 | 106 / 0 |
| Executed / refused: removePlayerItem | 0 / 37 | 37 / 0 |
| Executed / refused: removeAllItems | 0 / 1 | 1 / 0 |
| After the runs: quest variables (non-zero) | 158 (56) | 176 (60) |
| Chain end quest variables | 107 | 123 |

Every mission, menu, act and table count is unchanged.
