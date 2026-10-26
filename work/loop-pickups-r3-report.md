STATUS: PASS — legitimate pickups restored; native and Godot journey gates pass.

# Pickup input recovery, round 3

## Scope and root cause

Started from `codex/play-pickups` at `7314e259`. Only this worktree was
modified. No pushes, PRs, game-data changes or sibling-worktree edits.

Observed: the baseline Godot journey produced a real drop, walked to it,
and left inventory units at **10 → 10**. A temporary diagnostic confirmed
that help, dialog, pause and stop gates were inactive; it was removed before
committing. The baseline recorded prompts but no pickup recipients, and no
Chest/save/notification refusal on the pickup path.

The root cause is input initialization, not an overbroad PP2 refusal.
`Fight::Impl::setup_player` copied the executable initializer at `0x41b9c0`
into the live use-binding word. A read-only scalar check confirmed that this
initializer is zero. `Fight::request_use` accepted the input, but the next
tick ORed that zero binding into the pressed word. `Player_interact` could
therefore display prompts without ever reaching slot 14. This is a runtime
configuration table, not a static executable default. Existing archived
pre-PP2 journey results also recorded 10 → 10; attribution of this defect to
the PP2 commits is not established.

## Evidence and fix

- `t3/research-options-menu` `603b2b72ea10d954c763a1412b803934da90179f`,
  `docs/options-menu.md`, Applying values and persistence: executed/observed
  `config_setDefaults` at `0x1e6238` supplies nine masks per controller;
  the fourth mask is `0x80` in both supported schemes. Runtime commit copies
  these rows to the simulation's per-slot button tables.
- `t3/research-saves` `8d43286b9ca85227bc9782dbcf9f4e785b226053`,
  `docs/save-format.md`, config defaults: independently pins those rows.
- `t3/research-pickups` `9911a6b61c10c11ce861a472d5be7e3197d4ae40`,
  `tools/ee_tests.py`, `BUTTON_TABLES` and `commit_options`: the fourth row
  word maps to `g_btnUse` at `0x41b9c0`; slots have word stride 4.
- `t3/research-loot-drop` `77c2f70073e037f5b160e295eab9d803340ee6d1`,
  `docs/loot-drop.md` section 5: status 1 tests the remappable use binding
  against pressed input; prompt-only and held-only calls do not activate.
  The K1 gate and all interaction geometry/capacity rules remain unchanged.

Added `World::Button::Use` at the end of the existing enum, preserving all
existing button indices. Its table entry is the evidenced use-binding
address. The campaign binds the evidenced default through `SessionAdapter`,
beside its existing attack/block bindings, and no longer overwrites it with
the executable's uninitialized word during player creation. Subsequent use
requests and interactions still consult the live binding, not a hardcoded
pressed mask. This is a bounded default binding, not a port of the complete
controller-remapping screen.

The new campaign-construction regression failed before the fix (check 222)
and passes afterward. Manual interaction tests additionally exercise the
adapter binding, remapping away from the old mask, and zero-mask nonactivation.
Original prompt, held-input, display-only, range and height assertions are
retained. The playthrough script, Python runner and their assertions are
byte-for-byte unchanged from the task's starting commit.

PP2 production refusal code is unchanged. Native regressions still cover
Chest owner retirement/reuse; action/trap refusal before mutation;
inventory/equipment and changed-world save refusal; pending Gold through
full credit; and real Host-backed notification Refused/Fault propagation.

## Gates

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: PASS.
- Relevant `ctest --test-dir build/native -j 3 --output-on-failure` selection:
  **12/12 PASS**, final run 15.73 seconds. Includes pickups, campaign rewards,
  campaign adoption, script commands and command-measure checks, script items,
  item drops, PS2 save, player host and combat runtime host/round-2 host.
  Direct pickup executable: **225 checks PASS**.
- Exact replay gates `combat-runtime-exact`, `combat-runtime-2-exact`, and
  `player-exact`: **3/3 PASS** with `-j 3`, 177.21 seconds, no retries.
- `cmake --build build/godot-extension -j 4`: PASS. Linker emits duplicate
  static-library warnings; no build failure.
- `python3 -m unittest discover -s tools -p test_run_loop_playthrough.py`:
  **6/6 PASS**, unchanged gate tests.
- Godot `script_commands_smoke.gd` and `inventory_icons_smoke.gd`: PASS,
  exit 0, no reported test failures.
- Godot `scene_play_flow_smoke.gd`: exit 0, **65 checks / 0 failures**;
  its component fixture also logs two `loaded` property script errors during
  cleanup. This is not a clean error-free smoke; no unrelated fixture repair
  was made.
- Final command (external inputs represented by environment variables):
  `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/pickups/r3-final`:
  **PASS, runner exit 0**. Both headless and rendered variants exit 0 with
  `pickup_verified: true`, 26 steps each, and inventory **10 → 11**.
  Each produces a real drop, walks 20 ticks to it and picks it up; each
  `enemy_fight` passes with no missing fight steps. The unchanged journey
  retains its nine diagnostic combat retry placements; no placement or
  inventory injection was added to the pickup walk.
- Both variants' `pickup_save_preflight` refuses as expected and preserves
  existing files. The subsequent cold-load check confirms no lossy save was
  created. These expected refusals are not bypassed to make the runner pass.
- Private input file count remains **994 before / 994 after**. Inputs were
  read-only throughout; no game data or runtime screenshots are committed.
- Rendered variant writes 13 screenshots. Inspected
  `r3-final/rendered/05-fight-attempt.png`: the live client displays one loot
  item picked up, consistent with native inventory evidence. This is not
  ground-item artwork/animation parity: the existing
  `rendered_pickup_verified` field remains false. Both journey logs retain an
  ObjectDB-at-exit leak warning; neither has a script-error/test failure.

Logs and private runtime outputs remain ignored under `work/pickups/`:
`r3-regression-before.log`, `r3-configure.log`, `r3-build.log`, `r3-ctest.log`,
`r3-ctest-exact.log`, `r3-extension-build.log`, `r3-godot/`,
`r3-playthrough.log`, `r3-final/`.

## Local commits

- `a77a97a9`: initialize campaign use binding and add regression/provenance.
- `5fc6ecb6`: retain original range assertions and cover zero-use refusal.

## Boundaries

Native passing tests do not establish original-client visual parity or a
complete original campaign. Unsupported persistence, Gold sources, Chest
actions/traps and other unported services remain refused. The bounded
headless/rendered journey gates pass; there are no remaining blockers to
the requested pickup recovery. Full saved-inventory reload, original visual
parity and the unported systems are not claimed.

## PP2F-1 full CTest (recorded by the orchestrator, 2026-10-26)

Tests committed in `c87f08dc` and `4f61dfca`. The full `ctest --test-dir build/native -j 3 --output-on-failure` run happened while machine load was ~290: **232/242 passed**, 10653.13 s (log `work/pickups/pp2f-full-ctest.log`). Eight failures were timeouts; `session-anim-cache-transport` and `session-facing-exact` were reported Failed. All 10 were rerun at low load with `-j2` (log `work/pickups/pp2f-rerun-failed.log`): **10/10 passed**, 609.18 s. Guard-removal verification and the PP2F-1 playthrough/Godot results are still to be written up by the next job.
