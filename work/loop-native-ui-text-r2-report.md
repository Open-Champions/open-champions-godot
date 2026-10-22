STATUS: FIXED — all seven UI text audit findings are fixed and the focused arm64 CTest, Godot dev matrix, 111-level sweeps and FULL_TRACE pass; the earlier full arm64 CTest's player-exact timeout remains unverified.

# Native UI text extras, round 2 — 2026-10-22

## Scope and evidence

Merged `codex/playable-integration` at `2e57376` into `codex/native-ui-text`
(`4e08b22`). The pinned research remains `t3/research-ui-text-gaps` at
`523b3db`: its documented, spec and reference-model hashes match
`native/tests/ui_text_gaps_contract.json`. The 3,698 synthetic vector rows
match the pinned SHA-256; the native UI replay passed 13,941 checks.

The wave-24 dialog-gaps merge and the UI text port both touched the dialog
controller, script host header and link list. The merge keeps the host's
per-displayed-frame fade, uses the host's observed 0x7e glyph byte, counts
the drawn cursor glyphs, and links both libraries. The native host test now
checks the fade-gated leave message separately from the camera's later end
message. That test passes 1,122,365 checks; `dialog-gaps` passes 6,315.

Round 1 assumed the unmodelled compass gate words were open when drawing the
scene label. The scene now refuses both mesh and label until the researched
gate, player slot and player object are supplied. The native world, two-pass
and label plans remain tested. The second pass's light state is Unknown, so
the mesh remains refused. `docs/UI_TEXT_GAPS.md` records the boundary.

## Verification

- Arm64 Release: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`
  and `cmake --build build/native -j 4` passed. `file` identifies the UI test
  executable as arm64. Full `ctest -j 3`: **230/232 passed** in 2,505.80 s;
  `player-exact` hit CTest's 240-second timeout and `session-facing-exact`
  hit its runner's 600-second timeout. The latter **passed alone** in
  249.20 s. `player-exact` also timed out at 240 seconds when selected alone;
  its direct comparator then hit a 180-second native subprocess timeout.
  An ignored local runner extending only that subprocess limit was stopped
  after nine minutes because concurrent jobs left it at 23,665 of 274,304
  rows when measured at seven minutes. It produced no comparison result. The final
  targeted arm64 CTest for `dialog`, `dialog_gaps` and `ui-text-gaps-exact`
  passed **3/3**.
- Universal debug GDExtensions built. Godot dev matrix: 52/54 on the first
  pass; the two rendered creature-blend pixel checks failed while other
  render jobs were active, then both passed cleanly on focused rerun (22
  checks each, zero assertion errors). The final UI layout and play-HUD smokes
  passed after the compass gate change.
- Final headless Full: 2,620 checks, zero failures, **FULL_TRACE** SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  36,402 bytes. The trace differs from wave 23 after the wave-24 merge; it
  repeated identically on the final code.
- Read-only 111-level sweeps, scripts on and off: both **111/111 tried,
  loaded and played**, 230 checks, zero failures. Scripts-on refusals stayed
  14 stops, 266 stubs, zero callee refusals, matching round 1; scripts-off
  refusal counts were zero.
- Rendered real-level help and bust smoke: one focused level passed 20
  checks, drew 70 inner-frame pieces, and captured the late-frame button
  glyph and label. Before the audit fix, a meter-branch level passed 22 checks,
  with one clipped bar and one gamefontsmall caption. The audit fix now omits
  that bar until a real simulation frame is available; see below. The
  original-client pixels were not compared.
- Real-data dialog smoke: 111 levels loaded, 24 talks attempted, 22 opened
  and closed, 669 cursor-glyph quads, zero harness failures. Two talks are
  now refused by wave 24's documented uninitialised camera-offset case;
  the round-1 host had opened them without that evidence.

## Remaining refusals and limits

The compass mesh and scene label remain refused as described above. The
chime's two-update slot life is Inferred and needs the sound cursor the native
runtime refuses. The notice-box half and raster-exact odd-top sprite behavior
remain outside executed evidence. The initial UI fade value and headless
one-ready-tick-per-displayed-frame proxy are labelled host assumptions. The
animated bar is refused without `g_simFrame`; the host's `Hud::tick_message`
latch is passed to the bust plan. The player-exact arm64 comparison remains
unverified in this round because its wall-clock limits were repeatedly
exceeded under severe machine contention; no assertion mismatch was observed.
Rosetta x64 CTest and exports were outside this round. No private bytes,
names or decoded content were committed.

## Audit fixes — 2026-10-22

- **UTG-1:** `text_hud.gd` passes the host's exported `latch` to `bust_plan`
  and refuses the bust if that key is absent. `Hud::tick_message` sets it for
  four local players without network play (`native/src/strings.cpp:499`);
  the `hud_spellSelect` path remains unmodelled. The stale documentation and
  `UNVERIFIED` wording were corrected.
- **UTG-2:** removed the unproven `ready_ticks == g_simFrame` mapping from
  `text_hud()`. Without a real frame counter, the host plan omits the animated
  bar but retains the caption, meter field, portrait and bust fade. The portable
  exact plan still draws the bar when supplied a synthetic simulation frame.
- **UTG-3–5:** `UNVERIFIED` now labels the assumed-passing 3,000-unit help
  display-list gate; the compass-plan comment names the caller's actual gate
  check; `docs/UI_TEXT_GAPS.md` labels the headless ready-tick/display-frame
  proxy and its effect on dialog message 3.
- **UTG-6:** built the Godot extension from an isolated archive of merge
  `4e08b22` and ran the headless Full combat smoke. Its `FULL_TRACE` was
  **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  2,620 checks and zero failures. The fixed tip produced the same hash, byte
  count and checks. Thus this UI text branch's post-merge changes did not
  change that trace; the wave-23 difference was already present at the merge.
- **UTG-7:** force-added the 3,698-row authored synthetic TSV to the commit;
  its SHA-256 `82853156fa60c7b65ddd0a7a396cf9fb3bd03a10553c327d1b1b1e7a3c412b2e`
  matches the pinned contract. Documented the unresolved track-time offset:
  research prose says `+0x20`, while its spec table says `+0x18`.

Audit-fix verification: arm64 Release configure/build passed. Focused CTest
`-R '^(ui-text-gaps-exact|dialog|dialog_gaps)$'` passed **3/3**, including
the new no-bar/latch assertion. The Godot dev matrix passed **54/54**. Read-only
scripts-on and scripts-off sweeps each tried, loaded and played **111/111**
levels with **230** passed checks, zero failures and empty refusal lists.
Scripts-on retained 14 refusal stops, 266 stubs and zero callee refusals;
scripts-off retained zero of each. The rendered level-110 meter smoke passed
**22** checks, drew no clipped bar and retained one gamefontsmall caption. I
inspected its captured Godot play-scene frame. The original-client pixels
remain unverified. Logs and captures are ignored local files under
`work/native-ui-text/` and `work/private/`.

## Commands and records

`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`;
`cmake --build build/native -j 4`;
`ctest --test-dir build/native --output-on-failure -j 3`;
`ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'`;
`ctest --test-dir build/native --output-on-failure -j 1 -R '^session-facing-exact$'`;
`ctest --test-dir build/native --output-on-failure -j 1 -R '^(ui-text-gaps-exact|dialog|dialog_gaps)$'`;
`python3 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz`
(180-second native subprocess timeout);
`python3 work/native-ui-text/player-long-run.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz`
(stopped as described above; ignored local timeout-only runner);
`work/native-ui-text/build-ext.sh`;
`python3 work/native-ui-text/run-godot.py`;
`work/run-slices-r24.sh uitextr2on --scripts`;
`work/run-slices-r24.sh uitextr2off`;
Godot `real_data_text.gd` (rendered, focused help/bust and meter cases),
`real_data_dialog.gd` (headless), and `playable_wave_smoke_full_combat.gd`
(headless). Counts-only output and captures remain under ignored
`work/private/` and `work/native-ui-text/`.

Audit reruns: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`;
`cmake --build build/native -j 4`;
`ctest --test-dir build/native --output-on-failure -j 3 -R '^(ui-text-gaps-exact|dialog|dialog_gaps|strings)$'`
(the checkout registers the first three names, **3/3**);
`work/native-ui-text/build-ext.sh`;
`python3 work/native-ui-text/run-godot.py`;
`work/run-slices-r24.sh uitextauditon --scripts`;
`work/run-slices-r24.sh uitextauditoff`.
For the merge comparison, `git archive 4e08b22 native gdextension godot` was
extracted under ignored `work/native-ui-text/merge-baseline/`; both baseline
GDExtensions were configured and built for arm64 `template_debug` against the
same pinned godot-cpp, then Godot imported and ran
`res://tests/playable_wave_smoke_full_combat.gd -- --ready-ticks=640` there.
The rendered meter check ran `res://tests/real_data_text.gd` with the
read-only private disc, `--from=110 --to=111 --ticks=600`, and an ignored
capture directory.
