# Native archive browser

This is a desktop archive inspector, not a playable Champions port. It opens a
user-selected GOB or LMP, validates its structure, and displays metadata including
nested LMP entries. It shares the standalone inspector's C++ parser through a
small GDExtension. No game executable, BIOS or emulator is involved.

Select **Return to Arms** or **Champions of Norrath** explicitly. This records the
intended title, not a detected revision. RTA archive comparisons have private
reference evidence; Norrath remains unverified. Neither title selection certifies
that an archive contains supported game assets. Textures, meshes, audio, scripts
and gameplay are not decoded or executed.

## Reproducible dependencies

- [Godot 4.5.2 standard](https://godotengine.org/download/archive/4.5.2-stable/),
  single precision; use its matching 4.5.2 export templates when exporting.
- [godot-cpp](https://github.com/godotengine/godot-cpp/tree/e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77),
  Godot 4.5 API, commit `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`.
- Python 3, CMake 3.21+, Git and a C++17 compiler. macOS uses Xcode command-line
  tools; Windows uses Visual Studio with C++ desktop tools for the chosen CPU.

The pin is recorded in `gdextension/dependencies.json` and checked at configure
time. Bindings are fetched into ignored `work/`, not copied into the public source.
The CI workflow checks the official runtime's pinned SHA-512 before executing it.
The Godot executable below is called `godot`; substitute its installed path.

```sh
git clone --branch godot-4.5-stable --depth 1 --recurse-submodules \
  https://github.com/godotengine/godot-cpp.git work/deps/godot-cpp
git -C work/deps/godot-cpp checkout e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77
cmake -S gdextension -B build/godot-extension \
  -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release
cmake --build build/godot-extension --config Release --parallel 4
godot --headless --path godot --editor --import
godot --headless --path godot --script res://tests/archive_smoke.gd
godot --path godot
```

In PowerShell, use `$PWD/work/deps/godot-cpp` as the path value and add `-A x64`
or `-A ARM64` to the configure command. Build each architecture in a separate
build directory. The extension goes into `godot/bin/`; the descriptor selects the platform library.
Building for one CPU does not provide other CPU builds. macOS host and universal
builds share a library filename; rebuild the universal target before a Mac export.
The Windows extension uses the MSVC runtime; packaging/installing its appropriate
redistributable for a clean machine remains a distribution task.

The standalone build remains available with `cmake -S native -B build/native` and
has no Godot dependency. Its seven CTest cases still exercise the original core.

## Inputs and limits

The browser reads files without extracting or modifying them. Keep originals
outside the repository and select them at runtime. Its initial interactive limits
are 64 MiB per archive, 20,000 displayed records and 1,024 bytes per entry name.
Larger input is rejected rather than allocating an unbounded browser model. The
command-line inspector remains available for metadata work outside these UI limits.

Names are escaped to retain byte values and prevent control characters from
altering the display. File paths support Unicode through Godot FileAccess.
Parsing must finish successfully before any result is shown; errors clear stale
results. A valid empty archive is different from a rejected archive.

## Desktop exports

Install the official Godot 4.5.2 export templates through the editor, or place the
appropriate template in Godot's versioned export-template directory. Build the
extension for the matching architecture first. Export presets live in
`godot/export_presets.cfg`; exported files go to ignored `build/exports/`.

The official Mac template is Universal 2. Build both CPU slices before using the
`macOS universal` preset (run these commands on macOS):

```sh
cmake -S gdextension -B build/godot-universal \
  -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/godot-universal --config Release --parallel 4
mkdir -p build/exports/macos-universal
godot --headless --path godot --export-release "macOS universal"
```

Other presets are `Windows x64`, `Windows ARM64` and `Linux x64`; create their
output directories shown in the preset file first. The app's **Licenses** button
shows the bundled authored-code, Godot, godot-cpp and dependency notices.

The initial exports are development tools. They are not notarized or signed for
public distribution and include no original game content. Verify both the actual
exported application and the UI before claiming support on another platform.

## Verification boundary

The Godot workflow builds and loads the actual extension on Apple Silicon and
Intel macOS, Windows x64 and ARM64, and Linux x64. Its smoke script creates
synthetic fixtures and checks real binding behavior. These automated checks do
not prove rendering, file-picker behavior or installation on a clean machine.

Local exported-app inspection and current CI results are recorded after validation.
Asset decoding, gameplay, Norrath baseline validation and native multiplayer remain
separate milestones in the [roadmap](ROADMAP.md).
