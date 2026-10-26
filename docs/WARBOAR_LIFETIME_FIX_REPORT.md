STATUS: COMPLETE

# WBAIP-1: campaign actor lifetime

Branch: `codex/play-boar-ai`, base `d0027035`. Local commits only; never pushed.
Early implementation commit: `d2b80ada2d2c29b54beb8e0c0bcd2f7a655f1f86`.

## Fix and evidence

The runtime retires dead or replaced actor entries before construction,
adoption, lookup and enumeration. Liveness requires both the live flag and
the identical object pointer in the owning World's address map. Construction
and adoption share checked insertion and return only the stored actor.
Invalid/foreign adoption and duplicate live adoption refuse without replacing
the live actor. Actor references expire when their GameObjects are destroyed;
retirement occurs on the next runtime access, not through a World callback.

Evidence: audit `play-boar-ai-r1.md`, WBAIP-1; research-campaign-combat at
`328b072`, `tools/ref_objects.py:571` (immediate pool return and address-map
removal) and `tools/ref_core.py:2134` (free-list head insertion). Native
`objects_lifetime.cpp` retains object instances while recycling pool addresses,
so pointer identity distinguishes successive occupants. No new game behavior,
constants, assets or retail bytes are introduced.

The same inherited defect exists in `codex/valor-fight` (already merged into
integration); this fix reaches integration through `codex/play-boar-ai`.

## Synthetic regressions

- Destroy/reconstruct at the same pool address; access the replacement's kind,
  health and runtime lookup, dispatch its tick, then verify dead-entry retirement.
- Shared-World destroy/re-adopt, with and without an intervening lookup; verify
  replacement identity, shared health and tick dispatch, reject duplicate live,
  retired and foreign objects, and preserve the existing live actor on refusal.

Changed files: `native/include/champions/campaign_actors.hpp`,
`native/src/campaign_actors.cpp`, `native/tests/campaign_actors_tests.cpp`,
`native/tests/campaign_fight_adopt_tests.cpp`, and this report.

## Gates

Executed from this worktree on 2026-10-25. Ignored evidence is under
`work/boar-ai/`; private inputs were read-only and results below are counts only.

| Exact command / gate | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS; `lifetime-config.log` |
| `cmake --build build/native -j 4` | PASS; `lifetime-build.log` |
| `ctest --test-dir build/native --output-on-failure -R '^(campaign-actors\|campaign-fight-adopt)$'` | PASS, 2/2, 18.05 s; `lifetime-focused.log` |
| `ctest --test-dir build/native --output-on-failure -j 3` | PASS, 241/241, 954.93 s; `player-exact` passes in 116.15 s; `lifetime-ctest.log` |
| `cmake --build build/godot-extension -j 4` | PASS; Release/template_debug extension rebuilt against this fix; `lifetime-godot-build.log` |
| `python3 work/boar-ai/run-godot.py` | 54/54 commands exit 0; 52/54 logs contain no ERROR/SCRIPT ERROR. Two unrelated diagnostic cases are detailed below; `lifetime-matrix.log`, `matrix/godot-results.json` |
| `bash work/boar-ai/run-sweep.sh` | PASS, four slices; 111 tried/loaded/played, 230 checks, 0 failures/refusal kinds, 111 scripts attached, 76 started; `lifetime-sweep.log`, `sweep/summary-*.json` |
| FULL_TRACE in `matrix/godot-combined-full-combat.log` | PASS, unchanged SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, 36,402 bytes; FULL_SAVE_LOAD 300/300 |
| `build/native/champions-campaign-fight-probe --game /Users/syndicatemike/Projects/open-champions/private-validation/rta-disc --level valor --ticks 600 --ai-check` | PASS, 78 boars, 3 wandered/moved, 2 chased/attacked, 35 state changes, 13 attack entries, 14 hit events, 0 failures; `lifetime-approach.log` |
| `git diff --check` | PASS |

The first local fixture build failed because the new adoption fixture omitted
the required rank argument and refusal helper; both were corrected. Its first
focused run then exposed event-recording contamination from the new constructor
fixture; that fixture now isolates its event callback. The final Release build
and both focused and full suites pass; no production changes beyond WBAIP-1
were needed.

## Remaining gate caveats

The Godot matrix is not diagnostic-clean despite all 54 successful exit codes:

- `scene_play_flow`: two SCRIPT ERROR messages at `play_flow.gd:572`, because
  `scene_play_flow_component_fixture.gd` lacks the `loaded` property.
- `ui_layout_smoke`: one ERROR at its line 84, calling `hud_text_glyphs` with
  four arguments while the binding requires five. The script still prints PASS.

Those scripts and binding sources are unchanged from `d0027035`; these unrelated
test/harness defects remain outside this one-finding patch. No claim is made
that the Godot matrix is error-free. No export, cross-architecture trace or new
visual/presentation change was requested or verified. Existing unsupported game
services remain unsupported. The requested native lifetime fix and regressions
are complete; no processes were killed and nothing was pushed.
