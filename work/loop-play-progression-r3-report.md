# Play progression round 3

STATUS: PASS. Hub-offer admission follows the registered mission-menu run task. The PPR2-1 termination refusal is gone. Strict matrix 54/54, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged, journeys 80/81 with pickup 10 to 11.

Code tip: `d5889ae9` on `d366ffb7`. This report is a later commit on the same branch.

## What changed

`menu_open()` stays the client session latch. Dialog admission reads the run-task bit instead.

Observed and bound:

- Open registers the run task and resets state, the transition word, the countdown, and the selection-animation counter. Row, column, pause, pending text, and orientation stay. `research-mission-unlocks@e5c6176` `docs/spec/mission-unlocks.md:205-223`.
- State 4 stores a nonnegative `counter - 16`, including exact 0, and keeps the task. The next negative result clamps the counter to 0, clears pause, and removes the task when the transition word is 0. State stays 4. `docs/spec/mission-unlocks.md:143-170`. `research-mission2-route@6dc8968` `docs/mission2-compose.md:101-109`.
- A constructed nonzero transition word keeps the task and enters state 5. Natural reachability of that branch stays Unknown. `docs/spec/mission-unlocks.md:167-170`.
- The tick that enters state 8 still has the task. The state-8 body publishes, and the task is then gone. `docs/spec/mission-unlocks.md:196-201`.
- At counter 0 the offer script is not entered. After removal, reopen on the granted second offer selects node 8, and choice 1 does not grant again. `docs/spec/mission-unlocks.md:172-180`. `docs/mission2-compose.md:105-112`.
- The campaign image does not carry the run task. Same-process load keeps the live bit. A new host does not gain one. `docs/spec/mission-unlocks.md:245-260`.

The older `menu_tick` field machine is unchanged, so the quests vector fixture is unchanged. The lifetime wrapper is the additive model.

Synthetic coverage: the 256 live and fresh-host admission controls, the constructed counter-16 sequence, same-process and fresh save/load across counter 0 and the removing tick, node 8 choice 1, the 127 countdown through the negative step, state-8 arrival versus the publishing tick, and the constructed nonzero transition word.

## Refusals removed

- `mission menu admission refused: run-task termination is Unknown` is gone. A registered task suppresses the offer. A removed task admits the documented offer. Neither counter 0 nor a retained pending string is treated as termination.

## Refusals kept

- hub2 dialog choices, the loader body, arrival, physical use, and helper `0x18ebb8` stay unimplemented. `docs/mission2-compose.md:69-71,120-136`. The journey still refuses the hub trigger that would open the menu, so the play-through does not reach this cancellation path.
- The original menu-open flag clear on the first closing tick is not wired to the client latch. That flag is not the admission signal (`docs/spec/mission-unlocks.md:159-161`). Clearing the latch there would stop the Godot countdown while the counter is still positive. The display predicate still hides the panel at state 4 with counter <= 0. On the normal countdown from 127, the tick that stores 0 is the negative step that also removes the task. The constructed counter-16 case stores 0 while the task remains; the synthetic tests issue the following tick directly.
- Fade advance in the older menu machine stays `>= 128`. The exact-128 refinement belongs to the additive research model and is not applied to the quests vectors.
- Explicit task removal besides the negative close and the state-8 body, the draw task, and offers outside this hub dialog stay unbound.
- The post-grant node-3 retry is still the close-fade schedule, not menu admission. Headless recorded count 2. Rendered recorded count 1. Both took 3 choices and left the first story available. Count 1 is the same retry with a slower rendered frame advancing the fade before the next confirm. It is not a second grant and not the removed termination refusal. Previously recorded rendered counts of 2, 3, and 4 remain the same path.

## Gate results

| Gate | Result |
|---|---|
| Release `cmake --build build/native -j 4` | PASS. Incremental. |
| Focused `quests` and `dialog` | PASS. 0.28 s and 1.23 s. |
| Full `ctest --test-dir build/native --output-on-failure -j 3` | PASS. 253/253. Real time 791.06 s. No failures. |
| Three development GDExtensions, Release / template_debug, `-j 4` | PASS. Exit 0 for the session extension, the level-texture extension, and the creature-animation extension. |
| Strict Godot matrix with captures | PASS. 54/54, 0 failed, including the six graphical cases. `ui_layout_smoke` passed. |
| FULL_SAVE_LOAD | PASS. 300/300. Both presentation hashes `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`. |
| FULL_TRACE | PASS, unchanged. 36,402 bytes. SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. |
| Journeys | Headless 80 steps, rendered 81 steps. Pickup passed, inventory 10 to 11, 52 walk ticks, 0 retries. Eight refusal kinds on each. Godot exit 1 with 0 log errors, matching the expected death-reload refusal. Read-only input file count 994 before and after. No `run-task termination` refusal. |

Commands used the existing Release build directory, the existing extension build directories, the local Godot binary, and the read-only disc. No baseline, fixture, or harness edit. No push and no pull request.
