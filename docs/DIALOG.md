# NPC dialog

`champions-dialog` is the NPC dialog machine of Return to Arms (US v2.00).
Research is `t3/research-dialog` at `334578d`: `docs/dialog.md` (the
contract), `tools/ref_dialog.py` (the reference model), `tools/difftest_dialog.py`
and `tools/dialog_census.py`. The policy is `rta-dialog-v1`. The hashes of
those files are in `native/tests/dialog_contract.json`. Section numbers below
are the research document's.

The gaps `docs/dialog.md` left open (the camera, 24 more `dialog_isOpen` sites,
the box in `intro` and arena mode B, the cursor glyph, the `TestLocation`
talkers) are `t3/research-dialog-gaps` at `79a2e4a`, `docs/spec/dialog-gaps.md`;
see "The dialog gaps" below. Section numbers there are that document's.

Public tests use authored conversations, scripts and objects. The game's
conversations, text and voices are read from the owner's game folder at run
time and are never copied into this tree.

## The machine

`native/include/champions/dialog.hpp` ports the reference function by
function:

| Original | Port |
|---|---|
| `.dbin` layout (section 3) | `parse_dbin`, `build_dbin`, `start_candidates`, `reachable_nodes` |
| `dialog_start` 0x22fa50, message 0 | `Dialog::start`, `start_message`, `parse_start_message` |
| `dialog_onMsg` 0x22ee2c types 0-4 | `on_open`, `on_confirm`, `on_move`, `on_end`, `on_leave`, `on_message` |
| start node loop 0x22ef7c | `pick_start` (a condition whose entry is missing returns -1 and counts as true) |
| `dialog_showNode` 0x230c74, `dialog_queueLineAudio` 0x22ec9c | `show_node`, `queue_line_audio` |
| `dialog_runTask` 0x22ff48 | `tick` |
| `dialog_drawTask` 0x230628 | `draw` |
| `dialog_startTalkAnim` 0x22fc70, `dialog_syncMouth` 0x22fdd8 | `talk_anim`, `sync_mouth` |
| `dialog_free` 0x22f99c | `free` |
| `Creature_interact` 0x116bd8, `Creature_trigger` 0x11a5f0, the menu mutex | `interact`, `trigger`, `mutex_try`, `mutex_release` |
| `changeSwoop`, `startWithSwoopDone`, `newWorldPost`, `activateStore`, `activateMissionMenu` | the built-in methods |

The machine keeps only its own words. What it reads from other subsystems
comes from a `dialog::Host`, and every call it makes outside its own state is
appended to `log` in the original's order. Floats are raw patterns on the
existing R5900 arithmetic (`champions/core_arithmetic.hpp`). Where the
reference raises (the original would read through a null pointer or run off a
buffer) the port throws `dialog::Refused`.

Two hooks are not in the reference model: `on_send` hands a message to the
handler as it is sent (offline `net_sendGameMsg` calls the handler at once,
research `docs/spec/game-messages.md` 1.2), and `on_post_action` runs
`script_runPostAction` at its place in the end handler. The reference only logs
both. The contract test leaves them unset.

`TextTable` is `gtext.uni` as research `docs/data-tables.md` section 6
describes it: the first of two equal keys wins, double quotes are dropped from
the key, and a miss returns `NF:` and the first 16 characters of the key.

## Contract test

`tools/generate_dialog_vectors.py` drives `ref_dialog.py` through the ten
suite shapes of `difftest_dialog.py` (message, open, messages, tick, draw,
free, interact, builtins, mouth, sequence) and a `.dbin` suite, with seeds
20973 and 7: 1,260 cases, 10,943 operations, 7,434 run-task ticks. After every
operation the vectors hold the result or the refusal, the log, the scripts
run, the inputs asked for, every state field that changed and every object
that changed. The `dialog` CTest replays them on the port and compares all of
it: 1,122,292 checks. With the contract and quirk checks and 40 host checks
(below) the test makes 1,122,341. The host checks run on an authored
script, `objects.ob` and `.dbin`: a whole conversation through the use
button, a missing condition entry, a hidden choice, actions, quest state, the
mission-menu gate, the deferred shop, the post action, the pending level and
the swoop out; and the refusals.

```
python3 tools/generate_dialog_vectors.py --research-dir ../../champions-re-wt/research-dialog \
    --tsv native/tests/dialog_vectors.tsv --contract native/tests/dialog_contract.json
```

## Level-script host

`script_commands::Host::attach_dialog(level, services)` opts a level-script
host in. Without it nothing changes: the dialog built-ins stay refused and
`dialog_isOpen` answers 0.

- **Commands.** With a dialog attached, `startDialog`, `dialogRunning`,
  `forceMutexRelease`, `activateStore` and `newWorldPost` run their binder
  bodies instead of being refused. Their seams (`net_isLocalSlot`,
  `net_sendGameMsg`, `menuMutex_release`, `shop_openOrStock`,
  `net_closeLocalMenus`, `lump_load`, the camera calls) are answered by
  `native/src/script_dialog.cpp`. `dialog_isOpen` answers the machine for every
  command, so `activateMissionMenu` is refused while a dialog is open
  (section 6.1).
- **Shared words.** The binder and the machine share `g_dialogData` 0x8e2d60,
  the swoop words 0x4fdce4 / 0x4fdce8, the `newWorldPost` buffer 0x4fdcf0, the
  pending store 0x4fdd30 and `g_scriptActivePlayer` 0x4d8ae4. They are copied
  into the binder's memory before every command and every script the machine
  runs, and back after.
- **Messages** run at once, nested in the sender, as offline
  `net_sendGameMsg` does. Message 0 finds `<name>.dbin` in the level's lump
  under its exact name and the speaker among the host's objects named `<name>`
  (`world_findObjectsByName`, limit 100).
- **Use button.** `use(slot, position)` is `Player_interact`'s box (+-48 in x
  and y, the player's height to 60 above, each object's radius and height as
  `world_collectObjectsInBox` tests them), the nearest interactable by squared
  distance (first found wins a tie), then `Creature_interact`. Nothing happens while a dialog is open.
  `interact(address, ...)` calls `Creature_interact` directly.
- **Order.** The play session runs, per ready tick, the use button (priority
  10), `dialog_tick` (the run task, 110), then the Script task (125), as
  research `docs/script.md` 2.6 lists them. While a dialog is open the
  session's player commands are gated: `Player_run` takes no input and
  `Player_move` zeroes the horizontal velocity (section 6.1).
- **End handler.** Message 3 frees the mutex and the HUD word, writes the
  `newWorldPost` level to 0x41b6d0 with 0xff at 0x41b720 and to level flow's
  pending request, runs the post action in place, and hands a store asked for
  during the dialog to the presentation (`shop` event; the shop itself is not
  in this host).
- **Choice actions** run synchronously inside message 1. Mission unlocks, quest
  add and remove, `grantMission`, quest variables and good/evil reach the
  [quest and mission model](QUESTS.md) as they do from any other entry.
- **The `.anm` read.** The original waits on `lump_pollAsync` until a lip-sync
  read ends. This host reads from memory, so a read issued by one machine step
  is complete by the next one.

### Who can be talked to

A talker is an `objects.ob` record with `showName` and `dialogScript`
(property keys without case, first entry wins, as `dialog_census.py` counts)
whose class reaches the base `Creature_interact`: `NPC` (research
`docs/monsters-2.md`), `GnomeNavigator`, `MattMarr` and the `Orc2*` factory
names (research `docs/monsters.md` class table). The host's records are
unconstructed stubs, so the machine's view of a talker carries the flags its
documented constructor sets: 0x20 from `Creature_ctor`, 0x1100008 for `NPC`,
0x100000 when `showName` is present. Nothing else of the constructor is
applied. `TestLocation` (2 records on the disc) is in no class table and is
refused. The use button considers every documented-class object with
`showName`; the talk-to diagnostic and the census use the ones with
`dialogScript`.

## Godot

`ChampionsSessionController.enable_level_script(script, objects,
carry_campaign, dialog)` takes the level's `.dbin` entries, its
`ntsc_dialog.bin` directory and VAT size, `gtext.uni` and `gamefontmed.fnt`
(read from the game folder by `real_data/real_data.gd`) and attaches the
dialog before the level entry runs. In the real-data play scene, with the
level script on:

- **E** uses (`dialog_use`), **Up** / **Down** choose and **Enter** or
  **Space** confirm (`dialog_press` with 0x1000, 0x4000, 0x40: bits of the
  documented up, down and confirm words). The confirm mask 0x40 is the value
  observed on the live rig (research `docs/issues/live-rig.md`).
- `dialog_frame` turns `dialog_drawTask`'s calls into the `box.tex` sprite
  (560 x 156 texels at (35, 280), plus the row `ui_drawSprite` adds) and
  gamefontmed glyphs laid out by `champions/ui_text_layout.hpp`;
  `ui/npc_dialog_view.gd` paints them once per displayed frame, box under text.
- **Voice.** `dialog_events` hands the stream-1 calls to the native mixer:
  `<voice>.vag` is queued held with flags 0x28 (a line with no entry queues
  nothing and stays silent), `clear_hold` releases it, `audio_streamStop` stops
  it (`ChampionsNativeAudio.stream_stop`). The stream's queue count, status and
  position come back through `dialog_voice_state`. The held entry reports
  ready at once. The position is the bytes played, from the samples played at
  28 per 16-byte block (the conversion of section 5.4); whether
  `audio_streamPosition` counts played or buffered bytes is not stated (the
  0x400 in `dialog_syncMouth` suggests a lead), so lip-sync alignment is
  unverified.
- **Mouth.** The host calls `dialog_startTalkAnim` and `dialog_syncMouth` where
  the run task asks for them. The real-data scene draws no NPC models, so the
  resulting animation calls are counted and handed on, not shown.
- `dialog_talk_to(i)` is a labelled diagnostic, not an original routine: it
  calls `Creature_interact` on talker *i* from 30 units in front of it.
  `dialog_leave` is the client's way out of a conversation a refusal stalled.

## Quirks kept, and where the port refuses

| Original behaviour | Here |
|---|---|
| The visible-choice list 0x8e2d20 holds 10; entries 10 and 11 land on the text length and the reveal count | Kept up to 10. More than 10 visible choices are refused (the disc has at most 8) |
| A node stores 0..12 choices | `parse_dbin` refuses 13 or more, and a jump outside the file |
| Choice rows 0x8e2ce8 are not cleared between nodes | Kept: the cursor can read a stale row |
| A missing condition entry returns -1 and counts as true; a node condition can make node 0's own condition run | Kept |
| No cancel: a conversation ends only through a choice with next -1 or a node without choices | Kept |
| The mutex is taken before `dialogScript` runs and stays held if no dialog opens | Kept: the use button also takes objects with `showName` and no `dialogScript` (5 on the disc), which hold the mutex. Offline the inventory's close frees it; this session has none, so `dialog_leave` frees it instead (client action) |
| A talk request during the mission menu or a recall is dropped and gives the mutex back | Kept |
| The text buffer 0x4fdd38 has no bound check | Text of 0x400 units or more is refused |
| No speaker object makes the original read address 0xc | Refused |
| The side word 0x8e2cc4 and the span 0x8e2d58 are written and never read; 0x22fc44 has no caller | Kept as written-only state |
| The menu mutex has no owner | Kept: any machine step can release it |

When a script the machine needs refuses its answer (an item command such as
`playerHasItem`, research-items is running), or a step is refused above, the
host puts the machine back where it was before that step. If no dialog was
open, the menu mutex is given back, so the player can try again. If one was
open, the conversation is marked stalled and stays on screen until the client
leaves it (`dialog_leave`, not in the original). Scripts that already ran keep
their effects. A missing text key that has double quotes is refused: which
spelling the `NF:` text copies is not stated.

## The dialog gaps

`champions-dialog-gaps` (`native/include/champions/dialog_gaps.hpp`) ports
`tools/ref_dialog_gaps.py` of `t3/research-dialog-gaps` `79a2e4a`, policy
`rta-dialog-gaps-v1`, hashes in `native/tests/dialog_gaps_contract.json`. The
`dialog_gaps` CTest replays 6,293 reference cases (seeds 20973 and 7) generated by
`tools/generate_dialog_gaps_vectors.py`, and the quirks the research states:

```
python3 -I tools/generate_dialog_gaps_vectors.py --research-dir ../../champions-re-wt/research-dialog-gaps \
    --tsv native/tests/dialog_gaps_vectors.tsv --contract native/tests/dialog_gaps_contract.json
```

| Research | Port | Where it runs |
|---|---|---|
| `Camera_setupDialogView` 0x203bb8 (2.2) | `setup_dialog_view`, `parse_dialog_offset`, `side_index` | message 0 (`Host::attach_dialog` sets `dialog::Host::view`) |
| `Camera_lookAt` 0x204250 and the run task's camera step (2.3, 2.4) | `look_at`, `run_task_camera` | `dialog_tick`: one `Camera_lookAt` per tick (47 to 182 calls each way); the far clip events; message 3 only on arrival; closing held while the shop is open |
| `Camera_apply` 0x2035c8 (2.5) | `camera_apply` | every swoop step; the pose is the `camera_pose` event and `dialog_camera_pose()`. A shake above 0 needs `dialog_camera_env.cosmetic_seed`, else the step is refused |
| The 24 open checks (3) | `site_outcome`, `game_is_frozen`, `recall_poll`, `camera_update_control`, `idle_voice`, `sites()` | `Host::dialog_gate(site, conditions)` and the session's `dialog_gate`; the front end takes `set_dialog_open` (start button, `menu_recall` and `menu_exitmission` entries) |
| `dialog_loadBox` (4) | `load_box`, `box_sprite_reads` | `DialogLevel`: no box word in `intro` or arena mode B unless an earlier level left one; a dialog that opens with word 0 is refused |
| The cursor glyph (5) | `init_camera_buttons`, `cursor_glyph` | `dialog_frame` draws byte 0x7e (layout byte 1, the executable's value) |
| `TestLocation` (6) | `factory_fallback`, `props_get_*`, `has_any_light` | the object factory makes no object for it: no talker, no error; `no_object_talker_records` counts records with a `dialogScript` that make none |

What the port decides from evidence and what it refuses:

- **Refused, research section 10.** A speaker with no `dialogOffset` (or fewer than
  four fields) leaves the camera's sideways offset to a stack word nobody
  initialised; the opening is refused (`open: Camera_setupDialogView reads an
  uninitialised stack slot...`), the menu mutex is given back, no value is made up.
  The disc has 4 such openings (2 talkers). A dialog in `intro` or arena mode B
  with box word 0 is refused rather than drawn (the original reads address 0); the
  disc never opens one. `Camera_apply` with a forward vector is not modelled (the
  dialog never passes one).
- **Inferred (I).** The camera's start is the message's carried position, yaw and
  pitch (the `camera_apply` that message 0 makes just before). The float at 0x44f130,
  read once by the run task, is taken as element 8 of the matrix the camera last
  built (the forward vector of `g_cameraMatrix`, which `camera_setView` writes, is
  not stated to be that matrix verbatim). The client has one camera, the follow
  camera, camera mode 1 in the site table.
- **Not covered by the research, still as before.** `g_uiFadeIn[0]` rises by 0.12 per
  displayed frame (`docs/mainloop.md` step 13) but the pattern of that constant is
  not given, so the host passes 1.0. The `NF:` text for a missing key that has
  double quotes is refused: which spelling it copies is not stated.
- **Gates with no client system.** The props, physics props, player marker, compass,
  remote busy icon, interact prompt, spectator prompt, net message list, HUD panel,
  spell selector, removed controller, card save screen, status effects, idle voice
  and the shake's frozen test have no counterpart this client draws or runs (the real-data scene draws no props, no compass).
  Their outcomes are available and tested through `dialog_gate`, not applied.
  `warpMenu`, `menu_recall`, `menu_exitmission` and the recall path are applied in
  the front end; `Player_run` / `Player_move` were already gated.

## Not modelled

- **The Godot camera** does not follow the conversation camera: the native pose
  (`dialog_camera()`, the `camera_pose` events) is published and counted, not applied
  to the Godot view.
- **`g_uiFadeIn[0]`** and **the quoted-key `NF:` text**: see above.
- **`Creature_interactPrompt`**: the `talkCone` test and the hostile-creature
  test were read, not executed. The use button does not apply them.
- Net games: only the offline machine runs (one local slot). The remote
  player's name is not drawn.
- `creatures_freezeAll` is handed on as an event; the session's own actors are
  not frozen beyond the player-input gate.

## Census

`champions-dialog-census DIR` (counts only) parses every `.dbin` of every
`.lmp` of every `.gob` with the native reader, then talks to every talker on a
fresh host for each, twice (first choice, last choice). On the owner's US
v2.00 folder, 2026-10-21:

| | Count |
|---|---|
| `.dbin` files / nodes / bytes | 52 / 300 / 328,800 |
| cutscene line lists / nodes | 18 / 37 |
| NPC conversation files / nodes / choices | **34 / 263 / 685** |
| choices per node (1, 2, 3, 4, 5, 8) | 96, 111, 1, 7, 16, 32 |
| NPC nodes with a condition / unreachable | 241 / 4 |
| choices with condition / action / ending / jumping / action and jump | 319 / 511 / 379 / 306 / 250 |
| objects with `dialogScript` (all with `showName`) | 28 |
| talkers (documented class) / interactables (documented class with `showName`) | 26 / 31 |
| text table entries | 5,498 |
| talks / opened / ended through message 3 / stalled | 52 / 42 / 42 / 0 |
| talks refused: an item condition / a quoted missing text key | 6 / 4 |

All parse counts equal research section 10.

Through the real-data play scene (`godot/tests/real_data_dialog.gd`, windowed,
111 levels): 24 talkers on 19 levels; the talk-to diagnostic opened 21 and
closed all 21 through message 3, 3 were refused as above; the use button at
the story start found a talker on 1 level and opened it, and nothing in its
box on 18; 0 stalls; the box and text were drawn on 821 frames; 39 voiced
lines were queued and the mouth started 32 times (28 with a lip-sync `.anm`).
The real-data sweep (60 ticks per level, 111 / 111) equals the tree without
the dialog in every count, with scripts on and off, apart from the new
`dialog.*` counts.
