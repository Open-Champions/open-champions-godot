# Hub non-vendor services (RTA US v2.00)

Evidence: research `t3/research-hub-npcs`, final report `31db144`,
`docs/spec/hub-npc-services.md` and `tools/ref_hub_npcs.py`. The final
`research-hub-npcs-fix2.md` audit is PASS. The earlier FIX audits withdraw
claims about alternate-hub choice effects and dialog item-removal
reachability; neither is treated as established here.

## Direct savepoint use

`hub_services.hpp` implements only the executed `Savepoint_interact` gates
(0x28dc30): a busy card or held menu mutex refuses; accepted use sets the
activated and pending-effect words to 1 and requests a save. The function
does not receive or modify a player record. No price, heal, XP award,
inventory debit or skill change is assigned to it.

The controller enumerates the existing hub service candidates, walk-in trigger
and action-bearing effect ring without adding actors. `hub_service_use` accepts a Savepoint only with an attached hub,
session Player, available dialog/mutex state and no action property. Its
per-record point flags reset on level attachment. A request opens the existing
native front-end save flow, which writes the existing native continuation
format and Player companion. This is not PS2 memory-card persistence. The
host's active front-end supplies the busy gate; dialog, help and mission
menus conservatively hold the menu gate. Unknown savepoint action scripts
refuse rather than being skipped after a successful save.

The real-data scene adds **Use hub service (direct)** beside its service selector in the host
controls. This is explicitly a diagnostic direct-use adapter, not the
original interaction picker or screen layout. It does not teleport the
Player, forge a proximity result or allocate a second NPC. Existing primary-hub
conversation and level-action controls remain available. No shop implementation
or vendor state is added, keeping this work independent of `play-vendors`.

The ordinary **E** key continues to enter the primary-hub NPC conversation
through the existing native Player-position picker. Choices run the existing
dialog machine and binder against the attached Player and campaign. The focused
journey checks the audited fresh-hub ending: one quest added and one mission
availability bit added, with gold and XP unchanged. It does not establish later
choice effects or an item-removal fee. The service selector names each actual
record by class and index and supplies a diagnostic for each unsupported use.

## Not implemented / no parity claim

- Savepoint prompt rendering and original proximity/use dispatch, autosave,
  saved-effect completion and savepoint-state serialization.
- Co-op death countdown, free-spot placement and automatic resurrection. The
  observed half-maximum-HP respawn rule is not a healer dialog service.
- Integrated mission-map presentation, fade and cancellation completion;
  the existing script/quest module owns availability, quest records and the
  bounded native menu state machine. Its documentation explicitly leaves
  the end of the cancel countdown untraced (`QUESTS.md`, Menu, hub and acts).
- Real-world gate discovery, party-distance checks and gate travel; the
  existing gate fixture is not a real-hub service.
- ~~Alternate-hub NPC choice/action effects and dialog item-removal
  reachability~~: superseded by research-hub-npcs `fbbfe65` (next section).
  Ring rendering stays Inferred and is not drawn.
- Separate healer, trainer, stash or extra quest-giver services. The audited
  census supplies no such actor/service contract, so none is invented.

The service selector returns refusal diagnostics for non-savepoint routes
instead of treating them as successful service calls. Primary-hub generic
dialog/level-action APIs remain independently scoped and are not a claim
that their complete hub routes have been verified by this change.

## Validation

`hub-services` tests the four busy/mutex combinations, busy precedence and
preservation of already-set flags on a refused repeat use. The private
`loop_playthrough.gd --hub-only` path uses the real hub and session Player:
conversation mutex rejection, busy and invalid-index rejection, native
save-screen entry and persistence, and unchanged Player progression,
currency and health/mana records. It reloads the saved continuation and compares
the Player and campaign observations, then checks alternate-hub refusals and
direct savepoint use. The ordinary playthrough also records
this step before proceeding to the existing combat journey. Private paths,
decoded assets and generated output stay under ignored `work/`.

## Researched hub and hub2 NPC services (research-hub-npcs `fbbfe65`)

Evidence: `t3/research-hub-npcs` at `fbbfe65`, `docs/hub-services.md` (HS
below) and `tools/ref_hub_services.py` (RHS), audits
`research-hub-services.md` and `research-hub-services-fix.md` (PASS, HSF-1..3
LOW). Only Observed or Executed rows are ported; the code is
`native/include/champions/hub_dialog.hpp` and its use in
`script_dialog.cpp` / `script_commands.cpp`.

- **Dialog tables.** Both live dialogs bind field by field to the researched
  tables: 31 hub and 26 hub2 nodes, their condition offsets, choice counts,
  choice-condition and action offsets and next links (HS:160-228, RHS:58-123).
  Variable names come from the script's literal `getv` sites, as the research
  difftest resolves them (HS:23-35); no name is stored. Node and choice
  conditions answer from the researched predicates (HS:85-101), the start node
  is node 0 first and then file order (HS:76-83), and hidden choices stay hidden
  (hub2 H2.N01's C(1) / !C(1) pair, HS:201, 298). A dialog that does not match
  falls back to the earlier first/second-offer binding in hub; in hub2 it
  refuses (`DialogLevel::require_hub_services`).
- **Five natives only.** An action runs its original entry only as the current
  visible choice, and may execute only `setMissionAvailable`, `addQuest`,
  `setv`, `changeSwoop` and `activateStore`, each exactly as its researched
  effect (HS:112-125): A(n) with value 1 for that action's mission, one quest,
  swoop 0 and store request 0, the demo/intro variable set to 1. Anything else
  (an item or gold native included) stops the action before it runs and is
  counted in `hub_service_refused` (HS:127-145).
- **Store deferral.** With the dialog open a SHOP action sets the swoop to 0
  and only marks the request (0x4fdd30 = 1, Executed with the real
  `dialog_isOpen`, HS:247-257); the store opens at dialog end (Observed,
  `docs/dialog.md` `79a2e4a` line 273, HS:258-262).
- **Tables of 10 and 13.** `hub_research()` reads the bounds limits of the two
  entries `start` defers: 9 and 12, so 10- and 13-cell tables (Observed,
  HS:334-337); the loops visit 9 and 12 cells (Executed, HS:338-339). The
  checks themselves are the original bytecode.
- **Walk-in.** The rectangle's entry dispatches `trigger(ring record name, 0)`
  once (HS:395-399); see the validation report for how the host admits the
  ring target. The slot-3 rendered effect is Inferred and not drawn.
- **Mission-map warning.** `K.use` with a started incomplete mission shows the
  help box and queues `K.poll` (HS:373). `K.poll` re-queues itself, counts the
  runs on which `gamePaused` is 0, and calls `activateMissionMenu` on the 6th
  without writing the entrance; a new warning restarts the count (HS:374-383).
  Per HSF-2 the count is of Script-task runs (`on_ready_tick` deferred
  dispatch), never of frames. Whether the help box pauses the game is Inferred
  and not modelled.
- **PushTrigger record.** nethostonly=1 and radius 79 (hub) / 78 (hub2) are
  checked against the record (Observed, HS:363-366) and reported by
  `hub_service_targets`. Physical use at that radius stays refused: the
  research asserts the values, not the use query that applies them.
- **Cutscene dialog.** hub's one-node line list (no condition, no choices) is
  classified and never offered as a dialog; public 0x34a4 requests the same
  stem (Observed, HS:64-69). Its playback, its caller and the link between the
  two stay refused (caller Unknown, link Inferred).

Refused Unknowns, reported by `hub_service_research().unknowns`: the use of
L-INN's imported `checkPlayerGold` / `givePlayerGold` / `takePlayerGold`, and
whether L-INN charges for the stay (HS:351-353, HSF-3); how a player reaches
L-INN (HS:354); the cutscene caller; whether the help box pauses; several
queued `K.poll` copies; the stack-read store index; the bag cell
`removePlayerItem` deletes (HS:467-472). The hub-side mana restore charges
nothing (Executed, HS:325); no fee or refund is applied anywhere.
