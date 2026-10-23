STATUS: DONE — shared World authority implemented and all gates pass (player-exact needed an extended timeout under load).

# World authority round 1 — 2026-10-23

Branch `codex/world-authority`, based on `84c12fb`. Local commits: `e870a70`, `5b66c0f`, `3b075a2`.

## Change

`WorldAuthority` owns the one native memory image and the `objects::World` pool, handle nodes, and list/grid cells. It indexes live objects by address. The production `script_commands::Host` uses a byte view of that memory and the same object World; its level factory marks only built, ported objects for combat lookup. `combat_runtime::World` uses references to the authority's pool, handles, memory, and cells. `SessionAdapter::factory_object` resolves a retained factory handle to the script-host object. Host teardown invalidates retained handles before the object is released.

The ordinary play-session controller owns this authority, a combat World, and its adapter. They are prepared for later service binding; this change does not run real-level combat, change the DryCreature movement policy, or supply Player, creature, terrain, camera, animation, or resource behavior. The Full synthetic fixture constructs its own authority and retains its existing callbacks and scenarios.

## Verification

- Focused arm64 Release CTest: `ctest --test-dir build/native --output-on-failure -j 2 -R '^(level-objects|combat-runtime-host|combat-play)$'` — 3/3 PASS. The level-objects case uses a ported synthetic factory record to prove adapter handle identity, shared memory reads/writes, shared handle-pool use, and stale-handle invalidation.
- Full synthetic fixture: `ctest --test-dir build/native --output-on-failure -j 1 -R '^(playable-combat-fx|fixture-save)$'` — 2/2 PASS.
- Arm64 Release native build: PASS. Aggregate CTest (`-j 3`, player-exact and level-flow-exact excluded for serial runs): 234/234 PASS. `level-flow-exact` alone: PASS (0.20 s). `player-exact` alone: timed out twice at the 240 s limit on the heavily loaded machine (no failure output), then PASSED in 402.75 s with an extended timeout; this change does not touch Player movement.
- Strengthened `level-objects` test: `ctest --test-dir build/native --output-on-failure -j 1 -R '^level-objects$'` — 1/1 PASS after rebuilding the test. It also forces reuse of the same pool address and checks that the retired handle does not resolve to the replacement generation.
- Godot development matrix: 54/54 exits 0 after building the arm64 `gdextension`, `level_texture`, and `creature_anim` libraries.
- Full fixture `FULL_TRACE`: 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, matching the requested baseline in the final matrix run. `FULL_SAVE_LOAD` remained 300/300 presentations.
- Read-only 111-level sweep: scripts on 111/111 loaded and played with 230 checks and zero failures; scripts off 111/111 loaded and played with 230 checks and zero failures. The selected disc had 994 files before and after testing.

No game values or private data were added. No push or PR.

## Audit fixes

STATUS: DONE — audit `world-authority-r1` WA-1 (MEDIUM) and WA-2..WA-5 (LOW) addressed; every gate below was rerun fresh on this tree.

- **WA-1 (script-host memory semantics).** The shared `creature_hp::Memory` keeps its 1 MiB budget, `ResourceLimit`/`Error` types and read-only overlay for combat. The script host's view no longer inherits them. New `creature_hp::Memory::write_host_bytes` is the original script store: no sparse budget, no overlay refusal (the overlay still wins reads), zero bytes erase their entry, unlogged. `script_builtins::Memory` over the authority now uses it, wraps spans at 0xffffffff to address 0 as the paged store did (reads and writes), maps any `creature_hp::Error` to `amx::HostFault`, and returns only non-zero bytes from `nonzero_bytes()`. `poke`/`write_bytes` budget checks no longer underflow if the sparse map is already past the combat budget. `Memory::page()` still throws `HostFault` on the shared view (no mutable page alias); no caller exists. No game values were added; no new limit was invented.
  Test: `script_commands_tests.cpp` `test_authority_equivalence` runs the same script commands and the same raw memory operations through `Host()` and `Host(authority)` and requires identical traces (run results, quest value, seed, fault and refusal counts including the refused `getRand` path, 1.5 MiB of distinct bytes, 2 MiB of zero writes, the 0xffffffff wrap, `cstr` across it, write log length). It fails against the previous implementation.
- **WA-2.** `level_objects_tests.cpp` now keeps a retained handle linked while the address is re-registered to a different `Object` with the factory mark kept, and asserts `factory_object` returns null, then restores the registration and asserts it resolves again. It also asserts a script write to a combat-overlay byte is not refused.
- **WA-3.** `State::link` now uses a factory object's own `heads[0]` before any cached combat head at that address. Test: a combat Skelight is spawned and destroyed, a factory object reuses its block (asserted same address), and the retained handle resolves to the factory object.
- **WA-4.** `Variables::new_object` registers with the authority before touching the world, name table or `setup_objects`, and unregisters in the existing failure path, so an id collision leaves nothing behind.
- **WA-5 (verification).** Reran, not read from logs, on arm64 Release (`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release; cmake --build build/native -j 4`): focused `ctest -R 'level-objects|combat-runtime-host|combat-play|script-commands|script-builtins|creature-hp'` 12/12 PASS; full `ctest -j 3` 236/236 PASS in 659 s with no timeouts (`player-exact` 116.80 s under that load). Rebuilt the `gdextension` and ran `godot/tests/playable_wave_smoke_full_combat.gd` headless and drawn: both exit 0, `FULL_TRACE` 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` (unchanged). Not done: a `player-exact` timing at `84c12fb` for comparison, so the earlier 402.75 s run is still unexplained beyond machine load; the 116.80 s figure shows no slowdown at this load.
