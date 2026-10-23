STATUS: MORE — native HUD extras are ported, replay the research reference cases, and verify clean (CTest 227/227 arm64, Godot dev matrix 54/54, both 111-level sweeps 111/111, FULL_TRACE unchanged); the play scene draws the XP bar, ungated pending rectangles and gold counter from observation keys, but the native session does not yet produce level, experience or gold pickups, and potion notices, selector input and the minimap are native-tested but not wired into play.

# Native HUD extras, round 1 — 2026-10-21

Worktree `~/Projects/open-champions/worktrees/native-hud`, branch `codex/native-hud`, from wave-23 head `c86c539`. Local commits only; nothing pushed, no PR. Research source: `t3/research-hud-gaps` `f7c3964` (`~/Projects/champions-re-wt/research-hud-gaps`), read-only. `codex/player-state` (tip `b8b1813`) is not in this base and not modified; see Coordination.

## Commits

| Commit | Content |
|---|---|
| `1872a42` | `native/src/hud_extras.cpp`, header, CMake target `champions-hud-extras`, contract tests, vector generator |
| `d924cdc` | reference vectors `native/tests/vectors/hud_extras_*.tsv` (force-added; `*.tsv` is ignored by default) |
| `0c317a2` | GDExtension `ChampionsHudExtrasService`, `play_hud.gd` wiring, `hud_extras_art.gd`, smokes |
| `8c7f9dd` | `docs/HUD_EXTRAS.md`, sweep counters, run scripts |
| `145567b` | `<cstdlib>` for `std::abs` |

## What was ported (all from the pinned research; no value is invented)

- **Status block** (`statusDraws`): health, pending rectangles, **XP bar**, mana, glow, frame, replayed against `ref_hud_status.py` rows. The pending rectangle gate is the dead bit and a local slot, **not `health > 0`** (hud-status.md section 4); negative and over-maximum currents are drawn, inverted rectangles are passed through. `bar_green%02d.tex` pairing, frame `(tick>>2)%11`, integer-difference fill, `S = X+15`, clip (S,24,right,124), tint `0x80808080`, plus the clipped drawer's scissor and visible-column rule.
- **Starting/respawn current values**: `afterInit` (999999936.0, max 0, mana max 10.0), clamp to recalculated maxima (`firstLevelCurrent`), respawn health = max x 0.5 offline or max in PvP, mana unchanged.
- **Gold chain**: floater slots, 43-tick delay, `(p+7)>>3` payout, cap 99,999,999, level-clear flush, arena bonus life, hold-60 / alpha +-4 counter, slot 1/3 right alignment, separators with both quirks (9-digit and negative inputs reproduced, not "fixed").
- **Quick slots**: `countPotions`, `potionSize`, `usePotion` (notice lines 1/2 for 120 frames, 4/5 for 240 at 1..5 left, pending, regen rates, removal or inventory drop), the nine selector gates with the `g_localMenuOpen` override, pad axes, 3-frame mask, messages 0/13/1, button glyphs, ring icon source.
- **Minimap**: gate (world texture, 12000 free units, half width >= 2), scissor, rotation by minus yaw, cell transform and visible cell range, blip shadow and colour triangles, icon alpha.

## Refused and labelled (research marks them Unknown/Inferred/not executed)

Minimap **tiles** (VU1 `0x403e20` read, not executed) and **icons** (run scripts and `vars_get` from draw code): `minimapTiles()`/`minimapIcons()` throw `Refusal` with the reason, tested. Not ported: ammunition/thrown/camera notice starters, raster parity, console frame/tick ratio, gold coin (`gold03.tex` size not in the pinned research, so the coin is refused in the play scene), the floater's cosmetic seed draw (owed and counted in `GoldState::cosmetic_draws`, not consumed). Floater and bonus-life slots above 3 throw.

## Coordination with `codex/player-state`

That branch's `Record` (`hp`, `max_hp`, `mana`, `max_mana`, `current_known`, `level`, `acknowledged`, `experience`, `gold`) and `Tables::experience` are the intended producers. Its `create()` leaves `current_known = false` because the starting current values were unsettled; `hud-xp.md` part B settles them. After both merge: set `hp`/`mana` from `hud_extras::firstLevelCurrent(max_hp, max_mana)` and `current_known = true`; respawn via `respawnHp`; XP thresholds are `Tables::experience[ack-1]` and `[ack]`. The play observation keys that carry them are `acknowledged_level`, `experience`, `experience_base`, `experience_next`, `gold`, `gold_pickups`, `dead`. Files this branch touches that player-state also touches: none (`session_controller.cpp`, `play_flow.gd`, `hud_view.gd`, `player_state.*` are untouched; `play_hud.gd` is touched by both: player-state changed 15 lines there, so expect a textual merge in that file).

## Verification

Native Release (`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, arm64; `file` confirms arm64):

- `ctest --test-dir build/native -j 3`: **227/227 passed**, 1,237 s (`work/hud-ctest-arm.log`; baseline 226 + `hud-extras`). `hud-extras` runs **54,360 checks**: status 528 rows (411 rectangles, 270 XP bars, 133 glows), XP/init 1,064 rows, gold 4,144 rows (1,507 operations, 360 draws), quick slots 4,093 rows (1,600 potion uses, 148 notices, 128 messages), minimap 820 rows (260 views, 56 drawn, 48 blips).
- Godot 4.5.2 arm64 dev extension (`template_debug`) built; **dev matrix `work/native-hud-r1/run-godot.py`: 54/54 exit 0**. The first pass had 51/54: the three `creature-anim` entries need the separate `gdextension/creature_anim` build, which was not built in this worktree; after building it they passed (`work/native-hud-r1/godot-creature-anim*.rerun.log`). `play-hud` passes with the new gold, XP, gate and refusal checks.
- **FULL_TRACE** (`combined-full-combat`, headless): `{"bytes":31622,"renderer":"headless","sha256":"4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9"}` — identical to wave 23. **2,620 checks, 0 failures.**
- **111-level sweep** (`work/native-hud-r1/run-slices.sh`, 4 slices, 60 ready ticks, read-only disc): `hudon --scripts` and `hudoff` each **tried 111, loaded 111, played 111, 230 checks passed, 0 failures, empty refusal list**; `scripts.refusal_stops` 14 / `stub_refusals` 266 / `callee_refusals` 0 with scripts on (zeros off), same as wave 23. Count changes against the wave-23 sweeps: new keys `hud.extras_green_loaded` 44 and `hud.extras_green_refused` 0 (11 bar_green frames x 4 slices; each slice opens the folder once), added by this branch; run-dependent keys differ as usual (`audio.frames_pushed`, `memory.static_peak_bytes`, `time.*`); scripted `text.frames_drawn.bust` is 32 now against 27 in wave 23 (34 in wave 22): the wave-23 report attributes that count to display cadence, and this branch does not touch the bust path, but I did not isolate it. Nothing else changed.
- **Real-data smoke** (`godot/tests/hud_extras_real_smoke.gd`, opt-in, level 0, 60 ticks, read-only disc): all **11/11 `bar_green` frames decode** (100x6, 3 rows, 0 refused), the XP bar draws with a real texture on all **11 animation frames** at half fill (right = 76), the pending health rectangle draws at health 0, and the gold counter draws real-font glyphs for **133/133 visible frames** with credit starting at tick 43. Experience thresholds in that smoke are authored synthetic numbers, so it checks the real art and font, not the table. Exit 0.

## Commands

```sh
python3 -I tools/hud_extras_vectors.py ~/Projects/champions-re-wt/research-hud-gaps/tools native/tests/vectors
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/mac-template_debug -DGODOT_CPP_DIR=../integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DCHAMPIONS_LEVEL_FLOW_WATER_ROOT=$PWD -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-template_debug --parallel 4
python3 work/native-hud-r1/run-godot.py
work/native-hud-r1/run-slices.sh hudon --scripts ; work/native-hud-r1/run-slices.sh hudoff
godot --headless --path godot --script res://tests/hud_extras_real_smoke.gd -- --game-folder=<disc> --cache=<dir> --level=0 --ticks=60
```

## Not done / unverified

- Not wired into play (no producer or sim hook on this base): potion notices and the notice grid, the D-pad selector and its messages, the minimap overlay, level/experience values, gold pickups (Gold_onTouch and chest paths). The play scene shows XP and gold only when the observation carries the keys above, and keeps `experience` and `gold` in `refusals` otherwise.
- The vectors are research-model outputs, not new executions of the original code; the research's own difftests against `ee_emu` were not re-run here (they need the ELF and the research toolchain). The VU1 tile arithmetic, raster parity and console frame ratio remain unverified.
- Only arm64 was built and tested (no Rosetta/x86_64 CTest, no export matrix, no release-template build, no Windows/Linux build).
- Opt-in smoke ran on one level (0); the 111-level sweep exercises the new service and art loading but not XP/gold drawing (the session supplies no such observation).
- The status block was added beside `hudBaseLayers`/`hudPendingRects`, not in place of them; the old pending-rectangle function still refuses negative currents and is no longer used by the play view.
- Private disc: only counts above; no game bytes, names or decoded content were added to tracked files.
