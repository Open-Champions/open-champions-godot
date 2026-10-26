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
The original scheduler setup is Observed only; no new original-instruction
execution is claimed for it.
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
The PS2 save importer also refuses populated world/decal restoration and
camera application. Normal death therefore stops once in phase 1, with an
explicit `save_reload refused` diagnostic, the dead body, zero HP and full
fade retained. The special same-level loader is also unbound and refuses
before any HP refill. A missing checkpoint is never replaced by an entry
snapshot, arbitrary spawn point or half-HP recovery. Manual health restoration
also refuses a dead object; revival requires object replacement.

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
