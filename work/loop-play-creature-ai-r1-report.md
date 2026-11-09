STATUS: PASS — decision kernel ported and tested, not yet wired into the live actors. Gates held: native CTest 263/263 (262 + the new `creature-ai-m1-3`), strict Godot matrix 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged. Journeys match wave 39 at every step in both variants. The only difference is the frame-timing hub-action retry count.

# Creature AI, missions 1–3 — client round 1 (2026-11-09)

Branch `codex/play-creature-ai`, from wave 39 VERIFIED `f9176248`. Research pin `t3/research-campaign-combat` `fbb2e1d`: `docs/creature-ai-m1-3.md` ("C:"), `docs/monsters-2.md` ("M2:"), `tools/models_creature_ai_m1_3.py` ("MOD:"). Audits `research-creature-ai-m1-3`, `-fix` and `-fix2`: PASS with 4 LOW. Followed `~/Projects/champions-re-wt/cursor-runs/mini-workflow.md`. Every build, test, Godot run and journey ran on the mini.

## What the client did before

- `blood1_actors` (Kobold family and WarWraith) is an isolated runtime that the play session does not construct. It refuses state 3 with a target, targeted state 7, every state except 0/3/6/7/0x14, non-death `set_state`, and WarWraith fades other than 100 in 0x14.
- `mission2_actors` (cutter, hammer, Droid, IronGuardian) runs live in `campaign_fight` with contained refusals. It refuses the state-0 sight/home transitions, state 3 with a target, the Droid fuse request, every non-death `set_state`, and the Droid spawn.
- Orc, Goblin, Ogre, Warrior, Froglock, Amphian and SeaMonster had no client class at all.
- Combat already ran `hitTargetAtReach` (`combat_runtime_monsters.cpp:117-137`) as a Euclidean distance < 60 on x/y/z with an object-flag-8 skip, which is the research rule.

## What this round adds

The new module `creature_ai_m1_3` (`native/include/champions/creature_ai_m1_3.hpp`, `native/src/creature_ai_m1_3.cpp`) is described in `docs/CREATURE_AI_M1_3.md`. It is a per-tick decision kernel for the twelve classes. The caller answers each query, and the kernel emits the research's ordered events and the own-generator state. Every rule cites research path:line in the code and in the test:

- **State machines** for all 12 classes (C:253-479), with the head order and per-class knock-back (C:73-82).
- **Early exits:** `Creature_run` nonzero and `updateActive` zero, in every walked state (C:66-73).
- **Thresholds:**
  - ally counts (`> 2`, `> 1`, `> 0`, `≥ 1`, `< 2`, `== 0`) and the box count `< 3`;
  - the `+0xf4` deadline, compared strictly against the clock;
  - the clock-compared fields: WarWraith `+0x3cc`, Orc shaman `+0x47c`, goblin shaman `+0x3d8`, Droid `+0x3cc < clock`, and IronGuardian `+0x3cc` plus its counter `+ 1 > 120`.
- **Slot picks:** slot words 2/20/21/23/24/25 and HomeRange 1000/600/3000 (C:86-106, M2:6202, M2:1355).
- **3-D hit reach:** `hit_at_reach` (C:166-179) uses the ported `cos16`/`sin16`. No offset is added.
- **Amphian block:** it blocks only when HP is strictly below half of the maximum, and makes that draw only below half (C:459-465).
- **WarWraith signed fade:** a fade ≥ 99 turns; 100–127 deletes in state 0x14; 128 and above act as negative (C:279-298, M2:6128, M2:6277-6279).
- **Orc shaman:** the extra `see(0, 6000, 0x7fff)` turn in state 0x17 (C:329-332, M2:5492-5494).
- **IronGuardian:** it turns in states 7/8 only when the alive-player count is exactly 1 (C:432-435, M2:1392).
- **Rare branches:** the own-draw `% 30`, `% 60`, `% 5`, `% 3`, `& 3` and `& 1` tests.
- **`set_state`:**
  - the filterState redirects (C:115-122) and the no-case → 0 lists (C:124-127);
  - the executed entry table (MOD:1164-1196), with timers as ranges and the own draw as Observed;
  - SeaMonster's 0x10 writes no `+0x318`;
  - Droid's 0x14/0x17 call neither loot function (C:131-153).
- **Level records:** the per-level creature counts (807 records), the factory pick by type, and the four property-key counts (C:28-60), by name and count only.

## Respected LOWs and Unknowns

- **The ±60 offset and the 0.00001 figure are Inferred.** The kernel adds no offset. The test asserts only the executed rows. At facing 0, +60.0 hits and −60.0 misses; both hold with the client's own `cos16`/`sin16`.
- **The WarWraith HP load is scoped to walked paths.** The drain heal write is not ported, because the research did not compare it.
- **The shifted monsters-2 citations were re-checked at `fbb2e1d`:**
  - `:6128` fade range ("signed, 0–100");
  - `:6202` HomeRange 600;
  - `:6178` one wraith per victim;
  - `:5492-5494` and `:6277-6279` the dated corrections;
  - `:1355` and `:1392`, which come before the insertion and are unshifted.
- **Still refused:**
  - the Kobold 9 body and entry;
  - the Goblin 7/8 and Froglock 7/8 bodies;
  - any state the research did not walk;
  - unplaced level types.
- **Left out of the kernel:**
  - the Cazic, Critter and Catapult runs;
  - Droid spawning;
  - missile cadence;
  - slot 20/21 overrides by execution;
  - **HP rules inside the stubbed base `Creature_run`/`processDamage`**: the kernel only emits those calls and decides nothing inside them.
- **New finding:** C:131 says entering 0x10 stores `+0x318 = 60` "for all classes", but the executed entry table has no WarWraith 0x10 row. The kernel therefore reports `entry_known = false` for WarWraith 0x10 and assumes no write.

## Why the live actors are not rewired this round

Existing tests pin refusals that this research now resolves:

- `native/tests/mission2_actors_tests.cpp:241`: Clockwork `set_state(9)`;
- `:306`: the Droid fuse request at `+0x3cc < clock`;
- `native/tests/blood1_actors_tests.cpp:355`: targeted WarWraith state 7;
- `:419`: `set_state(8)`.

Routing the runtimes through the kernel would flip these assertions. The rule is to only add to existing tests, so this round leaves `blood1_actors`, `mission2_actors` and `campaign_fight` byte-identical. Wiring them needs approval to change those four assertions. Because nothing live changed, the combat gate and FULL_TRACE could not move, and they did not.

## Tests

- `creature-ai-m1-3`: 51,804 checks, synthetic only. It runs through `tools/test_creature_ai_m1_3_vectors.py`, which checks the manifest hashes and expands the fixture into the build directory.
  - Focused, cited checks cover each goal item: exits, head order and slots, thresholds, the 25 hitAtReach rows, the Amphian HP grid and the HP independence of every other class and state, the fade 98–255 range, the shaman sight, alive counts 0–4, rare branches, filter and entries, records, and Unknown refusals.
  - It also compares all **24,206 answer paths** against the research models on a sample of the research's own synthetic grid. The fixture is `native/tests/vectors/creature_ai_m1_3.tsv.gz` plus a `.json` manifest, generated on the mini by `tools/generate_creature_ai_m1_3_vectors.py` from the mini research worktree at `fbb2e1d`. It reads no executable, level or disc. Regenerating it gives a byte-identical file.
- **Mutation check** (mini copy only, restored by checksum afterwards). Each mutant fails the test:
  - `== 1` → `>= 1` (IronGuardian);
  - `lt` → `le` (Amphian);
  - fade read unsigned;
  - Orc recruit `> 1` → `> 0`;
  - shaman far-sight turn removed;
  - delete at fade 99.

## Gates (mini)

Evidence is under ignored `work/play-creature-ai/` (copies have the mini home path rewritten to `~`). The commands are in `work/play-creature-ai/gates.sh` and `all.sh`, run as one sequential chain with one `build/` and a disk check before each stage.

The mini no longer had `playable-integration-w39`, so the tree was seeded from the mini's `playable-integration-w40` copy, excluding `build/`, `.godot` and `godot/bin`. It was then made identical to this worktree with `rsync -rlp --checksum --delete` (excluding `.git`, `build`, `.godot`, `godot/bin` and `work`), and the changed files were touched.

Disk:
- 34 GB free at start.
- It dropped to 27–28 GB after the baseline build because of other jobs. I waited inside the turn until it was back above 30 GB (35 GB) before the next build.
- 34–40 GB during the gates.

- **Release build `-j 4`: PASS.** The baseline full build of the seeded tree took 527 s. The incremental build with this round took 38 s, with 0 compiler warnings.
- **GDExtensions: PASS**, 959 s, `--parallel 4`. All five libraries are `x86_64 arm64`. The warnings are the same two as wave 39 (`level_texture.cpp:388`, `ui_hud_round3_service.cpp:53`) plus the same duplicate-library linker note.
- **Focused test:** `creature-ai-m1-3` passed, 51,804 checks, 0.14 s. `blood1-actors` and `mission2-actors` also passed.
- **Full CTest `-j 3`: PASS, 263/263**, 995 s, with no timeouts and no reruns.
- **Godot import:** full `--import`, exit 0, 0 error lines.
- **Strict Godot matrix: 54/54 PASS**, 149 s. `check-log` found 0 failing logs out of 105.
- **Combat: 2,645 checks, 0 failures**, the same as wave 39.
- **FULL_SAVE_LOAD: 300/300.** Continued and uninterrupted presentation are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, the same as wave 39.
- **FULL_TRACE: unchanged**, 36,402 bytes, sha256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless.

## Journeys vs wave 39

The journey tool is unchanged, run serially headless then rendered, with the disc read-only. Disc files: 994 before, 994 after. Wrapper exit 1 is the retained death-reload refusal, as in wave 39. The comparison script is `work/play-creature-ai/compare_journeys.py`, and its output is in `journey-compare.txt`. It ignores `fps`, `memory_bytes` and `swing_wall_msec`.

| Variant | Steps | Step/result pairs equal | Other field differences | Pickup | Refusal kinds |
| --- | --- | --- | --- | --- | --- |
| Headless | 80 / 80 | 80 of 80 | none | 10→11 | 8 (w39 8) |
| Rendered | 81 / 81 | 81 of 81 | none | 10→11 | **8** (w39 6) |

The `enemy_fight` HP totals are identical: 31804.746 → 31588.280.

The only refusal differences are the hub-action retry count:
- headless `count=1` → `2`;
- rendered: the pair returns at `count=3`, together with its `tick: dialog confirm:` echo.

Wave 39 explained this as frame timing. Faster frames give more retries:
- the `dialog_choice_close` fps was 145 headless / 149 rendered here, against 91 / 34 in wave 39;
- wave 38, at 145 / 170 fps, showed the same counts 2 / 3;
- `choices_taken` is 3 in every run.

This round touched none of the dialog, quest or session code.

## Remaining

- Wiring the kernel into `blood1_actors` and `mission2_actors` needs approval to change the four pinned refusal assertions above.
- The seven classes without a client actor need constructors, stats and animation services, which this research does not cover.
- The kernel is not in the `check_simulation_boundary.py` census. Adding it would change that test's file count.

No push, no PR, nothing kept in `/tmp`, nothing installed, and no process killed by name. The mini's `build/` was deleted after verification.
