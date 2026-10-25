STATUS: MORE

Implementation: `5d4db4e` and viewport fix `d5f2e02` on `codex/play-hub-npcs`, based on `252f34a`.
No push, PR, issue, shop duplication, private-data commit or sibling edit.

Added the audited native direct Savepoint use model, per-record hub adapter,
service/refusal controls and Player continuation save flow. Existing physical E
conversation reaches the fresh primary-hub ending: one quest and one mission
availability bit added, gold and XP unchanged. Savepoint use preserves the Player
and its saved continuation reloads with matching Player and campaign observations.
No healer, trainer, stash or second quest NPC was invented.

Evidence: research-hub-npcs `31db144`, `FINDINGS.md` and
`docs/spec/hub-npc-services.md`; audits `research-hub-npcs.md` (FIX),
`research-hub-npcs-fix.md` (FIX) and `research-hub-npcs-fix2.md` (PASS).
The earlier FIX findings' unsupported effects remain Unknown and refused.
Shared `rules.txt` requires: "If the original behavior or format is not documented
well enough to implement exactly, stop and report precisely what evidence is
missing. Do not guess or invent constants." The untraced routes and integrated
state/placement evidence listed below are the remaining gate, not new permission
requirements.

Files: `native/include/champions/hub_services.hpp`, its native tests,
`gdextension/src/session_controller_hub.cpp` and additive controller/CMake hooks,
`godot/ui/hub_services.gd`, real-data scene hooks, hub smoke and journey tests,
`docs/HUB_SERVICES.md`. The existing scene-flow art fixture now supplies the
production HUD consumer's empty dictionaries; its focused rerun passes 65 checks
without the former script errors. Existing ObjectDB exit warnings remain.

Gate results (native collection pending):
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build/native -j 4`: final native test translation unit still compiling.
- `cmake --build build/godot-extension --config Release -j 4`: PASS; CMake Release, Godot template_debug ABI.
- Focused `ctest --test-dir build/native --output-on-failure -R '^hub-services$' -j 1`: PASS 1/1, five native checks.
- Full `ctest --test-dir build/native --output-on-failure -j 3`: running.
- Godot dev matrix: PASS 55/55 commands including the new 13-check hub smoke, with the scene-flow fixture corrected and rerun (65 checks, zero script errors). The retained initial matrix log contains the prior fixture errors; the final summary selects the clean rerun.
- `bash work/hub-npcs/run-slices.sh hub-npcs-resume --scripts`: PASS 111/111 loaded/played/scripts attached, 230 checks, zero failures. Gameplay counters match the inherited final sweep; audio/frame/time/memory counters vary with presentation.
- `FULL_TRACE`: unchanged SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, 36,402 bytes.
- Focused private hub journey: PASS headless and exported macOS arm64 app, 18 steps each; seven exported captures inspected, selector/action fit checked, zero unexpected refusals.
- `python3 tools/run_loop_playthrough.py`: FAIL/incomplete overall, exit 1 in both headless and rendered runs; 26 steps each, 0/14 captures, one refusal kind each. Hub quest, savepoint, Player/campaign readback and later continuation checks pass. The existing `blocked_unported_combat` result retains unimplemented ground-item pickup and Gold reachability (also original starting equipment/AI limits). Disc file count is unchanged, 994 before/after.
- Native continuation is verified here; original memory-card persistence is not claimed.

Remaining refusals: original Savepoint proximity/prompt, autosave, completed effect
and saved point state; co-op resurrection/placement; mission-map presentation and
completion (including untraced cancel/fade tail); gate discovery, party-distance
checks, travel and disc arrival; alternate-hub dialog/trigger effects; both rings;
and later/alternate choice effects. Either hub's dialog-choice reachability to
item removal is Unknown; no item cost or absence of a cost is assigned, and
the existing generic main-hub dialog is not certified for later choices. Original UI parity,
Windows/Linux/Intel execution and complete hub services remain unverified.

All generated captures, decoded assets, continuations and raw logs stay ignored.
Only counts and generic diagnostics are reported. Two pre-existing generated
`.gd.uid` files outside this task remain untracked and untouched.

Reproduction/evidence under ignored `work/hub-npcs/`:
- `python3 work/hub-npcs/resume-run-godot.py` runs the 55-command matrix; `resume-matrix-final-summary.json` records the clean fixture rerun and all final log selections.
- Godot `--headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder=DISC --out=OUT --hub-only`, with explicit owner-selected read-only DISC and ignored OUT; repeat in the exported app without `--headless`.
- Godot `--headless --path godot --export-debug 'macOS universal' build/exports/hub-services/OpenChampions.zip`, then `ditto -xk` and execute the exported app's `Contents/MacOS/Open Champions`. Only arm64 execution is verified; the preset name does not establish Intel coverage.
- `python3 tools/run_loop_playthrough.py --godot GODOT --game-folder DISC --out work/hub-npcs/resume-playthrough` records both variants and the unchanged file count in `run-summary.json`.
