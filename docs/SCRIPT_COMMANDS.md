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

### Callees research executed at `52984db`

Research `t3/research-script` `52984db` (`docs/script-effects.md`,
`tools/ref_script_effects.py`) executed the bodies some commands reach. Those
commands also run the binder, and the host answers the binder's call with
the executed body (`script_effects::Effects`). The composition census is
unchanged: the row keeps its `9649e6b` status.

| Command | Callee | Evidence |
|---|---|---|
| `setMonsterVal`, `selectSetMonsterVal` | field halves, or base slot 34 `from_int` at `+0x3c0` | executed |
| `setMonsterRange` | the same slot 34 when the range is above 0 | executed |
| `setCreatureParams`, `initScriptSetParams` | `Creature_scaleStats` | executed |
| `charmMonster`, `followPlayer` | `Creature_setCharm` with a null target at `+0x234` | executed |
| `hudResetFadeScreen`, `hudFadeScreen` | `hud_setFade`, `hud_stepFade` on the host's `g_hudFade` word | executed |
| `startMusic` | `music_playLevel`, then the host's `audio::Playback::music_play_level` | gate and calls executed; playback inferred (research-audio `6b08886`) |
| `endMusic` | `stream_queueStop` on stream 2, and `Playback::queue_stop(2)` | executed |
| `setCFlag`, `selectSetCFlag` (bit 0) | `creature_startBossMusic`: `Playback::play_boss_music` | name selector executed; stop and queue inferred, counted in `Counts::inferred` |

The host's `Playback` and `g_hudFade` are private to the host, like the
effect world. The Godot audio presenter is a separate object.

A routed command is refused before the binder runs, and stores nothing, when
for that state it would reach a callee research did not execute. Each such
refusal counts in `callee_refusals` and in the command's `refused`:

- `charmMonster`, `followPlayer`: a creature past `Creature_setCharm`'s gates
  (flag 0x20, not a player, charm timer not -1) whose target word at `+0x234`
  is not a node of the host's handle pool. Research `d4987fd` executed
  `handle_clear` of a live target (`docs/strings.md` 5): a target linked
  through the host's pool (`Host::link_target`) is cleared there, and only
  this creature's nodes and their partners go ([STRINGS.md](STRINGS.md)).
- `hudFadeScreen`: `hud_stepFade` adds to `g_hudFade`, which the game keeps
  between fades. The executable image starts the word at zero, so the first
  step can execute before `hudResetFadeScreen`. A replacement level-script
  host carries the known value within its `ChampionsSessionController`.
  A separate controller starts at zero. Bounded `ee_emu.py` probes in
  `t3/research-fade` 197f52a found no direct loader reset; indirect
  presentation owners remain outside that probe.

Since combat-ctors, `Playback` starts at the researched `save_init` music
volume 0.5 (`0x3f000000`), and the play session passes its current options
word to `Host::set_music_volume` before `preStart`. No options file uses the
same documented 0.5 boot default. `startMusic` and the boss-music bit of `setCFlag` use that
value through the existing gate. A placeholder object still refuses before
`setCFlag` reaches its binder.

The host executes `handle_clear` only for a live target in its handle pool.
Other unanswered callees still refuse; recorded audio and draw calls are not
claims of original-client presentation.

`setInfo`, `addHelpMessage`, `addHelpMessageF`, `showBust` and
`showCompass` run on research `d4987fd`'s string table, `wstr_format` and HUD
state ([STRINGS.md](STRINGS.md)) with the game's own `GTEXT.LMP` table, which
the caller registers (`Host::register_text`). They are callee refusals, with a
reason in `Counts::text_refusals`, without a table, without the help chime's
sound group (`set_help_chime`), without a camera mode (`hud().inputs`), for a
text over its buffer, a format that reads past its terminator or a missing
compass object.

Every command that is still partly documented or unknown is refused, as
below. That includes the bag walks, `generateMonster` (`world_createObject`
is not the combat spawn factory), `deleteMonster`, the slot-27 commands
(`rootMonster`, `killMonster`, `stopFollowingPath`), `followPath`
(`obj_runEveryFrame` was not entered), `sound`, `soundSequence`,
`hitPlayers`, `setWeaponType`, the medal boxes and `cutScene`.

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
| the constructor is SmallSpider 0x2ab450, Gnaw 0x26741c, Mosquito 0x297788, Scorpion 0x2a4d68, or Badger 0x244018 | built (`objects_built`), below |
| Skelight 0x2a8770, Zek 0x2c1f94, or an on-foot Horsemen 0x271c70 reaches `Holder.v1` | counted stub (`Holder.v1 table slot 1 is untraced`); no holder-dependent bytes or statistics are published |
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
and compare it case-sensitively; `Creature_setStats` draws from the object's
own generator and, with the selected level's `monsters.bal` and executable
level table, computes hit points, armor, damage, attack and experience.
It refuses this statistics path outside one player at difficulty 1. The
factory's Skelight level argument is 1 at call `0x15b608` (delay slot
`0x15b60c`; executed research `7a66b58`). The on-foot Horsemen rows `0x532dc8..0x532df8` call `0x271c70` at `0x15b900`
with variant 0..3 and mounted 0 (executed, research `dfcc8d3`); the four rider
rows `0x532cc8..0x532cf8` call it at `0x15b3d4` with mounted 1, and stay
unbuilt because the mount lifecycle is not ported. The on-foot path also
stays unbuilt: research-creature-stats `3469927`
`docs/creature-stats-port-check.md` pins the two Holder slot-1 bodies but not
which table slot points to which body. No class-name mapping substitutes for
that missing read. Only the base name of a
request (before `_`) is stored in the level-name global `0x44fe18` and used for
the `stomedal`/`innpit` override and Skelight's `blomedal` test (executed,
research `a684d7d`). A creature's hit points are readable only when the item
gate was observed (known world and HUD residency, or `intro`), the gate was
not refused, and no earlier record was left unbuilt: otherwise the shared seed
is not known to be exact at that point, so `getHitPoints` is refused and counted. `Creature_init` (research
`tools/ref_creature_init.py`, executed) stores the change mask; `handle_clear`
finds the empty target list `Creature_ctor` left; the animation, run-list
and grid calls are made on the host's world or have no effect on what is
kept. The constructor runs on the executable's image, so it reads the
original's constants. The object's bytes are then copied into the binder,
except fields no ported code computes: the height and draw byte
(`Creature_init` from the model's bounds), the hide mask,
the creature serial (an unbuilt creature before it would have taken one), and
the pointers into the record's strings. Statistics are copied only when the
selected balance and level data are present; otherwise hit-point reads still
refuse. A base name not present in the executable's level table, including a
short arbitrary name, is refused before it can be written into the emulated
level-name global. The allocation capacity at `0x44fe18` is unpinned;
the accepted domain is limited to names the original executable lists.
A record whose `Creature_init` would
call the class or run a script (a `dead` or `initScript` property, the
MiniBoss bit, a weapon element on a weapon) stays a stub and is counted in
`build_refusals`.

`tests/level_objects_tests.cpp` adopts 336 authored records and compares
each built object's bytes and the shared seed with what the pinned research
constructors produce for them (`tools/generate_level_objects_vectors.py`).

A documented command whose binder path reads, on an object its name finds,
the creature flag 0x20, hit points at +0x228 or the type name is refused when
any object it finds is a stub; `getHitPoints` also refuses a built creature
whose statistics were not supplied: `hideMonster`, `showMonster`,
`increaseEnemyRank`, `unRootMonster`, `stopFollowingPlayer`, `friendly`,
`unFriendly`, `getHitPoints`, `setHitPoints`, `countLiveObjects`,
`showOnMap`, and the `52984db` creature routes `setMonsterVal`,
`setMonsterRange`, `setCreatureParams`, `charmMonster` and `followPlayer`.
`selectSetMonsterVal`, `initScriptSetParams` and `selectSetCFlag` act on
`g_scriptSelectedObj` (`0x4039f8`) and are refused when that is a stub. Each
such refusal also counts in `stub_refusals`. A name argument
whose address the machine refuses would make the binder reuse a remembered
string slot; while stubs or built objects exist that is refused too. Commands
that read only a stub's position or angle (`moveMonster`,
`addParticleOnObject`, `findDistance`) still run.

Every partly documented or unknown command increments that command's refusal
count, and the binder does not run, except the `52984db` callee routes above,
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

The disc measurement then keeps resuming while the waiting state is new, and
stops when it repeats or when the resume count passes the script's public
count plus two. The state is the deferred name table and its counters, the
script's global variables, the quest variables and the post-action name
(`docs/script.md` 2.4: a waiting entry counts in a global variable and
defers itself again). The cap is a measurement bound, not the original's
behaviour: the original keeps ticking. A repeated state means nothing a later
tick reads has changed; it does not prove the wait never ends.

## Vectors

`tools/generate_script_commands_vectors.py` reads
`tools/ref_script_compose.py`, the modules it imports and `docs/script.md`
with `git show 9649e6b:` and refuses to run unless each hashes to its pin.
It writes `native/tests/vectors/script_commands.json` (the pins) and `native/tests/vectors/script_commands.tsv` and
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

`tools/generate_script_effects_vectors.py` reads `tools/ref_script_effects.py`,
the modules it imports and `docs/script-effects.md` with `git show 52984db:`,
refuses to run unless each hashes to its pin, runs the bodies on synthetic
fixtures (no executable) and writes `native/tests/vectors/script_effects.tsv`
(36 cases) and `script_effects.json` (the pins). The C++ test replays those
stores, returns and calls; an unstubbed call answers 0, the default of
`ref_script_effects.Trace.call`. The host tests check the routes and every
callee refusal against values from `docs/script-effects.md` or the pinned
reference.

## Play session

The session controller grows `enable_level_script`, `disable_level_script` and
`script_command_counts` (which also reports `callee_refusals` and the
`inferred` executions). Counts are not observation fields. An empty image is
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
The executed item-resource gate compares the world against `intro` and then
queries whether `hud.lmp` is resident (research `7a66b58`; both residency
outcomes at `938ccb8`). The 223 draws the loader makes when `hud.lmp` is
absent are documented in research-items `docs/startup.md:341` and
`docs/issues/sync-breakers.md:383`. Godot's level
script attach does not expose that residency, so it refuses the 223 seed
draws and reports `item_load_gate_refusals` and
`item_load_draws_inferred`. The latter is a conditional potential draw count,
not an observed seed advance. An omitted level name also counts a refusal.
A caller with a known level name and an observed residency result can execute
the exact gate through `advance_first_level_items`.

`Player1_ctor`'s placement (ctor 0x158928) is built only when the gate was
observed, no draw was omitted, a session seed and the level's WORLD are present,
and the caller has called `set_player1_entry` with the ranking's inputs: the
previous level name, the entrance name, gate travel and the load-from-save
state (research `tools/ref_objects.py` `player1_marker`; start rank 3 after a
save load, `docs/issues/arena-entry.md:190`). This host derives none of them
(`level().prev`, recall and save-load are not set by any attach path), so
without the setter the record is a counted refusal ("Player1 needs
caller-supplied level entry"). A refused placement restores the spawn slots and
start rank. A built creature's collision radius (+0x1c, a constructor
immediate) is copied into the host object, because the placement's box query
tests every earlier object's radius. The height extent (+0x20, from the model
bounds in `Creature_init`) is not modelled; the query only compares it against
a +/-100000 vertical window.

The counts-only disc tool is `champions-script-commands-measure --game rta DIR`.
It reads GOB files that have a sibling TEX, then `script.scr` and `objects.ob`
inside the matching lump. It prints counts and the 191 command names. Each
level runs as the play session runs it: one player, difficulty 1, a session
seed fixture of 17 (the control session's default) with
`SeedPolicy::OfflineSession`, and the directory's `SLUS_209.73` for the object
factory when there is one. `--bare` runs the host as wave 17 left it (no
executable, seed or player count); `--no-seed`, `--no-players` and
`--no-objects` leave one of the three out, to attribute a count change.
