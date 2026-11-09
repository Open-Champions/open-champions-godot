STATUS: IN PROGRESS — play-levelup-hud round 1 (2026-11-08).

# play-levelup-hud round 1

## Gates (mini, synced copy)

- Native Release configure/build `-j 4`: **PASS**, exit 0 (`work/r1/build.log`). GDExtensions: **PASS**, exit 0; archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`, all `lipo -archs` = `x86_64 arm64` (`work/r1/gdext.log`).
- Focused CTest (`levelup-hud|menus|ui-layout|strings|hud`): **PASS 7/7** (`work/r1/focused-ctest.log`). New `levelup-hud` test: 117 synthetic checks; `menus-exact` 317 checks including `observed_level_vectors`.
- Full CTest `-j 3`: **PASS 260/260**, 848.27 s, exit 0 (`work/r1/ctest.out`). Wave 37 was 259; the added test is `levelup-hud`.
- Strict Godot matrix `--captures --timeout 1200`: **PASS 54/54**, exit 0 (`work/r1/matrix/results.json`), checker 0 diagnostics. `menu_smoke` 94 checks, 0 failures. A first run (`work/r1/matrix-try1*`) failed 9 `playable_wave_smoke_*` cases: the round's HUD-view change drew zero-maximum bars, but the play scene uses a zero maximum to mean "value not available" (`unavailable play mana omitted`), and those draws produced negative `Rect2` sizes. That view change was reverted (`godot/ui/original_art/hud_view.gd` back to wave 37). The native `barClipRight` saturation stays. No test or checker was changed.
- Full synthetic combat: **2,645 checks, 0 failures** (`work/r1/matrix/playable_wave_smoke_full_combat.log`).
