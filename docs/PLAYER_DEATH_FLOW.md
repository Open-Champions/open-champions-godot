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

## Player damage slot, death and knock-back (research-spells-r3)

Evidence: `research-spells-r3` **1e764d4**, `docs/spec/player-spells-numeric-r3.md`
lines 121–200, executed by `tools/difftest_spell_numeric_r3.py:440-631`
(audits `research-spells-r5.md` and `research-spells-r5-fix.md`, both PASS).
The combat World already ran these bodies in the live path. The campaign fight
reaches them through `World::take_damage` (`Fight::diagnostic_lethal_hit`, and
`damage_apply` on a player victim), the player tick's `Player_processDamage`
call-out, and `Creature_applyKnockback` `0x115eb8` in its callback table. The
public `player-damage` test now runs the note's synthetic cases through those
same World entries on both probe seeds, 20973 and 7:

- `Player_takeDamage` `0x1b5330` (`:121-140`). Pending `+0xc8` stores
  `fadd(pending, amount)`, including for amount 0. The flag word `+0x2dc`
  stores the OR. No hit points, state or speed are written. Recall
  `0x41b51d`, menu lockout `0x41b4fc`, exit countdown `0x4f694a`, and a
  nonzero jump-gate query each leave the write log empty.
- `Player_processDamage` `0x1b2cc0` (`:143-163`). Pending is subtracted from
  record `+0xac`. Object `+0x228` is unchanged. A result at or below 0 stores
  0, state 4 and object flags `0x169`, and rebuilds the alive list once with
  no seed draw. A survived hit stores `0x160`, keeps state 2 and draws one
  seed step. Pending 0 leaves a record already at 0 unwritten and unkilled.
- Knock-back (`:166-201`). The player table is 100/150/200/250, chosen by the
  first set bit, also on a killing hit and when nothing is pending. The
  creature hit, no-pending and dead tables are 100/150/225/375,
  50/75/100/156.25 and 100/150/200/312.5. `Creature_applyKnockback` stores 0
  for a player in a net game or in PvP mode B. Otherwise it moves, and a
  planted 30 steps to 28.

The note leaves these **Unknown**, and nothing here claims them from it: the
hit direction `+0x2e0` (`:140`), the stagger threshold (`:178-179`), creature
kill credit (`:194-195`), spatial queries, the holy-shield split and the real
cast body (`:229-231`). The World still writes `+0x2e0`, `player_stagger`'s
threshold and `award_kill`, using the earlier round-2 defence and monster
contracts. They are unchanged and are not asserted by `player-damage`. In the
Skelight creature rows the World runs the Skelight's own slot 27, where the
note used a zero stand-in. Only the words the note measured are compared.

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

- Public `player-damage` tests: the research-spells-r3 player damage slot,
  death, knock-back tables and `Creature_applyKnockback` cases on the live
  World entries, both probe seeds.
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
