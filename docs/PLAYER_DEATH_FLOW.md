# Player death in the shared campaign World

RTA US v2.00 only. Evidence: `research-death-flow` **8be79eb**,
`docs/spec/player-death-flow.md`, its `ref_death_flow.py`, and the FIX then
PASS audits `research-death-flow.md` / `research-death-flow-fix.md`. These
are documented original branches and bounded executions with stubbed services;
they do not establish a console or continuous original-game wipe playthrough.

`campaign_fight` keeps the existing Player, record and World. The existing
`Player_takeDamage` queues a hit; `Player_processDamage` clamps lethal HP to
zero, marks the body dead, selects the model/weapon death clip (flags `0x206`,
blend argument `0.5`), enters state 4, records the frame and rebuilds the alive
list. The actual native animation runtime resolves and advances the clip.
Missing clip/model inputs refuse. A one-player Resurrect ward is **not**
composed: that path refuses before the ordinary death hand-off; it must never
be treated as an absent ward or an unsuccessful roll. Its caller was not
executed by the death-flow research.

`player_death_flow::Flow` ports the audited all-dead check and task phases.
The slot and wipe countdowns start at 240. Living teammates prevent a campaign
wipe; a dead campaign slot does not automatically respawn when its timer ends.
The module preserves pause, cutscene, loaded-game, network peer and PvP-B
gates and signed 16-bit timer behavior. Those gates have synthetic coverage;
the combat World still refuses network, cutscene and PvP execution.

The campaign binding explicitly refuses nonzero offline controller-out
(0x4f6708) or warm-up (0x506048) state before writing the wipe timer or menu
lockout or scheduling. It does not substitute the pause word for the complete
offline freeze service: task-name queries and their early player-selection
exception are not bound. This restricted campaign domain does not claim that
paused and frozen are equivalent offline. The slot countdown retains its
separate pause gate; an unsupported freeze refusal stops the all-dead check
before that gate. Evidence: research-death-flow `8be79eb`,
`docs/spec/player-death-flow.md:69`, `docs/issues/draw-sim-leaks.md:188-189`
and `docs/mainloop.md:517`. Campaign-binding synthetic tests cover pause-clear
controller-out and warm-up at both the initial timer and the zero crossing.

The campaign callback runs the death task after `runObjects` (priority 10),
at its documented priority 20, before the frame increment and end-of-tick
alive rebuild/check. Scheduling at that final check clears the fade and
starts phase 0; the task first runs on the next tick. Phase 0 steps the
existing native `hud_stepFade(4)` until 128. Alternate death waits in phase
10 for an external revival. Phase 1 looks up the level in the executable's
act lists, then asks the required checkpoint or same-level loader service.
Phase 2 waits for the pending request to clear and qualified names to match.
The later `research-death-reload` **0e3afdb8** executes the original countdown
and scheduler wrapper with recording freeze/fade/registration services
(`docs/death-reload.md:12-31`). Real scheduler timing remains Unknown.
These hooks cover the combat World's tasks. The controller's separate
script/dialog pumping is an authored composition; complete original task
interleaving and script-driven revival/loading are not established here.

The Godot binding exposes the task, countdowns, native animation state,
menu-close request serial, fade and refusal. Local menus close on death and
at the wipe zero crossing. The overlay paints the documented black fade
rectangle; its time comes only from native ready ticks. The player body
renderer still draws a **rest pose**, explicitly labelled on death. Loaded
native clip advancement is not evidence of a rendered death pose or pixel
parity. There is no invented game-over/continue menu: the audited campaign
branches contain none.

## Possessions and the remaining reload gate

Death and fading do not debit XP, Gold, mana, bags or equipment. A valid normal
checkpoint reload must instead restore the **entire saved player record**,
saved position/facing and saved level, along with the other save state. All
post-checkpoint record changes roll back; no separate fixed death tax was
established. The off-act-table branch retains current records and restores
max HP while requesting the same level. Its world-object/pickup reset is
Unknown in the death research.

This fight has **no valid in-memory retail checkpoint**. The existing manual
native save and `.player` / `.fight` companions are not `g_saveBuffer`.
The strict PS2 runtime now applies the existing save loader when a native
owner supplies a validated original-format buffer through
`Fight::bind_death_checkpoint`. It requires an explicit camera service at that
call; the importer diagnostic path does not silently stand in for it. No
campaign producer currently supplies such a buffer. Normal death therefore
stops once in phase 1, with an
explicit `save_reload refused` diagnostic, the dead body, zero HP and full
fade retained. The special same-level loader is also unbound and refuses
before any HP refill. A missing checkpoint is never replaced by an entry
snapshot, arbitrary spawn point or half-HP recovery. Manual health restoration
also refuses a dead object; revival requires object replacement.

The new bounded save/class work is pinned to `research-death-reload`
**0e3afdb8ce3cdf79746f535d8a2654db37c9e97b**:

- `docs/death-reload.md:23-42` and
  `tools/difftest_death_reload.py:104-126`: automatic buffer load restores the
  complete saved record, pose, player count and loaded flag, then requests the
  saved qualified destination with marker 253. The separate literal lookup
  `-1` branch requests the current level and changes only HP to max HP.
- `docs/death-reload.md:46-62`: pending request plus qualified-name match gate
  task completion. Reload retains the checkpoint; it never stores the dying
  world first. The campaign stops before another old-world tick after a bound
  restore request, since its replacement world is not established.
- `docs/death-reload.md:75-89`: `ps2_save::restore_class_fields` operates only
  on already constructed objects. It restores Trigger fields, bounded Creature
  fields/callback arguments, Chest/AutoChest state and clip calls, world-backed
  door collision/pose/height, JumpGate bytes/callbacks, and identity-pose
  Prop/ItemProp state. Existing `level_flow::Pieces`, core math and save
  memory are reused. The chest test runs the real native animation services.
- `docs/death-reload.md:76,82-83,91-97`: missing Creature stop/motion/animation/
  registry, JumpGate light/assets/updates, other portal classes, arbitrary
  quaternion/facing, whole-world construction and continuous death-to-playable
  behavior refuse. A class byte count is not proof of world membership. A
  supported prefix may be applied before a service refuses; callers must not
  publish partially reconstructed objects. Truncated/unsupported encodings are
  rejected before field or service mutation.

Individual savepoint/teammate revival remains unsupported: the free-spot
search, complete Player constructor, spawn-protection/light and local-camera
services are not bound to the campaign World. The research records half-max
HP (full in PvP-A), object replacement and protection, but leaves parts of
construction/protection and allocation failure unexecuted or undetermined.
Normal campaign wipe does not call this individual revival routine.

## Validation routes

- Public `player-death-flow` tests: timer/service gates, phases, restart
  retention, no guessed checkpoint heal, signed boundaries, all 24 death-clip
  selector cases on the real Player damage body, and actual World task order.
- Public `ps2-save` tests: the real death Flow calls the real strict loader;
  simple/qualified rollback, the separate restart branch, full-record retention,
  phase-2 waiting, missing/invalid checkpoint and missing camera refusals.
- Public `ps2-class-restore` tests: researched class fields, native door world
  setters and chest animation runtime, callback ordering and refusal prefixes.
- Public `godot/tests/player_death_smoke.gd`: fade states, render-only frames
  preserving native time, rendered half/full-alpha pixels and absent-fight
  diagnostic refusal. No retail input.
- Private `champions-campaign-fight-probe --death-check --attack-every 0` and
  `tools/run_loop_playthrough.py`: a labelled **diagnostic** lethal hit from a
  live level enemy is queued through `Player_takeDamage`; no HP, action-state
  or animation word is forced. The actual Player then dies and fades in the
  existing World. The probe compares XP, mana, Gold, all bag bytes and equipment
  without printing them. Boar attack AI is unported, so this does not establish
  an organic enemy attack. The playthrough reports `blocked_death_reload` and
  `respawn_completed: false`; successful bounded refusal is not respawn success.

Still required: a valid checkpoint producer, complete loader integration with
saved player records and level state, rendered player pose, and comparison of
a continuous original death/reload against the same saved state. Original
invalid-buffer behavior, alternate script revival, other profiles and physical
platform prompts remain Unknown.

## Death reload, client round 2 (2026-11-09)

Research: `t3/research-death-reload` at `98ea7a6`,
`docs/death-reload-client-path.md` (audit r8-fix PASS). It builds on
`t3/research-save-world` `31a0ca6` (`docs/save-world.md`) and
`t3/research-exits-r3` `350fc8b` (`docs/exits-r4-travel.md`). Below,
`client-path` is the first file, `save-world` the second and `exits-r4` the
third; `exits-r5` is `docs/exits-r5-travel.md` at the same pin. Only Observed
or Executed contracts are ported. Each line cites its
evidence.

A death reload is a card load fed from the live save buffer, not from a copy
frozen at the checkpoint (client-path:55-63, 115-129, 137-143; the live-buffer
reading is Inferred there). So the checkpoint header (players, quests, poses,
gate list, level, act, difficulty) comes back together with each level's
*latest* stored body.

**Native model, `level_flow`.**

- `save_game` (save-world:241-275, Executed). Nothing happens without a
  counted living player: a slot below `g_numPlayers` whose object pointer
  (+0x43d8) is set, whose object's flag 8 is clear, and whose HP is above 0
  (save-world:247-252). These are `PlayerRecord::object_built` and
  `object_flag8`, separate from the placement peer bit `active`
  (exits-r4:107-117), which is a different original test (round 3).
  - Act −1 or the `missionmenurun` task goes to `save_build` directly
    (save-world:253-254).
  - Otherwise, with no act row as a prefix of the qualified name, no checkpoint
    is built and nothing is stored (save-world:255-260). This test does not
    read the completed-game flag (round 3).
  - `save_build`: when the game is not complete and its own `save_findAct`
    misses, nothing is written (`t3/research-saves` `8d43286`
    `docs/save-format.md:55-58`). Otherwise the header goes into
    `State::saved` and the current level is stored into the live directory
    `State::states`. A completed game skips `save_build`'s lookup and the
    store, so that level keeps the body the directory already held
    (save-world:269-275; save-format.md:58-60; client-path:69-71).
  - At act −1 a non-hub name has no current-act row and is not stored
    (save-format.md:236-242). A `hub*` name there would read row 0 of no act,
    which was not executed, so it refuses.

  `checkpoint` is the earlier entry point. It now returns the same header
  without a copy of the directory.
- `card_load` replaces the header and the directory together (save-screen
  `c8895e2` `docs/save-screen.md:258-279`; client-path:103-108).
- `party_wipe(State&)` (client-path:88-91) runs the act lookup on the current
  level.
  - Found: the header from `State::saved`, the live directory left as it is,
    the loaded flag, and the saved level requested with `0xfd`.
  - Not found: the current level requested with `0xfd`, HP refilled for slots
    below `g_numPlayers` only (`deathTask` 0x1e76dc-0x1e7710; round 3), nothing
    else changed (step 5b).
  - Found but no header ever built or card-loaded: refuses, because the
    uninitialised-buffer behaviour is Unknown (client-path:183).

  The research note says step 5 looks up "the checkpoint's level"
  (client-path:89). The disassembly contradicts that wording: `deathTask` phase
  1 passes 0x44fe18, the current level base name, to `save_findAct`
  (0x1e76a8-0x1e76b4), and phase 5b copies the same register into the pending
  name (0x1e76c4-0x1e76d0); 0x44fe18 is the base name per `monsters.md:244`
  (`t3/research-death-reload` `98ea7a6`) and exits-r4:87. So this client keeps
  the current-level lookup. The wording is noted for the research owners.
- A death load through `SessionAdapter::change` with `death_task` set:
  - It does not store the dying level (step 6). It skips the act-change clear
    but still updates the act index (step 6a, exits-r4:67-80). Both happen
    before the split (step 7).
  - Rank is 3 from the loaded flag. At rank 3 the `Player1` markers and the
    free-spot search are skipped, and z comes from terrain (step 10,
    exits-r4:100-104; rank: Observed exits-r5:112-117, Executed
    save-world:94-96).
  - Only slots that pass the HP, peer (`active`) and medal-slot gates are
    built (exits-r4:107-117). Record +0x24 (recall destination) is not
    rewritten on an act change (exits-r4:118-121).
  - `death_task_done` is phase 2 (client-path:99).
- Step 9 stopped-slot rule (save-world:44-52, Executed).
  `LevelStates::restore_decision` compares slots 0..9. It stops at the first
  empty name, which loads fresh. After ten misses it reaches gap slot 10:
  empty, the level loads fresh; written, the original restores the gap body
  for every missing name.
- Gap slot. The port must choose a policy (save-world:314-316); the chosen
  gap-slot policy is to refuse. `require_restorable` names the key and the gap
  name. This refuses exactly the misses that `stored()` already refused, now
  with the precise reason. The eleventh key is written to the gap. Since round
  3 a twelfth key (or any later miss) rewrites the gap body at its offset with
  its kept size and keeps the ghost name, as save-world:35-39 and 196 execute;
  the client no longer refuses that store.
- An act change after the checkpoint (save-world:194-195, 293-296).
  `save_clearBuffer(0)` keeps directory slot 0's name and first act-0 row-0
  bytes (15000 in the retail table) and sets its size to that row's. Since
  round 3 the client keeps slot 0 only in the case whose restore was executed:
  the exact act-0 row-0 name (`hub`), already at that size, with a body that
  fits; that slot restores after the clear. In every other case (a larger
  body; a name such as `blood1` whose size the clear cuts; a hub sub-area such
  as `hub_west`, for which only the lookup was checked) it drops the whole
  directory and records the slot-0 key (`LevelStates::cleared_slot0`).
  - A death reload into that recorded key refuses.
  - So does any death reload once the dropped slot would push the client's
    slot 9 onto the original's gap.
  - Other levels of the old act come back fresh, as in the original.
  - With block byte 0x4f8f2a set (`ChangeOptions::clear_block`), the buffer is
    left untouched (save-format.md:296-297). Nothing in this client sets it.

**Client, campaign `Fight` and Godot scene.**

- `Fight::bind_death_reload` hands phases 1 and 2 of the death task to the
  owner. The act lookup now matches rows by prefix, as `save_findAct` does
  (exits-r4:12-17).
- When the death task asks for the reload, the controller refuses with a
  precise reason in each of these cases:
  - No card load or card save in this session has marked the live buffer
    (client-path:183).
  - The last card save was refused. The original's offline card save rebuilds
    the buffer with `save_build` before writing (save-screen:229), so the live
    header is unknown to this client (round 3).
  - The campaign act changed, or an act reset ran, since the card load or save
    (save-world:194). The client's object directory is not cleared on an act
    change.
  - The checkpoint level meets a written gap slot (save-world:44-52, 314-316).
- The same gap rule now applies to every scripted level load, instead of
  loading fresh.
- Step 5b (no act row) refuses in the scene. The scene has no current-level
  reload that keeps the dying record with only HP refilled (client-path:91).
- `real_data_play.gd` marks the live buffer after each successful card load
  and, since round 3, after each successful card save: an offline card save
  calls `save_build(slot byte, 0)`, after which the image at 0x895580 starts
  with the live level name (save-screen:229, Executed). So "load A, save to B,
  die" reloads B. A refused card save clears the mark with its reason.
  `run_death_reload` then reloads that image with `restoring_real_level`, so
  the dying level is not stored (client-path:92). It keeps the live object
  directory rather than the card's copy (client-path:120-124). It restores the
  header's player, quests, pose and HP, and reports the loaded key for phase 2.

**Still Unknown, so refused or not attempted:**

- A death fee (client-path:76-78). The real retail GetReg values.
- Item fixup with populated bags. Per-class load effects. Camera presentation.
- The alternate-death script (phase 10).
- The autosave daemon is not ported. `save_daemonRun` calls `save_game(0, 1)`
  only when the card state is idle, outside the front end, not PvP, with no
  full-screen menu, camera mode 1 and the game not paused, after
  `save_armAutosave` arms the countdown (`t3/research-death-reload`
  `98ea7a6` `docs/audio-save.md:156-162, 341-343`). `level_flow::save_game`
  has no production caller either: it is wired neither to the controller nor
  to script `saveCheckPoint` (still `Effect::Partial`). So the controller's
  live header comes only from a card load or a card save.
- A continuous original death-to-playable run to compare against
  (client-path:170-185).

## Death reload, client round 3 (2026-11-09)

Audit `play-death-reload-r2` (FIX). The round 2 text above is corrected in
place; this section lists what changed.

- **save_game act test (PDR2-1).** With no act row as a prefix, `save_game`
  builds nothing, even for a completed game. Only act −1 and the
  `missionmenurun` task go straight to `save_build` (save-world:253-260; the
  `save_game` body reads no completed-game flag on that path). The
  completed-game skip is `save_build`'s, and covers its own `save_findAct`
  and the level store (save-format.md:55-60).
- **Live header after a card save (PDR2-2).** A successful card save marks
  the live header with the saved file (save-screen:229). A refused card save
  clears the mark, and a later death reload refuses with that reason.
- **Two object tests (PDR2-3).** `save_game` uses `object_built` and
  `object_flag8`; placement keeps the peer bit `active`.
- **Step 5b (PDR2-4).** The HP refill covers slots below `g_numPlayers` only.
- **Wording (PDR2-5, PDR2-7).** The autosave daemon and the in-session
  `save_game` are not ported (see "Still Unknown" above). The chosen gap-slot
  policy is to refuse.
- **Research-backed pins.** An act change keeps the executed `hub` slot 0
  (save-world:194-195); a twelfth key rewrites the gap body (save-world:35-39,
  196). Both are described in the round 2 bullets above.
- **The card-save refusal on a fresh level (PDR2-6).** The scene now says "a
  built object's Creature HP word (+0x228) is not above 0", and
  `built_creature_hp_census()` lists the refusing records (ordinal, class,
  raw +0x228/+0x22c words). On the journey's freshly loaded checkpoint level
  the census finds no dead Creature: the refusing records are 16
  `ScriptGenerator` objects. `build_generator` reports them as built, and
  `save_built_object_fields` reads every built record with the Creature layout,
  so their never-written +0x228 word (0) fails the HP test. (The journey's own
  earlier save also meets its killed enemy, a `WarBoarSmall` with a negative HP
  word.)
- **Not unblocked (Part 3).** What the original saves for a `ScriptGenerator`
  is not established: whether `world_saveFilter` 0x15d9c8 rebuilds it from
  `objects.ob` or a save routine writes it is open (`t3/research-save-world`
  `31a0ca6` `docs/spec/object-system.md:805-806`; save-world:101-105 names
  only lights, fire, `Line`, `Blocker`, `customVars` and route points). The
  dead-Creature record is established (`Creature_saveState`, save-world:116),
  but porting it would not unblock this level. So the refusal stays, and a
  journey checkpoint, and with it a successful real-data death reload, remains
  blocked.
