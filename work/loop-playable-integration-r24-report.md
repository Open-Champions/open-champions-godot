STATUS: COMPLETE — Wave 24's 111-level sweeps, native arm64 and Rosetta cases, Godot dev matrix, export matrix, 36,000-tick run and exported real-data smoke have passing executions.

# Playable integration wave 24 — 2026-10-22

## Scope

Starting from wave 23 `c86c539`, only these audited tips were merged, in
order: `codex/combat-ctors` `08dbc72` → `0c59778`, `codex/player-state`
`010eba3` → `274e1d5`, `codex/wading` `94446c4` → `c67feef`, and
`codex/dialog-gaps` `fcfc7ac` → `2e57376`. No other branch was merged.
The combat conflict in `native/tests/combat_free_spot_tests.cpp` retains
both sides' tests. The player-state conflicts retain wave 23's music-volume
and fade carry plus the new player binding in `session_controller.{cpp,hpp}`.
`script_commands_tests.cpp` keeps the fade expectation established by wave
23's researched initial word: two callee refusals instead of three.

## Verified builds and level sweeps

- Native arm64 Release build and CTest pass **231/231** in **672.56 s**.
- Four universal macOS GDExtension configure/build pairs pass. The Godot
  dev matrix passes **54/54**. Dev headless Full passes **2,620 checks**,
  zero failures; `FULL_SAVE_LOAD` matches at 300/300 presents.
- `FULL_TRACE` is **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**,
  **36,402 bytes** in dev headless, exported headless and exported 4:3.
  Wave 23 was `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`
  at 31,622 bytes. The Full fixture now has a Skeleton in slot 5 alongside
  the already-present AntQueen in slot 4. The Skeleton constructor and
  equipped-holder draws shift the shared seed, and each trace line gains
  its actor pair. The AntQueen was already present in wave 23. This is an
  authored fixture consequence, not an original-game placement claim.
- Both four-slice, 60-ready-tick real-data sweeps, `w24on --scripts` and
  `w24off`, try, load and play **111/111** levels, each with **230 passed
  checks**, zero failures and an empty refusal list. Every deterministic
  count change from wave 23 is attributable: `codex/wading` adds
  `water.wading_refused` **0 → 2,832** in both sweeps; `codex/dialog-gaps`
  adds `dialog.camera_steps` **0 → 126** and `dialog.camera_views`
  **0 → 1** in the scripted sweep. `codex/combat-ctors` and
  `codex/player-state` change no deterministic level-sweep count. The
  runtime and displayed-frame variations are tabulated in
  [REAL_DATA.md](../docs/REAL_DATA.md); these concurrent runs cannot
  assign those variations to a branch.

## Export and real-data smoke

- `build/exports/wave24/OpenChampions.zip`: **71,471,892 bytes**, SHA-256
  **`3e4bfda711992d5fafd4501f2e5ac377270dc66b10bd4ecd4240cdc8ed5e8f2a`**;
  8 members, no game-data members or private-path markers. The app and both
  packaged extensions are `x86_64 arm64`, and the packaged extension bytes
  equal the locally built libraries.
- The first local ZIP extraction left the executable at mode `0644` despite
  the ZIP recording `0755`. The ignored export helper now restores archived
  modes; the extracted app is executable. The second export has its own ZIP
  hash recorded above; the app and packaged extension hashes still match.
- Exported 60-tick read-only smoke: scripts off levels 0–11 **12/12**
  tried/loaded/played, 26 checks; scripts on **12/12**, 26 checks; arena
  levels 108–110 with scripts **3/3**, 8 checks. All have zero failures
  and empty refusal lists.

## Export retries and Rosetta disposition

The initial exported matrix has 20/22 exit-zero entries. Its 1280×720
36,000-tick process stopped making progress after writing 11 captures and
was terminated by its exact PID after 339.74 s (exit -15); it has no
completion result. The 800×600 run failed one Skelight-removal assertion
under load, then passed as an isolated rerun: **2,746 checks**, zero
failures, expected `FULL_TRACE`. The 960×720 run passed **2,746 checks**.
The first isolated 36,000-tick rendered rerun reached tick 36,000 but
failed three assertions in the subsequent Full trace, including effect
tick count and headless/drawn and save/load agreement; its trace is
`399aed50402bbc57fa4577b3fc9b3b5476f80989ad73408ba0100247ff9fdb1e`
(36,426 bytes), not accepted as a pass. A separate **headless** exported
36,000-tick run passes **108,700 checks**, zero failures, reaches tick
36,000, and retains the expected 36,402-byte `FULL_TRACE` with matching
300/300 save/load presentation. A third **rendered** 1280×720 run passes
**108,826 checks**, zero failures, reaches tick 36,000, and prints the
expected `FULL_TRACE` with 62 PNG captures. Its deterministic save/load
trace matches; the effect presentation comparison is explicitly skipped
because the uninterrupted and continued runs drew 299 and 300 frames.
The 800×600 isolated rerun and this 1280×720 run close the two initial
matrix failures. I inspected the Full combat captures at 960×720,
800×600 and 1280×720.

The first full x86_64 CTest run under Rosetta began during the extension
build and Godot runs. It completed **228/231** in **3,498.28 s**. Only
three tests failed: `session-facing-exact` (its trace subprocess hit its
600 s timeout), `netplay` (900 s CTest timeout), and `player-exact`
(CTest timeout). A standard serial retry still timed out in
`player-exact` and `session-facing-exact` as machine load climbed above
350; the exact-PID `netplay` child and CTest parent were stopped before
another timeout. A temporary timeout-only CTest retry began, but the Mac
restarted before it produced a result. The generated CTest file was then
restored from its exact saved copy. Under lower load, the **standard**
serial CTest rerun passes all three: `player-exact` **142.88 s**,
`session-facing-exact` **213.32 s**, `netplay` **157.35 s**; **3/3 passed**
in **513.55 s**. Thus each of the 231 x86_64 tests has a passing
execution, while the initial full-run timeout evidence remains recorded.
`file` identifies the relevant executables as Mach-O `x86_64`.

The read-only owner-selected disc was used only for acceptance counts.
No private bytes, names, decoded content or assets were added to tracked
files or the export. Original-client fidelity, physical Intel execution,
Windows/Linux runtime and original-client netplay remain unverified.

Commands and counts-only logs are under ignored `work/playable-wave24/`
and `work/private/`. Main commands: `ctest --test-dir build/native-x64
--output-on-failure -j 3`, `python3 work/playable-wave24/run-builds.py`,
`python3 work/playable-wave24/run-godot.py`,
`work/run-slices-r21.sh w24on --scripts`,
`work/run-slices-r21.sh w24off`,
`python3 work/playable-wave24/export-mac.py`,
`python3 work/playable-wave24/run-export.py`, and
`python3 work/playable-wave24/run-real-export.py`.
