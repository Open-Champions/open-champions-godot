# Level-script commands

`champions-script-commands` is the opt-in host for a level's script. It does
not replace the script machine or the binder. A documented command runs the
existing binder. Where that command's whole effect is a call the binder
already makes, the host answers the call with an instance of a subsystem that
already exists. These instances are private to the host. Nothing in the play
session reads them yet:

| Command | Route |
|---|---|
| `hideMonster` | the host's own `objects::World::move_off_grid` |
| `showMonster`, `moveMonster` | the host's own `objects::World::update_grid_cell` |
| `getRand` | a bound session `SharedRandom`, through memory word `0x404464`; refused without one |
| `setEntrance` | the host's own `level_flow::State::entrance` |
| `addParticleOnObject` | the host's own `fx::sim::EffectWorld::particle_spawn` |

`getRand` must advance the session seed (research `9649e6b`,
`docs/script-composition.md`, "The seam"). With a seed bound by `bind_seed`,
it copies that seed into the binder's word, lets the binder draw once, and
copies the word back. Without a bound seed it is a counted refusal; there is
no host-private fallback generator. It is also refused once an `objects.ob`
record has been adopted: the original constructs each record and every
constructed object draws once from the shared seed during the load
(`docs/issues/arena-entry.md` 1.4), and the host's records are not
constructed. The play session binds no seed: its combat seed belongs to the
journaled combat scene. So in play `getRand` is always refused.

A particle whose definition is not in the caller's JSON increments
`missing_definitions` and spawns nothing else. The headless effect world does
not draw. The Godot effect presenter is unchanged.

## Level objects

`adopt_objects` registers each `objects.ob` record as an unconstructed stub:
object flags 0, the record's position, angle and properties (a `name=`
property is the lookup name), and the record's factory class string as the
stub's type string. The original builds each record with its factory
constructor (research `docs/objects.md`), which sets the flags, the creature
fields and the C++ type-info name, a different, length-prefixed string
(`docs/spec/object-system.md` 4.6). None of that runs here. A record without a
class string is refused (`objects_refused`), not given a default type.

A documented command whose binder path reads, on an object its name finds,
the creature flag 0x20, hit points at +0x228 or the type name is refused when
any object it finds is a stub: `hideMonster`, `showMonster`,
`increaseEnemyRank`, `unRootMonster`, `stopFollowingPlayer`, `friendly`,
`unFriendly`, `getHitPoints`, `setHitPoints`, `countLiveObjects` and
`showOnMap`. Each such refusal also counts in `stub_refusals`. A name argument
whose address the machine refuses would make the binder reuse a remembered
string slot; while stubs exist that is refused too. Commands that read only a
stub's position or angle (`moveMonster`, `addParticleOnObject`,
`findDistance`) still run.

Combat runtime, creature AI, audio and HUD have no command whose whole effect
is documented. Those commands, and every partly documented or unknown command,
increment that command's refusal count. The binder does not run, except
`setCFlag`, `activateMissionMenu`, and `givePlayerExp` once an experience
table is set. Those three, and the commands that stay refused, are specified
in [QUESTS.md](QUESTS.md). `setCFlag` tests the creature flag 0x20 on each
object its name finds, so it is also refused when any of them is a stub.

A refusal has no answer. The host looks at the script code after the call,
using the opcode table of research `docs/script.md` section 1. When
straight-line code sets PRI without reading it (for example the next
`SYSREQ.C`, or `ZERO.pri` before `RETN`), the answer is discarded: the
machine gets 0 and the entry goes on. Anything else (a store, a compare, a
jump, a `RETN` that returns it) uses the answer. Then the entry stops there
with `Stop::Refused`, the host counts it in `refusal_stops`, and nothing
else of that script runs: the rest of the level's script would depend on an
answer nobody knows. A refusal stop is not counted as a fault or as a
completion.

## Level entry

`preStart` and `start` are the loader's entry names. The host follows the
loader's order (research `9649e6b`, `docs/script.md` 2.1 and
`docs/issues/arena-entry.md` 1.4): `run_prestart` runs `preStart` when the
script defines it, then the caller adopts the level's objects, then
`run_start` runs `start` when the script defines it, then one post action
(`game_loadLevel` 0x1e60b8), then the deferred queue once, outside any tick
(`script_runDeferred` from `mainloop_runSession` 0x322e6c, `docs/mainloop.md`
step 7). So `preStart` finds no level object by name, and a name `start`
queued with `addDeferedCall` runs on the first ready tick. A queued name is
a wait. Every ready tick then runs the Script task once, also with an empty
queue. A script that returns with nothing queued has completed. A script can
be counted as both once a later tick empties the queue.

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
and 69 never called. The C++ test compares the compiled table to the TSV.

`tools/generate_script_host_vectors.py` reads `tools/ref_natives.py`,
`tools/ref_amx.py`, `tools/ref_objects.py` and `tools/ref_core.py` with
`git show 9649e6b:` and refuses to run unless each, and the four research
documents it follows, hashes to its pin. It builds eight synthetic script
images with `ref_amx.build_image`, runs them on `ref_natives.Game` in the
documented level-entry order and writes `native/tests/vectors/script_host.txt`:
the image bytes, the objects, and what the reference produced (entry results,
the deferred queue words and quest variables after the load and each tick,
the shared seed and draw count, object flags, hidden byte and grid list, the
entrance, and every `particle_spawn` word). The cases are a `getRand` draw,
hide and show on a creature and a non-creature, three particle spawns, an
entrance, a three-step deferred chain, `preStart` before the objects, a post
action, and a tick counter. The C++ test loads each image into the host and
compares every value. The hand-written checks there cover only the refusals,
the counts and the stubs; none compares against a value computed in C++.

## Play session

The session controller grows `enable_level_script`, `disable_level_script` and
`script_command_counts`. Counts are not observation fields. An empty image is
refused and leaves the observation as it was. Reset and stop drop the host.
`advance` and `advance_time` run the host's Script task once for each ready
tick the submission simulated (0 or 1), not once per call, and also while
play is paused, as the original's task list does (research `docs/script.md`
2.4 and 2.6). `godot/tests/script_commands_smoke.gd` plays the generated
`tick_counter` image and checks one `addv` per simulated ready tick,
including a fresh session whose first submission simulates none.
The real-data scene has a "Level script (opt in)" checkbox and `--level-script`.
`godot/tests/real_data_scene.gd --scripts` attaches after a level loads and
before play.

The counts-only disc tool is `champions-script-commands-measure --game rta DIR`.
It reads GOB files that have a sibling TEX, then `script.scr` and `objects.ob`
inside the matching lump. It prints counts and the 191 command names.
