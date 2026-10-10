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
  -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension --config Release --parallel 4
godot --headless --path godot --editor --quit-after 120 --frame-delay 50
godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
godot --path godot
```

The bounded editor startup above avoids Godot 4.5.2's [known first-import shutdown
crash](https://github.com/godotengine/godot/issues/111645). CI starts with an empty
import cache, requires normal exit and generated caches, then requires the real
binding's smoke-test PASS marker. It does not ignore crashes or retry them to pass.

In PowerShell, use `$PWD/work/deps/godot-cpp` as the path value and add `-A x64`
or `-A ARM64` to the configure command. Build each architecture in a separate
build directory. The extension goes into `godot/bin/`; the descriptor selects the platform library.
Building for one CPU does not provide other CPU builds. macOS host and universal
builds of each target share a library filename; build both universal targets before
a Mac export. Editor/debug and release libraries have separate filenames and
feature selectors. `CMAKE_BUILD_TYPE=Release` enables compiler optimizations; it
does not select the Godot ABI. The pinned bindings [require `template_release`
for release exports](https://github.com/godotengine/godot-cpp/blob/e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77/tools/godotcpp.py#L244).
The Windows extension and its linked core use the static release MSVC runtime
(`/MT`). Packaging rejects direct Visual C++ runtime DLL imports; no separate
Visual C++ redistributable is intended. Clean-machine installation remains a
separate validation step.

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
for target in template_debug template_release; do
  cmake -S gdextension -B "build/mac-$target" \
    -DGODOT_CPP_DIR="$PWD/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release \
    -DGODOTCPP_TARGET="$target" '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
  cmake --build "build/mac-$target" --config Release --parallel 4
done
mkdir -p build/exports/macos-universal
godot --headless --path godot --export-release "macOS universal"
```

Other presets are `Windows x64`, `Windows ARM64` and `Linux x64`. For these, also
configure and build a separate directory with `-DGODOTCPP_TARGET=template_release`
for the matching CPU while retaining the debug library for editor import. Create
the output directories shown in the preset file first. The app's **Licenses** button
shows the bundled authored-code, Godot, godot-cpp and dependency notices.

The initial exports are development tools. They are not notarized or signed for
public distribution and include no original game content. Verify both the actual
exported application and the UI before claiming support on another platform.

## Verified desktop packages

The Godot workflow exports and runs a standalone application on each of its five
hosts, then uploads a ZIP with its complete export, licenses, build information
and SHA-256 checksum. Mac jobs build Universal 2 binaries and execute them on
Apple Silicon and Intel separately. Windows x64 and ARM64 packages are separate.
These are unsigned development archive browsers, not playable game downloads.

After building both debug and release extension variants and initializing the
project as above, reproduce
the packaging check on the target operating system:

```sh
python3 tools/desktop_package.py --platform macos-universal \
  --godot /path/to/Godot --templates /path/to/Godot_v4.5.2-stable_export_templates.tpz \
  --output-dir build/desktop-package --source-commit "$(git rev-parse HEAD)"
```

Use `windows-x64`, `windows-arm64` or `linux-x64` on those hosts. The output
directory must be new or empty. Templates are checksum-verified before use;
add `--download-templates` to permit fetching the pinned official archive.
The helper verifies both executable and extension CPU architectures, runs the
actual exported application with synthetic inputs, and requires all smoke checks
to finish successfully. It never supplies game files. Packaging boundary tests
run with `python3 -m unittest discover -s tools -p 'test_*.py'`.

## Verification boundary

At revision `a9259bdcabef180a61f9809dbf565809728a7517`, all five
[Godot browser jobs](https://github.com/Open-Champions/open-champions-godot/actions/runs/38035765343)
and all five [standalone foundation jobs](https://github.com/Open-Champions/open-champions-godot/actions/runs/38035765314)
passed. Hosts were Apple Silicon and Intel macOS, Windows ARM64 and x64, and Linux
x64. Each browser job built distinct debug/release bindings, initialized a fresh
editor, passed 23 synthetic checks in the editor and actual exported application,
and passed 21 packaging boundary tests. The foundation ran seven CTest cases on
each host. Successful browser jobs provide downloadable ZIPs, checksums, build
metadata and smoke logs; artifacts are retained for 14 days.

Release exports require a release binding because Godot's debug/release allocator
contracts differ. Reusing the debug binding caused Windows and Linux heap
corruption even though editor tests passed. Separate library names and feature
selectors now enforce the correct choice; packaging rejects debug-only libraries.
Windows release extension import inspection confirms only `KERNEL32.dll` on both
CPUs, with no direct Visual C++ runtime DLL dependency.

On 2026-10-10 the corrected Universal 2 Mac export was also run graphically on
Apple Silicon. Its native file picker and three nested synthetic archive rows
were rechecked. Earlier visual checks covered filtering, title changes, result
clearing, malformed-file errors and the bundled license dialog. The downloadable
Mac package was replaced with the corrected release-binding build. No original
game files were included in packages or screenshots.

These automated checks do not prove graphics or file-picker behavior on Windows,
Intel Mac or Linux, or installation on a clean machine. Packages are unsigned and
not notarized. The tools inspect archive metadata; they are not playable games.
Asset decoding, gameplay, Norrath baseline validation and native multiplayer remain
separate milestones in the [roadmap](ROADMAP.md).
