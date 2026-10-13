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
| `codex/port-messages` | `2b0ada423c4064a4a007e381924be42bfe4b19a4` |
| `codex/port-player` | `cea3fd20ad840a94591699fff42603a7a1d31c18` |
| `codex/port-monsters` | `99bb28f38a342f001d6cb7441fa368821670a757` |
| `codex/port-arena-monsters` | `6df161f7c3fd8266633c8eb1ad0321af2148b434` |
| `codex/port-natives` | `41a28013e587aee707ea1f8d39ea5bb196dba3e1` |
| `codex/port-skills-a` | `b32a66e49841c809a8eb65d21c18c0b14cd28501` |
| `codex/port-skills-b` | `212a9af97e9c811aceaf23775ba7b51f71a1c112` |
| `codex/port-skills-c` | `25604650ae9dd7010362c72211ac7304422a9512` |

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

## Final audit: MORE

The tested integration snapshot is green and committed. Active source branches
advanced again while the latest full suite ran. These tips are **not included**
in the passing snapshot and must be merged with --no-ff on the next run, followed
by Release configure/build and full CTest:

| Branch | Pending tip |
|---|---|
| codex/port-messages | c15c4a9de4238743d55291095547e270a4d8e048 |
| codex/port-monsters | fc3db05365eb78ba1537f6573da583dd792978c4 |
| codex/port-arena-monsters | ebc8af12ec9e989684f571529c6834bf4794b047 |
| codex/port-natives | 026f17459c64c9e99efe3f10a0d356173b4fbf36 |
| codex/port-skills-a | ff50a1dce857b7d1757231179efa6a06b26f72fa |
| codex/port-skills-b | 8df5e3fb288caabb744d26bca291afdc328298d9 |
| codex/port-skills-c | 8d436c3bc20a71b7129cce1557b88a15a2d6252a |

No merge was aborted and no human decision is currently required. The proposal
above describes the included skills versions, not these pending commits.
