STATUS: IN PROGRESS — gates being recorded (2026-11-08).

# Play vendors round 2 — session vendor stock

Branch `codex/play-vendors-r2` from wave 37 `70d2de19`. Source: PASS-audited
`research-vendor-stock` at `fdf6752` (`docs/spec/vendor-stock.md`,
`tools/ref_vendor_stock.py`). Builds the earlier vendor round
(`codex/play-vendors` `061eadae..9c6e23d8`, already in wave 37).

## Gates (mini, `~/malik-champions/open-champions/worktrees/play-vendors-r2`)

Evidence in ignored `work/vendors-r2/`.

1. Release native build, `--clean-first -j 4`: **PASS**, exit 0, 113 s.
   GDExtensions `mac-template_debug`, `mac-template_release`,
   `level-texture-template_debug`, `level-texture-template_release` and
   `creature-template_debug`: **PASS**, every library `x86_64 arm64`.
   Evidence `build.log`, `*-build.log`, `build-status.txt`.
2. Focused CTest (`vendor-stock-exact`, `items-exact`, `messages-exact`,
   `messages-api`, `menus-exact`, `level-flow-exact`, `script-commands`,
   `script-items`), `-j 3`: **PASS 8/8**. Evidence `ctest-focused.out`.
