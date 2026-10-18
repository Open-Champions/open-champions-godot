STATUS: COMPLETE — wave 5 synthetic integration and full exported flow verified; upstream partial refusals retained; local commits only.

Branch: codex/playable-integration. Starting handoff: 41ec697b802b89d7d7a02e88cd2bca8f85d06155.
Worktree: /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration.

| Released branch | Immutable source | Local merge |
|---|---|---|
| codex/level-flow | 1cbd5db | 243fc39 |

Read the released r1 report, LEVEL_FLOW.md and owner brief before integration.
The only merge conflict was the end of native/CMakeLists.txt. Resolution retained
all existing registrations and appended level_flow_targets.cmake. No later level
research branch was merged. The upstream port remains PARTIAL; this round's
completion concerns the requested synthetic integration subset.

Implementation commit: d83e00b. Contract: docs/PLAYABLE_LEVEL_FLOW.md.
Previous waves remain documented in the r1–r4 reports.

`start_play_session(false, true)` and the default-off scene Level fixture checkbox
select rta-level-fixture-v1. The existing zero-argument normal bounded-play path
and one-argument combat fixture remain available. Both fixture flags together
refuse without replacing the live owner. Godot registers level_gate_open,
level_gate_confirm and level_request on the existing controller.

One native fixture Session owns the real level Loader/SessionAdapter, objects
World, wire ready clock, task list, terrain Level, empty Pieces and actual water
Service. Two authored in-memory GOBs contain LMP/WORLD/objects bytes. Native
parsers, real World free-spot/shared RNG and terrain_height perform placement.
The dry empty water tree is queried through the released WaterAdapter; all builds
set CHAMPIONS_LEVEL_FLOW_WATER_ROOT to this integrated worktree. No private retail
inputs are involved. Closed empty resource/service domains are explicit and are
not claimed as completed production engine hooks.

The avatar starts on aa's authored gate footprint. A native Trigger polls every
15 level ticks. Opening the gate pauses; confirmation starts its counter at 1.
The first wire submission has no ready tick. After 179 ready ticks the level is
still aa; the 180th loads aab outside the object/task scheduler, uses its Portal
marker and terrain height, then publishes generation 2. The first ready tick after
placement clears travel. Godot's elapsed-time path snaps interpolation on the
level-generation discontinuity. Pause and render/observation calls do not tick.

Returning to aa stores aab and decodes aa's actual 88-byte OCLF fixture state:
Trigger delay/inside plus serialized Player1/Portal records. The source Trigger's
true inside state is restored after a fresh destination had false state. The
native saved-record filtering/placement route is used; restoration is not a
hardcoded callback that supplies fake restored records. Generation 3 has two saved
levels and one decoded restoration. This is per-level in-memory state, not a
retail save or whole-session disk continuation. Both disk-save methods refuse
before creating files; a valid ordinary continuation can replace the fixture.

Explicit limits remain: no movement/combat commands in this policy, no combined
combat/level owner, arbitrary disc roots, general scripts or whole-game saves.
The separate fixture budget is 4,096 ready ticks. Normal play still uses the
36,000+ tick continuation acceptance. Upstream multiple-WORLD, cutscene recall
and packed moving-transform refusals remain unchanged.

Validation (commands/logs under work/playable-wave5):

- Native Release and universal template_debug/template_release builds, -j 4: pass.
- Native gate test on arm64: pass. x86_64/Rosetta level-flow-exact,
  level-flow-water-adapter and level-play: 3/3 pass, 0.62 s.
- Boundary: 78 selected paths / 39 tests, pass. Full Python tooling: 713 tests,
  385 expected skips, 30.407 s, pass in python-final.log.
- 22 Godot import/headless/default commands pass. After final RNG/interpolation
  glue, combined-level-final passes 1,630 checks at 640 normal-play ticks.
- Full CTest: 166/166 pass, 631.37 s, -j 3, exit 0. player-exact passed
  in 116.14 s; no timeouts or isolated reruns. All skills A/B/C checks pass.
- Final exported acceptance: all 17 commands pass; details below.

Review fixes and evidence qualifications:

- The new glue initially shifted SharedRandom::draw() twice. Review verified that
  the existing generator already returns its upper 16 bits; the glue now passes
  that result directly. Independent fixture facing is 1769 (fourth shared draw
  43769 after the three-draw accepted free-spot sample). Native and scene checks
  lock this boundary. No released arithmetic/RNG implementation changed.
- The initial boundary census still expected 76 paths while two new portable
  files were added. The census and its tests now expect 78; final Python/boundary
  reruns pass. Initial Python failures are retained in python.log.
- A nonfinal graphical long run overlapped a headless combined run using the same
  user://playable-wave-smoke fixture directory. The latter rewrote cosmetic.lmp,
  so the former's later stale-file check saw identical replacement bytes. Its
  single stale-preview failure and the packaging hash mismatch from an intervening
  dylib rebuild are retained in pre-final-export/. Final export acceptance runs
  sequentially after freezing the build. These were not accepted as green evidence.

Preservation: preservation-audit.json compares released level-flow native source,
headers/tests/corpus against 1cbd5db (excluding the deliberately extended target
registration). All compared files are unchanged. All 112 pre-wave-5 vector files
are byte-identical to 41ec697. No new oracle generation or provenance-pin change.

All 11 compared released native files and all 112 historical vector files pass
the preservation audit. Final artifact/capture results follow.

Visual review also caught the inherited pause-menu capability text promising
continuation saves for the level fixture. Commit 9e30a59 corrects it to explicitly
state in-memory saved levels and unavailable disk saves, with a scene assertion.
The prior green export evidence is retained in pre-label-export/; final evidence
is regenerated from the corrected scene. Native binaries are unchanged by this
label fix.


Final exported acceptance (after the visual label fix):

- Actual macOS exported app at 1280×720: 36,000 normal-play ticks, 72,420 checks,
  zero failures. Save/load continuation matches the uninterrupted owner through
  tick 36,257. Retained peak 6,650 records, stable capacity 8,512; continuation
  payload 2,979,244 bytes. Native original-art layout and inventory remain present.
- Same exported flow at 960×720 and 800×600: 1,700 checks each, zero failures.
- The inherited fight still proves three alive actors, HP 84 at tick 12, lethal
  player hit at 13, death queued at 14 and removal at 15; second creature survives.
- The added gate route proves contact/menu freeze, 179-tick nonarrival, arrival
  on tick 180 via advance_time, snapped presentation, next-tick cleanup, and aa's
  actual serialized Trigger restoration. Final level observation: generation 3,
  197 ready ticks, 198 submissions, two saved levels, one 88-byte restoration.
- Exported level headless check: 1,631 checks, zero failures. Existing diagnostic
  flows, component smokes, browser/sandbox/default/runtime and UI checks all pass.
- 134 PNGs from scripted interactions in the actual export. Inspected final
  arrival, restored aa, compact pause/menu and corrected save capability text,
  positioned HUD, inventory grid/modal and combat removal. Native state/assertions
  and owned images were checked together; this was scripted exported-app control,
  not manual mouse play or browser automation.

Capture groups under work/playable-wave5/:

- ui-captures-16x9/, ui-captures-4x3/, ui-captures-compact/: normal play, continuation,
  real component panels, combat and level-flow captures. Gate evidence names are
  level-01-on-gate-aa.png, level-02-gate-menu.png, level-03-travel-179.png,
  level-04-arrived-aab.png, level-05-restored-aa.png, level-06-paused-restored.png.
- captures-16x9/, captures-4x3/: preserved diagnostic exported flow.
- capture-manifest.json lists all final PNG paths, sizes and SHA-256 hashes.
  visual-inspection.md records the examined states and visual limits.

Final artifacts:

- App: build/exports/wave5-final/Open Champions.app
- ZIP: build/exports/wave5-final/OpenChampions.zip
- ZIP SHA-256: d41eb81b4eefebff7eafe5b2665a59e6924f2b9b5cc70f8d818590a19adac30a
- Embedded/local release dylib SHA-256: d87fcfae28cf749b469562c3f2f505a1762857c6df7288934eb1157ebb8f81b1
- App and dylib contain arm64 and x86_64. Embedded release dylib matches the local
  final build byte for byte. Graphical execution was arm64 macOS; x86_64 native
  tests ran under Rosetta, not physical Intel hardware.

Reproduce from this worktree:

```sh
"build/exports/wave5-final/Open Champions.app/Contents/MacOS/Open Champions" \
  --resolution 1280x720 --script res://tests/playable_wave_smoke_level.gd -- \
  --ready-ticks=36000 --capture-dir="$PWD/work/playable-wave5/reproduction"
```

Run combined scene tests sequentially: they deliberately use the same local
synthetic fixture directory. Full command arrays and exits are in build-results,
godot-results, export-results and validation-results JSON; final-summary.json
selects final evidence and python-final.log supersedes the initial Python failure.
No canonical/source-worktree writes, push, PR, issue, agent, scheduler or private
asset access. No Windows, console/emulator, physical Intel, retail-playability,
full original game-frame parity or general level-service completeness claim.
