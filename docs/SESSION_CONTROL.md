# Bounded controllable Session scene

The default Godot scene submits integer keyboard commands to a native RTA
Session. WASD moves one actor, H requests a health-test hit, Escape pauses,
and buttons reset, stop, load optional files or open the archive browser.
Original game content is neither required nor included. The current scene selects
[dry creature movement](SESSION_CONTROL_CREATURE.md) explicitly; the native
Controller keeps isolated movement as its default for existing callers.

## Gameplay ownership

The native Controller owns the World, actor, health binding, terrain and actual
one-peer Session network pipeline. Its first submission produces no ready tick;
the next applies the prior imported pad. The current scene runs original dry
`Creature_moveBy` against the same owned terrain and HP memory, using the game's
raw-bit arithmetic. The legacy `Isolated` policy remains available. The default movement result
is `0x3f7fffff` along one axis. Health starts at raw `0x42c80000` and an authored
hit applies the existing damage operation with amount two. No Godot transform,
physics delta, normalization or binary64 value enters gameplay.

The mapping of WASD to velocity, H to a hit and the flat practice terrain are
**authored test policy**. The original isolated controller uses
`rta-session-control-v1`; the scene's explicit dry creature policy uses
`rta-session-control-creature-v1`. Neither is a reconstruction
of Player_move, combat targeting, original stick mapping or game timing. The
presentation clock submits one frame per 30 Hz callback; it does not alter native
arithmetic or establish the original game's real-time pacing.

Pause, focus loss and file dialogs release input. Resume discards older buffered
controls instead of catching up. A hit edge is queued by input frame, applied once
on its genuine ready frame, and does not repeat while held. Invalid commands
refuse before mutation. Operational failure retains partial state, latches a stop
and cannot be silently retried. Reset creates a fresh Session with the same
configuration and initial terrain. The current scene stops at 512 ready ticks;
the legacy isolated policy retains its 2,000-ready-tick limit. The existing 8,008
submission cap remains. Complete histories are retained; see the new policy's
[budget and validation](SESSION_CONTROL_CREATURE.md).

## Terrain and appearance

The terrain-file parser accepts its documented bounded WORLD subset independently
of presentation. This scene renders only one enabled, plain, constant, unrotated
heightfield covering the full grid. Nine native ground probes must agree with
the proposed flat display before an atomic replacement. Unsupported presentation
leaves the current terrain and Session intact. Geometry, complex heightfields,
pieces and complete original level rendering remain work.

A selected GOB/LMP may supply a supported static VIF model as cosmetic appearance.
Selection uses its entry index and captured archive fingerprint, so duplicate
names or a changed file cannot select the wrong bytes. Unsupported, empty and
all-hidden models leave the current visual intact. Appearance changes never reset
position, health, network frames or seeds. Animation and original materials are
not inferred. Native X/Y/Z maps to presentation X/Z/-Y only after observation.

Only RTA's validated contract is exposed here. A shared application and successful
exports do not validate Norrath gameplay. This is a controllable systems test,
not a playable Champions game or proof of original-console network compatibility.

## Exact replay and application gates

The original isolated-policy Python generator executes seven immutable reference model bodies,
with five pinned snapshot helpers. It generates commands from authored recipes,
not native output. The 101 cases cover all 16 arithmetic configurations for
movement and health edges, plus pause, focus, reset, stop and limit policies.
Every initial Controller/Session/World/network/health/terrain state and lossless
per-action delta is retained and compared exactly. The older 235-case Session
terrain trace uses the same extracted test-only serializer; its fixture is unchanged.

The Godot smoke checks execute the actual GDExtension, dispatched keyboard input,
terrain replacement, cosmetic model selection, pause/focus/reset and failure
behavior. Desktop packaging independently requires completed zero-failure archive
browser and scene checks from the exported executable. Cross-platform CI runs
these checks on its actual hosts; local success alone does not prove Windows or
Intel Mac behavior. Original files and private validation artifacts stay outside
source, fixtures and exports.

Local implementation evidence is recorded below only after the corresponding
checks complete. Platform execution evidence belongs to the pull request's exact
head and downloaded reports, rather than stale local receipts.

## Historical isolated-policy implementation evidence

The results below describe the original isolated-policy milestone. Current
DryCreature scene evidence is recorded in [its own guide](SESSION_CONTROL_CREATURE.md).

The optimized and AddressSanitizer/UndefinedBehaviorSanitizer API checks each
passed all 236 assertions. The sanitized native runner matched all 101 complete
Python states exactly, SHA-256
`f9c4695819411c9cfeeedf9bd834002072f397f3b4de3243374e306327ec4644`.
Its 26 linked simulation units and both controller entrypoints were instrumented;
macOS leak detection was disabled, so this is not a leak test. Three independent
final-source Python generations produced byte-identical gzip and manifest files.

The source GDExtension scene passed 40 checks. A precommit working-tree Universal2
release export independently passed 40 scene and 283 archive-browser checks. That
local preview's metadata identifies the loader base commit, not this uncommitted
scene; it is unpublished, and does not stand in for exact-head CI. The visible
export was inspected on Apple Silicon: keyboard movement changed the ground view,
H changed health 100→98, paused H left health 98, and keyboard navigation to Reset
restored health 100 and the initial ground view. Its window, controls and branding
were inspected. The UI automation service subsequently timed out during optional
file-picker input; manual private-file selection was not verified through it.

A separate real-GDExtension check selected one previously validated owner VIF in
an authored LMP wrapper stored outside this repository. It created a nonempty
ArrayMesh while leaving native position, HP and frame counters unchanged. This
checks real model decoding and scene assignment, not its visual appearance or
pair compatibility. The terrain validator found no recognized WORLD files in the
selected private corpus; terrain proof remains authored fixtures and model parity.

Removing one redundant owned Session observation from the bridge reduced the same
120 neutral-command workload from 832,983µs to 568,671µs, with identical 119 ready ticks,
raw HP and position. This single local workload measures the redundant read; it
is not a general frame-rate benchmark. Shared target dependencies, Windows CRT,
PIC and the lossless serializer extraction received independent read-only review.

All 71 native CTest cases passed locally, including the new 101 exact controls,
the unchanged 235 Session-terrain cases and 133 terrain-file cases. All 319
Python unit tests passed with 170 explicitly skipped native/platform paths;
dedicated CTest transport checks exercised the new native runner. LLVM 20
formatting, GCC 15 syntax checks over four controller/trace translation units and
the 50-file simulation boundary passed. Native and Godot builds use one shared,
strict-arithmetic target graph with PIC and consistent static Windows CRT.

The control generator now publishes complete, synced temporary files with atomic
replacement, fixture first and manifest last. Seven failure-path tests verify
that interrupted generation preserves prior complete files, and that a failed
second replacement leaves a complete fixture with an intact old manifest whose
hash mismatch is detectable. This prevents a partially written gzip being used
as the next baseline; it does not claim a two-file filesystem transaction.
