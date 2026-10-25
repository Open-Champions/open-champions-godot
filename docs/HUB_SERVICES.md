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
- Alternate-hub NPC choice/action effects, action-bearing rings and either
  hub's choice-to-item-removal reachability. The controller refuses alternate-hub
  conversation and trigger entry requests before running an action. The generic
  interpreter is not new evidence for those routes.
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
