# RTA level flow

`champions-level-flow` is portable C++17 for the documented Return to Arms
SLUS-20973 v2.00 level contract. `champions-session-level-flow` is an opt-in
composition adapter. Neither depends on Godot or changes Session, Controller,
save glue, `objects_lifetime.cpp`, scene libraries, or another job's files.

Evidence is pinned by full commit and SHA256 in
[`native/tests/level_flow_contract.json`](../native/tests/level_flow_contract.json):
research commit `36036dab551e85aa99a31eb5d8a6eaa834d952b5` (`t3/research-levels`,
round-2 corrections in `docs/level-flow.md` section 16), `docs/level-flow.md`,
`ref_levels.py`, `test_ref_levels.py`, `difftest_levels.py`, and arithmetic
dependencies `ref_world.py`, `ref_core.py` and `world_info.py`. Round 1 used
`18e1a34`.
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
- Arena placement/rotation, aura light colours, full player/object constructors,
  scripts, retail save byte serialization, original warmup tick count and
  actual material/water construction are not supplied by this module. Required
  service calls are explicit callbacks. They must not be mistaken for verified
  implementations of those services.
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
`round2_cases` ports the 15 `Round2*` reference tests. Native tests also port
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
