STATUS: PASS — audited spell numerics are in the client and the wave-37 gates held. Journey wrapper exit 1 is the retained death refusal, at 80/81 steps with pickup 10→11.

# Spell numerics — 2026-11-03

Branch `codex/play-spells-numeric`, from wave 37 `70d2de19`. Research pin `t3/research-spells-r3` `cf8320e` (audit `cursor-runs/audits/research-spells-r4.md` PASS). Contract `research-spells-r3/docs/spec/player-spells-numeric-r3.md`. Spells round 5 was not read and was not used.

## What changed

Synthetic delivery for the observed subset only.

- Creature hits go through `damage_apply` `0x1e8230`, then `Creature_takeDamage` `0x116638` when the victim does not carry the player flag. Pending `+0xc8` stores the handed amount and `+0x2dc` stores the flag word (`player-spells-numeric-r3.md:43-61`). Equal range 40 with attack 100000 hands 40 for flags `0x80000` and 0, on seeds 20973 and 7. Armour 3000 on a `0x60` victim floors 40 to 20 (`0x41a00000`). Invulnerability `0x800` returns 0 and leaves hit points unchanged. Hit points equal to the handed 40 become 0 without entering death.
- Cold descriptor type `0x16` with both values 30 and no direct damage hands 30 when cold resistance is 0. The timer is `30 * 60 / hit points` at 100 hit points and 60 at 0 hit points. Cold resistance 1 hands 0 and leaves the timer 0 (`player-spells-numeric-r3.md:56-59`).
- ID 49 stun stores `statistic * 60` at `+0x31c` and state `0x13`. Flash 1 writes `0x3a, 0x9b, 0x9a, 0x48, 0x3a, 0x9b, 0x9a, 0` from `+0x90`. Flash 0 leaves colours unchanged. Boss rank 1 and corpse state `0x14` store nothing (`player-spells-numeric-r3.md:69-75`). The live creature slot 30 already had this body; the new entry is the direct fixture, not a vtable.
- ID 55 owner amount is `float32(float32(maximum * 0x3c71aa2e) * float32(float32(percent) * 0x3c23d70a))`, and one run adds that cached product to record `+0xac` (`player-spells-numeric-r3.md:81-88`). `healing_spell_ctor` and the player-owner add now call that product.
- After a zero cast return the helper subtracts the slot-5 float and stores the skill recast halfword. Mana one pattern below the cost still debits and becomes negative. Recast already 1, and the prediction copy, do not debit (`player-spells-numeric-r3.md:97-107`). `Player_castSkill` already returned in those two cases before the subtract. The simulation-boundary check does not allow `player.cpp` to include the new header, so the live subtract stays in `player.cpp` and the cases are locked by `spell-numerics`.

`spell-numerics`: 43 synthetic checks, 0 failures.

## Refusals

Kept, with the reasons in `player-spells-numeric-r3.md:63-66` and `:129-130`:

- Player damage slot. A `0x60` victim still gets the armour-floor amount from `damage_apply`. The player slot is not entered.
- Death and knock-back. Those are `Creature_processDamage` `0x110d30`. Reaching 0 hit points does not call it.
- Real spatial queries. Bash arc/sphere search and heal recipient search are not new work here.
- Real cast body inside the helper. Payment is the zero-return case only.
- Derived stun slot 27, the stun countdown, unlisted armour and resistance combinations, and mana regeneration stay unknown in that note.
- Spells round 5 (player damage and knock-back) is in progress. Its uncommitted and unaudited results were not used.

## Gates

Mini, Release, stdin closed. Godot `~/malik-champions/runtime/Godot.app`. Disc read-only, 994 files before and after. The mini `build/` directory was removed after the gates.

| Gate | Result |
| --- | --- |
| Release `cmake --build build/native -j 4` | PASS, exit 0, about 113 s |
| GDExtensions, universal `arm64` + `x86_64` | PASS. Archive and level-texture `template_debug` and `template_release`, creature-animation `template_debug`. `lipo -archs` is `x86_64 arm64` on all five |
| Focused `spell-numerics` | PASS, 43 checks |
| Full CTest `-j 3` | PASS, **260/260**, 727.55 s. An earlier run was 259/260 because `player.cpp` included the new header; `simulation-boundary` then passed alone and again in this full run |
| Strict Godot matrix `--captures`, timeout 1200 | PASS, **54/54**, 0 failed. Editor import exit 0 |
| Full combat | **2,645 checks, 0 failures** |
| FULL_SAVE_LOAD | **300/300**. Continued and uninterrupted presentation `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573` |
| FULL_TRACE | Unchanged, **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. The trace was not edited |

## Journey

`tools/run_loop_playthrough.py`, headless then rendered. Wrapper exit 1. Log errors 0. Disc 994/994.

| Variant | Steps | Exit | Pickup | Walk ticks | Screenshots | Log errors |
| --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | 10→11, produced, walked, picked up | 52 | 0 | 0 |
| Rendered | 81 | 1 | 10→11, produced, walked, picked up | 52 | 38 | 0 |

Six refusal kinds each. `player_death` stays refused, so `saved_level_reloaded` is not reached. The rendered extra step is `mission_menu_render` refused. Step and result pairs match the wave-37 journey at 80/81. Host-only differences from that run are timing and memory (`fps`, `memory_bytes`), not step results. `rendered_pickup_verified` stays false.

## Files

- `native/include/champions/spell_numerics.hpp`
- `native/src/spell_numerics.cpp`
- `native/tests/spell_numerics_tests.cpp`
- `native/src/skills_b.cpp` (ID 55 product)
- `native/src/player.cpp` (debit comment only; the subtract was already the observed branch)
- `native/CMakeLists.txt`, `native/simulation_targets.cmake`

No push, no pull request, no edits under `~/champions-re` or the playable-integration worktree.
