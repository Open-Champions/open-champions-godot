# Item round, bounded gaps

Research: `t3/research-items-gaps` at `ecf9742` (`docs/spec/items-gaps.md`,
`docs/spec/item-drops.md`, `docs/spec/hide-monster.md`,
`docs/spec/generate-monster-names.md`). It follows [SCRIPT_ITEMS.md](SCRIPT_ITEMS.md).
Everything below is a contract port. Each value traces to those notes or to the
research tools named in the generators' headers. What the research marks Unknown
stays refused.

| Gap | Where | Contract test |
|---|---|---|
| 1. `0x4d8ae8` has no writer | `script_items::recalc_load_limits` | `script_items_tests.cpp` |
| 2. the launch host bit and `+0xf4` | `net_launch.hpp`, `Host::receive_launch` | `net_launch_tests.cpp`, 1,612 vectors |
| 3. the ground item (`ItemProp_ctor2`) | refused until resource and bounds services are bound; `pool_new` remains traced | `item_drops_tests.cpp`, 400 refusal cases |
| 4. `givePlayerRandom` | refused before its first draw | `script_commands_tests.cpp` |
| 5. generators | `Host::build_generator`, `Host::create_object` | `script_commands_tests.cpp` |
| 6. `hideMonster` / `showMonster` on stubs | `creature_class_table.inc` | `level_factory_tests.cpp`, `script_commands_tests.cpp` |
| 7. non-literal `generateMonster` names | no special case; see below | `script_commands_tests.cpp` |

## 1. The load bonus word

`player_recalcStats` adds 5000.0 when the word `0x4d8ae8` is non-zero. The
research searched every way the executable could write it and found only the
reader (`items-gaps.md` section 1). `recalc_load_limits` now passes the constant
`LOAD_BONUS_VALUE = 0` and never reads the binder memory for it. `load_limit()`
keeps its bonus argument so the vectors of the first item round stay valid; the
branch is never taken. A memory poke from outside the program is not modelled.

## 2. The host bit and `+0xf4`

`net_launch` ports the bytes of `items-gaps.md` section 2, as pure functions:
`lobby_set_host`, `frontend_set_host`, `frontend_set_kind`, `own_record`,
`launch_fill_host_fields` and `launch_receive`.

- Only player 0 has the host bit. Only the host fills record `+0xa0..+0xa3` and
  `+0xf4`, and only it calls `game_scanCm1Vars(1)`.
- Every machine applies record 0. In modes 2 and 3 it copies `+0xf4` into
  `0x4f7110` (`game_set4f7110`; a `-1` rescans this machine's profile).
- `Host::receive_launch` is that receiving half on the binder memory.

The vectors come from `tools/generate_net_launch_vectors.py`, which runs
`ref_items_gaps.py`. The play session has no net launch yet, so nothing calls it.

## 3. The ground item

The underlying `pool_new(0x190)` contract takes a block from the world's pool
(the 0x500 class of 512, spilling into the 3000-byte class of 32, then 0).
`item-drops.md` section 1.2 and `ref_item_drops.py:186-206` require model and
texture names, `res_find` pointers and `model_getBounds` centre/height fields
before the object is complete. This host has no bound source for those results.
The item service therefore refuses at `pool_new` before allocating or drawing;
direct `ItemProp_ctor2` calls also refuse before registering an object. The old
zero-stub vectors did not test those fields: zero was
both the generator's answer and the unwritten block's value. The authored 400
cases now check that valid and faulting records publish no incomplete object.
The reference's one shared and one own draw are documented, not claimed to run.

## 4. `givePlayerRandom`

The host refuses this command before its first random draw. `item-drops.md`
section 6 explicitly leaves the handler's stack record `+0x00..+0x0f`
unknown after ten failed generations. The service's old scratch record could
retain an unrelated earlier definition. Its special-level byte store was also
over-broad: `item_parseDescriptor` is shared by other give commands, while the
research puts that store only in `givePlayerRandom` (`item-drops.md` section 2).
That store is removed. The 120 authored reference vectors remain research
fixtures, not executable native acceptance. The research found 0 shipped calls.

## 5. Generators and `generateMonster`

- **Records.** A record whose factory row reaches the `ScriptGenerator`
  constructor `0x265f54` is built as the 0x4c-byte base object: `GameObject_ctor`
  (one shared draw, its own generator `+0x44`), table `0x514e60`, flags `|= 9`, the
  facing at `+0x38`. The property-list word `+0x2c` is the host's identity for the
  list. Pinned `docs/monsters-2.md:7660-7667` states the constructor's table,
  size, facing and flags; `symbols/monsters.tsv:262` pins the table and size.
  A `hide` property would move it off the grid, so that record
  stays a stub.
- **Spawn.** `ScriptGenerator_spawn` (already ported) calls `world_createObject`.
  The host answers it from the factory row and a ported constructor chain with
  the generator's property list. Five creature constructors are ported. A
  class whose constructor is not ported refuses before it takes a pool block. A
  factory row that builds nothing returns 0, which the spawn leaves alone.
- **Skeleton rise.** Not ported; the Skeleton constructor is not, so a Skeleton
  class refuses with the other unported ones.
- **Stub gate.** `generateMonster` acts only on `ScriptGenerator` objects, and
  every one is built unless its build refused. A stub with another constructor no
  longer blocks the command.

## 6. `hideMonster` / `showMonster` on stubs

The handlers need only whether the object is a creature (flag `0x20`, set only by
`Creature_ctor`). `tools/generate_creature_class_table.py` runs the research's
`objects_census.probe_class` (the original factory and constructor chain in
`ee_emu`) on every name-test row of the factory table, cross-checks the pool size
and constructor, and writes `creature_class_table.inc`: the address of each row's
test string and one digit, nothing else. 201 object-building rows: 97 creature,
100 non-creature, 4 undecided.

A stub of a decided row runs both commands: the off-grid list with front
insertion, flag `0x8000`, and, for a creature only, bit 3 of `+0x231`, keeping the
other bits. The stub's flag word never gains `0x20` (so every other command that
reads it still refuses). An undecided row stays refused. A creature stub with
`hide=` or `wait=` also refuses hide/show: `Creature_ctor` applies those
properties at load (`hide-monster.md` section 3.4), but this stub has not run it.

## 7. Non-literal names

The 12 computed class names and 3 computed generator names are constant-table
reads behind `getRand` shuffles (4 and 7 shared draws). The script machine runs
that bytecode as it is, so nothing special was added and nothing was pre-resolved.
The generator name is passed through the same `find`, the class name through the
same factory classification. A reachable computed class reaches one of six
constructors; only the ported ones spawn.

## Refused, still

- classes whose constructors are not ported (spawn);
- the Skeleton rise and the Skeleton and AntQueen states;
- undecided factory rows for hide and show;
- `givePlayerRandom`, including with bound inputs (unknown stale stack record);
- `ItemProp_ctor2` without resource and bounds services;
- `bind_level_name` at 0x41b721 (allocation capacity unpinned);
- hide/show on a creature stub with `hide=` or `wait=` load properties.
