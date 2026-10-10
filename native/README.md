# Native data inspector

`champions-data` is an authored C++17 archive metadata tool. It reads GOB and LMP
files from your own copy of Champions: Return to Arms. It needs no game executable,
BIOS, emulator or third-party library. It is a foundation for native development,
not a playable game client. This library is now the foundation for a shared
Godot client targeting both Return to Arms and Champions of Norrath. Norrath
archives and revision differences have not been validated; the current CLI does
not detect or select game profiles.

## Build and test

Requirements: a C++17 compiler and CMake 3.21 or newer. Tests also require Python 3.

The portable [core game systems](../docs/CORE_SIMULATION.md) now implement the
RTA Python reference model independently of the inspector and Godot viewers.
The [item systems](../docs/ITEM_SYSTEMS.md) implement records, stats, generation,
loot, shops and localized names using the same gameplay arithmetic.
The [combat systems](../docs/COMBAT_SYSTEMS.md) implement damage, statistics and
experience with explicit records and callbacks.
The [script machine](../docs/SCRIPT_MACHINE.md) implements the RTA interpreter
and provides optional private-file differential validation.
CTest compares generated exact arithmetic, math and complete simulation state vectors.
This is not a playable game or independently validated Norrath simulation.

```sh
cmake -S native -B build/native
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

For a single-configuration generator the executable is
`build/native/champions-data` (or `.exe`); Visual Studio places it under
`build/native/Release/`. On Windows choose a native target with `-A x64` or
`-A ARM64` when configuring the Visual Studio generator. All dependencies must
match the target architecture; this tool has only standard-library dependencies.

```sh
build/native/champions-data list "/path with spaces/level.GOB" "/path/config.LMP"
```

Windows uses the wide-character command-line entry point and filesystem paths,
so Unicode input paths do not depend on the active console code page. macOS/Linux
paths are read as UTF-8. File extensions are case-insensitive.

The optional `champions-data-validate --game rta <directory>` tool reports
decoded/refused counts from privately supplied loose files and archive payloads.
See [private validation](../docs/PRIVATE_DATA_VALIDATION.md) for the counting
rules and current evidence limits.

## Output contract

For each valid file, output begins with a `file` row and a column heading:

```text
kind    parent    name    offset    size
```

The actual separator is a tab. Rows retain archive-table order. `gob-entry`
identifies a GOB record; `lmp-entry` identifies a top-level or nested LMP record.
`parent` is empty for top-level entries and contains the GOB entry name for nested
entries. Offsets are **decimal bytes relative to the input file**, including
nested entries. Sizes are decimal bytes.

Backslash, tab, carriage return and newline are escaped as `\\`, `\t`, `\r` and
`\n`. Other control and non-ASCII bytes are escaped as lowercase `\xNN`; this
keeps the report deterministic and prevents a name from creating extra rows.
Archive names are preserved as bytes rather than guessed Unicode. File paths are
converted to UTF-8 before escaping.

GOB children ending in `.LMP` are inspected as nested lumps. Other GOB entries
are listed without assuming their payload format. Files are read in memory;
this first version is not a streaming reader. It performs no extraction or writes
to the input. It validates table/count/name/payload bounds; it does not certify
semantic validity of models, scripts or other entry contents.

Exit status: `0` for all files successfully inspected, `1` if any input fails,
`2` for invalid command usage. Errors go to standard error. A malformed file
produces no partial metadata for that file; other requested files are still
processed. Script metadata and actual script execution are deferred.

## Verification boundary

The synthetic C++ tests cover valid/empty archives, nested offsets, incomplete
headers/tables, extreme counts and offsets, unterminated names, fixed-width names,
zero-length entries, endianness and TSV escaping. CTest also runs CLI checks for
nested inputs, Unicode paths, malformed input, help and missing arguments. Test
fixtures contain only authored bytes.

The first local validation used Apple Clang 21 on arm64 macOS, with warnings as
errors and AddressSanitizer/UndefinedBehaviorSanitizer on parser tests: 30 checks
passed. CLI checks passed, and 100 metadata rows across 11 private local archives
matched the [research archive reader](https://github.com/Open-Champions/champions-return-to-arms-re/blob/main/tools/gob.py); no game contents were published. The resulting CLI was
verified as Mach-O arm64 with only system C++/OS dynamic dependencies. The later [CI run 38032707157](https://github.com/Open-Champions/champions-return-to-arms-re/actions/runs/38032707157)
built this inspector with CMake and passed all seven CTest cases on macOS arm64
and x64, Windows arm64 and x64, and Linux x64 at source revision `cf00c28`.
Each platform has a downloadable tool package and checksum manifest in that run.
These results establish the inspector milestone, not native gameplay.

See the [two-game roadmap](https://github.com/Open-Champions/open-champions-godot/blob/main/docs/ROADMAP.md) for remaining game-engine work.

The move into this shared client repository was checked again with Apple Clang
21 on arm64 macOS: 30 parser checks passed with AddressSanitizer and
UndefinedBehaviorSanitizer, and all seven CTest-equivalent command scenarios
passed. That initial check used direct compiler commands. The shared repository now has
its own [five-platform foundation CI result](https://github.com/Open-Champions/open-champions-godot/actions/runs/38037733460)
at revision `4c0b60e`: all nine CTest cases passed. These include the independent
`champions-texture` library and its 349 authored texture checks. The CLI remains
a metadata inspector; texture display belongs to the
[Godot browser](../docs/TEXTURE_PREVIEW.md).

## Tool packages

After building and testing, create a ZIP and SHA-256 sidecar with CMake’s
[CPack](https://cmake.org/cmake/help/latest/module/CPack.html):

```sh
cpack --config build/native/CPackConfig.cmake -C Release -B build/packages
```

Each package contains the inspector, this guide, the MIT license and build-info.txt.
CI records the source commit and compiler/target information. Local builds record
`unrecorded` unless configured with `-DCHAMPIONS_SOURCE_COMMIT=<revision>`. An archive
checksum checks integrity; it is not application signing or gameplay verification.
Windows packages use the standard MSVC runtime for their target architecture.
The CI runners already provide that runtime; their checks do not establish a
standalone installation on a clean Windows machine.

The package does not contain game data, test executables or a Godot application.
