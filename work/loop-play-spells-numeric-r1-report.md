STATUS: PASS — synthetic fixtures. The live changes are the heal product and the cast payment. Audit play-spells-numeric-r1 findings PSN-1 to PSN-6 are fixed (round 2). Wave-37 gates held: CTest 260/260, matrix 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged. Journey wrapper exit 1 is the retained death refusal, at 80/81 steps with pickup 10→11.

# Spell numerics — 2026-11-03, fix round 2026-11-09

Branch `codex/play-spells-numeric`, from wave 37 `70d2de19`. Research pin `t3/research-spells-r3` `cf8320e`. Contract `docs/spec/player-spells-numeric-r3.md` ("spec") and `tools/difftest_spell_numeric_r3.py` ("difftest") at that commit. Spells round 5 and the later research commits were not read or used.

## What is live in the client

- **Cast payment.** `Tick::cast_skill` (`native/src/player.cpp`) calls `spell_numeric::recast_blocks`, `copy_blocks` and `cast_payment` (`native/include/champions/spell_numerics.hpp`). No other copy of the debit exists. The header and `spell_numerics.cpp` are in the simulation-boundary census (`tools/check_simulation_boundary.py`, 82 files; `tools/test_core_simulation_boundary.py` checks both).
- **ID 55 product.** `healing_spell_ctor`, `heal_share` and the `+0xac` add in `healing_run` call `heal_grouped_product` and `add_heal`. The arithmetic is byte-identical to base.

Everything else in `spell_numerics` is a synthetic fixture. The client does not call it.

## Cast payment against the research

Observed (spec :94-108; difftest :310-367): kind 0, skill recast 30, skill bit 0 clear, cast slot returning 0. `spell-numerics` now drives the live `Tick::cast_skill` with that layout and checks fixed values:

| Case | Record `+0xc0` | `+0x4ae` |
| --- | --- | --- |
| Mana = cost 40 | `0x00000000` | 30, read from skill `+0xc` |
| One pattern below (`0x421fffff`) | `0xb6800000` (below 0) | 30 |
| 1000 | `0x44700000` (960) | 30 |
| Recast 1 | unchanged, no writes | 1 |
| Prediction copy `0x20000`, bit 0 clear | unchanged, no writes | 0 |

The expected bit patterns were computed with `tools/ref_core.py` at `cf8320e` (ps2 model). They are not the code's own output.

Three live-vs-copy differences from round 1, settled against the research:

- **Recast signedness.** The gate reads the halfword as signed (`recast_blocks`). The round-1 copy took it unsigned. Recast −1 (`0xffff`) therefore does not block. The note ran recast 0 and 1 only (spec :100-103; difftest :362, :364).
- **Copy with bit 0 set.** `copy_blocks` returns only when bit 0 is clear. A copy with bit 0 set gets past the gate and pays. The note ran bit 0 clear only (spec :95; difftest :354).
- **Debit after a returned spell.** The live path pays after any cast return. The note stubbed the slot to 0 (spec :96-97). It lists payment with the real cast body as Unknown and leaves it to the player-tick contract (spec :110-112).
- Also kind ≠ 0: kind 1 multiplies the cost by `0x3c888888`. The note ran kind 0 only (spec :95).

These four cases are refused as research-spells-r3 claims. `cast_payment` sets one `Unprobed` bit for each, and the test asserts each bit. The live path still pays in these cases. That behaviour is the player-tick contract (`ref_player.py` at `356add27`), and the pinned vectors cover it: `player-exact` `casting_cast_gates` gate 5 (recast `0xffff`), `casting_cast_kind_copy` (bit 1, kinds 0/1/−1, spell 0/non-zero). Refusing these cases in the live path would change those 274,304 vectors and stop spells costing mana in play, so the behaviour is kept and labelled instead. `player-exact` passes unchanged.

## Synthetic fixtures

- **Creature hit.** `deliver_creature_hit` / `deliver_cold_hit` chain the live `combat::damage_apply` `0x1e8230` and `creature_hp::Model::take_damage` `0x116638` (spec :33-36, :43-61). The world's take-damage hook is swapped only for the call and restored on every exit. Earlier `world.events` are kept. The test checks that the hook is restored and the events survive.
  - Equal range 40, attack 100000, flags `0x80000` (seed 20973) and 0 (seed 7), hand 40.
  - Hit points 100 → 60 (`0x42700000`).
  - From a zero start, `+0xc8` = 40 and `+0x2dc` = `0x80000`, a fixed value. The caster is a player, so `damage_apply` adds no `0x1000000` and hands the flags unchanged (`ref_combat.py:1154-1155`, `:1389`; difftest :213, :233-234).
  - From a nonzero start (`+0xc8` = 1, `+0x2dc` = `0x11`), `creature_hp` adds and ORs: 41 (`0x42240000`) and `0x80011`. The note ran a zero start only, so this is the `creature_hp` contract, not a research claim.
  - Armour 3000 on a `0x60` victim floors to 20 and does not enter the player slot. Invulnerability `0x800` returns 0. Lethal 40 → 0 without death.
- **Cold.** Descriptor `{0x16, 30, 30, 0}`. Resistance 0 hands 30. The timer is 18 (`0x41900000`) at 100 hit points and 60 at 0. Resistance 1 hands 0, timer 0 (spec :56-59).
- **Stun.** No copy remains: `bash_stun` and `bash_frames` were removed. The test drives the live `monsters_1` slot 30 (`0x118a58`), with the base state body `0x3c2688` in slot 27, as the note ran it (spec :70-75; `ref_creature.py:2207-2210`, `:2249-2260`):
  - frames 420 at `+0x31c`, state `0x13`;
  - flash 1 sets all eight halfwords from `+0x90` to `0x3a, 0x9b, 0x9a, 0x48, 0x3a, 0x9b, 0x9a, 0`, and flash 0 leaves all eight at 0 (difftest :268-272);
  - boss rank 1 and corpse `0x14` store nothing.
  - The derived slot 27 stays Unknown (spec :77).
- **Heal.** The grouped product for maximum 100 and percent 10 is `0x3e170a5b`, and one add onto 50 is `0x4248970a` (`ref_core.py` at `cf8320e`, spec :85).
  - The note observed the owner amount only: statistic 0 and the owner record `+0xb0` (spec :81-89).
  - The non-owner share (`heal_share`: statistic 2 and the recipient's `+0xb0`) and the creature-owner ctor branch use the same grouping, but they come from the earlier `skills_b` contract. The comments say so, and the helper is named for the grouping, not the owner.

`spell-numerics`: 62 checks, 0 failures.

## Refusals

Kept, with the reasons in spec :63-66 and :129-130:

- The player damage slot. A `0x60` victim still gets the armour-floor amount from `damage_apply`. The player slot is not entered.
- Death and knock-back, which are `Creature_processDamage` `0x110d30`. Reaching 0 hit points does not call it.
- Real spatial queries.
- Payment with the real cast body. Recast below 0, a copy with bit 0 set and kind ≠ 0 are flagged as unprobed (see above).
- The derived stun slot 27, the stun countdown, armour and resistance combinations not listed in the note, and mana regeneration.
- Spells round 5. Its results were not used.

## Gates (round 2)

All gates ran on the mini, Release, with standard input closed. Godot is `runtime/Godot.app` in the mini's job folder. The private disc was read-only: 994 before and 994 after files. The mini `build/` directory was removed after the gates. Evidence is in `work/play-spells-numeric/`.

| Gate | Result |
| --- | --- |
| Release `cmake --build build/native -j 4` | PASS, exit 0 (`native-build.txt`) |
| GDExtensions, universal `arm64` + `x86_64` | PASS. Archive and level-texture `template_debug` and `template_release`, creature-animation `template_debug`; `lipo -archs` is `x86_64 arm64` on all five (`ext-lipo.txt`) |
| Focused | PASS. `spell-numerics` (62), `simulation-boundary`, `core-trace-integrity`, `player-host`, `player-vector-harness`, `skills-b-host`, `skills-b-exact`, `monsters-1-unit`, `monsters-1-exact` 9/9, and `player-exact` 1/1 (`focused-tests.txt`) |
| Full CTest `-j 3` | PASS, **260/260**, 969.66 s (`ctest.out`) |
| Strict Godot matrix `--captures`, timeout 1200 | PASS, **54/54**, 0 failed. Headless editor import exit 0 first (`matrix-driver.out`) |
| Full combat | **2,645 checks, 0 failures** (`full-combat.txt`) |
| FULL_SAVE_LOAD | **300/300**. Continued and uninterrupted presentation `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573` (`full-save-load.txt`) |
| FULL_TRACE | Unchanged: **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` (`full-trace.txt`). No trace or baseline was edited |

The trace is unchanged, as expected. The payment refactor keeps the read order and arithmetic of `Player_castSkill`, and `player-exact` confirms this on every pinned casting vector.

## Journey

`tools/run_loop_playthrough.py`, headless then rendered (`playthrough.out`, `playthrough-run-summary.json`).

Wrapper exit 1. Log errors 0. Disc 994/994.

| Variant | Steps | Exit | Pickup | Walk ticks | Screenshots | Log errors |
| --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | 10→11, produced, walked, picked up | 52 | 0 | 0 |
| Rendered | 81 | 1 | 10→11, produced, walked, picked up | 52 | 38 | 0 |

Each variant has six refusal kinds. `player_death` stays refused, so `saved_level_reloaded` is not reached. The rendered extra step is `mission_menu_render`, refused. Step and result pairs are identical to the round-1 journey, which matched wave 37: 80/80 headless, 81/81 rendered. Only timing and memory differ (`fps`, `memory_bytes`). `rendered_pickup_verified` stays false.

## Files

- `native/include/champions/spell_numerics.hpp`, `native/src/spell_numerics.cpp`, `native/tests/spell_numerics_tests.cpp`
- `native/src/player.cpp` (payment through `spell_numeric`)
- `native/src/skills_b.cpp` (ID 55 grouping)
- `native/CMakeLists.txt`, `native/simulation_targets.cmake`
- `tools/check_simulation_boundary.py`, `tools/test_core_simulation_boundary.py` (census 80 → 82, plus a spell-numerics check)
- `work/play-spells-numeric/` holds the gate evidence. The round-1 generic `work/` gate files were removed.

There was no push and no pull request. Nothing was edited in the research checkout or in the playable-integration worktree.
