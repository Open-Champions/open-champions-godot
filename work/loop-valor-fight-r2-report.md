STATUS: PARTIAL — the scripted `valor` fight now ends in a natural kill (a WarBoarSmall's hit points reach zero through the real damage path), the kill's XP raises the session player to level 2, and the loot roll runs over the real shared seed (this seed yields one equipment record); the dropped item's ground object, its pickup and Gold stay refused, with the missing evidence named below.

# Valor fight round 2 — 2026-10-23

Branch `codex/valor-fight`, from 9251556. Evidence pins: `t3/research-levelup` 576b41c, `t3/research-pickups` 9911a6b, `t3/research-campaign-combat` 328b072 (and the research-combat-ctors-4 6baf4cd specs those branches cite). The owner's RTA US v2.00 disc was read-only (994 files before and after the playthrough). Item names, table values and bytes are in no tracked file or report. No push.

## 1. The iterator bound, and a natural kill

**Finding.** "combat iterator retention budget exceeded" was a host limit, not a game rule. The combat World kept every iterator `walk()`/`iter_end` finished (for the lifetime of the World, so stale node references could read their `prev` links) and refused at 65,536; four list walks a tick reach that in ~5,800 ticks. The original's iterators are stack objects.

**Change (`cb1f6af`).** Finished iterators are released when nothing refers to them any more: no list head, pool node, object head or live iterator (`World::collect_link_references`, shared with the objects World's own `prune_iterators`). Pruning is lazy (at 4,096, then at twice what survives) and the 65,536 refusal now only fires on a runaway count of *referenced* iterators. New test `long_run_releases_iterators` (24,000 ticks, ~96,000 walks) in `campaign-fight-adopt`; it fails without the pruning ("combat iterator retention budget exceeded") and passes with it. A 120,000-tick probe fight ran to the end without a bound.

**The kill.** An unarmed level-1 player (the labelled stand-in; nothing is forced) needs ~190,000 ticks against the nearest boar (a large boar, 686.5 HP, 439 HP lost in 120,000 ticks), so the scripted run places the player at the boar with the **lowest hit points** (a WarBoarSmall, 214.48 HP). Only that placement is a diagnostic (`fight_place_player`, labelled in code, probe, playthrough and result); the organic WASD walk is still run first and reported (stops 925.5 units short, as in round 1). No HP write, no damage boost.

Playthrough step 4, headless and rendered (identical numbers):
- target: WarBoarSmall, HP 214.48 → **−1.76**, state 0x14 (20), `dead` flag set, `deaths` 1, `kill_evidence` true (that handle, hit points at or below zero, death transition counted).
- 347 swing cycles (J held 6 ticks, released 24), 10,945 ticks, 347 `damage_apply` calls by the player, 103 with damage; ~41 s wall time headless, ~42 s rendered.
- the death chain that runs: `Creature_processDamage` → state 0x14 → `Creature_dropLoot` (rank 0, zero Gold chance) → `loot_generateDrops` → XP award. Still refused and labelled: the death-animation clip choice (own draw → `death1`/`death2`); a dead boar is drawn faded as a presentation-only marker.

## 2. XP and level-up (`4294f1c`)

Evidence: `levelup.md` ("Kill award and sharing", "XP table and level transition"), `data-tables.md` 3.5 / 4.3 / 4.4. `Creature_processDamage`'s kill award (`arena_monsters.cpp`) was already executed against the research model; this round binds its `player_giveExp` call (new `Services::give_exp`, optional) to the session player's record in the shared bytes (`campaign_rewards::give_exp`): experience clamped to `table[80] − 1`; per level gained skill points by the old level band, stat points +3, earned level +1, the `player_recalcStats` maxima, refill if the record's HP was positive; acknowledged level untouched. The 81 thresholds and the class formulas come from the executable at run time (`player_state::read_tables`); `player_state::maxima` is the formula `create()` already used, factored out. `SkillLifeTap_getStat0Fraction` is answered only for rank 0 (0.0: `skill_get_stat` returns 0 for rank 0); a nonzero rank stays refused.

Result in the playthrough: award 2,100 XP (slot 0), level 1 → **2**, experience 0 → 2,100, skill points +1, stat points +3, maximum HP 32.8 → 43.4 and HP refilled (the player tick mirrors the record into the object), maximum mana recomputed. Experience, level and HP survive the in-game save and load (`load_and_carry`: 2,100 / level 2 / 43.4 before and after).

Not modelled (counted refusal): the rest of `player_recalcStats` (armour, attack rating, load limit, bonus bytes, item modifiers, figurines); the session has no equipment, and the research ties attack rating and skill requirements to the acknowledged level.

Tests: `campaign-rewards` (new): one level, below threshold, four levels (skill points 1+1+2+2, stat points 12), dead record not refilled, ceiling clamp, level 80 last, slot refusal.

## 3. Loot

`Creature_dropLoot` calls `loot_generateDrops` once per boar death. `campaign_rewards::Loot` now runs the audited items generator (`item_generation.cpp`, the port of `ref_items.loot_generate_drops`) with item tables read from the executable, the game inputs from the shared bytes (player count/classes, difficulty 0x4f719c, PvP flags, level monster level 0x4f714c) and the shared seed 0x404464, in the documented draw order, writing the seed back. Each "make the item prop" step takes the documented one shared draw (`GameObject_ctor`) and keeps the generated 0x48-byte record.

Result for this seed: **one equipment record** (category 0, count 1), 2,335 shared draws; the kill's Gold chance is zero so no Gold roll. It is reported as `fight_ground_drops` with `spawned: false`.

**Refused: the ground object and the pickup.** `pickups.md` documents the lifetime from the flying `PhProp` on (`PhProp_run` rest → `ItemProp_ctor3`, ported as `phprop_run`) and the pickup rules (`Player_interact` → `itemprop_tryPickup` → `inventory_giveItem`, 63-cell cap, load gate), but the research never records the loot path's own constructor call. Missing exactly:
1. the loot `PhProp_ctor` 0x155a0c arguments: lump, model and texture names, part mask, launch flags and scale (`items.md` section 8 says only "`PhProp_ctor` at (x + 0.0, y, z + zOffset)");
2. the launch velocity beyond x/y (`cos16/sin16 × 24 × throwSpeed`) and the `PhProp_initPhysics` call's inputs after it;
3. the item models' bounds (the combat World's `model_getBounds` is a placeholder; the rigid-body extents need them);
4. the body of `Player_interact` for an interactable object and the `ItemProp` prompt/interact slots 13/14 (the combat World refuses `Player_interact` with an interactable in reach), and the session's inventory bags (`player_giveStartingItems` is not ported, VF-1).
Placing a resting item at the corpse would invent its landing spot, so nothing is placed. Documented in `docs/CAMPAIGN_FIGHT_REWARDS.md`. Tests: `campaign-rewards` (a seed with no item and 7–9 draws; a seed with an item, deterministic; the seed advanced by exactly the draws taken; a positive Gold roll refused; no tables refused).

## 4. Gold

Refused. A boar death gives zero Gold; round 8's only Gold came from `AutoContainer_run`. Neither research branch documents the class to port level: `AutoObject_ctor` 0x23a9b0, the container constructor and `takeDamage` bodies, the exact own-draw order and arithmetic before the Gold roll (six `fx_spawn` from three own draws each, one `PhProp` per debris mesh), `obj_moveWithCollision`, and the Gold touch dispatch from the player's move step. `Gold_ctor2`, `Gold_run` and `hud_tickGold` are in the tree; the source object is not. Gold stays 500 → 500.

## Files

Native: `campaign_rewards.{hpp,cpp}` (new), `campaign_fight.{hpp,cpp}`, `campaign_fight_probe.cpp` (`--near-weakest`, per-boar lines, award and drop lines; diagnostics labelled), `combat_runtime_{world,services,internal}`, `combat_runtime.hpp` (`Services::give_exp`), `objects.hpp` / `objects_lifetime.cpp`, `player_state.{hpp,cpp}`, `campaign_fight_targets.cmake`; tests `campaign_rewards_tests.cpp` (new), `campaign_fight_adopt_tests.cpp`. Godot: `session_controller.cpp` (observation), `loop_playthrough.gd`, `session_sandbox.gd` (health label), `real_fight_enemies.gd` (dead boar faded). Docs: `docs/CAMPAIGN_FIGHT_REWARDS.md`.

## Verification (this tree, arm64, Release, final source)

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4` and `cmake --build build/godot-extension -j 4`: passed.
- `ctest --test-dir build/native -j 3 --output-on-failure`: **241/241 passed** in 637.65 s (240 + `campaign-rewards`), no timeouts, no reruns.
- Godot 4.5.2 development matrix (`playable-wave26/run-godot.py`, copied with its output directory changed): **54/54** exit 0. `FULL_TRACE` 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, **unchanged** (the Full fixture has no session record, creatures or loot; the only shared-code change it runs is the iterator release, which touches no simulated state). `FULL_SAVE_LOAD` 300/300.
- 111-level sweep (`play-enemies/run-sweep.sh`, four slices, all exit 0): **111 tried / 111 loaded / 111 played, 230 checks passed, 0 failures, no refusal kinds**; 111 scripts attached, 76 started.
- `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/loop-valor-fight-r2/final`: 24 steps each, 13 screenshots rendered, headless and rendered identical in every field but timing; exit 1 only because the journey is still incomplete (`enemy_fight` is `blocked_unported_combat`: item ground object/pickup and Gold). Disc: 994 files before and after.
- Rendered inspection: `work/loop-valor-fight-r2/final/rendered/05-fight-attempt.png` shows the level-2 HUD badge, the health label "Level 2 · Experience 2100 · kills 1 · loot rolls 1 (items 1, no ground object) · Gold 500" and the class-6 model beside a textured boar.
- Probe (`champions-campaign-fight-probe --game "$RTA_DISC" --level valor --ticks 26000 --near-weakest --attack-every 8`, diagnostic placement): same kill, award and loot roll; `--ticks 120000 --near` (large boar): no bound, no kill.

## Unverified

- That this seed's loot roll matches a real campaign kill (the shared seed at the kill, the route's equipped player and the real level load draws are not captured; `scripts.load_draws_unmodelled` is nonzero on levels).
- Kill speed and damage are a stand-in's (unarmed, class 5), not campaign evidence.
- The other `player_recalcStats` effects, the death animation, boar hit reactions and attacks, WarBoar's slot-25 `force`.
- x64 traces were not compared.
