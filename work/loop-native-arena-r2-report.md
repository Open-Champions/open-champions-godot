STATUS: BLOCKED — bounded arena prerequisites are ported; the requested Goblin engagement and player swing smoke cannot run in this native/Godot client without unported class and resource paths.

# Native arena match, round 2 — 2026-10-23

Base `d135ddc` (`codex/combat-ctors`, combat round-5 audit PASS); branch
`codex/native-arena`. Research pin `t3/research-arena-match` `5de93b5`,
`docs/spec/arena-match-probe.md` rounds 9–10 and PASS audits
`research-arena-r9.md` and `research-arena-r10.md`. The older local
`codex/arena-match` report was read first; it stopped before a real Player
replacement. Round-10 research adds the original post-intro and unarmed-hit
observations, but does not make the current native client a live match.

## Ported, with boundaries

- `rtaModelHull` reads all eight signed xyz points for a selected mesh from
  `.vif` header byte `+0x25` and its 24-byte entries. `World::bind_prop_model`
  validates every active prop mesh before copying caller-owned bytes into a
  World-owned fixture extent. Missing, truncated or out-of-range hulls refuse.
  The caller still has to load/bind the disc model; no asset is shipped.
- `World::begin_arena_intro` and `arena_intro_visit` preserve the mode-B
  180-positive-visit lock and visit-181 transition to `0xffff` with all three
  locks cleared. They stop at the observed intro domain; they are not a wave
  builder or the full task after the intro.
- `world_queries::terrain_raycast` follows the documented post-fixup WORLD
  sample march, flags, signed sample heights and clear-line `-1` answer.
  `combat_runtime::with_terrain` supplies ray, slide, height and blocked
  queries from one caller-owned Level. The existing `World::can_see` uses the
  ray callback; no Godot mode-B session calls this adapter yet.
- Synthetic tests cover hull binding and refusal, the 181 intro visits,
  clear sight service, and reference ray results including angled/sloped
  lines. The new C++ ray was not independently diffed against the original
  executable or a disc arena sight query in this round. These tests do **not**
  simulate or claim a Goblin fight.

## Remaining refusal

The Full combat World has no Goblin constructor or class run route:
`spawn_class("Goblin")` cannot create one. Its available Full fixture uses
authored Skelight/other actors, an authored player setup, synthetic clips and
flat terrain. `arena-goblin.md` explicitly limits its class comparison to a
neutral world with no target or animation events; holder, melee and
interacting branches remain outside that class model. The round-10 original
executable probe executes a natural Goblin hit and an unarmed player hit in a
bounded function-level fixture, but it does not supply this client's full
Goblin/holder/clip and player record construction, scheduler, input and Godot
mode-B wiring. An encounter smoke that hardcodes those outcomes would invent
behavior. The requested Goblin engagement/attack and player swing-damage
smoke is therefore **not implemented**. Natural kills, wave advance and a
full live match remain **unverified and refused**.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: PASS (arm64 Release).
- Affected arm64 CTest: `ctest --test-dir build/native -j 3
  --output-on-failure -R '^(model-parser|world-queries-host|world-queries-exact|combat-runtime-3-host|combat-runtime-3-exact)$'`:
  5/5 PASS. After the added ray vectors, direct world-query binary:
  389 checks PASS. Final full arm64 CTest,
  `ctest --test-dir build/native -j 3 --output-on-failure`: **214/214 PASS**
  in 938.08 seconds; `player-exact` passed in that aggregate run at 171.84
  seconds.
- Godot extension, creature-animation extension and level-texture extension
  Release `template_debug` builds: PASS. Each was configured with
  `-DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug` using
  `cmake -S gdextension -B build/godot-extension`,
  `cmake -S gdextension/creature_anim -B build/creature-godot`, and
  `cmake -S gdextension/level_texture -B build/godot-level-texture`,
  respectively, then `cmake --build` on each build directory with
  `--config Release --parallel 2`. `python3 work/native-arena/run-godot.py`
  ran the 45-command Godot dev matrix: 45/45 exit 0;
  the UI-layout smoke's intentionally wrong-arity `callv` logs one expected
  `ERROR` and then reports PASS, as it does on the base branch.
- `FULL_TRACE`: 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  unchanged from `d135ddc`. This existing fixture does not enter mode B.
- `python3 work/native-arena/run-sweep.py` ran the read-only private-disc
  sweep as four `--all --levels=0 --ticks=60 --scripts` slices with ranges
  `0/28`, `28/56`, `56/84`, `84/111`: **111 tried, 111 loaded, 111 played,
  zero failed checks, all four commands exited 0**. This is level-load/play
  regression evidence, not a mode-B match. Private inputs remain outside the
  checkout and this report records counts only.

No private bytes, extracted assets or proprietary names are tracked. No
process was killed, and there was no push, PR or issue action.

## Audit fixes

- **NA-R2-1:** Constructor-verified PhProp and GutChunk instances now retain
  their expected model pointer in the World. Binding refuses a different actor,
  table or pointer, validates all active hulls, reserves an exact World-owned
  fixture extent, checks its capacity, copies there, and only then publishes
  that address at `+0x4c`. The unowned constructor resource address is never
  a write destination. The fixture allocator preflights its existing 1 MiB
  address boundary before advancing. Oversized resources remain refused.
- **NA-R2-2:** The prop-run test now requires a normal return and checks the
  terrain height written after the hull loop in an authored stationary case.
  It separately checks missing/truncated hulls and rejects an unrelated actor
  and a tampered model pointer without changing the foreign destination. The
  prior broad `std::exception` catch hid a missing horizontal sweep handler;
  this stationary fixture does not request that unbound service.
- Focused arm64 Release CTest:
  `ctest --test-dir build/native -j 3 --output-on-failure -R '^(model-parser|world-queries-host|world-queries-exact|combat-runtime-3-host|combat-runtime-3-exact)$'`:
  **5/5 PASS** after `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`
  and `cmake --build build/native -j 4`. Full arm64 Release CTest:
  `ctest --test-dir build/native -j 3 --output-on-failure`:
  **214/214 PASS** in 828.61 seconds, including `player-exact` and the
  affected prop tests.
- Reconfigured and rebuilt `gdextension`, `gdextension/creature_anim`, and
  `gdextension/level_texture` with Release `template_debug` and the pinned
  `GODOT_CPP_DIR` above: all three builds PASS. `python3
  work/native-arena/run-godot.py`: **45/45 commands exited 0**. The
  `combined-full-combat` `FULL_TRACE` remains 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
  This fixture still does not enter mode B.
- `python3 work/native-arena/run-sweep.py` (read-only private input): four
  slices exited 0, **111 tried / 111 loaded / 111 played**, 119 passed checks,
  zero failures and zero script faults. This is a level regression sweep,
  not a native arena encounter. No private bytes or decoded content were
  added to tracked files.
