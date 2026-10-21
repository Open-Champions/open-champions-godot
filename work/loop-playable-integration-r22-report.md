STATUS: COMPLETE — all five authorized merges, deterministic gates, exports, and native tests verified; the initial Rosetta suite had one timeout that passed alone.

# Playable integration wave 22 — 2026-10-21

## Merge ledger

From wave 21 `ed63ccd`, merged only these branches in the requested order, with a local commit after each verified merge:

| Branch | Source | Local merge | Result |
|---|---|---|---|
| `codex/small-fixes` | `b56bf10` | `99e53e3` | Mission rows 1..5 and researched spell getters. Difficulty now aliases one quest source. Focused native tests passed. |
| `codex/strings` | `6fdb048` | `c7d5107` | GTEXT, script text commands, text HUD, help confirmation and dialog input ownership. Focused native and real-data text checks passed. |
| `codex/blend-fx` | `b297c44` | `5d7de5b` | Per-surface lit FX. Added the audit's BFX-1 nonzero red reference assertion; both rendered smoke modes pass 22 checks. |
| `codex/perf-2` | `8ec62d5` | `b0d3322` | Single streamed terrain, retained global VAT in the one process mixer, and paced cell publication. Native build and the 31-check stream plus 10-check textured session smokes pass. |
| `codex/win-linux` | `adf701c` | `afc087f` | Portable x64 cross-builds and exports. Canonicalized the build path in `source_path_map.cmake` (WL-2), and stated the verifier's actual private-marker scope (WL-1). Host extension rebuild and cross export verification pass. |

An intermediate docs commit `014aefa` records the two-branch comparison point. No other branch was merged. `codex/creature-stats` remains excluded after its two MEDIUM audit findings. The unaudited or out-of-scope `combat-ctors` tail, `player-state`, and `arena-match` remain excluded. All commits are local; no push or PR.

## Behavior and count attribution

Both final four-slice, 60-ready-tick real-data sweeps try, load, and play **111/111** levels with scripts on and off. Each has **230 passed checks**, zero failed checks, and an empty refusal list. Each slice uses its own `user://` cache. Scripts-off has no changed stable count versus wave 21. Scripts-on has no changed stable gameplay, script, quest, help, or dialog count versus the verified two-branch wave-22 baseline.

Against wave 21, `small-fixes` changes mission available bits **32 → 23** and complete bits **13 → 4**: the removed nine bits in each are row-0 skill bytes, while researched mission rows are 1..5. The spell getter correction changes no sweep count. `strings` resolves eight `addHelpMessage` and 384 `showBust` calls, and two previously refused scripts continue; all 8 help boxes confirm after their 90-frame delay, run their post actions, and release the pause. `blend-fx` and `win-linux` change no simulation count. `perf-2` leaves every stable count unchanged.

The only additional final-versus-two-branch draw count is `text.frames_drawn.bust` **30 → 34**. The HUD increments this from `_process(delta)` at display-frame cadence while the sweep advances fixed ready ticks; the paced worker changes wall scheduling, so this value is not deterministic. The `help_frame` count remains **713**. The table below lists every changed numeric key against wave 21 with its branch attribution; absent keys are shown as `—`.

| Numeric key | Wave 21 | Wave 22 final | Attribution |
|---|---:|---:|---|
| `audio.frames_pushed` | 2,011,377 | 2,691,153 | runtime scheduling; perf-2 affects pacing |
| `campaign.mission_available_bits` | 32 | 23 | small-fixes |
| `campaign.mission_complete_bits` | 13 | 4 | small-fixes |
| `campaign.vars` | 183 | 184 | strings (continued scripts or text HUD) |
| `executed.addDeferedCall` | 1,702 | 1,839 | strings (continued scripts or text HUD) |
| `executed.addHelpMessage` | — | 8 | strings (continued scripts or text HUD) |
| `executed.addParticleOnObject` | 4,644 | 5,724 | strings (continued scripts or text HUD) |
| `executed.callScript` | 103 | 104 | strings (continued scripts or text HUD) |
| `executed.countPlayersAlive` | 249 | 345 | strings (continued scripts or text HUD) |
| `executed.countQuestItems` | 33 | 274 | strings (continued scripts or text HUD) |
| `executed.gamePaused` | 2,460 | 2,672 | strings (continued scripts or text HUD) |
| `executed.getRand` | 1,457 | 1,817 | strings (continued scripts or text HUD) |
| `executed.getScaledTime` | 256 | 376 | strings (continued scripts or text HUD) |
| `executed.getv` | 980 | 1,040 | strings (continued scripts or text HUD) |
| `executed.removePlayerItem` | 37 | 47 | strings (continued scripts or text HUD) |
| `executed.setSaveInhibit` | 18 | 19 | strings (continued scripts or text HUD) |
| `executed.setv` | 99 | 100 | strings (continued scripts or text HUD) |
| `executed.showBust` | — | 384 | strings (continued scripts or text HUD) |
| `levels_executed.addHelpMessage` | — | 7 | strings (continued scripts or text HUD) |
| `levels_executed.addParticleOnObject` | 7 | 8 | strings (continued scripts or text HUD) |
| `levels_executed.countPlayersAlive` | 6 | 7 | strings (continued scripts or text HUD) |
| `levels_executed.gamePaused` | 18 | 19 | strings (continued scripts or text HUD) |
| `levels_executed.getRand` | 18 | 19 | strings (continued scripts or text HUD) |
| `levels_executed.getScaledTime` | 4 | 5 | strings (continued scripts or text HUD) |
| `levels_executed.getv` | 52 | 53 | strings (continued scripts or text HUD) |
| `levels_executed.removePlayerItem` | 3 | 4 | strings (continued scripts or text HUD) |
| `levels_executed.setSaveInhibit` | 18 | 19 | strings (continued scripts or text HUD) |
| `levels_executed.showBust` | — | 7 | strings (continued scripts or text HUD) |
| `levels_refused.addHelpMessage` | 7 | — | strings (continued scripts or text HUD) |
| `levels_refused.savePlayer` | 8 | 9 | strings (continued scripts or text HUD) |
| `levels_refused.setAlternateDeath` | 8 | 9 | strings (continued scripts or text HUD) |
| `levels_refused.setCreatureParams` | 9 | 10 | strings (continued scripts or text HUD) |
| `levels_refused.showBust` | 7 | — | strings (continued scripts or text HUD) |
| `memory.static_peak_bytes` | 2,198,041,428 | 1,531,496,240 | perf-2 shared terrain and VAT |
| `refused.addHelpMessage` | 8 | — | strings (continued scripts or text HUD) |
| `refused.savePlayer` | 8 | 9 | strings (continued scripts or text HUD) |
| `refused.setAlternateDeath` | 8 | 9 | strings (continued scripts or text HUD) |
| `refused.setCreatureParams` | 36 | 37 | strings (continued scripts or text HUD) |
| `refused.showBust` | 307 | — | strings (continued scripts or text HUD) |
| `scripts.reached_wait` | 29 | 30 | strings (continued scripts or text HUD) |
| `scripts.refusal_stops` | 16 | 14 | strings (continued scripts or text HUD) |
| `scripts.stub_refusals` | 265 | 266 | strings (continued scripts or text HUD) |
| `text.frames_drawn.bust` | — | 34 | strings; perf-2 display cadence |
| `text.frames_drawn.help` | — | 0 | strings (continued scripts or text HUD) |
| `text.frames_drawn.message` | — | 0 | strings (continued scripts or text HUD) |
| `text.help_boxes_confirmed_by_harness` | — | 8 | strings (continued scripts or text HUD) |
| `text.help_frames` | — | 713 | strings (continued scripts or text HUD) |
| `text.help_frames_max` | — | 357 | strings (continued scripts or text HUD) |
| `text.help_paused_session` | — | 8 | strings (continued scripts or text HUD) |
| `text.help_post_actions` | — | 8 | strings (continued scripts or text HUD) |
| `text.levels_with_table` | — | 111 | strings (continued scripts or text HUD) |
| `time.load_ms` | 40,749 | 39,958 | runtime scheduling; perf-2 affects pacing |
| `time.play_ms` | 51,042 | 79,226 | runtime scheduling; perf-2 affects pacing |

The four-process summed `memory.static_peak_bytes` falls from **2,201,931,956 → 1,531,496,240** with scripts on and **2,199,602,500 → 1,529,047,064** off. This is attributable to `perf-2`'s shared streamed terrain and retained global bank. Load time changes **48,140 → 39,958 ms** on and **45,263 → 39,898 ms** off; play time changes **59,466 → 79,226 ms** on and **54,337 → 78,462 ms** off. `audio.frames_pushed` changes **2,181,265 → 2,691,153** on and **2,046,001 → 2,646,961** off. Those time and audio totals depend on concurrent machine load and rendering cadence and do not isolate a performance gain or loss.

## Build, runtime, and export verification

- Four universal macOS GDExtension configure/build pairs pass. The Godot dev matrix is **53/53**. Headless Full passes **2,620 checks**, zero failures.
- The macOS export is `build/exports/wave22/OpenChampions.zip`, SHA-256 **`38d2109f513dd55f502d620c7c854022e6b6ba83d606473dc28d2a28bfc3e8f2`**. The executable and both release dylibs contain arm64 and x86_64 slices. Packaged dylibs match build outputs byte for byte. The ZIP has eight members, no game-data suffix, and no `private-validation` marker in extracted member bytes.
- Export matrix **21/21**. The graphical 1280×720 Full run reaches **36,000 ticks**, passes **108,826 checks**, zero failures, with 63 synthetic captures and save/load continuation. The dev headless and all four exported Full runs preserve **FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9` (31,622 bytes)**. Synthetic HUD, play, and Full combat captures were inspected.
- The exported app's read-only real-data smoke passes **12/12** without scripts, **12/12** with scripts, and **3/3** arena levels with scripts: 26, 26, and 8 checks, zero failures. All 12 creature previews draw in each main run; the nine wave-21 BLEND-7 mixed-pass material refusals are now absent. The exported windowed text smoke passes **17 checks**, including a drawn help box and bust, dialog input refusal during help, a paused ready tick, confirmation, 60-frame menu lockout, and resumed ticks.
- Windows x64 and Linux x64 Zig cross-builds and release exports pass. Each export has exactly three files and **273 checksummed resources**; the verifier finds matching release libraries, expected descriptors, no forbidden game-data suffix and no checked private marker. All four library entry symbols are present and no checkout/private marker appears in their printable strings. Target Windows/Linux execution remains unverified.
- Native arm64 Release CTest passed **226/226** in 1,161.42 s. The initial Rosetta x86_64 Release suite passed **225/226** in 1,733.65 s; only `player-exact` hit its 240 s timeout while other CTest suites shared the machine. Its unchanged isolated retry passed **1/1 in 172.35 s**, so all 226 x86_64 tests are verified, while the first parallel run is explicitly not called clean. Wave 21 had 225 tests; `strings-exact` is the added test.

## Preserved limits

A refused script command still stops execution, including `setCFlag` on a placeholder. The Full fight still excludes the Skeleton state that lacks pinned research. The native audio bridge remains one mixer per process and keeps its global VAT after session reset. `perf-2`'s `get_terrain()` is summary-only for the game-folder streamed path; callers that need full mesh arrays must use the full terrain path. Its 2,500 µs build budget is a client scheduling choice, not an original-client constant. The cross exporter verifier checks the current home path and `private-validation` marker, not every possible private path. Negative verifier fixtures and actual Windows/Linux launches were not part of this wave. The disc was read only; no private names, bytes, or level images were committed. No values were invented to satisfy a test.

The blend-fx audit's remaining LOW BFX-2 is separate negative coverage for an unmarked shader; the existing rendered smokes exercise the accepted marked surfaces. The perf and cross-export LOW limits above remain documented rather than being treated as original-client facts.

Counts-only sweep outputs and test logs are under ignored `work/private/` and `work/playable-wave22/`. Mac, Windows and Linux exports are under ignored `build/exports/`.
