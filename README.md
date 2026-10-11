# Open Champions

<p align="center">
  <img src="https://raw.githubusercontent.com/Open-Champions/.github/main/profile/assets/champions-of-norrath-logo.png" alt="Champions of Norrath" width="260">
  <img src="https://raw.githubusercontent.com/Open-Champions/.github/main/profile/assets/champions-return-to-arms-logo.png" alt="Champions: Return to Arms" width="260">
</p>

A shared native client project for **Champions of Norrath** and
**Champions: Return to Arms**, using **Godot 4 + C++**.

The working tools are a **Godot archive browser** and `champions-data`, a command-line
inspector. Both use the same checked C++17 GOB/LMP parser. The browser opens local
archives and shows their nested entries. A limited preview decodes documented RTA
32-bit, plain 4-bit and 8-bit lump textures. A bounded static model viewer adds rest-pose geometry, single-bit variant visibility and optional texture-color inspection; broader formats, materials, animation and gameplay remain unfinished. No emulator, BIOS or game executable is needed to build or run these tools.

| Game | Current evidence |
|---|---|
| Champions: Return to Arms | Archive inspection tested against private local files; format and behavior research available in the source project. |
| Champions of Norrath | Included in the intended client scope. Its source project has an experimental hash-pinned executable; the exact retail revision and native archive/asset/behavior compatibility remain unverified. |

Do not treat a shared file extension or engine family as proof that the two games
have identical formats. The current CLI has no game-profile detection.

The portable game-rule libraries now cover RTA core, items, combat, AMX and
network-wire Python contracts. An [owned session host](docs/SESSION_HOST.md)
composes them through explicit callbacks. The [object runtime](docs/OBJECT_RUNTIME.md)
ports base-object lifetime, scheduler order and queries. Its optional
[Session adapter](docs/SESSION_OBJECTS.md) runs the modeled scheduler on ready
network frames. A standalone [creature damage function](docs/CREATURE_HP.md)
updates hit points and pending damage with ordered handle callbacks. An explicit
[owned combat adapter](docs/COMBAT_CREATURE.md) connects combat to that storage.
A [shared handle adapter](docs/RAW_HANDLES.md) connects the existing object
pool to an explicit readonly creature-memory view. The [Session health adapter](docs/SESSION_HP.md)
retains base-creature health across ready frames with explicit bindings and shared
handles. Its [base combat integration](docs/SESSION_COMBAT.md) draws directly from
the same object random stream for explicitly configured nonplayer damage.
The [supplied terrain kernel](docs/WORLD_QUERIES.md) provides a separate collision
and isolated-movement contract. The [Session terrain adapter](docs/SESSION_TERRAIN.md) connects supplied velocity to owned ready ticks under a strict single-actor domain. The [bounded terrain-file loader](docs/TERRAIN_FILE.md) supplies that data from explicit RTA WORLD bytes. A controllable scene remains work.
Viewer extension is paused.

## Build the working tool

Requirements: C++17 and CMake 3.21 or newer. This standalone tool does not depend on Godot.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
build/native/champions-data list "/path to your game/level.GOB"
```

With Visual Studio on Windows, add `-A x64` or `-A ARM64` to configuration and run
`build/native/Release/champions-data.exe`. See the [inspector guide](native/README.md)
for output, Unicode paths, error handling and the exact validation boundary.

The workflow builds and runs native CTest cases on Apple Silicon and Intel Macs,
Windows ARM64 and x64, and Linux x64. Successful jobs package the inspector with
a checksum, license, usage guide and build information. These are command-line
tool packages; there is no playable client download yet. Inherited platform test
results are linked in the inspector guide; new repository runs establish this
repository's own build status.

## Open the Godot browser

Follow the [browser build and validation guide](docs/GODOT_BROWSER.md). It pins
Godot 4.5.2 and the C++ bindings, keeps private game files outside the project,
and explains the separate platform checks. Both titles appear in the browser;
Norrath remains explicitly unverified. A title choice is not automatic detection.
The [Godot workflow](https://github.com/Open-Champions/open-champions-godot/actions/workflows/godot.yml)
exports and smoke-tests standalone desktop ZIPs for macOS Universal 2, Windows
x64/ARM64 and Linux x64. Each package includes licenses, a checksum and build
evidence. These development packages are unsigned archive browsers.

## Three projects, separate evidence

- [Champions: Return to Arms source reconstruction](https://github.com/Open-Champions/champions-return-to-arms): original PS2 behavior, research and byte-matching reconstruction.
- [Champions of Norrath source reconstruction](https://github.com/Open-Champions/champions-of-norrath): a separate game baseline and reconstruction effort.
- **This repository:** shared portable libraries and the Godot client for both games.

[Existing Return to Arms research](https://github.com/Open-Champions/champions-return-to-arms-re)
provides the initial evidence. A reconstructed PS2 function and a native Godot
feature are different milestones; progress in either project is not completion
of the other.

The [texture preview guide](docs/TEXTURE_PREVIEW.md) describes the supported 32-bit, plain 4-bit and 8-bit
subset and evidence limits. The [model preview guide](docs/MODEL_PREVIEW.md) covers
static rest-pose geometry, variant inspection and remaining material/animation limitations.
The portable [skeleton reader](docs/ANIMATION_METADATA.md) preserves RTA hierarchy
and bind metadata; the [key decoder](docs/ANIMATION_KEYS.md) retains initial poses
and velocity records. The [pose sampler](docs/ANIMATION_POSES.md) evaluates local
joint poses at explicit ticks, and the [transform builder](docs/ANIMATION_TRANSFORMS.md)
forms model-space joint and skin matrices. The [animation joint viewer](docs/ANIMATION_VIEWER.md)
now displays an explicitly selected RTA clip at a chosen tick. The portable [weighted-position operation](docs/SKINNING_POSITIONS.md) applies
explicit skin transforms. The [explicit Godot sampling bridge](docs/SKINNING_BRIDGE.md) returns owned positions for two selected entries; the [experimental sampled model viewer](docs/SKINNING_VIEWER.md) draws explicit selections with unlit geometry and an unverified-pair label. Retail pair compatibility and playback remain work. Next are
broader formats and a small controllable scene, using the owner's game files. Read the
[two-game roadmap](docs/ROADMAP.md), [detailed native assessment](docs/NATIVE_PORT.md),
[contribution guide](CONTRIBUTING.md) and
[source origin](docs/ORIGIN.md).

This is an independent community project. Game names identify compatibility
targets; no affiliation or official endorsement is implied. Original game files,
extracted assets and proprietary code are not included. The identifying logos
above retain their owners’ rights and are excluded from the MIT license; see
[artwork credits](https://github.com/Open-Champions/.github/blob/main/ARTWORK.md).
Authored source is available under the [MIT license](LICENSE).
