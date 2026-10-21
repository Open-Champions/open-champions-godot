STATUS: COMPLETE — Wave 23 sweeps, arm64 and Rosetta CTest, dev matrix, export matrix (36,000-tick run), FULL_TRACE and the real-data export smoke all pass.

# Playable integration wave 23 — 2026-10-21

## Scope and evidence

Wave 23 starts at wave-22 `af9e3b5`. Merge commit `317be8c` contains only the audited `codex/session-music` tip `c703657`; no other branch was merged. The Claude audit in `~/Projects/champions-re-wt/cursor-runs/audits/session-music.md` is PASS with three LOW notes. Its closed-gate real-data error note is fixed in the merge, and the saved-options smoke now tests that route without retail bytes. The fade word remains per controller; indirect presentation owners have not been exhausted by the cited research.

There is exactly one authored music-volume implementation: `native/src/audio/music_volume.cpp`. `native/audio_targets.cmake` builds it once for the standalone audio target, and `gdextension/CMakeLists.txt` builds that same source once for the extension audio target. These are separate link targets, not competing implementations. No second mixer was added. The session copies the options music word bit for bit, or the documented 0.5 default, into the script host before `preStart`; the first menu theme obeys the researched volume gate. `hudFadeScreen` starts at the observed zero and is carried when that controller replaces its script host.

## Read-only real-data sweeps

Both four-slice, 60-ready-tick runs, `w23on --scripts` and `w23off`, tried, loaded, and played **111/111** levels. Each passed **230** checks with zero failures and an empty refusal list. Slices used separate `user://` caches. All deterministic counts match the final wave-22 baseline.

With scripts on, `startMusic` remains **0 executed / 0 refused**, `setCFlag` remains **0 / 1**, and `hudFadeScreen` remains **0 / 0**. No level in the measured sweep calls `startMusic` or `hudFadeScreen`; their successful paths are covered by synthetic tests, not inferred from the sweep. The one `setCFlag` call reaches a placeholder object and refuses before its music route. With scripts off all three command counts are zero. A refused command still stops its script; `scripts.refusal_stops` remains **14**, `scripts.stub_refusals` **266**, and `scripts.callee_refusals` **0**.

The changed counts are run-dependent observations: four-slice `audio.frames_pushed` is **2,691,153 → 1,650,449** with scripts and **2,646,961 → 1,615,409** without; `memory.static_peak_bytes` is **1,531,496,240 → 1,531,512,448** and **1,529,047,064 → 1,529,063,272**; `time.load_ms` is **39,958 → 19,105** and **39,898 → 18,592**; `time.play_ms` is **79,226 → 35,718** and **78,462 → 34,283**. Scripted bust frames drawn are **34 → 27**, which follows display cadence rather than ready ticks. The runs do not isolate a speed change.

## Build and runtime gates

- Four universal macOS GDExtension configure/build pairs passed. The Godot development matrix passed **54/54**; its session-music case passed four checks before the closed-gate addition, and the focused rerun of the final six-check case passed. Dev headless Full passed **2,620 checks**, zero failures, with **FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9` (31,622 bytes)**.
- Native arm64 Release CTest passed **226/226** in **647.83 s**. `file` identifies the audio test executable as arm64.

## Rosetta and export gates

- Native x86_64 Release CTest under Rosetta passed **226/226** in **1,071.97 s** (`work/playable-wave23/ctest-x64.log`).
- Export `build/exports/wave23` (`OpenChampions.zip`, 71,261,087 bytes, sha256 `7251b6bd…ff169`; app binary and both extension dylibs are `x86_64 arm64`). The package audit found **8** zip members, **0** game-data members and **0** private-marked members.
- Export matrix: all 21 smoke/headless entries exit 0. Full headless (640 ticks) and the 4:3 (960x720) and compact (800x600) rendered runs each print **FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes**. The two rendered runs report **2,746 checks**, zero failures.
- **36,000-tick run (1280x720, rendered):** the first matrix attempt reached tick 36,000 (nine progress lines, peak records 6,650) but then did not exit and was killed by the 5,400 s harness timeout (exit 124, no result line; captures incomplete, 47 of 63). That was not counted as a pass. A standalone rerun with the same arguments finished in **157.27 s**: **108,826 checks, zero failures**, peak records 6,650, **FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9` (31,622 bytes)**, 63 captures. The machine load average was about 15 during both. The cause of the first hang is not diagnosed; the rerun did not reproduce it. Recorded in `export-16x9-r2-results.json`; the original failed entry stays in `export-results.json`.
- **Real-data smoke on the exported app** (`--game-folder` to the read-only disc, 60 ticks, separate `user://` caches): scripts off levels 0-11 **12/12 tried, loaded, played**, 26 checks passed, 0 failed, empty refusals; scripts on levels 0-11 **12/12**, 26 passed, 0 failed, empty refusals; arena levels 108-110 with scripts **3/3**, 8 passed, 0 failed, empty refusals.

## Commands and limits

```sh
python3 work/playable-wave23/run-builds.py
python3 work/playable-wave23/run-godot.py
work/run-slices-r21.sh w23on --scripts
work/run-slices-r21.sh w23off
ctest --test-dir build/native --output-on-failure -j 3
python3 work/playable-wave23/run-export.py
python3 work/playable-wave23/run-real-export.py
```

Read-only disc input supplied acceptance counts only. No private bytes, names, decoded content, or level images were added to tracked files or exports. The original-client display and voice synchronization, actual Windows/Linux launch, physical Intel Mac execution (only Rosetta CTest was run), and original-client netplay interoperability remain unverified. The Full fight still excludes the Skeleton state without pinned research. No values were invented to satisfy a test.

Counts-only logs and test output are under ignored `work/private/` and `work/playable-wave23/`; build products are under ignored `build/`.
