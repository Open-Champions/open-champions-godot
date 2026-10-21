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
no host-private fallback generator.

What the seed is (research `t3/research-script-session` 6841b8a,
`docs/script-session.md` section 1): `getRand` draws `g_randSeed` 0x404464,
the one shared generator of the simulation. Nothing seeds it at a level start.
An offline session loads it once from the CPU cycle counter (0x322a04); a net
launch stores 0 (`net_startSync`, 0x13dd0c). A level's draws then follow in
file order: one in every `GameObject_ctor`, whatever the derived constructors
draw, `Player1_ctor`'s searches. `bind_seed` takes a policy:

- `SeedPolicy::Exact` (the default): `getRand` is also refused once an
  `objects.ob` record was adopted that the host did not construct, because
  the original draws while it constructs it.
- `SeedPolicy::OfflineSession`: the session's generator stands in for the
  offline clock seed, so no value is "the original's" and `getRand` is
  answered. Records the original constructs and the host does not are counted
  in `load_draws_unmodelled`. The play session uses this policy.

`start_single_player` stores the single-player game's `g_numPlayers` 0x4d8ae0
= 1 and `g_difficultyIdx` 0x4f719c = 1 (the executable image values and the
offline setup's stores, `docs/script-session.md` section 2; research-items
`docs/script-items.md` section 3 for the difficulty). The binder's memory
starts at zero, so without it every command that walks the players does
nothing and the mission bytes are read from difficulty row 0.

A particle whose definition is not in the caller's JSON increments
`missing_definitions` and spawns nothing else. The headless effect world does
not draw. The Godot effect presenter is unchanged.

## Level objects

Without an executable (`set_executable`), `adopt_objects` registers each
`objects.ob` record as an unconstructed stub: object flags 0, the record's
position, angle and properties (a `name=` property is the lookup name), and
the record's factory class string as the stub's type string
(`objects_unclassified`). A record without a class string is refused
(`objects_refused`), not given a default type.

With the owner's executable, each record goes through the object factory
first, in file order (`champions-level-factory`, research
`tools/ref_level_factory.py`: `world_objectFactory` 0x159c94's 206 name tests
in the original's order, the property fall-backs and the rows that build no
object; executed against every record of the disc's level files with 0
mismatches). The test strings and property keys are read from the executable
at their addresses; nothing read is kept. Then:

| Outcome | Host |
|---|---|
| the factory builds no object (`clearColor`, `customVars`, the arena start points, `Line`, no test and no fall-back) | nothing is registered (`objects_not_created`); a script finds nothing by that name |
| the pool has no block of the size class | nothing is registered (`objects_pool_refused`); the original calls no constructor |
| the constructor is one of SmallSpider 0x2ab450, Gnaw 0x26741c, Mosquito 0x297788, Scorpion 0x2a4d68, Badger 0x244018 | built (`objects_built`), below |
| any other constructor | a stub at its pool address (`objects_unbuilt`, by constructor in `unbuilt_constructors`) |

Every record the factory constructs takes its block from the host world's
pool (`objects::Pool`, the original's size classes), built or not, so the
pool runs out where the original's would. A `name=` value in double quotes
is unquoted, as `GameObject_ctor` does (step 7).

A built creature runs `GameObject_ctor` (research
`docs/spec/object-system.md` 2.2: the base fields, one draw from the bound
session seed into its own generator +0x44, the name), then the ported
`monsters_1` constructor, which runs `Creature_ctor`, the class body and its
set-state 0. Their call-outs are answered as research
`docs/script-session.md` section 5 says: the property getters lower the key
and compare it case-sensitively; `Creature_setStats` makes its one draw of
the object's own generator; `Creature_init` (research
`tools/ref_creature_init.py`, executed) stores the change mask; `handle_clear`
finds the empty target list `Creature_ctor` left; the animation, run-list
and grid calls are made on the host's world or have no effect on what is
kept. The constructor runs on the executable's image, so it reads the
original's constants. The object's bytes are then copied into the binder,
except the fields no ported code computes: hit points, maximum hit points
and the four statistics and experience value (`Creature_setStats`), the
height and draw byte (`Creature_init` from the model's bounds), the hide mask,
the creature serial (an unbuilt creature before it would have taken one), and
the pointers into the record's strings. A record whose `Creature_init` would
call the class or run a script (a `dead` or `initScript` property, the
MiniBoss bit, a weapon element on a weapon) stays a stub and is counted in
`build_refusals`.

`tests/level_objects_tests.cpp` adopts 336 authored records and compares
each built object's bytes and the shared seed with what the pinned research
constructors produce for them (`tools/generate_level_objects_vectors.py`).

A documented command whose binder path reads, on an object its name finds,
the creature flag 0x20, hit points at +0x228 or the type name is refused when
any object it finds is a stub, and `getHitPoints` also when it finds a built
creature (its hit points are not known): `hideMonster`, `showMonster`,
`increaseEnemyRank`, `unRootMonster`, `stopFollowingPlayer`, `friendly`,
`unFriendly`, `getHitPoints`, `setHitPoints`, `countLiveObjects` and
`showOnMap`. Each such refusal also counts in `stub_refusals`. A name argument
whose address the machine refuses would make the binder reuse a remembered
string slot; while stubs or built objects exist that is refused too. Commands
that read only a stub's position or angle (`moveMonster`,
`addParticleOnObject`, `findDistance`) still run.

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
with `Stop::Refused`, the host counts it in `refusal_stops` (and per command
in `stopped`), and nothing else of that script runs: the rest of the level's script would depend on an
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

`enable_level_script` starts a single-player game (`start_single_player`),
gives the host the executable `load_arena_tables` read (the real-data scene
loads the game folder's `SLUS_209.73` there), and binds one stream with
`SeedPolicy::OfflineSession`. The stream starts at the play session's
configured shared seed (`session_control::Config::shared_seed`, the port's
stand-in for the offline session's clock seed). The load's draws (the built
objects' `GameObject_ctor`, `getRand` in `preStart` and `start`) come first.
`start_play_session` then starts the control session's World from where they
left it, and the World creates the player after them, as the original builds
the players after the level's objects and `start`
(`docs/issues/arena-entry.md` 1.4). From then on the host draws the stream
the World hands over. The control session's World makes no shared draw in
its ticks. If it ever did, or a fixture session or a replaced control session
takes over, the host's seed is unbound (`seed_unpaired`), and `getRand` is
refused from then on. A script attached while a combat or level fixture is
active binds no seed, because the fixture's seed belongs to its journaled scene.
`script_command_counts` reports the object outcomes, `load_draws_unmodelled`,
`build_refusals` and `seed_bound` / `seed_paired` / `seed_unpaired`.

The counts-only disc tool is `champions-script-commands-measure --game rta DIR`.
It reads GOB files that have a sibling TEX, then `script.scr` and `objects.ob`
inside the matching lump. It prints counts and the 191 command names. Each
level runs as the play session runs it: one player, difficulty 1, a session
seed fixture of 17 (the control session's default) with
`SeedPolicy::OfflineSession`, and the directory's `SLUS_209.73` for the object
factory when there is one. `--bare` runs the host as wave 17 left it (no
executable, seed or player count); `--no-seed`, `--no-players` and
`--no-objects` leave one of the three out, to attribute a count change.
