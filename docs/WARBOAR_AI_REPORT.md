STATUS: MORE

Branch `codex/play-boar-ai`, base `252f34a6`; research pin `3f81c89` and its
PASS audit. Implementation and synthetic fixtures are authored C++17, with
no game bytes, decoded content or extracted assets added. No push.
Implementation commit: `cf40bb01`; Clang fixture-build adjustment: `224a6c2e`.

## Result

The shared-World boars now use force-0, pointblank-first sight, the inherited
acquisition and 480-frame home leash, constructor deadline zero versus idle
entry clock+180, wander/chase, own-LCG attack selection, native animation
events, continuation, hit-reaction rejection/recovery and cooldown retreat.
The class hit callback uses the existing multi-victim combat dispatch.

Factory adoption now shares the original object's three handle heads with
the combat view; root movement reuses its grid node. The campaign host uses
the current player's visibility positions. No enemy state is forced to make
the approach check succeed.

The native synthetic class fixtures pass **1,038 cases across seeds 20973/7**.
The optional pinned original-code check passes **543 cases per seed** (the
research's 530 plus 12 complete entry signatures and the finished-animation
continuation case). Shared-World fixtures exercise radius/angle/ray boundaries,
the sight wrapper, the 480-frame leash boundary, filter precedence, grid-node
identity, live-player activity and multi-victim damage/refusal.

## Commands and gates

Run from this worktree. Build and private acceptance logs stay ignored under
`work/boar-ai/`. The game folder is read-only.

| Command / gate | Result |
|---|---|
| `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` | PASS |
| `cmake --build build/native -j 4` | PASS; see Clang fixture-driver adjustment below |
| `ctest --test-dir build/native --output-on-failure -j 3` | 240/241 PASS in 3,136.70 s; only `player-exact` timed out at 240.07 s |
| `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'` | FAIL: timeout again at 240.04 s; no state mismatch reported |
| Unchanged `tools/test_player_vectors.py` invoked directly | PASS: 274,304 exact complete-state cases and transport refusals in 319.27 s; the 240 s CTest gate remains failed |
| Godot 4.5.2 development matrix, `python3 work/boar-ai/run-godot.py` | PASS, 54/54 after the display-label correction |
| 111-level sweep, `bash work/boar-ai/run-sweep.sh` | PASS, 111 tried/loaded/played; 230 checks; 0 failures/refusal kinds; 111 scripts attached, 76 started |
| `FULL_TRACE`, 36,402 bytes, expected `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` | PASS, unchanged; `FULL_SAVE_LOAD` 300/300 |
| `tools/run_loop_playthrough.py` headless and rendered | Both exit 1 at existing ground-loot/pickup/Gold boundaries; 24 steps each, 13 rendered screenshots; disc 994 before/after |
| Counts-only approach, 600 ticks, diagnostic player placement | PASS: 78 boars, 3 wandered/moved, 2 chased/attacked, 13 attack entries, 14 hit events, 0 World failures |

Additional builds: `gdextension` (Release/template_debug),
`gdextension/level_texture` (Release/template_debug), and
`gdextension/creature_anim` target `champions-creature-animation`, with
`GODOT_CPP_DIR` set to the pinned checkout in `worktrees/integration/work/deps/godot-cpp`.

Research check:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/check_warboar_ai_research.py \
  --research /Users/syndicatemike/Projects/champions-re-wt/research-boar-ai \
  --executable /Users/syndicatemike/Projects/open-champions/private-validation/rta-disc/SLUS_209.73
```

Counts-only native check:

```sh
build/native/champions-campaign-fight-probe \
  --game /Users/syndicatemike/Projects/open-champions/private-validation/rta-disc \
  --level valor --ticks 600 --ai-check
```

Playthrough:

```sh
python3 tools/run_loop_playthrough.py \
  --godot /Users/syndicatemike/Projects/open-champions/worktrees/integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --game-folder /Users/syndicatemike/Projects/open-champions/private-validation/rta-disc \
  --out work/boar-ai/playthrough-label
```

Both final playthrough variants record 10,945 fight ticks, 956 boar state
entries, 464 attack entries, 386 hit-event callbacks and one enemy death.
Their fight counters match exactly. The remaining journey steps are ground
item spawning/pickup and the separate container/Gold source and credit path.
Rendered inspection confirmed the corrected AI label, the drawn boars at
native positions, and the diagnostic-placement label. The boar pose remains
the existing rest-pose presentation.

## Changed files

- AI: `campaign_actors.{hpp,cpp}`, new `warboar_ai.cpp`,
  `simulation_targets.cmake`.
- Shared World and client: `campaign_fight.{hpp,cpp}`,
  `campaign_fight_probe.cpp`, `combat_runtime.hpp`,
  `combat_runtime_{adopt,damage,services}.cpp`, `player_services.{hpp,cpp}`,
  `gdextension/src/session_controller.cpp`, `godot/scenes/real_data_play.gd`.
- Fixtures/evidence: `campaign_actors_tests.cpp`,
  `campaign_fight_adopt_tests.cpp`, `tools/check_warboar_ai_research.py`.
- Documentation: `docs/ROADMAP.md`, `docs/WARBOAR_AI.md`, this report.

The first Release build's unchanged `level_flow_tests.cpp` was still in LLVM
full-unrolling after 15 minutes; a sample then identified expensive scalar
replacement in the fixture's inlined container code. In
`native/level_flow_targets.cmake`, only the Clang test driver now adds
`-fno-unroll-loops -fno-inline-functions`. All simulation-library flags and
Release optimization remain unchanged. The subsequent full build exited 0
in 250.60 seconds. Only the compiler PIDs started by this task were terminated.
The rebuilt level-flow driver also passes all 10,362 checks.

The direct player command was:

```sh
/usr/bin/time -p /opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 \
  tools/test_player_vectors.py --native build/native/champions-player-trace \
  --vectors native/tests/vectors/player.tsv.gz
```

It used 164.74 user and 5.43 system CPU seconds over 319.27 elapsed seconds;
other worktrees were running tests concurrently. Sampling showed the native
trace completed and the Python comparison was still running when elapsed time
passed 240 seconds. Neither the checker, frozen corpus nor timeout was changed.
All 241 test bodies therefore have passing observations, but the prescribed
CTest gate did not pass its time limit. Validation here is macOS arm64 only.

## Remaining refusals

Normal-route outgoing player damage is refused before shared RNG or HP
writes: the route's outgoing fields and victim equipment are not established.
Exact live event spacing/root trajectories and script/event-driven group
activation remain diagnosed Unknowns. Runtime path searches refuse the affected
move; no guessed path is returned. Creature-init model bounds/height, wet
services, death animation, rank/tint loot, ground item objects/pickups and the
existing player equipment/camera limits remain as documented. The Godot boar
view still renders a rest pose at the native position and facing.

See [WARBOAR_AI.md](WARBOAR_AI.md) for the contracts and evidence boundaries.
