STATUS: COMPLETE — the documented draw calls of every screen the research covers are native and painted with the user's own fonts and textures; Unknown parts stay a labelled host layout. All requested gates pass.

# Menus round 3 — documented front-end layout

Branch `codex/menus`, worktree `~/Projects/open-champions/worktrees/menus`. Local commits only: nothing pushed, no PRs, nothing written in the research repository (only its existing worktree `research-menus-layout` was read). The disc is read-only; only counts appear here, and no game data is in any tracked file.

## Base and evidence

- Base is wave 23 `b75c174` (`work/loop-playable-integration-r23-report.md` exists). It is already the merge-base of this branch and `codex/playable-integration`, so no merge was needed. The task named wave 22 `af9e3b5` and `815a8f3`; wave 23 is contained in the branch.
- Source: `t3/research-menus-layout` dc458d5 (audit PASS): `docs/frontend-layout.md`, `tools/ref_frontend_layout.py`, `tools/ref_frontend_screens.py` and the two difftests' tables. I re-derived the list anchors and frame calls by hand against the research's asserted calls: 4-row `(300,259,40,33)`, `(292,248,56,122)`, `(301,260,54,114)`; pause `(292,168,56,144)`, `(300,179,40,33)`, `(301,180,54,136)`; options backing `(214,78,212,320)`. All match.
- The character-screen colours (`0x325d18`, `0x68c7cc`, `0x316864`, `0x585e5d`) and anchors trace to `PLAYERSEL_PHASES` rows.

## Commits

| Commit | What |
|---|---|
| `0c2189c` | Native: `champions::frontend::Scene/Draw`, `Flow::layout()` and `layout::` helpers (`frontend_layout.{hpp,cpp}`). List geometry, the three frame calls, title and logo sprites, footer, message box, slot list, ability and name screens. Menu sound requests at the documented triggers. The first theme and the credits handoff go through the one `audio::music_gate_open`. `frontend-exact` 439 checks |
| `291c800` | Godot: `ui/frontend_view.gd` paints `Flow::layout()` (sprites from SOELOGO and FRONTEND, `ctile`/`tile` frame pieces from INTERFAC through `menu_frame`, `gamefontmed`/`gamefontsmall` through `text_layout`) under the layer painter rule. `frontend_menus.gd` uses it. Smoke: 124 checks |
| this round | `docs/FRONTEND.md` "Layout" section and evidence pin, ROADMAP line, `frontend_view.gd.uid` (missing from `291c800`), this report |

## What is drawn from the research

- **Lists** (main, game type, players, mode, difficulty, options, pause): medium font, layer 7, first row y = origin − (11·rows + 6), step 22, value rows 23 left. Three frames: cursor (style 0, `0x300000ff`, height 32 or 33 by y parity), backing (style 2, `0x80808080`), overlay (style 0, `0x3f000000`, layer 6). Row 0 is Executed. Other rows and widening for text width are labelled Inferred.
- **Sprites:** title `(141,15)` layer 5, states 0–4 only. Logo `(192,330)` and `(64,50)`, layer 3. Both tint `0x80808080`. Textures come from the user's disc at run time.
- **Fonts and colours:** per batch from the research's font table (small for card and character batches; medium for lists, name entry and the pause text `0x80464646`).
- **Footer, message box, slot list, import and character summary:** the executed anchors, batch colours, frame calls and highlight rectangles. Label text is host-authored; the research stores no localized string.
- **Cursor and highlight:** the style-0 list frame, and the Inferred slot-list rectangle `(66,133)–(574,152)` for the selected row.
- **Sounds:** `menu_main_toggle`, `menu_main_accept` and `menu_main_back` (`FX.LMP`) are requested at the Executed triggers (main-list move, main accept, game-type cancel, selection phases 1 and 3). They are **counted, not voiced**: the research has resource names only, not the clips' sample tables, mix level or group volume `0x41bac8`. Refused rather than invented.
- **First theme:** `mx_maintheme_l.vag` flags 1 is queued only when music volume is at or above the research gate `0x3d4ccccc`. Below it nothing is queued and the refusal is counted. Covered in `frontend-exact`.
- **Animation:** only logo 240 frames, film press after 30 frames and the credits scroll rule are documented. The scroll has one authored fixture line and no real data layout, so it is not drawn. Fade curves are not documented and none is drawn. The alpha byte uses the settled `0x80` (Inferred, noted in `notes()` and the docs).

## Still a labelled host layout (research says Unknown)

Class art, the 3D pose and per-race animation, appearance art, options value bars and knobs, card rows with real file data, the intro film picture, the credits layout, the Exit and options-load prompts. Each shows "HOST LAYOUT — …" and `Flow::notes()` lists the gaps.

## Verification (arm64 Release, this worktree)

| Gate | Result |
|---|---|
| `cmake --build build/native -j 4`; `ctest --test-dir build/native --output-on-failure -j 3` | **226/226** pass, 917.89 s (shared machine load; wave 23 took 647.83 s). `frontend-exact` passes, 439 checks. The test binary is arm64 |
| `cmake --build build/godot-extension -j 4` (template_debug) | pass; the `godot/bin` dylib is newer than all changed sources |
| Godot dev matrix, `work/menus-r3/run-godot.py` (wave 23's runner, output dir changed) | **54/54 exit 0**. `frontend_smoke` 124 checks, `menu_smoke` 36 checks, 0 failures |
| Dev headless Full (`combined-full-combat`) | 2,620 checks, 0 failures. **FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes — identical to wave 23** |
| 111-level sweep, `run-slices.sh m3 --scripts` and `m3n`, four slices, 60 ticks, compared with `w23on`/`w23off` | both **111/111** tried, loaded and played; 230 checks, 0 failures, empty refusal list. Every deterministic count equals wave 23 (script commands, refusals, dialog, HUD, water, levels). Only `audio.frames_pushed`, `memory.static_peak_bytes` (+1.5 MB, larger extension) and `time.*_ms` differ, and those are run-dependent |

Not run: the export and Rosetta/x64 matrices of wave 23. This change touches no native simulation, so they were outside the requested gates.

## Screenshots

`work/menus-r3/shots/*.png`: 21 screens, git-ignored because they contain the user's textures and fonts. They come from `frontend_capture.gd` on the private disc (`work/menus-r3/capture-2.log`: `captures 21`). I viewed the main menu (title, framed list, cursor), the ability screen and the SOE logo. Logs are in `work/menus-r3/`.

## Remaining and caveats

- `frontend_capture.gd` prints a script error at `real_data.gd:280` (`class_call_static` of `ChampionsRealWorld.decode_texture` returns an int) when it starts the hub level. It comes from the base `real_data.gd`, not from this change, and the capture still completes. I left it alone as out of scope.
- Sounds stay unvoiced until the research captures the `.anm` sound tables for these three clips.
- Pixel-exact finished screens need a displayed capture of the original (title size, clip and fade; text-width frame expansion; intro picture). The research itself says MORE for those.

## Audit fixes

Source: `native-menus.md` (Claude audit, verdict FIX, 0 HIGH, 5 MEDIUM, 5 LOW). Evidence is the pinned research `t3/research-menus-layout dc458d5` (`docs/frontend-layout.md`, `tools/ref_frontend_layout.py`, `tools/ref_frontend_screens.py`). No new research was done. Each finding was either traced and cited or refused, labelled and counted.

| Finding | Action |
|---|---|
| M1 list text colour `0x80464646` | **Refused.** `0x80464646` is the pause batch only (0x18d5e8). The front-end list batch 0x1a09e8 has fixture colour 0 and the parent batch 0x1e0dcc `0x64464646`. Front-end list rows now use a Host identity tint `0x80808080`, `Evidence::Host`; the test asserts only font, layer and anchor. |
| M2 text widening | **Refused.** No width rule is traced (`frontend-layout.md:65,247`). `list_frames` emits only the executed minimum-width calls; the `extra` and `widths` plumbing is removed through the extension and `frontend_menus.gd`; the widening test is replaced by a minimum-width test. |
| M3 invented alpha `0x80` | **Traced.** `Draw.colour` now carries the research word verbatim (`0x0A316864`, `0x08325D18`, `0x08585E5D`, `0x0068C7CC`, `0x001E6464`, `0x808080` for the message-box wrapper). A new `Draw.settled` flag says the alpha byte is fade state; the host (`frontend_view.gd`, `_tint`) paints it opaque. That paint choice is labelled Host in `docs/FRONTEND.md`. |
| M4 colours on the wrong calls | **Fixed from `PLAYERSEL_PHASES` call order:** phase 3 header and description `0x0A316864`, points row and value `0x08325D18`, unselected stat rows `0x08585E5D`, selected `0x0068C7CC`. Phase 2 wide text `0x0A316864`. Test asserts all eight. |
| M5 class-selection sounds and `(41,29)` text | **Refused.** Class selection has no phase of its own (`frontend-layout.md:132,146`); phase 1 is import selection. `CharClass` plays no menu sound, the `(41,29)` call is a Host text, and the class-page footer is Host. Phase-3 sounds are kept. Test: no `menu_sound` request on class confirm. |
| L1 slot-list highlight over the label | Fixed: the row text is submitted before the highlight (later submission paints first). |
| L2 NaN slider | Fixed: `!(live >= 0)` is rejected, then `min(live, 1)`. Test walks NaN, -1, +inf and 2.0. |
| L3 pause and in-game options labels | New `Evidence::Observed`; the pause rows and the in-game options rows are Observed for font and colour (geometry stays Executed in the frames). `docs/FRONTEND.md` says so. |
| L4 first-theme flags 5 | Documented in `Flow::notes()` and at the call site; the host always asks for flags 1 (condition not documented). Not changed. |
| L5 Host-labelled coordinates | Accepted as labelled Host layout (no change). |

Refusals counted: 4 behaviours (text widening, front-end list colour, class-selection sounds and phase-1 text, flags 5 branch). Other changes: `godot/tests/frontend_smoke.gd` now matches the `list row` role prefix.

Verification (arm64 Release, `work/menus-r4/`):

| Gate | Result |
|---|---|
| `ctest --test-dir build/native -j 3` | **226/226** after one rerun: `player-exact` timed out under machine load (load average ~130) in the full run and passes alone (221 s). `frontend-exact` 451 checks. |
| `cmake --build build/godot-extension` | pass |
| Godot dev matrix (`work/menus-r4/run-godot.py`) | **54/54 exit 0**; `frontend_smoke` 124 checks 0 failures (first run caught 3 stale role-name checks, fixed and rerun alone), `menu_smoke` 36 checks 0 failures |
| FULL_TRACE (`combined-full-combat`, 2,620 checks, 0 failures) | `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes, **identical** to round 3 |
| 111-level sweep, `r4` (`--scripts`) and `r4n` | both **111/111** tried, loaded and played; 230 checks, 0 failures, empty refusal list. Every deterministic count equals `m3`/`m3n`; only `audio.frames_pushed`, `memory.static_peak_bytes`, `text.frames_drawn.bust` (27 vs 32, timing) and `time.*_ms` differ |

Not run: screenshot recapture (the front-end draws changed only in colour, widening and sounds).

## Audit fixes — final audit (2026-10-22)

Source: `native-menus-final.md` (2 MEDIUM, 5 LOW), against pinned `t3/research-menus-layout dc458d5`. This supersedes the earlier claims that phase-3 sound triggers, nonzero cursor rows, and in-game options geometry were traced.

| Finding | Resolution and pinned evidence |
|---|---|
| M1 | **Refused native frame geometry for selected rows other than 0.** `tools/ref_frontend_layout.py:48-70` and `docs/frontend-layout.md:60-65` cover row 0 only. Other rows keep a visible `Evidence::Host` highlight; the self-derived coordinate test was replaced by an evidence-label test. |
| M2 | **Refused the front-end 13-row table for pause options.** `tools/ref_frontend_layout.py:20,31` and `docs/frontend-layout.md:111` cover the front-end table `0x4f6e88`; the in-game `0x44f218` count and layout are uncaptured. Pause options now show only two Host controls, no Executed frames or Observed pause-batch colour. |
| L1 | **Refused phase-3 sound triggers.** `docs/frontend-layout.md` lists sites but not input conditions. `CharAbility` no longer requests accept/back clips. The documented main-list triggers remain. |
| L2 | Message-box frame style, layer, opacity and inflation are `Evidence::Host`; `tools/ref_frontend_screens.py:740` gives only the wrapper tuple. |
| L3 | Host character, name and exit rows use the Host identity tint. `docs/frontend-layout.md` ties `0x80464646` only to pause text batch `0x18d5e8`. |
| L4 | Removed the claim that the import-row rectangle is drawn. Host class footer has no research site; phase-2–4 footer prompt retains `0x229cb4`, as recorded in `tools/ref_frontend_screens.py`. |
| L5 | **Refused blanket low-alpha fade treatment.** Only the slot-list batch sets `Draw::settled`; `docs/frontend-layout.md:88,177-180` and the credits colour points support fade. Other research words retain their alpha in `Draw.colour`. The Godot view paints its authored placeholder text at a readable Host opacity; Host highlights use explicit Host opacity. This is labelled presentation, not a claim about native fade. |

New refusals counted: **4 behaviour claims** (M1 cursor rows, M2 pause options table, L1 phase-3 sound triggers, L5 blanket fade conversion), **8 total** including the four earlier audit refusals above. L2 and L3 remain rendered as labelled Host choices. No private bytes or names were added to tracked files. Current verification follows below.

Verification on the final menus tree (arm64 Release):

| Gate | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`; `ctest --test-dir build/native --output-on-failure -j 3` | Configure/build pass. Full CTest **224/226** in 2,579 s; `player-exact` hit its 240 s timeout and `session-facing-exact` hit its internal 600 s timeout under shared worktree load. The initial failures are retained. Focused `frontend-exact` passed. Isolated `player-exact` passed in 239.67 s and `session-facing-exact` passed in 274.67 s. |
| `cmake --build build/godot-extension -j 4` | Pass (macOS template_debug). |
| Godot dev matrix, `work/menus-r5c/run-godot.py` | **54/54 exit 0** after the Host text presentation fix; `frontend_smoke` 124 checks, 0 failures. The first matrix was 53/54 because the rendered material test sampled before `frame_post_draw`; the isolated failure was reproduced, the test was fixed, and the full matrix passed twice afterward. |
| FULL_TRACE, `combined-full-combat` | **Unchanged** SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes. |
| 111-level sweep, `m5on --scripts` and `m5off` | Both **111/111** tried, loaded and played; 230 checks, 0 failures, empty refusal lists. Every deterministic count matches `r4`/`r4n`; only timing, audio frames, static peak memory and text frame counts were excluded from that comparison. |
| Rendered front-end capture | 21 screens captured with the read-only disc in ignored `work/private/`. Main, ability and pause-options screens inspected. The first ability capture exposed unreadable Host text; after the Host opacity fix the recapture is readable. The existing base `real_data.gd` static decode error still appears during hub startup, but all 21 captures complete. |

The rendered material test's `frame_post_draw` wait is a test timing fix: it waits for the viewport's new frame before checking alpha pixels. It changes no game material code.
