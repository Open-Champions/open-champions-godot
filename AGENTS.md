# Working on Open Champions

This repository owns the portable C++ foundation and planned Godot client for
Champions of Norrath and Champions: Return to Arms. Read CONTRIBUTING.md and
docs/ROADMAP.md. The PS2 source reconstruction and game-specific research live
in the sibling repositories linked from README.md.

- Inspect git status and active agent ownership before editing. Assign separate
  files to concurrent agents and preserve unrelated changes. Stage only files
  you own; never reset, stash or clean another agent's work.
- Keep game-independent parsing and deterministic behavior independent of Godot.
  Put future game differences behind explicit profiles; do not silently apply
  Return to Arms constants or assumptions to Norrath.
- Do not import a sibling's generated decompiler output, retail bytes, extracted
  assets, game/BIOS images or proprietary toolchains. Author implementations from
  documented contracts and use synthetic fixtures. Keep private validation inputs
  outside the repository.
- Native tools, decoded assets, Godot exports, playable scenes and deterministic
  multiplayer each need their own evidence. Do not describe a successful parser
  build or a blank scene as a playable port.
- Run cmake -S native -B build/native, build, and CTest for affected native changes.
  Use direct compiler tests if CMake is unavailable and state the limitation;
  never claim unexecuted target-platform tests passed.
- For future visual work, inspect the real exported application and interactions.
  For deterministic changes, compare state traces across arm64 and x64.
- Keep scratch under ignored work/ and build output under build/. No additional
  dependencies or Godot scaffold are needed for the current inspector milestone.
