# Windows player package

Extract the entire ZIP, then double-click `OpenChampions.exe`. Godot and the
Visual C++ redistributable are not separate installation requirements. The pack
is embedded in the EXE; keep the release extension DLL beside it. The default
scene is the bounded authored movement/health test, with an **Archive browser**
button. This is not a completed Champions game.

`README-windows.txt` explains the controls and selecting files from the player's
own data folder. There is no global folder import: **Open archive** selects a
GOB/LMP in that folder; **Open terrain** and **Choose appearance** are optional
file selections. Nothing extracts a disc or requires a BIOS/original executable.
No original content is included. Norrath asset previews remain unsupported.

## CI and inspected downloads

The latest successful main Godot run inspected on 2026-10-13 was
[38231191334](https://github.com/Open-Champions/open-champions-godot/actions/runs/38231191334),
source `103aac54670b7f57669157df2750d1648f310434`.

`.github/workflows/godot.yml` has **Godot Windows x64** on `windows-2025`,
MSVC `-A x64`, and **Godot Windows ARM64** on `windows-11-arm`, `-A ARM64`.
Both fetch checksum-pinned Godot 4.5.2 and pinned godot-cpp, build distinct
`template_debug` and `template_release` extensions using Release optimization
and a static release CRT, initialize a fresh editor, run the browser and Session
tests, and invoke `tools/desktop_package.py` with the corresponding platform.
The helper exports the release preset with an embedded pack, audits CPU identities
and DLL imports, and runs both smoke tests through the actual exported EXE.

The uploaded `open-champions-browser-windows-{x64,arm64}` artifact contains the
complete inner ZIP, its SHA-256 sidecar, build-info JSON and two smoke logs. It is
not just an editor build. Separate `desktop-export-logs-*` artifacts are diagnostic
logs. Artifacts expire after 14 days; they are not GitHub release assets.

Download commands run from this worktree:

```sh
gh run list --workflow godot.yml --branch main --status success --limit 3 --json databaseId,headSha,createdAt,url
gh run download 38231191334 -n open-champions-browser-windows-x64 -D work/windows/ci-38231191334/x64
gh run download 38231191334 -n open-champions-browser-windows-arm64 -D work/windows/ci-38231191334/arm64
```

Both original ZIP SHA-256 sidecars matched. Every inventoried payload file and
both smoke-log hashes matched their build-info. `file` identified x64 EXE/DLL as
PE32+ x86-64 and ARM64 EXE/DLL as PE32+ Aarch64. Both embedded V3 packs have 53
resources, including the movement scene, archive browser, descriptor and four
license notices; all embedded resource MD5s matched. Extension imports on each
CPU are only `KERNEL32.dll`. Both CI exports logged **283 archive checks, zero
failures** and **70 Session scene checks, zero failures**.

The source descriptor names `res://bin/champions_archive.windows.template_release.<arch>.dll`.
The official exporter puts that DLL beside the EXE. Godot's pinned
[Windows loader](https://github.com/godotengine/godot/blob/4.5.2-stable/platform/windows/os_windows.cpp#L441)
explicitly falls back to this basename beside its executable. The new check
rejects DLLs in arbitrary subfolders rather than changing this working layout.
Embedded-pack checks are authored from the pinned
[pack reader](https://github.com/godotengine/godot/blob/4.5.2-stable/core/io/file_access_pack.cpp#L177).

Ready-to-copy packages with the new Windows guide added locally are:

```text
work/windows/downloads/OpenChampions-windows-x64.zip
work/windows/downloads/OpenChampions-windows-arm64.zip
```

Their EXEs/DLLs are unchanged from the CI downloads. Their build-info records the
original CI ZIP checksum and the local guide addition; their new inventories and
ZIP SHA-256 sidecars were regenerated. ZIP CRC checks passed. Original artifacts
remain under `work/windows/ci-38231191334/`.

## Local cross-export on this Mac

This host already has MinGW-w64 x64 GCC 16.2.0, CMake, the pinned Mac Godot
runtime, godot-cpp checkout and complete matching template archive. No system
packages were installed. Windows templates are extracted from that verified
archive into Godot's user template directory. There is no ARM64 cross compiler
or Wine executable on PATH or in the inspected application/Homebrew locations.
ARM64 builds and execution checks therefore rely on CI.

The local x64 cross-build and export succeeded. Its ZIP is
`work/windows/local-cross-x64/OpenChampions-windows-x64.zip`; PE architectures,
53 embedded resources, resource hashes, ZIP CRCs, inventory and SHA-256 passed.
Its DLL imports `KERNEL32.dll` and Windows UCRT API-set DLLs, with no dynamic
MinGW C++/pthread runtime DLLs. Unlike the MSVC CI build, this binary has not
executed on Windows. Prefer the CI-derived download for transferring to a PC.

Reproduce with your existing pinned dependency/runtime paths (or the dependency
checkout described in [GODOT_BROWSER.md](GODOT_BROWSER.md)):

```sh
cmake -S gdextension -B build/windows-template_release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/cmake/windows-x64-mingw.cmake" \
  -DGODOT_CPP_DIR=/path/to/pinned/godot-cpp -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_release
cmake --build build/windows-template_release -j 4
cmake -S gdextension -B build/mac-template_debug \
  -DGODOT_CPP_DIR=/path/to/pinned/godot-cpp -DCMAKE_BUILD_TYPE=Release \
  -DGODOTCPP_TARGET=template_debug
cmake --build build/mac-template_debug -j 4
/path/to/Godot --headless --path godot --editor --quit-after 120 --frame-delay 50
python3 tools/desktop_package.py --platform windows-x64 --cross-export \
  --godot /path/to/Godot \
  --templates /path/to/Godot_v4.5.2-stable_export_templates.tpz \
  --output-dir work/windows/local-cross-x64 --source-commit "$(git rev-parse HEAD)"
```

The Mac debug binding is for editor import; the Windows release DLL is what is
shipped. Use a new output directory for each export. Cross-export performs the
same content/architecture/import checks, but records null smoke results and
`execution_verified: false`. It cannot substitute for Windows execution. CI's
normal host-only packaging still requires both exported-app smoke tests.

## Validation and limits

Packaging tests use authored synthetic fixtures only:

Final Python discovery passed: 619 tests, including all 26 desktop packaging
checks and both embedded-pack cases; 343 unrelated optional-contract cases were
skipped by their existing prerequisite guards. `git diff --check` passed.
The tracked MinGW toolchain also configured successfully in a fresh
`build/windows-toolchain-check` directory.
The native Release configure/build passed, followed by all 100 CTest cases on
this arm64 Mac with `-j 3` (1635.64 seconds under concurrent worktree load).
No test failed or timed out. Detailed logs remain in ignored `work/windows/`.

```sh
python3 -m unittest discover -s tools -p 'test_*.py'
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
```

No native simulation modules, export presets or workflows were changed. Windows
CI provides headless execution evidence for the downloaded binaries. Graphical
Windows interaction, clean-PC installation and this Mac's cross-built binary's
execution remain unverified. Packages are unsigned. No Wine launch was attempted
because Wine is not installed.

Changed files: `tools/desktop_package.py`, `tools/windows_pack.py`,
`tools/test_desktop_package.py`, `tools/test_windows_pack.py`,
`tools/cmake/windows-x64-mingw.cmake`, `docs/GODOT_BROWSER.md` and this guide.
