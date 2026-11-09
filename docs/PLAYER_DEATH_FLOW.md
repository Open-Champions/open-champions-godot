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
  counted living player: a slot below `g_numPlayers`, built, with HP above 0
  (save-world:247-252). With no act row as a prefix of the qualified name, no
  checkpoint is built and nothing is stored (save-world:255-260). Otherwise the
  header goes into `State::saved` and the current level is stored into the live
  directory `State::states`. A completed game skips the lookup and the store,
  so that level keeps the body the directory already held (save-world:269-275;
  client-path:69-71). `checkpoint` is the earlier entry point. It now returns
  the same header without a copy of the directory.
- `card_load` replaces the header and the directory together (save-screen
  `c8895e2` `docs/save-screen.md:258-279`; client-path:103-108).
- `party_wipe(State&)` (client-path:88-91) runs the act lookup on the current
  level.
  - Found: the header from `State::saved`, the live directory left as it is,
    the loaded flag, and the saved level requested with `0xfd`.
  - Not found: the current level requested with `0xfd`, HP refilled, nothing
    else changed (step 5b).
  - Found but no header ever built or card-loaded: refuses, because the
    uninitialised-buffer behaviour is Unknown (client-path:183).

  The research note says step 5 looks up "the checkpoint's level". This client
  keeps the round-1 current-level lookup, which matches its 5b wording ("request
  the current level") and the existing tests. That wording is noted for the
  research owners.
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
- Gap-slot policy: **refuse**. The port must choose a policy (save-world
  :314-316) and none is chosen. `require_restorable` names the key and the gap
  name. This refuses exactly the misses that `stored()` already refused, now
  with the precise reason. The eleventh key is still written to the gap. A
  twelfth key stays refused (an existing test pins that), although save-world
  :35-39 executes it as a rewrite of the gap body.
- An act change after the checkpoint (save-world:194, 293-296).
  `save_clearBuffer(0)` keeps directory slot 0, with its first 15000 bytes and
  the act-0 row-0 size. This client clears the whole directory on an act change
  (an existing test pins that) and records the slot-0 key it dropped
  (`LevelStates::cleared_slot0`).
  - A death reload into that key refuses.
  - So does any death reload once the dropped slot would push the client's
    slot 9 onto the original's gap.
  - Other levels of the old act come back fresh, as in the original.

**Client, campaign `Fight` and Godot scene.**

- `Fight::bind_death_reload` hands phases 1 and 2 of the death task to the
  owner. The act lookup now matches rows by prefix, as `save_findAct` does
  (exits-r4:12-17).
- When the death task asks for the reload, the controller refuses with a
  precise reason in each of these cases:
  - No card load in this session has marked the live buffer (client-path:183).
  - The campaign act changed, or an act reset ran, since the card load
    (save-world:194). The client's object directory is not cleared on an act
    change.
  - The checkpoint level meets a written gap slot (save-world:44-52, 314-316).
- The same gap rule now applies to every scripted level load, instead of
  loading fresh.
- Step 5b (no act row) refuses in the scene. The scene has no current-level
  reload that keeps the dying record with only HP refilled (client-path:91).
- `real_data_play.gd` marks the live buffer after each successful card load.
  `run_death_reload` then reloads that image with `restoring_real_level`, so
  the dying level is not stored (client-path:92). It keeps the live object
  directory rather than the card's copy (client-path:120-124). It restores the
  header's player, quests, pose and HP, and reports the loaded key for phase 2.

**Still Unknown, so refused or not attempted:**

- A death fee (client-path:76-78). The real retail GetReg values.
- Item fixup with populated bags. Per-class load effects. Camera presentation.
- The alternate-death script (phase 10).
- The entry autosave's camera-mode gate. Without it, a checkpoint comes only
  from a card load.
- A continuous original death-to-playable run to compare against
  (client-path:170-185).
