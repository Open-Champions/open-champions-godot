STATUS: MORE — five audited branches integrated and committed; final full CTest is still running. The campaign journey retains explicit physical-use, reward and checkpoint-reload refusals.

# Playable integration wave 32 — 2026-10-25

## Scope, evidence and merges

Worked only in `worktrees/playable-integration`, branch `codex/playable-integration`, starting clean at wave-31 `68c4ccaf`. Read the shared rules, repository instructions, contributing guide, roadmap, wave-31 report and the five requested PASS audits. Evidence remains the documented contracts pinned by those audits; no new retail behavior or values were invented. Private disc inputs were read-only. No push, branch creation, PR, issue or process-name termination occurred.

All five requested tips are ancestors of the integrated head through full `git merge --no-ff`, in the requested order. No branch was omitted and no cherry-pick was used.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-boar-ai` | `7cb5bee5` | `play-boar-ai-fix.md` | `621781c5` | `native/level_flow_targets.cmake` |
| `codex/play-combat-moves` | `32a87e18` | `play-combat-moves-r1.md` | `6fba46f9` | `godot/scenes/real_data_play.gd`, `godot/tests/scene_play_flow_component_fixture.gd`, `native/level_flow_targets.cmake` |
| `codex/play-vendors` | `9c6e23d8` | `native-vendors-r1-fix.md` | `2a17dafd` | `godot/tests/menu_capture.gd`, `godot/ui/game_menus.gd`, `native/src/menus.cpp`, `native/tests/menus_tests.cpp` |
| `codex/play-exits` | `226fcd4d` | `play-exits-r3-fix.md` | `1a032895` | `gdextension/src/session_controller.cpp`, `gdextension/src/session_controller.hpp`, `godot/scenes/real_data_play.gd`, `godot/tests/loop_playthrough.gd` |
| `codex/play-death` | `23d769ca` | `play-death-fix.md` | `b4aaefbc` | `godot/tests/scene_play_flow_component_fixture.gd`, `native/include/champions/campaign_fight.hpp`, `native/include/champions/combat_runtime.hpp`, `native/src/campaign_fight.cpp`, `native/src/campaign_fight_probe.cpp` |

Each merge was committed immediately after conflict resolution, before starting the next. Separate commit `6e654fb2` refreshes the vendor report's audit LOW: one retained sprite, two tint refusals, bounded live strings, authoritative pause/resume and queue order; its earlier verification is explicitly attributed to the earlier implementation, not the fix commit.

## Conflict resolutions

- Retained the campaign actor identity/lifetime fix before composing other work. The Clang-only Release test-driver protection keeps the existing no-inlining safeguard plus the boar branch's no-unrolling safeguard. Gameplay libraries and trace executables retain Release optimization; the older combat branch's test-driver `-O0` override was not substituted. No test/check was removed.
- Combined combat input help and WarBoar diagnostics/refusals. The lifecycle spy retains its existing empty art dictionaries. Existing campaign `MovePad` edge handling remains authoritative rather than restoring the death branch's obsolete attack-only latch.
- Vendor import clears both inventory display caches and synthetic stock. Inventory footprint/cursor checks and shop packet/tint/pause checks are all retained. Inventory fixtures now use the incoming authored executable-string reader, including the packed-grid fixture, rather than a zero reader that no longer satisfies bounded adoption. Inventory refusal notes and vendor stock notes coexist; inventory layer-7 drawing and shop queue drawing both remain.
- Exit state/status APIs coexist with hub savepoints and marker observations. Script-host replacement still installs the matching fight, adapter and shared authority before applying gate lifecycle resets. Physical `dialog_use` remains refused, including the existing alternate-hub boundary; the incoming indexed talk diagnostic is clearly separate. The researched mission-menu component is retained instead of restoring the older host-layout panel. The journey keeps exit refusal probes, the current fight-level setup and all combat-move checks.
- Death task/check callbacks and the boar outgoing-damage refusal gate coexist. Animation advancement retains both native boar yaw and death-clip accounting. Counts/views expose both features, and the native probe retains both AI and death switches. No checkpoint fallback or synthetic revival was added.

## Gates and evidence

Raw logs, private saves/caches and captures remain ignored under `work/playable-wave32/` and `work/private/w32-*`; no private payload or screenshot is tracked.

- Native Release configure/build: PASS, `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` followed by `cmake --build build/native -j 4`.
- Universal macOS GDExtension and level-texture libraries, both `template_debug` and `template_release`: all four configure/build pairs PASS, with Release optimization and `arm64;x86_64`.
- Full `ctest --test-dir build/native --output-on-failure -j 3`: IN PROGRESS; 247 registered tests. Final result will replace this line before completion.
- Godot dev matrix: **54/54 exit 0**. Full synthetic combat: **2,641 checks, 0 failures**. `FULL_SAVE_LOAD`: **300/300** presentations with identical continued/uninterrupted presentation hashes.
- `FULL_TRACE`: **36,402 bytes**, SHA-256 **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, unchanged. No baseline or fixture was weakened.
- Both private level sweeps: scripts-on **111/111 tried, loaded, played**, **230 checks**, 0 failures, 0 top-level refusal kinds; scripts-off the same. Each uses four 60-tick slices. Internal unsupported script/object/terrain services remain in their separate counters; top-level zero does not mean those paths are supported.
- Native AI probe: PASS, **78 boars**, **600 ticks**, **3 moved/wandered**, **2 chased**, **2 attacked**, **35 state changes**, **13 attack entries**, **14 hit events**, 0 failures. This is labelled diagnostic placement, not a normal-route trajectory or outgoing-damage claim.
- Native death probe: PASS, **78 actors**, **600 ticks**, dead/zero HP retained, both countdowns reached zero, phase 1/fade 128 retained, **239 native death-clip steps**, record-retention assertions passed, reload refused. The hit is explicitly diagnostic.
- Added focused Godot checks: campaign moves **33/33**, death UI headless **11/11**, death UI rendered **13/13**, all exit 0. The rendered test asserts half/full-alpha pixels and that presentation does not advance native death time.
- Rendered menu capture exits 0. **16 PNG files** were produced (the inherited summary prints 15); inventory cursor/footprint and vendor pause, live sale, no-debit purchase refusal, single-sprite and draw-order assertions passed. Inspected the actual live-shop and page-footprint-refusal captures, plus the journey's fight and death/reload captures. These are host renders, not original-client pixel comparisons.
- The matrix is not diagnostic-clean: `ui_layout_smoke` retains the known `hud_text_glyphs` four-versus-five-argument error despite exiting 0. The former lifecycle-spy missing-`loaded` errors are absent. Some runs retain ObjectDB exit-leak warnings. These are not relabelled as clean logs or original-client parity.

## Full scripted journey

`tools/run_loop_playthrough.py` completed both variants: **53 headless steps**, **54 rendered steps**, **37 rendered captures**, both exit **1** for bounded refusals rather than a crash or timeout. The wrapper counted **994 disc files before and after**. Both variants have the same authoritative fight/AI figures.

Order: game folder, boot/logo/intro/main menu, class and character creation, first level, player visual/pose, level script, guarded hub portal, physical-use refusal with explicit indexed-talk diagnostic, dialog/prompt/choice close, hub quest service, hub savepoint, first mission unlock/request, mission-menu opening/table/assets/unsupported-art/row-zero/art gate, four network prompts and unknown-network refusal, rendered-only menu frame, navigation/fade gate, story request/marker records/arrival, level change, walk-in exit probe, physical PushTrigger refusal, Portal/gate refusal, enemy level, combat moves, fight, multi-surface scene, pause/save/quit, carried load, cold load, mismatched display refusal, player death.

- Physical walk-and-talk is **not** claimed: ordinary use refuses, then the harness opens the indexed talker diagnostic. Hub quest/save services and the mission-menu route still execute.
- Walk-in exit remains **refused in this integrated full journey**: **92 source/attempt probes**, **82 navigation stalls**, **6 entered-without-change cases**, no queued-by-walk request and no observed successful dispatch at the final source. The branch's earlier standalone successful walk-in report does not establish success for this run. No teleport or indexed exit is substituted for a successful physical walk.
- PushTrigger use and Portal arrival return `refused_expected`, with no action/use/request/position mutation; the real gate control is visible and disabled by design.
- Combat moves pass **22/22** per variant, including native animation hooks, edge/hold/combo behavior, pause release and explicit unarmed guard/heavy/ability refusals without mana spending.
- Both fight records show **2,227 boar state changes**, **1,063 attack entries**, **956 hit events**. Boars are no longer inert. The separate AI probe confirms chase and attack. The fight uses labelled placement at the weakest enemy after physical approach fails; a real native damage kill awards **2,100 XP** and one level. One loot roll produced **0 items** with **8 shared RNG draws**, Gold stayed unchanged and no ground-object pickup is claimed. The step remains `blocked_unported_combat` for the missing automatic-container Gold source/gain.
- Carried and cold loads restore the saved campaign/player state in their bounded domain. Mismatched display is refused as expected; world-object and in-flight-script continuation remain unsupported.
- The last step is **`player_death: blocked_death_reload`**. The labelled lethal hit closes the menu, advances the native death clip and both countdowns, reaches full fade/phase 1 and retains possessions/dead state. There is no valid checkpoint/reload service, so the run ends there without respawn. The rendered body is explicitly a rest pose, not a verified death pose.

## Exact command families

Commands run from this worktree; private input paths are intentionally redacted in this tracked report. The ignored drivers copy wave-31 command lists with only the wave output directory changed.

```text
git merge --no-ff 7cb5bee5 -m <wave-32 merge message>
git merge --no-ff 32a87e18 -m <wave-32 merge message>
git merge --no-ff 9c6e23d8 -m <wave-32 merge message>
git merge --no-ff 226fcd4d -m <wave-32 merge message>
git merge --no-ff 23d769ca -m <wave-32 merge message>
git commit --no-edit
git commit -m "Refresh vendor report for audited string pause and draw fixes"
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 -u work/playable-wave32/run-builds.py
ctest --test-dir build/native --output-on-failure -j 3
python3 -u work/playable-wave32/run-godot.py
bash work/run-slices-r21.sh w32-on --scripts
bash work/run-slices-r21.sh w32-off
build/native/champions-campaign-fight-probe --game <read-only disc> --level <documented fight level> --ticks 600 --ai-check
build/native/champions-campaign-fight-probe --game <read-only disc> --level <documented fight level> --death-check
python3 -u tools/run_loop_playthrough.py --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder <read-only disc> --out work/playable-wave32/playthrough
<same Godot> --headless --path godot --script res://tests/campaign_moves_smoke.gd
<same Godot> [--headless] --path godot --script res://tests/player_death_smoke.gd
<same Godot> --path godot --script res://tests/menu_capture.gd -- --game-folder=<read-only disc> --capture-dir=<ignored wave-32 capture directory> --cache=<ignored wave-32 cache>
git diff --check 68c4ccaf..HEAD
git merge-base --is-ancestor <each of the five audited tips> HEAD
```

The extension driver runs `cmake -S gdextension -B build/mac-<target> -DGODOT_CPP_DIR=<existing pinned godot-cpp> -DCMAKE_BUILD_TYPE=Release -DCHAMPIONS_LEVEL_FLOW_WATER_ROOT=<this worktree> -DGODOTCPP_TARGET=<target> -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64`, then `cmake --build build/mac-<target> --config Release --parallel 2`. It repeats for `gdextension/level_texture` into `build/level-texture-<target>` without the water-root argument. Both targets are `template_debug` and `template_release`.

## Remaining boundaries

Normal-route boar-to-player damage, path search, group activation and live trajectory parity; researched equipment/learned-record handoff for guard/heavy/abilities; physical interaction and campaign gate lifecycle; successful walk-in reachability in this integrated journey; vendor stock generation/purchases, two sprite tints and localized/pixel parity; Gold sourcing and ground loot/pickup; Minotaur/placement and other explicit script factory refusals; valid checkpoint, world/camera restoration, respawn/revival and rendered death pose; other-platform execution and cross-architecture traces remain unsupported or unverified. These are not waived by the synthetic gates, and no guessed values or fallback behaviors were added.
