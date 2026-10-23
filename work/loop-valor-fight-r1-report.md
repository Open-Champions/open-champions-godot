STATUS: PARTIAL — the ordinary `valor` session now builds 78 WarBoar-family actors through the ported classes into one shared World with the session's Player, J reaches `Player_meleeHit → damage_apply → Creature_takeDamage` and lowers boar hit points; no kill, loot object or Gold is reachable in the scripted fight.

# Valor fight round 1 — 2026-10-23

Branch `codex/valor-fight`, from `codex/world-authority` 3ff0980 with `codex/player-services` 9069805 and `codex/port-warboar` 711eae1 merged (both merges were conflict-free). Research pin `t3/research-campaign-combat` 328b072 plus the player-model/player-animation pins the player-services branch already cites (`7f13899`, `cda95dc`). The owner's RTA US v2.00 disc was read-only (994 files before and after every run). No game bytes, names or decoded content are in the tree. No push.

## What now works in ordinary play

1. **The session's player is the ported Player in the shared World** (`campaign_fight`, `native/src/campaign_fight.cpp`). A real-level session with the executable, the level's WORLD and the audited player bundle builds a `Fight`: the combat World (Full composition) over the one `WorldAuthority`, the audited real-level services (terrain, water, camera, animation start/advance/root, weapon class, action state) and the session character record (class 6 / model 11 from character creation, real max HP 32.8). WASD is the stick, J is the attack pad bit of the executed `Player_run`. The dry scene is kept only as the ready-tick clock and save spine (it gets no movement and no damage bit); the presented position, facing and HP are the Player's. This is no longer a DryCreature stand-in for these sessions; ready-tick pacing is the only thing it still supplies.
   - Player animation: the model's 111 slot names and animation LMP come from the executable's definition table (`7f13899`), clips are decoded by the audited animation runtime, and `player_resetIdleAnim` is implemented as documented in `cda95dc` (idle, walk, run 1, run 2 of the weapon-class row into cells 0–3, flags 0x1205, count 4). The native animation state is mirrored into the memory cells in both directions (`animation.md` 2.1 layout) so the player tick sees what it reads.
   - Documented unarmed class 5 (`docs/player.md`). **No equipment is invented**: the assisted capture's club (38 / 19–24) belongs to that emulator session, not to this client's route, and is not used. The session player therefore fights unarmed.
   - Level load globals: kill plane `g_killPlaneZ` 0x4f7188 and level flag word 0x44fe08 come from the executable's `g_levelTable` row (`docs/data-tables.md` 7.1). Without the kill plane the first tick killed the player.
   - `net_getCamFlag` is "camera not in follow mode" (`movement-camera.md` 7.2): 0 normally, 1 while a script dialog is open.
2. **The level factory builds the boar family into the shared World.** The 78 WarBoar-family records on `valor` (research: 34 `WarBoar` + 44 `WarBoarSmall`) are built in file order: `GameObject_ctor` (one shared draw), the executed arena `Creature_ctor` (max-HP argument 10.0, definition 0x502f80, real `monsters.bal` row and level monster level, one own HP draw), then the `WarBoar_ctor` class part (`campaign_actors::Runtime::adopt`): class fields, hide mask from the real model (`model_buildHideMask`, `docs/formats/model-texture.md`), idle slot 16 pushed on the animation state, the 60-frame run list, the seven `creature_setDefEntryParams` writes on the real clips' event slots, `Creature_init`. State-0 entry age is the frame of the 60-frame list registration (0), as asked. All fields live in the shared authority memory; each object is also in the grid. 94 records are built on `valor` (was 16); the other 97 are 6 Minotaurs and 91 stubs of unported classes.
3. **J → documented melee path.** The ticks show 120 J edges → 120 `damage_apply` calls with the player as attacker (handle linked through the combat World's handle service) → boar `Creature_takeDamage` (slot 2, inherited 0x116638 through the executed arena base body). The standalone `campaign_actors::Runtime::take_damage` still refuses a nonzero attacker (PWB3); the shared path does not use it. The boar vtable image is the executed Creature base table with the documented slot 4 (0x2ba3a0) and slot 27 (0x2bac60) overrides.
4. **Death / loot.** The boar death chain runs in the native probe (see below) but is not reached in the scripted playthrough.

## Playthrough step 4 (valor, level index 99), final source

`python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/loop-valor-fight-r1/final`. Headless and rendered gave identical numbers (exit 1 only because the journey is still incomplete: no kill, no Gold). 24 steps in each, 13 screenshots rendered; every other step passes, including pause/save/load/cold load/level change.

- built boars: **78** (`warboar_unbuilt` 0); live enemies **78**; total enemy HP 31,800.89 before.
- J handled: **yes** (key path returned handled; 120 attack edges reached the player's pad).
- **boar HP decrease: 31,800.89 → 31,733.85 (−67.04)** from 120 `damage_apply` calls by the player, 32 of which dealt damage (~2.1 each); the rest were zero-damage results of the executed hit rolls. No boar died (average boar HP ≈ 408 against ~2.1 per hit).
- walking: the blind WASD walk (camera-calibrated, 392 ticks) closed the nearest boar from 1,203 to 926 units and stopped short (the walk is blind to walls; the cause was not diagnosed). **The swing then started from a DIAGNOSTIC placement** 40 units from the nearest boar (`fight_place_player`, not an original routine) — the organic distance is reported above, not hidden.
- player HP stays 32.8: the boars' AI is frozen/idle, so nothing attacks the player.
- death: no. drops: no (`loot_calls` 0). **Gold 500 → 500** (see Gold below).
- Native save carries the player's position/facing/HP in a `.fight` companion; load of the same level keeps the fight and restores them (enemies and other active objects are not saved: "World objects restart" was never true for these).

Rendered inspection: `work/loop-valor-fight-r1/final/rendered/05-fight-attempt.png` and `04b-fight-approach.png` show the class-6 player model with **textured WarBoar models** (the large boar beside the player, a second boar to the right, one at the top edge). Boars are drawn in the class model's rest pose at their native positions and yaw, scaled 0.75 / 0.6 (constructor +0xdc). Boar clips are not wired to the draw.

## Native probe (counts only)

`champions-campaign-fight-probe --game "$RTA_DISC" --level valor --ticks 600 --near --weak` (probe-only: the player starts 40 units from a boar; boars within 400 units have their HP set to 1.0 to exercise the death path): `deaths 1, loot_calls 1, dead 1`. The chain that ran: `Creature_processDamage` → state 0x14 through slot 27 → `Creature_dropLoot` (rank 0, zero gold chance, `loot_generateDrops` called once) → `Creature_onDeath`. It stops at three labelled refusals (below). This is **not** campaign evidence; it only shows the integrated death/loot services do not crash and where they stop.

## Still refused (counted, never replaced by an invented value)

- **Minotaurs (6):** base `Creature_ctor` cflags, hide variant draw and `WeaponShield` hand writes are Unknown, and `Minotaur_run` is not in the research. Not built, labelled in `build_refusals`.
- **Boar AI beyond idle:** for the 5 active boars `WarBoar_run` state 0 → state 1 effects after 180 frames are Unknown; the boar then stays idle (its `processDamage`/death check keep running). The research gives `+0xf4 = entry + 180` but nothing sets it in the constructor, so the entry age is the 60-frame registration frame. Hit-reaction state 0x12 (`WarBoar_setState(18)`, 12 refusals): clip choice by hit angle is not documented, so boars do not flinch. Attack states 6–10 are therefore never entered in play.
- **Death details:** the death-animation clip (own draw → `death1`/`death2`) mapping is undocumented; experience award needs the skill statistic tables (`SkillLifeTap_getStat0Fraction`) which are not bound; `loot_generateDrops` has no item tables, ground-object spawn or shared-seed pairing here, so a drop is a counted call, not an item object.
- **Gold:** AutoContainer records are unconstructed stubs (class not ported), so Gold cannot be produced or picked up. Round 8 shows Gold came from `AutoContainer_run`, not the boar death. Not reachable.
- **Creature_init step 2** (`model_getBounds` height/draw byte): not ported (78 labelled; base constructor defaults stay).
- **Presentation-only services:** `model_getAttachPoint` (spark origin is the actor position; ~8,000 labelled calls in the scripted fight), sample points reported as zero. They feed no HP or seed value.
- **Collision and targeting:** unconstructed stubs (triggers, containers and other unported classes) were left out of collision/target box queries 2,044 times in the scripted fight. `terrain_raycast` is ported (below); `world_raycast`'s object circles are not, so objects do not block rays.
- **Player:** `spawn_player` is the combat runtime's authored object setup (not `Player_ctor`); `Player_ctor`'s walk/run event slots (footstep sounds) are not applied; no equipment (above). If the host's binder records no current HP the scene's HP stands in (counted once).
- **Bounds:** the combat World retains iterators for its lifetime (65,536 budget). A development run of 900 swing cycles (5.8k ticks) hit "combat iterator retention budget exceeded"; the fight then stops cleanly (labelled) with the player frozen. The shipped playthrough uses 120 cycles (4,015 ticks). A kill is out of reach inside that bound for an unarmed player.

## Other changes

- `terrain_raycast` 0x18a5c0 ported by a sub-agent from `docs/spec/world-queries.md` 9.1 (`native/src/world_raycast.cpp`, 71-check test, 41 table cases whose expected values come from `ref_world.terrain_raycast` at 328b072 on synthetic levels). It replaces the player bundle's raycast refusal; the audited `player_services` test now pins the new behavior.
- **WA-F4 (world-authority audit):** script-store bytes no longer count against the combat sparse-memory budget (`host_extra_`), so an overwrite or a combat addition cannot throw because of a large script store. The script view maps any `std::runtime_error` from the combat overlay to a HostFault. **Measured footprint:** authority memory is 22,646 bytes after `valor` loads and a maximum of 25,828 bytes over the 91 levels the probe could load — far below 1 MiB, so the case was not hit. Synthetic test `campaign-fight-adopt` covers 1.5 MiB of script bytes plus a combat addition. WA-F1 is documented in the `write_host_bytes` comment; WA-F2's test now tears the host down and asserts the retained handle dies; WA-F3's collision check now precedes writing any object byte.
- Handle links to factory creatures accept the three back-list heads (+0, +4, +8), not just +0; the arena base `Creature_takeDamage`/`pickTarget` need the +4 head. The combat World unlinks its own list heads on teardown, which fixed a use-after-free found when a fight died before its host.
- New CTests: `world-raycast-host`, `campaign-fight-adopt`.

## Verification (this tree, arm64, Release)

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`: passed.
- Full `ctest --test-dir build/native -j 3 --output-on-failure` on the final source: **240/240 passed** in 653.6 s, no timeouts, no reruns needed. (An earlier full pass of 240/240 ran before the last two commits; the one regression it later exposed — a throwing teardown in my World destructor, failing `playable-combat-fx` and `combat-runtime-3-host` — was fixed in `de7dac4` and those two pass; the rerun above is on the final source.)
- Godot 4.5.2 development matrix, `python3 -u .../playable-wave26/run-godot.py`: **54/54** exit 0. `FULL_TRACE`: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, **unchanged**. `FULL_SAVE_LOAD` 300/300 presentations. The Full fixture uses `spawn_player` without `session_record`, no adopted creatures and no stubs, so none of the new paths run in it except the World's teardown unlink.
- `bash .../play-enemies/work/play-enemies/run-sweep.sh`: four slices exit 0; **111 tried / 111 loaded / 111 played, 230 checks passed, 0 failures, no refusal kinds**; 111 scripts attached, 76 started.
- Scripted playthrough headless and rendered: above.
- Commands for the probe and the playthrough use `$RTA_DISC` / `$GODOT_BIN`; no private path is committed.

## Unverified

- A kill, a loot object, Gold pickup/credit and AutoContainer on the route.
- Whether the starting player on the real campaign route is unarmed (the research does not say), and the player's real animation timing against hardware.
- Boar animation playback in the draw; hit sparks' pose origin.
- x64 traces were not compared.
