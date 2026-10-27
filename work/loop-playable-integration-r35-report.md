STATUS: MORE — WAVE 35 VERIFIED (2026-10-27). Four audited tips integrated; native CTest 256/256, strict Godot 54/54 plus 9/9, combat/save-load/unchanged FULL_TRACE, both private sweeps and probes PASS. Journey matches wave 34 at 80/81 steps with pickup 10→11; existing campaign and live-arena refusals remain. Post-merge adapter deduplication listed separately for audit.

# Playable integration wave 35 — 2026-10-27

## Scope, evidence and merges

Assigned clean `codex/playable-integration` at wave-34-verified `a68504da`. Read wave 34 and 33 reports, repository rules and all four PASS audits before merging. Full no-ff merges in requested order; private disc read-only, counts only. Raw evidence ignored under `work/playable-wave35/` and `work/private/w35-*`. No push, PR, branch creation, sibling edits or process-name termination.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-options` | `679ec562` | `native-options-r1-fix2.md` | `b261c31d` | frontend documentation and capture checks |
| `codex/native-arena` | `486d5769` | `native-arena-r4.md` | `14e8f820` | roadmap, simulation libraries |
| `codex/native-horsemen` | `bbaf93df` | `native-horsemen-r7.md` | `d553d0fe` | fixture declarations, runtime links |
| `codex/port-mission2-hostiles` | `8595fcf5` | `port-mission2-hostiles-r2.md` | `ab8a0607` | fight and probe additive overlaps |

## Conflict resolutions

- Options: retained both character-creation research pins and options pin; capture retains model-render check and refusal of untraced slider artwork. All other changes merged automatically.

- Arena merge: retained both arena-mode and bounded-match documentation, and all campaign/Blood1 libraries alongside arena-match. Live PvP/frame and unbound match services remain refused.

- Horsemen: retained hud_view and exactly one loaded/catalogue declaration each; combined all runtime link dependencies. Reviewed automatic runtime merges: audited property occurrence/null-list/key refusals, name registration/removal, explicit missing display-name service refusal and opt-in mount initialization coexist with newer integration services and destruction handling. No actor-lifetime algorithm introduced here.

- Mission-2: retained Chest and mission-2 dispatch and adoption; retained shared seed/starting/Chest probe flags plus counts-only flag, and both pickup and mission-2 resource lumps. Campaign and mission-2 registries delegate reconciliation/registration to the single audited actor_lifetime.hpp helper; mission-2 additionally checks authority identity. No duplicate lifetime algorithm retained.

## Non-merge code changes

Post-merge correction: automatic arena overlap duplicated creature_on_death declaration/definition. Removed duplicate declaration and unguarded adopt.cpp definition; retained api.cpp implementation with Full/nonplayer/creature guards and identical model call. Initial Release build failed at duplicate declaration; no checks weakened.

## Gates and evidence

All requested gates executed and recorded below. Live arena match stays refused; native/synthetic success is not campaign acceptance. Wave-34 comparison is 80/81 steps, pickup 10→11; broader trigger/loading/checkpoint/death-reload refusals remain.

- Native Release configure PASS; build retry PASS (`build-final.log`, -j 4), after the recorded duplicate adapter correction. All four audited tips verified ancestors; no unmerged paths and diff check PASS.

## Pending full CTest record

Full `ctest --test-dir build/native --output-on-failure -j 3` was **PENDING**, not started at checkpoint commit `1780604a`. Result will be recorded separately; any timeout will be rerun alone and both results retained.

- native-ai: **PASS**, exit 0, 1.88 s. Counts-only output: `No probe summary`. Labelled diagnostic placement/hit; no normal-route or respawn claim. Evidence `native-ai.log`.

- native-death: **PASS**, exit 0, 1.13 s. Counts-only output: `death_check actors=78 ticks=600 failed=0 dead=1 zero_hp=1 wipe_timer=0 slot_timer=0 task=1 phase=1 fade=128 native_clip_started=1 native_clip_steps=239 diagnostic_hit=1 retained_record_fields=1 same_dead_body=1 reload_refused=1`. Labelled diagnostic placement/hit; no normal-route or respawn claim. Evidence `native-death.log`.

- Private sweep `w35-on`: **PASS**, four slice exits [0, 0, 0, 0]; counts [111, 111, 111, 230] (tried/loaded/played/checks), failures 0, top-level refusal kinds 0. Four 60-tick slices run serially to limit load alongside CTest. Private disc read-only; internal unsupported-service counters remain separate. Evidence ignored `work/private/w35-on-s{0,28,56,84}.{json,log}`.

- Universal GDExtension and level-texture debug/release libraries **PASS**, 8/8 configure/build exits 0, arm64;x86_64, Release, parallelism 4. `build-results.json` and `extension-builds.out`. Debug extension rebuilt before sweep launch; no stale extension gate credited.

- Private sweep `w35-off`: **PASS**, four slice exits [0, 0, 0, 0]; counts [111, 111, 111, 230] (tried/loaded/played/checks), failures 0, top-level refusal kinds 0. Four 60-tick slices run serially to limit load alongside CTest. Private disc read-only; internal unsupported-service counters remain separate. Evidence ignored `work/private/w35-off-s{0,28,56,84}.{json,log}`.

- Strict Godot dev matrix **54/54 PASS**, including six graphical captures; strict diagnostic gate, isolated user directories, no allowlist. `matrix/results.json`.

- Full synthetic combat **2,645 checks, 0 failures**. `matrix/playable_wave_smoke_full_combat.log`.

- FULL_SAVE_LOAD **300/300 presentations**, matching continued/uninterrupted SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`. Same fresh full-combat log.

- FULL_TRACE **36,402 bytes / `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, unchanged. No baseline changed. Same fresh full-combat log.

- Extra wave-33 loading/audio/identity/moves/death/present-adapter cases **9/9 PASS**, strict log inspection including rendered identity, death and present cases. `extra-matrix/results.json`.

## Full scripted journey

Command: `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/playable-wave35/playthrough`. Unchanged driver, acceptance and timeout; serial headless/rendered.

| Variant | Steps | Exit | Ending | Pickup | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | player_death refused; saved_level_reloaded=false | **10→11**, produced/walked/picked_up | 0 | 0 |
| Rendered | 81 | 1 | player_death refused; saved_level_reloaded=false | **10→11**, produced/walked/picked_up | 0 | 38 |

Matches wave 34's 80/81 steps and pickup. Both pass 22/22 combat-input assertions, one death, one loot call, 11 shared loot draws, one generated consumable, 26 walking ticks and zero retry kills/diagnostic retry placements. Existing initial diagnostic placement remains labelled; no normal-route combat acceptance. Six refusal kinds each; one ObjectDB exit-leak warning each; no strict log errors. Wrapper exit 1 retains the unsupported full-journey acceptance boundary. Disc file counts **994 before / 994 after**.

Inspected actual rendered `05-fight-attempt.png`: visible fight presentation and diagnostic label report one loot item and one picked up. This is not original-client pixel parity; script retains rendered_pickup_verified=false, wrapper pickup_verified=true. Existing unsupported script-trigger stop, dead-Creature departure/checkpoint refusal, loading prerequisites and unsuccessful saved-level reload remain explicit. No new journey, RNG, fixture or baseline changes. Evidence ignored under `playthrough/` and `playthrough.out`.

## Full CTest result record

After pending commit `1780604a`, full `ctest --test-dir build/native --output-on-failure -j 3` on runtime `f648c673` completed **256/256 PASS**, **674.20 s**, exit 0. No timeout, so no isolated timeout rerun required. Evidence `work/playable-wave35/ctest.out`. Added native tests from merged work account for the count increase from wave 34's 253. No assertion, fixture or baseline weakened. Result recorded in this separate commit.

## Exact command families

Same wave-33/34 drivers and method, wave-35 output directories, native Release `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and `cmake --build build/native -j 4`, universal builds parallelism 4, full CTest parallelism 3. Strict matrix uses `tools/godot_matrix.py matrix --captures --timeout 1200`; combat/save-load/trace are from that fresh matrix's full-combat case. Private probes and both 111-level sweeps reuse the prior serial four-slice/60-tick method. Private paths occur only in ignored drivers/logs.

## Remaining boundaries and final disposition

All four requested full no-ff merges committed in order; none aborted or omitted. All tips are ancestors of HEAD. Post-merge correction `f648c673` is the sole additional source change beyond conflict resolution and is separately listed above for audit; no independent PASS audit of that integration correction is claimed.

Live arena match/PvP frame execution, normal-route mission-2 combat, unsupported script-trigger continuation, loading prerequisite completion, dead-Creature checkpoint/departure and successful death reload remain refused/unverified. Journey exit 1 is retained and reported, not converted to campaign acceptance. Native/synthetic gates do not establish original-client pixel parity or full playable acceptance.

Final ancestry, no-unmerged-path, ignored-evidence and `git diff --check a68504da..HEAD` checks PASS. No push, PR, branch creation, private-disc write, sibling worktree change or process termination. Only authorized local commits; final tree clean after report commit.
