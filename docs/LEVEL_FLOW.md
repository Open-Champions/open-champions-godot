# RTA level flow

`champions-level-flow` is portable C++17 for the documented Return to Arms
SLUS-20973 v2.00 level contract. `champions-session-level-flow` is an opt-in
composition adapter. Neither depends on Godot or changes Session, Controller,
save glue, `objects_lifetime.cpp`, scene libraries, or another job's files.

Evidence is pinned by full commit and SHA256 in
[`native/tests/level_flow_contract.json`](../native/tests/level_flow_contract.json):
research commit `13fafa465d5dad7acaeb1ee8becd30c4c8e4788b` (`t3/research-levels`,
round-4 arena wave start in `docs/level-flow.md` section 18), `docs/level-flow.md`,
`ref_levels.py`, `test_ref_levels.py`, `difftest_levels.py`,
`difftest_levels_r3.py`, `difftest_levels_r4.py`, and arithmetic dependencies
`ref_world.py`, `ref_core.py`, `world_info.py` and `ref_objects.py`. Round 3 used `3ef714a`.
Round 2 used `36036dab`. Round 1 used `18e1a34`.
Water is an adapter to the separately pinned `codex/creature-water-2` Service.
The research is static evidence, not a measured console run. This does not enable
Norrath or claim a playable Godot scene.

## Native API

Include `champions/level_flow.hpp`, `champions/level_flow_loader.hpp`, and
`champions/session_level_flow.hpp`; namespace
`champions::simulation::rta::level_flow`. All authoritative coordinates are raw32
patterns. Use the existing core arithmetic and shared RNG; never use Godot random,
render-frame elapsed time, or Godot physics as substitutes.

- `find_level_row`: exact qualified pass, first table-order prefix, substring,
  default. Supply the original table as data. Subnames affect state/table/gates,
  never resource filenames. Monster level preserves the difficulty/PvP overrides.
- `level_manifest`: inspect ordered required/optional resources against supplied
  inventory. A missing required resource throws `MissingResource` at its first
  required step. Player model tables are supplied by the owner, not invented.
- `Loader::load`: reads owned bytes via the selected disc provider, parses
  sequential GOB payloads from offset 0x800 ignoring directory offsets, fixes
  lump names and dialog entries, drops `pal_*`, reads the first 0x5000 TEX bytes,
  and loads dependencies in order. Lump entries remain case-sensitive; disc
  requests are uppercase with `;1`. The picture's redundant retry is preserved.
  `g_world` is the first `.world` entry in the level lump's stored directory
  order (`first_entry_with_extension`, 0x171fd8): text after the last `.`,
  exact case-sensitive comparison, no sorting. A lump without one throws
  `MissingResource` (the original faults). WORLD material/cell/texture decoding
  remains with level-textures.
- `base_light_level(row.flags)`: g_worldLightLevel 0x5d347c is 0x80, 0x20 with
  0x20000000, 0x40 with 0x04000000; with both set 0x40 (tested second). It is a
  base vertex light level, not an alpha. `LoadedLevel::base_light_level` carries
  it. `chunk_light(flags)`: 0x100 gives -230 (and is cleared), else 0x2000 gives
  +230, else 0x200 gives 0, else dynamic; 0x2200 gives +230. The renderer owner
  applies both values.
- `run_markers`, `place_players`: rank rules, all four marker draws/searches,
  gate arrival override, saved rank 3, recall x/y, highest-floor radius 12,
  persisted slot 1–3 scatter/facing, medal/alive/active/PvP gates. `Draw` returns
  the existing shared generator's upper 16 bits. `Height` must call
  `world_queries::terrain_height`, not `terrain_height_below`.
- Round 3 start (`PLACEMENT_RADIUS` 0x41400000). `player_ctor_z` repeats that
  radius-12 `terrain_height` and leaves x/y alone. `story_start_ground` is the
  cold story query another job can call: one player, identity search (the file
  position, or the incoming slot when no marker takes), then that snap.
  `ground` is true when the snapped z is not the floor sentinel. When that
  snap has a floor, or a Player1 took, `findFreeSpot` is not called. When a
  `Player1` takes, the original runs `Player1_ctor`'s `findFreeSpot` (spread
  36, radius 24) and draws facing from `g_randSeed`, whose value there is not
  a fixed constant (section 17.2). The identity search is the research's
  ground measurement, not that start, so the result sets `pre_search`: the xy
  is the marker before the search (the original can be up to 36 away) and
  `facing` is left empty. With no marker taken nothing is searched or drawn,
  `pre_search` is false and `facing` is the incoming slot's.
  `entry_start_ground` is the same snap for an explicit previous level,
  entrance, recall flag, gate, or owner-supplied restore xy. A gate entry that
  takes a `Portal` or `PortalHurt` throws `Unsupported`
  (`GateAnyHeightOnDisc`): `JumpGate_ctor`'s anyHeight search was not re-run
  on the disc (section 17.4). A gate flag with no portal takes nothing.
  `resolve_start` runs the caller's search; the caller owns the seed and the
  object list. `apply_act_entry` writes `None` into each recall destination
  when the act changed, the card state is not 4, and no death task is running.
  Acts do not choose a marker.
- Arena wave start is `arena_initial_place` (a0 ≠ 0): `arena_ring_angle`,
  ring radius `ARENA_RING_MODE_A` (0x43340000, 180) or `ARENA_RING_MODE_B`
  (0x42100000, 36), search spread 36 then spread 240 from the same centre when
  the ok byte is 0, and both facing branches. There is no arena name table in
  this module. `arena_mode_a_next` is the rotation step (index 0..12; a zero
  remainder returns the saved index). `arena_record_table_offset` keeps the
  record byte's low nibble times four, with the arithmetic shift that makes
  nibble 8 negative. `arena_register_spawn` writes entry 0 for index 0 and
  appends for index −1. `arena_mode_a_wave_ground` plants the origin, lets
  each `ArenaPlayerStart` overwrite entry 0 and each `ArenaPlayerAlternate`
  append, then places slot 0 at count 1 in mode A and snaps with
  `player_ctor_z`. Slot 0's angle is 0 for every positive count, so its centre
  does not depend on count. `cos16(0)` times 180 is the radius; `sin16(0)`
  times 180 is not 0.
- `arena_mode_a_start_ground` is that start before the search: entry 0, the
  slot-0 mode-A ring centre, and `player_ctor_z` there. `pre_search` is set
  and `facing` is empty. The original's point and facing come from
  `findFreeSpot` with the live `g_randSeed` and the real object list, neither
  of which is documented (sections 17.2, 18.3); section 18.3 proves only that
  the snap stays a floor for every seed and list on the three executed
  archives. `ground` here is the snap at the centre.
- `story_start_ground` has no arena fallback. Section 18.1 shows the three
  cold archives are reached only through the mode-A table at 0x5037b8 (menu
  `vs`, or net type 2/3), which this module does not read. Inferring mode A
  from the records (no floor, no Player1, an `ArenaPlayerStart`) would apply
  it to data the research never ran, so the story start stays the story snap
  and reports no ground there. The caller that knows the level was loaded in
  mode A (mode B set, 0x4f7124 clear: the executed case) calls
  `arena_mode_a_start_ground` itself. The Godot session does so (wave 17)
  only for a level whose story snap has no ground and whose name is in the
  mode-A table it reads from the owner's executable at run time
  (`arena_modes::read_names`, [ARENA_MODES.md](ARENA_MODES.md)); it publishes
  `start.arena_mode` "A". Without that table, or for an unlisted level, it
  still refuses.
- `begin_change`, `LevelStates`: ordered save/act/reset/name handling, ten-entry
  state directory and byte budget, card/flag suppression, hub allocation rule,
  qualified keys, retained entrance/post action/player state/spawn and cleared
  level transients. The typed native state is not a PS2 save-format serializer.
- `Trigger`, `PushTrigger`: inclusive XY rectangle edges, first slot, recall
  suppression; ordered key/action predicates, host/shop gates, once deletion.
  Schedule rectangle polling using the owner's documented 15-frame lists.
  `TriggerState` preserves delay/inside as fields; no undocumented byte packing
  is asserted. Script callbacks retain the presser/active slot in the owner.
- `GateList`, `gate_prompt`, `GateTravel`: discovery, stale hub removal,
  current-level exclusion, distance threshold, offline menu pause, 1-to-181
  travel counter. Confirm then call `advance_ready()` 180 times; the last call
  writes the pending level. Travel survives loading and clears on the first
  ready tick after destination placement, within the documented first/second
  tick window. Fixed destinations suppress destination-gate placement.
- `recall_finish` is the exact reference two-leg operation. `RecallCountdown`
  (`State::transient.recall_countdown`) is g_recallCountdown: `start()` on game
  message 3 sets 60. `recall_tick` is player_recallTick, the first call of
  gameMiscSync every simulation tick. It keeps running during cutscenes and
  pause. Only the 1-to-0 call reads the cutscene flag and alive count, as the
  previous tick left them. A cutscene then, or nobody alive, returns a
  `Dropped*` event: the recall is lost, nothing re-arms it, and no potion is
  refunded. `Finish` writes the request with `ORIENT_RESTORE`. `Aura` (the call
  that sees 60) starts the aura lights, and `Finish`/`Dropped*` clear them.
  These are owner effects. The 60th call writes the request: tick F+59 when
  message 3 ran in net frame F, or S+60 offline (`recall_request_tick`). Pass
  `recall_countdown.running()` to `Trigger::run`. `complete_recall` (round 1)
  remains as the guarded finish application. Resolve absent-player positions
  through the existing free-spot service before supplying them.
- `checkpoint`, `party_wipe`: current-act checkpoint snapshot with camera,
  player positions, records, quests, gates and level states; outside-act no-save
  and max-HP base-name restart; in-act restore with rank 3. Missing last save
  refuses. Keep the returned checkpoint in the owner's save-buffer lifecycle;
  pass `death_task=true` to `change` following a wipe reload. Capture current
  player positions into `State::spawn` before checkpointing; it is not a live
  object-position poll. Alternate death returns `Wait` for the owner task.
- `Pieces`: nearest inclusive 80-unit 3D claim, later tie winner and no fallback
  from an already claimed nearest piece; unclaimed pieces lose collision after
  constructors/deletions/start. Draw pose is absolute relative to rest. Moving
  it never moves collision. Collision/height delegate to the existing terrain
  API. Reset releases claims/enables collision/restores draw pose but retains
  modified element z. Negative indices are no-ops. AutoDoor open means collision
  off and draw offset z=-256.
- `move_piece` is world_movePiece 0x182c98, ported operation by operation: the
  offset is multiplied by 4 first, and rotation words are rounded with floorf(x*32767+0.5)
  and clamped to s16. Translations are truncated toward zero, plain chunks
  rotate by -angle and mirrored (flag 0x40) chunks by +angle. It always reads the
  rest values, so moves never accumulate. `sin16(0)` is 2^-22, so angle 0 is
  not an identity: rest (400,-800,40) opened by (0,0,-256) gives (399,-800,-984).
  `Pieces::move`/`autodoor` store the exact chunk fields in
  `PiecePose::chunks`, one per chunk listed in `Piece::chunk_mirrored`, or one
  chunk with `mirrored` when it is unset. Empty `chunks` means no move since
  load/reset, so the file values apply. The renderer draws them with the
  world.md rule (translation before rotation).

## Resource and Session integration

`LoadServices` requires a disc reader, bounded range reader, player-lump loader,
WORLD-to-model-name resolver and event dispatcher. Reads return owned bytes,
never implicit filesystem searches. The player-lump callback returns true when
it reloaded a model; that causes the documented second clear. Keep levels are
preserved. `resident` lets the owner inject already loaded lumps. The WORLD
resolver and fixup event belong to the terrain/texture integration; water setup
must use the existing Service and owned relocated memory.

`SessionHooks` requires all callbacks. Missing callbacks refuse construction.
These callbacks are the integration seam, not implemented Session internals:

1. `event` executes the named engine service at the indicated boundary, including
   network flush, conditional medal-player restore, pending player selection,
   clear/reset, water allocation/setup, constructor deletion flush, map/decal
   restore, path join, lighting/autosave/immunity, warmup, deferred calls, barrier,
   stock and environment upload. Throw if a required service is unavailable;
   a no-op callback does not establish engine integration.
2. `script` executes `preStart`, `start`, and the retained post action against
   the newly loaded script. Empty script bytes mean no machine; owner skips it.
3. `restore_level_state` decodes saved records in saved grid order. `construct`
   creates ordinary objects and applies their saved state. The adapter itself
   handles Player1/Portal spawn bookkeeping: do not repeat marker draws there.
   Saved records are constructed/flushed first, then saved decals/map are
   restored, then only save-filter classes from static `objects.ob` are built.
4. `construct_player` rebuilds live players with resolved position/facing.
   `remove_flag39_items` handles the owner's inventory representation on act
   reset. `disable_unclaimed_pieces` delegates to `Pieces` after `start`.

The owner should provide `find_free_spot` through the existing object World and
shared RNG, `terrain_height` through its owned `world_queries::Level`, and call
`change` at the main-loop load boundary. Do not call it inside a World scheduler
callback or while existing actor references are borrowed. `advance_ready` is
called once per accepted simulation tick, not per rendered frame.

State and Loader are caller-owned and must outlive SessionAdapter. Loading is a
synchronous sequence with observable prefixes. A thrown callback/read marks the
adapter failed and leaves `ready()==false`. It refuses further work; the owner
must recover/rebuild the adapter and resources. It never publishes a successful
scene after a partial load. Scene replacement/rendering is still the integration
job's responsibility.

## Godot-facing contract for the integration owner

No Godot class or method is registered by this patch. The new native adapter is
ready to wrap in the existing GDExtension without editing owned glue here.
The integration owner can include `native/level_flow_targets.cmake` after the
existing simulation and archive targets, then link `champions-session-level-flow`
into its extension. The include uses its own source directory, reuses
`champions-archive-binding-core` when present, and builds position-independent
libraries. This patch does not change the extension's CMake file.

The binding should expose these operations and return copied Godot values:

| Operation | Native call / result |
|---|---|
| Gate prompt | `SessionAdapter::prompt_gate`: `{usable, text, found}`; alive-player squared distances come from native objects |
| Open menu | `State::travel.open(net_game)`; present `gates.choosable(current)` and reflect `paused` |
| Confirm destination | `travel.confirm(destination,current,gates)`; false leaves selection unchanged |
| Simulation tick | `adapter.recall_tick(...)` in gameMiscSync's slot (start/clear aura on its event) and `adapter.advance_ready()` (true: a gate level request was written). Their relative order within a tick is not documented |
| Next main-loop boundary | apply `cancel_pending` when its documented conditions hold; otherwise `adapter.change(options,rows,placement)` |
| Read presentation | loaded WORLD bytes/row, `base_light_level`, `chunk_light`, `spawn.pos/facing`, `Pieces::poses` (exact `chunks` fields), and copied status/error |
| Failure | expose exception text; keep scene unavailable until owner recovery |

A player walks into gate range; the native prompt checks the party and discovers
the level; the UI confirms a destination; 180 ready ticks write the request; the
next boundary loads it; its Portal constructor sets rank 3 and gate positions;
terrain supplies the top-floor height; players are rebuilt. The synthetic test
executes this entire native path for two authored levels. Actual walking,
GDExtension registration and visual scene replacement remain unverified because
those files are owned by other running jobs.

`level_flow_water_adapter.hpp` forwards only `test` and `disturb` to
`creature_water::Service` using the same Memory/world address. It neither copies
water code nor constructs grids. The optional CMake path below compiles the
existing branch's source read-only into this worktree for adapter tests.

## Evidence conflicts and unsupported exact behavior

These are explicit limits, not inferred approvals:

- The four round-1 refusals (WORLD order, light precedence, recall countdown
  timing, movePiece arithmetic) are resolved by research section 16, which ran
  the original functions in an emulator. They are implemented above.
- Prose says 18 save-filter classes; the executable reference lists 17. The
  exact list is ported, with no invented eighteenth class.
- `refuse_open` throws `Unsupported` for an item named below. Production
  paths call it for the net-peer index (`arena_net_peer_index`) and the gate
  anyHeight search (`entry_start_ground`); the others are named limits that no
  function here implements. Arena respawn (`arena_pickSpawnPoint` with a0 = 0)
  was not executed by research-levels, but research-arena 17810bb executed it:
  `arena_respawn` now forwards the registered spawn table to
  `arena_modes::pick_spawn` with flag 0, and refuses an empty table or a hole
  below count. `OpenItem::ArenaRespawn` stays as the name of the
  research-levels gap. The items that were not executed: the
  live seed at `Player1_ctor`, the class-constructor radii inside
  `findFreeSpot`, the arena name table (the index step is `arena_mode_a_next`; the
  names are not copied), the rest of `Player_ctor` after the radius store and
  the snap, readers of the copied orientation byte, a first-tick sentinel
  reject beyond the snap and the kill plane, a nonzero store of the arena
  facing word through a pointer, the gate anyHeight search on the disc, the
  recall return position as a level field, and the unchecked net-peer index
  at message offset 0xf4 (`arena_net_peer_index`). Aura light colours, scripts,
  retail save byte serialization, original warmup tick count and actual material/water
  construction are not supplied by this module. Required service calls are
  explicit callbacks. They must not be mistaken for verified implementations
  of those services.
- Bounds checks replace original buffer overwrite/crash behavior. Names that
  exceed fixed buffers, 11th state entry, 16th gate, malformed object/archive
  bounds and payloads larger than state allocations refuse explicitly.

## Validation

All public inputs are authored synthetic bytes. The checked-in generator verifies
all research hashes before importing the reference; regeneration requires no
private files. It generates 192 raw32 piece cases, 90 marker decisions, 24 full
spawn/placement traces and an object parser fixture. Round 2 adds vectors with
the same case counts as `difftest_levels.py`, with expected values from
`ref_levels.py`: 204 base light levels, 216 chunk lights, 900 WORLD picks, 100
manifest WORLD steps, 3,430 recall calls, 6 request ticks and 401 `move_piece`
cases. They use their own seeded RNG, so the round-1 lines stay byte-identical.
`round2_cases` ports the 15 `Round2*` reference tests. `round3_cases` ports
`Round3Placement` and `Round3Arena`, including the five arena plans from
`difftest_levels_r3.py`. The generator appends those angle and plan lines
after the round-1/2 prefix. Round 4 appends after that prefix, so the earlier
lines stay byte-identical: the mode-A steps, the record-byte offsets, the
slot-0 ring centre, 34 solid-disc searches (seeds 0..15 and 20973, with and
without one object), 32 hole-disc searches, and the radius-37 lattice
(0 misses of 4293), and nine mode-A starts on a height-3 synthetic field:
the pre-search centre and the seeded search (seeds 0 and 20973, no objects)
for three record lists, composed in the generator from the reference's
`arena_register_spawn`, `arena_ring_centre`, `arena_initial_place`,
`ref_objects.World.find_free_spot` and `player_ctor_z`. The research has no
story-fallback function; that composition is native glue the research did not
execute as one call. `round4_cases` ports `Round4ArenaEntry` and keeps only
structural checks of the fallback. `champions-level-spawn-measure` reads a paired
archive directory and prints counts only. It reads each level lump with
`champions::readLmp` (u32 entry count, research `FORMATS.md`) through
`native/tools/level_spawn_lmp.hpp` and counts the lumps `readLmp` refuses
(`lmp_refused`). It used to read the count as u16, the bug NET-3 fixed in the
netplay tool (`codex/netplay` b06c4e5); `level-flow-exact` feeds an authored
lump whose count word is 0x00010001, which the u16 reading accepted. The
origin-test pass is unchanged.
A second pass decodes geometry-skipped archives and counts a level only when
radius 12 at the origin is the sentinel and an `ArenaPlayerStart` exists. It
prints cold, floor, search-snap, stand-in, lattice, water, alternate and
paired-index counts. It prints no archive names. Decode failures on that
second pass do not fail the tool. It is not a CTest. Native tests also port
the 37 in-scope round-1 reference cases. They exercise malformed boundaries,
kept/reset state, checkpoints/wipes, recall, trigger actions, collision-only
movement, the real terrain highest-floor/no-floor queries and a two-level gate
load. `loader_world_order` loads a lump with several WORLD-like entries. The
other round-1 research tests concern water or textures/materials. Round 2
ports the light-level case and the light part of the chunk-header case. Water
and material implementations are not copied. A separate adapter test executes
the real water Service on synthetic relocated memory.

`test_ref_levels.py` defines its `Round2*` classes after `unittest.main()`, so
running the file as a script reports only the 50 round-1 tests. Load the
module with `unittest` from outside `tools/`, which also avoids the `tools/dis.py`
shadowing (research 16.6). That gives all 65.

```sh
python3 tools/generate_level_flow_vectors.py --research /path/to/research-levels
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release \
  -DCHAMPIONS_LEVEL_FLOW_WATER_ROOT=/path/to/creature-water-2
cmake --build build/native -j 4
ctest --test-dir build/native -R '^level-flow' --output-on-failure
ctest --test-dir build/native -j 3 --output-on-failure
```

Omit the water-root option for standalone core/adapter builds. When an integrated
`champions-creature-water` target exists, it is reused. Compile/run evidence in
the round report is macOS arm64 only. No private disc acceptance, console run,
Godot visual test, Windows/x64 trace comparison or networking synchronization
claim is made.
