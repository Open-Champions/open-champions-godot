STATUS: COMPLETE

# codex/script-commands-2: Grok audit fix, round 1

Worktree `~/Projects/open-champions/worktrees/script-commands-2`, branch `codex/script-commands-2`.
The branch is rebased onto `codex/playable-integration` c42ad2d (wave 17) and fixed. It is not merged.
All commits are local: nothing was pushed and no PR was opened. `~/champions-re` was not touched.
The disc copy was only read; its counts and logs stay in the gitignored `work/private/`.
The pre-rebase tip is kept as `backup/script-commands-2-8494cc6`.

Every Grok finding (SC-10..18) was re-checked against the research before it was fixed. All nine are real defects; none is "not a defect".
The two inherited LOW findings that `codex/audit-fixes` had **not** fixed (SC-8, SC-9) are fixed here too. The brief said audit-fixes covered SC-1..9; its report and the code show only SC-1..7.
One more defect of the AUD-1 kind turned up during the re-check and is fixed.

## Order of work

I rebased first, then fixed. 8494cc6 is one commit on top of 2088ab1, and c42ad2d rewrote the same host for SC-1..7. Fixing on the old base would have meant resolving every conflict twice.

- 4dcb153 is 8494cc6 replayed on c42ad2d. Conflicts were resolved by keeping the wave-17 audit structure and adding 8494cc6's routes on top:
  - `run_prestart` / `adopt_objects` / `run_start`;
  - stub refusals;
  - a used refused answer stops the entry;
  - `setCFlag` is refused on stubs.

  `creature_startBossMusic` now notes the campaign and then reaches the host's scene. Its test was ported to `run_prestart`/`run_start`.
- The fixes follow in separate commits (see the Commits section).

Wave 18 had not landed when I finished. `codex/playable-integration` is at b5dab4c, with its merges in (perf, combat-runtime-6, water-ripples, save-import, exports, EXP-1..3). It has no "Record verified wave-18" commit, and that worktree has uncommitted doc edits. So the base stays c42ad2d. `git merge-tree --write-tree HEAD codex/playable-integration` merges cleanly against b5dab4c.

## Findings: re-check, then fix

Research: `t3/research-script` 52984db (`docs/script-effects.md`, `tools/ref_script_effects.py`, `tools/difftest_script_effects.py`), plus 9649e6b for the composition table. `research-quests` 7efd3ce is unchanged.

| ID | Re-check (evidence) | Fix |
|---|---|---|
| SC-10 HIGH | `0x4e1000` occurs nowhere in 52984db. The only mode-1 camera is the difftest's allocated fixture (`difftest_script_effects.py:131-133`). | Camera deleted. No routed command reads `g_localCamera`: `showBust` and `showCompass` stay refused (SC-12). The test checks that 0x4f8450 stays 0. |
| SC-11 HIGH | `scratch_ = 0x700000` up to `0xf00000` covers `QUEST_VARS` 0x86da80, `DEFER_*`, `POST_ACTION` 0x86fa80 and the `G_*` HUD buffers. `live-rig.md` puts heaps at 0x6f0000-0x7b0000. | Scratch, `intern_bytes`/`intern_wide`, `STACK_WIDE` and `note_type` are deleted. Nothing host-made is written into binder memory. The `startMusic` title is a host handle that never enters binder memory. `+0x34` is left as is (0 unless a test plants a type name). |
| SC-12 MED | `text_lookup` reads the game's string table (`docs/data-tables.md:538`, `engine.md:297`). No port exists in the repo. | `setInfo`, `addHelpMessage`, `addHelpMessageF`, `showBust` and `showCompass` are no longer routed, so they are counted refusals. The widened-key text and its test assertion are removed. |
| SC-13 MED | `wstr_format` 0x19ea78 was not entered. `script-builtins.md:532` uses two buffers. | `addHelpMessageF` is refused. `decimal_of` and `format_first_integer` are deleted. |
| SC-14 MED | `script-effects.md`: `sound_playOnObject` is **Unknown**. The difftest stubs it with 0x8001. | The `resource`/`sound_handle`/`stream_play_result`/`boss_music_result` defaults are gone. An unanswered callee throws, unless the host lists it as discarded. Discarded means no handler reads the answer: `handle_clear`, `ui_drawRect`, `audio_setChannelVolume`, `stream_playLevel`. The vector replay uses `Trace.call`'s default of 0. The help path is unreachable from the host (SC-12). |
| SC-15 MED | `script-effects.md` charmMonster: "**Unknown:** `handle_clear` of a live handle". | Before the binder runs, `charmMonster`/`followPlayer` are refused when a found creature past setCharm's gates has a live `+0x234`. They store nothing and count in `callee_refusals`. The scene also throws if that state is ever reached. |
| SC-16 LOW | `ref_script_effects.COMMANDS` lists `setCFlag`/`selectSetCFlag` as `partial`. `bare_class` is in no source. | `bare_class` is deleted, and the tests plant the bare class name. The route is kept and labelled. `Counts::inferred[]` counts bit-0 calls (stop and queue are research-audio 6b08886). Docs and counts say so. |
| SC-17 LOW | The difftest stubs `wstr_copy`/`strcpy`. The 4096/160 limits are in no source. | The copy bodies and their limits are deleted. `Effects` records those calls only. |
| SC-18 LOW | The generator imported the working tree; there was no pin and no manifest. | It reads `ref_script_effects`, `ref_natives`, `ref_core`, `ref_objects`, `ref_amx` and `docs/script-effects.md` with `git show 52984db:`, checks sha256 pins, and writes `script_effects.json`. Regenerated: the same 36 cases with identical stores, returns and calls. Only the header and the stub-line format changed. |
| SC-9 LOW (inherited) | `generate_script_commands_vectors.py` still had the unused `head` and imported the checkout. | Same `git show 9649e6b:` plus pins, including `docs/script.md`, which compose reads. The TSV and `.inc` are byte-identical; the manifest gains the pins. |
| SC-8 LOW (inherited) | `queue_key` ignored the variables. `script.md` 2.4: a wait "counts in a global variable and defers itself again". | The key adds the script's Pawn globals (data below `hea0`), the quest variables and the post-action name. The `+2` limit is labelled a measurement cap in the code, the header and the doc (both measure tools). The "polling loop" claim is removed. |
| SC-1..7 (inherited) | Present on c42ad2d from audit-fixes. | Kept unchanged. Their tests (`test_prestart_before_objects`, `test_refused_result_stops_entry`, `test_stub_objects`, the 8 host vectors) pass. |

### Found during the re-check: AUD-1 re-entered through the host

8494cc6 called `Playback::set_music_volume(memory.u32(G_MUSIC_VOLUME))`. That word is the binder's zero-initialised 0x4f8944, so the host supplied an invented volume and bypassed AUD-1's "no default". The `music_playLevel` gate also read that zero, and the duck scalar 0x4f894c, also zero.

Fix:
- `Host::set_music_volume(bits)` comes from the caller. Until it is set, `startMusic` is refused, and so is a bit-0 `setCFlag`/`selectSetCFlag` whose creature names a boss track (`callee_refusals`).
- With a volume, the gate and the channel word use Playback's volume and `duck_current()`.

Two similar unknown states got the same treatment:
- `hudFadeScreen` adds to `g_hudFade`, which the game "keeps between fades" (`recomp/app/gamestate.h:243`). It is refused until `hudResetFadeScreen` has set the word.
- `endMusic` reads stream 2's count from Playback rather than the binder's zero.

### The stub rule, extended to the new routes (kept from SC-5)

- `setMonsterVal`, `setMonsterRange`, `setCreatureParams`, `charmMonster` and `followPlayer` test flag 0x20 on every object `find(name, 0x200)` returns. They are now stub reads.
- `selectSetMonsterVal`, `initScriptSetParams` and `selectSetCFlag` refuse when `g_scriptSelectedObj` 0x4039f8 (`ref_natives.G_SELECTED_OBJ`) is a stub.
- `setCFlag` still refuses on stubs.
- A used refused answer still stops the entry: every new refusal goes through the same `refuse()`.

## What runs now (host)

| Command | Route | Evidence |
|---|---|---|
| `setMonsterVal`, `selectSetMonsterVal`, `setMonsterRange` | field halves / slot 34 | executed |
| `setCreatureParams`, `initScriptSetParams` | `Creature_scaleStats` | executed |
| `charmMonster`, `followPlayer` | `Creature_setCharm`, null target only | executed |
| `hudResetFadeScreen`, `hudFadeScreen` | host `g_hudFade`; step only after a reset | executed |
| `startMusic` | gate and calls, then `Playback::music_play_level`; needs a supplied volume | executed + inferred playback |
| `endMusic` | `stream_queueStop(2)` + `Playback::queue_stop(2)` | executed |
| `setCFlag`/`selectSetCFlag` bit 0 | `Playback::play_boss_music` | selector executed; stop and queue inferred (counted) |

Refused, with no binder run: the five text setters, and every other partial or unknown row.

## Verification

All commands were run from the worktree root.

| Check | Command | Result |
|---|---|---|
| Native build | `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4` | pass, 0 compiler warnings. Pre-existing `ld: ignoring duplicate libraries` notes on the arena, real-data, quests and script measure tools. |
| CTest arm64 | `ctest -j 3` in `build/native` | **209/209** on the first run (208 + new `script-effects`), 813 s. Another agent's x86_64 ctest was running at the same time (load about 13). |
| Godot extensions | `python3 work/sc2-fix1/run-builds.py` (4 universal builds), then rerun to pick up a mid-build edit | 8/8 steps exit 0, both times |
| Godot dev matrix | `python3 work/sc2-fix1/run-godot.py` (wave-17 list) | **45/45** on the first run; `script-commands` and `real-data-empty-script` pass |
| FULL_TRACE | `godot-combined-full-combat.log` | `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes: **unchanged** |
| Sweep, scripts on | `work/run-slices-sc2.sh sc2 --scripts` (60 ticks, 4 slices) | **111 / 111 / 111**, 0 failures, 0 refusal kinds, 5 min 19 s |
| Sweep, scripts off | `work/run-slices-sc2.sh sc2n` | **111 / 111 / 111**, 0 failures, 0 refusal kinds |
| Script tests | `ctest -R "script-\|quests"` | 7/7. A mutation check that disabled `callee_unknown` made 7 checks fail, so the new tests bite. |

### Sweep counts against wave 17 (`aggregate.py sc2 r17b` / `sc2n r17bn`)

Scripts off: only timing counts changed.
- `audio.frames_pushed`, `memory.static_peak_bytes`, `time.load_ms` and `time.play_ms` vary between identical-code runs. Wave 17's own r17 and r17b differ on frames_pushed by 352,704.
- Meshes, surfaces, textures, water and ticks are equal.

Scripts on: everything else is equal. That includes started 76, wait 14, complete 28, refusal stops 39, faults 0, objects 17,398 and `callee_refusals` 0. These changed:

| Count | r17b | now | Why |
|---|---:|---:|---|
| `stub_refusals` | 102 | 205 | `setMonsterVal` (63) and `setCreatureParams` (36) were refused as partial rows before. They now reach the stub check and refuse there. `charmMonster`'s 4 calls are stub refusals too. Total refusals per command are unchanged or lower. |
| `refused.setMonsterVal` | 75 | 63 | 12 calls name no object; with no stub found, the binder runs over an empty list and stores nothing |
| `executed.setMonsterVal` | 0 | 12 | same 12 calls, 1 level |
| `refused.setCreatureParams` | 40 | 36 | 4 calls name no object |
| `executed.setCreatureParams` | 0 | 4 | same 4, 1 level |
| timing (frames, memory, ms) | | | run to run |

None of the new routes changes a stop, a wait or a completion on the disc. All adopted records are stubs, the play session supplies no music volume, and the text setters stay refused. No routed command executed a store on disc data.

### Measure tools (counts only; wave-17 build of c42ad2d vs this tree)

`champions-script-commands-measure`:
- **Routing:** the same `stub_refusals` 102 → 205 and setMonsterVal/setCreatureParams shifts as the sweep.
- **SC-8 key:** isolated by building this tree with the old key, which removes every other difference. The new key lets counting waits resume up to the cap, so:
  - `scripts_stopped_on_refusal` 38 → 39;
  - `refused.showBust` 11 → 65 and `followPath` 12 → 13;
  - `executed` grows for `gamePaused` 49 → 472, `addParticleOnObject` 24 → 240, `addDeferedCall` 50 → 230, `sin1000`/`cos1000` 24 → 240, `callScript` 20 → 38, `getScaledTime` 7 → 52, `countPlayersAlive` 8 → 64, `getHealthUsed` 4 → 20, `getv` 142 → 148, `findDistance` 0 → 6, `closestPlayerDist` 0 → 2.

`champions-quests-measure`: `getv` 142 → 148 (the SC-8 key); every other line is identical. The wave-17 build reproduces `w17-quests-measure.txt` byte for byte.

## Files changed (c42ad2d..HEAD)

- `native/src/script_commands.cpp`, `native/include/champions/script_commands.hpp`
- `native/src/script_effects.cpp`, `native/include/champions/script_effects.hpp`
- `native/tests/script_commands_tests.cpp`, `native/tests/script_effects_tests.cpp`
- `native/tests/vectors/script_effects.{tsv,json}`, `native/tests/vectors/script_commands.json`
- `tools/generate_script_effects_vectors.py`, `tools/generate_script_commands_vectors.py`
- `native/src/script_commands_measure.cpp`, `native/src/quests_measure.cpp`, `native/script_commands_targets.cmake`
- `gdextension/src/session_controller.cpp`, `godot/tests/real_data_scene.gd`: additive `callee_refusals` and `inferred` counts
- `docs/SCRIPT_COMMANDS.md`, `docs/ROADMAP.md`, `docs/QUESTS.md`

## Commits

| Commit | What |
|---|---|
| 4dcb153 | 8494cc6 rebased onto c42ad2d |
| e009a6e | SC-8..18 code, tests, pinned generators, regenerated vectors |
| a5ff561 | docs and count export |
| 4727191 | `inferred` counts bit-0 calls only |
| 0333f81 | QUESTS.md |
| b79e4f9 | quests measure cap label |
| (this commit) | this report |

## Open / unverified

- `text_lookup` needs the game's string table (lump `.uni`/`.txt`, `text_registerTable`). Until a port exists, the five text setters stay refused: 95 + 65 + 7 + 31 + 3 disc calls.
- The play session supplies no music volume, so `startMusic` and boss music are refused in play. AUD-1's documented default 0.5 (research 22408a1) is for the native job that owns audio.
- `g_hudFade` at level start is not documented. `hudFadeScreen` (0 disc calls) runs only after `hudResetFadeScreen`.
- `handle_clear` of a live target, `sound_playOnObject`, `wstr_format`, `world_createObject` and the derived slot-27 bodies remain research gaps.
- The host's `Playback`, `g_hudFade` and effect world are host-private (SC-4's wording). Godot's audio presenter does not hear them.
- Not run: x86_64 ctest, the export matrix, native Intel/Windows/Linux, and manual play.
