# Opt-in scene level-flow fixture

Wave 5 adds `start_play_session(combat_fixture=false, level_fixture=false)`.
The zero-argument API remains normal bounded play with continuation saves;
`true` remains the combat fixture. Passing `(false, true)` or selecting **Level
fixture** before Start selects `rta-level-fixture-v1`. Both flags together refuse
without changing the owner. This integrates the released partial contract in
[LEVEL_FLOW.md](LEVEL_FLOW.md); it does not complete its unsupported cases.

## Real modules and closed fixture inputs

`level_play::Session` owns a native `level_flow::State`, `Loader`, `SessionAdapter`,
objects World, wire machine, task list, terrain Level, empty Pieces collection and
real creature-water Service. The bridge releases its previous actor owner when
publishing this policy. No Godot simulation or duplicate legacy actor Session runs.

Two authored in-memory GOBs (`aa`, `aab`) contain real LMP directories, a valid
flat collision WORLD, Player1/Portal object records and an empty water resource.
The real GOB, LMP and objects parsers load them. The terrain-file decoder supplies
collision data, actual World `find_free_spot` supplies marker placement, and the
same World shared RNG supplies facing. `SharedRandom::draw()` already returns the
upper 16 bits: the adapter must not shift it again. Native `terrain_height` supplies
placement height. An explicitly empty relocated water tree is queried through the
actual `WaterAdapter` and creature-water Service. No downloaded or retail assets
are needed. `CHAMPIONS_LEVEL_FLOW_WATER_ROOT` points to this integrated worktree.

The avatar is an authored ReferenceBase GameObject (100 HP), not the original
Player constructor. Each level has one authored 96×96 gate footprint around its
Portal; aa's marker is (0,0,0), aab's is (64,0,0). The real marker policy scatters
placement using spread 36/radius 24; this is not exact center placement. The scene
draws the resolved avatar position and an explicitly authored gate rectangle over
the collision-plane illustration. It does not claim retail world geometry.

This host accepts only its own byte-exact WORLD and two generated GOBs. There are
no model, script, equipment, audio, particle, decal, path, shop, inventory-object or
network-peer services in these fixtures. Mandatory loader/adapter notifications
for those empty domains are explicitly enumerated; unknown events, object classes
and nonempty script bytes refuse. Empty Pieces uses the real disable-unclaimed
operation. These closed empty-domain callbacks are **not production engine hooks**.
The standalone loader's full required hooks still need real engine implementations
before arbitrary levels can use this route.

## Gate and level-state boundaries

The priority-10 task invokes `advance_ready()` once per accepted native wire tick.
The initial submission has no ready tick. A separate native Trigger polls every
15 level ticks. The fixture gate action checks the player's authoritative XY
against that trigger footprint before calling the native gate prompt/open route.
The initial discovered list explicitly includes aab; this is fixture setup, not an
invented discovery mechanism for real levels.

`level_gate_open()` pauses via native gate state. `level_gate_confirm("aab")`
starts at travel tick 1. After 179 ready ticks the player is still in aa at travel
counter 180; the 180th ready tick writes the pending request. The outer main-loop
boundary runs `SessionAdapter::change()` after the scheduler returns. A guard
rejects loader notifications inside that scheduler. The destination Portal wins
native marker precedence, terrain height is applied, and one new avatar is built.
The next ready tick clears travel. Render frames, pause and observation calls do
not advance this clock. Godot interpolation snaps on a level-generation change.

`level_request("aa")` is the fixture return action, allowed only after arrival
cleanup in aab. Before teardown, the adapter stores an 88-byte owned fixture blob
through native `LevelStates`. It includes an `OCLF` version-1 envelope, Trigger's
saved delay/inside state, and actual serialized Player1/Portal object records.
Re-entry decodes those bytes, restores Trigger state and uses native saved-record
construction/filtering/marker placement. The acceptance enters aa's trigger,
travels to a fresh aab whose trigger is initially false, then returns to aa and
observes its saved true state. This proves state restoration rather than a fresh
reload or a test callback that returns hardcoded restored records.

This per-level state is in memory and is **not a retail save**. Since the
2026-10-20 review fixes the whole fixture saves to disk as a
[fixture continuation](NATIVE_SAVE.md#fixture-continuation-v1): the journal of its
operations (advance, gate open / confirm, level request, pause, stop) and a witness of
the flow, observation and shared seed; a save made mid gate travel continues
byte-identically.
A valid ordinary continuation may atomically replace this policy; invalid loads
retain it. Reset creates a new fixture and its one authoritative owner.

## Limits and verification

The checkbox defaults off. Nonzero movement/damage commands, optional diagnostics,
unknown destination requests, pre-menu travel, early return and simultaneous
combat/level policies refuse. This is a separate 4,096-ready-tick fixture budget;
normal play retains its 36,000+ tick acceptance. It does not combine combat actors
with level transitions, expose arbitrary disc roots, serialize whole-game state,
or add a generic script interpreter.

Wave 10 (`level-flow-2` at `2c57ba3`) supersedes the former native refusals:
stored-order WORLD selection, base/chunk light, cutscene recall countdown and
packed moving-chunk transforms are now documented and implemented. The existing
scene host does not access `RecallCountdown` directly and needs no type conversion.
Native exact tests cover the expanded contracts; scene acceptance still claims
only the two-level gate and saved-state subset above.

`level-play` tests native timing, no-ready/pause behavior, marker facing from the
shared RNG, placement, actual serialized-state re-entry and refusals. Godot's
`playable_wave_smoke_level.gd` extends the existing full flow: real library/load,
animation/art/HUD/inventory, bounded play/save/continue, combat fight, then the
level fixture. It captures gate contact, menu, tick 179, arrival, restored aa and
paused restored state. Detailed results and artifact paths are in the wave-5 report.
