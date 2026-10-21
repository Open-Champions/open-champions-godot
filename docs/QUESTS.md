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

Mission bytes are five difficulties of thirteen missions. Bit 0 is available
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
A new character has mission 0 available and complete on all five
difficulties.

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
counts a boss-music request. This host does not play audio. Other bits are
counted and store nothing.

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

`Campaign::save` writes a typed image, magic `OCQUEST1`, version 1. It is
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
from `ref_quests.py` and `ref_natives.py`. The first 241 lines are the round-1
cases. Round 2 appends:

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

11,304 lines. The test fails when the line count differs from the contract.

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

Two levels make 9 mission bytes read as available and complete without a
mission command. The binder's `clearAllSkills` (exactly `ref_natives`) writes
0xffff into 48 skill slots of 4 bytes from +0x4b08. The last 9 of those bytes
are +0x4bbf–+0x4bc7, the first 9 mission bytes of difficulty 0. Either the
slot count or the mission offset in the research is wrong, or the original
does this. That needs a research answer. This port follows the reference.
