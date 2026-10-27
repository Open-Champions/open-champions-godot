STATUS: MORE — audited props are committed; player-exact remains unverified after an isolated timeout and the playthrough gate remains red.

# Native Horsemen round 6 — 2026-10-25

Commits: `f814c21` implements the audited props; `257dc86` completes the authored play-flow lifecycle spy's missing dictionaries, whose consumer otherwise emitted script errors while returning exit 0. Base: `codex/native-horsemen` at `7f2f4dd601cd04c4dd86754b3cf6570a9e882794`. Work stayed in this worktree. No push, PR or issue.

Evidence: the PASS audit `champions-re-wt/cursor-runs/audits/research-horsemen-props.md`, scoped to research `t3/research-horsemen-mount` at `338d459c47d5e8e49dc4d1f393111910b2026f05`. Read the committed `docs/spec/horsemen-mount.md` section 2.2 and `tools/ref_horsemen_props.py`; no original executable bytes were imported. The research checkout is currently at `a3daea5`; these fixed mount fields are unchanged from its audited ancestor. The audit was static and disclosed three LOW caveats. Its original-executable difftest was not rerun here.

The factory and opt-in combat pair now share one authored table of the four documented fixed Nightmare lists. Each native combat mount retains the two entries and null terminator at the documented addresses, stores its list at `+0x2c`, sets the MiniBoss bit, and registers the variant's name through the existing object NameTable. Property/name queries and name removal on deletion use the owned World. Duplicate names, case-insensitive lookup, pool reuse and missing keys retain the existing table's behavior. The rider's properties remain independent. The factory now parses its mount list through the same generic property getter as record lists, rather than special-casing a single key address. `ShowName` and `hide` remain absent; constructor HomeRange, visual range and scale are preserved.

MiniBoss activation exposed Creature slot 19 at `0x118418`. `docs/creatures.md` identifies its display-name output; `ref_creature.py` leaves the body external. The World now requires an explicit `Services::display_name` and refuses with that address if missing. The synthetic Full fixture supplies an empty name buffer and records `presentation:Creature.v19`. This is authored presentation, not a port of the original display-name body. The new smoke tests both the refusal and actual fixture call-out, alongside the fourth mounted swing.

Changed: `native/include/champions/horsemen_properties.hpp`, `combat_runtime.hpp`; `native/src/combat_runtime_{horsemen,internal,services,world,own}`; `combat_play.cpp`, `script_commands_objects.cpp`; the Horsemen runtime and script-command test files; the additive `champions-objects` link in `native/combat_runtime_targets.cmake`; `godot/tests/scene_play_flow_component_fixture.gd`; `docs/HORSEMEN_MOUNT.md`; this report.

## Verification

- Initial affected CTest: 4/5 passed; mounted fight reached the newly enabled, unmodelled display-name slot. After adding the explicit service/refusal boundary, `ctest --test-dir build/native -R '^horsemen-runtime-fight$' --output-on-failure` passed 1/1. The fight includes all four fixed lists on seeds 20973 and 7, exact pointer/terminator checks, no extra shared draws, a non-MiniBoss rider control, missing-key defaults, modified/unknown-list diagnostics, duplicate-name deletion, fade deletion, and the mounted combat/lifecycle checks.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 4`: PASS. Final affected CTest (the five named cases below): 5/5 PASS. Full `ctest --test-dir build/native --output-on-failure -j 3`: 231/233 PASS, 3,496.23 seconds, exit 8. `player-exact` hit its 240-second CTest limit; `session-facing-exact` hit its wrapper's 600-second subprocess limit. No assertion mismatch was reported. Prescribed isolated retries ran separately, with unchanged limits: **player-exact timed out again at 240.09 seconds; facing passed 1/1 in 427.58 seconds**. Player library/test flags were verified as `-O3 -DNDEBUG`, with `CMAKE_BUILD_TYPE=Release`. This is not a clean 233/233 aggregate pass.
- Godot: both Release-optimized Universal 2 development bindings passed. `python3 work/native-horsemen/run-r6-godot.py` ran the existing 54-command development matrix, all exit 0. Log inspection found two script errors in the existing play-flow lifecycle spy; after support fix `257dc86`, the isolated `scene_play_flow_smoke.gd` rerun passed 65 checks, zero failures and zero script errors. The Full combat smoke passed 2,620 checks; wave-12 passed 34 checks, zero failures.
- `bash work/run-slices-nh.sh nhr6-on --scripts` and `bash work/run-slices-nh.sh nhr6-off` each passed four slices: 111/111 tried, loaded and played, 230 checks, zero failures and zero scene refusal keys. Scripts on: 332 built, 15,569 unbuilt, 15,905 registered, 15,569 unmodelled load draws and zero pool refusals. The zero scene-refusal count does not mean all script commands are implemented.
- `FULL_TRACE`: unchanged, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. Save/load retained this digest and matching presentation continuations. This default fixture does not spawn the opt-in pair; new prop coverage is in the mounted fight smoke.
- `python3 tools/run_loop_playthrough.py --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder ../../private-validation/rta-disc --out work/native-horsemen/r6-playthrough`: exit 1. Headless and rendered each wrote 19 steps and one diagnostic refusal kind; both retained incomplete enemy-fight, pause-save, load-and-carry and level-change stages. Rendered also refused dialog visual validation. Rendered produced 10 local screenshots, not committed or exported. Disc file count remained 994 before/after. The base lacks the driver and `godot/tests/loop_playthrough.gd`; unmodified copies were recovered from authored commit `ab06ce0` for validation only and are excluded from the implementation commit.

## Remaining refusals

Required gates still red: the isolated player comparison does not finish within 240 seconds, and the two playthrough variants retain the incomplete stages listed above. Facing is resolved by its isolated pass; the initial full-suite result is retained separately.

The original Creature display-name body, unknown or modified non-null combat property lists, live adoption of level-factory pairs into the separate combat World, exact original animation/seat drawing, original picker capacity, hide/show callers, and mounted/stranded rider reachability remain unsupported or Unknown. The Full fixture's statistics, placements, clips, event timing and empty name are authored inputs. The fixed mount list is not applied to an on-foot or mounted rider or substituted for arbitrary level-record properties.

Private disc validation is read-only and counts-only. No private bytes, decoded fields or screenshots are included in tracked outputs.

## Binding and focused-test commands

Both configure/build pairs below passed. The existing Godot 4.5.2 executable and godot-cpp pin `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77` were verified locally; executed tests were on arm64.

```sh
cmake -S gdextension -B build/mac-template_debug -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DCHAMPIONS_LEVEL_FLOW_WATER_ROOT="$PWD" -DGODOTCPP_TARGET=template_debug '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/mac-template_debug --target champions-archive-extension -j 4
cmake -S gdextension/level_texture -B build/level-texture-template_debug -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/level-texture-template_debug --target champions-level-texture-preview -j 4
ctest --test-dir build/native -R '^(horsemen-runtime-fight|horsemen-mount-contract|script-commands|combat-play|fixture-save)$' --output-on-failure
ctest --test-dir build/native -R '^player-exact$' --output-on-failure
ctest --test-dir build/native -R '^session-facing-exact$' --output-on-failure
../integration/work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --script res://tests/scene_play_flow_smoke.gd
```

Ignored local evidence: `work/native-horsemen/r6-gates.json`, `r6-timeout-retries.json`, `r6-full-trace.json`, `r6-godot-log-audit.json`, the corresponding phase logs, counts-only private slice summaries, and the playthrough summary. Temporary driver provenance is in `r6-harness-provenance.json`; the recovered driver, GDScript and generated UID were removed after validation.
