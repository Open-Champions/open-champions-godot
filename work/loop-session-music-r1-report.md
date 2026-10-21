STATUS: COMPLETE — all requested checks passed; private-data and target-rendering limits are stated below.

# Session music and HUD fade, round 1

Branch `codex/session-music`, worktree `~/Projects/open-champions/worktrees/session-music`, based on wave 21 `ed63ccd` (the wave 22 report was absent at setup). Local commits: `67ea43a`, `451a54a`, `3041166`, `fa2052c`; no push or PR.

## Result and evidence

- The play scene now passes its current raw music-volume word into `ChampionsSessionController.enable_level_script` before `preStart`. The script host and native mixer use the same `music_volume.hpp` contract: saved options are copied without clamping; absent options use `boot_music_volume(nullopt)` = `0x3f000000` (0.5). Direct game-folder entry reads an existing saved options image through the native front-end parser before attaching a script. `startMusic` and the boss-music bit of `setCFlag` then use that supplied word through the existing gate. Placeholder-object refusals still take priority.
- `hudFadeScreen` now starts from the executable image's observed zero instead of refusing before the first explicit `hudResetFadeScreen`. Replacement script hosts carry the previous fade word. This follows the bounded `ee_emu.py` research in `t3/research-fade` commit `197f52a` (`docs/fade-lifetime.md`, `tools/difftest_fade_lifetime.py`). Its four setter/step cases passed; nine direct setter call sites were found, none inside `game_loadLevel`, and the probed loader body retained a planted value. This establishes no unconditional *direct* loader reset. Indirect presentation owners were not exhaustively executed, so no per-level 0 or 128 reset is claimed.
- `play_menu_music` now checks the existing volume gate before queueing the first theme, retaining its prior request flags. Source: Codex-written, unaudited `t3/research-menus-layout` `8f528ce`, `docs/frontend-layout.md` and `tools/difftest_frontend_layout.py`.

## Verification

| Check | Exact command / result |
|---|---|
| Fade research | `python3 tools/difftest_fade_lifetime.py "$CHAMPIONS_ELF"` in the research worktree, with `CHAMPIONS_ELF` bound to the owner's read-only RTA US v2.00 executable: PASS. Private bytes were not saved. |
| Native arm64 build | `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4`: PASS; test executable inspected as Mach-O arm64. |
| Focused native | `ctest --test-dir build/native --output-on-failure -R '^(native-audio|script-commands)$' -j 2`: 2/2. Synthetic host tests execute the fade step from zero and execute music with a supplied volume; native audio tests the first theme below and at the exact gate. |
| Full native CTest | `ctest --test-dir build/native --output-on-failure -j 3`: 225/225 on the first full run, 1,040.42 seconds. |
| Godot extensions | `python3 work/session-music/run-builds.py`: four universal builds, all configure/build steps exit 0. |
| Godot dev matrix | `python3 work/session-music/run-godot.py`: final rerun 53/53. Initial run 50/53 because the new worktree lacked the unchanged optional creature-animation binary; a hash-matching wave 21 binary was staged, then the full matrix passed. |
| Saved options smoke | `~/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/session_music_smoke.gd`: 4 checks, zero failures (no file, saved zero, and saved 1.5 copied without clamp). |
| Scripts on | `bash work/session-music/run-slices.sh sm1on --scripts`: tried/loaded/played 111/111/111; 230 checks; zero failures and refusal kinds. |
| Scripts off | `bash work/session-music/run-slices.sh sm1off`: tried/loaded/played 111/111/111; 230 checks; zero failures and refusal kinds. |
| FULL_TRACE | Final Godot combined Full combat: `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes, equal to wave 21. |

### Command counts, scripts on

| Command | Wave 21 executed/refused | This branch executed/refused |
|---|---:|---:|
| `startMusic` | 0 / 0 | 0 / 0 |
| `setCFlag` | 0 / 1 | 0 / 1 |
| `hudFadeScreen` | 0 / 0 | 0 / 0 |

The one `setCFlag` call still refuses because it reads an unconstructed placeholder object, before the music route. The sweep has no `startMusic` or `hudFadeScreen` calls, so it cannot show an executed-count increase for those commands. Synthetic native and Godot checks cover the changed routes. `scripts.refusal_stops` stays 16, `callee_refusals` 0 and `stub_refusals` 265. All deterministic scripts-on and scripts-off counts equal wave 21; only load/play timing, pushed audio frames and peak memory vary with the run.

## Files and boundaries

Changed native script/audio host and tests, GDExtension session/audio bindings, Godot real-data and audio paths, synthetic smoke tests, and `docs/SCRIPT_COMMANDS.md` / `docs/NATIVE_AUDIO.md`. The script host's Playback remains presentation-private; this work establishes script execution and queue decisions, not speaker output or a target-console fade rendering. Research lives only in the allowed `research-fade` worktree. Private validation was read only for the counts-only sweeps and bounded research probe; no game bytes, decoded content or private names were committed. No x86_64, Windows or Linux CTest was requested or run for this branch.
