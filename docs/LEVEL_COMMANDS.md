# Level commands in the story-route levels

Contract note for `native/include/champions/level_commands.hpp`,
`native/src/level_commands.cpp` and their wiring in `script_commands.cpp`.

Research: `t3/research-level-commands` `8efa13d`, `docs/level-commands.md`
(cited as `:line`), reference model `tools/ref_level_commands.py` (cited as
`ref:line`). Audit `research-level-commands.md`: PASS, LC-1 to LC-11, all LOW.

## Scope

The four built-ins below are ported only in a story-route level, that is, a
host with an entry gate (`story_route::gate_for`). Everywhere else they stay
refused as before. `activateStore` keeps the dialog route it already had.

Every target is checked before the binder runs. If any target reaches a case
the research did not run, the command is refused, stores nothing, and the
entry stops. The reason is counted in `Counts::level_command_refusals`. A
target that is an unconstructed stub is refused through the existing stub
check (`stub_refusals`), because a stub's flags are not the constructed
object's.

## autoPropSetState (name, state)

Slot 18 of AutoProp and AutoPropLP, `AutoProp_setState` `0x23d680`
(`:110-170`, `ref:189-250`). Objects without the AutoProp bit are skipped
(`:121-124`). For n below States (`+0x120`, signed), in this order:

- the state `+0x178` = n;
- frames in state `+0x17c` = 0;
- states reached `+0x180` = bits 0..n (none for state 0);
- the mesh mask `+0xc8` = `model_buildHideMask`;
- the track count `+0x9c` = 0;
- if the state has a clip, it is pushed (mode 3 when looping, else 6) and
  jumped to its end, and frames = clip length + 1;
- `setPassable`: object flag `0x1` set or cleared from the state's Passable
  flag.

A state at or past States stores nothing (`:135-136`). No action script runs,
the done bit is left alone, and nothing draws from a generator (`:153-155`).

Refused (not run):

| Case | Basis |
|---|---|
| A class other than AutoProp / AutoPropLP | its slot 18 was a stub (`:169-170`) |
| A negative state, or a state of 10 or more | below the clip table / the `0x170c0c` hazard (`:157-162`) |
| An AutoDoor | `world_movePiece` was a stub (`:147`, `:167-168`) |
| No model block, or an unaligned clip | the mask reads the model (`:143`); the length load traps (`ref:168-172`) |
| ActionAtEnd with the done bit clear | the next AutoProp run `0x23cb54` would run the state's actions at the animation's end (`:209-210`). Inferred, not run, and that run is not ported (audit LC-6b). |

The reader list for `+0x178`/`+0x17c` (audit LC-6c) is `:209-216`, plus
`0x23cc84` in the run and `0x24230c`, `0x242330` in AutoPropLP. None of those
readers is ported. The port does not claim that `start` resets a saved state
on a revisit (`:224-226`, audit LC-6a). The client restores no AutoProp state.

The AutoProp fields come from a constructor the research did not run (the
fields were set by hand, `:57-63`, audit LC-5), and the client has no ported
AutoProp constructor. So in play the `tormenta` records are stubs and the call
refuses. Only objects whose fields were written by something else, such as
synthetic tests, run through the port.

## killMonster (name)

The binder already stores flags `|= 0x2009` (dead, killed, not solid) and hit
points 0 on each living creature, and then calls slot 27 with `0x14`
(`:232-240`). The port answers slot 27 for Gnome only, `Gnome_setState`
`0x269bb4` (`:242-276`, `ref:380-462`):

1. The state gate `0x10a9c0`. With the arena flag set, `0x4f711c` 0,
   `0x503890` set and `0x503894` 0, it refuses, and only the handler's stores
   remain. This is the arena-only refusal, counted in `arena_gate_refusals`.
   Otherwise it zeroes the four timers.
2. One draw of the creature's own generator at `+0x44`, which is not the
   shared seed.
3. `anim_start` of death animation (draw & 3) from the class definition, with
   a 0.1 s blend.
4. `Creature_onDeath(0)`: creature flags `&= ~0x8c00000`; the record's
   `deathScript` runs with `g_scriptSelectedObj` = the creature, and then the
   selected object is set to 0, not back to its old value. That is why a later
   `newWorld` in the same entry uses orientation `0xff` (`:302-306`).
5. AI state `+0x232` = `0x14`.

Refused (not run): any class other than Gnome (`:321-322`). Past the gate, it
also refuses when there is no class-definition block, a `dropitem`, object
flag `0x2` (`obj_delete`), the arena flag (`arena_addKill`), or a boss-music
name. Each of those calls was a stub in the research (`:54`, `:269-275`). A
deathscript entry the story-route gate does not admit is refused before any
kill is stored.

The deathscript runs as the level's own code. The port makes no claim about
which counter it changes (audit LC-2).

The client has no ported Gnome constructor, so in play the `islgnome` records
are stubs and the call refuses.

## soundSequence and preloadResource: recorded no-ops

- `soundSequence`: `res_find` was a stub in the research (`:331`, `:354-357`).
  The host answers no resource. Then `sound_playOnObject` returns -1 at once
  and nothing is stored (`:331-332`). The original takes a sound player and
  advances the handle serial when `res_find` finds a sequence. So after a
  recorded sequence, later sound handles and free-player availability may
  differ from the original's (audit LC-3). `Host::sound_players_diverged()`
  reports it. No direct gameplay reader of those words was found
  (`:350-357`).
- `preloadResource`: `lump_load`'s result is ignored (`:371`). A lump that the
  owner's game folder has (`Host::set_lump_exists`) is a recorded no-op
  (`:392-396`). A missing lump, or a host with no folder listing, is refused:
  the original stops the emulated run there, and on hardware the outcome is
  Unknown (`:384-386`, audit LC-4). The `lump_findLoaded` / `mem_allocTagged`
  bodies were stubs, and the blocking read's stall is Inferred.

Both are counted in `Counts::recorded_noops`.

## activateStore

Unchanged behaviour on the dialog route (`docs/DIALOG.md`), now labelled
(`:398-453`):

- The index `0x4f7150` is written when the argument is below `0x15`
  (unsigned). It is 0 at the 42 shipped hub/hub2 sites in top-level runs
  (`:412-417`, audit LC-9a).
- With a dialog open, it sets only the pending flag `0x4fdd30`. This was
  executed (`:422`).
- The deferred open at the dialog's end is Observed static, not run
  (`:426-432`). Offline it goes to the active player. On the net it goes to the
  first local slot under `g_numPlayers` (`:429`, audit LC-9b). The flag is
  cleared only where a dialog opens.
- With the dialog closed, the vendor-stock shop opens in mode 0, which keeps
  the stock (`:437-446`). Nothing here regenerates stock.

Without an attached dialog, `activateStore` stays refused, as before.

## Tests and acceptance

- Synthetic tests: `story-route` (`test_autoprop_set_state`,
  `test_kill_monster`, `test_recorded_no_ops`, `test_activate_store_binder`).
- Private probe (counts only): `m3-gnome-kill` and `m5-start` check the
  research outcomes. `m3-gnome-kill-stub-refusal` and `m5-start-stub-refusal`
  check the play gap that remains with the level's records adopted as stubs.
  The `islgnome` entrances were executed for three of the four
  (started, finished) pairs. The fourth, (0, 1), is Observed static only, the
  branch to `0x1ce4`, and is not a probe case (audit LC-8).
