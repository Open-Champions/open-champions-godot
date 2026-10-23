STATUS: COMPLETE — Wave 25's four pinned merges, arm64 CTest, Godot matrix, both 111-level sweeps, trace comparison and scripted playthrough have recorded results; Rosetta was skipped under high load.

# Playable integration wave 25 — 2026-10-22

## Merge scope

Starting at wave 24 `3850dbb`, merged only the audited pinned commits, in order:
`1143233` (creature stats) → `47f21a0`, `cb5ae47` (items gaps) →
`6eb0c23`, `2cf6668` (menus) → `c0f5279`, and
`b9f2ab4` (play visuals) → `a7ea343`. The creature merge retained both the
dialog/music attach and level statistics/WORLD attach. The items merge
retained the faithful-seed and statistics HP gates alongside the new item
refusals, generator path, and test cases. No other branch was merged.

## Verified so far

- Native arm64 Release configure/build: pass. Full arm64 CTest: **233/233**
  in **1,022.32 s**. No test timed out, so no serial timeout rerun was needed.
  Four universal macOS GDExtension configure/build pairs: pass.
- Godot dev matrix: **54/54** exit zero. Full headless combat: **2,620 checks**, zero
  failures; `FULL_SAVE_LOAD` agrees at 300/300 presents.
- `FULL_TRACE` remains **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**,
  **36,402 bytes**, exactly wave 24. None of the four merged changes altered
  this authored fixture's trace; there is no trace change to attribute.
- Both 60-ready-tick four-slice real-data sweeps: scripts on and off each
  **111/111 tried, loaded and played**, 230 passed checks, zero failures,
  empty top-level refusal lists. The scripts-off sweep retains its wave 24
  deterministic counts; only timing, memory peak and audio-frame counts vary.
  With scripts on, constructed objects rise **324 → 429**, unbuilt objects
  fall **15,577 → 15,472**, and stub refusals fall **266 → 163**. The new
  internal refusal counters include 20 level-statistics, 56 hide/wait stub,
  8 untraced constructor table-slot and 110 item-load gate refusals. These reflect
  the merged creature-stats/items-gaps paths; no isolated branch run of this
  exact wave 25 integration was made to allocate every downstream script
  count change between them. `FULL_TRACE` is independent of those sweeps.

## Scripted playthrough

`python3 tools/run_loop_playthrough.py` completed both variants and wrote
results. Its overall exit is **1** because it deliberately returns nonzero
for refused steps. The read-only disc had **994 files before and after**.
The headless run wrote zero screenshots; the rendered run wrote **11**.
I inspected the rendered level and dialog captures. The real-level dialog
is visible and readable; the shown/hidden viewport comparison changed
**7,454** dialog-box sample pixels and drew **41** glyphs. The rendered
actor comparison changed **145** sample pixels, but class-resource mapping
is still refused and the practice figure remains labelled as such.

| Step | Headless | Rendered |
| --- | --- | --- |
| game_folder | opened | opened |
| boot_card | boot-card | boot-card |
| logo | logo screen | logo screen |
| intro | intro-movie | intro-movie |
| main_menu | main | main |
| character_class | char-class | char-class |
| character_created | playing | playing |
| first_level | entered, script enabled | entered, script enabled |
| player_visual | not applicable headless | refused: no mapped class mesh |
| level_script | enabled | enabled |
| walk_and_talk | dialog_open | dialog_open |
| dialog_visual | not applicable headless | drawn |
| dialog_prompt | drawn | drawn |
| dialog_choice_close | closed | closed |
| enemy_fight | refused | refused |
| pause | pause | pause |
| pause_save | refused | refused |
| host_save_fallback | saved | saved |
| quit_to_main | main | main |
| load_and_carry | refused | refused |
| level_change | refused | refused |

Combat had zero actors before/after and no handled attack. The pause menu
exposes no save entry; the host save fallback wrote a snapshot. The attempted
load stayed on the load screen, and no pending script level change existed.
Both variants reported one deduplicated load refusal. These are open client
gaps, not successful end-to-end game progression.

## Remaining validation and limits

Rosetta CTest was skipped under the user's load condition: after arm64 CTest,
the host had **18 logical CPUs** and a load average of about **35** while
other agents' tests and builds were active. No wave 25 x86_64 execution is
claimed. The last wave's Rosetta result does not validate this merged head.
Original-client fidelity, physical Intel execution, Windows/Linux runtime,
and original-client netplay remain unverified. Private data was used only
for read-only acceptance; no private bytes, decoded content or assets are
committed.

Counts-only logs are under ignored `work/playable-wave25/` and
`work/private/`. Commands: `cmake -S native -B build/native
-DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j 4`,
`ctest --test-dir build/native --output-on-failure -j 3`,
`python3 work/playable-wave25/run-builds.py`,
`python3 work/playable-wave25/run-godot.py`,
`work/run-slices-r21.sh w25on --scripts`,
`work/run-slices-r21.sh w25off`, and
`python3 tools/run_loop_playthrough.py` with the owner-selected read-only
game folder and an ignored output directory.
