# Level-script commands

`champions-script-commands` is the opt-in host for a level's script. It does
not replace the script machine or the binder. A documented command runs the
existing binder. Where that command's whole effect is a call the binder
already makes, the host answers the call with a subsystem that already exists.

Research `t3/research-script` `52984db` executed a second set of callees. Those
commands also run the binder. The composition census is unchanged: a row stays
partly documented when its callee is what this host now runs.

| Command | Route |
|---|---|
| `hideMonster` | `objects::World::move_off_grid` |
| `showMonster`, `moveMonster` | `objects::World::update_grid_cell` |
| `getRand` | the shared generator at memory word `0x404464` |
| `setEntrance` | `level_flow::State::entrance` |
| `addParticleOnObject` | `fx::sim::EffectWorld::particle_spawn` |
| `setMonsterVal`, `selectSetMonsterVal` | field halves, or base slot 34 `from_int` at `+0x3c0` |
| `setMonsterRange` | the same slot 34 when the range is above 0 |
| `setCreatureParams`, `initScriptSetParams` | `Creature_scaleStats` |
| `charmMonster`, `followPlayer` | `Creature_setCharm` |
| `setInfo` | `hud_showMessage` |
| `addHelpMessage`, `addHelpMessageF` | `hud_showHelpMessage` |
| `showBust` | `hud_setBust` |
| `showCompass` | `hud_setCompassTarget` |
| `hudResetFadeScreen`, `hudFadeScreen` | `hud_setFade`, `hud_stepFade` |
| `startMusic` | `music_playLevel` |
| `endMusic` | `stream_queueStop` on stream 2 |
| `setCFlag`, `selectSetCFlag` | `audio::Playback::play_boss_music` |

`getRand` copies the shared seed into the binder's word, lets the binder draw
once, and copies the word back. It does not draw a second time. The combat
fixture keeps its own seed word; this host does not rewrite it. With no bound
seed, the draw uses the binder world's shared generator. None of the commands
added above draw that seed.

A particle whose definition is not in the caller's JSON increments
`missing_definitions` and spawns nothing else. The headless effect world does
not draw. The Godot effect presenter is unchanged. The host's `Playback` is
the same: `startMusic`, `endMusic` and `setCFlag` update it, and the Godot
audio presenter is a separate object.

Creature commands act only when object flag `0x20` is already set. An adopted
object file has no flag word. The constructors that set the bit were not
entered, so adoption leaves the bit clear and stores the file's class string
at `+0x34` for the boss-name lookup. A synthetic length prefix is removed only
when its integer equals the remainder length, so `10GameObject` is looked up
as `GameObject`. `text_lookup`'s string table was not entered: the host widens
the key to UTF-16 and the setter copies that. `wstr_format` was not entered:
`addHelpMessageF` replaces the first `%d`, `%i` or `%u` with the first float
truncated toward zero, and leaves every other percent sequence as text.
`sound_playOnObject` was not entered, so the help chime handle stays -1 and
the frozen-flag body stores nothing. The help box still opens.

A follow camera with mode 1 is installed at `0x4f8450`. A null camera faults
`showBust` and `showCompass`. The other HUD gates are the words research
measured.

Every command that is still partial or unknown returns 0 and increments that
command's refusal. The binder does not run. That includes the bag walks, 
`generateMonster` (`world_createObject` is not the combat spawn factory),
`deleteMonster`, the slot-27 commands (`rootMonster`, `killMonster`,
`stopFollowingPath`), `followPath` (`obj_runEveryFrame` was not entered),
`sound`, `soundSequence`, `hitPlayers`, `setWeaponType`, the medal boxes and
`cutScene`.

## Level entry

`preStart` and `start` are the loader's entry names. `begin_level` runs
`preStart` when the script defines it, then `start` when the script defines
it, then one post action. A name queued by `addDeferedCall` is a wait. The
binder runs that name on the next ready tick, after one tick that only
promotes the queue. A script that returns with nothing queued has completed.
A script can be counted as both once a later tick empties the queue.

Each entry starts with the play session's existing instruction budget
(`session::SCRIPT_BUDGET`, 100000). One ready tick shares that budget across
the deferred names it runs, because the binder's `run_deferred` does not reset
it. Budget exhaustion, a host fault, or a division by zero is a fault. The
play session keeps running.

The disc measurement then keeps resuming while the deferred name table is a
new state, and stops when that state repeats or the resume count passes the
script's public count plus two. A repeated state is a polling loop.

## Vectors

`tools/generate_script_commands_vectors.py` reads
`tools/ref_script_compose.py` from research `t3/research-script` at `9649e6b`
and writes `native/tests/vectors/script_commands.tsv` and
`native/src/script_commands_table.inc`. The census is 191 commands, 81
documented, 70 partly documented, 40 unknown, 5660 published calls, 122 used
and 69 never called. The C++ test compares the compiled table to the TSV and
runs synthetic scripts for the routes, the refusals, and the deferred wait.

`tools/generate_script_effects_vectors.py` reads `tools/ref_script_effects.py`
from the same research line at `52984db`, without the executable, and writes
`native/tests/vectors/script_effects.tsv`. The C++ test replays those stores,
returns and calls.

## Play session

The session controller grows `enable_level_script`, `disable_level_script` and
`script_command_counts`. Counts are not observation fields. An empty image is
refused and leaves the observation as it was. Reset and stop drop the host.
The real-data scene has a "Level script (opt in)" checkbox and `--level-script`.
`godot/tests/real_data_scene.gd --scripts` attaches after a level loads and
before play.

The counts-only disc tool is `champions-script-commands-measure --game rta DIR`.
It reads GOB files that have a sibling TEX, then `script.scr` and `objects.ob`
inside the matching lump. It prints counts and the 191 command names.
