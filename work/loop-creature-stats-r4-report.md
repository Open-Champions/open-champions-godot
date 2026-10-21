STATUS: FIXED — CS3-1 and CS3-2 resolved (radius traced and copied; level-flow inputs refused unless the caller supplies them), trivial lows handled; arm64 CTest 216/216, GDExtension builds 8/8, Godot dev matrix 51/51, 111/111 sweeps scripts on and off, FULL_TRACE unchanged.

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
