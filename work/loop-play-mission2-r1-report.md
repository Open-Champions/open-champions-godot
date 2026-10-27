STATUS: MORE

Controlled hub-menu selection loads mission 2's entry; the ordered playable
mission remains refused at documented evidence and implementation gaps.

Local commits: `eb7fd8b` (complete-command stop policy and synthetic tests),
`9d298fa` (mission entry bridge, refusal UI, private acceptance and documentation),
`baafab7` (locked menu state acceptance),
`33e7462` (portable table reader and unknown-input refusal),
`b26e39e` (actual World tick and runtime-health acceptance),
`8740300` (native act reset and unsupported-inventory refusal).
Branch `codex/play-mission2`, base `252f34a`. Only this worktree was edited;
no push, PR, issue or research-repository write.

Gate figures, macOS arm64:

- Native Release configure/build: **passed**, including the final level-flow
  test executable. The unchanged large test file dominated compile time.
- Full CTest `-j 3`: **239/241 passed**, 3,509.41 s, exit 8.
  `player-exact` timed out at 240 s; `session-facing-exact` hit its inner
  600 s subprocess timeout. Isolated `player-exact` again timed out at
  240.07 s (exit 8); isolated `session-facing-exact` **passed**, 349.77 s.
  The direct pinned player-vector command **passed all 274,304 exact cases
  and transport refusals**, 266.29 s, retaining its internal limits. The
  raw full CTest gate remains failed. Focused
  `script-commands|quests|script-builtins-api|script-effects`: **4/4 passed**;
  latest act-entry assertions: **script-commands 1/1 passed**, 1.24 s. These
  assertions were added after that target's full-run slot; its latest
  rebuilt executable was checked separately.
- Three development GDExtensions (session, level texture, creature animation),
  Release optimization / template_debug ABI: built. Godot 4.5.2 matrix:
  **54/54 commands exited 0**, final matrix 401.75 s. The existing
  scene-play-flow fixture `loaded` access and UI-layout glyph-arity
  diagnostics remain in two logs.
- `FULL_TRACE`: **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged.
  Full combat **2,620 checks / 0 failures**; save/load presentation **300/300**.
- Scripts-on sweep: **111 tried / 111 loaded / 111 played**, **230 checks /
  0 failures**; 111 scripts attached, 76 started. Two script VM faults and
  148 stub refusals match the base branch's existing four-slice summaries.
  Refusal stops increase **7 → 8**, the intended mission-2 cutscene stop.
  All four final slices exited 0, 219.32 s total.
- Mission-2 acceptance: headless **89 checks**, rendered **97 checks**,
  **0 failures**; eight rendered captures. Inspected the locked and available
  menu, inventory refusal, entry refusal and four level views. The levels account
  for **699 records / 660 unbuilt objects / 39 non-object records / 0 built
  placement objects**. Each binds the real Player to its World and runs
  **7 ready ticks and 7 World ticks without a runtime failure**.
  All unproved exits refuse.
  Native-only C++17 syntax checks verify that the table reader and campaign
  entry helper have no Godot dependency. An unresearched button chord and
  unsupported inventory refuse without changing menu state. Cross-act
  entry clears the authored old-act quest while retaining player identity,
  experience, gold, mission completion and mission-2 availability.
- Standard `tools/run_loop_playthrough.py`: both variants write **24 steps**;
  rendered **13 captures**. Both exit **1**, 264.73 s total, retaining
  `enemy_fight=blocked_unported_combat` (ground-item/pickup/Gold boundary). Owner disc:
  **994 files before and after** both acceptance runners. ObjectDB leak
  warnings remain at probe shutdown.

Remaining refusals: the PASS mission-2 audit explicitly leaves per-record
NPC/Trigger/PushTrigger link pairings and call containment Unknown, the
four-level sequence Inferred, both null-world destinations unresolved, and
NPC objective branches, deferred completion, boss/portal gates and portal
travel unexecuted. Cutscene service and all 660 selected placement objects
remain unbuilt here; enemy ports belong to port-mission2-hostiles. Physical
exit interaction, item drop/pickup, consecutive story travel, original arrival
placement, shared random draws during loading and campaign save/reload across
those exits are unverified.

The acceptance offer is explicitly **synthetic**, produced through the
pinned authored AMX generator. It proves the menu table/request, player and
campaign carry, rendering and refusal boundary. The other three level loads
are labelled **direct diagnostics**. No live hub unlock or complete retail
objective is claimed. The old-act test quest is authored and is cleared on
entry by the native act-reset contract. Entry supports only one local player
with known empty live and campaign bags; imported inventory, nonempty bags
and an unbound source act refuse before selection/loading. Live inventory
carry and flag-39 migration remain unported.

The supplied [shared rules](/Users/syndicatemike/.t3/scratch/2026-10-13-create-champions-return-to-arms-b6e61ac4/rules.txt)
require: "If the original behavior or format is not documented well enough
to implement exactly, stop and report precisely what evidence is missing."
The audit's Unknown links and executions therefore remain refusals.

Implementation: the table is read from the selected executable; mission 1
completion alone never unlocks mission 2; the real fade service drives menu
state 6. Act tables are read at the documented research-quests `7efd3ce`
addresses; `campaign_entry.hpp` uses the existing native change-level reset.
Mission-2 scripts stop even at discarded unsupported commands,
before later/deferred grants or exits. Null `newWorld(0)` and inferred boss
music refuse. Exit UI and invocation refuse the unproved mission-2 links.
Other levels retain their existing command policy. Binary table parsing
resides in `native/include/champions/mission_menu_table.hpp`. See
`docs/MISSION2_ROUTE.md` for the exact boundary and research needed next.

Evidence: research-mission2 `8b25047..ffff9a7`, `docs/mission2.md` at
`047adfe`, PASS audit `research-mission2-fix2.md`; research-story-route menu
pin `76d4447` and later PASS `research-story-route-r3.md` at `902b352`;
research-quest-log `03c2304`.
Bridge pattern: play-route `e141628`, read only. No constants or route
pairings were inferred into implementations.

Files: native `script_commands.{hpp,cpp}`, `mission_menu_table.hpp`,
`campaign_entry.hpp`, `script_commands_tests.cpp` and `mission2_host_tests.hpp`; GDExtension
`session_controller.{hpp,cpp}`; Godot `real_data_play.gd` and
`tests/mission2_route.gd` plus its UID; `tools/run_mission2_route.py`,
`docs/MISSION2_ROUTE.md` and this report.

Commands run (owner/runtime/dependency paths abbreviated):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(script-commands|quests|script-builtins-api|script-effects)$' -j 3 --output-on-failure
ctest --test-dir build/native -j 3 --output-on-failure
ctest --test-dir build/native -R '^player-exact$' -j 1 --output-on-failure
ctest --test-dir build/native -R '^session-facing-exact$' -j 1 --output-on-failure
python3 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz
cmake --build build/native --target champions-script-commands-tests -j 4
ctest --test-dir build/native -R '^script-commands$' -j 1 --output-on-failure
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension -j 4
cmake -S gdextension/level_texture -B build/level-texture-debug -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/level-texture-debug -j 4
cmake -S gdextension/creature_anim -B build/creature-anim-debug -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/creature-anim-debug -j 4
python3 work/mission2/run-godot-act.py
bash work/mission2/run-sweep-act.sh
python3 tools/run_mission2_route.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --research-dir "$RESEARCH_MISSION2" --out work/mission2/act-entry-final
python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/mission2/playthrough-act
```

Final counts/logs stay under ignored `work/mission2/`: `ctest-final.log`, `matrix-act/`,
`sweep-act/`, `act-entry-final/`, `playthrough-act/`, `final-gates.json` and
`player-direct-result.json`. Original content and
captures are untracked. The first full CTest attempt was stopped after 21
completed cases because a large native test file was still compiling. A
redundant follow-up build was also stopped. Only verified PIDs started for
those two commands were terminated; neither attempt is a completed gate.
