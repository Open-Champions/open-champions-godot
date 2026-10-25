# Front end — boot screens, main menu, character creation, options, load/save, pause

`champions::frontend` (`native/include/champions/frontend.hpp`) is the navigation
and state contract of Return to Arms' screens outside play, plus the game pause
menu. `ChampionsFrontendService` bridges it to Godot. `godot/ui/frontend_menus.gd`
paints the flow's draw calls over the play scene and performs its requests; what the
research leaves Unknown is a labelled **host layout** (see "Layout" below).

Evidence pins:

| Research | Head | Used for |
|---|---|---|
| t3/research-ui | 3e39392 | `docs/mainloop.md` 8 (boot order, modal loops, exit codes), `docs/ui.md` "Front end" (states 0–15, setup writes), `docs/player.md` (record fields), `docs/data-tables.md` 4–5, `docs/issues/live-rig.md` 4a and 21.7 (Ran), `docs/issues/native-online.md` (pause entries, Ran), `symbols/mainloop.tsv` (splash timings) |
| t3/research-menus | 099900e | `docs/menus.md` 1, 6, 7 (masks, pause font and colour, save index) |
| t3/research-saves | 8d43286 | `docs/save-format.md` (the 760-byte options image, card states, save latch) |
| t3/research-combat-ctors | e9777eb | `docs/spec/music-volume.md` (slider, commit, boot copy; ported as `music_volume.hpp`) |
| t3/research-menus-layout | dc458d5 | `docs/frontend-layout.md`, `tools/ref_frontend_layout.py`, `tools/ref_frontend_screens.py` (draw calls, fonts, layers, tints, menu sounds, the first-theme gate) |
| t3/research-save-screen | 9c2b941 | `docs/save-screen.md` (slot-list tile frame, indexed rows, BCD date and clock formatter calls, and completed-slot header-backed preview fields; `research-save-screen-r2-fix.md` audit PASS) |
| t3/research-char-create | b6dda4e | `docs/spec/character-creation.md` (13 carousel rows, seven starting-stat rows, keyboard initialization, record handoff bounds; audited PASS) |
| t3/research-player-model / t3/research-appearance | 7f13899 / e088796 | Body codes, selection lumps, appearance resource requests; constructor checks only |

Status words are the research's: **Executed**, **Observed**, **Ran** (seen on the
emulator), **Inferred**, **Unknown**.

## Layout (draw calls)

`Flow::layout()` (`native/src/frontend_layout.cpp`, `frontend_layout.hpp`) returns a
`Scene` of `Draw` records in the 640x448 UI space, each tagged **Executed**,
**Inferred** or **Host** (the research's word; Host is anything it lists Unknown).
Nothing from the disc is stored: a `Draw` names a lump and entry or a font word, and
`godot/ui/frontend_view.gd` loads the user's `gamefontmed`/`gamefontsmall`, the
`ctile*`/`tile*` frame pieces of `INTERFAC.LMP`, `SOELOGO.LMP` and `FRONTEND.LMP`
sprites at run time, through the existing UI services (`menu_frame`, `text_layout`).
Paint order is the research-ui rule: layers 0..7, and inside a layer the later
submission first.

| Element | Draw calls | Label |
|---|---|---|
| List rows (main, game type, players, mode, difficulty, front-end options, pause) | centred medium font (0x4f7100) on layer 7; first row y = origin_y - (11 rows + 6), step 22; origin (320,309), front-end options (320,238), pause (320,240); value rows (front-end options 0-2) shifted 23 left. Geometry, front-end font and layer Executed. **Colour**: the front-end list batch 0x1a09e8 has a fixture colour of 0 and the parent batch 0x64464646, so the front-end list colour is a **Host** identity tint (0x80808080). The pause list font word is zero in the fixture (Unknown); its medium font and colour 0x80464646 come from the later text_begin 0x18d5e8 and are **Observed**, not Executed. In-game options use two Host controls because table 0x44f218 was not captured | Executed geometry / Host colour / Observed pause |
| List frames | cursor (style 0, tint 0x300000ff, 40 wide, height 32 at an even y and 33 at an odd y), backing (style 2, 0x80808080, layer 7), front overlay (style 0, 0x3f000000, layer 6); the 4-row, pause and front-end options calls match the research's asserted row-0 calls. Other selected rows show a Host highlight because their frame calls were not captured. **Refused:** text widening of narrow lists (the research says only that text width "can enlarge" them and traces no width rule), so only the executed minimum-width calls are emitted | Executed row 0 / Host other rows |
| Title | `FRONTEND.LMP/title.tex` at (141,15), layer 5, tint 0x80808080, states 0-4 only | Executed |
| Publisher logo | `SOELOGO.LMP/soe_splash_url.tex` (192,330) and `soe_splash.tex` (64,50), layer 3, tint 0x80808080 | Executed |
| Footer, message box, slot list, character, name entry | the research's small-font layer-6 batches, colours and text anchors; the research word is carried verbatim in `Draw.colour`. Only the slot-list batch sets `Draw.settled` for known fade. The Godot host paints its authored placeholder text at a readable Host opacity, separate from the research alpha. Message-box backing style, layer and inflation are Host choices. Phase-3 colours follow the call order of `PLAYERSEL_PHASES` (header and description 0x0A316864, points row 0x08325D18, stat rows 0x08585E5D, selected 0x0068C7CC). Label text is a host choice | Executed positions, Host text |
| Save/load tile frame | `memcard_slotList`'s settled fixture passes (61,126,519,97) to the wrapper, which submits the style-2 `INTERFAC.LMP/tile*.tex` frame at (60,122,521,105), layer 6. The nine pieces are loaded from the selected disc at run time. The fixture tint 0x7f616369 is retained for this captured state; fade interpolation and exact retail pixels remain Unknown | Executed frame submission / Observed art names |
| Menu sounds | move `menu_main_toggle.anm`, select `menu_main_accept.anm`, back `menu_main_back.anm` (FX.LMP) are requested at the documented triggers and counted by the host (`sounds`) | Executed ids |
| First theme | `mx_maintheme_l.vag` flags 1 goes through the one music-volume gate (0x3d4ccccc), also for the credits handoff (flags 3) | Executed |

Cursor and highlight: the list cursor is the style-0 frame above for row 0. The slot-list
rectangle (66,133)-(574,152) follows the selected-row word in the fixture, but its
retail highlight role and cursor bounds are Inferred; this client does not paint it
without a card directory. The import row rectangle (108,101)-(298,118) is documented but
is not drawn by this host. No other cursor
is documented. Animation: the only documented timed behaviours (logo 240 frames, film
press after 30 frames, credits scroll one unit per call with the +2/+2 shadow pass and
the alpha points at y = 448/400/224) are flow or one-line fixture facts; fades are not
curved in the research, so none is drawn.

The save/load screen keeps its file chooser actions in a labelled host area below
the documented frame. No PS2 card directory or cached save headers are bound by
the normal host flow, so the card rows, selected-slot preview, date/clock values
and confirmation prompts stay absent there. A caller may explicitly supply a
card cache to `Flow::set_card_cache` / `ChampionsFrontendService.set_card_cache`.
The bounded 16-byte metadata and 0x70-byte header strides, five visible row
anchors, numbered markers, the save-only first free row, `%d/%d/%d` BCD date and tested 12-hour clocks then
paint with the disc small font. An explicitly selected header supplies raw
player count, difficulty index and play seconds at the preview anchors; their
labels are visibly Host because localized text and general minute formatting
are not established. Real directory timestamps, card capacity, level/class
text, locale and retail pixel parity remain unverified. The saved confirmation is
also a labelled host layout because its frame art is not established.

Still a labelled host layout (the research lists them Unknown): class art, the 3D
model pose and the per-race animations, appearance art, the options value bars and
knobs, card rows with real file data, the intro film picture, the credits data
layout, the Exit prompt and options-load prompt. The sound clips are not voiced: the
research captured resource names only, not the clips' sample tables, mix level or
group volume (0x41bac8).

The class and appearance host screens show the selected model's `bodyv1` texture
from the user's `SELECT.LMP` at run time, with opaque RGB as in the existing
model viewer. This is a labelled disc asset preview,
not the original 3D carousel camera or a rendered appearance variant. The
original selection path and complete confirm/cancel walk remain unverified.

## What is not in the research

None of the following is documented, so the flow refuses it or the host labels it:
- The publisher logo picture and the intro film (MPEG-2, not decoded). Because the
  film never finishes here, only a press after 30 frames ends it.
- Main-menu entries after the first: their keys and order are unknown. The
  on-screen texts seen are listed in host order.
- Other entries of the players, mode and difficulty tables. Internet Game.
  Difficulties above the first: the unlock test `options_get4f8f38Level` was not
  executed. The values of `g_pvpModeA/B`, 0x4f7124 and the kill limit per choice.
- Credits (`credits_run` not read). The state-14 "save config?" prompt.
- Option entries other than music at index 1. The commit model covers music only.
- Character creation:
  - the 13 carousel positions are known: `0,11,10,0,1,2,3,4,5,6,7,8,9`; focus timing and final camera image are unverified;
  - the appearance choices and their defaults are Unknown; the host refuses individual choices and labels its one continuing default route;
  - removing an ability point;
  - the name grid's 26 lowercase letters and ten digits are established, but the 28 extended glyph identities and editing/acceptance branches are Unknown; the host keyboard types under the record's 31-byte maximum;
  - skill points at creation;
  - starting gear (`player_giveStartingItems` has no table).
- Card browser screens and slot lists. Host file choosers stand in for them.
- Use Gate Scroll: the play session has no recall path.
- The exit code the pause menu's exit countdown leads to. `return_to_frontend()`
  restarts with `frontend_start(0)`, labelled.
- Menu sound samples (see Layout): only the resource names are documented.
- Audit fixes (refused, each counted in the report): text widening of narrow lists (no width rule traced); front-end list text colour (fixture 0; the pause colour is not borrowed); class selection's menu sounds and its phase-1 wrapped text (class selection has no phase of its own); the branch that selects first-theme flags 5 instead of 1 (the host always asks for 1); executed cursor-frame claims for rows other than 0; in-game options table geometry and row count; phase-3 menu sound triggers.

## Flow (all native)

| Screen | Contract | Status |
|---|---|---|
| Boot card check | `save_init` defaults (image +0x08 = 0.5). An existing file is read strictly and copied over the image at browser step 29, mode 1 (bit for bit, no clamp). A blank card asks to create; yes writes the image | Executed / Ran |
| Logo | done after 240 frames, or on a press, which sets `g_skipSplash` and skips the film | Observed |
| Film | a press after 30 frames | Observed |
| Front end | `frontend_start` clears `g_paused`; the session's `game_addCoreTasks` copies image +0x08 to `g_musicVolume` and applies it; `mx_maintheme_l.vag` is queued | Observed / Executed |
| Main → Game type → Players → Mode → Difficulty | states 0, 1, 2, 4, 3 in that order; first entries only; `g_numPlayers` 1, `g_difficultyIdx` 1 | Ran |
| Character pages 1–4 | audited 13-position carousel and seven starting-stat rows; class from model (`player_initNewCharacter`); host's 20-point route preserves the one Ran class-6 example; name `char[0x20]`; start stats are read at run time from the user's executable at 0x4ef400 and refused unless they match the audited table | Observed / Ran / host for unverified branches |
| Done | `playerSel_finish` queues `hub`. A created character starts at 500 gold, level 1 and acknowledged level 1 | Observed |
| Options | music: type-4 slider, 1/120 steps, clamped to 0..1. The repeat timer gains 0x3e4fdef8 per call, so the first four calls are idle, and it resets when the cursor moves. A change previews `mx_maintheme_l.vag` (front end) or `mx_main_l.vag` (pause) through gated `music_playGlobal(…, 0x41)` when stream 2 is idle; `music_applyVolume` runs every call. Circle leaves through `config_commitOptions`; pause leaves query, then commit. Triangle opens card state 10 (choice 1 cancels; choice 2 loads and copies the 17 scalars, not +0x00 and not the button rows) | Executed / Observed |
| Load | `memcard_requestLoad` stores `g_inFrontend`. Native session saves (`.ocplay` / `.ocnsave`) or a PS2 slot import; a slot import asks for its saved level | host + Executed import |
| Save | opened from play (savepoint, script or host); offline it pushes `g_paused`; choosing stores latch 0x4f8f28 = 1; an error keeps it; the "saved" box clears it | Executed |
| Pause | start opens it; `g_paused` is set offline. Entries: Return to Game, Use Gate Scroll, Load Game, Options..., Exit Game. Kick Player is net only. The exit prompt opens on No; left then cross confirms. Closing commits the options (warpMenu 0x18e2f8). `recall_executed()` closes it with lockout 60 | Ran / Observed |
| Input | the pad goes through the executed `netwire::pad_apply`. Moves read the auto-repeat word +0x10 (press, then every 4th call after 20) (Inferred); confirm and cancel read +0xc; the lockout makes menus read a dummy pad and clears when nothing is held | Executed / Inferred |

Session exit codes 1, 0x65 and 0xc9 restart the front end with `frontend_start`
0, 1 and 2. What those arguments change is not documented.

## Host

- **Keys:**
  - arrows = d-pad
  - Enter, Space or X = cross
  - Backspace, C or Esc = circle
  - T or Tab = triangle
  - P = start
  On the name page, letters type.
- **Starting it:** the play scene's **Title / front end** action, or `-- --frontend`.
  In any running session, P opens the game pause menu, and Escape stays the host
  menu.
- **Options file:** the 760-byte image is kept at `user://frontend/options.img`
  (`--frontend-options=` overrides it). The card file name is unknown
  (`ps2_save::config_open_name` refuses).
- **Character:**
  - A created or imported character's documented record fields reach the in-game
    menus through `Ledger::adopt`.
  - The movement session does not read the player record (unchanged).
  - With a game folder open, the `hub` stem (or the slot's level) is loaded.
    Without one, the built-in play session starts and the overlay says so.

## Tests

- `frontend-exact` (CTest): 451 checks, including every asserted frame call, list anchor, sound trigger and the theme gate. Covers boot with and without a file, the
  Ran state order, auto-repeat timing, the slider walk against the executed model,
  commit and load, character creation, import, pause, lockout, save latch and exit
  codes.
- `godot/tests/frontend_smoke.gd` (124 checks): drives every screen on the play scene through
  the overlay, with saves, a PS2 import and the options file in a per-process
  `user://` directory.
- `godot/tests/frontend_capture.gd`: a game folder, every screen, PNGs to a given
  directory (keep it under `work/private/`). It needs a display.
