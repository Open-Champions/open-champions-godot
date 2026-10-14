# Local integration verification

Latest refresh (2026-10-14): tested snapshot `e117cd55`, full Release suite
**130/130 PASS**. Only `codex/port-skills-c` remains blocked by the unresolved
240-versus-1,800-second test timeout. See the final refresh section below.
Skills-b advanced again after the closing table was recorded; see the closing
note for that unmerged, untested tip.

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
worktrees under `/Users/syndicatemike/Projects/open-champions/worktrees`,
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
