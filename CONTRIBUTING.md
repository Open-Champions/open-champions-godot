# Contributing

Open an issue before a large change and state which game, format and files you
will cover. Coordinate with active research and reconstruction work in the two
source repositories. Small contributions with bounded evidence are easiest to
review.

## Authored code and private inputs

Contribute original implementations, documented structures, address references,
synthetic test data and explanations in your own words. Preserve applicable
licenses and attribution when importing authored code.

Never commit retail executables, discs, BIOS files, unpacked game assets, raw
disassembly, copied decompiler output or proprietary toolchains. Keep game data
outside the checkout and select it explicitly at runtime. Public tests and
packages must work without shipping any original game content.

## State what is established

Separate observed behavior, inference and unknowns. Record the game, revision,
platform and method for a private validation without publishing its contents.
Return to Arms evidence does not establish Norrath compatibility. Parser acceptance
alone does not establish that a model, save or script is semantically correct.

Keep local UI/render/audio state separate from authoritative game state. Define
fixed-width fields, numeric behavior, tick order and random streams explicitly.
Game-specific rules belong to explicit profiles, not scattered title checks.

## Verify changes

Use the build and CTest commands in README.md. Seven registered CTest cases cover
the parser, fixture creation and CLI behavior; the parser case includes 30 checks.
Add targeted synthetic cases for new formats and concrete regressions. Keep the
archive core free of Godot dependencies so these checks run without an editor.

Visual features require exported-app inspection on the target platform. Native
multiplayer requires comparable state traces and divergent-frame diagnostics.
Neither a fixed update rate nor a successful Godot export proves deterministic
simulation or original-client network compatibility.

Describe the problem, resulting behavior, evidence and remaining limits in the
pull request. Do not claim tested platforms or supported game revisions without
execution evidence.
