STATUS: MORE — the supported Nightmare and mounted-rider contracts pass; four real rider records remain labelled refusals at the research's Unknown mount property lists.

# Native mounted Horsemen, round 2 — 2026-10-22

## Scope and evidence

Continued `codex/native-horsemen` from `12adddc`. Round 1's result was `STATUS: MORE`; its `work/loop-native-horsemen-r1-report.md` was absent. The native port and 1,101 synthetic reference vectors were already committed. Evidence is research `t3/research-horsemen-mount` at `397c35c`, audited PASS after its fix round, especially `docs/spec/horsemen-mount.md` sections 1–7, 9–11 and `tools/ref_horsemen_mount.py`. The existing vector test compares ordered writes, call-outs, draws and results for 1,101 research-model cases; two additional native refusal checks make 1,103. These are authored synthetic fixtures, not console execution.

I extended `native/tests/script_commands_tests.cpp` to exercise all four mounted factory rows together. It checks two shared draws per rider record, one for a placed Nightmare, four labelled Unknown-list refusals, four unbuilt riders, ten registered objects (four rider/mount pairs, one on-foot stub, one Nightmare), and a built placed Nightmare. `tools/mounted_horsemen_real_smoke.py`, left uncommitted by round 1, now resolves the caller's cache and summary paths before invoking Godot and reports a missing summary without a traceback.

## Real-data acceptance (counts only)

The read-only RTA US v2.00 disc was used only for counts. Four 60-tick slices with scripts on and four with scripts off each tried, loaded and played **111/111** levels: **230 passed checks, zero failed checks, empty refusal list**. The scripts-on aggregate has **324 built**, **15,577 unbuilt**, **15,905 registered** objects, **15,573 unmodelled load draws**, and **4** `scripts.build_refused.mount property list contents Unknown`. Against the wave-23 baseline on the same 111 levels, built and unbuilt are unchanged; registered is **+4**, unmodelled draws **−4**, and labelled mount refusals **+4**. Each mounted record still counts as one unbuilt record, but its allocated mount adds a registered stub and its second shared draw is modelled. The synthetic placed-Nightmare host case is built; no claim is made that any mounted pair runs in the play scene. With scripts off, deterministic counts equal wave 23.

The focused `mounted_horsemen_real_smoke.py` run over levels 0–27 passed **28/28**, with exactly **4** labelled mount refusals, **3,751** unbuilt records, and **3,747** unmodelled draws. The first attempt used relative output paths and did not produce the requested summary; after the path fix the fresh run passed. The first scripts-on sweep attempt preceded Godot's project import, could not compile its script classes, and was stopped by exact PIDs owned by this run; the clean imported-project rerun passed 111/111.

## Verification

| Check | Result |
|---|---|
| Focused native `horsemen-mount-contract` and `script-commands` | 2/2 passed after the test edit; direct vector runner reports 1,103/1,103 |
| Universal macOS archive and level-texture GDExtensions from round 1 at this production-code tip; optional creature-animation extension built in round 2 | Passed |
| arm64 Release CTest, `ctest --test-dir build/native --output-on-failure -j 3` | 226/227 passed in 2,343.03 s; only `player-exact` timed out at 240.07 s under concurrent worktree load. The changed `horsemen-mount-contract` and `script-commands` cases passed in this run. Isolated `player-exact` rerun passed 1/1 in 235.35 s. |
| Godot dev matrix, wave-24 54-command list on this branch | 54/54 passed |
| `FULL_TRACE` from Godot combined Full combat | 2,620 checks, zero failures; `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes, equal to wave 23 |
| Scripts-on/off 111-level sweeps, 60 ticks, four slices each | 111/111 each; 230 checks each, zero failures |
| Focused 28-level mounted smoke | 28/28, four labelled refusals |

Commands: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`; `ctest --test-dir build/native --output-on-failure -j 3`; `ctest --test-dir build/native --output-on-failure -R '^player-exact$' -j 1 --timeout 900`; `cmake -S gdextension/creature_anim -B build/creature-anim-template_debug -DGODOT_CPP_DIR=<pinned godot-cpp> -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64\;x86_64`; `cmake --build build/creature-anim-template_debug --config Release --parallel 4`; `python3 work/native-horsemen/run-godot.py`; `bash work/run-slices-nh.sh nhr2-on2 --scripts`; `bash work/run-slices-nh.sh nhr2-off`; `python3 -I tools/mounted_horsemen_real_smoke.py <Godot> <read-only disc> <cache> <summary> 0 28 4`. Round 1's `python3 work/native-horsemen/run-builds.py` has four successful configure/build steps recorded in `work/native-horsemen/build-results.json`.

The mount property-list entries, any MiniBoss flag and per-variant name are **Unknown** in the research, so the four mounted records refuse at their first property read. The rider/mount run-list order, state 0x19/0x1a triggers, rider stun/root reachability, and charge-picker capacity are also Unknown or Inferred and stay refused or host-bounded. The play scene does not tick Horsemen-class pairs; the unchanged Full trace is a regression check, not mounted gameplay evidence. No retail bytes, decoded content, or private names were committed. No push or PR.
