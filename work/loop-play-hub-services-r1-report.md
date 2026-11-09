STATUS: COMPLETE — play-hub-services round 1 (2026-11-09). All gates PASS on the mini: Release build and GDExtensions, focused 4/4, full CTest 260/260, strict Godot 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged. Journeys match wave 38 step for step, with 10 (headless) / 11 (rendered) hub and mission-menu rows moving from refused to passing and none regressing.

# play-hub-services round 1

Branch `codex/play-hub-services`, from wave 38 VERIFIED `16b66176`. Commits:
`bec4532b` (researched dialog tables and five-native fence), `4a3865cc`
(walk-in ring trigger, docs), `f24bca3a` (focused hub journey), and this report.

Research: `t3/research-hub-npcs` @ `fbbfe65`, `docs/hub-services.md` (HS below),
`tools/ref_hub_services.py` (RHS); audits `research-hub-services.md` and
`research-hub-services-fix.md` (PASS, HSF-1..3 LOW). HSF-2 and HSF-3 are
followed (see below). HSF-1 concerns only the research report's sequence counts;
nothing here uses them.

## What the client already did (wave 37/38) and what changed

Already present and kept: the generic AMX interpreter with the binder's
original-memory natives. That covers the double-buffered deferred queue
(`Variables::queue_deferred` / `run_deferred`), `gamePaused` (0x41b4f4 / 0x500ae8 /
0x44fcd0), `activateMissionMenu` gated on `dialog_isOpen` and the recall byte,
`activateStore` deferring with 0x4fdd30 while a dialog is open, `setEntrance` →
0x4f8468, and the dialog machine opening the store at message 3. The play-hub-npcs
round's narrower first/second-offer hub binding (`hub_offers.hpp`) stays as the
hub fallback, and its tests are unchanged.

Ported now (all Observed or Executed in HS):

| Behaviour | Client | Research |
|---|---|---|
| Hub 31-node / hub2 26-node dialog tables, bound field by field (condition, choice count, choice condition, action, next); variables from literal `getv` sites | `hub_dialog.hpp` `Binding::match`; `script_dialog.cpp` open | HS:155-228, 23-35; RHS:58-151 |
| Predicates and start rule; hub2 C(1)/!C(1) choice guards | `Binding::predicate` / `start` | HS:76-101, 201, 289-299 |
| Action only as the current visible choice; natives limited to setMissionAvailable, addQuest, setv, changeSwoop, activateStore, each as its effect (A(n),1 / one quest / swoop 0 / store 0 / demo-intro var = 1). Item, gold or any other native refuses before it runs | `command_map` fence, `hub_service_refused` | HS:112-145 |
| Store deferral inside the dialog, open at dialog end | existing binder + dialog machine; now admitted for every SHOP action | HS:247-262 |
| hub2 must bind the table, else the open refuses | `DialogLevel::require_hub_services` | HS:196-245 |
| Tables of 10 and 13 cells (bounds 9 / 12), loops visit 9 / 12 | `dedupe_tables`, `hub_service_research()` | HS:329-341 |
| Walk-in dispatches `trigger(ring record name, 0)` once: admitted for the ring record in hub/hub2 and counted only | `Host::hub_ring`, `callee_unknown`, `virtual` slot 3 | HS:393-401 |
| Mission-map warning: K.poll re-queues itself, counts unpaused **Script-task runs**, calls `activateMissionMenu` on the 6th with no entrance write; new warning restarts | original bytecode on the client's deferred queue; synthetic test counts runs, not frames (HSF-2) | HS:373-383; RHS:331-349 |
| PushTrigger nethostonly=1, radius 79 (hub) / 78 (hub2) checked against the record | `hub_service_targets().record_matches_research` | HS:363-366 |
| Cutscene line list (one node, no condition, no choices) classified, never offered as a dialog; 0x34a4 requests its stem | `cutscene_line_list`, `hub_research()` | HS:64-72 |
| hub2 refusals lifted in `dialog_use` / `dialog_talk_to` / `script_run_exit`, now that the research covers hub2 | `session_controller.cpp` | HS:196-245, 361-401 |

Refused with reasons (`hub_service_research().unknowns`, `docs/HUB_SERVICES.md`):
the use of L-INN's imported checkPlayerGold / givePlayerGold / takePlayerGold, and
whether L-INN charges for the stay (HS:351-353; HSF-3 wording: the hub-side
restore charges nothing, Executed HS:325; L-INN's charge is Unknown); how a player
reaches L-INN (HS:354); the caller of 0x34a4 and the line list's link to it
(HS:66-69); whether the help box pauses (Inferred, HS:381-382); several queued
K.poll copies (HS:382-383); the stack-read store index (HS:152-153); the bag cell
`removePlayerItem` deletes (HS:470); physical PushTrigger use at the radius (the
research asserts the values, not the use query). The ring's own state request
(`autoPropSetState`, virtual slot 18) stays refused and non-stopping, as in
wave 38. Its rendered effect is Inferred (HS:403-409).

## Tests (synthetic only, added to existing files)

`native/tests/hub_offer_host_tests.hpp` (run by the existing `dialog` CTest) gained
a `table` section with authored AMX at the researched offsets and synthetic text.
It covers: both tables binding (the legacy fixture does not); hub2 intro with and
without C(1); the mission-2 offer; SHOP on the mission-3 offer (A(3), swoop 0,
store only pending while open, deferred shop at end); every hub2 reminder node and
node 0 once started; the hub mission-10 MORE chain with its quest; the fence (gold
native first, wrong mission) refusing before any write; hub2 refusing a
mismatched table; the 10/13 bounds; the line-list rule; walk-in `trigger(ring, 0)`
counted once, while value 1 or a non-hub level refuses; and K.poll on the 6th
run, on the 9th with runs 2-4 paused, blocked by recall, with no entrance write.
dialog: 1,126,638 checks, 0 failed. `godot/tests/loop_playthrough.gd`
`--hub-only`: the hub2 expectation changed from "conversation and trigger
refuse" to "the conversation binds the researched hub2 table and closes with no
fence refusal; the PushTrigger record matches". This is an expectation change,
because the research supersedes the old refusal. The full-journey path is
unchanged.

## Private disc probe (counts only, ignored `work/hub-services-r1/probe.gd`)

hub: live dialog binds the `hub` table, start node 1 (fresh); dedupe tables 10/13
match; 3 dialog files = live + 1 backup + 1 cutscene line list whose stem 0x34a4
requests; PushTrigger record matches. hub2: binds the `hub2` table, start node 1;
dedupe 10/13; 2 files = live + 1 backup; PushTrigger matches. Focused
`--hub-only`: hub2 conversation bound, 4 choices taken, closed, 0 fence
refusals, direct savepoint accepted (`alternate_hub_services` = bounded). The
focused run still exits 1 on the pre-existing primary-hub `hub_savepoint`
readback refusal ("reload is not implemented"), which wave 38's journey also
reports.

## Gates (mini, tree at `f24bca3a`, evidence in ignored `work/hub-services-r1/`)

- Disk: the chain waited at 25 GB free and rechecked every 3 minutes. It started
  at 36 GB. An earlier chain started at 29 GB was stopped by its own PIDs before
  it did any build work.
- Release native build `-j 4`: PASS, exit 0 (incremental, 59 s).
  GDExtensions `--parallel 4`: PASS, exit 0, 348 s. `lipo` reports x86_64 and
  arm64 for all five libraries (`ext-lipo.txt`).
- Focused CTest `^(dialog|hub-services|script-commands|quests)$`: 4/4 PASS.
- Full CTest `-j 3`: **260/260 PASS**, 1,274.06 s. No load timeout, so no rerun.
- Strict Godot matrix (`tools/godot_matrix.py matrix --captures --timeout 1200`):
  **54/54 PASS**. Same 54 cases in the same order as wave 38. check-log: 105 logs,
  0 failing.
- Combat: **2,645 checks, 0 failures**. FULL_SAVE_LOAD **300/300**,
  presentation `cb2309ab…d8573` continued = uninterrupted (same as wave 38).
  FULL_TRACE **unchanged**: 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- Journeys (`tools/run_loop_playthrough.py`, headless then rendered, read-only
  disc 994/994 files): wrapper exit 1 (the retained full-journey refusal, as in
  wave 38). 80 / 81 steps, log errors 0 / 0, 38 rendered captures, pickup 10→11
  in 52 walk ticks, ending `player_death` refused, as in wave 38.

### Journey comparison with wave 38, step by step

The step names are identical at every index in both variants. These results
changed, and every change is a refused row that now passes:

| Index (headless / rendered) | Step | Wave 38 | Now |
|---|---|---|---|
| 24 / 24 | hub_portal_script_stop | refused_script_stopped | not_stopped |
| 25 / 25 | hub_portal_request | refused | requested (1 first, 0 repeat) |
| 26 / 26 | mission_menu_opening_gate | refused | checked |
| 27 / 27 | hub_trigger_mission_menu | refused | opened |
| 30, 31, 32 / same | mission_menu_unsupported_art, row_zero_draw, art_gate | refused | checked |
| 33, 35, 36 / same | mission_menu_network_prompt (3 of 4) | refused | checked |
| — / 38 | mission_menu_render | refused | checked |
| 38 / 39 | mission_menu_locked_navigation | refused | checked |

Cause: wave 38's opening conversation hit "hub action beyond documented
first/second offers is Unknown" on the close-fade retry, and the walk-in's
`trigger(ring, 0)` was refused as a non-talker target, which stopped the level
script. Both are now admitted by the research. The refused rows went from 27 to
17 (headless) and from 28 to 17 (rendered). Refusal kinds went from 8 to 7: the
two hub-action kinds are gone, and one kind is new,
`mission_menu_art_gate: Mission art refused`. That one is the step's own
deliberate negative control (it configures the art with an empty font and
requires refusal). Wave 38 never reached it because the menu never opened.
`mission_1_story_request` stays refused, as in wave 38.

## Notes

- The mini's bare repo has no `codex/play-hub-services` branch, and its worktrees
  are plain copies. So the commits were made on the MacBook worktree, which only
  moved forward from `16b66176`, and the tree was rsynced to a mini copy seeded
  from `play-quest-log` (without `build/` or `.godot`). One earlier sync with
  `--delete` removed the mini's generated `godot/bin` during an extension build.
  That build was rerun, and later syncs delete only inside source directories.
- No push, PR, install, `/tmp` use on the mini, process-name kill, private-disc
  write or mini home path in tracked files. The mini `build/` was deleted
  after the gates.
