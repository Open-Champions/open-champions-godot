# Local integration verification

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
