STATUS: PASS. CAI13-1..7 are fixed and the kernel is wired into `mission2_actors` and `blood1_actors` following the r1 audit table. Gates on the mini:
- native CTest 263/263;
- strict Godot matrix 54/54;
- combat 2,645 checks, 0 failures;
- FULL_SAVE_LOAD 300/300;
- FULL_TRACE unchanged;
- journeys equal to round 1 at every step;
- enemy-fight HP totals equal to waves 39 and 41.

# Creature AI, missions 1–3 — client round 2 (2026-11-09)

Branch `codex/play-creature-ai`, from round 1 `cc60c8ff`. Audit `play-creature-ai-r1` (VERDICT FIX, 3 MEDIUM, 4 LOW). Research pin `t3/research-campaign-combat` `fbb2e1d`. Citation keys:
- "C:" `docs/creature-ai-m1-3.md`
- "M2:" `docs/monsters-2.md`
- "MOD:" `tools/models_creature_ai_m1_3.py`
- "REF:" `tools/ref_creature_ai_m1_3.py`
- "DT:" `tools/difftest_creature_ai_m1_3.py`

I followed `~/Projects/champions-re-wt/cursor-runs/mini-workflow.md`. Every build, test, Godot run and journey ran on the mini.

Commits:
- `50c25e48`: Part 1, CAI13-1..7.
- `24298dcc`: Part 2, wiring.
- The commit that adds this report.

## Part 1 — CAI13-1..7

- **CAI13-1 Froglock water.** `Context::water` (`FroglockWater`) is now a required Froglock input; a missing one refuses.
  - A blowgun in reach (`see(0,400,6000)`) requests **8** while the standing-in-water cflag is set, and 7 when it is clear (C:447-448, M2:4158-4159).
  - Helper `0x265754` fires when water was just entered (`+0x460 == 0` and the cflag is set) or when the water ahead just ended (`+0x462 != 0` and `+0x461 == 0`) (M2:4143-4145). It re-requests the current state:
    - in 1, after `moveToDest` (M2:4152-4153);
    - in 3, after `moveToTarget`, for both weapons (M2:4157, M2:4160);
    - in 0x10, before anything else (M2:4178-4179).
  - State 5 has no water branch (M2:4161-4162).
  - These rules are labelled **Observed**: the research walk was dry (MOD:1141-1142).
  - Refused input values: water bytes other than 0/1, because they are copies of booleans (M2:4139-4142).
  - `+0x461` is defined as the value the helper reads, after this tick's head sample. The end-of-run copies into `+0x460`/`+0x462` are documented as caller writes.
- **CAI13-2 hammer puffs.** `Context::object_flags` is required for Clockwork.
  - Flag 2 gates the hammer puff velocities (M2:3721-3723, MOD:738-742).
  - It also gates the dead burst itself (M2:3711, "When object flag 2 is clear"). The burst sets flag 2 (0x4003, M2:3716) for the rest of that tick.
- **CAI13-3 landings.** `set_state` throws `Unsupported` when the filtered state is neither a `SET_ENTRY` row (MOD:1163-1197) nor a `check_filter` landing 0/3/5/6/0x10/0x13/0x14 (DT:442-461).
  - The test asserts the refusal for WarWraith 8, Droid 9 and 1, Goblin 7/8, SeaMonster 7/8/9/0x17/0x18, Orc 7 and Warrior 9/10.
  - Filter landings without a row keep `entry_known = false`.
  - The hpp and doc wording is fixed.
  - Addition, same reasoning: every setState row ran on a rig that writes no variant field (DT:384-391, DT:410, DT:455). `SetContext::variant` is therefore refused when it is nonzero for Kobold, Orc, Goblin and Clockwork.
- **CAI13-4 citations.** Corrected to the actual fbb2e1d lines:

  | Item | Now cited |
  |---|---|
  | REF CREATURES | 19-42 |
  | RECORDS | 43-45 |
  | KEY_COUNTS | 49-54 |
  | CLASSES | 57-71 |
  | DMG | 73 |
  | `ask` | 88-92 |
  | `M` | 77-157 |
  | `hit_reach` | 119-121 |
  | MOD Goblin states | 1114 |
  | MOD Froglock states | 1141 |
  | MOD Kobold ally | 103 |
  | MOD Froglock holder | 890 |

- **CAI13-5 variants.** Values outside the walked set refuse:
  - Kobold `+0x3cc` 0–2 (MOD:152);
  - Orc `+0x478` 0–5 (MOD:1111);
  - Goblin `+0x3d4` 0–5 (MOD:1115);
  - Clockwork `+0x3c8` 0/1 (MOD:1125-1127);
  - Froglock holder 0x12/0x10 (MOD:1142).
- **CAI13-6.** The check is now `t.draws == 1u`.
- **CAI13-7.** `creature_ai_m1_3.{hpp,cpp}` are in `tools/check_simulation_boundary.py`, so the census is now 84 files, and a new unit test is added.
  - The census forbids host `float`/`double`, so event arguments are now integers × `ARG_SCALE` (10000).
  - The research compares its float arguments rounded to four decimals (`creature_ai_rig.py` `f()`, DT:80-89). The knock-back scales 0.4/0.1/0.2 are therefore held to that precision, not as guessed IEEE bits.
  - Mission 2 measured Clockwork's 0.4 as `0x3ecccccc`, while `raw(0.4f)` in r1 was `0x3ecccccd`.
  - The 24,206 vectors compare unchanged.

## Part 2 — wiring, following the audit table

- **mission2.**
  - **Droid fuse.**
    - `droid_fuse_due` (C:416-417, MOD:752-753) decides it, and it is requested through slot 27 (`0x27c0a0`), as in campaign_fight.
    - `set_state(0x17)` runs the filter service and the kernel entry, then stores **0x17** (MOD:1189).
    - Still refused, with no clip, slot-28 or loot call: the fuse sound, object flag 0x4000, the explode clip and the Droid_setState call order (M2:1502-1503 are Observed only; C:151-153 checks call membership, not sequence).
  - **Live state 9.**
    - Cutter Clockwork and IronGuardian `set_state(9)` apply the executed entries: Clockwork `+0xf4 = +0x4f8 = clock` (MOD:1184), IronGuardian `+0x3c8 = 0` (MOD:1190). Then they store 9 and refuse the clip start and the rest of setState.
    - The hammer (variant 1), Droid 9 and requests on a dead actor refuse.
    - A filter refusal or redirect refuses.
  - **Every other non-death request** refuses with the old message, before any service call.
- **blood1.**
  - **Targeted state 7** runs the kernel's state body from `Tick::body`:
    - Kobold variants: ev&1 → flags 0x80001, ev&2 → flags 1, clip over → 5 (C:267-268, MOD:109-117). A new `TickInput::second_event` is required for this branch.
    - WarWraith drain (C:289-292, MOD:194-204): a new `cold_weapons` service (skill +0x5c), then `drain_at_reach(0x404, value)`, then turn plus the z-follow (M2:6149-6150) and the clip-over → 3.
    - **A landed drain (nonzero result) refuses.** The max HP / 15 heal (M2:6165-6167) is Observed but was not compared (C:291-292). M2 gives the factor only as "x 0.0667", so it is not ported and it is not skipped silently.
    - A missing ColdWeapons service refuses before any effect.
  - **Live plain-Kobold `set_state(8)`** stores `+0x3c8 = 8` and the state (MOD:1165), then refuses the clip start.
    - Tough and shaman refuse (variant gate).
    - WarWraith 8 refuses in the kernel (CAI13-3).
    - Post-death requests refuse.
    - A global filter refusal returns without effect, like death. A redirect refuses.
- **CMake.** The kernel library moved to `native/creature_ai_m1_3_library.cmake`, which is included from `simulation_targets.cmake`, `campaign_fight_targets.cmake` and the test file. The native tree and the GDExtension each define it once.
- **Live effect.** In `campaign_fight` every mission-2 AI tick refuses at `Creature_stepAnim` (`campaign_fight.cpp:649-650`) before the fuse test. The World requests only 0x10 and death states through slot 27 (`combat_runtime_monsters.cpp:181,192`, creature_hp). Those paths take the unchanged refusal with no new call. `blood1_actors` is not constructed by the play session. The gates confirm no change.

### Changed pre-existing assertions

Old lines are at `cc60c8ff`; new lines are at `24298dcc`.

| file:line (old → new) | old | new | superseded by |
|---|---|---|---|
| `native/tests/mission2_actors_tests.cpp:306` → `:315` | `refused(tick)`: fuse entry unexecuted | still `refused(tick)`. The fixture now routes slot 27 to `set_state` (`:306-314`), and the refusal is the clip, sound and order remainder | C:416-417, MOD:752-753, MOD:1189 |
| `native/tests/mission2_actors_tests.cpp:307` → `:320` | `+0x232 == 0` | `+0x232 == 0x17`, plus no `0x16cfe0`/`0x10beb8`/`0x10f2c8` call (`:321-324`) | MOD:1189 (landing), C:416-417 |
| `native/tests/blood1_actors_tests.cpp:355` → `:363-408` | `refuses(tick)`: "state7 targeted branch remains Unknown" | the targeted tick runs (`calls` = head, settle/turn or turn/grid, no transition). The refusal remains only when `second_event` is missing (Kobold), on a landed drain, and with no ColdWeapons service | C:267-268, MOD:109-117; C:289-292, MOD:194-204; heal refused per M2:6165-6167 |
| `native/tests/creature_ai_m1_3_tests.cpp:196` → `:212` | `knockback == f32(0.4f)` (IEEE bits) | `knockback == U(0.4)` (4000 ten-thousandths) | C:77-80 at the DT:80-89 four-decimal precision (CAI13-7) |
| `creature_ai_m1_3_tests.cpp:211-212` → `:227-228` | `float(args[0]) == knock`, `args == vector<double>{0}` | `args[0] == U(knock)`, `args == vector<int64_t>{0}` | the same, CAI13-7 representation |
| `creature_ai_m1_3_tests.cpp:257, :395, :408, :578, :581` → `:273, :411, :424, :594, :597` | `A::Event{name, {doubles}}` | `E(name, {same values})` (scaled) | the same values, CAI13-7 representation |
| `creature_ai_m1_3_tests.cpp:295` → `:311` | `args[0] == 240` | `args[0] == U(240)` | the same, CAI13-7 |
| `creature_ai_m1_3_tests.cpp:151` → `:167` (vector parse) | strtod value | `U(strtod)`; rows also state the dry water and the clear flags (`:157-158`) | MOD:1141-1142 (dry walk); flags clear at rig start |
| `creature_ai_m1_3_tests.cpp:343` → `:359` | `draws == (p.below ? 1u : 1u)` | `draws == 1u` | CAI13-6 |
| `tools/test_core_simulation_boundary.py:39, :45, :56, :193` | `len(paths) == 82` | `== 84` | CAI13-7 (audit process item; no research line) |

Kept unchanged, as the table requires:
- `mission2_actors_tests.cpp:241` (dead `set_state(9)` refused);
- `blood1_actors_tests.cpp:419` (dead `set_state(8)` refused).

Not assertions:
- the test `base()` helper now states the probe's fixed inputs (Froglock holder 0x12, dry water, flags 0);
- citation comments at `:70` and `:94-95`.

New tests:
- kernel: `froglock_water`, `clockwork_flag2`, `unexecuted_landings`, `variants`, `hooks` (51,871 checks, up from 51,804);
- mission2: `executed_entries` (live cutter/IronGuardian 9, filter refusal, hammer/Droid 9, other states refuse before any call);
- blood1: targeted-7 Kobold hits, drain miss/landed/no-service, live Kobold `set_state(8)` with the global refusal, the stun redirect, and tough/shaman/WarWraith 8 refusals;
- census: `test_creature_ai_kernel_is_in_census_and_rejects_host_floats`.

Mutation check on the mini copy. All 7 mutants were killed:
- the landed-drain refusal removed;
- the fuse store removed;
- the water 8 → 7;
- the flag-2 puff gate removed;
- the state-1 water re-request removed;
- the CAI13-3 landing gate removed;
- the Kobold variant gate removed.

## Gates (mini)

Evidence is in ignored `work/play-creature-ai-r2/`; the mini home path is rewritten to `~` in the copy. The scripts are `gates.sh` and `all.sh`. There is one `build/`, and `-j 4` / `ctest -j 3`. `TMPDIR` points inside the worktree (`local/tmp`), so the Python tests' temporary trees stay in `~/malik-champions`.

**Sync.** I used `rsync -rlp --checksum --delete`, excluding `.git`, `build`, `.godot`, `godot/bin`, `work` and `local`. Only this round's files differed, so the mini copy was at `cc60c8ff`. Every changed file was touched after both syncs.

**Disk.** 39 GB free at the start, 34–45 GB during the run; it never fell below 30 GB.

- **Release build: PASS.** Configure and full build took 155 s; the Part 2 incremental build took 11 s. There were 0 compiler warnings, only the existing duplicate-library linker notes.
- **GDExtensions: PASS**, 503 s. All five libraries are `x86_64 arm64`. The warnings are the same two as wave 39 (`level_texture.cpp:388`, `ui_hud_round3_service.cpp:53`) plus the same linker note.
- **Focused tests: 10/10 PASS:**
  - `creature-ai-m1-3`, 51,871 checks;
  - `mission2-actors`;
  - `blood1-actors`;
  - `simulation-boundary`;
  - `core-trace-integrity`;
  - `campaign-fight-adopt`;
  - `campaign-fight-boundaries`;
  - `player-death-flow`;
  - `pickups`;
  - `campaign-rewards`.
- **Full CTest `-j 3`: PASS, 263/263**, 710 s.
  - The first full run (1,082 s) had 262/263. `blood1-actors` aborted on "landed drain refuses".
  - Cause: a stale object from the mutation check. The restored `blood1_actors.cpp` had an mtime 0.35 s after the mutant's `.o` in the same second, so make did not rebuild it. The library lacked the refusal string.
  - After a touch and rebuild the string is present and the test passes. The whole suite was then rerun clean.
  - The GDExtension builds had compiled from the restored sources in their own fresh build directories. The first run's log is kept as `ctest-run1-stale-blood1.out`.
- **Godot import:** full `--import`, exit 0, 0 error lines.
- **Strict Godot matrix: 54/54 PASS**, 160 s. `check-log` found 0 failing logs out of 105.
- **Combat: 2,645 checks, 0 failures**, the same as waves 39 and 41.
- **FULL_SAVE_LOAD: 300/300.** Continued and uninterrupted presentation are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, unchanged.
- **FULL_TRACE: unchanged**, 36,402 bytes, sha256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.

## Journeys vs waves 39 and 41

The tool is unchanged, run headless then rendered, with the disc read-only. Disc files: 994 before, 994 after. Wrapper exit 1 is the retained death-reload refusal, as in waves 39 and 41 and round 1. The output is in `work/play-creature-ai-r2/journey-compare.txt`; `fps`, `memory_bytes` and `swing_wall_msec` are ignored.

| Baseline | Variant | Steps | Step/result pairs equal | Other step-field differences | Refusal kinds |
|---|---|---|---|---|---|
| wave 39 | headless | 80/80 | 80 | none | 8 (w39 8) |
| wave 39 | rendered | 81/81 | 81 | none | 8 (w39 6) |
| wave 41 | headless | 80/80 | 80 | step 20 journal refusal text | 8 (w41 8) |
| wave 41 | rendered | 81/81 | 81 | step 20 journal refusal text | 8 (w41 8) |
| round 1 | both | 80/81 | all | none | identical entries |

- **`enemy_fight` HP totals** are identical in all five runs: 31804.746 → 31588.280; target 214.482 → −1.984; player 32.8 → 43.4.
- **Wave 39 refusal differences** are only the hub-action retry count, the same as round 1: headless `count=1` → `2`; rendered `count=3` plus its `tick: dialog confirm:` echo. Round 1 already explained this as frame timing.
- **Wave 41 differences** are the step-20 `journal_after_dialog` refusal wording and the hub retry count. The wording comes from wave 41's newer quest-log work, which this branch, based on wave 39, does not contain.
- **Against round 1** the refusal entries are identical. The wiring changed nothing live, as expected from the code reading above.

## Remaining

- Kernel `set_state` for nonzero variants (tough/shaman Kobold, Orc/Goblin roles 1–5, the hammer) is now refused. The research would need setState rows run with those variants. Example: the Orc shaman's 0x17 deadline, which the run reads from `+0x47c` (C:329-330).
- The Froglock setState rig answered the holder hand-1 part through a recording stub, so its rows are not tied to 0x12/0x10. This is not gated, and is noted for the research.
- The WarWraith drain heal (M2:6165-6167) needs its exact factor and cap compared before it can be ported.
- Clip starts inside the class setState functions for 9, 0x17 and Kobold 8 stay refused, together with the setState call order.
- The seven classes without a client actor still need constructors, stats and animation services.

No push, no PR, nothing kept in `/tmp`, nothing installed, and no process killed by name. The mini's `build/` was deleted after verification.
