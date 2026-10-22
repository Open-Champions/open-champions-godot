STATUS: COMPLETE — CS5 audit fixes; arm64 CTest 216/216, GDExtension builds 8/8, Godot dev matrix 51/51, 111/111 sweeps scripts on and off, FULL_TRACE unchanged.

# Creature statistics audit fixes, round 4

Branch `codex/creature-stats`; implementation commit `f6ff05f` (on top of `d341267`). No push, no PR. No new research worktree was needed: every value traces to the research docs already pinned (`docs/objects.md:87`, `data-tables.md:211,319`, `tools/ref_objects.py` `player1_marker`, `docs/issues/arena-entry.md:190`, `docs/monsters.md:165`).

## Findings

| Finding | Resolution |
| --- | --- |
| CS3-1 MEDIUM (collision radius) | **Traced.** Research: +0x1c is the collision radius, default 24 from `GameObject_ctor`, and "radius: immediates in each class constructor" (`objects.md:87`, `data-tables.md:211,319`). The ported constructors store it in the binder-bound memory image (Skelight 12, Horsemen 20, Zek 30 via scale, etc.). After a build the host object's `radius` is now copied from binder word +0x1c (next to the existing flag-word sync), so Player1's free-spot box query (`o->radius` in the collect box and in the `r² + o.radius²` reach test, `ref_objects.py` find_free_spot) tests each earlier creature's real radius. Earlier Player1 objects keep the 24 default, which is what the research `player1_marker` leaves (its `construct` has no radius write). Any earlier unbuilt or stubbed record already refuses Player1 (`load_draws_unmodelled`), so no object with an unknown radius precedes a built Player1. Tests: Skelight host radius equals the binder word and `0x41400000` (12.0); Gnaw host radius follows its binder word. Residual, not claimed: the height extent (+0x20, from `Creature_init`'s model bounds) is still the 72 default; the query only uses it against a ±100000 vertical window (`0x47c35000`), documented in `docs/SCRIPT_COMMANDS.md`. |
| CS3-2 MEDIUM (level-flow inputs defaulted) | **Refused unless supplied.** Nothing in this host derives the previous level, gate travel/recall, the load-from-save state or the entrance, so Player1 placement no longer reads `Host::flow_`. New `Host::set_player1_entry({previous_level, entrance, gate_travel, loaded_from_save})`; without it the record is a counted refusal (`Player1 needs caller-supplied level entry`, record stays a stub, seed untouched). The take rules read only the supplied values. The caller owns tracing them (the research function's `prev_level`, `entrance`, `gate_travel` byte 0x41b51e; start rank 3 after a save load). Godot/sweeps do not call it, so sweeps are unchanged (Player1 refused earlier by the faithful-seed rule). |
| CS3-3 LOW (no Player1 test) | New block in `level_objects_tests.cpp` with an authored decode-only WORLD fixture and an authored `Player1` factory row: refusal without gate, without WORLD, without entry (seed unchanged, rank 0); rank 3 after save load builds with exactly one draw and no spawn slots; a taken marker (name equals entrance) builds, ranks 3, draws and fills all four facing slots. **Not covered:** the mid-placement refusal path (restore of slots/rank). I found no authored input that makes the WORLD queries throw (NaN and huge positions are accepted), so that path is verified by inspection only. |
| CS3-4 LOW (comment) | Reworded: `Creature_setStats` runs inside `Creature_ctor`, before `Creature_init`, so the MiniBoss refusal comes after it and leaves the record unbuilt with nothing copied; rank 2 is irrelevant because of the one-player guard. |
| CS3-5 LOW (partial state on refusal) | `spawn_pos`, `spawn_facing` and `start_rank` are snapshotted before the placement and restored in the refusal path. The shared seed still follows the draws made (the original draws before it would reach the same point; the audit's concern was the slots). |
| CS3-6 LOW (citations) | Code comment and `docs/SCRIPT_COMMANDS.md` now cite `938ccb8` (both HUD outcomes) and research-items `docs/startup.md:341`, `docs/issues/sync-breakers.md:383` for the 223 draws. |
| CS3-7 LOW (Holder.v1 mapping, hash vector) | Partly resolved. The comment now cites `docs/monsters.md:165` (the last table each constructor loads for the helper: 0x271d94, 0x2a87e8, 0x2b3f64; WeaponShield 0x50cda0 for Horsemen/UndeadKnight, Weapon1 0x50cf40 for Skelight) and states that the table-slot read itself is **not** pinned. Row hash `0xd61c` for "Gnaw" recomputed independently with the research tool's `hash_name` (`difftest_balance_lookup.py`, itself executed against `balance_hashName`); the test comment says no research-authored game-name vector exists. Still open: a research vector for a game name and a pinned table-slot read. |
| CS3-8 LOW (0x44fe18 capacity) | Unchanged: research gives no capacity; the guard (longest name in the executable's own table) stays and is documented as such. |

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build … -j 4`: PASS.
- Full native `ctest --test-dir build/native --output-on-failure -j 3`: **216/216 PASS**, 787 s (`work/creature-stats/r4-ctest.log`). `level-objects`: 1,791 checks, `script-commands`, `script-items` PASS.
- `work/creature-stats/run-builds.py` (both GDExtension variants, both level-texture variants): **8/8** exit 0.
- `work/creature-stats/run-godot.py` (dev matrix): **51/51** exit 0. `FULL_TRACE`: 21,990 bytes, SHA-256 `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035` — identical to rounds 2 and 3.
- `work/run-slices-creature-stats.sh csr4 --scripts` and `csr4n`: each **111/111** tried/loaded/played, 230 checks passed, 0 failures, all slices exit 0. Counts equal round 3 exactly (332 built, 15,569 unbuilt, 15,901 registered, 171 `Player1 placement needs faithful seed` refusals, 260 stub refusals, 16 refusal stops, 0 faults).

Counts only; logs under ignored `work/`, no game data committed.

## Remaining / not claimed

- The sweeps never reach the Player1 take path (gate unobserved in a Godot attach, and no caller supplies the entry), so the radius copy and the entry rules are exercised by authored tests, not by disc data.
- The inputs of `set_player1_entry` are the caller's responsibility; how the original sets `prev`, gate travel and the save-load state outside the research function is not traced here.
- Height extent (+0x20) of built creatures, the Holder.v1 table-slot read, a research hash vector, and the 0x44fe18 capacity stay open.
- Mid-placement refusal restore has no test.
- Norrath was not inferred from or tested with these RTA findings.

## audit-final (creature-stats-final, 8bdd705..9cfe9b2: 0 HIGH, 1 MEDIUM, 4 LOW)

Refusals counted in this round: **2** (CS4-3 table-slot read, CS4-4 research marker vector). No new research was started.

| Finding | Resolution |
| --- | --- |
| CS4-1 MEDIUM (`intro` applies balance rows) | **Fixed, traced.** research-creature-stats `docs/data-tables.md:216-217` (monsters.bal loaded on every level load outside the front end) and `docs/issues/live-rig.md:164,220` (`intro` is the front end). `set_level_stats` now refuses a base name of `intro` (counts `Front-end level has no balance table`, leaves `stats_ready_` false) and `advance_first_level_items("intro")` also clears `stats_ready_`, so either call order leaves hit points unreadable. New test covers both orders: refusal counted, no balance-scaled hit points. |
| CS4-2 LOW (pvp default) | **Documented.** Comment at the `monster_level` call states the single-player-only assumption (`start_single_player` is the only mode modelled; PvP mode B level 3.0 is unreachable). |
| CS4-3 LOW (Holder.v1 table slot) | **Refused (1).** No pinned research for the holder table-slot read; the audit says none is needed now. Left as documented open item. |
| CS4-4 LOW (Player1 vector, Horsemen variants 1-3) | **Refused (2).** No pinned `difftest_objects.py -k marker` vector for the slot values exists and starting new research is out of scope; extra Horsemen variant rows would need authored image rows for unverified statistics. Stays open. |
| CS4-5 LOW (attach aborts on malformed stats/WORLD) | **Fixed.** `session_controller.cpp` catches both, counts `Level statistics refused` / `Level WORLD refused` (new `Host::count_refusal`), and attaches without them. |

Verification (audit-final): arm64 Release `ctest -j 3` **216/216 PASS** (`work/creature-stats/final-ctest.log`); GDExtension + level-texture builds **8/8**; Godot dev matrix **51/51** exit 0; sweeps scripts on and off each **111/111** tried/loaded/played, 230 checks, 0 failures; `FULL_TRACE` 21,990 bytes, SHA-256 `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035` (unchanged). Counts only; logs under ignored `work/`.

## Audit fixes (creature-stats-final2, 2026-10-22)

This section supersedes the earlier claim that Skelight, Zek and on-foot
Horsemen complete construction in the script host. The final audit found one
MEDIUM and three LOW issues. Pinned research is `research-creature-stats`
`3469927` and the cited earlier executed contracts.

| Finding | Resolution |
| --- | --- |
| CS5-1 MEDIUM (`Holder.v1`) | **Refused.** `docs/creature-stats-port-check.md`, “Weapon holder slot 1,” pins the bodies at `0x11afb0` and `0x11bc18`, but no pinned source reads virtual slot 1 from `0x50cf40` or `0x50cda0`. The callback now throws `Holder.v1 table slot 1 is untraced` before selecting a body or writing holder bytes. Skelight, Zek and on-foot Horsemen remain counted stubs; mounted Horsemen retains its earlier Nightmare refusal. Native tests assert the new boundary. |
| CS5-2 LOW (`0x44fe18` capacity) | **Bounded refusal.** `docs/monsters.md:244` and `symbols/fmt-world.tsv:44` establish the global's role, and `difftest_level_name_split.py` establishes the base-name split, but none pins allocation size. `set_level_stats` now refuses any base name absent from the executable level table, including short arbitrary names; the existing overlong-name refusal remains. Exact allocation capacity is still unclaimed. |
| CS5-3 LOW (teardown exception) | **Fixed.** `World::unregister_external` now detaches paired nodes and live iterators without `HandlePool::clear` throwing, then removes the external object's names and address. `Variables::~Variables` also catches an unexpected `HandleError` so it cannot escape the implicitly `noexcept` destructor. `script-builtins-api` checks a synthetic same-list paired-node case and the surviving World's registries. |
| CS5-4 LOW (Player1 oracle) | **Exactness refused.** `docs/creature-stats-port-check.md` records an executed marker difftest but supplies no exact vector for this authored WORLD fixture. The local taken-marker test is explicitly labelled smoke coverage for rank and slot presence; it does not claim exact spawn positions, facings or final seed. |

**Runtime refusal categories added or widened: 2** — Holder-dependent
construction and level names outside the executable table. **Evidence claims
refused: 1** — exact Player1 placement fidelity for this fixture. The
destructor fix is a behavior repair, not a research-value refusal. No guessed
table-slot mapping or allocation size was added.

### Audit verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: pass; native test binary is arm64.
- `ctest --test-dir build/native --output-on-failure -j 3`: **216/216 pass**,
  625.45 s (`work/creature-stats/audit-final2-ctest.log`). Focused
  `level-objects`, `script-builtins-api`, `script-commands` and
  `level-factory` passed; after the teardown change, `objects-runtime-host`,
  `level-objects` and `script-builtins-api` passed **3/3**.
- `python3 work/creature-stats/run-builds.py`: **8/8** configure/build steps
  pass; main debug extension is universal arm64/x86_64.
- `python3 work/creature-stats/run-godot.py`: **51/51** dev matrix cases pass.
  FULL_TRACE: 21,990 bytes, SHA-256
  `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`,
  unchanged.
- `bash work/run-slices-creature-stats.sh cs-audit2-on --scripts` and
  `bash work/run-slices-creature-stats.sh cs-audit2-off`: each **111/111 tried,
  loaded and played**, 230 checks, 0 failures, empty sweep refusal lists;
  all four slice processes exit 0 in each run. Against the latest prior
  `csfin/csfinn` counts, scripts-off is unchanged. Scripts-on has eight
  counted Holder construction refusals and 20 counted level-statistics
  refusals for off-table base names; built objects fall 332→324, unbuilt
  rise 15,569→15,577, and stub command refusals rise 260→265.

The exact Player1 spawn positions, facings and final seed for the authored
WORLD fixture remain unverified. The original allocation size of `0x44fe18`
also remains unknown; off-table input is refused. Private-disc results above
are counts only; logs stay under ignored `work/`.
