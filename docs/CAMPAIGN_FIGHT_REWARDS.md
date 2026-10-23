# What a kill gives the session player (valor fight, round 2)

Native code: `native/src/campaign_rewards.cpp` (`campaign_rewards::give_exp`,
`campaign_rewards::Loot`), wired into `campaign_fight.cpp`. Evidence is the
research repositories only: `t3/research-levelup` 576b41c
(`docs/spec/levelup.md`), `t3/research-pickups` 9911a6b (`docs/pickups.md`,
`docs/spec/items.md` section 8, `docs/issues/kill-loot-xp.md`) and
`t3/research-campaign-combat` 328b072 (`docs/data-tables.md` 3.5 and 4.3-4.4,
`docs/monsters-2.md` AutoContainer, `docs/issues/campaign-combat-handoff.md`
rounds 3 and 8). No table value is in the tree: the experience thresholds, the
class formulas and the item tables are read from the user's executable at run
time.

## Kill XP and level-up

`Creature_processDamage` awards the kill (`arena_monsters.cpp`, already
executed against the research model): `xp = trunc(trunc(pow(expValue / sum of
earned levels, 1.4)) * 0.75)`, capped for one player at
`table[level + 1] - table[level]`, handed to `player_giveExp(slot, xp)`.

`player_giveExp` 0x1d3150 now runs on the session player's record in the shared
bytes (`give_exp`): experience (+0x5c) += amount, clamped to `table[80] - 1`;
while experience >= `table[earned level]`: skill points (+0xe8) by the *old*
level band (1, 2, 3, 4, 5 from levels 1-5, 6-15, 16-25, 26-35, 36+), stat
points (+0xec) += 3, earned level (+0x59) += 1, the maxima of
`player_recalcStats` (below), and if the record's current hit points were
positive both currents are refilled. The acknowledged level (+0x58) is left to
the level-up panel, as in the research. The player tick copies the record's hit
points into the object (+0x228) every tick, so the refill shows in play.

`player_recalcStats` 0x1b6458 is modelled only for what depends on the earned
level in the research: `maxHp = level * hpPerLevel + stamina * hpPerStamina -
hpOffset` and `maxMana = (intelligence - manaIntOffset) * manaPerInt + level *
manaPerLevel` (`data-tables.md` 4.3; `player_state::maxima`, which creation
uses too), over the effective statistics (+0x90). Item modifiers and figurines
are not in this session (no equipment), so effective equals base. The other
derived fields (armour, attack rating, load limit, bonus bytes) are not
recomputed; the research ties attack rating and skill requirements to the
acknowledged level, not the earned one. Counted as a refusal.

`SkillLifeTap_getStat0Fraction` (the orb chance in the kill path) is answered
only for rank 0, which is the session player: statistic getters return 0 for
rank 0 (`tools/ref_skills_b.py skill_get_stat`), so the chance is 0.0. A
nonzero rank is refused (the skill tables are not bound).

## Loot

`Creature_dropLoot` (rank 0, zero Gold chance) calls `loot_generateDrops`
0x100d54 once. `campaign_rewards::Loot` runs the audited items generator
(`item_generation.cpp`, the port of `ref_items.loot_generate_drops`) with the
tables from the executable, the player count, classes and PvP flags from the
shared bytes and the shared seed 0x404464: draws happen in the documented
order, and the seed is written back.

**The level's monster level (`g_levelMonsterLevel` 0x4f714c) and the difficulty
(`g_difficultyIdx` 0x4f719c) are bound, not read from the World.** Nothing in
the fight path stores them in the World's bytes (they hold a 1.0 constructor
default and zero), and the original sets them in `game_loadLevel` (research-pickups
9911a6b `docs/data-tables.md` section 5: level-table byte + 2 on difficulty 1,
`(byte + 2) x 0.75 + {34, 61, 88, 115}` on difficulties 2-5, 1.0 in `stomedal`,
2.0 in `innpit`). They come from the script host's own level load
(`script_commands::Host::level_load_globals`): `level_flow::monster_level` of
the loaded level's table row and the host's `g_difficultyIdx`, which is 1 in a
single-player game (the executable image value and the offline setup's store,
`script_commands.hpp`; research `native-app.md`/`native-rendezvous.md`: difficulty
1). They are available only while the difficulty word is the 1 the level value
was computed for, with no PvP; otherwise, or before the level's table is
loaded, the roll is **refused**: a counted `loot_generateDrops: the level's
monster level and the difficulty are not bound ...` refusal, no draw is taken
and the shared seed is untouched. `Loot::roll` itself refuses until
`bind_level_globals` was called and rejects a difficulty outside 1..5.
Which difficulty a real campaign run has is not recorded for this session
beyond that image value (Unverified).

Each "make the item prop" step takes the one shared draw `GameObject_ctor`
takes (`items.md` section 8) and keeps the generated 0x48-byte record. **No
ground object is built**: `pool_new(0x248)` / `PhProp_ctor 0x155a0c` for the
loot path needs the lump, model and texture names, part mask, launch flags and
scale it is called with, the launch velocity and `PhProp_initPhysics` inputs
(`items.md` gives only the x/y velocity), and the item models' bounds; none is
in the research. So the flight, the rest as an `ItemProp` (documented in
`phprop-run.md`, ported as `phprop_run`) and the pick-up (`Player_interact` ->
`itemprop_tryPickup` -> `inventory_giveItem`, documented in `pickups.md` section
3) have nothing to act on. The generated item shows in the observation as
`fight_ground_drops` with `spawned: false` and as a counted refusal.

## Gold

Not reachable. A boar's death passes a zero Gold chance (round 3), and the
only Gold in the round-8 capture came from `AutoContainer_run`. The class is
described (`monsters-2.md` section 6: properties, states, the loot order) but
not to the level a port needs: `AutoObject_ctor` 0x23a9b0, the container's
constructor and `takeDamage` bodies, the exact own-draw order and arithmetic
before the Gold roll (six `fx_spawn` calls from three own draws each, one
`PhProp` per debris mesh), `obj_moveWithCollision`, and the Gold touch dispatch
from the player's move step. `Gold_ctor2`, `Gold_run` and `hud_tickGold` exist
in the tree (combat ctors); the source object does not.

## Still refused

- Death-animation choice (own draw -> `death1`/`death2`): `WarBoar_setState(0x14)` refuses
  the clip; the death state, XP and loot roll still run.
- Hit-reaction state 0x12, boar attack states, `slot 25 force` for WarBoar sight.
- The starting equipment (unarmed stand-in; kill speed is therefore not campaign evidence).
