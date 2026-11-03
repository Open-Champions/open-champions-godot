# Play progression round 3

STATUS: IN PROGRESS. Native admission is committed. Godot gates are not finished.

Tip before this round: `d366ffb7`. Code tip: `d5889ae9`.

## What changed

Hub-offer admission follows the registered mission-menu run task. `menu_open()` remains the client session latch and is not the admission signal.

Observed and bound:

- Open registers the run task and resets state, the transition word, the countdown, and the selection-animation counter. Row, column, pause, pending text, and orientation stay. `research-mission-unlocks@e5c6176` `docs/spec/mission-unlocks.md:205-223`.
- State 4 stores a nonnegative `counter - 16`, including exact 0, and keeps the task. The next negative result clamps the counter to 0, clears pause, and removes the task when the transition word is 0. State stays 4. `docs/spec/mission-unlocks.md:143-170`. `research-mission2-route@6dc8968` `docs/mission2-compose.md:101-109`.
- A constructed nonzero transition word keeps the task and enters state 5. Natural reachability of that branch stays Unknown. `docs/spec/mission-unlocks.md:167-170`.
- The tick that enters state 8 still has the task. The state-8 body publishes and then the task is gone. `docs/spec/mission-unlocks.md:196-201`.
- Dialog open calls the same check as `task_isRunning` on that run task. At counter 0 the offer script is not entered. After removal, reopen on the granted second offer selects node 8, and choice 1 does not grant again. `docs/spec/mission-unlocks.md:172-180`. `docs/mission2-compose.md:105-112`.
- The campaign image does not carry the run task. Same-process load keeps the live bit. A new host does not gain one. `docs/spec/mission-unlocks.md:245-260`.

The older `menu_tick` field machine is unchanged, so the quests vector fixture is unchanged. `docs/spec/mission-unlocks.md` describes that lifetime as additive.

## Refusals removed

- PPR2-1 `mission menu admission refused: run-task termination is Unknown` is gone. A registered task suppresses the offer with the existing admission suppressor. A removed task admits the documented offer.

## Refusals kept

- hub2 dialog choices, the loader body, arrival, physical use, and helper `0x18ebb8` stay unimplemented. `docs/mission2-compose.md:69-71,120-136`.
- The original menu-open flag clear on the first closing tick is not wired to the client latch. That flag is not the admission signal (`docs/spec/mission-unlocks.md:159-161`). Clearing the latch on the first closing tick would stop the Godot countdown while the counter is still positive. The display predicate still hides the panel at state 4 with counter <= 0. On the normal countdown from 127, the tick that stores 0 is the negative step that also removes the task, so the panel and the task end together. The constructed counter-16 case stores 0 while the task remains; the synthetic tests issue the following tick directly.
- Fade advance in the older menu machine stays `>= 128`. The exact-128 refinement is in the additive research model and is not applied to the quests vectors.
- Explicit `missionMenu_removeTasks`, the draw task, hub2 choices, and the campaign-wide refusal outside this hub offer stay as they were.
- The post-grant node-3 retry can still print 2, 3, or 4. That count is the close-fade schedule, not this admission path.

## Gate results

| Gate | Result |
|---|---|
| Release `cmake --build build/native -j 4` | PASS. Incremental link of the quests, script-command, dialog, and campaign-fight targets. |
| Focused `quests` and `dialog` | PASS. 0.28 s and 1.23 s. |
| Full `ctest --test-dir build/native --output-on-failure -j 3` | PASS. 253/253. Real time 791.06 s. No failures. |
| GDExtensions | PENDING |
| Strict Godot matrix | PENDING |
| FULL_SAVE_LOAD 300/300 | PENDING |
| FULL_TRACE | PENDING |
| Headless and rendered journeys | PENDING |
