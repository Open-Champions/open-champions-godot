# Item, gold, skill and monster-builder script commands

`champions-script-items` holds the callees of the level-script item commands
and the arena monster builder. Research: `t3/research-items` at `49bc578`
(`docs/script-items.md`, `tools/ref_script_items.py`,
`tools/ref_monster_build.py`, `tools/difftest_script_items.py`,
`tools/difftest_monster_build.py`, and the `docs/script.md` / `docs/player.md`
fixes of that commit). The policy is `rta-script-items-v1`. The hashes of the
research files are in `native/tests/script_items_contract.json`.

The handler halves were already in the binder (`script_builtins.cpp`): the
loop over players, the medal-mode skip, the weight test and the drop choice.
They call their callees through `Variables::inventory_service`.
`script_items::Service` answers those calls on the binder memory.
`inventory_giveItem`, `inventory_removeItem` and `inventory_equipItem` are
`messages::rta::Machine` ([GAME_MESSAGES.md](GAME_MESSAGES.md)). They run over
an overlay of the binder memory, and the overlay's writes are copied back in
order once the call returns.

## What runs

In the research's order of priority:

1. **Counts.** `playerHasItem`, `onePlayerHasItem` and `countQuestItems` run
   `inventory_countItemByName`. It sums the s16 stacks of every entry inside
   each bag's count whose definition name equals the text exactly. The count
   is re-read after every entry. The definition pointer is not tested.
2. **`removePlayerItem`** runs `inventory_findItemBySubstring`. An entry
   matches when its definition **name occurs inside the text**. Each match
   overwrites the result, so the last match of the last bag that has one
   wins. A definition with an empty name matches any text. The removal is
   `inventory_removeItem(slot, index, bag, 0)`, with the equipment-slot and
   weapon-pointer fixups.
3. **`g_difficultyIdx` starts at 1**, the executable image's value. The host
   writes 1 to `0x4f719c` and to the campaign. `clearAllSkills` (already in
   the binder) fills the skill pairs `+0x4b08..+0x4bc7` with 0xff. Those bytes
   are the first nine of mission row 0, and no reader uses row 0. At
   difficulty 0 the quest port read mission state out of the skill list. That
   was the missions-marked-done bug the quests round reported.
4. **Gives.** The third `givePlayerItem` argument is the parser's
   `usePresets`. The variant is always −1, so it is drawn when the definition
   has variants.
   - **Fits:** `load + weight < limit`. `givePlayerItem` also accepts a weight
     of exactly 0.
   - **Bag full, or a second held-once item:** `inventory_giveItem` returns −1.
     The item is lost, and `Service::counts.lost` counts it.
   - **Equip after a −1:** the handler would equip the address 0x48 bytes
     before the bag. That path **refuses**.
   - **Does not fit:** the ground-item path refuses before `pool_new` takes a
     block. `ItemProp_ctor2` needs unbound resource names, pointers and model
     bounds ([ITEMS_GAPS.md](ITEMS_GAPS.md) section 3).
5. **`game_scanCm1Vars`.** `scan_cm1_vars(0, …, word)` returns the word at
   `0x4f7110`, which the host starts at the image's −1. With any other
   argument it scans this machine's profile names, and
   `set_cm1_word(-1)` stores that scan (`game_set4f7110`). Nothing in the play
   session calls `frontend_start`'s rescan yet. Before the first set, mode A
   would index −1, which `arena_modes` refuses.
6. **`generateMonster`.** The binder interns the class name, and
   `ScriptGenerator_spawn` (`Service::generator_spawn`) builds the monster:
   - `world_createObject(type, generator position, s16(angle − 0x4000), props)`;
   - then `+0xa8 |= 0x1000000` and `+0x34 =` the table entry.
   An exact `Skeleton` would go to the rise state 0x17, and that **refuses**,
   because combat-ctors keeps the Skeleton states refused. With no object
   factory bound, the spawn refuses.
7. **Gold and remove-all.** Gold was already the binder's. It is now checked
   against the research's model. `removeAllItems` and `removeAllWeapons`
   remove entry 0 as many times as the count read at the start.
8. **`arena_spawnMonster`** (`monster_build.hpp`):
   - ring search: up to 17 `world_findFreeSpot` calls, 4000 added after a miss;
   - facing toward the centre;
   - object flag 2;
   - per-class scale: cycle + one of the 30 additive constants;
   - the `SpellArena` record with its one draw;
   - home range 5000, then AI state 0.
   Every world call is an explicit callback, and `refused_class` lets the
   caller keep the AntQueen and Skeleton states refused. Nothing in the
   session calls it yet.

Inventory capacity (`player_recalcStats`' load limit):

```
float(strength + 125) + float(passive 0x40) × 15.0, capped at 9999.0
(the +5000.0 branch is kept in load_limit() for parity; 0x4d8ae8 has no writer, so it is never taken)
```

A record whose object pointer is 0 keeps its old limit. The passive is asked
for with the slot the object names (+0x3c8). `player_itemWeight` returns 1.0
for a null item and for a definition pointer of 0, so an empty entry inside
the count weighs 1.0 in `recalc_load`.

## What stays refused

The bounded item-round paths are in [ITEMS_GAPS.md](ITEMS_GAPS.md) (research
`t3/research-items-gaps` `ecf9742`). What remains refused:

| Command or path | Why |
|---|---|
| `generateMonster` for a class whose constructor chain is not ported | The factory row is decided, but only five creature constructors are ported. Any other row refuses before it takes a pool block. |
| `hideMonster` / `showMonster` on a stub whose factory row the research probe did not decide | 4 rows are undecided; no shipped hide or show target is one. |
| `givePlayerRandom` | The record after ten failed generations is unknown in pinned research, so the host refuses before its first draw even if other inputs are bound. 0 shipped calls. |
| Ground `ItemProp_ctor2` | Resource names, lookup pointers and model bounds are unbound; the host refuses before allocating a pool block. |
| `hideMonster` / `showMonster` on a creature stub with `hide=` or `wait=` | Its constructor's load-time effects have not run. |
| The Skeleton rise; the AntQueen and Skeleton states in the arena builder | combat-ctors keeps them refused. |
| Weapon equips that run `*weaponEquipNotify` / `*bowEquipNotify` | They run a script inside `inventory_equipItem`, which is not composed here. `script_runByName` refuses. |
| `dropItem`, `selectDropItem` | Not in this contract. |

A call-out outside the contract refuses. It is never answered with an
invented value. Some call-outs reached inside the composed bodies are
counted in `Service::counts.deferred` and not applied, because the script
host does not own them:

- `player_countPotions`;
- the rest of `player_recalcStats` (stats, armour, HP and mana; the load
  limits are applied);
- `player_resetIdleAnim` and `inventory_selectIdleAnim`;
- sounds;
- HUD text.

The combat world's `combat_stats::recalc_stats` is the session's.

## Script host

`script_commands::Host` lets the item commands through its refusal gate
(`script_items::routes`). The audit rules still hold:

- **Gives need the item tables and the bound session seed.** The parse may
  draw. The play session binds neither yet, so both gives refuse there.
- **`generateMonster` reads its generators' type names.** It stays on the stub-read
  list, but every `ScriptGenerator` record is now built ([ITEMS_GAPS.md](ITEMS_GAPS.md)
  section 5), so only a generator whose build refused is a stub that blocks it.
- **A refusal part-way** (an equip after −1, the Skeleton rise)
  is counted as a refusal and stops the entry. What ran before it stays.

`Host::items()` exposes the service.

## Vectors

```
python3 tools/generate_script_items_vectors.py \
  --research-dir <research-checkouts>/research-items \
  --out native/tests/vectors/script_items.txt \
  --contract native/tests/script_items_contract.json
```

The generator checks the research HEAD, imports the reference models and
runs them on seeded worlds (seed 20973). The worlds are authored: a synthetic
"image" holds made-up definition rows, names and strings at the addresses
`ref_items` reads. No game byte is used. Each case stores:

- the starting bytes and the call;
- the call-outs' answers;
- the result, the delegated call log;
- every byte the reference wrote.

The scratch record is excluded, because the native port keeps it outside
the binder memory.

`script-items` (CTest) replays the cases through the binder handlers, the
service and the builder:

- 652 cases and 32,512 checks;
- the original's faults and refusals are expected to refuse;
- also the load-limit, fits, cm1 and synthetic host tests.

A second set with `--seed 7 --cases 80` (1,303 cases, 71,757 checks) also
passes. It is not committed.

## Measure

`champions-script-items-measure DIR` walks every `.GOB`, every container lump
and every `script.scr`, as `items_census.py` does. It prints call sites per
command, counts only.

Run on the owner's disc copy (2026-10-21):

- 120 scripts, 5,660 native calls;
- has/count 158 (61 + 36 + 61);
- `removePlayerItem` 147;
- gives 12 (10 + 2);
- `generateMonster` 136.

These equal the research's census.
