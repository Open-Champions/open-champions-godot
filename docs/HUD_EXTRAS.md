# HUD extras

Native port of the gaps documented by research branch `t3/research-hud-gaps`
(commit `f7c3964`, audit PASS): `docs/hud-gaps.md` and `docs/spec/hud-status.md`,
`hud-xp.md`, `hud-gold.md`, `hud-quickslots.md`, `hud-minimap.md`. Code:
`native/src/hud_extras.cpp` (`champions-hud-extras`), tests
`native/tests/hud_extras_tests.cpp` (CTest `hud-extras`), GDExtension service
`gdextension/src/hud_extras_service.cpp` (`ChampionsHudExtrasService`), play view
`godot/ui/play_hud.gd`, art `godot/ui/original_art/hud_extras_art.gd`.

All values are raw R5900 patterns through `core_arithmetic`, as in the research
models. `tools/hud_extras_vectors.py RESEARCH_TOOLS OUT_DIR` regenerates
`native/tests/vectors/hud_extras_*.tsv` from `tools/ref_hud_*.py` (seeded, synthetic
inputs, no game data). The tests replay those rows: status 528 (411 rectangles, 270
XP bars, 133 glows), XP/init 1,064, gold 4,144 (1,507 operations, 360 draws), quick
slots 4,093 (1,600 potion uses), minimap 820 (260 views, 56 drawn, 48 blips).

## What is implemented

| Area | API | Evidence |
|---|---|---|
| Status block incl. XP bar and pending rectangles | `statusDraws`, `fillRight`, `xpFillRight`, `scissor`, `visibleColumns`, `statusVisible`, `barGreenShape` | hud-status.md, hud-xp.md part A; Executed |
| Pending rectangle gate | `pendingRectsAllowed`: local slot and dead bit 0x8 clear, each rectangle on its own pending > 0. **No health gate**; negative and over-maximum currents are drawn (inverted rectangles are passed through) | hud-status.md section 4; Executed |
| Starting and respawn current values | `afterInit` (999999936.0, max 0, max mana 10.0), `clampCurrent`, `firstLevelCurrent`, `respawnHp` (max x 0.5 offline, max in PvP, mana unchanged), `recalcClamps` | hud-xp.md part B; Executed |
| Gold chain | `addGoldFloater`, `tickGold` (43-tick delay, `(p+7)>>3`, cap 99,999,999), `flushGold`, `arenaBonusLife`, `drawGold` (hold 60, alpha +-4, slots 1/3 right-aligned), `formatGold`/`insertSeparators` with both separator quirks | hud-gold.md; Executed |
| Potion notices | `countPotions`, `potionSize`, `usePotion` (line 1/2 for 120 frames, line 4/5 for 240 frames at 1..5 left, pending, regen rates, removal or inventory drop) | hud-quickslots.md sections 1, 2; Executed |
| Selector input | `padReadable` (nine gates, `g_localMenuOpen` overrides), `padAxes`, `maskLocalButtons` (3 frames), `selectorMessage` (0 A/B, 13 use, 1 assign), `buttonGlyph`, `recordIcon` | hud-quickslots.md sections 3, 4; Executed |
| Minimap projector | `minimapGate`, `worldToCell`, `minimapView` (scissor, rotation by minus yaw, visible cell range), `blipTriangles` (shadow and colour triangles), `iconAlpha` | hud-minimap.md steps 1-6; EE side Executed |

## Refused, with reasons

- **Minimap tiles** (`minimapTiles`): the map is drawn by VU1 program `0x403e20`, read
  (O) but not executed. Refused until a VU1 interpreter or frame capture confirms the
  arithmetic and the world +0x6c atlas layout.
- **Minimap icons** (`minimapIcons`): they call `vars_get` and `script_runByName` from
  draw code on one machine, and the icon table writer and +0x10 are unread.
- **Ammunition, thrown and camera notice starters** (lines 0, 3, 6, 7): Observed, not
  executed in the research.
- **Raster parity** of rectangles and the console frame/tick ratio of the gold floater
  (the counter steps once per observation here, one displayed frame per tick).
- **Gold coin** `gold03.tex`: its size is not in the pinned research (only its
  position), so the coin is not drawn.
- **Cosmetic seed draws**: the floater start owes one cosmetic draw (counted in
  `GoldState::cosmetic_draws`), the sound calls' draws are Unknown. The native play scene
  approximates the floater start with no drift; it changes no shared state.
- Floater and bonus-life slots above 3 throw `Refusal`: the presentation tables are only
  documented for slots 0-3.

## Play scene

`play_hud.gd` now draws, through the service:

- the pending rectangles without the `health > 0` gate when the observation
  supplies a Boolean `dead` bit that is clear. A missing or invalid bit refuses
  the rectangles at every HP;
- the experience bar when the observation carries integer `acknowledged_level`,
  `experience`, `experience_base`, `experience_next` and the eleven `bar_green` frames
  are loaded (`hud_extras_art.gd`, same pinned-shape checks as the other HUD art);
- the gold counter from `gold_pickups: [{slot, amount, x, y}]` and `gold`, with the
  real HUD font, credit after 43 ticks and a 133-frame single-pickup life.

After merging `codex/playable-integration` at `2e57376`, the native play session
observes the player record's `level`, `acknowledged`, `experience`, `gold` and the
selected executable's `Tables.experience` thresholds. The play HUD draws XP
when these values and all eleven green frames are present. A direct writer to
record gold does not start the counter: it still requires an observed floater
from `Gold_onTouch` or chest gold. The current play session has no such pickup
producer, so the counter stays hidden and labelled as refused.
If observed record gold changes while a floater or pending credit is busy, the
play HUD refuses the composite counter: that observation cannot be ordered
against the local payout. Gold tick and draw-frame timing continue while the
composite is refused; an idle record change then synchronizes directly.

Created records leave current HP and mana unestablished: the session has no
live `Player` recalculation or record-to-object mirror. The standalone
`firstLevelCurrent(max_hp, max_mana)` arithmetic remains tested, but it is not
applied to created records. Imported and loaded records retain their observed
current values. The native play simulation's HP/mana remain separate from
record HP/mana; the HUD bars
continue to draw the simulation's values because no original record-to-object
mirror has been ported.

The GDExtension exposes `potion_use` and `selector_input` to the play HUD for
complete, observed bag-2 and raw-pad inputs. The selector also requires an
observed local slot and positive current HP before reading its nine gates. The play HUD computes those only
when an event/record is supplied; it labels the absent producers and art as
refusals. The minimap projector similarly accepts a complete local-camera,
grid and texture-gate record as `hud_minimap`, but the current play session
supplies none. Tiles and icons remain refused for the research's VU1 and
draw-script Unknowns.

### Player-state integration

The merged record provides the XP and gold inputs and executable-backed
thresholds. `set_player_character` keeps created currents unknown and
current-dependent script commands refused. A native player respawn path is not
present, so
`respawnHp` remains tested standalone and uncalled in play. Neither the raw
pad nor bag-2 inventory is inferred from that record.

## Tests

- `ctest -R hud-extras` replays the TSV vectors plus authored checks (dead-bit gate, the
  single-pickup timeline: first credit at tick 43, visible 133 frames, minimap and tile
  refusals).
- `godot/tests/play_hud_smoke.gd` (dev matrix `play-hud`) covers the play view with
  authored images.
- `godot/tests/hud_extras_real_smoke.gd` (opt-in) runs on the user's game folder:
  all 11 `bar_green` frames decode, the experience bar draws on every animation frame,
  the gold counter draws with the real font for 133 frames.
