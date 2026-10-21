# Shippable builds

These builds are the client only. They do not contain a game folder, disc image,
or extracted game files. After you launch a build, you point it at a folder you
already have.

Godot is 4.5.2 stable (`6ce3de25a`). The GDExtension bindings are the godot-cpp
commit in `gdextension/dependencies.json` (`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`,
single precision). `CMAKE_BUILD_TYPE=Release` is the optimization level. The
library that a release export loads is selected by `-DGODOTCPP_TARGET=template_release`.
The editor loads `template_debug`.

Two extensions ship inside `godot/`:

| Extension | Entry | Release library |
|---|---|---|
| Archive, session, HUD, audio, effects | `champions_archive_init` | `godot/bin/champions_archive.<platform>.template_release…` |
| Level textures and the game-folder world | `champions_level_texture_init` | `godot/bin/champions_level_texture.<platform>.template_release…` |

macOS names have no architecture suffix and are universal (`arm64` and `x86_64`
in one file). Windows and Linux names include `x86_64`. The export presets in
`godot/export_presets.cfg` are **macOS universal**, **Windows x64**, and
**Linux x64**. Each preset drops the other platforms' libraries. Release export
packs the release libraries only.

## Point a build at your game folder

Unzip or copy the export, then launch it. The opening scene is the authored
play scene. Choose **Your game folder…**. That opens the real-data scene, which
is still empty of game files. Choose **Game folder…** and select the folder
that contains your own game data.

The scene reads that folder at run time. It does not write into the folder.
The only copies it makes are WORLD entries, one per level opened, stored
under the app's user data directory and removed when the scene exits.

You can pass the folder on the command line. Put user arguments after `--`:

```sh
# macOS, after opening the .app
"Open Champions.app/Contents/MacOS/Open Champions" -- --game-folder="/path/to/your/folder"
```

```text
OpenChampions.exe -- --game-folder=C:\path\to\your\folder
./OpenChampions.x86_64 -- --game-folder=/path/to/your/folder
```

Launching with that argument still starts on the play scene. **Your game
folder…** then opens the real-data scene, and that scene applies
`--game-folder=`. On the same scene, **Skill recipe JSON…** and **Level lights
JSON…** load optional local JSON files. The matching arguments are
`--skill-recipes=` and `--level-lights=`.

A headless measurement of that scene is included as a project script. It is
not a game-data payload. It requires a cache directory you choose:

```sh
"Open Champions" --headless --script res://tests/real_data_scene.gd -- \
  --game-folder="/path/to/your/folder" \
  --cache="/tmp/open-champions-cache" \
  --levels=12 --ticks=60
```

`--fx-recipes=` and `--level-lights=` are optional on that script. The cache
directory is created by the script and cleared when the run finishes.

## What you need on the build machine

This route was run on Apple Silicon macOS. Windows and Linux libraries are
cross-compiled here; they still have to be launched on those systems.

- CMake 3.21 or newer, Git, and a C++17 compiler for the host (Xcode on macOS).
- [Zig](https://ziglang.org/) 0.17 on `PATH` (`brew install zig`). The Windows
  and Linux wrappers in `tools/cmake/` call `zig cc` / `zig c++`.
- A godot-cpp checkout at the commit above. Pass it as `-DGODOT_CPP_DIR`.
- The Godot 4.5.2 editor, and the matching official export templates in
  `~/Library/Application Support/Godot/export_templates/4.5.2.stable/`.
  `tools/desktop_package.py` pins the template archive SHA-512. The Linux
  x86_64 debug and release templates have to be present or the Linux export
  stops. macOS and Windows x86_64 templates are required for those presets.

The Zig wrappers drop `-static`, `-static-libgcc`, `-static-libstdc++`, and
`-lstdc++`. godot-cpp adds those for a portable runtime. Zig's bare `-static`
would emit an archive instead of a shared library. Zig links libc++ into the
binary itself. The Linux target is `x86_64-linux-gnu.2.28`. The Windows target
is `x86_64-windows-gnu` (MinGW, Universal CRT).

`gdextension/source_path_map.cmake` remaps the checkout, build directory, and
pinned godot-cpp directory in Clang/GCC diagnostic file names. This keeps local
absolute paths out of the exported release libraries, including names expanded
from `__FILE__`; it does not change game data or simulation behavior. Inspect
all four release libraries for build-machine paths before packaging.

## Build the libraries

Set `GODOT_CPP_DIR` to the pinned checkout. Run these from the repository root.
Each extension and platform uses its own build directory. Outputs are written
to `godot/bin/`, which is gitignored.

```sh
export GODOT_CPP_DIR=/path/to/godot-cpp

# macOS universal, editor and release export
cmake -S gdextension -B build/mac-template_debug \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build/mac-template_debug -j 4
cmake -S gdextension -B build/mac-template_release \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build/mac-template_release -j 4
cmake -S gdextension/level_texture -B build/level-texture-template_debug \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build/level-texture-template_debug -j 4
cmake -S gdextension/level_texture -B build/level-texture-template_release \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build/level-texture-template_release -j 4

# Windows x86_64 release
cmake -S gdextension -B build/windows-x64-template_release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/windows-x64-zig.cmake" \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_release
cmake --build build/windows-x64-template_release -j 4
cmake -S gdextension/level_texture -B build/level-texture-windows-x64-template_release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/windows-x64-zig.cmake" \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_release
cmake --build build/level-texture-windows-x64-template_release -j 4

# Linux x86_64 release
cmake -S gdextension -B build/linux-x64-template_release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/linux-x64-zig.cmake" \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_release
cmake --build build/linux-x64-template_release -j 4
cmake -S gdextension/level_texture -B build/level-texture-linux-x64-template_release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/linux-x64-zig.cmake" \
  -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_release
cmake --build build/level-texture-linux-x64-template_release -j 4
```

The existing `tools/cmake/windows-x64-mingw.cmake` file is the MinGW GCC
route. This export uses the Zig files next to it.

Host native tests are separate from the export libraries:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
```

## Export

Import the project once so Godot sees both extensions, then export. `GODOT` is
the 4.5.2 editor binary. On a macOS build host, build both macOS
`template_debug` extensions above before importing a fresh checkout. The editor
loads those libraries even when the requested release export targets Windows or
Linux.

```sh
"$GODOT" --headless --path godot --editor --quit-after 120 --frame-delay 50
mkdir -p build/exports/macos-universal build/exports/windows-x64 build/exports/linux-x64
"$GODOT" --headless --path godot --export-release "macOS universal" \
  "$PWD/build/exports/macos-universal/OpenChampions.zip"
"$GODOT" --headless --path godot --export-release "Windows x64" \
  "$PWD/build/exports/windows-x64/OpenChampions.exe"
"$GODOT" --headless --path godot --export-release "Linux x64" \
  "$PWD/build/exports/linux-x64/OpenChampions.x86_64"
python3 tools/verify_cross_exports.py
```

The verifier checks both embedded packs, their release-library descriptors,
checksums, exact copied libraries, expected three-file layouts, game-data file
types, the current home path, and the `private-validation` marker. It does not execute either
target binary.

Keep every file the exporter writes in that directory. On Windows the two
release DLLs sit beside `OpenChampions.exe`. On Linux the two release `.so`
files sit beside `OpenChampions.x86_64`. On macOS the release dylibs are inside
the zipped `.app`. The macOS preset does not sign or notarize the app.

The Windows player imports the Universal CRT (`api-ms-win-crt-*.dll`), which
current Windows 10 and Windows 11 installs already provide. It does not need a
separate MinGW runtime. The Linux player and both `.so` files import only the
glibc family. The player binary's ELF note asks for Linux 5.15.0, and its
glibc symbols go up to 2.28, so launch it on a distribution at least that new.
The extension `.so` files themselves do not ask for a glibc newer than 2.16.

Cross-build and export checks on macOS establish binary format, imported
libraries, exported entry points, and package contents. They do not establish
that Windows or Linux can load or run the client. On a Windows machine, run
`OpenChampions.exe --headless --quit-after 40`
from the export directory and confirm both DLLs load. On a Linux machine, run
`ldd` on the player and both `.so` files, then
`./OpenChampions.x86_64 --headless --quit-after 40`.
