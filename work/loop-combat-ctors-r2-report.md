STATUS: DONE, with one placement decision to confirm (the AntQueen stands at (-110, 110), not the round-5 (0, 100); see section 1).

# combat-ctors, round 2 (2026-10-21)

Branch `codex/combat-ctors`, worktree `~/Projects/open-champions/worktrees/combat-ctors`, continued from `ed062c3` without a rebase. Commits are local only. Nothing was pushed, and nothing was written in `~/champions-re` or in any research checkout. The private disc was not used: everything here is authored synthetic fixtures and pinned reference models.

Evidence: `t3/research-combat-ctors-2` at `5b928373a4bc31e5566312de598f861cc9f58071` (`~/Projects/champions-re-wt/research-combat-ctors-2`, clean):
- `docs/spec/combat-ctors-2.md`, `tools/ref_combat_ctors2.py`;
- `docs/spec/antqueen-run.md`, `tools/ref_antqueen_run.py`;
- `docs/spec/fire-touch.md`, `tools/ref_fire_touch.py`;
- `docs/spec/full-placement.md`, `tools/probe_full_placement.py`;
- `docs/runtime.md` 4.4, for the GameObject default slots (Observed).

Every research file the generators read is pinned by SHA-256, both in git at `5b92837` and on disk. They are listed in `native/tests/vectors/combat_ctors_2.json` and `fire_touch.json`, and the doc pins are in `docs/COMBAT_RUNTIME.md`. Every address and constant in the new code is quoted from those files.

| Commit | What |
|---|---|
| `098a495` | Merge of `codex/combat-runtime-6` (`396a112`) for `poisonGasSpell_run` (item 6). |
| `5c019fe` | The library port and the `combat-ctors-2-exact` contract test. |
| `900e81e` | The World integration, the fire entry, the Frost Armor refusal, `fire-touch-exact`, the AntQueen in the Full fight, the test updates and the docs. |
| (this commit) | ROADMAP line and this report. |

## 1. AntQueen_run, pickFlyDest, and the AntQueen in the Full fight (item 1)

**The port.** `native/src/combat_ctors_2.cpp` covers:
- `AntQueen_run` `0x236f78`, the whole switch: 26 states, plus the out-of-range return;
- the bank helpers `0x2385dc` and `0x238448`;
- `AntQueen_pickFlyDest` `0x238150`.

`AntQueen_setState` state 0x17 now executes `pickFlyDest`. The round-1 vectors pin `e9777eb`, where it was a call-out, so a model that answers it as a call-out still gets its answer and the round-1 test stays exact. States 7, 9 and 10 pass the poison stack descriptor `(0x17, v, v, 0)` into `damage_apply`; v comes from `SkillPoisonWeapons` slot 11. `AntWave_ctor` refuses, because state 0x19 has no requester (antqueen-run.md section 7).

**Contract test.** `combat-ctors-2-exact` runs the pinned `ref_antqueen_run` model on authored images:
- 200 AntQueen cases;
- every state 0..0x19 plus 0x1a, 0x1b, 0x40, 0x7fff, 0x8000 and 0xffff (counts are in the JSON);
- 216 `terrain_height` probes;
- 53 AntWave frames.

It compares call names and arguments, every stored byte, shared draws and results. As a check, I planted three mistakes: a state-6 reach of 40, a probe distance of 151, and `isEnemy` returning r instead of r + 1. Together they gave 328 failures.

**In the Full fight.** The AntQueen is in slot 4 and runs the ported `AntQueen_run`. Over the idle fight she follows the research probe's sequence:

| Ready tick | State |
|---|---|
| 33 | sees the player: 3 |
| 182 | takes off: 8 |
| from 220 | stings: 9, alternating with 8 |
| 388 | flies to the `pickFlyDest` point: 0x17 |
| 537 | lands: 0x18 |

She reaches no unexecuted body.

**Decision for you: the placement.** I did not use the round-5 placement. `(0, 140)` is off the synthetic level's flat ground (±128); `99f22ef` had moved it to `(0, 100)`, and at `(0, 140)` the Godot Full start fails. At `(0, 100)` she stands in the walk-and-combo lane:
- **The bleed refusal.** When she chases the player, his swings hit her. Her blood (`fx_spawn` 9, then the gib) reaches `GutChunk_ctor`, whose `PhProp_ctor` you asked me to keep refused, and the session stops.
- **Seed-sensitive scenarios.** `playable-combat-fx` and the Godot full-combat smoke could not finish their Skelight combo at that placement. Her placement also makes one more shared draw, which shifts every later roll.
- **Other fixes I tried.** Other walk lengths, steering back to the Skelight, and swinging only within reach all ended at `GutChunk_ctor` or a party wipe.

So she stands at `(-110, 110)`, an authored corner of the flat ground. Every suite passes there, and she still runs her full sequence. If you want `(0, 100)` instead, the two combo scenarios need re-authoring; the refusal stays the same.

## 2. Skeleton_isEnemy (item 2)

The port covers `Skeleton_isEnemy` `0x2a7e04`, the eight type-info getters (built on first use) and `rtti_typeInfoEquals`. The contract cases return 0 (171), 1 (12), 2 (70) and 5 (18). They include the synthetic `8Skeleton` descriptor at another address, which matches by name, and the `Creature.v0` getters.

In the World it replaces the round-1 refusal. The Skeleton stays out of the default Full fight (slot 5 is empty). A spawned Skeleton now gets past every target scan and stops at its first sighting of the player, at `Skeleton_run state 3: the switch past states 0 and 1 is not executed`:
- `fixture-save`: spawned at ready tick 3, it stops at **ready tick 66**;
- Godot wave-12: placed at (0, 80) with the player attacking, it stops at **ready tick 33**.

`creature_allyInLineOfFire` beside it was never reached, because the state refuses first.

## 3. The take-damage slots (item 3)

- **`Soul_takeDamage` `0x2aa908`.** Only an exact-class Demon attacker does anything: `Creature_takeDamage`, then exactly six flat particles of `0x8e4600` from own draws when the sample count is non-zero. A null attacker makes the `cxx_throwBadTypeid` call-out and then refuses, because what catches the throw is Unknown (section 8). The reference cases refuse at the same point (43 cases).
- **`AntQueen_takeDamage` and `Skeleton_takeDamage`.** Both are `Creature_takeDamage` with the same four arguments.

In the World:
- A Skelight that attacks the fixture's team-0 Soul no longer stops the session; the Soul ignores non-Demon attackers, and the Skelight runs all 299 wave-12 ticks.
- The World has no Demon class, so the particle path runs only in the contract test. If it is ever reached in the World with a non-zero sample count, it refuses at `model_getSamplePoint`.

## 4. The six destructors (item 4)

`AntQueen_dtor`, `Skeleton_dtor` and `Soul_dtor` run over `Creature_dtor`. `Fire_dtor`, `LooseFire_dtor` and `poisonGasSpell_dtor` run over `GameObject_dtor`, including its `name=` scan. In the contract test they cover flags 0..3, property lists and attached spells: 160 creature cases and 40 GameObject cases.

The World runs them on its model:
- `handle_clear`, `iter_*`, `Spell.v1` and `pool_free` are World services;
- `sub_202500` and `AnimState_dtor` are recorded;
- `objNames_remove` refuses. No World object has a property list, so it is never reached.

The three potion tables also get the GameObject default slots 7, 10, 11 and 12. `docs/runtime.md` 4.4 observes them in all 156 non-creature classes. Without them, a creature hit next to the gas cloud stopped at slot 12 (`world_notifyObjectsNear`).

**Potion 1 now runs past tick 101.** Its fires end at about tick 106 and its LooseFire at F + 121. It runs to **ready tick 725** in `fixture-save`, where the idle fight's own `party wipe: deathTask` stops it, and the Godot wave-12 run lasts its full 299 ticks. The mid-fight save case (save at tick 60, 18 fires burning) continues byte for byte through the destructors.

## 5. The fire entry and the burn tick (item 5)

`LooseFire_onTouch` now applies `(0x15, v, v, 0)` through the real `damage_apply`, with rating 10,000,000, flags `0x0100c040` and range 0..0. The burn tick in `Creature_run` was already ported.

`fire-touch-exact` replays 306 `damage_apply` rows through the native combat-damage harness:
- **Touch:** creature, player and plain touchers × fire, creature-owner and player-owner attackers.
- **Burn tick:** monster and player; the player's range draw is made first.
- **Re-touch:** a burning victim.

The rows vary fire resistance, armour, PvP mode A and a bounded Frost Armor. Their arguments are captured from `ref_fire_touch.loosefire_touch` and `burn_tick` themselves, on the pinned `ref_combat.py`, which is byte-identical to the combat-damage corpus pin. The 44,704-row combat-damage corpus is unchanged and passes.

**The Frost Armor endless re-entry case** is a carrier that hits itself with a strike-back chance of 100 or more. `frost_armor_on_hit` now refuses it right after the original's own roll and chance lookup, so the call sequence is unchanged. It no longer runs into the 128-deep recursion limit. The harness anchor checks both sides: the refusal comes before any loop, and a chance of 30 still runs.

## 6. The poison gas run (item 6)

`codex/combat-runtime-6` (`396a112`, `STATUS: DONE`) had already ported `poisonGasSpell_run` from round 6 (`faa7033`). I merged that branch rather than port it again, so wave 18 sees one copy. Three conflicts were resolved: the cmake source list, the flat fixture services, and COMBAT_RUNTIME.md, where both sections are kept. The round-1 audit fix that refuses `DiseaseCloud_dtor` survived the merge.

**Potion 2 no longer stops at tick 2.** Its cloud runs, ends through `poisonGasSpell_dtor`, and the session reaches **ready tick 787** (party wipe) in `fixture-save`; the Godot wave-12 run lasts its full 299 ticks.

## 7. Kept refused

- `Skeleton_run` past states 0 and 1, and `creature_allyInLineOfFire`.
- `Skeleton_onNearbyEvent` (Skeleton slot 12).
- The Soul's states 0 and 1 past the demon check: no new code; they keep round 1's switch. One note: full-placement.md ranks them unexecuted because its probe counts only the combat-round tables. Research-anim's `own-state-machines.md` section 11 executed their decisions with the prologue stubbed, and round 1 ported that.
- `Creature_setCharm` beyond its Observed prose.
- `GutChunk_ctor` (`PhProp_ctor`).
- The death path: `Skeleton_shatter`, loot, experience and `anim_findTrack`.
- `AntWave_ctor`.
- A Soul's model sample points in the World.

## 8. FULL_TRACE

`41e063910f5d82df9fa5ed2371f4544de96c9aefebcdc25792327b87e14fee75` (26,856 bytes) became **`4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9` (31,622 bytes)** (headless).

I rebuilt this branch with slot 4 left empty and the hash went back to `41e06391…` exactly. So the merge, the destructors, the fire entry and the gas run don't change it; the AntQueen is the only cause:
- her placement's GameObject shared draw shifts every later shared draw;
- every trace line gains her as an actor;
- her runs add own-generator draws and state changes.

## 9. How far the fights run now

| Run (`fixture-save`, up to 4,000 ready ticks) | Stops at | Reason |
|---|---|---|
| Idle Full fight, AntQueen in slot 4 | 539 | `party wipe: deathTask` |
| Potion 1 | 725 | the same party wipe |
| Potion 2 | 787 | the same party wipe |
| Spawned Skeleton | 66 | `Skeleton_run state 3` |

In the idle fight only the two Skelights damage the player. The fixture's clips carry no events for the AntQueen, so her bites and stings never land. Her placement draw shifts the Skelights' rolls.

## 10. Verification

All builds are Release.

- **arm64** (`build/native`): `ctest -j 3` passed **213 of 213** in 1,064 s, under a load average around 50 from other agents. Log: `work/combat-ctors-2/ctest-arm64.log`.
- **x86_64 under Rosetta** (`build/native-x64`, `CMAKE_OSX_ARCHITECTURES=x86_64`; binaries checked with `file`): `ctest -j 3` passed **212 of 213** in 1,842 s. The one failure, `player-exact`, only timed out at 240 s. Rerun alone, it passed in 233 s; the load average was 66 during the rerun. Logs: `work/combat-ctors-2/ctest-x64.log` and `ctest-x64-player-rerun.log`.
- **Godot.** I rebuilt `godot-extension`, `creature-godot` and `godot-level-texture` from this branch and ran round 1's 45-command dev matrix (`work/combat-ctors-2/run-godot.py`): **45 of 45 exited 0**.
  - `combined-full-combat`: 2,619 checks, 0 failures.
  - `wave12-combat-extensions`: 28 checks, 0 failures, with its expectations updated to the new behaviour.
  - Logs: `work/combat-ctors-2/godot-*.log`.
- **fixture-save:** 92 checks, 0 failures. The 300-tick Full fight still continues byte for byte after the mid-fight load. It now has 19 demon tests and 2,169 Holy Shield sample points, both unchanged. The combat-ctors save case also continues byte for byte.

Key commands:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_combat_ctors_2_vectors.py   # reproducible (same content hash)
PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_fire_touch_vectors.py      # reproducible (same content hash)
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4
cd build/native && ctest -j 3
```

## 11. Open

- **The placement decision** in section 1.
- **Wave-18 merge.** `combat-runtime-6` is already merged here.
- **Test updates.** I updated the tests that pinned round-1 refusals:
  - `potion-ctors-exact`;
  - `combat-runtime-5-host`;
  - `fixture-save`;
  - the Godot wave-12 smoke;
  - the combat-damage Frost Armor anchor.

  Each checks the new executed behaviour instead.
- **Two World fixes:**
  - `terrain_height` now uses the Full World's `terrain_height` service when it has one; before, it used `height_below`. On the flat fixture the results are identical.
  - The potion tables gain the GameObject default slots 7, 10, 11 and 12.
- **Newer research.** A newer research checkout, `research-combat-ctors-3`, exists. It was not read or used.
