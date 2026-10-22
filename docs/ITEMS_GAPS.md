# Item round, gaps closed

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
| 3. the ground item (`ItemProp_ctor2`) | `Service::ground_item`, `pool_new` | `item_drops_tests.cpp`, 400 vectors |
| 4. `givePlayerRandom` | binder handler + `Service` callees | `script_items_tests.cpp`, 120 vectors |
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

`pool_new(0x190)` takes a block from the world's pool (the 0x500 class of 512,
spilling into the 3000-byte class of 32, then 0, which loses the item).
`ItemProp_ctor2` at launch 0 (every caller) makes **one shared-seed draw**, which
seeds the prop's own generator `+0x44`, and **one own draw**, the facing `+0x38`.
It writes the block as `item-drops.md` section 1 lists: the empty prop fields, the
table `0x511c38`, the item record at `+0x148`, flags `(f | 0x1809 | 0x102000) &
~0x1800` (and `& ~0x3000` for item flag bit 39), radius 4.0, life 180, the matrix
at `+0x110`, and the object registered in the world.

Not written, because they are not simulation state: the model and texture names,
the resource pointers (`res_find`) and the bounds centre (`model_getBounds`).
A record with definition 0 (a stale record) and a block that is not 8-aligned
refuse; the original reads address 0x34 or faults.

`tools/generate_item_drops_vectors.py` runs `ref_item_drops.Ground` and stores the
whole 0x190-byte block (264 cases; 136 refusals). The test compares every byte.

## 4. `givePlayerRandom`

The binder already composed the handler. This round makes it run:

- the record is blanked as the handler does it (on the first call after the draw);
- the special-level difficulty items set bytes `+0x20/+0x21`;
- `item_generateWithAffixes` is the native generator, with the weapon multipliers
  (7) and loot quality rows (12) read from the image;
- a record whose definition is 0 (the generator failed 10 times) refuses at the
  give and at the drop;
- the ground item is the one of section 3.

`tools/generate_give_random_vectors.py` runs `ref_item_drops.RandomGiver` on the
authored synthetic world of the first item round plus authored prefix and suffix
rows: 120 cases, 100 completed (54 ground drops, 66 special-level items), 20
stale-record faults. The test compares the result, the call log and every byte,
including the shared seed and the ground blocks.

The command stays gated in the host: it refuses until an executable is set and
the session binds the level name (`Host::bind_level_name`) and the monster level
(`Host::bind_monster_level`). The play session binds neither yet. The research
found 0 shipped calls.

## 5. Generators and `generateMonster`

- **Records.** A record whose factory row reaches the `ScriptGenerator`
  constructor `0x265f54` is built as the 0x4c-byte base object: `GameObject_ctor`
  (one shared draw, its own generator `+0x44`), table `0x514e60`, flags `|= 9`, the
  facing at `+0x38`. The property-list word `+0x2c` is the host's identity for the
  list. (The constructor body is read from the pinned research repository's
  disassembly, `0x265f54-0x265fbc`; `hide-monster.md` 4.1 and 5 give the class and
  the 0x4c size.) A `hide` property would move it off the grid, so that record
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
reads it still refuses). An undecided row stays refused. Not done: a stub of a
creature row that has the `hide` property is not moved off the grid at load as
`Creature_ctor` does; the first hide or show relinks it either way.

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
- `givePlayerRandom` without a bound level name, monster level and executable.
