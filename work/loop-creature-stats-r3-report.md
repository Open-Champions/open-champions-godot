STATUS: FIXED — all three MEDIUM audit findings (CS2-1..3) and the trivial LOW ones resolved; arm64 CTest 216/216, Godot dev matrix 51/51, 111/111 sweeps scripts on and off, FULL_TRACE unchanged.

# Creature statistics audit fixes, round 3

Branch `codex/creature-stats`; implementation commit `5c11524` (on top of `f4c2087`).
Research (permitted research worktree `t3/research-horsemen-factory`, created
earlier): `dfcc8d3` (Horsemen factory difftest), `a684d7d` (level-name split
difftest and statistics vector check). Both were re-run this round against the
owner executable (SHA-256 `292f6f48…d7`, checked by the tools). No push, no PR.
Earlier runs left the native edits uncommitted; they were reviewed, kept where
correct and corrected below.

## Findings

| Finding | Resolution |
| --- | --- |
| CS2-1 MEDIUM | `tools/difftest_horsemen_factory.py` executes each of the eight factory rows in `ee_emu.py` (name compare from the image's own class string, allocator and constructor stubbed) and asserts the `0x271c70` call arguments. On-foot rows `0x532dc8/dd8/de8/df8` reach call `0x15b900` with variant 0..3 and mounted 0, so the native `(test − 0x532dc8)/0x10` mapping is now traced. Rider rows `0x532cc8..0x532cf8` reach `0x15b3d4` with mounted 1; they stay **refused** (counted as `mounted Horsemen needs Nightmare_ctor`, 4 records) because the mount lifecycle is not ported. Source comments and `docs/SCRIPT_COMMANDS.md` cite the research commit. |
| CS2-2 MEDIUM | A creature's hit points become readable only if, when it is built, the item gate was observed (`intro`, or a supplied residency answer), no gate refusal has been counted, and `load_draws_unmodelled == 0` (no earlier unbuilt record). Otherwise `getHitPoints` is refused and counted. `bind_seed` resets the observation. Player1 placement (draws spawn positions from the same shared state) is likewise refused and counted (`Player1 placement needs faithful seed`) unless the seed is faithful. A creature built before a later unbuilt record stays readable: its own seed came from an exact state. New test `test_hit_points_need_faithful_seed` (5 scenarios: no gate call, observed gate, refused gate, unbuilt record before, unbuilt record after); it was not registered in `main` by the earlier partial work and is now. |
| CS2-3 MEDIUM | `tools/difftest_level_name_split.py` executes the request split at `0x3210c0`: the base name is stored in `g_levelName` and in `0x44fe18`; the level-scale stretch (`0x1e5b88`) and Skelight's `blomedal` compare (`0x2a88e0`) see the base name (`stomedal_sub`→level 1, `innpit_sub`→2, `blomedal_sub`→5 and Skelight match true only for blomedal). Native `level_name_` is now the base name, so the emulated `0x44fe18`, the stomedal/innpit override and `monster_level` agree. The length guard now checks the base name (a long `_sub` is no longer a false refusal). A new test shows a long-sub request yields the same hit points and seed as the base request. |
| CS2-4 LOW | Holder.v1 accepts only the two executed tables (`0x50cf40`, `0x50cda0`); anything else throws `Refused`. The `lump_load` stub return is commented as discarded by the constructors. |
| CS2-5 LOW | `Variables::new_object` unregisters the external object if `handles.link` or the bookkeeping throws. |
| CS2-6 LOW | Test vector provenance now cites `tools/check_creature_stats_vector.py` (research `a684d7d`), which reproduces Gnaw level 2 (`0x41edee1a`, 27, 3/6, 27, `0x41a00000`) and Skelight level 1 (`0x41a8355b`) from `ref_combat.monster_stats` and passes. |
| CS2-7 LOW | `docs/SCRIPT_COMMANDS.md` states the Horsemen rows as executed, the rider refusal, the base-name rule and the hit-point readiness rule. |

## Verification

- Research: `difftest_horsemen_factory.py`, `difftest_level_name_split.py`, `check_creature_stats_vector.py` all PASS.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build … -j 4`: PASS; level-objects test binary is Mach-O arm64.
- Full native `ctest --test-dir build/native --output-on-failure -j 3`: **216/216 PASS**, 728 s.
- `python3 work/creature-stats/run-builds.py`: both main GDExtension variants and both level-texture variants, 8/8 PASS.
- `python3 work/creature-stats/run-godot.py` (dev matrix): **51/51 PASS**. Full combat smoke 2,621 checks, 0 failures. `FULL_TRACE`: 21,990 bytes, SHA-256 `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035` — identical to round 2.
- `work/run-slices-creature-stats.sh csr3 --scripts` and `csr3n`: each **111/111** tried/loaded/played, 230 checks passed, 0 failed levels, all four slices exit 0.
- `champions-script-commands-measure --game rta <owner disc>`: 111 scripts loaded, 0 faults.

Counts only; logs under ignored `work/private`, no game data committed.

## Count changes (scripts on, round 2 → round 3)

| Metric | R2 | R3 |
| --- | ---: | ---: |
| Levels tried / loaded / played | 111 / 111 / 111 | 111 / 111 / 111 |
| Passed / failed checks | 230 / 0 | 230 / 0 |
| Built factory records | 503 | **332** |
| Unbuilt factory records | 15,398 | **15,569** |
| Registered objects | 15,901 | 15,901 |
| Unmodelled constructor draws | 15,398 | 15,569 |
| Player1 placement refusals (new) | 0 | 171 |
| Item gate refusals / conditional draws | 110 / 24,530 | 110 / 24,530 |
| Stub refusals / refusal stops | 261 / 16 | 260 / 16 |

The −171 built / +171 unbuilt move is exactly the new Player1 refusal: in an
independent Godot attach the item gate is unknown (no `hud.lmp` residency
signal), so the seed is not known to be faithful and placement, which draws
from the shared seed, is refused instead of guessed. Registered objects are
unchanged (each refused Player1 stays a stub), and the refused record still
counts as an unmodelled draw. The 4 mounted Horsemen remain refused. Scripts
off: no scripts are attached, so counts are 0 as before and level results are
unchanged (111/111, 230 checks).

## Remaining / not claimed

- The exact original seed after a Godot attach is still unknown until the client can observe `hud.lmp` residency; hit points and Player1 placement stay refused until then (or when a caller supplies the residency answer).
- Rider Horsemen (mounted 1) arguments are traced but the Nightmare/mount lifecycle is unported, so they are refused.
- The level-name difftest starts from a bounded mainloop stretch with the request already in the global and the underscore index in `s3`; the earlier mainloop code that establishes those is not re-executed here.
- Norrath was not inferred from or tested with these RTA findings.
