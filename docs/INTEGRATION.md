# Local integration verification

Current playable integration (2026-10-18): wave 10 native integration is verified
locally on `codex/playable-integration`: **175/175 CTests pass**. The latest graphical
export remains wave 9, including its 36,000-tick continuation/audio flow.
See the wave-10 and wave-9 sections below.
Older canonical integration results are historical.

Historical codex/integration refresh (2026-10-14): skills handle-kernel is merged. All 28 eligible
worktree branch tips are included. Release native and Godot extension builds,
37 boundary tests, Python tooling (711 tests, 385 skipped), all ten Godot
headless commands and the full CTest suite **148/148 PASS** (671.71 seconds).
See the final refresh section below; earlier blockers and snapshots are historical.

Branch: `codex/integration`. Run date: 2026-10-13. Local Apple Silicon macOS,
AppleClang 21, Release. No pushes, PRs or source-worktree edits.

## Integrated branch tips

Each branch was merged by name using `git merge --no-ff`, in the order below.
After the first suite, newer save-schema, arena-monsters, natives, skills-b and
skills-c tips were merged individually and the Release build and full suite repeated.
A further audit found newer messages, player, monsters and skills-a tips; these
were then merged individually for another Release build and full-suite pass.

| Branch | Included tip |
|---|---|
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/save-schema` | `7e2637a24966a9dcc8d6cb014a42ea0a203c7e15` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/port-messages` | `c15c4a9de4238743d55291095547e270a4d8e048` |
| `codex/port-player` | `cea3fd20ad840a94591699fff42603a7a1d31c18` |
| `codex/port-monsters` | `fc3db05365eb78ba1537f6573da583dd792978c4` |
| `codex/port-arena-monsters` | `ebc8af12ec9e989684f571529c6834bf4794b047` |
| `codex/port-natives` | `026f17459c64c9e99efe3f10a0d356173b4fbf36` |
| `codex/port-skills-a` | `ff50a1dce857b7d1757231179efa6a06b26f72fa` |
| `codex/port-skills-b` | `8df5e3fb288caabb744d26bca291afdc328298d9` |
| `codex/port-skills-c` | `8d436c3bc20a71b7129cce1557b88a15a2d6252a` |

## Integration resolutions and fixes

Only additive conflicts occurred. Both sets of additions were retained:

- save-schema: native/CMakeLists.txt test registrations.
- port-messages, port-player, port-arena-monsters, port-skills-a:
  native/simulation_targets.cmake target additions.
- port-monsters, port-skills-b, port-skills-c: native/CMakeLists.txt,
  native/simulation_targets.cmake and docs/ROADMAP.md additions.
- port-natives: native/CMakeLists.txt test registrations.

The conflicting repeated base `set(CHAMPIONS_SIMULATION_TARGETS ...)` was
kept once, with native-save retained and every port's include/append preserved.
This prevents later base-list assignments from dropping previously added targets.
No simulation implementation or fixture was changed. No post-merge compile/link
fix has been needed. All nine follow-up merges required no conflict resolution.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build build/native -j 4`: PASS.
- First `ctest --test-dir build/native --output-on-failure -j 3`: 120/122 passed
  in 3134.68 seconds; two internal subprocess timeouts under shared-host load.
- `ctest --test-dir build/native -R '^objects-runtime-transport$' --output-on-failure -j 1`: PASS (22.57 seconds).
- `ctest --test-dir build/native -R '^session-hp-transport$' --output-on-failure -j 1`: PASS (61.75 seconds).
- Release configure/build repeated after five newer tips: PASS.
- Second `ctest --test-dir build/native --output-on-failure -j 3`: PASS,
  **122/122**, 1377.82 seconds, no reruns required.
- Latest Release configure and build after four more tips: PASS.
- Latest `ctest --test-dir build/native --output-on-failure -j 3`: PASS,
  **122/122**, 2431.95 seconds, no reruns required.
- `git diff --check`: PASS.

CTest output: `build/native/integration-ctest.log`,
`integration-objects-rerun.log`, `integration-hp-rerun.log`, and
`integration-ctest-final.log` and `integration-ctest-latest.log` under build/native/ (ignored local output).
Evidence used: committed branch code, documented contracts, authored synthetic
fixtures and local build/test results. No private game input was read by this
integration task. Successful portable tests do not verify Godot exports,
playability, original runtime wiring, Windows or cross-architecture traces.

## Skills consolidation proposal (not implemented)

Shared code appears in native/src/skills_a.cpp, skills_b.cpp and skills_c.cpp:

| Shared behavior | skills-a | skills-b | skills-c |
|---|---|---|---|
| Signed16 first/base, raw32 step, tier fallback/cap | skill_stat / skill_stat_tier2 | stat_impl / get_stat / get_stat_tier2 | tier / execute Stat and Tier2 |
| Player flag and owner handle selection | skill_stat_for / skill_stat_for_handle | get_stat_for / get_stat_for_handle | execute ForCaster and ForHandle |
| Identity level, statistic-4 mana | skill_adjust_level / skill_mana_from_stat4 | adjust_level / mana_from_stat4 | execute Adjust and ManaFromStat4 |
| Four-draw randomized range and rounding/clamp | skill_stat_range | stat_range | execute Range |
| Shared LCG and upper16 result | draw | next / draw | draw |
| Vtable row, signed this adjustment, callback dispatch | skill_call / scalar | skill_call / invoke | invoke / skill_call / scalar |
| Statistic-to-percent passive arithmetic | dodge_fraction | archery_fraction / lifetap_fraction / naturalresistance_fraction | execute Complement / Throwing / BattleMage / PenetratingArrows |

Propose an RTA-only shared Skill kernel under native/, with small adapters for
each port. Extract common arithmetic and tier reads first; retain per-ID mana,
passive behavior, constructors and supplied-code shape recognition in their
respective modules. The newer b/c ports also duplicate spell-base initialization,
spell/object private RNG steps, and paired handle allocation/link/unlink/free.
Compare skills-b `spell_base_ctor`, `next`, `node_take`, `handle_link`,
`handle_remove_node`/`handle_clear` against skills-c `execute(SpellBase)`,
`own_draw`, `handle_take`, `execute(HandleLink)` and `handle_unlink`. The latest skills-a `spell_base` and `spell_rand` also duplicate spell fields and
private RNG. Its base initializer writes only the final vtable, while b/c also
write a temporary base vtable. Spell attachment is an external `handle_link`
callback in a and implemented internally in b; preserve those boundaries.
Their spell
constructor store order differs, b/c write observations differ, and list traversal
refusal policies differ; preserve those adapters until pinned-contract reconciliation
establishes an identical contract. Share the LCG step through an injected ordered write/draw
sink so memory histories, model writes and exception prefixes remain unchanged.

Do not replace the three Models wholesale: skills-b restricts statistic indices
to 0..4 and refuses oversized signed level deltas; skills-a/c allow signed
indices and low32 conversion. skills-b's range writes output pointers immediately
(including unclamped then clamped stores), so aliasing may influence later draws;
skills-a/c return pairs. skills-a also forwards an extra argument on player tier
selection, accepts different virtual arities and handles slot-5 supplied-code
shapes; skills-b uses named scalar callbacks and address aliases, while a/c use
slot-keyed vector callbacks. Preserve these documented boundaries in adapters.

Before any later consolidation, reconcile the pinned Python contracts for each
shared path and run all three existing complete-state corpora unchanged. Compare
return/refusal, callback order/arguments, memory/store history and draw counts,
including signed extremes, NaN rounding, aliasing, handler replacement and
exception prefixes. This proposal adds no Norrath behavior.

## Follow-up integration run (2026-10-13)

Merged the seven pending tips above individually by branch name using
`git merge --no-ff --no-edit codex/<branch>` in the requested order. Messages,
monsters, arena-monsters, natives and all three skills ports advanced; the other
nine branches were already included. All seven merges completed automatically,
without conflicts. No source worktree was checked out or modified.

Release configuration and build passed with no integration fixes required:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build/native -j 4`

Full suite: PASS, **122/122**, 1744.69 seconds, exit 0, no reruns.
Command: `ctest --test-dir build/native --output-on-failure -j 3`, with output
retained in `build/native/integration-ctest-current.log`.
`git diff --check`: PASS.

The updated skills proposal remains deliberately unimplemented. Additional
helper overlap now includes a's `spell_call`/`spell_delete`, b's
`spell_call`/`spell_end`, and c's `destroy`: all dispatch a destructor using a
signed table adjustment and deletion argument 3, but c exposes a dedicated
service callback whereas a/b retain generic virtual dispatch. Share only an
adjustment/argument helper after reconciling callbacks and null handling.
A's named vector callbacks, b's typed vector/scalar callbacks and scratch-backed
spell iteration, and c's numeric service callbacks must remain separate adapters.
A's Undead/secondary shield and c's Retribution/Spirit Shield/Frost Armor share
some supporting spell/RNG/flash/sound machinery; their damage formulas, particle
lifecycles and expiry rules are distinct contracts, not duplicate class bodies.

Evidence for this update: merged authored source and per-port contract notes,
synthetic fixtures, local Release build and test output. No private game input
was read. Cross-platform execution, exported Godot behavior and playability are
outside this integration verification.

## Prior audit: census merge blocked (resolved below)

Source tips advanced during the full suite. A follow-up
`git merge --no-ff --no-edit codex/save-schema` attempted tip
`e07c5abf4db374dfbd6676aa9ee0ddfa12f883a0` and encountered non-additive conflicts:

- `tools/check_simulation_boundary.py`: integration retains animation-runtime
  and asserts 58 files, while save-schema adds four checkpoint/save files and
  replaces the census with 60 and a different error message. Keeping both guards
  would be incorrect; a reconciled census needs a deliberate replacement.
- `tools/test_core_simulation_boundary.py`: two existing census assertions
  conflict as 58 versus 60.

Per the user's conflict rule, `git merge --abort` completed successfully.
The integration branch is restored to the fully tested snapshot above. No
non-additive resolution was chosen, no integration fix was made, and no source
worktree was modified. A human decision on this census resolution is required
before retrying the save-schema merge. Other newly advanced tips remain pending:

| Branch | Pending tip |
|---|---|
| `codex/save-schema` | `e07c5abf4db374dfbd6676aa9ee0ddfa12f883a0` |
| `codex/port-player` | `35ba61907e120961d71c6881f57aaf209a5571ce` |
| `codex/port-arena-monsters` | `412e6d0300cc561027952b607508ba65fd340dc1` |
| `codex/port-natives` | `a03966b332b7590175cf1ce94881b89bfdecee63` |
| `codex/port-skills-a` | `1e1acd21c565cdd48485dff2fbc722e7da1ea41a` |
| `codex/port-skills-b` | `1ecd26e3c60ff380dbc69f282094761d680eac40` |

These pending tips are not covered by the 122/122 passing integration result.
The skills proposal above describes the integrated versions, not the pending tips.

## Authorized census reconciliation and latest integration (2026-10-13)

The user authorized reconciling hard-coded census conflicts. The save-schema
merge retains animation-runtime's two entries and all four save/checkpoint
entries: **62 entries, 62 unique files**. The checker and all three census
assertions now require 62. This is the only integration resolution in this run;
no simulation behavior, target name or symbol fix was needed.

Commands executed successfully before the full suite:

- `python3 tools/check_simulation_boundary.py .`: PASS.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- Direct `simulation_paths` length/uniqueness assertion: PASS, 62/62.
- `python3 tools/test_windows_pack.py`: PASS, 2 tests.
- `python3 tools/test_desktop_package.py`: PASS, 26 tests. Packaging tests use
  mocked subprocesses; their smoke output is not actual exported-app evidence.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build build/native -j 4`: PASS.

Merged save-schema, messages, player, arena-monsters, natives, skills-a,
skills-b, windows-build and ui-frames in the requested order. Skills-c and
monsters advanced during preparation and were subsequently merged individually
with `git merge --no-ff --no-edit`. The initial test launch was stopped early
before the monsters merge; the complete suite was restarted after rebuilding.
All other listed branches were already ancestors. Only integration files were
edited; sibling worktrees were neither checked out nor modified. All commits
remain local.

Snapshot at the final test start:

| Branch | Included tip |
|---|---|
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/save-schema` | `e07c5abf4db374dfbd6676aa9ee0ddfa12f883a0` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/port-messages` | `aaad13aee11254c08bfabc37aac86d694af7bc48` |
| `codex/port-player` | `35ba61907e120961d71c6881f57aaf209a5571ce` |
| `codex/port-monsters` | `cb23b685eb64155cb67dd8410daba31303af7c85` |
| `codex/port-arena-monsters` | `412e6d0300cc561027952b607508ba65fd340dc1` |
| `codex/port-natives` | `a03966b332b7590175cf1ce94881b89bfdecee63` |
| `codex/port-skills-a` | `1e1acd21c565cdd48485dff2fbc722e7da1ea41a` |
| `codex/port-skills-b` | `1ecd26e3c60ff380dbc69f282094761d680eac40` |
| `codex/port-skills-c` | `6839008fc942585c9f5136d5894286d4fb90050a` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |
| `codex/item-upscale` | `103aac54670b7f57669157df2750d1648f310434` |
| `codex/ui-frames` | `d8c61eadae17ba4df3fbd48630b46c92082da920` |
| `codex/fps-scaling` | `103aac54670b7f57669157df2750d1648f310434` |

Verification output is retained in `build/native/integration-build-final.log`
and `build/native/integration-ctest-current.log`. Full suite: PASS, **123/123**, 1275.25 seconds, exit 0, no reruns.
Command: `ctest --test-dir build/native --output-on-failure -j 3`.

The shared-statistics proposal above was checked against the current a/b/c
sources. The common bodies remain in `native/src/skills_a.cpp` (`skill_stat`,
`skill_stat_tier2`, `skill_stat_range`, `draw`, `spell_base`, `spell_rand`),
`native/src/skills_b.cpp` (`stat_impl`, `get_stat_tier2`, `stat_range`, `next`,
`spell_base_ctor`, `spell_rand`) and `native/src/skills_c.cpp` (`tier`, `draw`,
`own_draw`, and `execute` cases Stat/Tier2/Range/SpellBase). Extract an RTA-only
statistic/LCG kernel first, with per-port adapters preserving refusal domains,
callback arity, aliasing, draw/store ordering and exception prefixes. Then
consider common dispatch and spell helpers after contract reconciliation. Keep
per-skill formulas and lifecycles separate. Run all existing complete-state
corpora unchanged before accepting a later refactor. No consolidation was
implemented here.

Evidence: committed authored branch source and documented contracts, synthetic
fixtures and executed local checks. No private game input was read. This run
provides no Windows execution, Godot export inspection, cross-architecture
trace comparison or playable-client evidence.

## Follow-up tips and ui-frames conflict

After the 123/123 pass, branch tips advanced. Save-schema, messages and player
merged automatically. Item-upscale required two additive CMake resolutions:
retain game-profiles and the complete texture source list (`texture.cpp` plus
`texture_upscale.cpp`), and retain all port test registrations plus texture-upscale.
No duplicate targets were introduced. FPS-scaling was already included.

`git merge --no-ff --no-edit codex/ui-frames` at
`75810a1de6e52fb8d170a52e48744fbe6e03b548` conflicted in
`godot/ui/archive_browser.gd`, function `_build_interface`, footer text:

- Integration: "Files stay on your device. Archives are read only. AI cache export saves locally."
- Ui-frames: "Files stay on your device. Game files are read only. Gallery measurements save locally; nothing is uploaded."

Both end with the same asset-preview/playability disclosure. This replacement
is not purely additive and is not a census conflict. Per the user's rule,
`git merge --abort` completed; no footer resolution was chosen. Proposed human
resolution: retain both local-export and local-gallery disclosures in one footer.

Subsequent Release configure/build: PASS, no integration behavior fixes.
`python3 tools/test_texture_upscale_cache.py`: PASS, 6 tests.
Boundary checker and 34 boundary tests: PASS again.
Second full suite: PASS, **124/124**, 721.56 seconds, exit 0, no reruns.
Command: `ctest --test-dir build/native --output-on-failure -j 3`. Previous green log preserved as
`build/native/integration-ctest-123-pass.log`.

Snapshot for this test run (ui-frames is pending, not included):

| Branch | Tip at test start |
|---|---|
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/save-schema` | `f33780e75be39c404adccb394ae9bde1b95d3b87` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/port-messages` | `0ec0b31729c64e941bd2686411656e51668266d6` |
| `codex/port-player` | `06f0ab8ffd66630e5716124e57d24ca1f24b8cba` |
| `codex/port-monsters` | `cb23b685eb64155cb67dd8410daba31303af7c85` |
| `codex/port-arena-monsters` | `412e6d0300cc561027952b607508ba65fd340dc1` |
| `codex/port-natives` | `a03966b332b7590175cf1ce94881b89bfdecee63` |
| `codex/port-skills-a` | `1e1acd21c565cdd48485dff2fbc722e7da1ea41a` |
| `codex/port-skills-b` | `1ecd26e3c60ff380dbc69f282094761d680eac40` |
| `codex/port-skills-c` | `6839008fc942585c9f5136d5894286d4fb90050a` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |
| `codex/item-upscale` | `722741de71e4bb7d4da6cfad18141f77ffd96666` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/fps-scaling` | `103aac54670b7f57669157df2750d1648f310434` |

## Latest advances and fps-scaling decision blocker

The audit after the second green suite found further advances. Save-schema,
messages, monsters, arena-monsters and item-upscale merged individually with
`git merge --no-ff --no-edit`, without conflicts. Their new code requires a
fresh Release build/full suite; the 124-test passing log is preserved as
`build/native/integration-ctest-124-pass.log`.

`codex/fps-scaling` at `5ced92ee39cf439ca56a37985564f8b9971fe072`
was attempted and aborted because its conflicts are not purely additive:

- `gdextension/src/session_controller.hpp`: save-schema owns a `native_save::Scene`;
  fps-scaling retains a `session_control::Controller` while adding clock/cache state.
- `native/src/session_control.cpp`, `Controller::observation`: save-schema uses
  `control_observation(actor_serial, hp)` and its failure state; fps-scaling builds
  observations via `observe_actor(hp)` and separately queries Session failure.
- `gdextension/src/session_controller.cpp` also has additive method-binding
  conflicts; these were not resolved because the merge includes the substantive
  conflicts above.

`git merge --abort` restored integration successfully. The complete conflict
artifact is retained locally as `build/native/integration-fps-conflict.patch`.
A human decision must authorize reconciliation of the save-owning scene and
frame-clock adapter, and identify the observation/failure contract to preserve.
Ui-frames remains separately pending at the footer conflict described above.
No source worktree was changed and no substantive conflict was chosen.

Latest texture cache tests: `python3 tools/test_texture_upscale_cache.py`: PASS,
8 tests, 1 skipped (optional real Godot binding check; `GODOT_FIDELITY_RUNTIME` unset).
Third full suite: PASS, **125/125**, 834.23 seconds, exit 0, no reruns.
Commands: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`,
`cmake --build build/native -j 4`,
`ctest --test-dir build/native --output-on-failure -j 3`.
Current build and CTest logs: `build/native/integration-build-final.log` and
`build/native/integration-ctest-current.log`.

Latest included branch tips at the third test start:

| Branch | Included tip |
|---|---|
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/save-schema` | `bbe23b667b52058fd4a9597f46eeb9412fc8374e` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/port-messages` | `48e0b5afcb26edab92949e38cf46ff8d6e93bb33` |
| `codex/port-player` | `4c12c266565fc945fb8859236b4c8febe9a57168` |
| `codex/port-monsters` | `29c794d99560e2d9fabc97d5e2eff3600570ff6e` |
| `codex/port-arena-monsters` | `45e7901cc81ef35976ee77c060e5eba3760e14cb` |
| `codex/port-natives` | `a03966b332b7590175cf1ce94881b89bfdecee63` |
| `codex/port-skills-a` | `1e1acd21c565cdd48485dff2fbc722e7da1ea41a` |
| `codex/port-skills-b` | `1ecd26e3c60ff380dbc69f282094761d680eac40` |
| `codex/port-skills-c` | `6839008fc942585c9f5136d5894286d4fb90050a` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |

## Closing audit: BLOCKED

The tested combined snapshot is green. Ui-frames and fps-scaling remain
blocked by the non-additive conflicts above; no resolution was implemented.
The final live branch audit found the following tips outside integration. New
advances are not covered by the 125/125 result and remain for a later integration
run after the merge decisions.

| Branch | Pending tip |
|---|---|
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-player` | `4c12c266565fc945fb8859236b4c8febe9a57168` |
| `codex/port-arena-monsters` | `53cfd855d72d5e3c519cfef5fba20840b01950dc` |
| `codex/port-natives` | `503b9ea27ae7446f8401aedddeff39096790a5ef` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |

`git diff --check`: PASS. Source worktrees remain untouched. The skills
consolidation remains a proposal only. No portable-core behavior was authored
or rewritten by this integration run.

## Authorized UI/save/frame-clock integration (2026-10-13)

This run supersedes the two prior merge blockers. The user explicitly authorized
both resolutions. All merges are local `git merge --no-ff --no-edit` operations;
source worktrees are untouched.

- UI footer: retained the local cache-export and gallery-measurement disclosures.
  The gallery entry point and Licenses button share a wrapping footer row under
  the disclosure, inside the existing page scroll container. The texture cache
  controls remain in the preview. Added a real-UI smoke assertion that both footer
  controls are reachable and the gallery button opens its dialog.
- Save/frame-clock ownership: `ChampionsSessionController` owns the native save
  wrapper (`native_save::Scene`, a C++ owner of `session_control::Controller`).
  That native wrapper retains the closed replay/checkpoint mutation surface;
  the GDScript scene only submits requests and displays prepared results. No
  gameplay state, save encoding or replay moves into GDScript.
- Kept the save branch's bounded singleton/serial observation and failed-state
  latch; added fps-scaling's facing observation and address-identity check. Both
  observation APIs remain available. Failed state can still be observed without
  re-running an exhausted history-budget check; failed controllers never retry
  partial simulation. Existing native failure tests remain unchanged.
- Kept fixed-tick accumulation, interpolation and frame-rate settings. Native
  load/terrain publication prepares all potentially failing observation work
  before swapping authority, then resets both position/facing poses and clock
  debt. Scene load clears wall-clock/input debt and displays the restored pose.
  Added bridge regressions for replay and direct-snapshot loads, no smearing,
  no catch-up debt, failure atomicity and the real scene request path.
- No existing ownership assertions were replaced or weakened: both branches'
  existing tests already request operations through the native bridge. The new
  checks cover their combined behavior. No duplicate-target or clashing-symbol
  fix has been necessary so far.

Evidence: the merged authored sources, branch contracts and synthetic fixtures.
No private game input was read. Verification results and final branch census
follow below when complete.

## Dynamic worktree integration (2026-10-13)

This run audits `git worktree list --porcelain` and includes every local `codex/*`
branch whose worktree is under this project's `worktrees/`, except integration
and session-creature. The previous metadata-access blocker is no longer present:
Git status, worktree discovery and merges succeeded normally. The two authorized
UI/frame-clock resolutions above are already committed and were retained.

Seven pending branches were merged individually by name with
`git merge --no-ff --no-edit`: save-schema, port-monsters, port-natives,
port-player, port-skills-a, port-skills-b and port-skills-c. Only save-schema
conflicted: `ChampionsSessionController::observation` now presents the cached
native observation (preserving fixed-tick interpolation behavior) and adds the
native save owner's `save_history_pending` field. This keeps both observations
under the pre-decided ownership resolution. Native gameplay/save ownership,
strict failure latching and atomic load publication remain intact. No test was
changed to encode a different ownership model. No duplicate target or clashing
symbol fix was required during either Release build.

The current skills sources still duplicate statistic/tier arithmetic and
caster/handle selection in `skills_a.cpp` (`skill_stat`, `skill_stat_tier2`,
`skill_stat_for[_handle]`), `skills_b.cpp` (`stat_impl`, `get_stat[_tier2]`,
`get_stat_for[_handle]`) and `skills_c.cpp` (`tier`, `execute` Stat/Tier2/ForCaster/
ForHandle). Mana-from-stat4, randomized range, virtual dispatch, LCG steps and
spell-base initialization also overlap. The detailed proposal above remains
unimplemented: extract an RTA-only native arithmetic/RNG kernel first, retaining
per-port adapters for index/refusal domains, callback arity, aliases, write order,
partial-failure histories and range output aliasing. Reconcile pinned contracts
before sharing handle/spell helpers; keep individual skill behavior separate and
require all three exact-state corpora to remain green.

Verification and the final branch-tip census are recorded below.

Commands completed on Apple Silicon macOS (AppleClang 21, Godot 4.5.2):

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build build/native -j 4`: PASS.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`: PASS.
- `cmake --build build/godot-extension --config Release --parallel 4`: PASS.
- `python3 tools/check_simulation_boundary.py .`: PASS, 62 entries/62 unique files.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: PASS, 675 tests,
  365 skips; mocked packaging output is not real export evidence.
- `work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50`: PASS.
- Same executable with `--headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd`: PASS, 283 checks, zero failures.
- Same arguments with `session_sandbox_smoke.gd` and `--quit-after 180`: PASS,
  119 checks, zero failures.
- Same arguments with `frame_rate_smoke.gd`, `texture_upscale_smoke.gd`, and
  `ui_art_gallery_smoke.gd`, each at 120 frames: PASS; gallery has 45 checks.
- Default scene (`--headless --path godot --quit-after 120`): PASS.

Logs are under ignored `work/integration/current-*.log`; exact Godot commands,
exit statuses and timings are in `current-godot-results.json`. Evidence is the
merged authored source, documented contracts and synthetic fixtures. Private
acceptance was skipped because the private inputs are unavailable; no search
or private-file read was performed. No Windows/x64 execution, cross-architecture
state-trace comparison, new exported-app visual verification or original-game
playability is established by this run.

First full `ctest --test-dir build/native --output-on-failure -j 3`: PASS,
**128/128**, 669.20 seconds, exit 0, no reruns. The closing worktree audit found
one new arena-monsters commit (`bd94ac5373c3d68f8f20aaad8e3bee6e87c1bdb3`). It
was merged by name with `--no-ff --no-edit`, cleanly, before repeating Release
configure/build and the complete CTest run. It changes only the standalone arena
port, synthetic fixtures and its documentation; Godot bridge/presentation and
the already passing headless tests are unchanged.

Second Release configure/build: PASS. Second full
`ctest --test-dir build/native --output-on-failure -j 3`: PASS, **128/128**,
568.25 seconds, exit 0, no reruns. Output: `work/integration/followup-ctest.log`.
The extension was rebuilt and all five real-runtime headless smoke scripts plus
normal default-scene launch passed again, using the commands above. Output:
`followup-godot-*.log` and `followup-godot-results.json`. `git diff --check`: PASS.

Tested native/Godot snapshot: `8aca330d1d93566e32908cf6bdaae7b64b79920f`.
All tested branch tips:

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `bd94ac5373c3d68f8f20aaad8e3bee6e87c1bdb3` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `31c718cc116e8e747b21cc7e01d5c6d49156e606` |
| `codex/port-natives` | `99fcf917e193035ff82d60d960218a742d3d95df` |
| `codex/port-player` | `0980d01dce051bf8ac3c349f023d8fab84162578` |
| `codex/port-skills-a` | `dd225f994e8e38d3bda253ddab5cc23c6f2cc79a` |
| `codex/port-skills-b` | `67ea9c1c0997168795760a049fd79fb2a39dbbc0` |
| `codex/port-skills-c` | `d65e6e9af18fc402373c49233508cd8fe6e89b37` |
| `codex/save-schema` | `8a73c339a28c8a79af78e6f3837fb74c632a8218` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

Closing status: **MORE**. While the second full suite ran, three source branches
advanced again. They are not included in the 128/128 snapshot and are left for
the next integration run; this separates verified integration from newer agent
work. The closing dynamic worktree audit found no other pending tips.

| Branch | New pending tip | Unmerged commits |
|---|---|---|
| `codex/port-arena-monsters` | `054f06076ab7992a44b0c5fc5509f6249bb1045a` | 1 |
| `codex/port-monsters` | `69c3bf92150fa929491f99df5e165942d870d70f` | 1 |
| `codex/port-player` | `87699aad8ab8062b206e45f5cb0b69000a397e4c` | 1 |

Only integration's bridge observation resolution and this verification document
were edited directly; all other changes came from the eight non-fast-forward
merges. No source worktree was modified, no ownership test was weakened, and
no skills consolidation was implemented. Commits remain local; no push, PR or
issue operation was performed.


## Dynamic integration refresh (2026-10-13, snapshot 2cb73ea)

Discovered eligible branches from `git worktree list --porcelain`, restricted to
`codex/*` worktrees under this project's `worktrees/`, excluding integration and
session-creature. Merged arena-monsters, monsters, natives, player and skills-a
one at a time with `git merge --no-ff --no-edit codex/<branch>`. All five merges
were clean. No direct implementation edits, duplicate-target/symbol fixes,
census changes or ownership-test updates were needed. The authorized UI/footer
and native Controller save/frame-clock resolutions above were already included
and remain unchanged. Other agents' worktrees were not modified. No push or
PR/issue operation was performed.

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `54f294a7bcc71596aac2837e4bee079c831b1473` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `34fc826fe5d464252375b15c7c3641b1015b50c2` |
| `codex/port-natives` | `0f84cf13e4acfceb1460f2604267432e193a736d` |
| `codex/port-player` | `3d4f6cd102293a43c247a9facc3c1bb2cf2e7bba` |
| `codex/port-skills-a` | `15781ab3dd0a06002ea25a5c0d9b2f3c144488d9` |
| `codex/port-skills-b` | `67ea9c1c0997168795760a049fd79fb2a39dbbc0` |
| `codex/port-skills-c` | `d65e6e9af18fc402373c49233508cd8fe6e89b37` |
| `codex/save-schema` | `8a73c339a28c8a79af78e6f3837fb74c632a8218` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

Executed checks on Apple Silicon macOS:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build build/native -j 4`: PASS.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`: PASS.
- `cmake --build build/godot-extension --config Release --parallel 4`: PASS.
- `python3 tools/check_simulation_boundary.py .`: PASS.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: PASS, 675 tests,
  365 skips; mocked platform/package checks do not verify exported applications.
- `work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50`: PASS.
- Same executable with `--headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd`: PASS, 283 checks, zero failures.
- Same executable with `--headless --path godot --quit-after 180 --script res://tests/session_sandbox_smoke.gd`: PASS, 119 checks, zero failures.
- Same executable with `--headless --path godot --quit-after 120 --script res://tests/frame_rate_smoke.gd`: PASS.
- Same executable with `--headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd`: PASS.
- Same executable with `--headless --path godot --quit-after 120 --script res://tests/ui_art_gallery_smoke.gd`: PASS, 45 checks.
- Same executable with `--headless --path godot --quit-after 120`: PASS.

First full `ctest --test-dir build/native --output-on-failure -j 3`: PASS,
**128/128**, 647.96 seconds, exit 0, no reruns.
Three branches advanced during that run: natives `570175a`, player `2ba831d`,
and skills-a `e95b5f8`. All three merged cleanly by name with `--no-ff --no-edit`;
no integration fixes were needed. Release rebuild and complete suite repeated
for this newer snapshot; final results follow below.

Logs: ignored `work/integration/run-build.log`, `run-extension.log`,
`run-python.log`, `run-ctest.log`, `run-godot-*.log`; exact Godot commands,
exit statuses and timings are in `run-godot-results.json`.

Skills proposal (not implemented): the current `native/src/skills_a.cpp`
`skill_stat`, `skill_stat_tier2`, `skill_stat_for[_handle]`, `skill_stat_range`,
`draw`, `spell_base` and `spell_rand` overlap with `skills_b.cpp` `stat_impl`,
`get_stat[_tier2]`, `get_stat_for[_handle]`, `stat_range`, `next`,
`spell_base_ctor` and `spell_rand`, and `skills_c.cpp` `tier`, `draw`, `own_draw`
and `execute` Stat/Tier2/ForCaster/ForHandle/Range/SpellBase. Shared mana-from-stat4,
virtual dispatch, percent arithmetic and handle helpers also remain as described
above. Extract a native RTA statistic/LCG kernel first, with explicit adapters
preserving each port's refusal domains, callback arity, signed conversion,
output aliasing, draw/store order and partial-failure observations. Reconcile
pinned contracts before sharing spell/handle helpers; keep skill formulas and
lifecycles separate. Require all three existing exact-state corpora unchanged.

Evidence: merged handwritten portable code, committed contract notes and authored
synthetic fixtures, plus executed local checks. Private acceptance was skipped
because `../../private-validation` is unavailable; no alternative search or
private input read was performed. Windows/x64 runtime, cross-architecture state
traces, exported-app visual/interaction quality and original-game playability
remain unverified by this integration run.


Final refreshed snapshot: `be5ecacf45c42d811a43cd6a9e562a04872358e0`.
Second Release configure/build: PASS. Second complete
`ctest --test-dir build/native --output-on-failure -j 3`: PASS, **128/128**,
622.75 seconds, exit 0, no reruns. Refreshed extension build: PASS; all five
headless smoke scripts and the default launch passed again using the exact
commands above. Logs: `work/integration/refresh-build.log`,
`refresh-extension.log`, `refresh-ctest.log`, `refresh-godot-*.log`, and
`refresh-godot-results.json` (ignored).

An extra `python3 -m unittest discover -s tools -p 'test_player_vectors.py'`
attempt returned exit 5 with zero tests: these vector validators are executable
scripts, not unittest cases. No test pass is claimed from that command; their
proper invocation and native comparison passed in the full CTest run. The
chained skills-a/natives discovery commands consequently did not execute.

Closing dynamic audit: **20 eligible branches, zero pending tips**. All current
tips are ancestors of the tested integration snapshot. No new jobs appeared.

| Branch | Final included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `54f294a7bcc71596aac2837e4bee079c831b1473` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `34fc826fe5d464252375b15c7c3641b1015b50c2` |
| `codex/port-natives` | `570175a50fbfc326b12735d963edfd9e11698008` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `e95b5f865025e06ab4c9c77bdc6d30ded1d4731a` |
| `codex/port-skills-b` | `67ea9c1c0997168795760a049fd79fb2a39dbbc0` |
| `codex/port-skills-c` | `d65e6e9af18fc402373c49233508cd8fe6e89b37` |
| `codex/save-schema` | `8a73c339a28c8a79af78e6f3837fb74c632a8218` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

`git diff --check`: PASS. Closing status: **COMPLETE**. Eight clean local
non-fast-forward merges were performed in this run. The only direct tracked
edit is this integration record; implementation and fixture changes arrived
through the source branch merges. No ownership tests were changed and no
skills consolidation was implemented. The private and platform/visual
verification limits above still apply.


## Dynamic integration refresh (2026-10-13, snapshot a9b92e2)

Discovered eligible branches using `git worktree list --porcelain`: all local
`codex/*` branches with worktrees under this project's `worktrees/`, excluding
integration and session-creature. Nine clean merges were made individually by
name using `git merge --no-ff --no-edit`: natives, skills-a, skills-b, skills-c;
then newer natives, skills-a, skills-c; then newer natives and skills-c.
No source worktree was checked out or modified. No push, PR or issue action
was performed. All commits remain local.

The pre-decided footer and save/frame-clock resolutions were already present
at entry and remain unchanged: gallery and Licenses controls share a wrapping
footer row, the native-backed Controller owns save/load, and load resets both
interpolation poses and clock debt. Both observation surfaces and strict failure
latching remain. No ownership test was replaced or weakened. No conflicts,
census changes, duplicate targets, clashing symbols or other integration fixes
were needed. The only direct tracked edit in this run is this record; authored
implementation, test and fixture changes arrived through the merges in
`native/`, `tools/`, and the per-port contract documentation.

Verification on Apple Silicon macOS:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS on all three snapshots.
- `cmake --build build/native -j 4`: PASS on all three snapshots.
- `ctest --test-dir build/native --output-on-failure -j 3`: PASS on all three snapshots:
  d731ffb, **128/128**, 596.76 seconds; 72f80d4, **128/128**, 622.15 seconds;
  a9b92e2, **128/128**, 642.26 seconds. No failures, timeouts or reruns.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`: PASS.
- `cmake --build build/godot-extension --config Release --parallel 4`: PASS after each merge batch.
- `python3 tools/check_simulation_boundary.py .`: PASS.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: PASS on each snapshot,
  675 tests with 365 skips. Mocked package tests are not exported-app evidence.

The following real Godot commands passed after each extension rebuild (the
final commands used the a9b92e2 snapshot):

```sh
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 180 --script res://tests/session_sandbox_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/frame_rate_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/ui_art_gallery_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120
```

Browser: 283 checks, zero failures. Sandbox: 119 checks, zero failures.
Frame-rate: zero failures. Texture upscale: PASS. UI gallery: 45 checks, PASS.
Editor import and default headless launch exited normally. Logs are under
ignored `work/integration/`: `current-*`, `final-*` and `closing-*`, including
exact Godot commands, exit statuses and timings in the respective
`*-godot-results.json` files. The first two full-suite logs are preserved as
`current-ctest-first-pass.log` and `final-ctest-second-pass.log`; final suite
output is `closing-ctest.log`.

Skills consolidation proposal (not implemented): `native/src/skills_a.cpp`
`skill_stat`, `skill_stat_tier2`, `skill_stat_for[_handle]`, `skill_stat_range`,
`draw`, `spell_base`, `spell_rand` duplicate the shared statistics/RNG bodies in
`skills_b.cpp` `stat_impl`, `get_stat[_tier2]`, `get_stat_for[_handle]`,
`stat_range`, `next`, `spell_base_ctor`, `spell_rand`, and `skills_c.cpp` `tier`,
`draw`, `own_draw` and `execute` Stat/Tier2/ForCaster/ForHandle/Range/SpellBase.
Mana-from-stat4, signed virtual dispatch and percent arithmetic also overlap.
Random-ball helpers overlap in a's `spell_ball`/`obj_ball`, b's
`ball`/`spell_ball`/`object_ball`, and c's `ball`/SpellBall/ObjectBall, but a
caps rejection attempts, b bounds traversal, and c bounds observed writes.

Propose an RTA-only native statistic and LCG kernel first, taking ordered
memory/write/draw adapters. Preserve each port's signed conversion/refusal
domain, callback arity, range-output aliasing, draw/store order and exception
prefix. Reconcile pinned contracts before sharing spell initialization,
virtual dispatch, handle lists or ball sampling; keep differing rejection
budgets and observation sinks explicit. Retain per-skill formulas/lifecycles
and run all three complete-state corpora unchanged before accepting any later
refactor. No Norrath behavior is implied or added.

Evidence: committed authored source, documented per-port contracts, synthetic
fixtures and executed local checks. Private acceptance was skipped because
`../../private-validation` is unavailable; no search elsewhere or private-input
read occurred. Windows/x64 execution, cross-architecture traces, exported-app
visual/interaction quality and original-game playability remain unverified.

Closing audit: **21 eligible branches, zero pending tips**. The new
`codex/ui-font-data` worktree points at the tested integration snapshot and has
no distinct commits to merge. All eligible tips are ancestors of a9b92e2:

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `54f294a7bcc71596aac2837e4bee079c831b1473` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `34fc826fe5d464252375b15c7c3641b1015b50c2` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `b2053ee778c20e8b5bb97b8920a5d0b039b3b51d` |
| `codex/port-skills-b` | `22738e848c5fccf91a790329f70b6d5f435764c4` |
| `codex/port-skills-c` | `765ebd8b90a92e441df6bbceb23d6cf72caa7cea` |
| `codex/save-schema` | `8a73c339a28c8a79af78e6f3837fb74c632a8218` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a9b92e29f26101c9abf37de4c43446c8fccd83b7` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

`git diff --check`: PASS. Closing status: **COMPLETE**.


## Dynamic integration refresh (2026-10-14)

Entry branch: `codex/integration`, clean snapshot `f8e2b8c`. Discovered all
eligible local branches with `git worktree list --porcelain`, restricted to
worktrees under the local worktrees directory,
excluding integration and session-creature. Merged monsters, skills-a,
skills-b and skills-c individually by name with
`git merge --no-ff --no-edit codex/<branch>`. All four merges were clean.
The other eligible tips were already included. No sibling worktree was
checked out or modified. No pushes, PRs or issue changes were made.

The pre-decided footer and ownership resolutions were already integrated:
the UI gallery and Licenses controls remain in a wrapping footer row;
the native-backed Controller owns save/load and the GDScript scene requests
and displays it. Native load publication resets both interpolation poses and
clock debt. Strict failure latching and both observation surfaces remain.
No ownership tests were changed this run. No census conflict, duplicate target,
symbol clash or integration implementation fix was necessary.

Merged implementation/fixture/documentation changes are confined to the
monster and three skills ports in `native/`, their generators/tests in `tools/`,
and `docs/MONSTER_SYSTEMS.md`, `SKILLS_A.md`, `SKILLS_B.md`, `SKILLS_C.md`.
The only direct tracked edit by integration is this record.

Executed commands on Apple Silicon macOS:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build build/native -j 4`: PASS.
- `ctest --test-dir build/native --output-on-failure -j 3`: FAIL (exit 8),
  **125/128 passed**, 2878.56 seconds. Only player-exact, monsters-1-exact and
  skills-c-exact failed, all by timeout; no comparison failure was reported.
- `ctest --test-dir build/native -R '^player-exact$' --output-on-failure -j 1`:
  FAIL (exit 8), timeout, 240.69 seconds.
- `ctest --test-dir build/native -R '^monsters-1-exact$' --output-on-failure -j 1`:
  FAIL (exit 8), timeout, 241.48 seconds.
- `ctest --test-dir build/native -R '^skills-c-exact$' --output-on-failure -j 1`:
  FAIL (exit 8), timeout, 241.38 seconds.
- `python3 tools/check_simulation_boundary.py .`: PASS.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: PASS, 675 tests,
  365 skipped. Mocked packaging output is not actual exported-app evidence.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`: PASS.
- `cmake --build build/godot-extension --config Release --parallel 4`: PASS.

Real Godot commands all exited 0, with expected smoke markers and no failures:

```sh
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 180 --script res://tests/session_sandbox_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/frame_rate_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/ui_art_gallery_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120
```

Browser: 283 checks, zero failures; sandbox: 119 checks, zero failures;
UI gallery: 45 checks, PASS; frame-rate: zero failures; texture upscale: PASS.
The frame-rate tests cover both native save formats, post-load pose/debt reset,
failed-load preservation, Scene request/display behavior and persisted settings.
Logs and exact Godot command/exit/timing records are under ignored
`work/integration/refresh-*`.

Skills consolidation remains unimplemented. The existing proposals above describe
the shared statistics/LCG, dispatch, spell and handle helpers. A fresh proposal
is deferred until the full suite is green, as requested.

Evidence: committed authored code, per-port documented contracts, public
synthetic fixtures and the executed local checks above. Private acceptance was
skipped because `../../private-validation` is unavailable; no private files
were sought elsewhere or read. Windows/x64 runtime, cross-architecture state
trace comparison, graphical/exported-app interactions and original-game
playability remain unverified by this run.

Closing dynamic audit: **21 eligible branches, 2 pending tips**.
The closing audit found new skills-b and ui-font-data commits. Those two
tips below are pending and are not covered by tested merge snapshot `3c57cf8`;
the remaining tips are ancestors. Integration continues with these new tips.
The suite is not green: all three timeout-only failures persisted in isolated
reruns. No simulation comparison failure was observed, but these runs do not
prove those three corpora pass. Other worktrees were testing concurrently;
host contention was observed, but its causal contribution is not established.
No test limit or implementation was changed to turn a timeout into a pass.
A successful native exact-test run remains required. Closing status: **MORE**.
`git diff --check`: PASS.

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `54f294a7bcc71596aac2837e4bee079c831b1473` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `98a3e90c48d95829bcc47214d02d64451a1ae258` |
| `codex/port-skills-b` | `bb94bb2c3765ee8388fcfd10e3f90c70cbd725c0` |
| `codex/port-skills-c` | `6d45df207e12a84dce6a75273ee9cad4ee90684b` |
| `codex/save-schema` | `8a73c339a28c8a79af78e6f3837fb74c632a8218` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `772320dd971a68156a54d0e989a9d840bdb9420e` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |


## Follow-up commits during verification (2026-10-14)

The first closing audit discovered newer skills-b and ui-font-data tips. Both
were merged individually by name with `git merge --no-ff --no-edit`, without
conflicts, after committing the prior verification record. The new snapshot is
`841994b`. Skills-b adds its newer authored lifecycle contract and vectors;
ui-font-data adds bounded font/shape decoders, GDExtension gallery presentation
and synthetic native/Godot tests. Changes arrived through source merges; no
sibling worktree was checked out or modified. No integration fix was necessary.
No push, PR, issue, proprietary-input read or test-limit change was performed.

Repeated verification commands:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build build/native -j 4`: PASS.
- `ctest --test-dir build/native --output-on-failure -j 3`: exit 8,
  128/130 passed, 1393.15 seconds. Two timeout-only failures:
  session-combat-transport's 600-second subprocess limit and skills-c-exact.
- `ctest --test-dir build/native -R '^session-combat-transport$' --output-on-failure -j 1`:
  PASS, 59.66 seconds.
- `ctest --test-dir build/native -R '^skills-c-exact$' --output-on-failure -j 1`:
  PASS, 59.11 seconds.

All 130 cases have passing results for snapshot `841994b` after the required
isolated timeout reruns. Player and monsters passed in the full second run,
resolving their earlier timeouts. No test limits were changed.
- `python3 tools/check_simulation_boundary.py .`: PASS.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: PASS, 675 tests,
  365 skipped, 398.802 seconds.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`: PASS.
- `cmake --build build/godot-extension --config Release --parallel 4`: PASS.

Repeated all seven Godot commands listed in the preceding record. Each exited
0: editor import; browser 283 checks; sandbox 119 checks; frame-rate, upscale
and gallery smoke; default scene headless launch. Gallery additionally executed
`ui_font_shape_cases.gd`: **20 checks, zero failures**, `UI_FONT_SHAPE_PASS`;
its existing 45 gallery checks also passed. Logs, exact command/exit/timing
records and suite output are under ignored `work/integration/newtips-*`.

No consolidation was implemented. The existing skills proposal remains unimplemented; the
next snapshot requires its own verification. Evidence uses the merged authored code and documented
contracts, public synthetic fixtures and executed local checks. Private
acceptance remains skipped because `../../private-validation` is unavailable.
Windows/x64 runtime, cross-architecture traces, graphical/exported-app
interactions and original-game playability remain unverified.

Closing audit discovered six newer tips (arena-monsters, skills-a/b/c,
save-schema and ui-font-data). This passing snapshot does not cover them.
Those merges and their verification are recorded below.


## Latest integrated snapshot and skills-c merge blocker (2026-10-14)

After `841994b` passed all 130 cases including isolated timeout reruns, newer
tips were merged individually with `git merge --no-ff --no-edit`: arena-monsters,
skills-a, skills-b, save-schema, ui-font-data, then further skills-a and skills-b
commits. All were clean except the authorized save/fps observation conflict.
Final tested snapshot: `60753a5`. Across this run, 13 merges completed locally.
No source worktree was checked out or modified; nothing was pushed.

### Authorized save/frame-clock integration resolution

The save-schema merge conflicted in `gdextension/src/session_controller.cpp`,
`advance()`: save-schema built the returned dictionary directly from native
advance and exposed `save_history_pending`, while integration cached the new
state for interpolation and returned `observation()`. Kept `cache_state(...,
true)` followed by `observation()`. The auto-merged `observation()` now adds
`control_->history_pending()`, preserving the new field for both explicit and
fixed-tick advances without dropping the interpolation cache or failure latch.
Native-backed Controller ownership, Scene request/display, both observation
surfaces and load pose/debt reset remain intact. No ownership test was weakened
or changed by integration; source-branch synthetic tests advanced through the
merge. No duplicate targets, clashing symbols or census changes were needed.

### Aborted non-additive skills-c conflict

Attempted `git merge --no-ff --no-edit codex/port-skills-c` at
`1dd21f96e60c30a9aab0bea472de41d77413f000`. Conflict in
`native/CMakeLists.txt` after skills-c-exact registration:

- Integration: `set_tests_properties(skills-c-exact PROPERTIES TIMEOUT 240)`
  plus the texture-upscale test registration.
- Skills-c: a CPU-sharing comment and
  `set_tests_properties(skills-c-exact PROPERTIES TIMEOUT 1800)`.

The texture-upscale registration is additive, but the competing timeout value
is not a census/count-of-entries conflict and is outside the pre-decided footer
and ownership conflicts. Per the user's non-additive conflict rule,
`git merge --abort` succeeded. No timeout choice was made; the latest skills-c
commit remains unmerged and untested here. Human decision needed: retain the
240-second limit or accept the source branch's 1,800-second limit. A pending
text question presents that exact choice; no answer has been inferred.

### Final verification

Apple Silicon macOS, authored public fixtures, Release:

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build build/native -j 4`: PASS.
- After the five follow-up merges, `cmake` Release configure/build and
  `ctest --test-dir build/native --output-on-failure -j 3`: PASS,
  **130/130**, 613.52 seconds, exit 0, no reruns, snapshot `36dd6dc`.
- Final `ctest --test-dir build/native --output-on-failure -j 3`: PASS,
  **130/130**, 592.42 seconds, exit 0, no reruns, snapshot `60753a5`.
- `python3 tools/check_simulation_boundary.py .`: PASS.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: PASS, 675 tests,
  365 skipped, 27.905 seconds.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`: PASS.
- `cmake --build build/godot-extension --config Release --parallel 4`: PASS.

Repeated the seven Godot commands recorded above, each exited 0: bounded editor
import, browser 283 checks, sandbox 124 checks, frame-rate, texture-upscale,
gallery 45 checks plus font/shape 23 checks, and default scene headless launch.
No failure markers. In particular, the real sandbox tests retain deferred save
history through continuation, and the frame-rate tests cover both native save
formats, interpolation/debt reset and strict failed-load preservation.
Full native/Godot/Python logs and command/exit/timing records are under ignored
`work/integration/2026-10-14-final-*`; previous snapshots' outputs are retained
under `refresh-*`, `newtips-*` and `2026-10-14-closing-*`.

### Shared skills helpers (proposal only)

The current integrated a/b/c still duplicate statistics in
`native/src/skills_a.cpp` (`skill_stat`, `skill_stat_tier2`, `skill_stat_for`,
`skill_stat_for_handle`, `skill_stat_range`), `skills_b.cpp` (`stat_impl`,
`get_stat`, `get_stat_tier2`, `get_stat_for`, `get_stat_for_handle`, `stat_range`)
and `skills_c.cpp` (`tier` and execute Stat/Tier2/ForCaster/ForHandle/Range).
`skill_mana_from_stat4`/`mana_from_stat4`/ManaFromStat4 share the level/statistic
conversion. LCG helpers `draw`, a/b `spell_rand`, b `next` and c `own_draw`
also overlap, as do signed virtual dispatch and spell/handle support.

Propose an RTA-only native statistics and LCG kernel with injected ordered
memory/read/write/draw adapters. Preserve signed/refusal domains, b's immediate
output-pointer stores/aliasing, a's extra player-tier callback argument, callback
arities and exception prefixes. Reconcile the pinned contracts before sharing
spell construction, virtual dispatch, handles or ball sampling: store order,
temporary vtables, callback ownership and rejection/traversal/history limits
differ. Keep per-skill IDs, formulas and lifecycles separate and run the three
complete-state corpora unchanged before accepting any later consolidation.
No unification was implemented; this describes the integrated versions,
not the blocked skills-c tip.

Evidence: merged authored sources, documented contracts, public synthetic
fixtures and executed commands. Private acceptance was skipped because
`../../private-validation` is unavailable; no search elsewhere, private bytes
or decoded game content were used. Windows/x64 runtime, cross-architecture
state traces, exported graphical interactions and original-game playability
remain unverified by this run.

Closing dynamic audit: **21 eligible branches; three pending branches**.
Skills-a and skills-b each advanced by one commit during the final full suite;
skills-c has two pending commits, including the aborted timeout conflict.
The newer skills-c tip retains the 1,800-second timeout (verified by reading
its tracked CMake file); its additional commit does not change that file.
The green result applies to the included tips below. Pending tips are not
covered and remain for the next refresh, with skills-c requiring the stated
human decision. The source tree has no active merge or conflict markers.
`git diff --check`: PASS. Closing status: **BLOCKED**.

| Branch | Included tip in tested snapshot | Latest tip at closing audit | Pending commits |
|---|---|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` | `6cb4162d1b28181e006c724431d919432b7564e7` | 0 |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` | `5ced92ee39cf439ca56a37985564f8b9971fe072` | 0 |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` | 0 |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` | 0 |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` | 0 |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` | 0 |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` | 0 |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` | `b02dbb5930d9e95393cf53404702006107a82dbe` | 0 |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` | `4499d5c293573b69897d7bf2efa09ba3477241f9` | 0 |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` | `5df354465fb92eb3ffa42ff200c23a52434151fa` | 0 |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` | 0 |
| `codex/port-skills-a` | `943d75a18869433100426c88b55d3e83e9359c7e` | `f1605917aafa279d7b22793d60065f66995dfd8d` | 1 |
| `codex/port-skills-b` | `f205868fca67206cfd71af5ca64d8ac8004be3f4` | `fedc272f6c47b9827c044b3ca906177e0dad258f` | 1 |
| `codex/port-skills-c` | `6d45df207e12a84dce6a75273ee9cad4ee90684b` | `6719eb13a684daaa8ea9ef375667c7d3173c0edf` | 2 |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` | `df24e2ae5035287c28c3af5902b660efa3e32b68` | 0 |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` | 0 |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` | `f79371ec0486e7cd094650a81698371a6fa82925` | 0 |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` | 0 |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` | `a8eb94538d594fe51833b691796e399a1e7ded1d` | 0 |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` | 0 |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` | `9f36778c0e1374e650f432e298c2673f574cee22` | 0 |


## Dynamic worktree refresh and remaining timeout decision (2026-10-14)

Final tested code snapshot: `e117cd55ef5486b556c02c6fffb2814bbd4f445d`.
Enumerated all local `codex/*` branches from `git worktree list --porcelain`
under the requested worktrees directory, excluding integration and
session-creature. There are 21 eligible branches. Merged skills-a once and
skills-b three times as its tips advanced, each by branch name with
`git merge --no-ff --no-edit codex/port-skills-a` or
`git merge --no-ff --no-edit codex/port-skills-b`. All four merges were clean.
No source worktree was checked out or modified; nothing was pushed.

The pre-decided footer and native Controller save/fps ownership resolutions
were already integrated at the start of this refresh. No ownership test was
changed in this refresh. The executed Godot sandbox/frame-rate tests retain
native-backed ownership, both save observations, strict failed-load preservation,
and post-load interpolation/debt reset. No duplicate targets, symbol fixes,
census edits or other integration behavior fixes were needed.

### Skills-c merge remains aborted

Two attempts, first at `6719eb13` and then at `2110be62`, encountered the same
non-additive `native/CMakeLists.txt` conflict: `skills-c-exact` has TIMEOUT 240
in integration versus TIMEOUT 1800 and a CPU-sharing comment in skills-c.
The nearby texture-upscale registration is additive and must be retained in
either eventual resolution. Both attempts were aborted using `git merge --abort`.
The latest audited tip `1f50ded5` still specifies 1800 (checked with
`git show codex/port-skills-c:native/CMakeLists.txt`). Its four pending commits
are not covered by this passing integration snapshot.

The user's non-additive conflict rule requires a human choice between those
timeouts. The footer/save ownership exceptions and census exception do not
resolve test time limits. A text question requested that choice; no answer was
received or inferred. The source tree has no active merge or unmerged files.

### Executed checks

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`: PASS after each merge batch.
- `cmake --build build/native -j 4`: PASS after each merge batch.
- `ctest --test-dir build/native --output-on-failure -j 3`: PASS **130/130**,
  677.75 seconds at `a55570fc`, 604.52 seconds at `e5614278`, and **591.28 seconds**
  at final code snapshot `e117cd55`; exit 0 throughout, no timeout reruns.
  An early refresh test launch was interrupted after the skills-c retry; the
  clean post-abort tree was reconfigured/rebuilt and that suite restarted in full.
- `python3 tools/check_simulation_boundary.py .`: PASS.
- `python3 tools/test_core_simulation_boundary.py`: PASS, 34 tests.
- `python3 -m unittest discover -s tools -p 'test_*.py'`: PASS, 675 tests,
  365 skipped; repeated after the final skills-b tip.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`: PASS.
- `cmake --build build/godot-extension --config Release --parallel 4`: PASS.

Godot 4.5.2 headless commands, all PASS/exit 0 on the final code snapshot:

```sh
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 180 --script res://tests/session_sandbox_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/frame_rate_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/ui_art_gallery_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120
```

Browser: 283 checks; sandbox: 124 checks; gallery: 45 checks plus 23 font/shape
checks; frame-rate: zero failures; texture upscale: PASS. Editor import and
default launch exited normally. Logs are under ignored `work/integration/`:
`current-*`, `current-refresh-*`, `current-final-*`; Godot command/exit/timing
records are in the corresponding `*-godot-results.json`. Mocked Python package
output is not evidence of an actual export. `git diff --check`: PASS.

### Shared skills proposal, not implemented

Reviewed the integrated source again: statistics are duplicated across
`native/src/skills_a.cpp` (`skill_stat`, `skill_stat_tier2`, `skill_stat_for`,
`skill_stat_for_handle`, `skill_stat_range`), `native/src/skills_b.cpp`
(`stat_impl`, `get_stat`, `get_stat_tier2`, `get_stat_for`, `get_stat_for_handle`,
`stat_range`), and `native/src/skills_c.cpp` (`tier` and execute
Stat/Tier2/ForCaster/ForHandle/Range). Mana conversion, LCG `draw`/`next`/
`own_draw`/`spell_rand`, signed virtual dispatch, spell initialization and
handle helpers also overlap.

Propose an RTA-only native statistic/LCG kernel with ordered read/write/draw
adapters first. Keep b's restricted statistic/delta domain and immediate
output-pointer writes/aliasing, a's extra player-tier callback argument, and
all callback arities and exception prefixes. Reconcile pinned contracts before
sharing virtual dispatch, spell construction or handles: temporary vtables,
store order, callback ownership and traversal/history limits differ. Keep
per-skill IDs, formulas and lifecycles separate. Require all three complete-state
corpora to pass unchanged before accepting a later consolidation. No unification
was implemented; pending skills-c changes are outside this source review.

Evidence: merged handwritten source, documented public contracts, authored
synthetic fixtures, executed Apple Silicon Release/headless tests. Private
acceptance was skipped because `../../private-validation` is unavailable; no
search elsewhere or private input was used. This refresh does not establish
Windows/Intel execution, cross-architecture trace equivalence, exported graphical
interaction, original-game compatibility or playability.

### Closing audited branch tips

| Branch | Audited tip | Pending commits |
|---|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` | 0 |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` | 0 |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` | 0 |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` | 0 |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` | 0 |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` | 0 |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` | 0 |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` | 0 |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` | 0 |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` | 0 |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` | 0 |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` | 0 |
| `codex/port-skills-b` | `592efe35f7fdf90dcbb8fe562927313b70e70214` | 0 |
| `codex/port-skills-c` | `1f50ded596cd8ebb2a3cd7b692e9e3caa2ce59ec` | 4 |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` | 0 |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` | 0 |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` | 0 |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` | 0 |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` | 0 |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` | 0 |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` | 0 |

Post-recording audit: skills-b advanced to `2cfed3c` (passive effects family)
after the final tested snapshot/table, leaving one additional pending commit.
That new tip is unmerged and untested in this refresh. Skills-c still has four
pending commits and needs the timeout decision above. The green result applies
only to `e117cd55`; the next refresh must merge the new skills-b tip and, after
the decision, skills-c, then repeat Release/CTest verification.


## Authorized larger-timeout refresh (2026-10-14)

Enumerated all 21 eligible codex worktree branches. Skills-b merged cleanly.
Skills-c conflicted only over `skills-c-exact` TIMEOUT 240 versus 1800 and
the adjacent additive texture-upscale registration. Per the explicit
2026-10-14 decision, retained the larger **1800 seconds**, its CPU-sharing
comment, and the complete texture-upscale registration. This is the only
integration resolution in this refresh. No skills statistics/RNG unification
was implemented. Footer and native-backed Controller save/load ownership
resolutions were already present; no ownership tests were changed this run.

### Verification and proposal for this refresh

Tested code snapshot: `bb186af`. Native Release configure and build passed:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
```

Boundary checker PASS; boundary tests 34/34 PASS. Python suite PASS,
675 tests, 365 skipped. Native and extension builds PASS. All seven Godot
headless commands listed in the preceding refresh were repeated on this
snapshot: editor import, browser (283 checks), sandbox (124), frame-rate
(zero failures), texture upscale, UI gallery (45 plus 23 font/shape checks),
and default launch all exited 0. Logs: `work/integration/timeout-refresh-*`;
native build/CTest logs: `build/native/integration-*-timeout-refresh.log`.

`ctest --test-dir build/native --output-on-failure -j 3`: PASS 130/130,
639.33 seconds at `bb186af`, exit 0, no failures/timeouts/reruns.

Reviewed the merged a/b/c helper implementations. The existing consolidation
proposal still applies: a `skill_stat`/`skill_stat_tier2`/`skill_stat_range`,
b `stat_impl`/`get_stat_tier2`/`stat_range`, and c `tier` plus execute
Stat/Tier2/Range duplicate statistic/tier/range behavior. Their `draw`,
`next`, `own_draw` and `spell_rand` also duplicate LCG steps. Spell-base
initialization, signed virtual dispatch and handle helpers overlap. A later
dedicated job should extract an RTA-only statistic/RNG kernel with ordered
read/write/draw adapters, preserving each module's refusal domain, aliasing,
callback arity, temporary vtables, store histories and exception prefixes.
Keep per-skill formulas/lifecycles separate, reconcile the pinned contracts,
and require all three complete-state corpora unchanged. **No unification
was implemented.**

Evidence: authored merged source, public contracts, synthetic fixtures and
local Apple Silicon tests. Private acceptance skipped: the designated
`../../private-validation` is unavailable; no other locations were searched.
Windows/Intel execution, cross-architecture trace comparison, graphical export
interaction and playability remain unverified in this run.

### Included worktree tips at suite start

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `c9438bf30b370db17a3b828e48fd5b4460177f15` |
| `codex/port-skills-c` | `1f50ded596cd8ebb2a3cd7b692e9e3caa2ce59ec` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |


### Tips advanced during verification

Closing audit detected skills-b `6c055cb` and skills-c `6624041`. Both merged
cleanly by name with `git merge --no-ff --no-edit`; no integration fixes.
New eligible `codex/session-recovery` worktree currently points to `bb186af`,
already included. There are now 22 eligible worktree branches. Latest code
snapshot `e213334` requires a repeated Release build/full suite. The preceding
639.33-second green result applies to `bb186af`, not this newer code.
The documentation commit `1d49c94` preceded that audit; its title describes the
completed first green run.


### Final verification of refreshed tips

Code snapshot `e213334c978c08380313603061b283ef34de01bb`: Release configure
and `cmake --build build/native -j 4` PASS.
`ctest --test-dir build/native --output-on-failure -j 3`: **130/130 PASS**,
582.91 seconds, exit 0, no failures/timeouts/reruns. All three skills
complete-state tests passed. Python tooling suite repeated: 675 tests,
365 skipped, PASS. Release debug-ABI extension configure/build repeated: PASS.
All seven Godot headless commands above repeated on this snapshot: exit 0
throughout; browser 283 checks, sandbox 124, gallery 45 plus font/shape 23,
frame-rate zero failures, texture-upscale PASS, editor/default normal exits.
Logs: `build/native/integration-{configure,build}-latest.log`,
`build/native/integration-ctest-latest-refresh.log`,
`work/integration/timeout-refresh-python-latest.log`,
`work/integration/timeout-refresh-extension-*-latest.log`, and
`work/integration/latest-refresh-godot-*`.

Final audit: **22 eligible branches, zero pending commits**. No additional
integration fixes or ownership-test changes. `git diff --check` PASS. Skills
statistics/RNG remain separate as merged; the proposal above is unimplemented.
Private acceptance remains skipped; no graphical export, other-CPU/platform
execution, cross-architecture equivalence or playable-game claim is made.
No pushes, PR/issue actions or source-worktree modifications.

| Branch | Closing included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `6c055cb0f9aad0b03805907d3c418845b3bfade3` |
| `codex/port-skills-c` | `6624041f38e1a16575bf7061ebbcb2b6f0e43bc8` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/session-recovery` | `bb186afc0172bd2c046c8239eb4bbd2284bc90ce` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

## Skills-c and Session recovery refresh (2026-10-14)

Started with a clean `codex/integration` at `de831d9`. Enumerated eligible local
branches using `git worktree list --porcelain`, restricting worktrees to
the local worktrees directory and excluding
`codex/integration` and `codex/session-creature`. Of 22 eligible branches,
only skills-c (four commits) and session-recovery (one commit) were pending.
Merged each by name, sequentially:

```sh
git merge --no-ff --no-edit codex/port-skills-c
git merge --no-ff --no-edit codex/session-recovery
```

Both merges completed cleanly. No conflicts, target/symbol fixes, census
resolutions or ownership-test changes were needed. The previously resolved
footer retains gallery and Licenses controls in an HFlowContainer and both
local-export/gallery disclosures. Native-backed Controller save/load ownership,
atomic failure handling and both observations remain merged. Successful load
sets both interpolation endpoints to the restored position/facing and resets
the native clock; the Scene requests operations and presents their results.
The authorized larger skills-c-exact TIMEOUT remains **1800 seconds**.
No statistics/RNG unification was implemented.

### Executed verification

Tested code snapshot `7dac05cbc68055aaacf8cf2b0234b66f4853f880`:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
ctest --test-dir build/native --output-on-failure -j 3
```

All commands PASS/exit 0. Boundary tests: 34/34. Python suite: 675 tests,
365 skipped. Full CTest: **132/132**, 629.07 seconds, no failures, timeouts
or reruns. All three skills complete-state corpora and new recovery API/exact
cases passed. Python packaging mocks do not establish actual export execution.

Godot 4.5.2 headless commands, all PASS/exit 0:

```sh
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 180 --script res://tests/session_sandbox_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/frame_rate_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/ui_art_gallery_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_recovery_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120
```

Browser: 283 checks; sandbox: 124; gallery: 45 plus 23 font/shape checks;
recovery: 40; frame-rate: zero failures; texture upscale: PASS. Editor import
and default launch exited normally. Logs: `work/integration/refresh-*.log`,
`work/integration/refresh-godot-results.json`, and
`build/native/integration-ctest-refresh.log`.

### Shared skills proposal retained, not implemented

Reviewed merged `native/src/skills_a.cpp` (`skill_stat`, `skill_stat_tier2`,
`skill_stat_for`, `skill_stat_for_handle`, `skill_stat_range`),
`native/src/skills_b.cpp` (`stat_impl`, `get_stat`, `get_stat_tier2`,
`get_stat_for`, `get_stat_for_handle`, `stat_range`), and
`native/src/skills_c.cpp` (`tier`, execute Stat/Tier2/ForCaster/ForHandle/Range).
They duplicate signed statistic/tier arithmetic, caster selection, mana and
randomized range helpers. Their shared/private LCG draws, signed virtual
dispatch, spell-base initialization and handle helpers also overlap.

For a later dedicated job, reconcile pinned contracts and extract an RTA-only
statistic/LCG kernel with ordered read/write/draw adapters. Preserve b's restricted
statistic/delta domain and immediate pointer writes/aliasing, a's extra player-tier
argument, callback arities, temporary vtables, store order and exception prefixes.
Consider dispatch/spell/handle helpers only after their differing contracts are
reconciled. Keep each skill's formulas and lifecycles separate; require all three
complete-state corpora unchanged. Each module retains its own statistics/RNG here.

Evidence: committed handwritten code, public documented contracts, authored
synthetic fixtures and executed local Apple Silicon checks. Private acceptance
measurements skipped: `../../private-validation` is unavailable; no alternate
locations were searched. Graphical/export interaction, Intel/Windows execution,
cross-architecture state traces and original-game playability remain unverified.
No pushes, PRs/issues or source-worktree edits.

### Closing audited tips

All 22 eligible tips are ancestors of the tested snapshot, zero pending commits.

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `6c055cb0f9aad0b03805907d3c418845b3bfade3` |
| `codex/port-skills-c` | `b7de20f946a65f5c78d14e07de33c0ded3cb3919` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/session-recovery` | `12f930aebc9df25a3656635ee6f68f064dabb07b` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

## Integration refresh — 2026-10-14 (session-facing and skills-c)

Code snapshot: `1f706feaea37024692695f0d7f7c0483521cfdc5`.
Discovered all eligible local `codex/*` branches using `git worktree list
--porcelain`, restricted to the project worktrees directory, excluding
integration and session-creature. Merged pending `codex/port-skills-c`
(`cc9986e`) and new `codex/session-facing` (`f77633b`) sequentially using
`git merge --no-ff --no-edit`. Both were clean. No integration fix or losing-model
test update was necessary. Source worktrees were neither checked out nor edited.

The existing footer resolution retains the UI art gallery and Licenses controls
in a wrapping HFlowContainer, with both local-export/gallery disclosures.
Native-backed Controller save/load ownership and atomic failure behavior remain
intact. Successful load snaps position/facing interpolation endpoints and resets
the native frame clock. The larger skills-c-exact TIMEOUT remains 1800 seconds.

### Commands and verification

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
ctest --test-dir build/native --output-on-failure -j 3
```

Native and GDExtension Release configure/build: PASS. Boundary checker: PASS;
boundary tests: 34 PASS. Python suite: 675 tests, OK, 365 skipped. Packaging
mocks are not exported-application evidence.

Full CTest: **134/134 PASS**, 701.81 seconds, exit 0; no failures, timeouts
or reruns. All three skills complete-state corpora and both new facing tests
passed. Closing branch audit: 23 eligible branches, zero pending commits.

All nine Godot 4.5.2 headless checks: PASS/exit 0:

```sh
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/frame_rate_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_facing_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/session_recovery_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 180 --script res://tests/session_sandbox_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120 --script res://tests/ui_art_gallery_smoke.gd
work/runtime/Godot.app/Contents/MacOS/Godot --headless --path godot --quit-after 120
```

Browser 283 checks; facing 62; recovery 40; sandbox 124; gallery 45 plus
23 font/shape checks; frame-rate zero failures; texture upscale PASS. Editor
import and default-scene launch exited normally. Logs: `work/integration/current-*`,
`build/native/integration-build-current.log`,
`build/native/integration-ctest-current.log`, and
`build/godot-extension/integration-build-current.log` (ignored local outputs).

### Shared skills proposal (not implemented)

Reviewed the merged sources: `native/src/skills_a.cpp` (`skill_stat`,
`skill_stat_tier2`, `skill_stat_for`, `skill_stat_for_handle`, `skill_stat_range`,
`draw`, `spell_base`, `spell_rand`); `native/src/skills_b.cpp` (`stat_impl`,
`get_stat`, `get_stat_tier2`, `get_stat_for`, `get_stat_for_handle`, `stat_range`,
`next`, `spell_base_ctor`, `spell_rand`); `native/src/skills_c.cpp` (`tier`,
`draw`, `own_draw`, execute Stat/Tier2/ForCaster/ForHandle/Range/SpellBase).
Statistics/tier arithmetic, caster selection, mana, randomized ranges, LCG steps,
virtual dispatch, spell-base and handle helpers overlap.

A later dedicated job could reconcile pinned contracts and extract an RTA-only
statistics/LCG kernel with per-port ordered read/write/draw adapters. Preserve
b's restricted statistic/delta domain and immediate pointer stores/aliasing,
a's extra player-tier argument, callback arities, temporary vtables, store order
and failure prefixes. Reconcile dispatch/spell/handle contracts before sharing
those helpers. Keep skill-specific formulas and lifecycles separate and require
all three complete-state corpora unchanged. No statistics or RNG unification
was implemented in integration.

Evidence: committed authored source, project contracts, synthetic fixtures,
and executed local Apple Silicon checks. Private acceptance measurements skipped:
`../../private-validation` is unavailable; no alternative locations were searched.
Graphical/export interactions, Intel/Windows execution, cross-architecture trace
comparison, original-game playability and unsupported contract paths remain
unverified. No pushes or PR/issue actions.

### Audited included tips

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `6c055cb0f9aad0b03805907d3c418845b3bfade3` |
| `codex/port-skills-c` | `cc9986e435737f08a716ad2c83b202fe710975f1` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/session-facing` | `f77633ba44d7e46e2488aca0fa46919de35c2a06` |
| `codex/session-recovery` | `12f930aebc9df25a3656635ee6f68f064dabb07b` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |


## Integration refresh — 2026-10-14 (skills-kernel scope decision)

Inspected `AGENTS.md`, `CONTRIBUTING.md`, `docs/ROADMAP.md`, the previous
integration resolutions and `docs/GODOT_BROWSER.md`. A clean
`codex/integration` began at `873fcb4`. Enumerated local worktrees with
`git worktree list --porcelain`, selecting every `codex/*` worktree under
the local worktrees directory, excluding integration
and session-creature. There are **24 eligible branches**; all previously
included 23 tips remain ancestors. The sole pending commit is
`codex/skills-kernel` at `359536d9ad7b1cdb6d09306a0c9f3479af618ef2`.

That commit introduces `champions/detail/skill_kernel.hpp` and changes
`skills_a.cpp`, `skills_b.cpp` and `skills_c.cpp` to delegate statistics and
LCG primitives to it. The instruction to merge all worktree branches therefore
conflicts with the explicit instruction to leave each skills module's own
statistics/RNG in place and keep consolidation a proposal. Asked the owner
whether the completed dedicated job should now be merged or excluded. No merge
was attempted while that decision is pending. No implementation, fixture or
source-worktree change was made. No integration fix was needed.

Previously authorized resolutions remain integrated: the archive footer retains
UI art gallery and Licenses controls in a wrapping container; save/load is owned
by the native-backed Controller, successful load resets interpolation and frame
pacing, and stricter failure behavior and observations are retained. The larger
skills-c-exact CTest TIMEOUT remains **1800 seconds**. Their headless regressions
passed again; no losing-ownership test was changed this refresh.

### Commands and evidence

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
ctest --test-dir build/native --output-on-failure -j 3
```

Both Release configure/build pairs PASS. Boundary checker PASS; boundary tests
**34 PASS**. Python tooling: **675 tests, OK, 365 skipped**. Packaging mocks do
not establish exported-application execution.

Repeated all nine Godot headless commands from the preceding refresh, using
`work/runtime/Godot.app/Contents/MacOS/Godot` (4.5.2). Bounded editor import,
archive smoke (283 checks), frame-rate smoke (zero failures), facing smoke
(62 checks), recovery smoke (40), sandbox smoke (124), texture-upscale smoke,
UI art gallery smoke (45 plus 23 font/shape checks), and default-scene launch
all PASS/exit 0. Exact command/exit/timing records are in
`work/integration/godot-refresh-results.json`; outputs are in
`work/integration/godot-refresh-*.log`.

Native and extension build logs: `work/integration/build-refresh.log` and
`work/integration/extension-refresh.log`. Python output:
`work/integration/python-refresh.log`. CTest output:
`work/integration/ctest-refresh.log`. Branch snapshot:
`work/integration/branch-refresh-audit.json`. All are ignored local artifacts.

The existing **unimplemented** skills proposal was rechecked against the actual
sources: a's `skill_stat`/`skill_stat_tier2`/`skill_stat_range`/`draw`, b's
`stat_impl`/`get_stat_tier2`/`stat_range`/`next`, and c's `tier`/`draw`/`own_draw`
and Stat/Tier2/Range operations duplicate arithmetic, cap/fallback and LCG
behavior. Caster/mana dispatch, spell initialization and handle helpers also
overlap. A dedicated consolidation should preserve each adapter's domain,
read/write/draw order, aliasing, callback arity and failure prefixes, with all
three complete-state corpora unchanged. No unification was implemented or merged
in this refresh.

Evidence is committed authored source, documented project contracts and executed
synthetic tests. Private acceptance measurements were skipped because
`../../private-validation` is unavailable; no alternative locations were searched.
No graphical/export inspection, Windows/Intel execution, cross-architecture
trace comparison or original-game playability was established. No pushes,
PRs or issue actions.


Final CTest result for this refresh: **134/134 PASS**, **894.79 seconds**,
exit 0, no failures, timeouts or reruns. Closing branch audit: 24 eligible
branches; only `codex/skills-kernel` has pending work (one commit). The owner
scope question remains unanswered. Status: **BLOCKED** on whether to merge that
dedicated extraction or explicitly exclude its branch. `git diff --check`: PASS.
Only `docs/INTEGRATION.md` changed in this refresh.


## Authorized skills-kernel integration — 2026-10-14

The owner explicitly identified `codex/skills-kernel` as the dedicated job
anticipated by the earlier proposal and authorized merging it. Enumerated local
worktrees, selecting every `codex/*` worktree under the project worktrees root,
except `codex/integration` and `codex/session-creature`. Of 24 eligible branches,
only skills-kernel had pending commits. Ran
`git merge --no-ff --no-edit codex/skills-kernel`: clean merge of
`359536d9ad7b1cdb6d09306a0c9f3479af618ef2`, resulting snapshot `e5e6490`.
No conflicts, integration fixes, timeout changes or ownership-test changes were
needed. Source worktrees were neither checked out nor modified. All commits
remain local; no push or PR/issue actions.

The prior footer, native-backed Controller save/load ownership, interpolation
reset and stricter failure/observation resolutions remain integrated. All their
headless regressions pass. The larger skills-c-exact timeout remains 1800 seconds.

### Skills proposal after the dedicated extraction

The historical proposal above described the pre-kernel source. Statistics
arithmetic, signed cap/fallback, low32 delta and LCG step/result now delegate
from all three ports to `native/include/champions/detail/skill_kernel.hpp`.
See [the kernel reconciliation](SKILLS_KERNEL.md) for the pinned contracts and
retained adapter differences. Integration authored no unification itself.

Remaining overlap is in a's `skill_stat_range`, `skill_call`, `spell_base` and
`spell_rand`; b's `stat_range`, `skill_call`, `spell_base_ctor`, `spell_rand` and
handle helpers; and c's `skill_call`, `own_draw` and Range/SpellBase/handle
operations in `execute`. Propose a later dedicated job to reconcile common
range numeric operations and signed vtable adjustment helpers first, preserving
B's immediate pointer stores/aliasing versus A/C pair returns, callback arities,
rounding predicates, refusal domains and partial failure histories. Reconcile
spell constructor store order and handle/service ownership before sharing those
helpers. Preserve separate per-skill formulas and lifecycles. Run the existing
three complete-state corpora unchanged. This remaining proposal is not implemented.

### Executed checks

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
ctest --test-dir build/native --output-on-failure -j 3
```

Release builds PASS. The boundary checker PASS with its merged 67-entry census;
35 boundary tests PASS. Python tooling: 676 tests, OK, 365 skipped; package mocks
are not exported-app evidence. All nine Godot headless checks PASS/exit 0:
bounded editor import; archive (283 checks); frame-rate (zero failures); facing
(62); recovery (40); sandbox (124); texture-upscale; UI art gallery (45 plus
23 font/shape checks); default-scene launch. Exact commands, exits and timings
are retained in `work/integration/godot-kernel-results.json`; individual outputs
are `godot-kernel-*.log` in that directory. This includes the headless commands
in `docs/GODOT_BROWSER.md` and the integrated presentation regression scripts.

Build/Python/CTest logs are `work/integration/{configure,build,python,ctest}-kernel.log`
and `extension-configure-kernel.log`/`extension-kernel.log`. Worktree branch audit
is `work/integration/branch-kernel-audit.json`. These are ignored local artifacts.

Evidence: committed authored source, documented contracts, synthetic fixtures
and executed local Apple Silicon checks. Private acceptance measurements skipped:
`../../private-validation` is unavailable; no alternative locations were searched.
No graphical/export inspection, Windows/Intel execution, cross-architecture
trace comparison or original-game playability is established by this refresh.

Final full CTest: **136/136 PASS**, **619.04 seconds**, exit 0, no failures,
timeouts or reruns. Closing audit found a new `codex/session-anim-cache` worktree
at the tested snapshot `e5e6490`, already included: **25 eligible branch tips,
zero pending commits**. `git diff --check` PASS. Status: **COMPLETE**.

### Closing included-tip audit

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `6c055cb0f9aad0b03805907d3c418845b3bfade3` |
| `codex/port-skills-c` | `cc9986e435737f08a716ad2c83b202fe710975f1` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/session-anim-cache` | `e5e64903472ab3a63361328d0eb02ae056a05287` |
| `codex/session-facing` | `f77633ba44d7e46e2488aca0fa46919de35c2a06` |
| `codex/session-recovery` | `12f930aebc9df25a3656635ee6f68f064dabb07b` |
| `codex/skills-kernel` | `359536d9ad7b1cdb6d09306a0c9f3479af618ef2` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |


## Session animation-cache integration refresh — 2026-10-14

Enumerated every local `codex/*` worktree under the project worktrees root,
excluding `codex/integration` and `codex/session-creature`. Of 25 eligible
branches, only `codex/session-anim-cache` had pending work. Merged its
`e1e29ba` tip with `git merge --no-ff --no-edit codex/session-anim-cache`;
clean merge, resulting implementation snapshot `510f2af`. All 25 branch tips
are included in the closing audit. No source worktrees were modified.
No integration fixes, conflicts, timeout changes or test ownership changes
were required. Prior footer controls, Controller save ownership, interpolation
reset, stricter failure handling and observations remain integrated and their
Godot regressions pass. The larger skills-c timeout remains 1800 seconds.

### Executed validation

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
ctest --test-dir build/native --output-on-failure -j 3
```

Both Release builds PASS. The boundary checker PASS with the actual 68-entry
census; its 36 tests PASS. Python tooling: 707 tests, OK, 385 skipped.
Full CTest: **141/141 PASS**, **665.95 seconds**, exit 0; no failures, timeouts
or reruns. All five new Session animation-cache checks pass.

All nine Godot headless commands PASS/exit 0: bounded editor import;
archive (283 checks); frame-rate; facing (62); recovery (40); sandbox (124);
texture-upscale; UI gallery (45 plus 23 font/shape checks); default launch.
This includes the headless checks in `docs/GODOT_BROWSER.md`. Exact commands,
exit statuses and timings are in `work/integration/godot-cache-results.json`;
outputs in `godot-cache-*.log`. Native, extension, Python and CTest logs are
`work/integration/{configure,build,extension-configure,extension,python,ctest}-cache.log`.
The closing branch audit is `work/integration/branch-cache-audit.json`.
These are ignored local evidence artifacts.

### Remaining skills proposal, not implemented

The dedicated skills-kernel branch is already merged: all three ports delegate
statistics arithmetic and RNG step/result to
`native/include/champions/detail/skill_kernel.hpp`. Remaining overlap is A's
`skill_stat_range`, `skill_call`, `spell_base`, `spell_rand`; B's `stat_range`,
`skill_call`, `spell_base_ctor`, `spell_rand` and handle helpers; and C's
`skill_call`, `own_draw`, Range/SpellBase/handle operations in `execute`.
A later dedicated job should reconcile range arithmetic and signed virtual-table
adjustment first, then spell construction and handle adapters. Preserve B's
immediate pointer stores/aliasing versus A/C pair returns, callback arities,
rounding predicates, service ownership, store order, refusal domains and partial
failure histories. Validate against all three unchanged complete-state corpora.
Integration implemented no additional statistics/RNG or helper unification.

Evidence: merged authored source, documented contracts and executed synthetic
checks on this Apple Silicon host. Private acceptance measurements skipped:
`../../private-validation` is unavailable and no alternative locations were
searched. Graphical/export inspection, Windows/Intel execution,
cross-architecture trace comparison, original-game playability and Norrath
compatibility remain unverified. All commits stay local; no push or PR actions.

### Closing included-tip audit

| Branch | Included tip |
|---|---|
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `6c055cb0f9aad0b03805907d3c418845b3bfade3` |
| `codex/port-skills-c` | `cc9986e435737f08a716ad2c83b202fe710975f1` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/session-anim-cache` | `e1e29ba0f975e126aeeb3348c0f394e0553adeb1` |
| `codex/session-facing` | `f77633ba44d7e46e2488aca0fa46919de35c2a06` |
| `codex/session-recovery` | `12f930aebc9df25a3656635ee6f68f064dabb07b` |
| `codex/skills-kernel` | `359536d9ad7b1cdb6d09306a0c9f3479af618ef2` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

## Controller animation-cache and range-kernel refresh — 2026-10-14

Enumerated all local `codex/*` worktrees under the project worktrees root,
excluding `codex/integration` and `codex/session-creature`. Two of 27 eligible
branches had new commits. Merged separately by name:

- `git merge --no-ff --no-edit codex/control-anim-cache`: tip `a45b444`,
  merge `2155e63`.
- `git merge --no-ff --no-edit codex/skills-range-kernel`: tip `b6ebce3`,
  merge `4a5b8f5`.

Both merges were clean. No integration fixes, census conflict resolutions,
timeout changes or ownership-test changes were required. Source worktrees were
not modified. The pre-decided footer controls, native Controller save/load
ownership, interpolation reset and stricter failure observations remain included;
the archive, sandbox and frame-rate regression smokes pass. The prior larger
skills-c CTest timeout remains unchanged. Integration authored no skills
statistics/RNG unification; both dedicated kernel jobs are now included.

### Executed validation

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
ctest --test-dir build/native --output-on-failure -j 3
```

Both Release builds PASS. Boundary checker and all 37 boundary tests PASS;
direct census assertion confirms 71 entries and 71 unique files. Python tooling:
711 tests, OK, 385 skipped. Mocked packaging tests do not establish actual exports.
Full CTest: **146/146 PASS**, **772.31 seconds**, exit 0; no failures,
timeouts or reruns. `git diff --check`: PASS.

All ten Godot headless commands PASS, exit 0: bounded editor import; archive
(283 checks); frame-rate; facing (62); recovery (40); sandbox (124);
texture-upscale; UI gallery (45 plus 23 font/shape checks); control animation
cache (30); default launch. These include the checks in `docs/GODOT_BROWSER.md`.
Exact commands, statuses and elapsed times are retained in
`work/integration/godot-control-range-results.json`, with logs named
`godot-control-range-*.log`. Native configure/build, extension configure/build,
Python and CTest output are `work/integration/*-control-range.log`.

### Remaining skills overlap and proposal, not implemented

All three ports now use `native/include/champions/detail/skill_kernel.hpp` for
statistic/tier arithmetic, LCG step/result, range scale/combine and signed
virtual-this adjustment. The dedicated range job preserves local draw acquisition,
rounding predicates, pointer publication and callback routing. These remaining
adapters are intentional: B stores unclamped/clamped outputs immediately and
permits observable aliasing, while A/C return pairs; NaN predicates and refusal
bounds also differ.

Further helper overlap remains in A's `spell_base`/`spell_rand` and spell dispatch,
B's `spell_base_ctor`/`spell_rand`, `node_take`, `handle_link`, `handle_clear`,
and C's `execute(SpellBase)`, `own_draw`, `handle_take`, `handle_unlink` and
service dispatch. Propose a later dedicated reconciliation of spell field
initialization, ordered RNG publication and paired handle allocation/unlink.
Extract only proven common primitives with adapter-owned write/callback sinks;
preserve temporary-vtable store differences, draw/store ordering, callback arity,
service ownership, traversal limits and partial-failure histories. Keep per-skill
formulas and lifecycles separate. Require all three unchanged complete-state
corpora before accepting any further extraction. No additional unification was
implemented by integration.

Evidence: merged authored source and its pinned documented-contract notes,
synthetic fixtures and executed checks on Apple Silicon macOS. Private acceptance
measurements skipped because `../../private-validation` is unavailable; no other
locations were searched. Graphical/export inspection, Windows/Intel execution,
cross-architecture traces, original-game playability and Norrath compatibility
remain unverified. No push, PR or issue actions.

### Closing included-tip audit

A new worktree branch appeared during validation; its tip was already included.
The closing audit contains **28 eligible branches, zero pending commits**.
Snapshot retained as `work/integration/branch-control-range-audit.json`.

| Branch | Included tip |
|---|---|
| `codex/control-anim-cache` | `a45b444a0a327cdcd0537bc372868b242cfba04e` |
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `6c055cb0f9aad0b03805907d3c418845b3bfade3` |
| `codex/port-skills-c` | `cc9986e435737f08a716ad2c83b202fe710975f1` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/session-anim-cache` | `e1e29ba0f975e126aeeb3348c0f394e0553adeb1` |
| `codex/session-facing` | `f77633ba44d7e46e2488aca0fa46919de35c2a06` |
| `codex/session-recovery` | `12f930aebc9df25a3656635ee6f68f064dabb07b` |
| `codex/skills-handle-kernel` | `4a5b8f528c76d09b14d014fdb95dedf30be7044d` |
| `codex/skills-kernel` | `359536d9ad7b1cdb6d09306a0c9f3479af618ef2` |
| `codex/skills-range-kernel` | `b6ebce303dbbe0d0e726d002719549ce0ac0dafc` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

## Skills handle-kernel integration refresh — 2026-10-14

Enumerated local `codex/*` branches with worktrees under the project worktrees
root, excluding integration and session-creature. Of 28 eligible branches,
only skills-handle-kernel had new commits. Merged its tip `2cb35e3` by name:
`git merge --no-ff --no-edit codex/skills-handle-kernel`, merge `c9a2fe1`.
The merge was clean. No integration fixes, timeout/count resolutions or test
ownership changes were needed. Source worktrees were not modified. Earlier
footer, native Controller save/load, interpolation reset and failure-observation
resolutions remain merged. Dedicated skills-kernel/range/handle work is included;
integration authored no additional unification.

### Executed validation

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
ctest --test-dir build/native --output-on-failure -j 3
```

Both Release builds PASS. Boundary checker and 37 boundary tests PASS. Python
tooling: 711 tests, OK, 385 skipped. All ten Godot headless commands PASS, exit 0:
bounded editor import; archive (283 checks); frame-rate (zero failures); facing
(62); recovery (40); sandbox (124); texture-upscale; UI gallery (45 plus 23
font/shape); Controller animation cache (30); default launch. These include all
headless commands in docs/GODOT_BROWSER.md. Exact commands, exit statuses and
timings are in `work/integration/godot-handle-results.json`; outputs are
`godot-handle-*.log`. Native/extension configure/build and Python outputs are
`work/integration/*-handle.log`.

Full CTest: **148/148 PASS**, **671.71 seconds**, exit 0. No failures,
timeouts or reruns. New skills-handle-kernel adapter/exact checks and all
three existing skills corpora pass. Log: `work/integration/ctest-handle.log`.
`git diff --check`: PASS.

### Remaining skills overlap and proposal, not implemented

All three skills ports delegate statistics/tier arithmetic, LCG step/result,
range scale/combine and signed virtual-this adjustment to
`native/include/champions/detail/skill_kernel.hpp`. B/C additionally delegate
free-list take/recycle and paired-link orchestration to
`native/include/champions/detail/skill_handle_kernel.hpp`. Their remaining
insertion/unlink/iteration adapters deliberately preserve different read orders,
aliasing, validation and failure prefixes; A retains external handle callbacks.

Remaining overlap includes A `spell_base`/`spell_rand`, B `spell_base_ctor`/
`spell_rand`/`next`, C `execute(SpellBase)`/`own_draw`, and each port's spell
virtual/service dispatch. A future dedicated job could extract only proven
common spell-field and RNG-publication primitives with injected ordered
read/write/callback sinks. Preserve A's final-vtable-only initialization versus
B/C temporary-vtable writes, all three initialization orders, checked versus
wide addressing, Model history versus direct Memory writes, callback arities,
service ownership and partial failures. Keep per-skill formulas/lifecycles and
handle insertion/unlink policies separate unless pinned evidence proves common
behavior. Require all three unchanged complete-state corpora and explicit alias
and failure-prefix checks. No proposal was implemented by integration.

Evidence: merged authored source, docs/SKILLS_HANDLE_KERNEL.md and per-port
pinned contract notes, synthetic fixtures and executed local checks on Apple
Silicon macOS. Private acceptance measurements skipped because
../../private-validation is unavailable; no alternative locations searched.
Graphical/export inspection, Windows/Intel execution, cross-architecture trace
comparison, original-game playability and Norrath compatibility remain unverified
by this run. All commits remain local; no push, PR or issue actions.

### Closing included-tip audit

All **28 eligible branches have zero pending commits** at the closing audit.
Snapshot: `work/integration/branch-handle-audit.json`.

| Branch | Included tip |
|---|---|
| `codex/control-anim-cache` | `a45b444a0a327cdcd0537bc372868b242cfba04e` |
| `codex/creature-water` | `6cb4162d1b28181e006c724431d919432b7564e7` |
| `codex/fps-scaling` | `5ced92ee39cf439ca56a37985564f8b9971fe072` |
| `codex/game-profiles` | `8ebfd90368ded171ccfc6623561d14735bb1cf23` |
| `codex/godot-health` | `ccd38b1cbfbebf9a967938e880fbd77b5d3144eb` |
| `codex/item-upscale` | `39bc3128f6c6d1185467898d6194c69c5f3370e9` |
| `codex/model-coverage` | `7c9de3cdc85068f4b7196e8f5077f071f532aeb5` |
| `codex/port-arena-monsters` | `de680bf1373d8e1f9ce1a28c0345183292d74d75` |
| `codex/port-messages` | `b02dbb5930d9e95393cf53404702006107a82dbe` |
| `codex/port-monsters` | `4499d5c293573b69897d7bf2efa09ba3477241f9` |
| `codex/port-natives` | `5df354465fb92eb3ffa42ff200c23a52434151fa` |
| `codex/port-player` | `2ba831d0560e6cfc0ea359befa671a78a61998d0` |
| `codex/port-skills-a` | `f1605917aafa279d7b22793d60065f66995dfd8d` |
| `codex/port-skills-b` | `6c055cb0f9aad0b03805907d3c418845b3bfade3` |
| `codex/port-skills-c` | `cc9986e435737f08a716ad2c83b202fe710975f1` |
| `codex/save-schema` | `df24e2ae5035287c28c3af5902b660efa3e32b68` |
| `codex/animation-runtime` | `f20a87d767e5c45e7354efc321fc31ae66fc2fc3` |
| `codex/session-anim-cache` | `e1e29ba0f975e126aeeb3348c0f394e0553adeb1` |
| `codex/session-facing` | `f77633ba44d7e46e2488aca0fa46919de35c2a06` |
| `codex/session-recovery` | `12f930aebc9df25a3656635ee6f68f064dabb07b` |
| `codex/skills-handle-kernel` | `2cb35e361f21b150c1c410b68053469990a1a98e` |
| `codex/skills-kernel` | `359536d9ad7b1cdb6d09306a0c9f3479af618ef2` |
| `codex/skills-range-kernel` | `b6ebce303dbbe0d0e726d002719549ce0ac0dafc` |
| `codex/texture-palettes` | `f79371ec0486e7cd094650a81698371a6fa82925` |
| `codex/trace-diagnostics` | `d7bb6c7aed04f303393ebac50d4874bd123219c5` |
| `codex/ui-font-data` | `a8eb94538d594fe51833b691796e399a1e7ded1d` |
| `codex/ui-frames` | `75810a1de6e52fb8d170a52e48744fbe6e03b548` |
| `codex/windows-build` | `9f36778c0e1374e650f432e298c2673f574cee22` |

## Playable-scene wave 1 — 2026-10-17

**STATUS: MORE — wave 2 pending.** This is an isolated local handoff on
`codex/playable-integration`, starting at `60def02`; the canonical integration
checkout was not changed. The coordinator explicitly deferred
`session-play-runtime` and released these seven scopes.

| Source branch | Completed tip | Local merge |
|---|---|---|
| scene-level-library | 95a6dda | 2341a79 |
| scene-animation-preview | 2606de2 | 4fd8c43 |
| scene-original-art | 1af101e | 719cf45 |
| scene-play-flow | 2f0a072 | 47dce83 |
| skills-spell-kernel | 3ef9b9a | be912f4 |
| skills-a-ctor | c094b96 | 20de046 |
| creature-water-2 | 23699af | 8e199a6 |

Seven serial `--no-ff --no-edit` merges were clean. Skills-A automatically kept
both the constructor supplement and shared spell primitives. All 108 historical
vector/manifest files match the base byte for byte; only the separately owned
spell-kernel corpus is added.

Combined acceptance uses the real production library, animation and art modules
with the locally compiled native extension. Integration fixes cover water census
registration, explicit absent-component test setup, queued cosmetic playback
from a paused picker, opaque/readable animation UI, bounded art-dialog layout,
and save-selector width after hidden-menu reload. No original numeric bodies,
save meanings, runtime retention policy or gameplay constants changed.

Release native and universal debug/release extension builds pass. Boundary
checker covers 73 paths; 38 boundary tests pass. Python: 712 tests, 385 skipped,
OK. All 15 Godot import/headless/default commands pass. Actual release export
passes legacy browser/sandbox routes, changed component suites and combined
68-check headless flow. Graphical combined flow passes 102 checks each at
1280x720 and 960x720, with 32 actual viewport captures and visual inspection.
Full CTest `-j 3`: **151/152 passed**, 1167.79 seconds, exit 8; only
`player-exact` reached its unchanged 240-second limit. The isolated `-j 1 -R
'^player-exact$'` rerun **passed in 196.75 seconds**, exit 0. All 152 named
cases have passing evidence across these two runs; the full run itself remains
151/152. All 15 merged Skills-A/B/C/kernel tests passed, including the new
constructor and shared spell-kernel checks. Logs: `work/playable-wave/ctest.log`
and `ctest-alone-player-exact.log`.

The additional 800x600 exported run also passed 102 checks. Final captures total
48 across 800x600, 960x720 and 1280x720. Source integration commit:
`cb1052bd5bc09a3160e963a330d73825c5b04743`. Documentation handoff is a separate
local commit; no source changed after this verified commit.

See [the wave report](PLAYABLE_WAVE_2026_10_17.md) for exact commands, source pins,
all fixes, observed limits, artifact hashes and reproduction. Local report:
`work/loop-playable-integration-r1-report.md`. Logs/JSON:
`work/playable-wave/`; capture folders `captures-16x9/` and `captures-4x3/`.
Export: `build/exports/wave1-final/Open Champions.app`; executable
`Contents/MacOS/Open Champions` starts normal entry without arguments.

Expected limitation: the native diagnostic still stops at **512 ready ticks**.
No >512/36,000-tick runtime claim is made before wave 2. Legacy native saves
restore exact collision/state continuation with a collision-ground display;
they do not restore the chosen WORLD render mesh association. Indexed snapshots
can retain deferred historical validation. Supplied-grid water remains outside
Session, and synthetic animation/art do not establish retail pairing or HUD
fidelity. Private inputs, Windows, physical Intel, Norrath and retail-level
completion remain unverified. Reference-function coverage unchanged; client
integration only. All commits remain local; no push, PR or issue actions.

## 2026-10-18: playable integration wave 2 — COMPLETE (local)

Worktree/branch: `worktrees/playable-integration`, `codex/playable-integration`.
Clean wave-1 handoff `7a23f0e` now includes the released session-play-runtime
`a01d260dc30203d4456fbe5dee28d3c60c0374e9` via local no-ff merge `d26d562`.
Its r4 report and explicit World checkpoint scope extension were read first.
Boundary census conflicts were resolved as the true additive union: 74 paths.
All 108 historical corpus files are unchanged; all 16 runtime native/bridge
source/header/target files match the runtime tip exactly. No algorithm rewrite.

Scene integration `1a51907324579c3d034de633dca2e2777d7caeff` connects the existing
Start play action to the released capability and continuation format. The visible
selector reads Continuation (current state); Save uses `.ocplay`, Load accepts
both native save extensions, and native validation owns format dispatch.
Unsupported diagnostic switches refresh Play availability. Explicit diagnostic
sessions retain their original limit and legacy save contracts.

Fresh verification in this worktree:

- Release native and universal debug/release extension builds passed (`-j 4`).
- Full CTest **156/156 passed**, `-j 3`, **741.58 s**, exit 0. `player-exact`
  passed in the full run in 142.08 s. All 15 skills/kernel cases passed, including
  A/B/C and the merged constructor/spell kernel; all four runtime cases passed.
- Boundary checker: 74 paths and 38 passing unit tests. Python discovery:
  712 tests, 385 skips, pass. Existing and new Godot suites pass; initial new
  test-expectation corrections and final rerun are recorded in the detailed report.
- Actual release-export combined flow: **36,000 ticks**, **72,099 checks**, zero
  failures; continuation then matched 257 uninterrupted future observations
  through **36,257 ticks**. Retention peaked at **6,650 records**, capacity stayed
  at **8,512 slots** after warm-up, and the continuation was **2,979,244 bytes**.
- Exported 4:3 and 800×600 play flows each passed 1,379 checks; both 16:9 and 4:3
  diagnostic flows passed 102 checks. All 13 exported commands passed. **89**
  actual viewport captures were saved, with representative states visually inspected.

The validation helper briefly started an unnecessary isolated player rerun from
wave 1's stale failure-list file after the fully successful suite. That redundant
run was stopped, and the helper now checks the full-run exit before consulting
the list. This does not alter the fresh 156/156 full-suite result.

App: `build/exports/wave2-final/Open Champions.app` in the playable worktree.
Detailed reproducible commands, source pins, hashes, capture paths and merge table:
[`PLAYABLE_WAVE_2026_10_17.md`](PLAYABLE_WAVE_2026_10_17.md).
Local round report: `work/loop-playable-integration-r2-report.md` (STATUS: COMPLETE).
Fresh logs/captures: `work/playable-wave2/`.

This is synthetic arm64 macOS acceptance of authored dry movement/health play,
not retail Champions playability. Saves restore collision state without original
WORLD mesh/source associations; cosmetic selection clears on load, art remains
read-only, and original combat/progression are unimplemented. Private retail,
Windows and physical Intel validation remain unavailable/unexecuted. Standalone
runtime Rosetta/RSS proof is identified as owner evidence, not this integration's
execution. Reference-function coverage unchanged; client integration only.
No push, PR, issue, protected-worktree access or canonical integration commit.

## 2026-10-18: playable integration wave 3 — COMPLETE (local)

`codex/playable-integration` starts this round from clean wave-2 handoff
`65901c8`. Both released final reports/APIs were read before serial no-ff merges:

| Branch | Released tip | Local merge |
|---|---|---|
| inventory-icons | d13ec1c | c4823bb |
| hud-layout | 5e5fb93 | 96341ef |

Conflicts were additive CMake registrations/source lists; both sides were kept.
All incoming portable native source/header bytes remain unchanged. All 108
historical vector files remain byte-identical to `60def02`.

Scene integration `56df452` adds native-positioned HUD frames/bars inside the
original-art panel and a simple explicit inventory-picture inspection grid.
Native services own HUD coordinates, fill, animation frame and painter order;
selected original-art resources are decoded/shaped natively. The scene omits
unavailable mana instead of fabricating a play-session maximum. A separate real
native recovery observation verifies the mana mapping. Unknown HUD gates and
full original-screen/framebuffer parity remain unclaimed.

Inventory pictures come from explicitly selected HDR/DAT files and exact names.
Native lookup, decoder and footprint/origin feed an authored 8×8 grid; checked
sector reads, 64-picture limit, failure preservation and modal pause are enforced.
Live item ownership, item tables/body selection, original cache scheduling and
original inventory screen associations are not invented. Native filename/cache
policies are merged/tested, while this explicit picture inspector remains separate.

Fresh verification:

- Release native and universal extension builds passed with four build jobs.
- Full CTest **160/160 passed** in **652.66 s**, `-j 3`, exit 0. `player-exact`
  passed in 117.69 s; no timeout or isolated rerun. All 15 skills/kernel checks
  and all four newly added inventory/layout tests passed.
- Boundary: 74 selected paths, 38 tests, pass. Python: 712 tests, 385 skips, pass.
- All 21 headless/import/default commands and all **17 final export commands** passed.
- Actual combined export: **36,000 ticks**, **72,142 checks**, zero failures;
  save/load matched continuation through **36,257**. Peak retained records 6,650,
  stable capacity 8,512 slots, continuation size 2,979,244 bytes.
- The 960×720 and 800×600 combined UI runs each passed **1,422 checks**. Legacy
  diagnostic flow still passed 102 checks at both 16:9 and 4:3.
- **98 final viewport captures** saved. Inspected native HUD placement, decoded
  grid pictures, compact dialog fit, retained-grid error and continuation state.
  A preliminary oversized inventory modal was fixed and reverified before the
  final export; no remaining visual defect was found in inspected states.

App/ZIP: `build/exports/wave3-final/` in the playable-integration worktree.
Round report: `work/loop-playable-integration-r3-report.md` (STATUS: COMPLETE).
Logs/captures: `work/playable-wave3/`; detailed commands, hashes, source pins,
merge table, changed files and capture paths:
[`PLAYABLE_WAVE_2026_10_17.md`](PLAYABLE_WAVE_2026_10_17.md).

Synthetic arm64 macOS acceptance only. Private retail files are absent; original
HUD frame parity, full gameplay inventory, Windows and physical Intel execution
remain unverified. Reference-function coverage unchanged; client integration only.
No push, PR, issue, agent, scheduler or canonical integration write.

## Playable integration wave 4 — opt-in combat — 2026-10-18

STATUS: COMPLETE

Starting handoff: `e745034e06e9441a626f038e595211c6b6004dd1` on
`codex/playable-integration`. Read the released report and contract before merging.

| Branch | Released source | Local merge |
|---|---|---|
| combat-runtime | 188bd74981f1856fe15b038394f2863f64a55e9a | 50a4192 |

The native CMake conflict was resolved by retaining all existing play/inventory
registrations and adding combat tests. All 12 released native combat files and
all 108 historical vector files remain unchanged. Integration commits: `d2a2312`
(native/scene policy), `1172405` (import-safe vector CLI), `a9e2787` (verified
combat help text). No original arithmetic, timeout or research corpus changed.

`start_play_session(true)` selects the explicit `rta-combat-fixture-v1` policy;
the scene checkbox is off by default. One combat World owns a player and two
Skelights, driven by the released SessionAdapter at the native ready priority-10
boundary. The old actor World is destroyed when combat is published. Native
observations drive actor markers and the original-art HUD. The zero-argument
start method retains normal bounded play and its continuation format.

The fixture accepts only supported flat/dry ground and explicitly selects
synthetic empty-resource services and animation timings. No original encounter,
player action switch, attack-selection rule, walking or asset pairing is invented.
The player hit-event button queues mask 0x1, without changing action state.
Combat saves refuse before file creation because no versioned combat format
exists. A valid ordinary continuation can atomically replace combat. The separate
4,096-tick fixture budget does not alter normal long-play acceptance.
Contract and exact supported/refused boundaries: [PLAYABLE_COMBAT.md](PLAYABLE_COMBAT.md).

Fresh evidence:

- Native Release and universal debug/release extension builds passed, `-j 4`.
- Full CTest **163/163 passed** in **690.59 s**, `-j 3`, exit 0. `player-exact` passed in 148.48 s; no timeout or isolated rerun. All 15 skills/kernel checks passed.
- Native combat checks on arm64 and x86_64/Rosetta agree: 71 runtime host checks,
  490,407 exact comparisons, and the added integrated fight/refusal check.
- Boundary: 76 selected paths / 38 checks, pass. Python: 712 tests / 385 expected
  skips, pass after guarding the new vector CLI against execution during import.
- 21 headless/import/default commands and all 17 final export commands pass.
- Actual exported combined flow: 36,000 normal-play ticks, 72,189 checks, zero
  failures; continuation matched through 36,257. Retained peak 6,650 records,
  stable capacity 8,512, continuation 2,979,244 bytes.
- Opt-in fight: all three actors alive initially; creature hits reduce player HP
  to exactly 84 at tick 12; player hit at 13 makes Skelight 1 HP nonpositive;
  own-run death/state 20 at 14; deletion/marker removal at 15. Second creature
  remains alive, final shared seed 1140036074. Pause and refusals preserve state.
- 960×720 and 800×600 combined runs each pass 1,469 checks. Legacy diagnostic
  exported flow still passes at 16:9 and 4:3.
- 116 final captures, with inspected combat lifecycle, HUD, inventory/error and
  compact modal states. The initial combat-help label incorrectly offered WASD/H;
  it was fixed and the entire final export check set rerun. Captures and visual
  notes: `work/playable-wave4/{ui-captures-16x9,ui-captures-4x3,ui-captures-compact,
  captures-16x9,captures-4x3,visual-inspection.md}`.

Final app/ZIP: `build/exports/wave4-final/`. ZIP SHA-256:
`15d8122b8f53a368d7a509bb098e156d522d7e2224fe8f789fedf9f2800bb1ba`.
Embedded/local release dylib is identical, SHA-256:
`cb25c33977c91f8ed3fa7ad50eee441a7c1b3b31ee1d7e7bd11896f1a716a542`.
Both architectures are present; actual graphical execution is arm64 macOS.

Round report: `work/loop-playable-integration-r4-report.md`. Commands, results,
hashes and audits: `work/playable-wave4/`. Prior waves' evidence remains intact.
Synthetic local acceptance only; no private retail, original full-screen/art
parity, physical Intel, Windows, console/emulator or full original combat claim.
No push, PR, issue, agent, scheduler or canonical/source-worktree write.

## Playable scene wave 5 — opt-in level gate and saved state

Released partial level-flow `1cbd5db` merged locally as `243fc39`; scene/native
composition is `d83e00b`. The existing CMake conflict was resolved additively.
No later research branch is included. The port's multiple-WORLD, cutscene-recall
and packed moving-transform refusals remain intact.

`start_play_session(false, true)` selects the default-off synthetic two-level
policy. Real GOB/LMP/objects loading, World free-spot/shared RNG, terrain height,
water Service and Session adapter drive the gate trip. The 180th ready tick loads
the destination outside the scheduler; the next ready tick clears travel. Return
to aa decodes its saved 88-byte fixture state and restores Trigger state. The
scene reads native observations and snaps interpolation at level changes.

This explicitly closed fixture does not implement arbitrary production service
hooks, whole-game disk saves or combined combat/level ownership. Normal bounded
play/continuations and the separate combat fixture remain unchanged. Contract:
[PLAYABLE_LEVEL_FLOW.md](PLAYABLE_LEVEL_FLOW.md). Water adapter builds use
`CHAMPIONS_LEVEL_FLOW_WATER_ROOT` pointing to this integrated worktree.

Final evidence (scene save-capability label corrected in `9e30a59`):

- Release native and universal debug/release builds pass, -j 4. Full CTest
  **166/166 passed in 631.37 s**, -j 3. player-exact passed in 116.14 s; no timeout
  or isolated rerun. All skills A/B/C checks remain green.
- Python 713 tests / 385 expected skips; 78-path boundary census / 39 tests pass.
- Native level exact (1,303 checks), actual water adapter and integrated gate
  tests pass on arm64 and x86_64/Rosetta. All 11 released native files and 112
  historical vector files remain unchanged.
- 22 Godot import/headless/default commands pass; final export has 17/17 passing
  commands. 36,000-tick graphical flow passes 72,420 checks; save/load continuation
  matches through 36,257. Separate combat fight and level-gate acceptance pass.
- 960×720 and 800×600 exported flows each pass 1,700 checks. There are 134 final
  captures with inspected arrival, saved-state return, pause, HUD and inventory.
- The level fixture explicitly advertises no disk-save format; its pause-menu
  label now agrees. Initial test-fixture concurrency/census issues are documented
  with superseded evidence; final acceptance is green.

Artifacts: `build/exports/wave5-final/`. ZIP SHA-256:
`d41eb81b4eefebff7eafe5b2665a59e6924f2b9b5cc70f8d818590a19adac30a`.
Embedded/local universal release dylib SHA-256:
`d87fcfae28cf749b469562c3f2f505a1762857c6df7288934eb1157ebb8f81b1` (byte-identical).

Report: `work/loop-playable-integration-r5-report.md`. Commands, hashes, final
summary and capture manifest: `work/playable-wave5/`. Synthetic local acceptance
only; no retail/full-engine completeness or physical Intel claim. Local commits
only; no push, PR, issues or canonical/source-worktree changes.


## Playable integration wave 6 — animation composition

Released `codex/creature-anim` tip `5e6748d` merged locally as `9d656a1`.
Implementation `9bbd912` wires the real animation table, poses and skinning into
`start_play_session(true, false, true)`; `d6f2382` keeps compact-window health
visible. The existing manual animation preview remains independent.

The opt-in fixture uses supported Agony state-six animation on explicitly
synthetic combat carriers, plus real player idle dispatch. Combat supports
Skelight bodies while the animation table refuses Skelight's special dispatch;
this pairing is labelled and does not invent Agony AI. The stationary fixture
stops at 90 ticks and refuses player action-release injection and disk saves.
Ordinary play retains its 36,000+ tick continuation path, and the existing
Skelight fixture retains its retaliation/death/removal checks.

Typed mask-`0x1` animation events enter the combat Session adapter in the same
ready tick, once. The generated 0.6-second event lands on tick 37: two hits,
health 100 → 84. The next event lands on tick 67, health 84 → 74. Paused/no-ready
input does not advance poses or combat. Audio is absent; a typed `0x4`/`0x8`
footstep callback point is documented without claiming playback.

See [PLAYABLE_ANIMATION.md](PLAYABLE_ANIMATION.md) and
[wave-6 report](../work/loop-playable-integration-r6-report.md) for the precise
synthetic boundary, retained refusals, captures and final validation totals.


Wave-6 final validation: Release `-j4`; full CTest `-j3` **171/171 pass** in
660.23 seconds, with no timeout/rerun (`player-exact`: 117.28 seconds). Python:
714 tests / 385 expected skips; 80-file portable boundary. All 23 final
import/headless/default commands and 17 exported commands pass. The exported
36,000-tick flow passes 72,597 checks; 960×720 and 800×600 runs pass 1,877 each.
The compact health-header overflow found by capture inspection is fixed and
covered by a graphical width assertion. There are 149 final captures.

Final local app: `build/exports/wave6-final/Open Champions.app`; ZIP SHA-256
`1797f875cb61ac1f6172f2eb95a1b2e5c335cbad503102130fab5b55c43f6037`.
The exported universal release dylib matches the local build byte-for-byte.
All 13 released animation source/header/target files and all 112 pre-wave-six
vector files remain unchanged. No remote publication or retail-playability
claim is made; the precise synthetic carriers and remaining unknowns are
recorded in the linked report.

## Playable integration wave 7 — lighting and combat FX

Released `codex/native-fx` tip `2a4a3a1` merged locally as `8d9463a`; implementation
`cc5ae86` composes the play scene. The merge used additive
CMake/registration conflict resolution. The owner remains PARTIAL: glow, trail,
beam, body-part and unknown death-recipe rendering are not enabled by this
integration. Reference-function coverage is unchanged.

Play mode now uses the real native model/WORLD vertex lighting and the released
particle/decal renderer. The native animation/combat chain drives cosmetic hit
and death hooks once per ready tick, including catch-up batches. The tick-37
animation hit produces health 84, 600 particles and two floor splats. Player
retaliation, death and next-tick removal preserve the exact gameplay RNG and do
not duplicate effects. Blood settings retain native burst/splat/pool gating.

No user definitions were supplied. The scene labels its authored synthetic
particle definitions, sheets and lighting; it reads JSON only through an
explicit pause-menu file selection. The picker acceptance fixture is synthetic
too. No retail value is committed. The red-material/death-pool assignment is a
client test policy, not original actor-material or death-recipe reconstruction.

Source/pose caches and material overrides preserve current animated vertices;
static WORLD lighting publishes with its observation, retaining failed-load
resource identity. A graphical correction puts the synthetic FX disclosure
above the viewport and removes the play footer's obsolete claim that effects
are omitted. Diagnostic labels retain their original scope.

Contract: [PLAYABLE_FX.md](PLAYABLE_FX.md). Round report:
[wave 7](../work/loop-playable-integration-r7-report.md). Build/test/export logs,
source audit and captures: `work/playable-wave7/`. Initial graphical evidence is
kept separately under `initial-export/`.

Wave-7 final validation: native Release `-j4`; full CTest `-j3` **172/172 pass**
in 624.42 seconds, no timeout or rerun (`player-exact`: 112.49 seconds). Python
714 tests / 385 expected skips; 80-file portable boundary. Godot's 25-command
import/headless/default matrix passes; the final combined rerun passes 1,867
checks. All 18 final exported commands pass. The 36,000-tick graphical flow
passes 72,687 checks and continues its saved state through tick 36,257; 960×720
and 800×600 each pass 1,967 checks. **179 current exported captures** include
visible bursts, floor decals and real decoded WORLD geometry with native light
colours. Development blend/additive overlap and floor-decal visuals also pass.

The 8 audited released FX implementation/header/bridge/document files and all
118 pre-wave-seven vector files are unchanged. Three affected native checks
also pass under x86_64/Rosetta; graphical execution is arm64 macOS. Reference-function
coverage unchanged; client integration only.

Final app: `build/exports/wave7-final/Open Champions.app`. ZIP SHA-256:
`5115597c41ac1bcc9addac53e8a166bdc765af1604e8089d0edeabb1e0f4b980`.
Embedded/local universal release dylib matches byte-for-byte, SHA-256:
`582bbc45183a35e29e7f25f1afbd7a583d7895443046d38f1bb4f599256ae309`.
Reports, hashes, manifest and final summary are local; no remote publication,
private asset search or retail-playability claim.

## Playable integration wave 8 — textured WORLD with native lighting

Released `codex/level-textures` tip `973782d` merged as `c34004e`; implementation
`757a372` connects it to the real play shell. The native CMake test conflict
retains both additions. The released decoder, native material binding and C++
texture bridge remain unchanged.

Explicit WORLD/TEX selection prepares decoded image/material/UV resources and
native collision before atomic publication. Revisit reopens only selected pairs.
Native-FX supplies WORLD vertex colours to the texture shader; no new point-light
formula was introduced. Existing synthetic lighting inputs remain labelled.
Blending, cutout, clamp/bilinear sampling and layer priority retain the released
material rules. A graphical pixel comparison confirms actual texture modulation.

Environment mapping, console depth parity and automap tile-code mapping remain
gated. Chunks marked non-dynamic retain their base pass, with special constant
lighting also gated. Full model 0x46 decoding still refuses; its two row helpers
remain explicit caller options. No retail input was used or committed.

Native save bytes remain unchanged. Bounded client resource ownership restores
textures only for an exact same-session save path/hash; unknown or changed files
restore neutral collision display without guessing original asset associations.
See [PLAYABLE_LEVEL_TEXTURES.md](PLAYABLE_LEVEL_TEXTURES.md) and
[wave-8 report](../work/loop-playable-integration-r8-report.md) for exact policies,
validation and captures. Reference-function coverage unchanged; client integration
only. Commands, results and source audits are under `work/playable-wave8/`.

Wave-8 final validation: ten Release native/main-extension/texture-extension
configure/build commands pass (`-j4`); both extensions are universal. Full CTest
`-j3` **173/173 pass in 605.46 seconds**, no timeout/rerun (`player-exact`: 115.05 s).
Python 714 tests / 385 expected skips; 80-file simulation boundary unchanged.
Godot matrix 29/29 and actual export 21/21 commands pass. Final combined exported
headless check: 1,896 assertions. The 36,000-tick graphical journey passes
72,723 checks and the prior identical continuation through tick 36,257; 4:3 and
800×600 each pass 2,003 checks. There are **188 final exported captures**.
The texture/light pixel comparison changes 6,538 pixels at 16:9 and 4:3 and
2,984 at 800×600. Actual final textured/lit, restored texture, HUD/inventory and
animated-hit captures were inspected.

The 8 audited released texture files, 7 prior native-FX files and 118 historical
vector files remain unchanged. The exported synthetic fixture is byte-identical
to its native source. Native texture checks also pass on x86_64/Rosetta (1,067);
no physical Intel or private retail execution is claimed.

Final app: `build/exports/wave8-final/Open Champions.app`; ZIP SHA-256:
`df05d1a58ad687c308290ab92201b0e6376f0a1c8620ae43d4e4c8cba0d5d522`.
Texture extension SHA-256: `05264dc90fcbc1eaa184db9c33a16c5398565bb7bba069b14f3822e03958b2a0`.
Both embedded main/texture extensions match local release libraries byte-for-byte.
Final hashes, capture manifest and summary are in `work/playable-wave8/`.
Local commits only; no push, PR or issue.

## Playable integration wave 9 — native audio — 2026-10-18

Released `codex/native-audio` **bd7bd54** is merged locally as **df4633c**;
composition commit **634c0dc**. CMake/registration/ROADMAP conflicts retained all
previous additions. The released core, its test and all 118 prior vectors remain
unchanged. Reference-function coverage unchanged; client integration only.

The play scene now drives native audio once per successful ready tick, including
catch-up batches, and routes creature-animation events using explicit ClipRole.
No-ready, paused and failed advances do not tick audio. Device pulls only consume
PCM. New play/reset/continuation load restart cosmetic sound at the saved tick
origin without changing native save bytes or game RNG. Menu sound has a separate
entry-only presentation clock. All banks and cue associations are synthetic.

Native menu/level/ambient selectors and explicit Broon boss enter/leave hooks use
the documented policies. The second boss refuses; unknown flag meanings and all
other audio gates stay gated. The current stationary combat carriers have no boss
identity or walk AI. Real FatCyclops walk dispatch is separately checked headlessly:
footsteps produce PCM at ticks 13/49; level music starts at tick 4. The scene's
actual animated attacks produce two hit voices and damage on tick 37.
Contract and limits: [PLAYABLE_AUDIO.md](PLAYABLE_AUDIO.md).

Fresh verification:

- Release `-j4` native and universal debug/release main/texture builds pass.
- Full CTest `-j3`: **175/175**, 599.87 seconds; `player-exact` passes in aggregate
  (112.01 seconds), no timeout/isolated rerun.
- Audio suites: 185 core and 164 composition checks pass on arm64 and x86_64/Rosetta,
  with identical stdout. Python tooling passes 714 tests, 385 expected skips.
- Godot matrix: 31/31 commands. Exported matrix: 22/22 commands.
- Exported 36,000-tick plus save/load/continue flow: 108,758 assertions; 960×720 and
  800×600: 2,678 each; exported headless: 2,563. All zero failures.
- 200 captures under `work/playable-wave9/{captures-*,ui-captures-*}/`; inspected
  actual audio status, tick-37 HP/actors, decoded lit WORLD and compact layout.
  Full native-lighting pixel checks remain green (6,538/2,984 changed pixels).

App: `build/exports/wave9-final/Open Champions.app`.
ZIP SHA-256: `463fb7d4e87c9d7dae07827afe5896a50a9658ed7588836320b02922d4eabb70`.
Main universal extension: `0d5f132e58f985b6cea39c4745d30e8bdd88c90da456d0d84bd230e05b25ac17`.
Texture universal extension: `05264dc90fcbc1eaa184db9c33a16c5398565bb7bba069b14f3822e03958b2a0`.
The packaged libraries match the local builds byte-for-byte. Launch with
`open 'build/exports/wave9-final/Open Champions.app'` from this worktree.

Commands, logs, source audit, capture manifest and artifact metadata are under
`work/playable-wave9/`. Round report:
`work/loop-playable-integration-r9-report.md`. No push, PR, issue or protected-tree
edit. No retail, physical Intel, Windows/Linux or heard-speaker acceptance.
The owner-known ObjectDB shutdown warning remains documented: verbose evidence
identifies one AudioStreamGeneratorPlayback reference in the isolated smoke and
three after the combined reset/load flow; assertions and exit codes pass. Exact
engine-side lifetime cause is unverified. Earlier gameplay, level, FX and texture
gates remain unchanged.

## Playable integration wave 10 — level-flow-2 — 2026-10-18

Released `2c57ba3` merged cleanly as `103658a`, on top of wave 5's `1cbd5db`.
It resolves native stored-order WORLD selection, base/chunk light, recall expiry
(network F+59/offline S+60, lost during cutscene) and exact moving-piece transforms.
The wave-5 host does not directly access `RecallCountdown`; no type adaptation
was needed. Existing scene scope remains gate travel and saved-level return.

Release native and universal main-extension builds pass (`-j4`). Level-flow exact,
water-adapter and level-play tests pass **3/3**; the Godot combined level check
passes **1,631 assertions**. Full CTest: **175/175 pass**, 602.89 seconds, no timeouts or reruns. Evidence:
`work/playable-wave10/`; short addendum in `work/loop-playable-integration-r9-report.md`.
No new graphical export this round; wave-9 artifacts retain their original scope.
Local commits only; released gates and private-data boundaries preserved.

## Playable integration wave 11 — Full combat and native-fx-2 — 2026-10-19

Released `codex/combat-runtime-2` `4d5a010` merged as `7544903` and
`codex/native-fx-2` `8b01a7f` as `514ac5f`, on top of `b16db1c`. Both tips are
ancestors of the branch. Integration commits `517c35b..0d042ca`.

- **Full composition, opt-in.** `combat_play::Fixture::Full`
  (`rta-full-combat-fixture-v1`) is a third explicit combat selection; the
  ordinary and animated fixtures are unchanged. WASD is the stick (executed
  `Player_move`), J holds attack, K holds block and L queues Holy Strike. The
  research `PlayerAnim` fixture never writes track weights, so `Player_run` could
  never release a block; the session's labelled clip fixture starts player
  tracks at full weight. See [PLAYABLE_COMBAT.md](PLAYABLE_COMBAT.md).
- **EffectSimulation every tick.** `combat_play::Effects` runs as the session's
  ready-tick observer after the World pass, drawing or headless. Effect-class
  damage reaches `World::damage_descriptor`, the executed descriptor glue the
  spells use. Combat-runtime-2 refused DiseaseCloud's damage because allocating
  a DiseaseEffect had no body. Round 3 builds that object with no attacker.
  The next creature tick still refuses `SpellDiseaseEffect_run`. Fire and
  ice-shatter constructors still refuse. GutChunk's constructor is round 3;
  its run was not executed.
- **Exact GS-order rendering.** The scene's round-1 adapter called removed
  bridge methods. It now presents the bridge's GS-order runs through a game
  camera derived from the scene camera. Integration fix: `NativeFxRenderer`
  wrote clip-space Y unsigned, so effects were mirrored vertically against the
  scene (a sprite projected at y 574.6 drew at 145 of 720). It now takes the
  sign from `PROJECTION_MATRIX[1][1]`. `playable_fx_camera_smoke.gd` checks the
  projection (≤ 0.16 px) and the rendered pixel centroid (≤ 0.6 px on forward+,
  mobile and compatibility). See [PLAYABLE_FX.md](PLAYABLE_FX.md), *Wave 11*.

Checks: native `playable-combat-fx` (39) and Godot
`playable_wave_smoke_full_combat.gd` cover four cases:

- (a) The stick walks the player more than 40 units. Held presses chain
  swings, and `P+0x460` counts up by one per swing with at least 2 variants.
  The first hit lands on swing start + 16. The Skelight is deleted the next
  tick and destroyed at the following flush. A front block takes no health;
  after release, the next swing hurts.
- (b) Holy Strike cast on tick 2 damages on tick 10 (cast + 8: speed 450, radii
  12 + 12, 80 apart), with 267 puffs, glows and its LightEffect.
- (c) A 300-tick scenario gives the same seed/HP trace headless and drawn. The
  SHA-256 is `8dc349c1…aa0c` in the dev headless, dev GL, exported headless and
  exported 16:9 / 4:3 / 800×600 runs.
- (d) Wave 11 met the DiseaseEffect allocation refusal. Round 3 attaches the
  effect instead, and the following tick refuses `SpellDiseaseEffect_run`.

Fresh verification (`work/playable-wave11/`):

- Ten Release `-j4` build commands pass (native; universal debug/release main
  and texture extensions).
- Full CTest `-j3`: **183/183**, 614.91 s. `player-exact` passed in the
  aggregate run (109.05 s), so no isolated rerun was needed.
- Godot dev matrix: **35/35**. Full combat headless: 2,593 checks.
- Exported matrix: **13/13**. The 36,000-tick 1280×720 Full run has
  **108,800** checks; 960×720 and 800×600 have 2,720 each; exported headless
  has 2,593. All have zero failures.
- `combined-fx` 1,864 / `combined-audio` 2,560 (wave 9: 2,563; the ported FX
  smoke replaces five round-1 snapshot checks with four native-fx-2 ones per
  gore level).

Captures: 186 PNGs, 18 of them new `full-*` images
(`work/playable-wave11/captures-{16x9,4x3,compact}/`). `capture-manifest.json`
holds their hashes. The inspected images show the walk, combo, block and Holy
Strike puffs travelling from the player to the Skelight. The Holy Strike glow
is at ground height, so its lower half is depth-occluded by the floor.

App: `build/exports/wave11-final/Open Champions.app`.
ZIP SHA-256: `34e07b0b6b0b5a5a30d9d2dddcdf383669fede6c6c8131db3fc7ecd08effa9a2`.
Main universal extension: `07128c4ff866fc57172e81c4516e85c53909d86a02c2ec3b1b7e121d26ac0935`.
Texture universal extension: `05264dc90fcbc1eaa184db9c33a16c5398565bb7bba069b14f3822e03958b2a0`.
The packaged libraries are byte-identical to the local builds. Launch with
`open 'build/exports/wave11-final/Open Champions.app'`.

Limits: all definitions, sheets, recipe constants, clips and services are
synthetic. Block sparks, trail points, sounds and LightEffect colours reach the
trace without GS arguments, so they are counted, not drawn. No drawable `fx_spawn` was reached in these
scenarios (count 0). No retail data, Python tooling
rerun, Intel/Windows/Linux hardware, manual play or heard audio is claimed.
Reference-function coverage unchanged; client integration only. Round report:
`work/loop-playable-integration-r11-report.md`. No push, PR or issue.

## Playable integration wave 12 — review fixes, real data and ten feature branches — 2026-10-20

Start `68e7016`. Eleven released branches were merged with local `--no-ff`
merges, in this order. Every tip is an ancestor of the branch.

| Branch | Tip | Merge |
|---|---|---|
| codex/review-fixes | 28e6cf0 | 17be224 |
| codex/real-data | 386b52a | 4428c6f |
| codex/hud-fx-3 | 9a287f4 | e523927 |
| codex/audio-2 | 46d0970 | bf1fe55 |
| codex/textures-2 | 2f1a547 | 90c9640 |
| codex/creature-ai | aee703f | bd490a8 |
| codex/combat-runtime-3 | 7f43045 | via 13efdfc (combat-runtime-4 contains it) |
| codex/combat-runtime-4 | faf44b6 | 13efdfc |
| codex/ui-fonts-2 | e8ee00e | 828346f |
| codex/level-lights | 2d0a58f | cf10465 |
| codex/level-spawn | 11167ac | 371c053 |

`codex/fx-trig` is still running and is not in this wave.

Conflict resolutions (all unions, no behaviour chosen):
- `gdextension/CMakeLists.txt`: the extension's source list gains
  `ui_hud_round3_service.cpp`, `ui_font_page.cpp` and `level_lights_service.cpp`
  beside review-fixes' save libraries.
- `native/src/audio/native_audio.cpp` `load_stream`: audio-2's queued span
  (offset, length, loop restart) is kept, and review-fixes' refusal of an entry
  missing from the current directory still applies.
- level-texture `CMakeLists.txt` / `level_texture_preview.cpp` /
  `level_texture_material.gd`: textures-2's level-draw source and alpha modes
  are kept with real-data's real-world sources and per-program shader cache.
- `combat_runtime_targets.cmake` and `combat_runtime_services.cpp`:
  creature-ai's own-class slot-27 dispatch and combat-runtime-3's empty-slot
  return are both kept.
- `docs/HUD_LAYOUT.md` and `ui_layout_dynamic_tests.cpp`: both sections and
  both test blocks.

Integration work:
- **Session start and mesh cap** (`a0f5618`, `d4ecfda`).
  `session_control::Config::start` is an optional client start position. It
  is saved in the terrain marker byte (2 = terrain plus start); scenes without
  it keep the historical bytes, and the native-save trace digest is unchanged.
  `set_terrain_start_objects` applies level-spawn's `story_start_ground`.
  `present_geometry` merges each chunk's batches into one surface. Its cap
  moves from 4,096 surfaces / 1 M triangles to 16,384 / 4 M. Measured peak:
  4,307 surfaces and 1.83 M triangles; +125 MB static and +66 MB video
  rendered. real-data's private not-drawn copy is removed.
- **Full fixture extensions** (`934a7ff`). Journaled operations 14/15 add one
  creature-ai own-machine creature and thrown potions 1–3
  (combat-runtime-4). Holy Shield is not offered; see
  [PLAYABLE_COMBAT.md](PLAYABLE_COMBAT.md).
- **Real-data scene** (`a71f470`): per-level light records, story-start
  ground, FNT rows as an explicit HUD font choice, and recipe/light JSON
  arguments. See [REAL_DATA.md](REAL_DATA.md), *Wave 12*.
- **Fix** (`8b1f124`): level-lights' smoke compared `shade_world` with a
  Vector3, but review-fixes returns a refusal dictionary. The script stopped
  on the error and never quit.

Real data (counts only, user's disc): **108 / 111 levels load and play**
(59 in real-data round 2). The 3 refusals all have no ground under the
radius-12 story start. HUD 34 / 34, FNT pages 8 / 8, light records 90 named /
18 default rows, recipes 1 / 1 with 3 light records. The graphical pass gives
the same 108 / 111 and 20 captures. The exported app ran 12 / 12.

Verification (`work/playable-wave12/`):
- Ten Release `-j4` builds pass (native; universal debug/release main and
  texture extensions).
- Full CTest arm64 `-j3`: **196/196**, 682 s (`player-exact` 134 s).
- x86_64 under Rosetta, `build/native-x64` (`CMAKE_OSX_ARCHITECTURES=x86_64`), `-j3`: **196/196**, 980 s; `player-exact` passed in the aggregate run (153.9 s), so no rerun was needed.
- Godot dev matrix **43/43**. The three creature-anim entries first failed
  because `godot/creature_anim/bin` was not built in this worktree; after
  building it they pass (32, 25, 231 checks).
  - Full combat: 2,616 checks. Wave-12 extensions: 40. FX bridge refusals: 33.
- Export matrix **16/16**:
  - The 36,000-tick 1280×720 Full run first had 1 failure in 108,822 checks:
    the known flaky "stale clip clears cosmetic owner" (also reported by
    review-fixes and audio-2). Rerun alone: 108,822 checks, 0 failures.
  - 960×720 and 800×600: 2,742 each. Exported headless: 2,616.
  - Every `FULL_TRACE` is `8dc349c1…aa0c`.

App: `build/exports/wave12-final/Open Champions.app`.
ZIP SHA-256: `30e3ca55ae5615392588e4d91f2cb68ba4c47865c978a3ff9e06348258620d87`.
Main universal extension: `3484af976129edf6d80b06fe899056fe9162b27ecdc1b8e502ded2debbff78d0`.
Texture universal extension: `858bf6747dcbb16a11b9c49bb86c16e11407ae5f3a85184a74b0184b1e027c51`.
The packaged libraries are byte-identical to the local builds.

Limits: everything listed as refused in the branch reports stays refused.
The Full fixture's definitions, clips and services are still synthetic. No
Intel/Windows/Linux hardware run (x86_64 is Rosetta only), no manual play and
no heard audio are claimed.

## Playable integration wave 13 — fx-trig (math_cos16) — 2026-10-20

Start `eb583b8`. `codex/fx-trig` (tip `f9900c7`, branched from review-fixes
`28e6cf0`, which wave 12 already merged) is merged in `917a26f` with no
conflicts. Its tip is an ancestor of the branch.

What it changes: the emitter cosine and sine (`fx::exact::angle_cos_sin`) now
use `math_cos16` / `math_sin16` (`champions/fx_trig.hpp`). Those are the
degree-7 half-angle polynomial with the ps2 adder, in integer arithmetic. Each
float32 result is promoted to binary64. This replaces the correctly rounded
double-double stopgap from review-fixes finding 13, and `tools/binary64_trig.py`
is removed. Glows, trails, decals, recipes and the combat cosine already
called `core::cos16` / `sin16`. No other caller in `native/` or `gdextension/`
uses host `cos` / `sin`. New refusals: `draw_haze`,
`trig::refuse_block_sparks_angle` and
`trig::refuse_creature_process_damage_angles`. Comments mark the existing
block-spark and `process_damage` bodies that these refusals cover; the bodies
themselves are unchanged. Combat runtime round 5 replaces those two refusals
with the executed registers (the sign-extended hit halfword plus `0x7fff`, the
sign-extended facing, and the gib heading) and calls `fx::trig::cos16` /
`sin16` there. Haze kind 3 and the haze screen axis stay refused. See
[NATIVE_FX.md](NATIVE_FX.md) and [COMBAT_RUNTIME.md](COMBAT_RUNTIME.md).

Changed vectors: only `fx_exact.tsv`, with 900 `emitter` rows and all 16
`trig` rows (every 16-bit angle). No combat, session or other vector file
changed. Checked against the research:
- Regenerating with `python3 -I tools/generate_fx_exact_vectors.py --out <tmp>`
  from pin `19f80a6` gives a TSV byte-identical to the committed one, and the
  same `vectors_sha256` `67290583…30bb`.
- The pin's `ref_core.cos16` / `sin16` equal `t3/research-fx` `fe137cc`
  `tools/ref_trig.py` on all 65,536 angles (0 mismatches). `sin16(0)` is
  `0x34800000`, as the spec says.

No trace hash changed. The Full `FULL_TRACE` is still `8dc349c1…aa0c`, and
`FULL_HOLY_STRIKE` and `FULL_SAVE_LOAD` (including the bridge presentation)
match wave 12 line for line. The Full fixture's emitter use does not reach
seeds, health or the bridge counts.

Verification (`work/playable-wave13/`):
- Native Release builds for arm64 and x86_64 pass, as do the four universal
  extension builds.
- Full CTest arm64 `-j3`: **196/196**, 643 s (`player-exact` 115 s).
- x86_64 Rosetta, `build/native-x64` `-j3`: **196/196**, 1,002 s
  (`player-exact` 145 s). `fx-exact`: 14,690 rows, 0 mismatches on both
  architectures.
- Real data (counts only, `work/run-slices-r13.sh`): **108 / 111**, the same
  3 refusals (no ground at the story start), 0 failed checks. Every
  deterministic count equals wave 12. Only timing differs (`time.*`,
  `audio.frames_pushed`, which is pumped once per process frame) plus a
  5.8 KB difference in the peak static memory. The exported app ran
  12 / 12 levels.
- Godot dev matrix **43/43**: Full combat 2,616 checks, combined FX 1,876,
  wave-12 extensions 40, FX bridge refusals 33, creature-anim 32 / 25 / 231.
- Export matrix **16/16** on the first run. The 36,000-tick 1280×720 Full run
  passed 108,822 checks with 0 failures, so the flaky stale-clip check did not
  fire this time. 960×720 and 800×600 passed 2,742 checks each, and exported
  headless 2,616. Every `FULL_TRACE` is `8dc349c1…aa0c`.

App: `build/exports/wave13-final/Open Champions.app`.
ZIP SHA-256: `9574f43d51234996669a5fbfa6b2c6a85a75553f1057ef00158839c03756710c`.
Main universal extension: `12aceca2b2dafbe0a84f81eade312bb9388e43626b47774a3f296ec35f97f05e`.
Texture universal extension: unchanged at `858bf674…7c51`.
The packaged libraries are byte-identical to the local builds.

Limits: the console's last bit of an add is still the research's open
question, and the port uses the ps2 model. The three fx-trig refusals stay
refused. No native Intel, Windows or Linux run (x86_64 is Rosetta only). No
manual play is claimed.

## Playable integration wave 14 — level-spawn-2 (mode-A arena start) — 2026-10-20

Start `037e26d`. `codex/level-spawn-2` (`53c4883..e7ac3d5`, branched from
`037e26d`) is merged in `b7374ca` with no conflicts.

What it changes: when a cold story start's radius-12 snap has no floor, no
Player1 took and the level has an `ArenaPlayerStart`, `story_start_ground` uses
the mode-A wave start (ring radius 180, slot 0, count 1, `findFreeSpot` spread
36 then 240 on an empty object list with seed 0, then `player_ctor_z` at
radius 12). Research pin `t3/research-levels` `13fafa46`, `docs/level-flow.md`
section 18. `champions-level-flow` now links `champions-objects`. See
[LEVEL_FLOW.md](LEVEL_FLOW.md).

Integration fix (`ed499e2`): the level-texture extension lists its native
sources by hand and compiles `level_flow.cpp`, so after the merge it failed to
link (`objects::World` undefined). It now also compiles
`objects_lifetime.cpp`, `objects_scheduler.cpp` and `objects_queries.cpp`.
level-spawn-2 had not rebuilt that extension.

Verification (`work/playable-wave14/`):
- Native Release arm64 and x86_64 builds pass, as do the four universal
  extension builds (after `ed499e2`).
- Full CTest arm64 `-j3`: **196/196**, 659 s (`player-exact` 118 s).
- x86_64 Rosetta, `build/native-x64` `-j3`: 194/196 in the aggregate run
  (2,567 s, overlapping the sweep and the Godot runs). `monsters-1-exact`
  (its 180 s trace subprocess limit) and `skills-a-exact` (120 s) only timed
  out; rerun alone both pass (210 s, 116 s), so **196/196**.
  `level-flow-exact` and `fx-exact` pass on both architectures.
- Real data (counts only, `work/run-slices-r14.sh`): **111 / 111** loaded and
  played, 0 refused for any reason, 0 failed checks. Every deterministic count
  equals level-spawn-2's sweep. See [REAL_DATA.md](REAL_DATA.md).
- Godot dev matrix **43/43**: Full combat 2,616 checks, combined FX 1,876,
  wave-12 extensions 40, FX bridge refusals 33, creature-anim 32 / 25 / 231,
  `native_fx_visual` 4 runs, no refusals.
- Export matrix **16/16** on the first run. The 36,000-tick 1280×720 Full run
  passed 108,822 checks with 0 failures (140 s). 960×720 and 800×600 passed
  2,742 each, exported headless 2,616. Every `FULL_TRACE` is still
  `8dc349c1…aa0c`. The exported app also ran the 12-level real-data set
  (12 / 12, counts equal wave 13 apart from timing) and the three arena
  levels (3 / 3).

App: `build/exports/wave14-final/Open Champions.app`.
ZIP SHA-256: `8bd72823081a9cafa75103baa0a6cd2691ceb13b9d128cfcfc1e4a544739721b`.
Main universal extension: `bfaf12dd7da6168677a4f7c3f35ea96faebfcbf82be46b58bbc97b46abe4c0bf`.
Texture universal extension: `0b93213ee395cfd16701fea24f0c375aeb77e7fd7f7aa7d87c24aa1b64349941`.
The packaged libraries are byte-identical to the local builds (x86_64 + arm64).

Limits: the live `Player1_ctor` seed, the `findFreeSpot` class radii, the
mode-A name table, `arena_pickSpawnPoint` with a0 = 0 and the unchecked
net-peer index stay refused. No native Intel, Windows or Linux run (x86_64 is
Rosetta only). No manual play is claimed.

## Playable integration wave 15 — script-commands (opt-in level scripts) — 2026-10-20

Start `6081211`. `codex/script-commands` (`037e26d..2088ab1`, three commits) is
merged in `b2882e5` with no conflicts. See [SCRIPT_COMMANDS.md](SCRIPT_COMMANDS.md).

What it adds: `champions-script-commands`, an opt-in host for a level's script.
Documented commands run the existing binder, and six of them route to existing
subsystems. Partly documented and unknown commands return 0 and are counted as
refusals. The session controller gains `enable_level_script`,
`disable_level_script` and `script_command_counts`. It pumps one ready tick per
simulation step only while a script is enabled. The counts are not observation
fields. The host owns its own variables, object world, effect world and
level-flow state, so script effects do not reach the drawn level.

Scene wiring: `real_data_play` already had a "Level script (opt in)"
checkbox and `--level-script`. With either, `load_real_level` reads the level's
`script.scr` and `objects.ob` and attaches the host, and `start_play` reattaches
it after a reset. Without them the scene calls `disable_level_script`, and no
host exists. The merged code needed no change. `8141634` adds per-level counts
for each command to `real_data_scene.gd --scripts`. `becea3e` raises
`skills-a-exact`'s CTest limit from 120 s to 300 s. Under Rosetta it ran 116 s
alone in wave 14, about five times its arm64 time.

Verification (`work/playable-wave15/`):
- Native Release arm64 and x86_64 builds pass, as do the four universal
  extension builds.
- Full CTest arm64 `-j3`: **199/199**, 694 s (`player-exact` 147 s,
  `skills-a-exact` 28 s). A first attempt ran while sibling worktrees held the
  load average near 200. `player-exact` timed out there, so that attempt was
  stopped and rerun once the load fell.
- x86_64 Rosetta, `build/native-x64` `-j3`: **199/199** on the first run,
  1,076 s, no timeouts (`skills-a-exact` 47 s, `monsters-1-exact` 79 s).
  The new `script-commands` tests pass on both architectures.
- `champions-script-commands-measure --game rta` from the integrated build
  matches the branch: 111 scripts loaded, 76 started, 41 wait, 37 complete,
  0 faults, 1,681 executed across 36 names, 529 refused across 27.
- Real data (counts only, `work/run-slices-r15.sh r15 --scripts`): **111 / 111**
  loaded and played, 0 refused, 0 failed checks. 111 scripts attached and 76
  started. 22,371 commands executed across 38 names and 1,375 refused across
  28. Every count outside the host equals wave 14. See
  [REAL_DATA.md](REAL_DATA.md).
- The scene's own path, with `--level-script`, attached on 3 / 3 levels.
  Without the flag, 0 / 3 attached.
- Godot dev matrix **45/45**: the wave-14 43 plus `script_commands_smoke.gd`
  and `real_data_play.tscn -- --level-script` with no game folder. Full combat
  2,616 checks, combined FX 1,876, wave-12 extensions 40, FX bridge refusals
  33, creature-anim 32 / 25 / 231, `native_fx_visual` 4 runs with no
  refusals.
- Export matrix **18/18**: the wave-14 16 plus the same two script checks
  exported. The 36,000-tick 1280×720 Full run passed 108,822 checks with
  0 failures (142 s). Exported headless passed 2,616 and 800×600 passed
  2,742. The 960×720 run stalled once in the matrix: its window drew no
  frames, so it wrote 0 captures, the headless-versus-drawn trace check failed,
  and it was killed after 1,179 s. Rerun alone it passed 2,742 checks with
  63 captures in 18 s. Every `FULL_TRACE` is still `8dc349c1…aa0c`.
- Exported app on real data:
  - The 12-level set equals wave 14 apart from timing, both without and with
    `--scripts`. With scripts, 6 of the 12 levels started a script: 656
    commands executed and 112 refused, 0 faults.
  - The three arena indices: 3 / 3, equal to wave 14. Their scripts have no
    level entry, so none started.

App: `build/exports/wave15-final/Open Champions.app`.
ZIP SHA-256: `87423687cd015a4d854bd80ecfeca6708d1b8dee926641426ea72eeb23b8f019`.
Main universal extension: `283420664a26c469ec66842379ca859c3d0c6c8160191b56dd36a7e6046ee95a`.
Texture universal extension (unchanged from wave 14): `0b93213ee395cfd16701fea24f0c375aeb77e7fd7f7aa7d87c24aa1b64349941`.
The packaged libraries are byte-identical to the local builds (x86_64 + arm64).

Limits:
- Combat, creature AI, audio, HUD text, item, cutscene and path commands stay
  refusals.
- Script effects stay in the host's own worlds. Particles are not drawn, and
  `hide`/`show`/`moveMonster` do not move drawn meshes.
- The post action runs once at load.
- No native Intel, Windows or Linux run (x86_64 is Rosetta only). No manual
  play is claimed.

## Playable integration wave 16 — water-draw (documented water surface) — 2026-10-21

Start `685c6e6`. `codex/water-draw` (`037e26d..be0f580`, three commits) is
merged in `679b7d7`. See [WATER_DRAW.md](WATER_DRAW.md).

Conflicts: `docs/REAL_DATA.md` (both sides kept: the wave-14/15 sections, then
the water-draw section) and `godot/tests/real_data_scene.gd` (both
`--scripts` and `--water` kept). No code change was needed beyond that.

What it adds: `champions-water-draw`, the documented water colour, FIX blend,
slope UVs, vertices and layer 3, with the wave stepped once per simulation
tick. The level-texture extension compiles it into `ChampionsRealWorld`
(`water_sync`, `water_texture`). `real_world.gd` replaces the neutral tile with
one mesh that has a surface per water set. It uses `render_priority` 3,
`depth_draw_never` and `depth_test_disabled`, and it textures the tiles from
the level's first water `.tex`. The claim-frame seed is refused, so every
block is a zero grid and real tiles stay flat. The main extension is unchanged.

Verification (`work/playable-wave16/`):
- Native Release arm64 and x86_64 builds pass, with 0 warnings on arm64. The
  four universal extension builds pass.
- Full CTest arm64 `-j3`: **200/200** on the first run, 772 s
  (`player-exact` 130 s, `skills-a-exact` 31 s). The new test is
  `water-draw`.
- x86_64 Rosetta, `build/native-x64` `-j3`: **200/200** on the first run,
  1,246 s, no timeouts (`player-exact` 187 s, `monsters-1-exact` 82 s,
  `skills-a-exact` 50 s).
- Real data (counts only, `work/run-slices-r16.sh`): **111 / 111** loaded and
  played with scripts on (`r16 --scripts`) and with scripts off (`r16n`).
  0 refused and 0 failed checks in both. 48 / 48 water textures decoded, all
  64×64. Every other count equals wave 15 (scripts on) and wave 14 (scripts
  off), apart from mesh surfaces, which rose by one: 174,457. One level uses
  both water sets, so its water mesh has two surfaces. See
  [REAL_DATA.md](REAL_DATA.md).
- Windowed captures of four water levels (indices 20, 28, 29 and 30), checked
  by eye. The sheets are flat, with no ripples, and are textured and tinted.
  Overlapping tiles blend twice. A sheet draws over nearer rock because water
  has no depth test.
- Godot dev matrix **45/45**, the same set as wave 15. Full combat 2,616
  checks, combined FX 1,876, wave-12 extensions 40, FX bridge refusals 33,
  creature-anim 32 / 25 / 231, `native_fx_visual` 4 runs with no refusals.
  `FULL_TRACE` is `8dc349c1…aa0c`.
- Export matrix: 17 of 18 passed in the matrix run, then **18/18** after one rerun.
  - The 36,000-tick 1280×720 Full run failed one check in the matrix: 108,822
    checks, 1 failure, "stale clip clears cosmetic owner and restores static
    mesh". This is the animation-picker refusal step. It rewrites the LMP and
    restarts the clip. water-draw does not touch that path.
  - Rerun alone, it passed 108,822 checks with 0 failures (152 s) and 63
    captures.
  - The cause of the single failure is not proven. No timestamp cache was
    found in the scripts.
  - Exported headless passed 2,616, 960×720 passed 2,742 and 800×600 passed
    2,742, each with 63 captures.
  - Every `FULL_TRACE` is `8dc349c1…aa0c`.
- Exported app on real data: the 12-level set with and without `--scripts`,
  and the three arena indices with `--scripts`. Each equals wave 15 apart
  from timing and the new water-texture counts.

App: `build/exports/wave16-final/Open Champions.app`.
ZIP SHA-256: `317bdd38be2a83d088f3acaefdc7da4c5a7189d32e4a6ad2243c40fa9e6a71b6`.
Main universal extension (unchanged from wave 15): `283420664a26c469ec66842379ca859c3d0c6c8160191b56dd36a7e6046ee95a`.
Texture universal extension: `7ef534a2d01ee57ea6084db252639b6b10c8e5ebdfeed9f36536b1d9a950927c`.
The packaged libraries are byte-identical to the local builds (x86_64 + arm64).

Limits:
- No ripples. The claim-frame weights, the drawer's splash point and the
  simulation splash stay refused, so nothing seeds or disturbs the drawn grid.
  A research job is working on them.
- Level-option bit 8 (`filled_mask()`) is implemented and not turned on.
- The play mesh uses world positions and the Godot camera frustum. The
  documented ±1000 screen clip is tested only in `project_strips`.
- No native Intel, Windows or Linux run (x86_64 is Rosetta only). No manual
  play is claimed.

## Playable integration wave 17 — audit fixes, quests, arena — 2026-10-21

Start `77d1260`. Merged in order:

| Branch | Tip | Merge |
|---|---|---|
| `codex/audit-fixes` (combat-runtime-5, the 50 high/medium Grok-audit fixes, `world_findFreeSpot`) | `bffec5f` | `d2e7a8b` (clean; tree equals `bffec5f`) |
| `codex/quests` | `265904e` | `6d5033f` |
| `codex/arena` | `a9af2e4` | `764ddff` (clean) |

Quests conflicts, against the audit's script host (SC-1..7):
- `script_commands.cpp`: undocumented commands go through `refuse()` (a used
  answer stops the entry), except the three that `quests::route_undocumented`
  routes. The `getRand` and stub gates follow.
- `setCFlag` joins the stub-read list, limit 0x200: `ref_natives.n_setCFlag`
  (research-quests 7efd3ce) tests creature flag 0x20 on each object found. The
  stub test fails without the row and passes with it.
- `quests_tests` and `quests_measure` use `run_prestart` / `adopt_objects` /
  `run_start` (the audit removed `begin_level`). The synthetic quest script
  ends `ZERO.pri`, `RETN`.
- ROADMAP and SCRIPT_COMMANDS: both sides kept.

### AntQueen, Skeleton and Soul stay refused

combat-runtime-5 had added them to the Full fight. The audit (CAI-4) removed
them. Evidence checked:
- `t3/research-combat` `tools/difftest_combat_round5.py` (faa7033) executes
  `AntQueen_run` 0x236f78, `Skeleton_run` 0x2a6864 and `Soul_run` 0x2a9870
  (states 0/1 and the phase gate). Each runs on a zeroed 0x800-byte block with
  a stub vtable. `docs/combat-runtime.md` records 101 of 607, 143 of 809 and
  126 of 1062 instructions.
- No tool on that branch calls the constructors 0x236c70, 0x2a5af4 or
  0x2a9478. `docs/monsters-2.md` describes them as **Observed** (read, not
  run). combat-runtime-5's `spawn_class` built all three through the Skelight
  `spawn()` path, with size 0x3c8 and the base vtable.
- `t3/research-combat-ctors` e9777eb (`docs/spec/combat-ctors.md` section 1)
  confirms that round 5 never ran them. It now executes them, and a separate
  native job will port them. They are not restored here. Thrown potions 1 and
  2 (98c081a) and the music volume (22408a1) are documented there too, and
  stay refused here for the same job.

### Wiring the documented replacements

- **Arena respawn pick.** `level_flow::arena_respawn` now forwards the
  registered spawn table to `arena_modes::pick_spawn` with flag 0, which
  research-arena 17810bb executed. It refuses an empty table or a hole below
  count. `level-flow-exact` replays the three generated flag-0 cases through
  it. `champions-level-flow` links `champions-arena-modes` (the new guarded
  `arena_modes_library.cmake`), and the level-texture extension compiles
  `arena_modes.cpp`.
- **Arena mode A.** `arena_executable.{hpp,cpp}` moves the measure tool's ELF
  table reader into the library, plus `read_names` and `mode_a_index`
  (synthetic-ELF tests in `arena-modes-exact`). The arena measure output is
  unchanged.
  - `ChampionsSessionController.load_arena_tables(exe)` keeps the mode-A
    names in memory and returns counts.
  - `set_terrain_start_objects(path, objects, level_name)`: when the story
    snap has no ground and the table lists the level, the start is
    `arena_mode_a_start_ground`, and `start.arena_mode` is "A".
  - `real_data.gd` reads the folder's `SLUS_209.73` when present.
- **Campaign state.** `enable_level_script(script, objects, carry_campaign)`
  can start the new host from the previous host's campaign image. It is off
  by default, so sweep slices stay order-independent.
  `script_command_counts().campaign` reports quest lines, mission bits,
  variables and events.

### Flaky "stale clip" check: cause found and fixed (`4e89ddb`)

`playable_wave_smoke.gd` wrote its fixtures to `user://playable-wave-smoke`.
Every checkout of the project and the exported app share one project name,
so they share one `app_userdata/Open Champions/` folder. A smoke running at
the same time in another worktree could rewrite `cosmetic.lmp`. The picker
then opened a file that already held the 512 clip, so the test's own 512
rewrite changed nothing. The SHA-256 fingerprint still matched, the restart
succeeded, and the check failed.

The directory is now per process (`OS.get_process_id()`) and is removed in
`_finalize`.

Four concurrent `combined-animation` runs, three rounds each:
- Before: 4 of 12 failed. Two were "stale clip" and two were "short DAT
  rejected", the same race on another fixture.
- After: 0 of 12.

The wave-17 dev and export matrices passed on their first runs.

Also fixed (`ac1b0d9`): `fx.reset()` recorded "level light refused: loaded
level name unknown" before the loader named the level, and a later
successful selection never cleared it. Every real level reported it, even
though all 111 applied their light record.

### Verification (`work/playable-wave17/`)

- Native Release builds pass on arm64 and x86_64 with 0 compiler warnings. The four universal
  extension builds pass.
- CTest arm64 `-j3`: **208/208** on the first run, 692 s (`player-exact`
  140 s).
- CTest x86_64 (Rosetta, `build/native-x64`): **208/208** on the first run, 1,284 s, no
  timeouts (`player-exact` 179 s, `monsters-1-exact` 84 s, `skills-a-exact`
  55 s). The binaries are Mach-O x86_64.
- Real data: **111 / 111** loaded and played with scripts on and off, 0
  refused, 0 failed checks, an empty refusal list (final tree, `r17b` /
  `r17bn`). 3 levels start in arena mode A. See
  [REAL_DATA.md](REAL_DATA.md).
- Godot dev matrix **45/45** on the first run:
  - Full combat 2,620 checks; combined FX 1,876; animation 1,809; play-short
    1,341; level 1,643; wave-12 extensions 22; FX bridge refusals 33.
  - creature-anim 32 / 25 / 231. `native_fx_visual` ran 4 times with no
    refusals. `script_commands_smoke` passed.
  - `FULL_TRACE` is `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`
    (21,990 bytes), the audit-fixes value.
- Export matrix **18/18** on the first run. The 36,000-tick 1280×720 Full run
  passed 108,826 checks with 0 failures and 63 captures. Exported headless
  passed 2,620; 960×720 and 800×600 passed 2,746 each. Every `FULL_TRACE` is
  `374e40c9…2035`.
- Exported app on real data: 12 / 12, 12 / 12 with scripts, and 3 / 3 arena
  levels, with 0 failures.

App: `build/exports/wave17-final/Open Champions.app`. SHA-256 values are in
`work/playable-wave17/export-artifacts.json`. Both extensions are packaged
byte-identical to the local builds (x86_64 + arm64).

### Regressions kept (each is an audit refusal with no documented replacement yet)

- Full fight without AntQueen, Skeleton and Soul (CAI-4). Their constructors
  are executed only in research-combat-ctors e9777eb, for a separate job.
- Own classes other than Gnaw and Skelight refuse to spawn (CAI-4).
- Thrown potions 1 and 2 stop a Full session (CR4-1). They are documented in
  98c081a, for the same job.
- No level or boss music until something supplies the music volume (AUD-1).
  The default of 0.5 is documented in 22408a1, for the same job.
- Level scripts:
  - `getRand` is refused, because the play session binds no seed (SC-4).
  - objects.ob records are stubs, so creature, HP and type reads refuse
    (SC-5). This now includes `setCFlag`.
  - A used refused answer stops its entry (SC-6).
  - Executed command calls fell from 22,371 to 4,192 in the sweep.
- The HUD overlay draws nothing (HUD-1..7). The medal box needs
  `hud_drawMessageBox` and `menuList_draw`; campaign state alone does not
  supply them.
- The combat round-3/4 refusals (CR3-1..7), the removed env-pass guesses
  (TEX) and the other audit refusals are as `codex/audit-fixes` left them.

## Playable integration wave 18 — perf, combat round 6, water ripples, PS2 import, exports — 2026-10-21

Start `c42ad2d`. Merged in order, one at a time:

| Branch | Tip | Merge | Notes |
|---|---|---|---|
| `codex/perf` (STATUS PARTIAL; every exact test unchanged) | `65974a0` | `2daf360` | Conflicts in `real_audio.gd`, `playable_fx.gd`; one fix (below) |
| `codex/combat-runtime-6` | `396a112` | `ef6d289` | Conflict in `COMBAT_RUNTIME.md` only |
| `codex/water-ripples` | `14ae473` | `fe01847` | Conflicts in `real_data.gd`, `real_data_scene.gd` |
| `codex/save-import` | `7948736` | `99cb6cf` | Clean |
| `codex/exports` | `a157fcc` | `b6a2621` | Clean; the three low audit findings fixed in `b5dab4c` |

Not merged (their fix jobs are still running): netplay, creature-blend,
combat-ctors, dialog, script-commands-2.

### Conflict resolution, against the wave-17 audit rules

- **perf / `real_audio.gd`.** perf freed the old mixer and built a new
  `ChampionsNativeAudio` on every reset, to release its ~100 MB copy of the
  global bank first. That undoes AUD-4 (one mixer per process; `g_soundFrame`,
  `g_drawFrame` and the duck words are process-lifetime). The merge keeps
  AUD-4's `end_session()`. `set_global_vat` still replaces the bank in place.
- **perf / `playable_fx.gd`.** Both kept: TEX-3's `light_constant` and perf's
  `world_lighting_held`.
- **perf, fixed in the merge.** The streamed display asked `cell_view` for
  constant-light colours at `LevelTextureMaterial.PLAY_LIGHT_BASE`, the fixed
  base 128 that TEX-3 removed (the constant no longer exists, so the stream
  would not have parsed). `cell_view` now leaves constant-light groups
  uncoloured for a negative base; the stream passes -1 and tags each group
  `level_constant_light` / `level_constant_base` -1, so
  `playable_fx.light_constant` applies the loaded level's base level and the
  texel × colour / 128 presentation, as for the full display. Real data:
  every streamed constant chunk is on its level base (130 of 130).
- **combat-runtime-6.** The code merged cleanly. `spawn_class` still refuses
  AntQueen, Skeleton and Soul (CAI-4; `wave12-combat-extensions` shows the
  refusal in the dev and exported runs). Round 6's `delete_self` hands a
  `DiseaseCloud_dtor` slot to `destroy()`, which still throws the audit's
  `DISEASE_CLOUD_DTOR_REFUSAL` (CR3). In `COMBAT_RUNTIME.md` the Holy Shield
  paragraph keeps the audit text (the round-4 plant, no fixture table), and
  the `world_findFreeSpot` and Round 6 sections are both kept.
- **water-ripples.** The level result keeps WAT-1's `water_tiles_refused` and
  adds the line, additive and level-flag fields. The scene runs `--wade`
  first, then the TEX-3 constant-light counts. A wading player still does not
  disturb the water: `Player_move`'s water immediates are not published, and
  no standing-disturb binding exists.
- Script host (SC-1..7) and the per-process `user://` fixture folders
  (`OS.get_process_id()`, `4e89ddb`) are untouched by these branches. A used
  refused answer still stops its entry (`scripts.refusal_stops` 39, as in
  wave 17).

### Exports audit lows (`b5dab4c`)

- **EXP-1.** `skills_b.hpp` suspends the `alloca` macro for the
  `Model::alloca` declaration only (`#pragma push_macro` / `pop_macro`).
  `skills_b.cpp` keeps its plain `#undef`. `skills_b_tests.cpp` gets the same
  translation-unit `#undef`: macOS `<alloca.h>` also defines `alloca`, and
  the test had relied on the header's leaked `#undef`. `zig c++
  x86_64-windows-gnu` compiles `skills_b.cpp`, the test, and a file that
  calls `alloca()` after including the header.
- **EXP-2.** The global `GameFolderEntry` autoload (a `node_added` hook on
  every node of every scene) is gone. `play_flow.gd` adds the "Your game
  folder…" action itself. `scene_play_flow_smoke` checks it (65 checks).
- **EXP-3.** `BUILDS.md` says one WORLD copy per level opened, removed when
  the scene exits.

### Verification (`work/playable-wave18/`)

- Native Release builds pass on arm64 and x86_64 with 0 compiler warnings (the existing
  ld duplicate-library note only). The four universal extension builds pass.
- CTest arm64 `-j3`: **211/211** on the first run, 677 s (`session-facing-exact`
  160 s, `player-exact` 134 s). New since wave 17:
  `combat-runtime-6-host`, `combat-runtime-6-exact` (6,830 checks) and
  `ps2-save` (328 checks).
- CTest x86_64 (Rosetta, `build/native-x64`): **211/211** on the first run,
  1,282 s, no timeouts (`session-facing-exact` 269 s, `player-exact` 180 s,
  `native-save-indexed` 175 s; machine load about 20 from other worktrees).
  The binaries are Mach-O x86_64.
- Real data: **111 / 111** loaded and played with scripts on and off, 0
  refused, 0 failed checks, an empty refusal list. Every deterministic count
  equals wave 17. See [REAL_DATA.md](REAL_DATA.md) for the changed display
  counts and the new water counts.
- Godot dev matrix **48/48** on the first run: wave 17's 45 plus
  `level_texture_stream_smoke` (27), `fx_shade_batch_smoke` (12) and
  `ps2_save_import_smoke` (20). Every other smoke has wave 17's check count,
  except `scene_play_flow` (64 → 65, the EXP-2 check). Full combat 2,620;
  FX 1,876; animation 1,809; play-short 1,341; level 1,643; textures 1,909;
  audio 2,578; creature-anim 32 / 25 / 231.
- **FULL_TRACE `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`**
  (21,990 bytes), unchanged. No round-6 object or spell exists in the live
  Full fight.
- Export `--export-release 'macOS universal'` → `build/exports/wave18/`.
  Export matrix **18/18** on the first run. The 36,000-tick 1280×720 Full run
  passed 108,826 checks with 0 failures and 63 captures. Exported headless
  2,620; 960×720 and 800×600 2,746 each. Every `FULL_TRACE` is `374e40c9…2035`.
  Both extensions are packaged byte-identical (x86_64 + arm64); SHA-256 in
  `work/playable-wave18/export-artifacts.json`. No `.gob`, `.lmp` or `.iso`
  member and no private path in the zip or app.
- Exported app on real data (60 ticks): 12 / 12, 12 / 12 with `--scripts`,
  and 3 / 3 arena levels with `--scripts`; 0 failures, 0 refusals.

### Expected changes (not regressions)

- **Drawn claimed pieces 52 → 14.** Only resident cells draw (perf). The
  claimed count (174) is unchanged.
- Display counts from streaming and batching: meshes 174,456 → 6,133,
  textured meshes 174,408 → 6,085, constant-light chunks 1,244 → 130 (all on
  the level base), world lighting calls 4,774,659 → 100,557. The full-decode
  peaks `display.max_level_surfaces` / `max_level_rgba_bytes` read 0, since a
  streamed display has no full decode.
- Sweep load time 815 s → 38 s summed over the four slices; static peak
  7.6 → 2.2 GB summed.

### Regressions kept

As wave 17 left them: no AntQueen, Skeleton or Soul in the Full fight; own
classes other than Gnaw and Skelight refuse to spawn; thrown potions 1 and 2
stop a Full session; no level or boss music without the music volume; the
level-script refusals (SC-4, SC-5, SC-6); the HUD overlay draws nothing.
Wave 18 adds: a wading player does not disturb water; round-6 objects and
spells refuse at their destructors; the PS2 import fills native state but no
live session consumes it.

## Playable integration wave 19 — script commands 2, netplay, creature blend — 2026-10-21

Start `0f3cafc`. Merged in order, one at a time:

| Branch | Tip | Merge | Notes |
|---|---|---|---|
| `codex/script-commands-2` (SC-8..18 fixed) | `30bc2a6` | `6df4a10` | Clean |
| `codex/netplay` (NET-1..5 fixed) | `c60759e` | `52133a3` | Conflict in `native/CMakeLists.txt` only |
| `codex/creature-blend` (BLEND-1..6 fixed) | `fb4f433` | `ee4d03a` | Conflicts in `real_data.gd`, `real_data_scene.gd`, `REAL_DATA.md` |

None was backed out. Not merged (still running): combat-ctors, dialog,
items, script-session, hud-overlay, menus.

### Conflict resolution, against the audit rules

- **script-commands-2** merged cleanly. A used refused answer still stops its
  entry: every new refusal (stub reads on `setMonsterVal`,
  `setCreatureParams`, `charmMonster`, `followPlayer`, the `select*` forms;
  callee refusals for a live charm target, an unsupplied music volume, an
  unknown fade level) goes through the same `refuse()`. `setCFlag` /
  `selectSetCFlag` still refuse on stubs. The host no longer reads the
  binder's zero `g_musicVolume` (AUD-1 kept); its `Playback` is host-private,
  so the one-mixer rule (AUD-4) is untouched.
- **netplay.** Both `CMakeLists.txt` sides are additive and both are kept
  (wave 18's `combat-free-spot` tests and `arena_modes_targets.cmake` include;
  netplay's test and counts-only level tool). The −100000 kill plane
  (`level_flow::LevelRow{}.kill_plane`) is written only by the netplay
  session's own world, not by the Full play session, so FULL_TRACE is
  unaffected. Trace record 72 bytes, save format v2.
- **creature-blend.** `real_data.gd` keeps wave 18's `stream_textures`, level
  flag word and water-line fields and adds the branch's `env.lmp` environment
  image and `environment` result key. `real_data_scene.gd` keeps the
  `creature.two_pass` / `creature.environment` counts inside the preview block
  and the wave-18 `--wade` loop after it.
- **Two-pass draws under perf's batching.** perf's streamed level display
  packs only order-independent batches into atlas pages;
  `level_texture_stream.cpp` `order_dependent()` returns true for every
  environment-pass material, so those groups keep their own node, chunk/batch
  slot and draw order. Creature two-pass models draw through one composite
  `ShaderMaterial` per mesh state in `real_creature.gd`, which perf does not
  batch, so the environment-then-base order inside the shader is unchanged.
  The sweep now counts it: `display.environment_groups` 4,797,
  `display.environment_in_atlas` **0** over 111 levels.
- AntQueen, Skeleton and Soul stay refused (`wave12-combat-extensions`, dev
  and exported). The per-run `user://` fixture folders are untouched.

### Verification (`work/playable-wave19/`)

- Native Release builds pass on arm64 and x86_64 with 0 compiler warnings
  (the existing ld duplicate-library note only). The four universal extension
  builds pass after each merge.
- CTest arm64 `-j3`: **213/213** on the first run, 1,101 s (`session-facing-exact`
  194 s, `player-exact` 174 s, `netplay` 154 s). New: `script-effects`, `netplay`.
- CTest x86_64 (Rosetta, `build/native-x64`, Mach-O x86_64): **213/213** on the
  first run, 1,620 s, no timeouts (`session-facing-exact` 306 s, `player-exact`
  218 s, `native-save-indexed` 209 s, `netplay` 205 s; machine load about 30).
- `champions-netplay-tests` (114 checks) prints the branch's four digests
  (`ff00f5fe…`, `851dedc8…`, `e00a5850…`, `cc5d9f83…`); arm64 and x86_64
  output is byte-identical.
- Real data: **111 / 111** with scripts on and off, 0 failed checks, empty
  refusal list. See [REAL_DATA.md](REAL_DATA.md) wave 19 for the count changes.
- Godot dev matrix **50/50** on the first run: wave 18's 48 plus
  `creature_blend_smoke` under GL Compatibility and Forward+ (14 checks each).
  Every other smoke has wave 18's check count.
- **FULL_TRACE `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`**
  (21,990 bytes), unchanged.
- Export `--export-release 'macOS universal'` → `build/exports/wave19/`
  (zip SHA-256 `9ddf4309…`). Export matrix **18/18**; the 36,000-tick
  1280×720 Full run passed 108,826 checks, 0 failures, 63 captures. Every
  exported FULL_TRACE is `374e40c9…2035`. Both extensions packaged
  byte-identical (x86_64 + arm64). No `.gob`, `.lmp` or `.iso` member and no
  private path in the zip or app.
- Exported app on real data (60 ticks): 12 / 12, 12 / 12 with `--scripts`,
  3 / 3 arena levels with `--scripts`; 0 failures. The 12-level runs now list
  BLEND-7 (`fx.runtime | material must consume vertex colors`, 9 runs).

### Regressions kept / open

As wave 18 left them, plus: BLEND-7 stays open (the FX renderer refuses the
per-surface `ShaderMaterial`s a mixed-pass creature preview gets; the fix
belongs in `native_fx_renderer.gd`). The five text setters, `startMusic` and
boss music stay refused in play (no string table port, no supplied music
volume). Netplay is an offline co-op simulation over the lockstep transport;
no Godot net scene.

## Playable integration wave 20 — script session, items, play HUD — 2026-10-21

Start `63c40ab`. Merged the three requested finished branches in order, one at a
time, with a local commit after each verified merge:

| Branch | Tip | Merge | Conflict resolution |
|---|---|---|---|
| `codex/script-session` | `b6b735b` | `3a3b261` | Kept wave-19 executed callee routing and refusal accounting alongside the offline seed, classified objects and one-player setup. |
| `codex/items` | `290b951` | `919b6e4` | Kept both host sets; one difficulty source is `script_items::DIFFICULTY_IMAGE` at host construction. |
| `codex/hud-overlay` | `fbffecb` | `ded5aab` | Kept PS2 import and the new play HUD in `play_flow.gd`. |

No other branch was merged. All commits are local. The private research checkout was not
touched. The owner’s disc copy was read only and contributed counts only.

### Script and presentation contracts

A refused answer still stops the script entry. `setCFlag` /
`selectSetCFlag` still refuse on placeholder objects, and item-command
partial refusals also stop. The offline play session opts into
`SeedPolicy::OfflineSession` for `getRand`; the host’s default is strict
`Exact`. Netplay has no script-host binding and keeps its own kill plane in its
own world. The one process audio mixer and the per-process `user://` fixture
folders remain. AntQueen, Skeleton and Soul constructors stay refused.

The HUD draws the documented status frame and health bar on all tested real
levels. Mana and pending overlays draw when supplied by a synthetic
observation; missing real-level state still refuses the other elements.
A rendered authored-texture view was inspected. No private-disc screenshot was
saved.

### Real data and Godot verification

- The four-slice 60-tick sweep passed **111 / 111** with scripts on and
  **111 / 111** with scripts off; zero failed checks and empty refusal lists.
  Scripted executed calls progress 4,208 → 8,472 → 17,128, `getRand`
  0 → 367 → 1,457, and refusal stops 39 → 35 → 16 as script-session then
  items let entries advance. HUD adds 111 checks (119 → 230), 111 health bars
  and 111 status frames. The exact key-by-key attribution is in the
  [wave-20 report](../work/loop-playable-integration-r20-report.md) and
  [REAL_DATA.md](REAL_DATA.md).
- The four universal GDExtension builds pass. Godot dev matrix **51 / 51**
  (wave 19’s 50 plus `play_hud_smoke`). The Full combat headless run passes
  2,621 checks with zero failures.
- **FULL_TRACE** remains
  `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`
  (21,990 bytes) in the dev Full combat run and the exported headless run.
- Export `--export-release 'macOS universal'` produced
  `build/exports/wave20/OpenChampions.zip` and `Open Champions.app`, with
  arm64 and x86_64 executable and extension slices. The ZIP has no `.gob`,
  `.lmp` or `.iso` members and the app has no private-validation path string.
  Headless exported-app real-data checks pass 12 / 12, 12 / 12 with scripts,
  and 3 / 3 arena levels with scripts. HUD health bar and frame draw on every
  tested level. BLEND-7 remains the wave-19 mixed-pass FX refusal, with no
  failed level checks.
- Export matrix **19 / 19**, including `play_hud_smoke`; the graphical
  36,000-tick Full flow passed **108,827 checks, zero failures**, with 63
  captures. All four exported `FULL_TRACE` records equal `374e40c9…2035`.
  ZIP SHA-256 is `e9f96e47…` and the packaged extensions match the release
  dylibs byte-for-byte.
- Native arm64 Release and x86_64 Release builds pass. Arm64 CTest
  finished **215/216** on the first full `-j 3` run; `player-exact` hit its
  240 s timeout during concurrent work and passed alone in 148.31 s, so all
  216 tests are verified with that retry recorded separately. x86_64 CTest
  under Rosetta passed **216/216** on the first `-j 3` run, 1,398.01 s,
  with no timeouts or failures. No compiler warnings were emitted; the
  linker’s existing duplicate-library notes remain.

### Remaining limits

The offline script generator has 15,577 unmodelled object-load draws, which
it counts; it does not claim the original clock seed. The real-level session
has no item tables for gives, and the HUD lacks observed mana, XP, level,
gold and other producers. BLEND-7 and the wave-18 refusals remain. Netplay is
an in-process offline co-op simulation, without a Godot net scene or sockets.

## Playable integration wave 21 — combat walk, constructors, dialog, menus — 2026-10-21

From `8bdd705`, merge commits `362c9c4` (combat-walk `07f2479`), `7371245`
(combat-ctors **`a0d7118` exactly**), `b1640a7` (dialog `01b6e8a`) and
`0548440` (menus **`815a8f3`**) were made in that order. No other branch was
merged. The combat-walk netplay ancestor was already in wave 19; the
chosen combat-ctors commit includes combat-runtime-6. Combat-ctors and menus share the
same early music-volume ancestor `04d2f1a`, so one researched implementation
is linked into each distinct native and extension target. Its `save_init`
default is 0.5. The script host was adjusted from an optional volume to that
raw default; existing refusals for placeholders, live charm and unsupported
callee states remain.

The Full fixture now builds the AntQueen in slot 4 at (-110, 110) and the
Soul in slot 6. The Skeleton remains outside slot 5 because its state 3 is
not executed in the pinned research. The Full trace is
**`4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**,
**31,622 bytes**, in dev headless and all four exported Full runs. It differs
from wave 20 because the Soul and AntQueen add actors, shared draws and state
changes, as the combat-ctors reports isolate. Combat-walk, dialog and menus
do not change it.

Dialog composes with wave-20 items and the offline script seed. A refused
command still stops its script; `setCFlag` on a placeholder still refuses.
The real-data dialog census now opens and ends **48/52** talks instead of
42/52: all six item-condition talks open, while four remain refused for a
missing quoted text key. The windowed scene opens and closes **24/24** talks
instead of 21/24, with 133 passed checks and zero failures. One real-level
script opens a talk in the 60-tick sweep.

Both 111-level sweeps pass **111/111**, zero failed checks. With scripts off,
all deterministic counts equal wave 20. With scripts on, dialog adds 111
attached text/font levels, 24 talkers, one open, one executed `startDialog`
instead of a refusal, and one extra `getv`; all other deterministic counts
match. The count-by-count and runtime-measure differences are in
[REAL_DATA.md](REAL_DATA.md) and the
[wave-21 report](../work/loop-playable-integration-r21-report.md).

Four universal GDExtension builds and the Godot dev matrix **53/53** pass.
Full headless has 2,620 checks, zero failures; its one-check drop from wave 20
is the researched music-default assertion replacing two refusal assertions.
The exported universal app has arm64 and x86_64 executable and extension
slices, no packaged game data, and matching release dylibs. Export matrix
**21/21** passes. The 1280×720 graphical Full flow reaches **36,000 ticks**,
passes **108,826 checks** with zero failures, saves and continues, and has
63 synthetic captures; the boot, main menu, inventory, and Full combat
captures were inspected. Exported real-data smoke passes 12/12
without scripts, 12/12 with scripts and 3/3 arena levels with scripts; the
HUD and one creature preview are present on each tested level. BLEND-7's
mixed-pass material refusal remains on nine of the 12 preview levels, with
zero failed checks.

The final native CTest suites passed **225/225 on arm64** and **225/225 under
Rosetta x86_64**. The host free-spot test was adjusted after the AntQueen's
new Full-fight search and a Skelight hit interrupted its player-only timing
check; the corrected test passed on both architectures. The disc was read only
and contributed counts, not committed content.
All commits are local; there was no push or PR. Original-client raster parity,
Windows/Linux exports, manual play and Internet netplay remain unverified.

## Playable integration wave 22 — text, FX, streaming, and portable exports — 2026-10-21

From wave 21 `ed63ccd`, the local merge commits are `99e53e3` (small-fixes
`b56bf10`), `c7d5107` (strings `6fdb048`), `5d7de5b` (audited blend-fx
`b297c44`), `b0d3322` (audited perf-2 `8ec62d5`), and `afc087f`
(audited win-linux `adf701c`), in that order. No other branch was merged;
creature-stats remains excluded after two MEDIUM audit findings. The BFX-1
nonzero lighting assertion, canonical build path mapping (WL-2), and accurate
private-marker verification wording (WL-1) were included.

Quest mission rows are the researched 1..5, with one difficulty source shared
by the quest and item code. The researched spell getters replace the invented
type values. Text commands install GTEXT and draw the help and bust overlays;
the help box owns confirm/use input while open, pauses the session, closes
after its 90-frame delay, records the 60-frame menu lockout, and allows the
script to continue. A refused command still stops its script, including
`setCFlag` on a placeholder. The Full fight still omits the unresearched
Skeleton state. Audio keeps one process-lifetime mixer and global VAT.

The final scripted and unscripted real-data sweeps each pass **111/111** levels,
**230 checks**, zero failures. The scripted stable counts equal the verified
two-branch baseline. Against wave 21, small-fixes removes nine row-0 bits
from both mission-available and mission-complete counts; strings resolves
eight help and 384 bust calls and lets two scripts continue. The only final
draw-counter variation is bust frames **30 → 34**, which follows display-frame
scheduling rather than fixed ready ticks. `perf-2` lowers the four-process
summed peak memory from about 2.20 GB to 1.53 GB in both modes; load/play
timings and audio delivery vary with concurrent load. The full per-key
attribution is in [REAL_DATA.md](REAL_DATA.md) and the
[wave-22 report](../work/loop-playable-integration-r22-report.md).

Four universal macOS GDExtension builds and Godot dev **53/53** pass. The
export matrix is **21/21**; 36,000 rendered Full ticks pass **108,826 checks**
with 63 synthetic captures. The dev and all exported Full runs keep
**FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**, **31,622 bytes**. The universal macOS ZIP at
`build/exports/wave22/OpenChampions.zip` has SHA-256
`38d2109f513dd55f502d620c7c854022e6b6ba83d606473dc28d2a28bfc3e8f2`;
both packaged release libraries match their builds byte for byte, and no
game data is packaged. Exported real-data smoke passes 12/12 without scripts,
12/12 with scripts, and 3/3 arena levels with scripts. All creature previews
draw; the nine prior BLEND-7 material refusals are gone. The windowed text
smoke passes 17 checks, including help/dialog input and pause/resume behavior.

Windows x64 and Linux x64 exports each contain exactly the player and two
matching libraries. Static checks pass for 273 embedded resources per package,
expected native entry symbols, no game-data suffix, and no checked private
marker. Target Windows/Linux execution remains unverified. Native arm64 CTest
passes **226/226**. The initial Rosetta x86_64 suite passed **225/226**;
`player-exact` hit its 240 s timeout during concurrent CTest work and passed
alone unchanged in 172.35 s, verifying all 226 tests with the initial timeout
recorded separately. `strings-exact` is the new test over wave 21. Synthetic
output was inspected, and private disc data was read only. All commits are
local; no push or PR.

## Playable integration wave 23 — session music and initial fade — 2026-10-21

From wave 22 `af9e3b5`, merge commit `317be8c` brings in only
`codex/session-music` tip `c703657`. The existing native
`music_volume.cpp` is the sole music-volume implementation, compiled once
into each distinct native and extension target. The play session supplies
the raw options word, or the documented 0.5 default, to the script host
before `preStart`. The first menu-theme request checks the researched music
gate. `hudFadeScreen` starts from the observed zero and the controller carries
its fade word when replacing the host. The Claude PASS audit's real-data
closed-gate error report was corrected, and a synthetic Godot smoke now
checks it. A separate controller starts its own fade word at zero; the
research did not exhaust indirect presentation owners.

Both four-slice, 60-ready-tick real-data sweeps still try, load and play
**111/111** levels, with **230 passed checks**, zero failures and an empty
refusal list, using separate `user://` caches. The scripted command counts
stay `startMusic` **0/0**, `setCFlag` **0 executed / 1 refused** and
`hudFadeScreen` **0/0**. The `setCFlag` placeholder refuses before music
handling; no sweep level reaches the other two calls. Every deterministic
count equals wave 22. The runtime and displayed-frame count changes are
recorded in [REAL_DATA.md](REAL_DATA.md).

The four universal extension builds pass, and the Godot dev matrix passes
**54/54**, including the new session-music smoke. Its later focused
closed-gate rerun passes **6 checks**. Dev headless Full retains
**FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**
at **31,622 bytes**. A refused command still stops its script. The one
process mixer, per-run user folders, and the Full fight's Skeleton exclusion
remain intact. The [wave-23 report](../work/loop-playable-integration-r23-report.md)
records the full native, export and real-data smoke results.

## Playable integration wave 24 — combat constructors, player state, wading and dialog gaps — 2026-10-22

From wave 23 `c86c539`, this wave merges only `codex/combat-ctors`
`08dbc72` (`0c59778`), `codex/player-state` `010eba3` (`274e1d5`),
`codex/wading` `94446c4` (`c67feef`) and `codex/dialog-gaps`
`fcfc7ac` (`2e57376`). The combat test conflict retains both sets of
tests. The player-state controller conflict retains wave 23's music and
fade carry as well as the new player binding; the script-command test keeps
the researched initial fade behavior. No other branch was merged.

The arm64 Release CTest passes **231/231**. Four universal macOS extension
builds pass, and the Godot dev matrix passes **54/54**. Dev Full headless
passes **2,620 checks** with zero failures, with **FULL_TRACE
`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**
at **36,402 bytes**. Relative to wave 23's `4da57055…78d9`, the Full
fixture now places a Skeleton in slot 5 beside the already-present
AntQueen in slot 4. The Skeleton's constructor and equipped-holder draws
advance the shared seed, later rolls shift, and each trace line includes
another actor pair. This is the authored fixture consequence documented in
[COMBAT_RUNTIME.md](COMBAT_RUNTIME.md), not an original-game placement
claim. The other three merges do not change this Full trace.

The first full native x86_64 CTest under Rosetta passed **228/231** while
the machine was heavily loaded; three timeout-only cases were retained as
failures in that log. With the original CTest definitions restored, those
three pass **3/3** on a serial rerun under lower load. Every x86_64 case
therefore has a passing execution; the initial timeouts and the Mac restart
during an intermediate timeout-only attempt remain in the wave-24 report.

Both read-only 111-level sweeps still try, load and play **111/111**, with
**230 checks** each, zero failures and no refusals. `codex/wading` adds
`water.wading_refused` **2,832** in both sweeps because the default scene
does not invent missing input and frame services. `codex/dialog-gaps` adds
`dialog.camera_steps` **126** and `dialog.camera_views` **1** in the
scripted sweep. All other deterministic counts match wave 23; runtime and
drawn-frame variations are listed in [REAL_DATA.md](REAL_DATA.md).

The universal macOS export is `build/exports/wave24/OpenChampions.zip`:
**8** members, no game-data members or private-path markers; its app and
both extension libraries contain `x86_64 arm64`. The exported app's
read-only 60-tick smoke passes **12/12** levels without scripts,
**12/12** with scripts and **3/3** arena levels, with zero failures and
empty refusal lists. The [wave-24 report](../work/loop-playable-integration-r24-report.md)
records the Rosetta and export-matrix results, including any isolated
retries.

The exported matrix has a passing run for all **22** entries. Full headless
and 960×720 rendered pass in the matrix; the 800×600 rendered entry passes
on its isolated rerun. The 1280×720 rendered **36,000-tick** run passes
on its third attempt with **108,826 checks**, zero failures, and the same
`FULL_TRACE`; an additional headless 36,000-tick run passes **108,700**
checks. The initial rendered 36,000-tick attempt stalled and the next
reached the endpoint but failed three subsequent Full checks; those
failed attempts remain recorded in the report.
