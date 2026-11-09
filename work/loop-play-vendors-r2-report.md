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
3. Full CTest `-j 3`, standard input closed: **PASS 260/260**, 845.38 s,
   exit 0. Wave 37 was 259; the added test is `vendor-stock-exact`.
   Evidence `ctest.out`.
4. Headless editor import first (`--import`, exit 0, `editor-import.log`),
   because the mini copy had no `godot/.godot` (see "Incident" below).
   Strict Godot dev matrix, unchanged `tools/godot_matrix.py matrix
   --captures --timeout 1200`: **PASS 54/54**, exit 0. Evidence
   `matrix/results.json`, `matrix-run.log`.
5. FULL_SAVE_LOAD: **PASS 300/300**; continued and uninterrupted presentation
   SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`,
   identical. Full synthetic combat 2,645 checks, 0 failures.
6. FULL_TRACE: **PASS, unchanged**, 36,402 bytes, SHA-256
   `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
   Evidence `matrix/playable_wave_smoke_full_combat.log`.
