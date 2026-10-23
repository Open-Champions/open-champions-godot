STATUS: BLOCKED — ordinary play still uses DryCreature; no unified real-level Player World was shipped.

# Unify World round 1 — 2026-10-23

Branch `codex/unify-world` at the requested `1cfddf0` base. This round contains the play-enemies r6 acceptance-gate fix below. The requested unified gameplay implementation was not shipped or claimed.

## Audit fixes (play-enemies r6)

- PE6-1 (`cursor-runs/audits/play-enemies-r6.md`, VERDICT: FIX): `godot/tests/loop_playthrough.gd` now marks `character_created` refused and exits 1 immediately when the created record is not class 6/model 11. The stop precedes both the normal and visual-only success paths. The refusal says “character creation expected class 6 and model 11.” No captured item or damage value was assigned.
- A temporary, uncommitted probe copy forced the predicate false. Both ordinary and `--visual-only` headless runs exited 1 after seven steps; the last row was `character_created: refused`, with one clear mismatch refusal. The temporary copy was removed after the checks.

## Blocking boundary

- Ordinary play constructs `native_save::Scene` with `MovementPolicy::DryCreature` in `gdextension/src/session_controller.cpp` at the `start_play_session` ordinary branch. It pairs only the shared seed with the level script host. The script host allocates factory records in `Variables::world`, an `objects::World` with its own pool, memory, handles, and object graph. The combat Full fixture separately constructs a `combat_runtime::World` and `SessionAdapter`. Reparenting either pointer would not make one actor graph or one damage/target grid.
- The Full fixture accepts only flat, dry synthetic terrain and installs `flat_fixture_services`, authored camera planes, clip timing, zero root motion, empty resources, and no loot. `combat_runtime::World::player_move` requires camera yaw, animation advance/root, the screen frustum, and terrain/water callbacks. The code explicitly refuses absent callbacks. Using the Full fixture callbacks for a selected real level would invent the player's movement/animation and hide missing production services.
- The level factory still refuses the WarBoar and Minotaur actor paths after named-resource resolution. Those refusals remain in scope and untouched for the parallel job.
- Research `t3/research-campaign-combat` pin `328b072`, `docs/issues/campaign-combat-handoff.md` round 4, establishes class/model-dependent starting grant calls but says the parser, inventory grant/equip outcomes and active weapon on this route are Unknown. No captured club or weapon statistics were copied. The equipped slot remains empty in the current ordinary route; it cannot yet be described as a granted and equipped record.
- New `t3/research-player-anim` pin `4ad5721`, `docs/spec/player-animation-selection.md` section 4.1, identifies weapon animation class at player record `R+0x4a86` and action state at actor `P+0x58c`. Both must be read from the eventual production actor/record, alongside speed/blend and item. The r2 audit returned VERDICT: FIX for unsupported precise instruction-site claims in the class/equip handoff and an imprecise cast transition; its field offsets were traced OK. No ordinary-play actor exists to observe either field from, so both remain unexposed.

## Required implementation evidence

The next integration needs one explicit production `World` memory/pool/handle authority shared by `script_commands::Host` and `combat_runtime::SessionAdapter`, with an object-factory registration path for the already ported object classes. It also needs a real-level Player service bundle for animation start/advance/root and the per-machine camera frustum/yaw, plus the documented terrain/water callbacks. Only then can the player record be bound to the World actor and its position, speed/blend and equipped item observed from that actor. The fixture's authored callbacks and equipment must remain fixture-only.

## Verification

Executed: `git diff --check` PASS. `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR=.../integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug` and `cmake --build build/godot-extension --parallel 4` PASS. The same configure/build for `gdextension/level_texture` PASS. A Godot 4.5.2 headless editor import PASS after both libraries were built. An earlier import attempt failed because the level-texture library was absent; an earlier probe invocation refused a relative output path. Those setup errors were corrected before the recorded probes.

`python3 tools/run_loop_playthrough.py --godot .../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder .../private-validation/rta-disc --out work/loop-unify-world-r1/playthrough` completed headless and rendered. Each wrote 24 steps with the created class 6/model 11; the rendered run wrote 11 screenshots. Both exited 1 because the existing combat journey remains incomplete. I inspected the rendered fight-attempt screenshot: player and terrain appeared, with no visible enemy. The read-only disc was 994 files before and after. The two temporary forced-mismatch Godot commands also used this disc read-only and exited 1 as described above.

`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4` PASS on arm64 Release. Full CTest (including serial `player-exact`), Godot dev matrix, 111-level sweep and FULL_TRACE were not run because the gameplay integration is blocked and the only shipped change is a playthrough exit gate. No push or PR.
