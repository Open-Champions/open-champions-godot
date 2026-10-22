STATUS: PARTIAL — UI text implementation and requested Godot/real-level checks pass; the full arm64 CTest has one unresolved player-exact timeout under machine contention.

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
  glyph and label. A meter-branch level passed 22 checks, with exactly one
  clipped bar and one gamefontsmall caption. I inspected the actual captures
  in the Godot play scene. The original-client pixels were not compared.
- Real-data dialog smoke: 111 levels loaded, 24 talks attempted, 22 opened
  and closed, 669 cursor-glyph quads, zero harness failures. Two talks are
  now refused by wave 24's documented uninitialised camera-offset case;
  the round-1 host had opened them without that evidence.

## Remaining refusals and limits

The compass mesh and scene label remain refused as described above. The
chime's two-update slot life is Inferred and needs the sound cursor the native
runtime refuses. The notice-box half and raster-exact odd-top sprite behavior
remain outside executed evidence. The initial UI fade value, ready-tick proxy
for `g_simFrame`, and bust hide latch are labelled host assumptions; no
original value is claimed. The player-exact arm64 comparison remains
unverified in this round because its wall-clock limits were repeatedly
exceeded under severe machine contention; no assertion mismatch was observed.
Rosetta x64 CTest and exports were outside this round. No private bytes,
names or decoded content were committed.

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
