STATUS: COMPLETE

# Items round 1: item, gold, skill and monster-builder script commands

Branch `codex/items` in worktree `~/Projects/open-champions/worktrees/items`,
started at `c42ad2d` (verified wave 17). Wave 18's merges are on
`codex/playable-integration` (`b6a2621`). The integrator has not recorded or
verified them yet, and the integrator's checkout had uncommitted changes, so
this branch starts from the verified wave-17 commit. This round touches no
file that those merges change. Local commits only: nothing was pushed, and
no PR was opened. `~/champions-re` was not touched. The disc copy was only
read, and its outputs (counts only) stay in the gitignored `work/private/`.

Source of truth: `t3/research-items` `49bc578`
(`~/Projects/champions-re-wt/research-items`). Files used:

- `docs/script-items.md`;
- `tools/ref_script_items.py`, `tools/ref_monster_build.py`;
- `tools/difftest_script_items.py`, `tools/difftest_monster_build.py`;
- the `docs/script.md` / `docs/player.md` fixes of that commit;
- `ref_items`, `ref_messages`, `ref_natives`, `ref_core`, which the models
  import.

Their SHA-256 values are pinned in `native/tests/script_items_contract.json`.

## What was added (research priority order)

1. **has/count.** `playerHasItem`, `onePlayerHasItem` and `countQuestItems`
   now run `inventory_countItemByName`: exact `strcmp`, the count re-read per
   entry, s16 stacks summed in 32 bits.
2. **`removePlayerItem`** uses `inventory_findItemBySubstring`. The rule is:
   the name occurs inside the text, and the last match wins. The removal is
   `messages::rta::Machine::remove_item`, with its equipment and weapon
   fixups.
3. **`g_difficultyIdx` starts at 1.** The host writes the image value to
   `0x4f719c` and to the campaign. `clearAllSkills` was already the binder's,
   and exactly the research's. A synthetic test shows its 0xff bytes land only
   in mission row 0, which nothing reads at difficulty 1. That fixes the
   quests port's missions-marked-done reads.
4. **Gives.**
   - The third argument is `usePresets`; the variant is −1, so it is always
     random.
   - Weight test: `load + w < limit`. `givePlayerItem` also accepts w == 0,
     including −0.0.
   - Bag full, or a held-once item: `inventory_giveItem` gives −1 and the
     item is lost (`counts.lost`).
   - Equipping the address before the bag **refuses**.
   - The ground-item path (`pool_new` → `ItemProp_ctor2`) refuses.
   - In the script host, gives also need the item tables and the bound seed.
5. **`game_scanCm1Vars`** returns the `0x4f7110` word (−1 in the image,
   written by the host). Any other argument scans the profile in the
   research's name order. `set_cm1_word` is `game_set4f7110`.
6. **`generateMonster`.**
   - The binder interns the name.
   - `Service::generator_spawn` ports `ScriptGenerator_spawn`: position,
     angle − 0x4000, props, `+0xa8 |= 0x1000000`, `+0x34`.
   - The Skeleton rise refuses.
   - With no object factory bound, the spawn refuses.
   - `generateMonster` joined the stub-read list.
7. **Gold and remove-all.** Gold was already the binder's and now matches the
   model in the vectors. `removeAllItems` and `removeAllWeapons` run.
8. **Arena builder.** `monster_build::arena_spawn_monster` is the ring
   search (≤ 17 tries), facing, flag 2, the 30-entry scale table, the
   `SpellArena` record with its draw, home range 5000 and state 0. The world
   is reached through explicit callbacks, and `refused_class` keeps the
   AntQueen and Skeleton states refused. Nothing in the session calls it yet.

Also:

- **Capacity:** the load limit with the object test (a record with no
  object keeps its limit; the passive is the object's slot's), +5000 when
  `0x4d8ae8` is non-zero, and the 9999 cap.
- **Weight:** `player_itemWeight` returns 1.0 for a null item.
- **Tables:** the item tables are read from an image at the `ref_items`
  addresses (`image_tables`).
- **Measure:** `champions-script-items-measure` gives disc call-site counts.

Kept refused:

- the writer of `0x4d8ae8` (the word is read only);
- the `ItemProp_ctor2` draw (ground items);
- `givePlayerRandom`;
- non-literal class names (same refusing factory as every name);
- the Skeleton rise, and the AntQueen and Skeleton states in the builder.

A refused command still stops the entry when its answer is used. A refusal
part-way through a command (a ground item, an equip after −1, the Skeleton
rise) is counted as refused and stops the entry.

Not applied in the script host, counted in `counts.deferred`:

- `player_countPotions`;
- the non-load part of `player_recalcStats`;
- idle-animation resets, sounds and HUD text.

The script host does not own those. Weapon equips that run the notify
scripts refuse (`script_runByName`).

## Files

New:

- `native/include/champions/script_items.hpp`, `native/src/script_items.cpp`
- `native/include/champions/monster_build.hpp`, `native/src/monster_build.cpp`
- `native/src/script_items_measure.cpp`, `native/script_items_targets.cmake`
- `native/tests/script_items_tests.cpp`, `native/tests/vectors/script_items.txt`
  (83,805 lines), `native/tests/script_items_contract.json`
- `tools/generate_script_items_vectors.py`
- `docs/SCRIPT_ITEMS.md`

Changed (additive):

- `native/script_commands_targets.cmake`: include, link, test target.
- `native/include/champions/script_commands.hpp` and
  `native/src/script_commands.cpp`:
  - the route gate, the tables and seed gate for gives, and the refusal
    conversion;
  - the `generateMonster` stub-read row;
  - the difficulty 1 and cm1 −1 start.
- `native/tests/script_commands_tests.cpp` and `native/tests/quests_tests.cpp`:
  - the two tests that used now-routed commands as their example refusal use
    `givePlayerRandom` and `dropItem`, which stay refused;
  - the quests test now expects `countQuestItems` and `removePlayerItem` to
    execute.
- `docs/QUESTS.md` (new section), `docs/ROADMAP.md` (one sentence).

The parallel script-session job owns the seed binding, level objects and the
player count. This round does not do those. In `script_commands.cpp` it
touches only the command-map gate, the constructor tail and the stub-read
table.

## Commits

- `0e6ce65` the port, host wiring and contract vectors
- `b3b417d` `champions-script-items-measure`
- `8c0cbfb` docs
- this report

## Verification

| Check | Command | Result |
|---|---|---|
| Contract vectors | `champions-script-items-tests native/tests/vectors/script_items.txt native/tests/script_items_contract.json` | 652 cases, 32,512 checks, **0 failures** |
| Second seed (not committed) | generator `--seed 7 --cases 80` | 1,303 cases, 71,757 checks, 0 failures |
| Harness sanity | flipped one byte, one log argument, one result in copies of the vectors | each caught (1 failure each) |
| Native CTest arm64 | `cmake --build build/native -j 4`, `ctest -j 3` | **209/209** on the first run, 882 s; 0 compiler warnings |
| GDExtension | arm64 `template_debug`, main and level-texture | built, 0 compiler warnings |
| Godot dev matrix | wave-17 `run-godot.py`, outputs to `work/items/godot` | **45/45** on the first run; Full combat 2,620 checks, 0 failures; `script_commands_smoke` PASS |
| FULL_TRACE | `playable_wave_smoke_full_combat.gd` | **unchanged**: `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes. No level script runs in that smoke, so the items port cannot reach it. |

The vectors come from the research's own models, run on authored synthetic
worlds with a synthetic image. They cover:

- every handler, the substring rule, held-once and full-bag losses;
- equips, the recall hint, drops and faults;
- weights, complements, load and limit, fits;
- cm1;
- 80 arena spawns (61 run; 19 are the original's faults and refuse);
- 40 generator spawns (5 Skeleton refusals) and 40 interning cases.

The first runs found one generator bug and no native bug. The generator's
"equip outside the bag" test lacked the 0x48 alignment: 0x48 bytes before
bag 2 is inside bag 1's byte range. The native refusal was right, so the
generator was fixed.

## Disc counts (owner's disc copy, read-only, counts only)

`champions-script-items-measure` (every `.GOB`, lump and `script.scr`):

- 120 scripts, 5,660 native calls;
- has/count **158** (61 + 36 + 61);
- removes **147**;
- gives **12** (10 + 2);
- `generateMonster` **136**.

All equal the research census.

## 111-level sweep (`work/items/run-slices.sh`, 60 ticks, 4 slices)

| Run | Tried / loaded / played | Failed checks | Script faults |
|---|---|---|---|
| `i1 --scripts` | 111 / 111 / 111 | 0 (119 passed) | 0 |
| `i1n` (no scripts) | 111 / 111 / 111 | 0 (119 passed) | 0 |

Against wave 17's `r17b` / `r17bn`:

- **Scripts off:** only the timing and process counters differ: load and
  play milliseconds, audio frames pushed (wall-clock driven), and static peak
  memory (−1.6 MB). The sweeps ran two at a time. Every content count is
  identical.
- **Scripts on:** the item commands now execute instead of refusing:
  - playerHasItem 91, onePlayerHasItem 72, countQuestItems 31,
    removePlayerItem 37, and 1 each of removeAllItems and removeAllWeapons;
  - their refusals are 0.
- **Why the other counts moved:** before, those refusals had used answers
  and stopped their entries. So:
  - refusal stops fell from 39 to 23;
  - wait / complete went from 14 / 28 to 25 / 33;
  - more script code runs: addDeferedCall 691 → 1,400, gamePaused 955 →
    1,990, addParticleOnObject 472 → 1,888, sin1000 and cos1000 472 → 1,180,
    getv 490 → 544, setv 82 → 96, callScript 38 → 100;
  - campaign variables are 158 (56 non-zero) → 177 (60);
  - the newly reached code meets other refusals: hideMonster 67 → 119,
    showMonster 7 → 14, getRand 5 → 8, cutScene 14 → 18, showBust 265 → 307,
    startDialog 0 → 1;
  - givePlayerItem refused 1 → 7. More gives are reached, and they refuse,
    because the session binds no seed and no item tables.
- **Stub refusals 102 → 166** (+64): hideMonster +52, showMonster +7, and
  generateMonster's 5. generateMonster now refuses on its stub type read
  instead of at the gate.
- `champions-script-commands-measure` and `champions-quests-measure`, built
  from `c42ad2d` and from this branch, show the same direction. The quests
  measure's mission, menu, act and table counts are unchanged. Its table is
  in `docs/QUESTS.md`.

## Open / unverified

- **The session binds no item tables and no seed.** The play session does not
  hand the executable to the script host. Both gives refuse in the sweep, so
  real-disc gives are verified only in synthetic vectors. Binding the tables
  and seed is the script-session job's.
- **No caller for the builder or the scan.** Nothing in the session calls
  `arena_spawn_monster` or `frontend_start`'s cm1 rescan, and `ScriptGenerator`
  objects stay unconstructed stubs. These are library contracts with tests.
- **Quests-port follow-ups.** The quests port's campaign array covers mission
  rows 0..4 from `+0x4bbf`. The research's rows are 1..5: row 5 is not
  mirrored, and `seed_new_character` marks rows 0..4. The quests measure's bit
  totals still include row 0.
- **Not composed here:**
  - weapon equips that run the notify scripts refuse;
  - `player_countPotions` and the rest of `player_recalcStats` are counted,
    not applied.
- **Not run this round:**
  - CTest on x86_64 (Rosetta);
  - the universal and release extension builds;
  - the export matrix;
  - manual play.
- **One process kill.** One `pkill -f real_data_scene.gd` was used to stop my
  own first sweep, which was racing on a shared cache. The other worktrees'
  sweep logs had all finished before then, so no other job's run was
  interrupted.
