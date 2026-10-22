STATUS: FIXED — both MEDIUM and all five LOW audit findings resolved; arm64 CTest, Godot dev matrix, both 111-level sweeps, and FULL_TRACE verified.

# Creature statistics audit fixes, round 2

Branch `codex/creature-stats`, based on `00b1aa6`; implementation commits
`259c3f9`, `bbda8d7`, and `c9f70f6`. Research lives only in the permitted research
worktree on `t3/research-factory-skelight`, commits `7a66b58`, `d847349`,
and `938ccb8`. No push or PR.

## Audit findings

| Finding | Resolution |
| --- | --- |
| CS-1 MEDIUM | Executed the factory path in `ee_emu.py`. The Skelight call at `0x15b608` has fifth argument **1**, set by its delay slot at `0x15b60c`. The native value stays 1 with a source-address comment. The research difftest asserts the observed call argument. |
| CS-2 MEDIUM | Executed the original `game_loadLevel` gate. It compares the world to `intro` at `0x1e5690` (case sensitive), then queries `hud.lmp` residency at `0x1e56a4`. An absent HUD reaches `item_loadResources` at `0x1e56bc`; a resident HUD skips it. The Godot attach and counts-only measurement have no actual HUD residency signal. They now refuse the 223 draws, leave the seed untouched, and count an unknown gate plus 223 **conditional potential** draws. An empty caller-supplied level name also counts a refusal. A caller that supplies a known name and an observed residency result can execute the exact gate. “First game world after boot” remains inferred. |
| CS-3 LOW | With selected statistics, the factory refuses creature builds unless player count and difficulty are both 1. Synthetic multiplayer case passes. |
| CS-4 LOW | Before writing the emulated level-name global, a supplied name longer than every executable level-table name is refused. This uses the original image's supported-name domain without guessing its buffer capacity. |
| CS-5 LOW | `Variables` teardown unregisters its objects from a surviving `World`, removing list links, name entries, object addresses and owned-pointer entries. A test queries the surviving World after binder destruction. |
| CS-6 LOW | If Player1 placement throws after handing the seed to `World`, the consumed world seed is copied back before the record is stubbed and the unmodelled load is counted. |
| CS-7 LOW | `docs/SCRIPT_COMMANDS.md` now describes copied statistics, hit-point readiness, the Skelight argument and the item gate. The round-1 report uses a placeholder instead of the local private-data path. |

## Acceptance counts

Four-slice, 60-tick counts-only scene sweeps used the owner-selected read-only
RTA disc. Logs and caches remain ignored; no private bytes or decoded content
are committed.

| Metric | Round 1 scripts on | Round 2 scripts on | Round 2 scripts off |
| --- | ---: | ---: | ---: |
| Levels tried / loaded / played | 111 / 111 / 111 | 111 / 111 / 111 | 111 / 111 / 111 |
| Passed / failed checks | 230 / 0 | 230 / 0 | 230 / 0 |
| Failed levels / refusal kinds | 0 / 0 | 0 / 0 | 0 / 0 |
| Built / unbuilt factory records | 503 / 15,398 | 503 / 15,398 | 0 / 0 |
| Stub refusals / script refusal stops | 260 / 16 | 261 / 16 | 0 / 0 |
| Unmodelled constructor draws | 15,398 | 15,398 | 0 |
| Item gate refusals | 0 | 110 | 0 |
| Conditional item draws withheld | 0 | 24,530 | 0 |
| Bound / paired script seeds | 111 / 111 | 111 / 111 | 0 / 0 |

Skelight remains built because the factory argument 1 was proved, so built
counts do not move. The one additional stub refusal is in the third sweep
slice; refusal stops do not change. The association with changed seed state is
plausible, but the aggregate summary does not prove the exact command cause.
The 24,530 value is 110 × 223 potential draws, **not** a measured original
seed delta: residency for these independent Godot attaches is unknown. The
native seed is deliberately left unchanged at that gate. The separate
counts-only script measure likewise loaded 111/111 scripts with zero faults,
503 built and 15,398 unbuilt, and reported 110 unknown gates / 24,530
conditional draws.

## Verification

- `python3 -B tools/difftest_factory_skelight_gate.py <owner-elf>` in the
  research worktree: Skelight call and four gate outcomes PASS, including the
  actual item-loader call only when HUD residency is false.
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: PASS. `file` reports the level-objects
  test binary as Mach-O arm64.
- Focused native CTest (`script-builtins-api`, `script-builtins-exact`,
  `session-objects-exact`, `level-objects`, `script-commands`): 5/5 PASS.
- Full native `ctest --test-dir build/native --output-on-failure -j 3` on the
  final build: **216/216 PASS**, zero failures, 835.73 s. An earlier full run
  was interrupted after 7 passes to add the missing-level-name refusal;
  the complete result is from the rebuilt final code.
- `python3 work/creature-stats/run-builds.py`: both main GDExtension template
  variants and both level-texture variants configured and built, 8/8 PASS.
- `python3 work/creature-stats/run-godot.py`: 51/51 PASS. The Full combat
  smoke passed 2,621 checks with zero failures. `FULL_TRACE` is unchanged:
  21,990 bytes, SHA-256
  `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`.
- `work/run-slices-creature-stats.sh csr2final --scripts` and
  `work/run-slices-creature-stats.sh csr2finaln`: each 111/111 PASS, 230 checks,
  zero failed checks and zero failed levels. The scripts-on summary includes
  the new gate counters. Both ran after the final rebuilt extension.
- `./build/native/champions-script-commands-measure --game rta
  <owner-selected-rta-disc>`: 111 scripts loaded, zero faults; counts above.

The exact original seed after a Godot attach remains unverified until that
path can observe `hud.lmp` residency. The research execution establishes the
gate and the 223-draw count, not the residency of this separate client.
The previously open four mounted Horsemen records remain counted as unbuilt;
their Nightmare and mount lifecycle are outside this audit fix.
Norrath behavior was not inferred from these RTA findings or tested here.
