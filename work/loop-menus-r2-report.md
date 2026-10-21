STATUS: COMPLETE — the documented front-end flow is built, wired and verified. Screen layouts, frames, colours and menu sounds are not in the research, so they are refused and labelled. The host draws a layout marked "HOST LAYOUT".

Menus round 2 on `codex/menus`, in worktree `~/Projects/open-champions/worktrees/menus`. All commits are local only: nothing pushed, no PRs, nothing written in `~/champions-re`.

## Setup: a deviation from the brief

- **No reset.** The brief said to run `git reset --hard codex/playable-integration` only if the branch had no commits of its own. It had three:
  - `500af57`, `235b633` and `3294d1f` (2026-10-21 01:16–01:28) are a menus round-1 implementation: inventory, shop, skills, stats, level-up, save index and pause.
  - That work is real, has a report (`work/loop-menus-r1-report.md`) and passed its own tests.
  - So I did not reset. The tip is kept as `backup/menus-r1-3294d1f`, and the base was merged forward instead.
- **No leftover Grok files.** `git status` was clean at the start. Nothing was stashed or moved.
- **Base.** At the start, `work/loop-playable-integration-r18-report.md` did not exist, so I first merged wave 17 (`c42ad2d`). The report appeared during the job: wave 18 is recorded at `0f3cafc` (STATUS COMPLETE). Merges:
  1. `c42ad2d`, wave 17 → `d7794fc`.
  2. `7948736` (`codex/save-import`) → `125b1d3`.
  3. `04d2f1a` (`codex/combat-ctors-music`) → `69e0ce0`. This is the existing port of the music-volume slider and its 0.5 default from research-combat-ctors 22408a1 and e9777eb. Reusing it avoided a second copy. That branch has been idle since 03:59, and its base is inside wave 17.
  4. `0f3cafc`, the wave-18 head → `1b8db76`.
  - `6df4a10` and the uncommitted netplay files after `0f3cafc` belong to the next wave and were not taken.
  - Two conflicts, both additive: `gdextension/CMakeLists.txt` and `play_flow.gd`.

## Commits (this round)

| Commit | What |
|---|---|
| `42db125` | `champions::frontend` (native): the boot, front-end, character, options, load/save and pause flow. `frontend-exact`. `Ledger::adopt` |
| `7858131` | `ChampionsFrontendService`, `godot/ui/frontend_menus.gd`, play-scene wiring, `frontend_smoke.gd`. Audio bridge `music_play_global`. The PS2 import reports the acknowledged level |
| `17f320c` | Overlay size fix and host placement. `frontend_capture.gd` |
| `0120e6b` | `docs/FRONTEND.md`, plus one ROADMAP line |
| `11a4d15` | Refuse an unrequested import result |

## What is documented and built

Sources:
- `t3/research-menus` 099900e
- `t3/research-ui` 3e39392
- `t3/research-saves` 8d43286
- `t3/research-audio` 6b08886
- `t3/research-combat-ctors` e9777eb

`docs/FRONTEND.md` has the full table. All state is native; Godot only maps keys, performs requests and draws.

- **Boot**
  - `save_init` defaults put 0.5 in image +0x08 (Executed).
  - Card check: a stored 760-byte options image is parsed strictly and copied bit for bit, unclamped, at browser step 29, mode 1 (Executed). A blank card offers to create one; yes writes the image (Ran).
  - Logo: 240 frames, or a press that sets `g_skipSplash` and skips the film (Observed).
  - Film: a press after 30 frames (Observed).
  - Front end start: `frontend_start` clears `g_paused`, `game_addCoreTasks` copies image → live and applies it, and `mx_maintheme_l.vag` is queued.
- **Front end:** states 0 → 1 → 2 → 4 → 3 in the Ran order, first entries only. `g_numPlayers` 1, `g_difficultyIdx` 1. Internet Game and the higher difficulties are refused.
- **Character creation**
  - Model → class (Observed).
  - 20 ability points must be spent; right adds one (Ran).
  - Name is `char[0x20]`.
  - Class start stats are read at run time from the user's executable at 0x4ef400.
  - Done → `hub` (`playerSel_finish`), starting at 500 gold, level 1 and acknowledged level 1.
  - Triangle → PS2 import.
- **Options: the music slider, exact**
  - 1/120 steps, clamped to 0..1, starting at 0.5. Timer 0x3e4fdef8: four idle calls, reset when the cursor moves.
  - Preview through gated `music_playGlobal(…, 0x41)`: `mx_maintheme_l.vag` in the front end, `mx_main_l.vag` in pause, only while stream 2 is idle.
  - `music_applyVolume` runs on every call.
  - `config_commitOptions` on leave. The pause leave queries first.
  - Triangle → card state 10, which copies the 17 scalars and not +0x00 or the button rows.
- **Load and save**
  - Load: native session saves, and the PS2 slot import, which asks for its saved level.
  - Save: opens from play. Offline it pushes `g_paused`. Latch 0x4f8f28 is set to 1 on choosing, kept on an error and cleared when the "saved" box closes (Executed).
- **Pause**
  - Start opens it; offline it sets `g_paused`.
  - Entries: Return to Game, Use Gate Scroll, Load Game, Options..., Exit Game. Kick Player is net only.
  - The exit prompt opens on No; left then cross confirms (Ran).
  - Closing commits the options (0x18e2f8). `recall_executed()` sets the lockout to 60.
  - Text colour 0x80464646 (Observed) is the only documented style. The overlay uses it.
- **Input:** the executed `netwire::pad_apply` provides press, held and auto-repeat. Moves read the auto-repeat word +0x10 (Inferred); confirm and cancel read the press word +0xc. The lockout gives a dummy pad until no button is held.
- **Wiring**
  - The session's `.ocplay` save and load, and the `.menus` companion, are unchanged.
  - The created or imported character reaches round 1's in-game menus through `Ledger::adopt`.
  - With a game folder, the queued level is loaded by stem. Without one, the built-in session starts and the overlay says so.
  - In any running session, P is the start button. Escape stays the host menu.

## Partial, refused and unknown (all labelled in `notes()` and on screen)

- **Layout and sound**
  - Positions, frames, fonts and colours of every screen except the pause text.
  - Every menu sound sample. Only a "browse sound" name exists.
  - Screen-to-screen fades.
- **Front end**
  - Main-menu entries after the first: the order is the host's, and the texts are those seen on screen.
  - The other entries of the players, mode and difficulty tables.
  - The PvP, 0x4f7124 and kill-limit values per choice.
  - Credits (`credits_run` not read).
  - The state-14 prompt.
  - `frontend_start(0/1/2)` differences.
  - The exit code the pause exit leads to. The host uses `frontend_start(0)`, labelled.
- **Character creation:** which models the page shows and in which order, appearance choices and defaults, point removal, the name grid's characters, skill points at creation, and starting gear.
- **Options**
  - Every option entry except music at index 1. The commit model covers music only.
  - The `force` and `noCardSave` arguments at the pause and front-end commit sites (0 passed, labelled).
  - The OR of word 0x88d3cc at boot.
  - The options file's card name: a host file `user://frontend/options.img` stands in.
- **Card screens:** the card browser and slot screens. Host file choosers stand in.
- **Use Gate Scroll** is refused. The play session has no recall path; this is a client gap, not a research gap.
- **Inferred:** left/right auto-repeat frames step the slider like the executed no-edge calls. Circle as "back" is host behaviour.
- **Possible discrepancy, not changed here:** research-audio says the front end's first theme queues (0x1e09b0, 0x1e09c4) "also lack 0x40" in a `music_playGlobal` paragraph. `play_menu_music` queues them ungated, with flags 1 or 5. This is worth an audio audit.

## Verification (arm64, this worktree's own builds)

- **Native build:** `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build/native -j 4`. Pass.
- **CTest:** `ctest --test-dir build/native --output-on-failure -j 3` → **215/215**, first run, 866 s.
  - That is wave 18's 211 plus `menus-exact`, `music-volume-host`, `music-volume-exact` and `frontend-exact`.
  - `frontend-exact` was 339 checks in that run. With the import guard it is 340: `PASS frontend 340 checks`.
- **GDExtensions:** rebuilt here from source: archive (template_debug), level texture (template_debug) and creature animation. godot-cpp is `integration/work/deps/godot-cpp`.
- **Godot dev matrix:** `python3 work/menus-r2/run-godot.py` (wave 18's runner plus `menu_smoke` and `frontend_smoke`) → **50/50 exit 0**.
  - `FRONTEND_SMOKE checks=85 failures=0 screens=20`
  - `MENU_SMOKE checks=36 failures=0`
  - `SCENE_PLAY_FLOW_PASS checks=65 failures=0`
  - Creature animation 32 / 25 / 231. Wave12 22.
  - Combined counts equal wave 18, except full combat 2,619 (was 2,620) and audio 2,577 (was 2,578), both with 0 failures. The merged `04d2f1a` replaced two music-refusal checks in the shared audio continuation cases with one check of the 0.5 default.
- **FULL_TRACE:** `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes, headless. Unchanged.
- **111-level sweep:** `work/menus-r2/run-slices.sh m2 --scripts` and `run-slices.sh m2n`, wave 18's driver args with its skill-recipe and level-light JSON. Aggregated against `r18` / `r18n`:
  - **111/111** loaded and played, scripts on and scripts off. 0 failures.
  - Every level, mesh, script, refusal, water and start count is equal.
  - The only differences: wall-clock `time.*_ms` and `audio.frames_pushed`, which are time-dependent (they differ between the baseline runs too), and the static peak, up about 2.3 MB summed over the four slices because the extension is larger.
- **Real-folder captures:** `Godot --path godot --script res://tests/frontend_capture.gd -- --game-folder=<disc> --capture-dir=work/private/frontend-r2 --cache=work/private/frontend-cache` (with a display).
  - Printed `levels 111 font_loaded true game_font true start_stats_refused false`, `start level hub loaded session true` and `captures 21`.
  - I looked at the captures: main, options, class, ability, playing, pause, pause options and exit prompt.
  - Menu entries draw in the game font loaded from the folder's first font row. The pause list uses the documented pause colour over the hub level. The two-column class list and the side-by-side Yes/No are host placement.
  - The first capture run exposed a 0×0 overlay (anchors without offsets). It was fixed in `17f320c`, and the captures were re-taken.
  - The 21 PNGs stay in gitignored `work/private/frontend-r2/`. No game imagery or disc values are committed.

## Files

- **Native:** `native/include/champions/frontend.hpp`, `native/src/frontend.cpp`, `native/tests/frontend_tests.cpp`, `native/frontend_targets.cmake`, `native/CMakeLists.txt` (one include line), `native/include/champions/menus.hpp` and `native/src/menus.cpp` (`adopt`).
- **Extension:** `gdextension/src/frontend_service.cpp`, `gdextension/src/menu_service.cpp`, `gdextension/src/native_audio_bridge.{hpp,cpp}`, `gdextension/src/ps2_save_import.cpp` (one field), `gdextension/CMakeLists.txt`.
- **Godot:** `godot/ui/frontend_menus.gd`, `godot/scenes/play_flow.gd`, `godot/tests/frontend_smoke.gd`, `godot/tests/frontend_capture.gd`, and the `.uid` files (including round 1's missing `menu_capture.gd.uid`).
- **Docs:** `docs/FRONTEND.md`, `docs/ROADMAP.md`.
