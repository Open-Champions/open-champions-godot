STATUS: COMPLETE — wave 6 synthetic animation/combat integration, full CTest and final exported flow verified; local commits only.

Local-only implementation in `codex/playable-integration`, canonical worktree
`/Users/syndicatemike/Projects/open-champions/worktrees/playable-integration`.

| Released branch | Immutable source | Local merge |
|---|---|---|
| codex/creature-anim | 5e6748db763ed2d9f99155f6a88ee7a6f75e7408 | 9d656a1 |

Read the complete r4 owner report and `docs/CREATURE_ANIMATION.md` before merge.
The native CMake registrations, simulation target include and ROADMAP EOF
conflicts retained both sides additively. No native audio branch was merged.
Waves 1–5 remain in ancestry; their separate reports preserve their evidence.

## Composition and evidence boundary

`start_play_session(true, false, true)` opts into the scene's animated fixture.
Player idle and two Agony state-six animations use the released dispatch table,
Runtime, poses and skinning. Native frames supply all three rendered meshes.
The manual cosmetic model/clip picker remains independent, and can temporarily
replace the player mesh while simulation is paused. Reset and continuation load
replace all owners coherently. No presentation timer delivers combat events.

The animation table **refuses Skelight dispatch**, while combat currently has
Player and Skelight bodies. The explicit synthetic fixture pairs supported
Agony animation with Skelight's contracted shared hit-arc body as a labelled
combat carrier. It does not invent Agony AI or claim original Skelight
animation. Authored clip/root data are stationary; child poses visibly deform.
The fixture stops at 90 ticks before unknown transitions, refuses movement,
player action-release injection and disk saves. Ordinary play remains the
36,000+ tick continuation/save path. Existing wave-four checks still prove
retaliation, death and next-tick removal with their prior fixture policy.

Typed `hit_test` mask `0x1` emissions enter the combat Session adapter inside
`animation_advance`; World consumes them in that same ready tick. The callback
returns zero to avoid a second delivery path. Exact native checks establish:

- Ticks 1–36: health 100; no hits. No-ready input and pause leave poses unchanged.
- Tick 37: the generated 0.6-second event damages twice, health 100 → 84.
- Tick 38: no duplicate. Tick 67: next generated event, health 84 → 74.
- Tick 90: bounded stop, four damage events, all three actors still present.

Native audio is absent from ancestry. `play_animation::Actors::set_footstep_hook`
is the documented typed ready-event hook for `0x4`/`0x8` footsteps. This
stationary fixture emits none; playback is not claimed. See
`docs/PLAYABLE_ANIMATION.md` for the exact adapter and unsupported boundaries.

## Validation and retained diagnostics

- Release native and both universal Godot extension templates build with `-j4`.
- Python: 714 tests, 385 expected skips, pass. Portable boundary: 80 files.
- Initial scene matrix: all prior commands pass; the new driver initially left
  the cosmetic modal owner open by hiding only its child panel. Its corrected
  existing Hide-picker action passes 1,797 combined headless checks. Initial
  and final logs are retained separately; no runtime fix was needed for this.
- No generated oracle or generator changed. Source audit in
  `work/playable-wave6/source-audit.json` records unchanged upstream animation
  files and every pre-wave-six committed vector file.
- One narrow integration fix makes `tools/test_creature_dispatch.py` import-safe
  with a `main()` guard; exact CLI payload/hash checks remain intact.

## Visual correction

Initial captures exposed a real compact-window overflow: the unwrapped actor
summary forced the layout wider than 800 pixels and clipped the health header.
Wrapping that label fixes it. The exported check now asserts the rendered
layout stays inside the window. It is intentionally a graphical assertion:
Godot's headless dummy viewport is only 64×64. The first width-check attempt
incorrectly applied that assertion to the dummy viewport; its diagnostic log
is retained. Native source and its running full CTest were unchanged.

Initial exports remain under `work/playable-wave6/initial-export/`; current
captures and export metadata are written at `work/playable-wave6/` after the
fix. Inspected compact capture now shows health 84/100 fully within the window,
all three native meshes, and wrapped mode text. The 4:3 manual picker still
shows an independent cosmetic mesh; HUD/art and inventory captures retain the
same explicit synthetic-art labels and native placement as wave 3.

## Capture paths and execution

All interaction was driven by scripted scene UI signals/native calls in the
actual exported macOS app, with graphical viewport captures. No browser or
private assets were used. Capture locations (relative to this worktree):

- `work/playable-wave6/ui-captures-16x9/animation-01-native-poses.png`
- `work/playable-wave6/ui-captures-16x9/animation-02-before-tick37.png`
- `work/playable-wave6/ui-captures-16x9/animation-03-tick37-hit.png`
- `work/playable-wave6/ui-captures-4x3/animation-04-manual-cosmetic-tool.png`
- `work/playable-wave6/ui-captures-compact/animation-03-tick37-hit.png`
- `work/playable-wave6/ui-captures-16x9/animation-05-bounded-complete.png`
- `work/playable-wave6/ui-captures-16x9/ui-01-positioned-hud.png`
- `work/playable-wave6/ui-captures-16x9/ui-02-inventory-grid.png`
- `work/playable-wave6/ui-captures-16x9/12-loaded-continuation.png`
- `work/playable-wave6/ui-captures-16x9/13-continued.png`
- `work/playable-wave6/ui-captures-16x9/level-04-arrived-aab.png`
- `work/playable-wave6/ui-captures-16x9/level-05-restored-aa.png`

Complete entry/library/animation/art/pause/save/load/refusal captures also live
in `captures-16x9/`, `captures-4x3/` and the three `ui-captures-*` directories.
The report does not claim retail visuals, original Agony AI, audio playback,
Windows/Linux runtime execution or Intel hardware testing. Both extension
architectures were compiled locally; graphical execution was Apple Silicon.

Local implementation commits: `9bbd912` (native/table/scene composition and
checks), `d6f2382` (compact label wrapping and graphical width regression).


## Final validation

- Release native configure/build `-j4`: pass. Both Godot extension templates
  built universal arm64/x86_64, using the pinned Godot 4.5.2/godot-cpp setup.
  Water adapter root remains this integrated tree in every configure.
- Full CTest `ctest --test-dir build/native --output-on-failure -j 3`:
  **171/171 pass, 660.23 seconds**. No failures/timeouts; no isolated reruns.
  `player-exact` passed within the aggregate run in 117.28 seconds.
- New `animated-combat-play`: pass. Released creature-animation API/exact and
  dispatch API/exact tests all pass, including the 28,395-case dispatch corpus.
  All skills A/B/C and prior session/combat/level checks remain green.
- Python tooling: **714 tests, 385 expected skips**, pass. Boundary census now
  covers 80 portable files; all 40 boundary tests are included in that pass.
- **23 import/default/headless commands pass**, counting the corrected new
  combined test's final rerun. Final combined headless check: 1,797 assertions.
- **17 final exported commands pass**, including real components, runtime,
  HUD/inventory and the combined animation/level/combat/save journey.
- Final graphical 1280×720: **72,597 checks, 36,000 ready ticks**, no failures;
  6,650 peak retained records, allocation capacities stable after warm-up.
  Its save/load continuation reaches ready tick 36,257 before later fixtures.
- Final 960×720 and 800×600: **1,877 checks each**, 640 ready ticks plus
  continuation, combat, level and animation fixture checks. Compact health
  header is visible; rendered width assertion passes.
- **149 final captures** across the five current capture directories. Initial
  captures are separate and excluded from that count. Actual final export
  images were inspected, including exact-frame health, manual preview,
  HUD/inventory and loaded continuation. The ordinary soak retains its existing
  authored diagnostic-damage policy (including negative HP); it is not an
  original death/gameplay demonstration.
- Source audit: **13/13 released animation files unchanged**, **112/112 prior
  vector files unchanged**. No golden observation or generator was rewritten.

Evidence: `work/playable-wave6/build-results.json`, `validation-results.json`,
`ctest.log`, `python.log`, `godot-results.json`,
`godot-combined-animation-final.log`, `export-results.json`,
`export-artifacts.json`, `source-audit.json`. Corrected test-driver diagnostics
remain in `godot-combined-animation-initial.log`,
`godot-combined-animation-wrapped.log`, and `layout-diagnostic.log`.

Final application: `build/exports/wave6-final/Open Champions.app`.
Final ZIP: `build/exports/wave6-final/OpenChampions.zip` (66,873,690 bytes).
ZIP SHA-256:
`1797f875cb61ac1f6172f2eb95a1b2e5c335cbad503102130fab5b55c43f6037`.
Universal release extension SHA-256:
`9d5351afecda3cb7a105bb8278d4caf290c1ee85e7e50eff6ceb2a95394f2984`.
The dylib inside the app is byte-identical to the locally built release dylib.
No push, PR, issue, private asset search or protected-checkout modification.
