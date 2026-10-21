# Quest and mission state

`champions-quests` is the campaign record for Return to Arms. Research is
`t3/research-quests` at `7efd3ce`: `docs/quests.md`, `tools/ref_quests.py`,
`tools/test_ref_quests.py`, `tools/difftest_quests.py`, and the campaign
operations in `tools/ref_natives.py`. The policy is `rta-quests-v1`. The
hashes of those files are in `native/tests/quests_contract.json`.

The retail experience table, the retail act names and the retail mission
level names are not in this tree. Callers pass the thresholds and the act
and mission rows. Public tests use the synthetic table and the synthetic
rows from the reference unit test.

## Record

Four player records. The quest list is eight 32-byte title and text pairs
and a count. `quest_add` stops the whole call at the first player whose list
is full or already has the title. `quest_remove` overlays the nine-byte
placeholder on every match, slides once from the last match, and drops the
count by one. `strcpy` leaves the bytes after the new terminator.

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

Gold is the amount divided by the player count, C division toward zero, then
capped at 99999999. A player count of zero does not enter the loop. The
script binder still faults that case. Experience uses the caller-supplied
81 thresholds. The cap is the last entry minus one. A dead record still
gains the level and the points and keeps both pools. A negative amount
lowers experience and does not lower the level.

Quest variables are a lower-case name and a signed value byte.
`vars_get` appends an unknown name with 0. `vars_clearAll` zeros the values
and keeps the names. A new character has mission 0 available and complete
on all five difficulties.

An item with flag bit 39 is removed on an act change unless its name
contains `bloodstone` or `shardof`, or is exactly `spiritrunes`, `emptybag`,
`fullbag`, `pickaxe` or `twistedkey`. The comparison is case-sensitive.
Equipped-pointer fixups from `inventory_removeItem` are not applied: that
body was stubbed in the research.

## Menu, hub and acts

The menu states 0–8 are `missionMenu_run`. One button is applied per tick.
State 4 subtracts 16 from the counter and stays in state 4. The research
did not follow that countdown to 0, and a two-button chord was not tested.
The draw at the menu function is not this module.

`game_getHubName` returns `hub2` when this difficulty's bit is set in both
campaign bytes and mission 1 is complete. Otherwise it returns `hub`.

`save_findAct` tries the current act first, then acts 0..n−1. A row matches
when its name is a non-empty prefix of the pending level. Comparison is
case-sensitive. An empty prefix does not match. Level flow's own prefix
match does match an empty prefix. `change_level` uses the quest decision
for the campaign clear and level flow's decision for the level-flow clear.
The two can disagree. The clear runs only when the quest decision changed,
the card latch is clear and no death task is running.

## Script host

Documented quest commands already run the binder. After each of those
commands the host copies the binder record into the campaign. `setCFlag`
and `activateMissionMenu` now run as well. `givePlayerExp` runs only after
`set_xp_table`. `setCFlag` stores creature bits 0 and 26. Bit 0 counts a
boss-music request. This host does not play audio. Other bits are counted
and store nothing.

`activateMissionMenu` opens when the recall byte is 0. This host has no
dialog machine, so the dialog query returns 0. That is the missing machine,
not a claim that a dialog is closed on the console. The menu draw does not
run.

These stay refused. Their bodies are not in the pinned quest contract:

| Command | What is missing |
|---|---|
| `countQuestItems`, `playerHasItem`, `onePlayerHasItem` | the item-name compare inside `inventory_countItemByName` |
| `givePlayerItem`, `givePlayerRandom` | the descriptor parse and the bag insert |
| `removePlayerItem`, `removeAllItems` | the equipped-pointer fixups |

## Saves

`Campaign::save` writes a typed image, magic `OCQUEST1`, version 1. It is
not the PS2 card image. `save_build` was not re-run. The replay checkpoint
in `NATIVE_SAVE.md` keeps its byte-stable layout and does not carry this
image. Level flow still keeps its own quest strings and quest values.
Loading refuses a bag larger than the 64 inventory slots and refuses
overrun counts that are not original capacities.

## Measure

`champions-quests-measure --game rta DIR` reads the same TEX-paired lumps
as the level-script measure. It plants one player in the binder's player
count word so quest writes have a record. It prints counts only.
