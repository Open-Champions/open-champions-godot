# Strings: the text table, wstr_format and the HUD text commands

`champions-strings` ports research `t3/research-strings` `d4987fd`
(`docs/strings.md`, `tools/ref_strings.py`) for Return to Arms US v2.00. It is
the string table, the wide-string formatter and the HUD state of the five
script commands `setInfo`, `addHelpMessage`, `addHelpMessageF`, `showBust` and
`showCompass`, plus `sound_playOnObject`'s slot choice and
`Creature_setCharm`'s `handle_clear` of a live target. Keys and texts are read
from the user's game folder at run time. The repository holds none.

## What is ported

| Part | Research | Native |
|---|---|---|
| `gtext.uni` walk, signed 13-bit hash, 8,192 slots, first copy of a repeated key wins, quotes dropped from a request, 16-entry `NF:` miss ring | 1.2–1.4 | `TextTable`, `uni_entries`, `text_hash` |
| `wstr_format`: every argument a float (`f12`–`f19`), `%%` `%d` `%c` `%s` (8-bit) `%u` (16-bit string) `%f` (`%.2f` of the R5900 widening) | 2 | `wstr_format`, `wstr_format_ascii`, `float_text`, `widen` |
| `setInfo`: 180 run ticks, the slot and local-menu gate, `hud_tickMessage` | 3.2 | `Hud::show_message`, `Hud::tick_message` |
| `addHelpMessage(F)`: one slot, the last call wins, pointers not copies, 90-frame close delay, chime, `ui_startPulse(0)`, net message 2 | 3.3 | `Hud::show_help`, `Commands::add_help_message_f` |
| `help_task`: the delay countdown, the offline pause (`g_paused`, `0x44fa80`), the confirm, the 60-frame delay and lockout, the post action | 3.3 (Observed, not executed) | `Hud::help_task` |
| `showBust`, `showCompass`: the shared gate, the 5-tick lease of `hud_run` | 3.4, 3.5 | `Hud::set_bust`, `Hud::set_compass`, `Hud::tick_widgets` |
| `sound_playOnObject`: the last free slot, the category cap, the handle, no seed read | 4 | `SoundSeqs::play_on_object` |
| `Creature_setCharm` with a live target | 5 | `set_charm` on the native `HandlePool` |

The English table is used, as the US build does (1.5). Only `GTEXT.LMP` is
loaded; a level lump's `.uni` joins the table after it while that level runs
(1.1).

## Refused on purpose

Each refusal throws `strings::Refused`. In the script host it is a counted
`callee_refusal` with a reason in `Counts::text_refusals`, checked before the
binder runs, so a refused command stores nothing and does not advance the
miss ring.

- **A format that would read past its terminator.** The original's end test
  looks only after a copied unit (2.3), so a format that ends in a conversion,
  in a lone `%`, or is empty copies its terminator and reads on. The port
  refuses it instead of reproducing the overread.
- **A text over its buffer.** Bust text 31 units, compass label 31, message 99,
  `addHelpMessageF` title 31 and body 127, a formatted text 255, a bust texture
  name 39 bytes. The original overwrites the next field. Only a setter that
  would store is refused: a gated call stores nothing in the original either.
- **No string table, no help chime, no camera.** Without a registered table the
  five commands refuse. The help box plays `fx.lmp/butn_message.anm`, whose
  sound group (byte `+0x3c`, research-audio `docs/formats/anim-audio.md` 2.1)
  comes from the game folder. `showBust` and `showCompass` read the local camera
  mode; the host has no camera until the caller sets `camera_mode`.
- **A missing compass object.** The handler has no null check (3.5).
- **`%s` and `%u` in an `addHelpMessageF` text.** They dereference address 0.
- **A ninth conversion.** It reads the formatter's own scratch buffer.

On the disc none of the text refusals is reached: no text the five commands use
is over its buffer, and no `addHelpMessageF` text at a call site ends in a
conversion (section "Disc counts").

## Script host

`script_commands::Host` routes the five commands (`docs/SCRIPT_COMMANDS.md`).
The binder's own calls are answered: `text_lookup` returns the text's address
(a host text address, not a binder one), `hud_showMessage`, `hud_showHelpMessage`,
`hud_setBust` and `hud_setCompassTarget` run the `Hud` setters, and
`wstr_format`/`wstr_copy` fill the two `addHelpMessageF` buffers in call order.
The gate inputs `g_hudSuppressed`, `g_localMenuOpen`, the load screen, the menu
flag and the network byte are read from the binder's words before each setter.
The open flag `0x41b928` (which `helpMessageActive` reads), `g_paused` and the
other help words are mirrored back.

`on_ready_tick` runs `HUD Run` (priority 50) before `Script` (125), as the task
table of research `docs/mainloop.md` orders them: the message timer and the two
countdowns step, then the deferred names run. `help_task(confirm)` is one
displayed frame of the draw task; a close runs the post action offline.

The help chime takes a slot in the host's 32 sequence slots with the observed
group limits (2, group 14 9, groups 50 and 51 8; research-audio `6b08886`
`docs/audio-playback.md` 4) and plays while frozen. The slots never finish:
`soundSeq_updateAll` is not ported, so the slot table is exact only until the
first sequence would have ended. Nothing else reads it, and the chime is not
played aloud.

`charmMonster` and `followPlayer` no longer refuse a live target word when it
is a node of the host's handle pool (`Host::link_target`). The clear runs on
that pool and leaves the target and every other handle to it. A target word the
pool does not hold is still refused.

## Play session and Godot

`ChampionsSessionController.set_text_sources(tables, chime_group)` takes
`GTEXT.LMP`'s `.uni` tables and the chime byte; `set_level_text` the level
lump's. `enable_level_script` registers them before `preStart` and sets the
camera mode to 1: `Camera_setMode` writes 0 only while this machine's
inventory, shop, dialog camera or a cutscene is up (research-ui
`docs/monsters-2.md` 7.1 M6), and the play session has none. An open offline
help box pauses the session until `help_frame(true)` closes it after the delay.

`godot/ui/text_hud.gd` draws over the real-data play view, at 60 displayed
frames per second, with `FONT.LMP`'s `gamefontmed.fnt` (installed as the default
font and as the font at `0x4f7100`, research-ui `docs/formats/ui-fonts.md`):

- the message centred at (320, 78) with its ctile frame and draw-side alpha
  (`hud_dynamic::messageFrame`);
- the help box: the title centred at (324, 100), the body wrapped at (132, 130),
  width 404, line 21, and the `0x17fbb8` frame at (116, 94), colours scaled by
  `g_uiPulse`;
- the bust panel without a maximum: the wrapped caption at (523, 391), the value
  centred at (542, 371), `fieldnumber.tex`, the portrait and `frame.tex` from
  `bust.lmp`, with the bust fade.

Enter confirms the help box (an authored key; the original's button is the byte
at `0x41b978`, undocumented). Not drawn: the help box's inner frame (its style
argument is not given), its button glyph and `Affirmation` label, the bust
meter (the maximum branch's second wrap is undocumented) and the compass (mesh
footprint unknown; its three disc keys are missing, so it would show `NF:`).
Drawn but unverified: the bust text font (the default font), the bust sprite
RGB (neutral), the bust art's field storage (woven: decoded unwoven, it shows two stacked half-height copies), the ctile pieces'
weave (as the documented tile pieces), the pulse decay in host floats and the
value as a decimal.

## Vectors

`tools/generate_strings_vectors.py` reads `tools/ref_strings.py`,
`tools/ref_core.py` and `docs/strings.md` with `git show d4987fd:` and refuses
to run unless each hashes to its pin. For seeds 20973 and 7 it runs the model on
synthetic cases and writes `native/tests/vectors/strings.tsv.gz` (52,308 lines)
and the manifest `strings.json`:

| Section | Cases per seed |
|---|---|
| `text_hash` | 4,000 keys of 0–60 bytes, a third of the bytes ≥ 0x80 |
| tables | 6 rounds: up to 3 blobs with colliding, repeated and quoted keys; every slot after each registration and unregistration; 700 finds and lookups with the miss ring entry and stale stack bytes |
| helpers | 600 `wstr_copy`, `wstr_fromAscii`, `wstr_toAscii`; 416 `%.2f` texts including edge patterns |
| `wstr_format` | 2,500 random formats with up to 8 float arguments, 6 8-bit and 6 16-bit strings; formats that read past their terminator are expected refused |
| `wstr_formatAscii` | 800 |
| HUD | 3,000 steps: the five handlers, the message setter, both timers and random gate inputs; the whole HUD state after each step |
| sound | 3 rounds of 400 steps: plays, finishes, object deaths, track links; all 32 slots and the handle pool after each play |
| charm | 3 rounds of 400 steps over 16 creatures and 4 lists; every list and the free list after each step |

`strings-exact` (`tools/run_strings_vectors.py`) checks the manifest hash and
replays every line: 82,076 checks. The research's seven mutations (unsigned
hash, 181 ticks, a 4-tick lease, a 60-frame open delay, `%f` of exponent 255 as
infinity, `setCharm` without its clear, the first free sound slot) each fail it.
The hand checks cover what the model does not: `help_task`'s 90 frames and
pause, the last call winning, the 5-tick lease and the observed group limits.

## Disc counts

`champions-strings-census --game rta DIR` (counts only) on the US v2.00 disc
copy, equal to research section 6:

| | Count |
|---|---|
| string files | 10 (6 `.uni`, 4 `.txt`) |
| English `gtext.uni` | 5,498 entries, 4,309 keys, 1,189 repeats, longest key 33, longest text 543, longest probe 261 |
| level scripts | 121, all loaded |
| `setInfo` | 95 calls resolved, 71 keys, 1 missing (`NF:`), longest text 47 |
| `addHelpMessage` | 65 calls resolved, 88 keys, 7 missing, longest text 169 |
| `addHelpMessageF` | 7 calls resolved, 11 keys, 6 texts `%d` and 8 none, 6 values not literal, none ending in a conversion |
| `showBust` | 31 calls resolved, 29 keys, 11 with maximum −1, 20 not literal |
| `showCompass` | 3 calls resolved, 3 keys, all 3 missing |
| refused by the port | 0 of the 201 calls |

Research section 6 says 2 `addHelpMessageF` texts end in `%d`. Neither the
research census nor this one finds such a text at an `addHelpMessageF` call
site; the 55 English texts that end in a conversion are elsewhere.
