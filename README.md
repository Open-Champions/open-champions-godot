# Open Champions

<p align="center">
  <img src="https://raw.githubusercontent.com/Open-Champions/.github/main/profile/assets/champions-of-norrath-logo.png" alt="Champions of Norrath" width="260">
  <img src="https://raw.githubusercontent.com/Open-Champions/.github/main/profile/assets/champions-return-to-arms-logo.png" alt="Champions: Return to Arms" width="260">
</p>

The hand-written native rebuild of **Champions: Return to Arms** and, later,
**Champions of Norrath**: portable C++ libraries and a Godot 4 client, written by hand
and checked function by function against the original game's behaviour as documented
in the [research repository](https://github.com/Open-Champions/champions-return-to-arms-re).
It holds no game data: every test runs on synthetic files, and anything read from a
game comes from the player's own copy and stays on their machine.

## Direction

- **One portal, two games:** *Classic* (the original game, faithful, plus the sharp art pack
  and a steady 60 fps) and *Remastered* (Godot, 60+ fps, new HUD and lighting, five views,
  aim where you look, Hardcore combat, the Elden Ring look from your own copy). Rooms are one or the other.
- **Online play is exact:** every gameplay change is lockstep-safe, a room option all copies
  agree on, tested in multi-copy rooms; with a mode off the game is byte-identical to the original.
- **Champions of Norrath** joins later as more campaign content; each player brings their own discs.
- **No game data, disc images, BIOS, Elden Ring data or personal information in any repository;**
  everything from a game stays on the player's machine.
- This repository is the **long-term clean-code line**, aiming at the same Classic/Remastered
  product, the same 60+ fps bar and the same lockstep rules as the playable line.

The whole direction: [docs/DIRECTION.md](docs/DIRECTION.md).

## Play

The game you can play today is the translated native game and its Godot host in the
research repository: **[how to get it and play](https://github.com/Open-Champions/champions-return-to-arms-re/blob/main/docs/DOWNLOADS.md)**
(the *Champions Setup* builds it on your computer from your own disc).

This repository's own client is not a full game yet. Its workflow exports development
packages (macOS, Windows, Linux) of the Godot test client with the archive browser;
see the [browser build guide](docs/GODOT_BROWSER.md). Where the rebuild stands and what
comes next: [docs/ROADMAP.md](docs/ROADMAP.md) and [docs/NATIVE_PORT.md](docs/NATIVE_PORT.md).

## Test

None of these needs game files. The [native](.github/workflows/native.yml) and
[Godot](.github/workflows/godot.yml) workflows run the same steps on macOS, Windows and Linux.

```sh
# portable libraries and their exact-trace tests (C++17, CMake 3.21+)
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure

# Python tools and packaging checks
python3 -m unittest discover -s tools -p 'test_*.py'
```

The Godot extensions and their headless smoke tests need
Godot 4.5.2 and the pinned godot-cpp: the exact commands are in the
[Godot workflow](.github/workflows/godot.yml) and the [browser build guide](docs/GODOT_BROWSER.md).
Checks against your own game files are described in
[docs/PRIVATE_DATA_VALIDATION.md](docs/PRIVATE_DATA_VALIDATION.md); their inputs and
outputs stay outside the repository.

## What is here

| Path | What |
|---|---|
| `native/` | portable C++17 libraries (archives, models, textures, Session, combat, items, network wire, ...) and their tests; the `champions-data` inspector ([guide](native/README.md)) |
| `gdextension/` | the Godot bindings of the native libraries |
| `godot/` | the Godot 4.5 client project and its headless smoke tests |
| `tools/` | Python tools: trace comparison, packaging, vector generators |
| `docs/` | one guide per library or system, with its evidence and limits |

## The other repository

[champions-return-to-arms-re](https://github.com/Open-Champions/champions-return-to-arms-re)
holds the research (symbols, formats, how each system works, the online-bug evidence) and
the playable line: the game's own code translated to C++ on each player's machine, hosted in
Godot with the portal. This repository ports from that research and aims at the same product;
when it plays a mode as well as the translated line, it takes that mode over.

Related: [Return to Arms source reconstruction](https://github.com/Open-Champions/champions-return-to-arms)
(byte-matching PS2 reconstruction) and [Champions of Norrath](https://github.com/Open-Champions/champions-of-norrath)
(separate baseline, not yet established). Contributing: [CONTRIBUTING.md](CONTRIBUTING.md);
source origin: [docs/ORIGIN.md](docs/ORIGIN.md).

## Legal

This is an independent community project. Game names identify compatibility
targets; no affiliation or official endorsement is implied. Original game files,
extracted assets and proprietary code are not included. The identifying logos
above retain their owners’ rights and are excluded from the MIT license; see
[artwork credits](https://github.com/Open-Champions/.github/blob/main/ARTWORK.md).
Authored source is available under the [MIT license](LICENSE).
