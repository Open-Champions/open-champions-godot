# One native client, two games

## Current milestone

The default Godot scene is a bounded, Session-backed movement and health test with
authored controls. A standalone C++17 archive inspector and archive browser remain
available using the same parser through GDExtension. They list GOB/LMP
metadata, including nested LMP entries, with explicit little-endian fields and
bounded tables, names and payload ranges. The browser has explicit title selection
and a local file picker. Selected RTA lump entries can preview the documented
32-bit, plain 4-bit and 8-bit texture subsets. Bounded RTA static
[model forms](MODEL_FORMS.md) support base rest-pose inspection with neutral shading or opaque texture colors and single-bit variant visibility. The portable core also retains per-corner joint indices and exact byte weights; a bounded [skeleton metadata reader](ANIMATION_METADATA.md) now preserves RTA joint hierarchies and signed bind offsets. The [key decoder](ANIMATION_KEYS.md) also retains exact initial poses and velocity records. A [local-pose sampler](ANIMATION_POSES.md) now evaluates one clip at an explicit tick. The [transform builder](ANIMATION_TRANSFORMS.md) composes model-space and skin matrices. The [joint viewer](ANIMATION_VIEWER.md) now displays manually sampled RTA clips. A portable [weighted-position operation](SKINNING_POSITIONS.md) now applies explicit skin transforms; an [explicit Godot sampling bridge](SKINNING_BRIDGE.md) exposes positions for two selected entries. An [experimental sampled model viewer](SKINNING_VIEWER.md) draws explicitly selected pairs with unlit geometry. Semantic model pairing and animation playback remain work. Other palette layouts,
original materials, engine script commands and playable gameplay remain unimplemented. Title labels
are not validated game profiles; texture and model decoding
explicitly rejects Norrath until its contract is established.

See the [browser guide](GODOT_BROWSER.md) for pinned dependencies, builds and
platform verification. Godot and godot-cpp are fetched locally, not vendored.

The initial private archive comparisons cover **Return to Arms only**. Norrath now has an
[experimental hash-pinned executable](https://github.com/Open-Champions/champions-of-norrath/blob/main/docs/BASELINE.md);
its retail revision and native format/behavior compatibility remain unverified here. Public tests
use authored synthetic bytes and do not certify either game's entire data set.

## Architecture and remaining work

| Layer | Responsibility | Evidence boundary |
|---|---|---|
| Portable C++ core (`native/`) | Validated archives, format decoders, simulation contracts and reproducible traces | RTA core, items, combat, script-machine and network-wire model ports have exact tests; archives, texture subsets and model forms decode independently of Godot. Engine integration remains work. |
| Game profiles (planned) | Explicit title, region/revision, baseline fingerprint, supported formats, constants, script/ability/object contracts and save schema | RTA research is a starting point. Norrath needs its own evidence before sharing a rule. |
| Thin GDExtension (`gdextension/`) | Expose validated archive metadata and limited RTA texture pixels and model geometry to Godot | Copies results into owned Godot values; archive fingerprints reject stale preview selections. Native-owned Session commands and observations support the bounded movement test; the scene explicitly selects dry creature movement with authored controls and a 512-ready-tick cap; original player controls remain work. |
| Godot 4 presentation (`godot/`) | Archive selection, nested metadata, 32-bit/plain 4-bit/8-bit texture preview, static model inspection and status/error UI | Authored movement/health controls, flat-terrain presentation and private model selection run through native state; original controls, complete levels, materials and audio remain work. |

Select the title/profile explicitly until detection is verified. Use a profile's
supported revisions and content fingerprints to reject unknown inputs with a
useful error. Share a decoder or behavior only after both games' contracts agree;
keep revisions and differences explicit. A successful GOB listing is not a title
or revision detector. No Norrath executable hash or supported revision is invented.

The source siblings own original PS2 reconstruction:
[Return to Arms](https://github.com/Open-Champions/champions-return-to-arms) and
[Norrath](https://github.com/Open-Champions/champions-of-norrath).
Use their documented behavior to author portable implementations. Binary-matching
progress is measured there and must not be counted as native client completion.
Existing [format research](https://github.com/Open-Champions/champions-return-to-arms-re/blob/main/docs/FORMATS.md)
and the [preserved native assessment](NATIVE_PORT.md)
remain research references; do not duplicate active area-agent investigations.

## Why Godot

[Open-BFME's client](https://github.com/Open-BFME/openbfme-godot) is a useful
precedent for separating a portable C++ core from the presentation engine and
from original-executable matching projects. It does not supply Snowblind assets,
formats, simulation or an automatic PS2 conversion.

Godot's [GDExtension interface](https://docs.godotengine.org/en/stable/engine_details/engine_api/gdextension/what_is_gdextension.html)
and [official C++ bindings](https://github.com/godotengine/godot-cpp) provide the
integration route. The initial browser pins Godot 4.5.2, its matching export templates,
and godot-cpp at its Godot 4.5 API tag. The archive library continues to build and
test without any Godot dependency.

Native client targets are Apple Silicon/Intel macOS and ARM64/x64 Windows.
Godot documents [macOS exports](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_macos.html)
and [Windows exports](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html);
matching extension binaries, renderer behavior and actual exported-app execution
still need per-platform verification. Inspector CI is not Godot export evidence.
Linux remains useful for portable-core tests and tooling; client support needs
its own launch and rendering evidence.

## Current development order

1. **Core game systems.** The [Python-contract C++ core](CORE_SIMULATION.md)
   now implements RTA arithmetic, math, RNGs, pool metadata and handle lists.
   Exact generated vectors and cross-platform state traces gate changes.
2. **Items, then combat.** The [item systems](ITEM_SYSTEMS.md) port records,
   statistics, generation, loot, shops and naming from `ref_items.py`.
   The [combat systems](COMBAT_SYSTEMS.md) now add damage, statistics and
   experience under their own pinned Python-generated exact vectors.
3. **Script machine, then network wire.** The [script machine](SCRIPT_MACHINE.md)
   ports `ref_amx.py` with exact state tests and private script validation.
   The [network-wire rules](NETWORK_WIRE.md) now port `ref_netwire.py` with exact
   packet, peer state and callback tests. Engine commands and socket integration
   remain separate work.
4. **Complete documented model coverage.** [Model-form decoding](MODEL_FORMS.md)
   retains reflection, shell and facing-point data after the gameplay ports. Use optional
   [private-file coverage reports](PRIVATE_DATA_VALIDATION.md) to measure real
   asset acceptance. Do not compare sample occurrences with full-disc totals.
5. **Integrate a controllable scene.** Begin with the owned
   [session/replay host](SESSION_HOST.md), connecting ready network frames to
   explicitly authored tasks through the validated modules. The
   [object runtime](OBJECT_RUNTIME.md) ports the published object lifetime,
   scheduler and query contract. Its [Session adapter](SESSION_OBJECTS.md)
   connects ready frames to the owned World. The standalone
   [creature hit-point function](CREATURE_HP.md) ports damage-to-HP updates;
   [owned combat storage adapter](COMBAT_CREATURE.md) connects combat callbacks
   to the base HP body. A [shared handle adapter](RAW_HANDLES.md) connects
   the existing World pool to explicit creature-memory reads. The
   [Session health adapter](SESSION_HP.md) retains that state across ready
   scheduler frames. The [base combat integration](SESSION_COMBAT.md) connects
   restricted nonplayer damage to the existing shared random authority.
   The [supplied terrain kernel](WORLD_QUERIES.md) ports collision queries and
   the isolated-object movement subset. The [Session terrain adapter](SESSION_TERRAIN.md) connects supplied velocity to owned ready ticks under a strict single-actor domain. The [terrain-file loader](TERRAIN_FILE.md) projects explicitly selected RTA WORLD bytes into the same owned collision data. The [bounded controllable scene](SESSION_CONTROL.md) connects that single-actor domain to keyboard presentation with exact state replay. The standalone [creature collision step](CREATURE_MOVEMENT.md) ports original root motion, water bookkeeping, ordered contacts, gravity and settling through explicit callbacks. Its [dry singleton Session adapter](SESSION_CREATURE.md) now shares the existing World, Level and HP Memory under ready-tick ordering, with explicit authored dispatch storage and complete observations. The [scene's explicit DryCreature policy](SESSION_CONTROL_CREATURE.md) now uses this path with a 512-ready-tick cap while preserving the legacy isolated default and fixtures. Original player controls, water services and dynamic contacts remain unvalidated. The [static WORLD ground decoder](WORLD_GEOMETRY.md) now prepares actual geometry and collision from the same selected bytes, with atomic mesh/controller replacement and explicit water/moving-piece refusals. Continue with supported game-system integration and richer terrain presentation,
   input, collision and saves. The standalone [animation clock](ANIMATION_CLOCK.md)
   now implements cache-free track lifecycle, timing and events from pinned Python
   contracts. The [Session animation adapter](SESSION_ANIMATION.md) connects owned
   timing and events to explicit ready tasks and health state; this authored order
   does not establish original Creature animation dispatch. Scene playback, root
   motion and sound remain separate work. The standalone [raw32 decoder](ANIMATION_DECODER.md)
   now covers clip parsing, owned decoder-slot lifetime and original joint
   decoding, including stale state and failure prefixes. Clock/Session/scene
   consumers and animation-generated root motion require separate integration evidence.
   Compare native peers using exact state traces.
   Validate Norrath separately before sharing RTA behavior.

Viewer extension is paused. RTA byte-exact reconstruction is parked until a
period PS2 `ee-gcc` compiler is used, with function names from the research
repository's `names.tsv` and `symbols/`. Modern Clang probes are not continued.

Each milestone needs an executable result and recorded evidence. A blank window,
placeholder character or accepted archive table is not a completed game feature.

## Deterministic behavior and game differences

Specify integer widths/overflow, conversion rules, floating-point expectations,
random streams, stable iteration order and fixed simulation tick sequencing.
Keep input collection, wall-clock timers, audio, camera, loading speed and render
rate outside authoritative state. Game-specific scripts, object layouts, abilities,
content IDs, navigation, save formats and network behavior are not assumed shared.

Godot physics/navigation and scene multiplayer are useful facilities but are not
proven equivalents of Snowblind collision/pathfinding or the original network
protocol. An initial native save schema is also distinct from PS2 save import or
round-trip compatibility. Cross-platform trace comparison and behavior tests are
required even when both peers use a fixed update rate.

## Content and distribution

Runtime access will use a user-selected game-data folder; original assets stay
outside the source tree and exported packages. Public CI uses synthetic fixtures.
The inspector packages contain only the inspector, license, usage and build metadata.
Browser exports contain authored UI and the native extension, without original game data.
A future playable export must state the exact supported title/revision, operating
systems, remaining provisional behavior and independent verification results.
