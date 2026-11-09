STATUS: IN PROGRESS — play-levelup-hud round 1 (2026-11-08).

# play-levelup-hud round 1

## Gates (mini, synced copy)

- Native Release configure/build `-j 4`: **PASS**, exit 0 (`work/r1/build.log`). GDExtensions: **PASS**, exit 0; archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`, all `lipo -archs` = `x86_64 arm64` (`work/r1/gdext.log`).
- Focused CTest (`levelup-hud|menus|ui-layout|strings|hud`): **PASS 7/7** (`work/r1/focused-ctest.log`). New `levelup-hud` test: 117 synthetic checks; `menus-exact` 317 checks including `observed_level_vectors`.
- Full CTest `-j 3`: **PASS 260/260**, 848.27 s, exit 0 (`work/r1/ctest.out`). Wave 37 was 259; the added test is `levelup-hud`.
