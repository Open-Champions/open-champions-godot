STATUS: PASS — the observed givePlayerItem rules are locked by a new synthetic test, the weapon notify follows the research (no level script returns, a loaded level script refuses), and the wave-37 gates held. Journey wrapper exit 1 is the retained death refusal, at 80/81 steps with pickup 10→11.

# givePlayerItem — round 1 — 2026-11-09

Branch `codex/play-give-item`, from wave 37 VERIFIED `70d2de19`. Research pin `t3/research-items` `5a683b4`, `docs/give-player-item.md` (audits `cursor-runs/audits/research-give-item.md` and `research-give-item-fix.md`, PASS). Edited and committed on the MacBook. Every build, test, Godot run and journey ran on the mini in an rsynced copy (`.git`, `build`, `work` excluded; seeded locally from a sibling mini worktree, then a delta rsync, checksum-verified identical). Godot `~/malik-champions/runtime/Godot.app` 4.5.2. Disc read-only, 994 files before and after. The mini `build/` was removed after the gates.

## How the client handled givePlayerItem before this round

- Binder handler `native/src/script_builtins.cpp:1728` (`Variables::give` at `:1646`): loop below the re-read `g_numPlayers`, the medal skip on `0x4f713c` / `0x4f7148`, parse with variant −1 and the third cell, the `load + weight < limit` test with the zero-weight exception, the drop through `pool_new(0x190)`, `player_recalcLoad` after every give, then `inventory_equipItem(slot, cell, 0)` and `player_resetIdleAnim`. Result 0.
- Callees in `script_items::Service` (`native/src/script_items.cpp`): `inventory_giveItem` and `inventory_equipItem` are the composed bodies in `messages.cpp` (`Machine::give_item` `:672`, `Machine::_equip_item` `:785`); flag 41 is `EQUIP_FLAG_41` in `items.cpp:624`.
- So the argument contract, recipients, bag choice, stacking, full bag and weight rules were already in place, from the older `49bc578` contract. Nothing tested them against the new research.
- The gap: every weapon equip under `givePlayerItem` reached `script_runByName`. In the script host that ran the level's notify entry (the pickups delegate, PP2-4); outside a host it always refused. The research executed only the no-level-script return and leaves the loaded-level hooks Unknown.
- The play session binds the item tables (`campaign_fight.cpp`, `prepare_items`) and the seed (`session_controller.cpp`, `bind_seed`), so gives can run in play. `docs/QUESTS.md` and `docs/SCRIPT_ITEMS.md` still said they refused; corrected.

## What changed

- `Service::command` names the running script command. The host sets it around every command it dispatches (`script_commands.cpp:575`, restored after nested runs). `Service::level_script_loaded` states whether `g_scriptMachine` `0x86b270` holds a level script. The host binds it to its loaded machine (`:251`).
- Under `givePlayerItem` only, `script_runByName` inside `inventory_equipItem` (`script_items.cpp:479`):
  - no level script: returns without running anything, as executed (`give-player-item.md:131-134`);
  - level script loaded: **refuses** before any script runs (`:135-136`, `:186`);
  - no statement: refuses.

  Pickups and other callers keep the host's notify delegate, unchanged.
- The ground-prop and lost-give-equip refusal reasons now cite the research (`script_items.cpp:658`, `:630`). The binder's misnamed `variant` local is now `use_presets` (`:21-38`). No numeric value was added.
- Starting equipment (`player_giveStartingItems`) is untouched and not routed through this handler (`:5-10`, `:141-151`).

## Rules and their synthetic checks (`native/tests/give_player_item_tests.cpp`, 87 checks)

The test uses authored rows only: blade (category 0), charm (1), token (2, weight 0, bit 39), stack (2, bit 8, count 5), ward (1, bit 41), plus one preset row.

| Rule | Research | Check |
| --- | --- | --- |
| Cells: descriptor, equip, usePresets; parser variant −1, usePresets = third cell | `:21-38` | parse arguments recorded for usePresets 0 and 1; preset table chosen by 1 |
| Result 0 on success, full bag and drop | `:96-100` | binder result 0 in every case. Through the host, `STOR.pri` of a 0x55555555 sentinel becomes 0 on success and at count 64 |
| Every joined slot; medal mode only `0x4f7148` | `:45-52` | two players: slots 0 and 1 written, slot 2 not; medal byte 1: slot 0 empty, slot 1 written |
| Bag = category `+0x1d`, index 0, stamp 0, other bags empty | `:54-56` | categories 0, 1, 2 |
| Flag 39 kept; keep list not applied | `:104-111` | bag 2 with bit 39; no `inventory_removeItem`, `removed` 0 |
| Bit 8 stacks on definition, flags, prefix, suffix | `:60-65` | count stays 1 and adds 5; changed prefix appends; matching again stacks into the first cell |
| Count 63 appends; count 64 loses without drop and recomputes load | `:69-77` | index 63, stamp 63, count 64. At 64: all 64 definition words empty, count 64, `lost` 1, load `0x42800000`, no `pool_new` |
| Weight 0 given at limit 0; positive weight at limit 0 drops; strict less-than | `:85-94` | zero-weight row given at limit 0. Positive weight refuses at the ground prop. With the probe's own `pool_new` → 0 stub: one `pool_new(0x190)`, no constructor, result 0, bag untouched. 99 + 1 at limit 100 does not fit |
| Equip with check 0 stores the cell and the weapon class | `:123-134` | `+0x43f0` = new cell, `+0x4a86` from sentinel 0xa5 to the subtype, weapon set = cell, one `script_runByName(0x538048)`, idle reset |
| Flag bit 41 given, not left equipped | `:138-139` | in bag 1; no equipment word holds the cell; equip called |
| Loaded level script refuses the weapon hook | `:135-136`, `:186` | host run stops `Refused`; give stays; no equipment pointer committed; nothing after it runs |

The zero-weight rule is the handler's weight test, not a list of rows. The research gave one retail row and leaves the other 43 Unknown (`:90-94`), so the test uses an authored row and claims nothing about retail rows. A mutation run on the mini copy (medal gate removed; givePlayerItem branch disabled) made the test fail. The restored build passed 87/87.

## Kept refusals

- **Ground `ItemProp_ctor2`** for the overweight drop. In the probe `pool_new` returned 0, so the constructor body was never entered and stays Unknown (`:86-88`, `:183`). Its resource and bounds services are also unbound here. The refusal stops the entry, so the script never sees the research's result 0 for that case.
- **Weapon and bow hooks while a level script is loaded** (`:135-136`, `:186`). In play the host's level script is always loaded, so a `givePlayerItem` weapon equip refuses there. Armour and charm equips run.
- **Keep-list names not in the research.** `givePlayerItem` never calls `player_removeFlag39Items`. Only `pickaxe` and one `bloodstone` name were re-run on that remover (`:111-119`). The act-change list stays as `t3/research-quests` `7efd3ce` `docs/quests.md:163-169` gives it, and no name was added.
- **Equip after a lost give into bag 0.** The handler equips 0x48 bytes before the bag. The probe planted 0 in that word (`:79-81`; audit `research-give-item.md` traces the early return at `0x1c2800`/`0x1c2804`); a real record's `+0xe8` is not established. This keeps the 10 existing `equip_entry_outside_the_bag` vector refusals unchanged. For bags 1–2 that address is the previous bag's cell 63, inside the bags, and it still goes to the composed equip as before; this round changed nothing there.

## Gates

| Gate | Result |
| --- | --- |
| Release configure + `cmake --build build/native -j 4` | PASS, exit 0, about 165 s |
| GDExtensions, universal | PASS, exit 0. Archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`. `lipo -archs` is `x86_64 arm64` on all five, built fresh in this run |
| Focused `give-player-item` (+ `script-items`, `script-commands`, `quests`, `pickups`) | PASS. 87 checks, 0 failures; 5/5 |
| Full CTest `-j 3` | PASS, **260/260**, 874.74 s. The baseline is 259; the addition is `give-player-item`. No test, vector, fixture or baseline was edited |
| Strict Godot matrix `--captures`, timeout 1200 | PASS, **54/54**. Editor import exit 0. `godot_matrix.py check-log`: 0 of 105 logs flagged |
| Full combat | 2,645 checks, 0 failures |
| FULL_SAVE_LOAD | **300/300**. Continued = uninterrupted `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573` |
| FULL_TRACE | Unchanged, **36,402 bytes**, `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` |

## Journey

`tools/run_loop_playthrough.py`, headless then rendered. Wrapper exit 1. Disc 994/994.

| Variant | Steps | Exit | Pickup | Walk ticks | Screenshots | Log errors | Refused rows | Refusal kinds |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | 10→11, produced, walked, picked up | 52 | 0 | 0 | 27 | 6 |
| Rendered | 81 | 1 | 10→11, produced, walked, picked up | 52 | 38 | 0 | 28 | 6 |

Compared with the wave-37-equivalent run on the mini (`play-spells-numeric` `work/playthrough`, which recorded step/result pairs equal to wave 37): step/result pairs are identical at every index in both variants, and the refusal maps are identical.

The only field differences are host measurements: `fps` on most steps, `memory_bytes` on some rendered steps, and `enemy_fight` `swing_wall_msec` (the swing-loop wall time). The journey's logs contain no `givePlayerItem` call, so this change cannot reach it. `player_death` stays refused and `saved_level_reloaded` is not reached. The rendered extra step is still `mission_menu_render`. `rendered_pickup_verified` stays false.

Evidence (ignored; the mini's home path replaced with `~/`): `work/play-give-item/` (configure, build, extension, CTest, matrix logs, journey summaries).

## Files

- `native/include/champions/script_items.hpp` — `Service::command`, `Service::level_script_loaded`
- `native/src/script_items.cpp` — givePlayerItem notify branch; refusal reasons
- `native/src/script_commands.cpp` — command scope; level-script statement
- `native/src/script_builtins.cpp` — research comment; `use_presets` name
- `native/tests/give_player_item_tests.cpp`, `native/script_commands_targets.cmake` — new `give-player-item` CTest
- `docs/SCRIPT_ITEMS.md`, `docs/QUESTS.md`

No push, no pull request, no edits under `~/champions-re` or the playable-integration worktree, no process killed by name. The one rsync stopped was this round's own PID.
