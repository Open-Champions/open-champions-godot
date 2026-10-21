STATUS: COMPLETE — Windows/Linux x86_64 cross-builds, exports, and static checks passed; host CTest timeout passed alone; target execution unverified.

# Windows and Linux x86_64 build, round 1 (2026-10-21)

## Scope and evidence

- Worktree: `~/Projects/open-champions/worktrees/win-linux`, branch `codex/win-linux`, based on wave 18 `0f3cafc`. The wave-19 report did not exist when this branch was created. No push or PR.
- Read the project `AGENTS.md`, `CONTRIBUTING.md`, `docs/ROADMAP.md`, `docs/BUILDS.md`, the shared job rules, and the wave-18 development matrix at `worktrees/playable-integration/work/playable-wave18/run-godot.py`. The exact `work/run-godot.py` path named in the job is absent from this wave-18 worktree; the wave-18 script is gitignored in its source worktree.
- Pinned Godot `4.5.2.stable.official.6ce3de25a`, godot-cpp `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`, Zig `0.17.0`. Existing `godot/export_presets.cfg` already has `Windows x64` and `Linux x64`; no preset change was needed.
- No game data was read for this job. `~/champions-re` was not touched. No game data was copied into the repository or exports.

## Changes

- `gdextension/source_path_map.cmake` is included by both extension CMake projects. For the documented Clang/GCC build route, `-ffile-prefix-map` remaps source, dependency, and build paths in diagnostics and macros. This is a portable build setting; no simulation behavior, game constants, or platform-specific behavior changed.
- `docs/BUILDS.md` now records path remapping, the macOS editor import prerequisite for cross exports, the static export verifier, and the target-runtime proof boundary.
- `tools/verify_cross_exports.py` checks the two three-file export directories, byte-identical copied libraries, both descriptors, checksummed embedded packs, game-data suffixes, and local private paths. Existing `tools/windows_pack.py` provides the pack parser and Windows loader check.

## Commands and results

Commands below ran from `~/Projects/open-champions/worktrees/win-linux`. Set the pinned paths first:

```sh
GODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp
GODOT=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot
```

Cross-build commands (each initial configuration and build exited 0; after the path-remap change, all four `cmake --build` commands reconfigured and rebuilt successfully):

```sh
cmake -S gdextension -B build/windows-x64-template_release -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/windows-x64-zig.cmake" -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release
cmake --build build/windows-x64-template_release -j 4
cmake -S gdextension/level_texture -B build/level-texture-windows-x64-template_release -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/windows-x64-zig.cmake" -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release
cmake --build build/level-texture-windows-x64-template_release -j 4
cmake -S gdextension -B build/linux-x64-template_release -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/linux-x64-zig.cmake" -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release
cmake --build build/linux-x64-template_release -j 4
cmake -S gdextension/level_texture -B build/level-texture-linux-x64-template_release -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/linux-x64-zig.cmake" -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_release
cmake --build build/level-texture-linux-x64-template_release -j 4
```

Initial `"$GODOT" --headless --path godot --editor --quit-after 120 --frame-delay 50` exited 134: the new worktree lacked `champions_archive.macos.template_debug.dylib`, and Godot crashed while opening that missing library. The following host editor builds exited 0, then the same import command exited 0 without `ERROR`, `WARNING`, or script parse errors:

```sh
cmake -S gdextension -B build/mac-editor-template_debug -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/mac-editor-template_debug -j 4
cmake -S gdextension/level_texture -B build/level-texture-mac-editor-template_debug -DGODOT_CPP_DIR="$GODOT_CPP_DIR" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/level-texture-mac-editor-template_debug -j 4
"$GODOT" --headless --path godot --editor --quit-after 120 --frame-delay 50
```

Exports and static checks:

```sh
mkdir -p build/exports/windows-x64 build/exports/linux-x64
"$GODOT" --headless --path godot --export-release 'Windows x64' "$PWD/build/exports/windows-x64/OpenChampions.exe"
"$GODOT" --headless --path godot --export-release 'Linux x64' "$PWD/build/exports/linux-x64/OpenChampions.x86_64"
python3 tools/verify_cross_exports.py
python3 tools/test_windows_pack.py
file build/exports/windows-x64/* build/exports/linux-x64/*
shasum -a 256 build/exports/windows-x64/* build/exports/linux-x64/*
```

- Exports: both exited 0 with no Godot export errors. Windows and Linux each have exactly the player and two matching release libraries. The verifier passed for both: **243 checksummed embedded resources** each; no game-data file suffix or local private path; the exported libraries match the cross-build bytes.
- `tools/test_windows_pack.py`: 2/2 tests passed.
- `file`: Windows player and DLLs are PE32+ x86-64; Linux player and `.so` files are ELF64 x86-64.
- `llvm-objdump -p` found only expected Windows system/UCRT imports and Linux glibc-family imports. The Windows DLL export tables contain `champions_archive_init` and `champions_level_texture_init`; `llvm-nm -D` found both Linux entry symbols. These are static link checks, not target loader or launch evidence.
- The first release libraries had 74/55 Windows and 24/16 Linux strings containing the local home path. After `-ffile-prefix-map`, `strings -a` found **zero** such strings in all four release libraries. The export verifier also scans the player files and each embedded resource.
- One existing `-Wmissing-field-initializers` warning from `native/src/level_texture/level_texture.cpp` appeared in the level-texture builds; all four final builds linked.

Artifact SHA-256 (the output directories are gitignored and stay local):

| File under `build/exports/` | SHA-256 |
|---|---|
| `windows-x64/OpenChampions.exe` | `45f989167273643bfedb9a744e61ec404539e9c4dde27af83c112eb42e9db418` |
| `windows-x64/champions_archive.windows.template_release.x86_64.dll` | `b6b75c645f49f20e327eaea270b97690fc16b6dc670e9b0065c73fdca4b61f84` |
| `windows-x64/champions_level_texture.windows.template_release.x86_64.dll` | `d25c08ef7e9360050f0328d5844f6139b0d26a527f3cd30b84115edbf3c018f2` |
| `linux-x64/OpenChampions.x86_64` | `6b795a412eb24aad263be8db83036a48088c77d10b0dd44f0fe840717167deb5` |
| `linux-x64/champions_archive.linux.template_release.x86_64.so` | `aa7050b372df891636e2854d53193ce70e8c8f62d0af1442e1413423abc2b89e` |
| `linux-x64/champions_level_texture.linux.template_release.x86_64.so` | `dc977a19bb57d95b5da5d81c843296e3c76e6709deac9947e06604e5826797a1` |

Native host test commands and results:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native -R '^player-exact$' --output-on-failure -j 1
```

- Release configure and build: exit 0. The full macOS arm64 CTest run completed in **1202.94 seconds** with **210/211 passed** and `player-exact` timed out at **240.04 seconds** (CTest exit 8). There were no assertion failures.
- The required isolated `player-exact` retry passed **1/1** in **174.58 seconds** (exit 0). This resolves the timeout as a parallel-run limit, not a demonstrated test failure. The full parallel run is still accurately recorded as 210/211.

The command logs remain in the ignored `work/win-linux-*.log` files. Portable build changes and documentation were committed as `befb102`; the export verifier was committed as `b147096`. All commits are local.

## Runtime limits

- No Windows launch: Wine is absent and there is no Windows VM available through this session.
- No x86_64 Linux launch or Linux-target native CTest: Docker's local daemon socket is unavailable; Colima lists only stopped ARM64 profiles; `utmctl list` returns OSStatus `-1743` and says it cannot work from this session; no `qemu-x86_64` executable is installed. These target-platform tests remain unverified.
- The macOS native Release CTest is host evidence only. It does not substitute for Linux-target CTest.
