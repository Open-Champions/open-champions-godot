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
32-bit and plain 4-bit lump textures; 8-bit palettes, models and gameplay remain unfinished. No emulator, BIOS or game executable is needed to build or run these tools.

| Game | Current evidence |
|---|---|
| Champions: Return to Arms | Archive inspection tested against private local files; format and behavior research available in the source project. |
| Champions of Norrath | Included in the intended client scope. Its baseline, archive differences, assets and behavior still need independent validation. |

Do not treat a shared file extension or engine family as proof that the two games
have identical formats. The current CLI has no game-profile detection.

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

The workflow builds and runs nine CTest cases on Apple Silicon and Intel Macs,
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

The [texture preview guide](docs/TEXTURE_PREVIEW.md) describes the supported 32-bit and plain 4-bit
subset and evidence limits. Next are broader texture formats, a static mesh and a
small controllable scene, using the owner's game files. Read the
[two-game roadmap](docs/ROADMAP.md), [detailed native assessment](docs/NATIVE_PORT.md),
[contribution guide](CONTRIBUTING.md) and
[source origin](docs/ORIGIN.md).

This is an independent community project. Game names identify compatibility
targets; no affiliation or official endorsement is implied. Original game files,
extracted assets and proprietary code are not included. The identifying logos
above retain their owners’ rights and are excluded from the MIT license; see
[artwork credits](https://github.com/Open-Champions/.github/blob/main/ARTWORK.md).
Authored source is available under the [MIT license](LICENSE).
