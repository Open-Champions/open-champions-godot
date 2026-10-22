STATUS: FIXED — all six audit findings are addressed; arm64 CTest, the Godot dev matrix, both 111-level sweeps, and FULL_TRACE passed. The opt-in real-character smoke keeps its documented unsupported-HUD exit 1.

# Player state round 2 — 2026-10-21

## Branch and scope

Worktree `~/Projects/open-champions/worktrees/player-state`, branch `codex/player-state`. Rebased locally onto the latest wave-22 `codex/playable-integration` tip `af9e3b5` after its final documentation commit appeared. The branch already contained menus and the HUD overlay. No push or PR. Local implementation commits after the rebase are `ed66bcc` (native guard and tables), `ed53320` (live carry and glow), and `5a4356e` (menu-open glow reset).

The changes address the Claude Sonnet 5.5 audit in `~/Projects/champions-re-wt/cursor-runs/audits/player-state.md`:

| Finding | Resolution |
|---|---|
| PS-1 MEDIUM | Research-menus `docs/player.md` and research-combat `docs/data-tables.md` document new-character level, stats, gold and maximum HP/mana, but not starting **current** HP/mana. A local `tools/ee_emu.py` synthetic initializer probe did not establish valid starting maxima under its setup, so it was not used as a source for current values. The controller no longer assigns zero to campaign HP/mana. Slot-0 campaign mirroring and restoration skip unknown current fields. The script host refuses and counts commands that could read, copy or overwrite them; refusal results still stop a script when the result is used. Imported records retain their documented current fields. Native tests check four counted refusals and unchanged raw sentinels through a campaign carry. |
| PS-2 MEDIUM | A carried level captures `observed_player()` before saving the old campaign, then binds that live record into the new host and retains it for further transitions. A synthetic authored script tries an unknown-HP read, then grants 123 gold. The real-character smoke loads it three times across two carried boundaries: all three gold steps pass, both carries report true, and all three unknown-HP reads are counted as refusals. |
| PS-3 MEDIUM | Research-ui `docs/hud-dynamic.md` §§2–3 documents the glow: local slot, present live object, not dead, current HP > 0, pending level; alpha increases by 4 per displayed draw to 128 and resets when `hud_run` opens the level menu. The view requires each gate, keeps alpha while a gate is false, and resets/suppresses on the menu-open event. The play session has no original player-object/dead-bit observation, so its real-character glow remains refused. HUD smoke checks live, HP=0, unknown HP, missing object and menu-open states. |
| PS-4 LOW | The dormant name input is called `arena_colour`, matching the native adjunct argument; its absent source remains a refusal. The glow explicitly retains the documented `(0,0,1000,1000)` clip; HUD smoke checks it. |
| PS-5 LOW | `validate` rejects class/model mismatches using the documented model-to-class table. A synthetic ELF test exercises `read_tables`, including XP thresholds, the class-formula instruction walk and a malformed threshold refusal. No game bytes are embedded. |
| PS-6 LOW | The opt-in real-character smoke writes to one PID-scoped `user://` directory and removes its files and directory on every explicit exit. Its intentional exit 1 for unsupported HUD elements is recorded separately from passing checks. |

## Verification

- Release native configure/build on arm64: passed. Focused `player-state` and `script-commands` CTest: 2/2 passed. Full arm64 CTest: **227/227 passed**, 674.66 seconds.
- Godot 4.5.2 arm64 dev extension build: passed. Wave-22 dev matrix: **53/53 passed**. The final menu-open-only HUD change was rechecked by `play_hud_smoke.gd`: passed.
- `FULL_TRACE` from the dev matrix's full-combat smoke: **31,622 bytes**, SHA-256 `4da5705531dd885150aa42a715fff04e88274b78d9`, equal to wave 22.
- Read-only real-data sweeps: scripts off **111 tried / 111 loaded / 111 played**, 230 checks passed, zero failures and refusal reasons; scripts on the same **111/111**, 230 passed, zero failures/refusal reasons, all 111 scripts attached. Each of four slices exited 0 in each run.
- Opt-in real-character smoke: folder/player/level/level text, native companion write/read, plain native save/load and native player roundtrip passed. Its two carried level transitions and three unknown-current refusals passed. The smoke exits **1 by design** because the XP texture, created-character current mana fill and gold floater/alpha producer remain unsupported (`experience=0`, `mana_bar=0`, `gold=0`). Full real-level scene restore also still refuses the existing multi-surface collision display path; the player companion itself reads successfully. None of these failures is counted as a passed smoke.
- `tools/difftest_combat.py --quick -k player` through `ee_emu.py`: **28 properties, 0 failed** for player stats, level-up and related contracts. This does not establish `player_initNewCharacter` current HP/mana. A separate initializer probe was inconclusive and is not used as evidence for a starting value.
- Rendered synthetic 1280×720 HUD inspected in its Godot window at the documented 4:3 placement with a pending-level glow. Synthetic placeholder art establishes composition and gating only, not retail pixel parity.

## Commands

```text
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -R '^(player-state|script-commands)$'
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/mac-template_debug -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DGODOT_CPP_DIR=<pinned godot-cpp> -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-template_debug --target champions-archive-extension -j 4
python3 work/playable-wave22/run-godot.py
bash work/run-slices-player-state.sh psr2n
bash work/run-slices-player-state.sh psr2 --scripts
<Godot 4.5.2> --headless --path godot --script res://tests/player_state_real_smoke.gd -- --disc=<owner-selected read-only folder> --cache=<ignored private cache>
python3 tools/difftest_combat.py <owner-selected executable> --quick -k player
<Godot 4.5.2> --headless --path godot --script res://tests/play_hud_smoke.gd
<Godot 4.5.2> --path godot --script res://../work/player-state-hud-visual.gd
```

Private game files remained read-only. This report contains aggregate results only. No retail bytes, names or decoded content were added to tracked files.
