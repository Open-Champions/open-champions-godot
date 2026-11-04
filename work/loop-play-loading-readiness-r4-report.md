# Play loading readiness, round 4

STATUS: DONE. Starting-equipment selection and the observed arrival prefixes are in the client. Live arrival is still refused. Headless and rendered journeys stay 67 and 68 steps, with the same step results as round 3. The blood1 loading refusal now also names the five limits that research still marks Unknown.

Research pin: `research-loading-readiness` `366b65a` (round 4 audited PASS). Client base: `d07f7e4e`. Implementation commit: `c21e66a`.

## What was implemented

Observed, Executed only. Citations are `366b65a` paths.

- Starting selection for bodies 0–11, seeds 7 and 20973, and kinds -1 and 1. Class, weapon slot, weapon definition, armor slots 4/10/12, armor definitions, player-0 caches `0x8659e8` / `0x865b48` / `0x865f68`. `docs/starting-equipment-r4.md:43-56`.
- Header choice for player counts 1 and 2 with override 0 or 1. Group counts 243, 245, and 155 for `0x535c08`, `0x535bf8`, and `0x535c28`. `docs/starting-equipment-r4.md:76-83`.
- Single-player primary armor metadata deltas for slots 4, 10, and 12. `docs/starting-equipment-r4.md:104-124`.
- Cache span `offset = begin * 0x800`, `size = (split - begin + tail) * 0x800`. `docs/starting-equipment-r4.md:134-135`.
- Generic weapon index: definition signed short `+0x34`, plus the caller-supplied variant only when flag bit 36 is set. Row base `0x3e9ce0`, source `0x520030`. `docs/starting-equipment-r4.md:156-160`.
- Arrival reset clears front count, back count, parity, save inhibit, and the post-action first byte. Record bytes stay. A 65th deferred name is rejected. `docs/loading-arrival-r4.md:78-109`.
- Explicit route prefixes, synthetic arguments only:
  - Route 0: `0x1473b4`, `0x1420d0`, `0x1420a4`, then stop. Refusal: object load (`0x142320`).
  - Route 1: no natives, then stop. Refusal: resource bootstrap (`0x14222c`).
  - Route 2, completion clear: `0x142084`, `0x142044`, `0x1433c8`, `0x1420d0`, `0x143540`, `0x143540`, `0x143ee4`, `0x142010`, post-action `0x143500`, then stop. Refusal: cutscene runtime (`0x143f1c`).
  - Route 2, completion set: omits `0x142044` and `0x1433c8`. Choice bit does not change the address order. Quest count ends at 1, 8, or 0 for the empty-add, full, and completed-empty controls.
  - `docs/loading-arrival-r4.md:154-179`.
- An ordinal outside 0..2 refuses with `route state` and does not pick an order.

The live loader does not apply a prefix. The destination's route ordinal is Unknown, and the private script arguments are not copied into the client.

## Refusals

Removed: none of the live journey refusals. The new API no longer treats the executed selection table, header counts, metadata deltas, cache span, generic index, queue bound, or the three explicit prefixes as unknown.

Kept:

- `blood1_arrival` remains `refused`. `loading_ready` remains false. The previous sentences stay, and the refusal now continues with `normal deferred census`, `route state`, `object load`, `resource bootstrap`, and `cutscene runtime`. Those five are `docs/loading-arrival-r4.md:184-207`. Naming them does not set `deferred_complete` or publish equipment.
- `portal_arrival` remains `refused_expected`. The message is unchanged: gate travel still refuses the hub/GateList lifecycle, paused menu task, and destination constructor seed/blocking grid. Round 4 does not execute those constructors.
- `player_death` remains `blocked_death_reload` with `respawn_completed` false.
- The campaign fight still reports `fight_starting_equipment = refused_unknown` and keeps weapon class 5. The initializer table is not applied there: this fight does not run initializer `0x1b1d70`, and the route's seed, kind, and saved inventory are outside the executed controls (`docs/starting-equipment-r4.md:59-61`, `docs/starting-equipment-r4.md:257-263`).
- Item-instance bytes, formatter output, private resource payloads, cache allocation ownership, and destination inventory changes stay unimplemented. Public tests use synthetic fixtures only.

Step results versus round 3: every headless pair (67) and every rendered pair (68) is unchanged. The only arrival-text change is the five appended limit names on `blood1_arrival`.

## Gates

All commands ran on the Mac mini, inside `~/malik-champions/open-champions/worktrees/play-loading-readiness`, Release `-j 4` and CTest `-j 3`.

| Gate | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 4` | PASS |
| Focused `champions-script-commands-tests` | PASS (`script-commands ok`) |
| Full CTest `-j 3` | PASS, 251/251, 788.50 s. This branch's suite is 251. Wave 37's 259 includes other merged branches that are not in this worktree. |
| GDExtension `template_debug`: archive, level texture, creature animation | PASS. Libraries landed in `godot/bin` and `godot/creature_anim/bin`. |
| Strict Godot matrix, `work/loading-r2-tools/godot_matrix.py matrix --captures` | 58/61. The three failures match round 3 and were not introduced here: `loading_disc_cases` exits 2 because the strict command does not pass a game folder; `ui_layout_smoke` still calls `hud_text_glyphs` with 4 arguments against the 5-argument binding; `native_fx_visual` still cannot save its PNG at the relative capture path. |
| `FULL_SAVE_LOAD` | 300/300. Continued and uninterrupted presentation hashes match (`cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`). |
| `FULL_TRACE` | Unchanged: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. Combat checks 2,641, failures 0. |
| Journeys | Headless 67 steps, exit 1, 6 refusal kinds, 0 captures. Rendered 68 steps, 43 PNGs, 6 refusal kinds. Disc file count stayed 994. |

The first journey launch compiled `real_data_play.gd` before the project import had registered `NativeFxRenderer`. A headless editor import (`--quit-after 120 --frame-delay 50`) fixed that, and the journeys above are the rerun. No playthrough script was edited.

## Files

- `native/include/champions/loading_equipment.hpp`
- `native/src/loading_equipment.cpp`
- `native/include/champions/script_commands.hpp`
- `native/src/script_commands.cpp`
- `native/tests/script_commands_tests.cpp`
- `native/src/campaign_fight.cpp` (comment only; the stand-in refusal is unchanged)
- `native/src/frontend.cpp` (creation note no longer says the selection table does not exist; instances are still not published)
- `gdextension/src/session_controller.cpp` (live loading readiness names the five limits)
