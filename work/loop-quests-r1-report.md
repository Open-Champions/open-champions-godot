STATUS: PARTIAL — opening quest state and the hub-to-mission-1 route work. Journal presentation awaits layout evidence; live turn-in and mission-2 action are unverified, and full native CTest retains one fixed-limit timeout.

2026-10-25 | `codex/play-quests` from `7ed3f21` | local only; no push

## Implementation and evidence

- The base already routed the documented RTA quest commands through the script host and carried campaign state between levels. This change adds a read-only `quest_journal()` view of player 0's active title/body localization keys, with the original `NoQuests` key for an empty list. It exposes mission-1 completion and mission-2 availability separately from the quest list.
- Audit `native-quest-log-r1.md` found unsupported journal presentation. The page now checks the read-only runtime parchment header (320×350) and live quest-state view, then refuses the unresolved font, wrap, body/entry advancement, empty-state position and sprite crop. It submits no parchment or glyphs. The host shows the refusal. Images remain only in ignored `work/`.
- Q is handled only in an active, unpaused game-folder play session after front-end, game-menu, dialog, help and picker ownership checks. The route confirms that Q types into the character name and cannot open the journal over pause or the in-game menu.
- The synthetic native script test exercises `setv`, `setMissionAvailable`, `addQuest`, `removeQuest`, `grantMission`, `newWorld('hub')`, then a *separate* mission-2 availability action. The playthrough checks 0→1 journal entries after the opening NPC choice and the existing hub menu → `blood1` route. It requires a journal result, verifies the explicit layout refusal and checks the actual sprite/text/glyph submission queue is empty; missing UI or failed Q handling fails the route.
- Evidence: `t3/research-quest-log` `03c2304`, `docs/spec/quest-log.md` and its passing audits; command model from `t3/research-quests` `7efd3ce`; route base `7ed3f21`. The text notice/help and compass host paths existed at the base. A quest row alone does not imply a HUD objective; no automatic objective was invented.

## Verification

- macOS arm64 Release native reconfigure/build passed. Full CTest (`-j 3`): **229 passed, 2 timed out, 231 total** in 3,126.31 seconds under heavy shared load. `player-exact` reached its fixed 240-second CTest limit in both the full and isolated runs. `session-facing-exact` reached its 600-second subprocess limit in those runs, then **passed 1/1 CTest in 340.03 seconds** on a later isolated rerun. The original player vector assertions passed **274,304** exact cases plus transport refusals and the original facing assertions passed **528** exact cases with its pinned trace hash in separate diagnostics. An ignored `work/` wrapper changed only their operational subprocess limits from 180/600 to 1,200 seconds. The full CTest remains non-green because `player-exact` has no standard CTest pass in this rerun. The affected `script-commands`, `quests`, `script-builtins-api` and `level-flow-exact` cases all passed in the full run.
- Godot 4.5.2 extensions are reused unchanged. Godot development matrix rerun: **54/54 commands exited 0**. The `scene_play_flow` and `ui_layout_smoke` logs retain two and one pre-existing script/API diagnostics despite exit 0. Full-combat headless: **2,620 checks, zero failures**; `FULL_TRACE` **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from the route base.
- Bounded `loop_playthrough.gd --route-only` passed headless and rendered after the audit fixes: opening dialog reached and closed, 0→1 journal entries after its choice, Q reached the page only in play, and the required journal check reported `layout_refused` with zero sprite/text/glyph submissions. The selected hub PushTrigger host action opened the native mission menu and arrived at `blood1` with campaign image and player record carried; mission 2 stayed locked. The new rendered capture was inspected and visibly shows the refusal. The dialog visual probe still reported zero box sprites/glyphs while the script choice path completed.
- Scripts-on real-data sweep rerun over the read-only owner disc: **111/111 tried, loaded, played**, **224 checks passed**, zero failures, 111 scripts attached, zero script faults. The disc still contained 994 files afterward; no extracted bytes, text or images are tracked.

Commands rerun from this worktree (all outputs are ignored `work/`; `DISC` is the read-only owner folder):

```sh
GODOT=../integration/work/runtime/Godot.app/Contents/MacOS/Godot
DISC="$PWD/../../private-validation/rta-disc"
OUT="$PWD/work/quest-fix-r2"
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'
ctest --test-dir build/native --output-on-failure -j 1 -R '^session-facing-exact$'
python3 "$OUT/long_vector_probe.py" player
python3 "$OUT/long_vector_probe.py" facing
"$GODOT" --headless --path godot --check-only --script res://ui/quest_journal.gd
"$GODOT" --headless --path godot --check-only --script res://scenes/real_data_play.gd
"$GODOT" --headless --path godot --check-only --script res://tests/loop_playthrough.gd
"$GODOT" --headless --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$OUT/route-headless-final" --route-only
"$GODOT" --path godot --script res://tests/loop_playthrough.gd -- --game-folder="$DISC" --out="$OUT/route-rendered-final" --route-only --rendered
"$GODOT" --headless --path godot --script res://tests/real_data_scene.gd -- --game-folder="$DISC" --cache="$OUT/sweep-cache" --all --levels=0 --from=0 --to=111 --ticks=60 --scripts --summary="$OUT/sweep-summary.json"
python3 work/play-route/run-godot.py
```

## Remaining boundary

- The journal's original font identity, wrap parameters versus clip, body and next-entry advancement, empty-state draw position and parchment sprite presentation need pinned research and an original rendered capture before the client can draw that page. Its quest-state view and header check are available; the visible page is an explicit host refusal.
- The synthetic turn-in test proves command bookkeeping and a queued hub request, not the shipped `exitzone` demo/counter branch in live combat. The separate hub action's live dialog reachability and mission-2 selection remain unverified. The route still invokes the selected hub PushTrigger action through the host; physical proximity/use dispatch and exact arrival marker are not proved.
- `setInfo` and `addHelpMessage/F` render through the existing timed HUD and modal help paths. `showCompass` remains subject to the existing documented camera/menu gate and its unresolved mesh/key evidence. Which script shows an objective during a real mission remains unknown.
