# Front end — boot screens, main menu, character creation, options, load/save, pause

`champions::frontend` (`native/include/champions/frontend.hpp`) is the navigation
and state contract of Return to Arms' screens outside play, plus the game pause
menu. `ChampionsFrontendService` bridges it to Godot. `godot/ui/frontend_menus.gd`
draws a **host layout** over the play scene and performs the flow's requests.

Evidence pins:

| Research | Head | Used for |
|---|---|---|
| t3/research-ui | 3e39392 | `docs/mainloop.md` 8 (boot order, modal loops, exit codes), `docs/ui.md` "Front end" (states 0–15, setup writes), `docs/player.md` (record fields), `docs/data-tables.md` 4–5, `docs/issues/live-rig.md` 4a and 21.7 (Ran), `docs/issues/native-online.md` (pause entries, Ran), `symbols/mainloop.tsv` (splash timings) |
| t3/research-menus | 099900e | `docs/menus.md` 1, 6, 7 (masks, pause font and colour, save index) |
| t3/research-saves | 8d43286 | `docs/save-format.md` (the 760-byte options image, card states, save latch) |
| t3/research-combat-ctors | e9777eb | `docs/spec/music-volume.md` (slider, commit, boot copy; ported as `music_volume.hpp`) |

Status words are the research's: **Executed**, **Observed**, **Ran** (seen on the
emulator), **Inferred**, **Unknown**.

## What is not in the research

None of these screens' positions, frames, fonts, colours or sound samples are
documented. The one exception is the pause menu's text: font word 0x4f7100, layer
7, colour 0x80464646 (Observed). The host therefore draws its own layout, labelled
"HOST LAYOUT" on screen, and `Flow::notes()` lists every gap for the current
screen. Things the flow refuses or leaves out:

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
  - which models the class page shows, and in which order;
  - the appearance choices and their defaults;
  - removing an ability point;
  - the name grid's characters (the host keyboard types);
  - skill points at creation;
  - starting gear (`player_giveStartingItems` has no table).
- Card browser screens and slot lists. Host file choosers stand in for them.
- Use Gate Scroll: the play session has no recall path.
- The exit code the pause menu's exit countdown leads to. `return_to_frontend()`
  restarts with `frontend_start(0)`, labelled.
- Menu sounds: no sample is documented for any cursor, confirm or cancel in these
  screens.

## Flow (all native)

| Screen | Contract | Status |
|---|---|---|
| Boot card check | `save_init` defaults (image +0x08 = 0.5). An existing file is read strictly and copied over the image at browser step 29, mode 1 (bit for bit, no clamp). A blank card asks to create; yes writes the image | Executed / Ran |
| Logo | done after 240 frames, or on a press, which sets `g_skipSplash` and skips the film | Observed |
| Film | a press after 30 frames | Observed |
| Front end | `frontend_start` clears `g_paused`; the session's `game_addCoreTasks` copies image +0x08 to `g_musicVolume` and applies it; `mx_maintheme_l.vag` is queued | Observed / Executed |
| Main → Game type → Players → Mode → Difficulty | states 0, 1, 2, 4, 3 in that order; first entries only; `g_numPlayers` 1, `g_difficultyIdx` 1 | Ran |
| Character pages 1–4 | class from model (`player_initNewCharacter`); 20 ability points must be spent and right adds one; name `char[0x20]`; class start stats are read at run time from the user's executable at 0x4ef400 | Observed / Ran |
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

- `frontend-exact` (CTest): 339 checks. Covers boot with and without a file, the
  Ran state order, auto-repeat timing, the slider walk against the executed model,
  commit and load, character creation, import, pause, lockout, save latch and exit
  codes.
- `godot/tests/frontend_smoke.gd`: drives every screen on the play scene through
  the overlay, with saves, a PS2 import and the options file in a per-process
  `user://` directory.
- `godot/tests/frontend_capture.gd`: a game folder, every screen, PNGs to a given
  directory (keep it under `work/private/`). It needs a display.
