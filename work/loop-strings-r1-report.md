STATUS: COMPLETE

Job: strings, round 1. The research model of the string table, `wstr_format` and
the five HUD text commands is ported to native C++ with contract tests. The
commands run in the level-script host on the game's own `GTEXT.LMP`, and
`setInfo`, the help box and the bust panel are drawn in the Godot play view in
the game's fonts.

Worktree `~/Projects/open-champions/worktrees/strings`, branch `codex/strings`.
It was cut from `codex/playable-integration` at `ee4d03a`, which already held
`codex/script-commands-2` (`6df4a10`). Wave 19 landed during the round, so the
branch was rebased onto `ded5aab`. That base also holds hud-overlay `fbffecb`,
items and script-session. Everything below was rerun after the rebase.

- Local commits only: nothing was pushed, and no PR or issue was opened.
- `~/champions-re` was not touched.
- The disc copy was only read. Counts, logs and captures are in the gitignored `work/private/`.

Source of truth: `t3/research-strings` at `d4987fd`. The generator pins
`tools/ref_strings.py`, `tools/ref_core.py` and `docs/strings.md` by hash.

## Commits (on ded5aab)

| Commit | What |
|---|---|
| 86f1a07 | `champions-strings`: port of `ref_strings.py`, the vector generator, `strings-exact` (52,308 vector lines) |
| 9b359d5 | Host routing for the five commands, `champions-strings-census`, the measure tool's text sources, the controller (`set_text_sources`, `set_level_text`, `text_hud`, `help_frame`), `text_hud.gd`, `real_text.gd`, the smoke, `docs/STRINGS.md` |
| f35a242 | Godot uids |
| 1ea3f0b | The smoke waits for the help delay by time; bust art drawn woven |
| 000ab12 | The merged play HUD's refusals point at the text overlay |
| HEAD | Report (this file) |

The rebase had additive conflicts only, with items and script-session, in these files:
- `script_commands_targets.cmake`, `script_commands.hpp` and `script_commands_measure.cpp`;
- `session_controller.{hpp,cpp}`;
- `real_data_play.gd` and `ROADMAP.md`.

Both sides were kept in each. In the measure tool's `--bare` mode the text sources stay off, so bare still reproduces the earlier host.

## Ported, in the research's order

1. **The string table.**
   - The `gtext.uni` walk, an ASCII key followed by UTF-16LE text.
   - The signed 13-bit hash and 8,192 slots.
   - The first copy of a repeated key wins, and quotes are dropped from a request.
   - A miss gives `NF:` plus the first 16 key bytes, from a 16-entry ring.
   - English only. Every `.uni` of `GTEXT.LMP` is registered, then the level lump's.
2. **`wstr_format`.**
   - Every argument is a float, `f12`–`f19`.
   - Conversions: `%%`, `%d`, `%c`, `%s` (8-bit), `%u` (a 16-bit string) and `%f` (`%.2f` of the R5900 widening, so exponent 255 is a number, not infinity).
   - Refused:
     - a format that would copy its own terminator: one ending in a conversion or in a lone `%`, or an empty one;
     - a ninth conversion;
     - `%s` or `%u` in a help text.
3. **`setInfo`.** 180 run ticks, the slot and local-menu gate, and `hud_tickMessage`. HUD Run (priority 50) runs before Script (125).
4. **`addHelpMessage` and `addHelpMessageF`.**
   - No queue: the last call replaces the box, by pointer and not by copy.
   - The close delay is 90 frames on open and 60 on close.
   - Each opening plays the chime and calls `ui_startPulse(0)`.
   - The offline game pauses until the box is confirmed. Then the post action runs and the lockout is set.
   - The controller pauses the session while a box is up. `text_hud.gd` steps `help_task` at 60 Hz, and Enter confirms (an authored key).
5. **`showBust`.** The shared gate and a 5-tick lease that each call renews.
6. **`showCompass`.**
   - The same gate and 5-tick lease.
   - A missing object is refused.
   - Its three disc keys are missing, so it shows `NF:` as the original does.
   - The compass mesh stays refused.
7. **`sound_playOnObject`.**
   - Takes the highest free slot. The category cap uses the observed limits: 2, except 9 for group 14 and 8 for groups 50 and 51 (research-audio `6b08886`).
   - Draws no seed. The help chime uses it, and its group is byte `+0x3c` of `fx.lmp/butn_message.anm`, read from the user's folder.
8. **`handle_clear` on a live target.**
   - `charmMonster`/`followPlayer` no longer refuse a live `+0x234` held in the host's handle pool.
   - The clear removes only this creature's node and its partner.
   - A target word the pool does not hold is still refused.

Buffers: a bust or compass text over 31 units, a message over 99, an F title over 31, an F body over 127, a formatted text over 255 units or a texture name over 39 bytes is refused before anything is stored. The miss ring is not advanced either.

## Verification

| Check | Result |
|---|---|
| `strings-exact`: 52,308 vector lines from `ref_strings.py` (seeds 20973 and 7) on synthetic tables, formats, HUD sequences, sound slots and charm handle lists, plus hand checks | **82,076 checks, 0 failed**. All seven research mutations fail it: unsigned hash, 181 ticks, 4-tick lease, open delay 60, `%f` inf, no clear, first free slot |
| Host tests (`script-commands`) | Pass. New checks: the five commands on a synthetic table, the refusals, and a live-target charm. A probe confirmed the new checks can fail |
| Native CTest arm64, `ctest -j 3`, final rebased tree | **217/217**, first run |
| Godot dev matrix (wave-20 `run-godot.py`) | **51/51** |
| FULL_TRACE | `374e40c9…2035`, 21,990 bytes, **unchanged** |
| Real-level smoke `godot/tests/real_data_text.gd`, windowed, level index 3 | **14/14 checks**: tables, chime and font load; a real help box and the bust panel are drawn; the session pauses with no ready tick while the box is up; confirm closes it after the delay; ticks resume. Captures inspected: title, wrapped body, `0x17fbb8` frame, bust caption, number and portrait, with the merged base HUD |
| Sweep, 111 levels × 60 ticks, scripts **off**, vs base `ded5aab` | 111/111, 6,549 ticks, 230 checks: **equal** (only `audio.frames_pushed`, which is wall-clock, differs) |
| Sweep, scripts **on**, vs base `ded5aab` | 111/111, 6,549 ticks, 230 checks: equal. Changes are explained below |
| Disc census (`champions-strings-census`, counts only) | **95 setInfo, 65 addHelpMessage, 7 addHelpMessageF, 31 showBust, 3 showCompass calls resolved.** 0 of the 201 are refused by the port. Table figures equal research section 6 |

Changes in the scripts-on sweep (base → this branch):
- `addHelpMessage`: 8 refused → 8 executed, on 7 levels. The harness confirmed the 8 boxes after their delay (713 frames in all), and each ran its post action.
- `showBust`: 307 refused → 384 executed.
- `refusal_stops`: 16 → 14. Two scripts used the answer of a refused text command and stopped there. They now run on, which explains the rest:
  - more executed calls: `addParticleOnObject` +1,080, `getRand` +360, `countQuestItems` +241, `addDeferedCall` +137, `gamePaused` +212, `getScaledTime` +120, `getv` +60, `countPlayersAlive` +96, `removePlayerItem` +10, and +1 each for `callScript`, `setv` and `setSaveInhibit`;
  - +1 each for these refusals: `setCreatureParams`, `setAlternateDeath`, `savePlayer`, a stub refusal and a wait.
- No new fault.

The native measure tool (load, `start`, drain) shows the same pattern on the new base: 7 + 87 executed, where there were 7 + 75 refusals.

Commands:
- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j 4`, `ctest -j 3`;
- `tools/generate_strings_vectors.py --research-dir …/research-strings`;
- `work/strings-r1/build-ext.sh` (the three extensions, template_debug arm64);
- `work/strings-r1/run-godot.py`;
- `work/strings-r1/sweep.sh` (tags `b2-on`/`b2-off` against `n2-on`/`n2-off`, on a base tree from `git archive ded5aab`);
- `champions-strings-census --game rta DIR` and `champions-script-commands-measure --game rta DIR`;
- `godot --path godot --script res://tests/real_data_text.gd -- --game-folder=… --capture-dir=…`.

Before the rebase, the same checks also passed on `ee4d03a`: CTest 214/214, matrix 50/50, both sweeps 111/111 with equal ticks and checks.

## Not done, refused or unverified

- **No setInfo was reached in play.** The 95 calls sit in trigger and callback code that a standing player does not reach within the sweep's 60 ticks or the smoke's 600. The smoke shows a real help box and bust panel. The message path is covered by the host tests, the vectors and the drawing code, but no capture of a real `setInfo` was made.
- **Not drawn:**
  - the help box's inner frame (its style argument is not given);
  - its button glyph (byte `0x41b978`) and `Affirmation` label;
  - the bust meter (the second wrap of the maximum branch is undocumented, and 20 disc calls pass a non-literal maximum);
  - the compass.
- **Drawn but unverified:**
  - the bust text font (the default font, gamefontmed);
  - the bust sprite RGB (neutral);
  - the bust art's field weave: it decodes as two stacked half-height copies, so it is drawn woven;
  - the ctile pieces woven like the documented tile pieces;
  - the `g_uiPulse` decay in host floats;
  - the bust value as a decimal.
- **The chime is not heard.** Its slot is modelled, but `soundSeq_updateAll` is not ported, so slots never finish. The slot table is exact only until the first chime would have ended. Nothing reads it.
- **Research discrepancy.** Research section 6 says two `addHelpMessageF` texts end in `%d`. Neither the research census nor the native one finds such a text at a call site.
- **Scope.** The dialog branch has its own text table, and switching it to `champions-strings` at merge would remove the duplicate. `soundSequence` and `healPlayer` remain refused. No x86_64 CTest and no export builds this round.
- **Pre-existing.** An avatar import error appears in fresh Godot runs of this tree. It is unrelated to this work.
