STATUS: MORE — WAVE 34 VERIFIED (2026-10-27). Eight audited branches integrated (play-levelup, play-quests, play-save, native-hud, play-pickups, play-dialog, play-mission2, matrix-errors); integration-made fixes 30d78414..dcf6d30f audited PASS (audits/wave34-integration-fixes.md). CTest 253/253, Godot 54/54, strict matrix, combat, save/load, FULL_TRACE unchanged, sweeps, probes; pickup restored 10→11 headless and rendered; journey ends at explicit bounded refusals (player death reload, unsupported dialog trigger stops the script, loading readiness).

# Playable integration wave 34 — 2026-10-26

## Scope, evidence and merges

Worked only in `worktrees/playable-integration`, branch `codex/playable-integration`, starting clean at wave-33 `66e52972`. Read wave 33 and 32 first, repository rules, contributing guide, roadmap and all eight requested PASS audits. Private disc inputs remain read-only; raw logs/captures stay ignored under `work/playable-wave34/` and `work/private/w34-*`. No push, branch creation, PR, or process-name termination.

All eight requested tips integrated through full `git merge --no-ff` in the requested order, each committed before the next merge. No cherry-pick or omitted tip.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-levelup` | `018d0377` | `native-levelup-r1-fix4.md` | `c704bb41` | menu service/native ledger, HUD and menu fixtures, play flow |
| `codex/play-quests` | `710b28fe` | `play-quests-fix.md` | `264dfed1` | controller/text APIs, real scene and journey |
| `codex/play-save` | `071f0bc5` | `play-save-fix2.md` | `ffc55ce6` | controller, native object fields, real scene and journey |
| `codex/native-hud` | `b8c54fa0` | `native-hud-r5.md` | `9a0e6090` | none |
| `codex/play-pickups` | `4d6cb8e3` | `play-pickups-r4.md` | `29043e41` | fight/controller, inputs, object construction and journey |
| `codex/play-dialog` | `457394a4` | `play-dialog-r2.md` | `527b0ed6` | none |
| `codex/play-mission2` | `c0695018` | `play-mission2-r1.md` | `dd0ab53d` | mission controller and real scene |
| `codex/matrix-errors` | `a8fcedec` | `matrix-errors-r2.md` | `30d78414` | component fixture and journey wrapper |

## Conflict resolutions

- Level-up: retained inventory display-grid and vendor pause/refusal checks alongside staged companion validation, actual sender-half restoration and XP rules. HUD retains existing glow and incoming green XP bar assets/checks. NLU-10 report status corrected; later wave-24 trace attribution is static, distinct from the historical branch measurement.
- Quests: retained campaign carry, loading, Blood1 and combat journey checks; added journal input ownership, quest observations and zero-submission layout refusal assertions. Frontend/game menus own Q before journal handling.
- Save: combined native field directory and attachment/key validation with existing shared-World fight and trigger state. Both fight and world-state companions and refusals retained; diagnostic level selection remains labelled. Index/ordinal use the same parsed record order. PSF2-1 API wording is checked in follow-up below.
- Pickups: kept MovePad edge/hold/combo mapping, AI/death lifecycle and native pickup/use routing, replacing obsolete attack-only latch. Retained native starting inventory and record observations, production Chest guard regressions, save preflight refusals, world-field roundtrip checks and pickup inventory-growth check. Actual inventory 10→11 remains a required journey outcome.
- Dialog: unsupported trigger stops the script; later journey effects must be refused explicitly rather than assumed executed.
- Mission 2: retained researched mission-art panel and existing network/menu checks, composed act-transfer and complete-command refusal gates with the incoming runtime fade service. No replacement host-layout panel. Mission-2 links/objectives remain Unknown/refused.
- Matrix: one declaration per fixture field, retaining hud_view; wrapper retains both Blood1 options, pickup verification and strict log errors. No log allowlist, weakened baseline or guessed game value.

## Gates and evidence

Native Release configure/build PASS (`cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/native -j 4`); logs `configure.log`, `build.log`. PSF2-1 API comment distinguishes the independently specified native raw HP words from the writer HP fraction, with the audited in-memory layout citation. PP4-1 section reference corrected in fixture and owner report. Remaining gates pending.

## Full scripted journey

Wave-33 baseline: 67 headless / 68 rendered steps, exit 1 at bounded readiness/death-reload refusals. Fresh wave-34 validation is blocked by lost pickup; see the final journey recovery record below. No campaign acceptance is claimed.

## Exact command families

Same drivers/method as waves 32/33, output directory wave 34, build parallelism 4 and full CTest parallelism 3. Incoming strict matrix runs over the integrated candidate; added wave-33 cases retain strict log inspection. Private paths remain only in ignored drivers/logs.

## Remaining boundaries

Gates pending. Unsupported behavior remains explicitly refused; synthetic/native success is not campaign acceptance or original-client pixel parity.

- Universal GDExtension + level-texture debug/release libraries PASS: all four configure/build pairs (8/8), Release, arm64;x86_64, parallelism 4. `build-results.json` / `extension-builds.out`.
- Strict tooling regression suite PASS: 12 tests. Initial fixture lacked the newly mandatory pickup; corrected its success fixture to an authored 10→11 pickup, retaining diagnostic-error rejection and adding missing-pickup rejection. No acceptance rule weakened. `tooling-tests{,-final}.log`.

## Pending full CTest record

Full `ctest --test-dir build/native --output-on-failure -j 3` is **PENDING**, not yet started at this commit. Native runtime source is the eight-merge candidate plus API-comment clarification only. Result will be recorded in a separate commit, with any timeout rerun alone and both results retained.

- Initial strict matrix caught one composed HUD fixture error: merged level-up catalogue now loads green frames, while inherited missing-art test assumed they were absent. The negative test now explicitly removes all eleven green frames and restores them for the following positive assertions. All existing assertions retained; focused HUD smoke passes with no diagnostics (`hud-fixture-fixed.log`). Initial matrix result remains separate; final retry pending.

- Full synthetic combat PASS: **2,645 checks, 0 failures** (four additional HUD assertions from audited native-hud work; wave 33 had 2,641). Strict log gate accepts the fresh output. `matrix/playable_wave_smoke_full_combat.log`.

- FULL_SAVE_LOAD PASS: **300/300 presentations**, continued/uninterrupted presentation hashes identical (`cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`). Same fresh full-combat log.

- FULL_TRACE PASS unchanged: **36,402 bytes / `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**. No trace baseline or fixture changed. Same fresh full-combat log.

- Strict Godot dev matrix: first run **53/54 PASS**, only the corrected missing-green-art fixture failed; isolated affected retry **1/1 PASS**. All 54 cases now pass, including six graphical captures and ui_layout_smoke with its corrected five-argument call. Extra wave-33 suites **9/9 PASS** with strict log inspection: loading display/audio, identity (headless/rendered), campaign moves, death (headless/rendered), and loading-present/partial-adapter (headless/rendered). No caught error was allowlisted or ignored; no SCRIPT ERROR remains in successful logs. Evidence `matrix/results.json`, `matrix-retry/results.json`, `extra-matrix/results.json`.

## Journey composition follow-up

Initial fresh journey: **77 headless / 78 rendered steps**, both exit 1, zero log errors; pickup **10→10**, no drop. Disc file count **994/994**. Save's newly enforced dead-Creature refusal left the harness at save-game, and ordinary level departure correctly refused checkpointing those fields. Thus enemy level/fight/pickup were not reached; no success claimed. Evidence `playthrough/`. Follow-up closes a refused save with the existing Cancel input while retaining failed save assertions, logs the unsupported dialog trigger's script stop, and records ordinary fight departure refusal before a separately labelled diagnostic selection using the save branch's existing non-checkpoint census option. No production refusal is bypassed or changed; normal-route acceptance remains false.

- Journey follow-up reaches the separately labelled diagnostic fight level after preserving ordinary departure refusal. Combat inputs pass 20/22 inherited assertions: only the unarmed guard/swap expectations fail because audited starting inventory now equips the player. Journey assertions retain the original unarmed refusal checks when weapon class is 5 and require actual blocking/state 8 plus state 11 and no refusal for the equipped path; no test is changed to unconditional success. Learned-record/mana, combo/edge/hold and pause assertions remain. Values 8/11 and class 5 are the existing checked native domains, not newly invented behavior. Focused/full execution of this correction is pending.

## Full CTest result record

After pending commit `17499ff0`, full CTest completed **250/253 PASS**, **915.50 s**; no timeout. Three assertion/refusal failures: menus-exact aborts on missing live stack stem in a newly imported level-up fixture; player-death-flow expects a no-executable failed-start fight to continue ticking; pickups catches a stale factory-head cache during adopted Chest destruction. All failures remain recorded (`ctest.out`). Fixes and reruns pending; this is not a green native gate.

- Native integration fixes: adopted Chest destruction now removes cached aliases only when each points to that exact destroyed host owner's cleared head; shared heads are never moved into combat ownership or retired by address alone. The existing Chest lifetime test covers destruction, retained handles, immediate pool reuse and replacement ownership. Menu level-up import fixture supplies two authored nonempty stack stems required by the retained vendor adoption guard; refusal checks remain. Death fixture preserves failed-start/no-mutation assertions, then drives the synthetic World with the actual installed campaign death callbacks; all controller-out/warm-up, pause, countdown and HUD refusal assertions remain. This verifies callback binding, not successful gameplay after invalid startup.
- Native fix Release build PASS; focused **3/3 PASS**, 1.58 s: menus-exact, player-death-flow, pickups. `native-fixes-build.log`, `native-fixes-focused.log`. No assertion, fixture baseline or production refusal weakened. Full CTest on this runtime change remains pending.

- Universal rebuild after exact Chest-head cache fix: **8/8 configure/build exits 0**, both GDExtension and level-texture debug/release, arm64;x86_64, parallelism 4 (`extension-builds-final.out`).

## Pending final full CTest record

Final full CTest on runtime `0e4c886a` is **PENDING**, not started at this commit. Initial full run's 250/253 and the three focused fixes remain distinct. No timeout occurred in the initial run.

- native-ai: **PASS**, exit 0, 3.10 s. Counts-only output: `No probe summary`. Labelled diagnostic placement/hit; no normal-route or respawn claim. Evidence `native-ai.log`.

- native-death: **PASS**, exit 0, 1.27 s. Counts-only output: `death_check actors=78 ticks=600 failed=0 dead=1 zero_hp=1 wipe_timer=0 slot_timer=0 task=1 phase=1 fade=128 native_clip_started=1 native_clip_steps=239 diagnostic_hit=1 retained_record_fields=1 same_dead_body=1 reload_refused=1`. Labelled diagnostic placement/hit; no normal-route or respawn claim. Evidence `native-death.log`.

- Private sweep `w34-on`: **PASS**, four slice exits [0, 0, 0, 0]; counts [111, 111, 111, 230] (tried/loaded/played/checks), failures 0, top-level refusal kinds 0. Four 60-tick slices run serially to limit load alongside CTest. Private disc read-only; internal unsupported-service counters remain separate. Evidence ignored `work/private/w34-on-s{0,28,56,84}.{json,log}`.

- Private sweep `w34-off`: **PASS**, four slice exits [0, 0, 0, 0]; counts [111, 111, 111, 230] (tried/loaded/played/checks), failures 0, top-level refusal kinds 0. Four 60-tick slices run serially to limit load alongside CTest. Private disc read-only; internal unsupported-service counters remain separate. Evidence ignored `work/private/w34-off-s{0,28,56,84}.{json,log}`.

Final full CTest (recorded by the orchestrator, 2026-10-26 ~20:55, after the job's provider stopped on a 401): `ctest --test-dir build/native --output-on-failure -j 3` on the post-fix candidate — **253/253 passed, 814.37 s** (log `work/playable-wave34/ctest-final.out`). This supersedes the first run's 250/253. The journey rerun and the audit of the integration-made fixes (30d78414..tip) are still pending. Note: an uncommitted 24-line edit to `godot/tests/loop_playthrough.gd` from the stopped job remains in the worktree for the next job to review; it is not part of this record.

## Final journey recovery — 2026-10-27 (checkpoint)

Reviewed the stopped job’s 24-line pursuit edit and `playthrough-followup/` first. Previous follow-up timed out headless (124) and ended rendered with signal 15, neither writing a result. Fresh wrapper execution uses `playthrough-final/`, with both variants loading the pursuit candidate before its rejection. Headless completed 80 steps / exit 1, no log errors, no captures; last step `player_death: refused`, saved_level_reloaded=false. Pickup failed: inventory 10→10, no drop, no walking/pickup; eight retry kills and nine diagnostic placements. Combat inputs passed 22/22.

The pursuit candidate was discarded with `git checkout -- godot/tests/loop_playthrough.gd`: movement telemetry showed retry 8 stuck about 1,440 units away with unchanged target HP through the 3,000-cycle bound, and the final player position reached the lower Z bound. It is not a demonstrated integration fix and must not be committed as one. No checks, fixtures, combat values, RNG or production refusals changed. Fresh results describe the rejected candidate, not a green verification of the restored script. Rendered confirmation and final diagnosis follow in the next record.

## Final journey result and blocker — 2026-10-27

Command: `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/playable-wave34/playthrough-final`. Serial variants, unchanged wrapper timeout/acceptance rules. Both loaded the rejected pursuit candidate; neither result certifies the restored script. No further speculative harness or runtime edit was made.

| Variant | Steps | Exit | Ending | Pickup | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | `player_death: refused`, saved_level_reloaded=false | 10→10; no produced drop, walking or pickup | 0 | 0 |
| Rendered | 81 | 1 | `player_death: refused`, saved_level_reloaded=false | 10→10; no produced drop, walking or pickup | 0 | 38 |

Both write results, with six aggregate refusal kinds and an ObjectDB exit-leak warning each. Neither hangs or reaches the wrapper timeout. Compared with wave 33’s 67/68 steps, these have additional merged checks, but lose required pickup acceptance and do not reach a successful saved-level reload/death probe. A larger step count and exit 1 do not make this equivalent to wave 33’s bounded loading-readiness refusal. Disc file counts remain **994 before / 994 after**, private input read-only.

**Observed:** both pass 22/22 combat-input checks, record nine deaths, eight retry kills / nine diagnostic retry placements, 13 loot calls, 64 loot shared draws, zero generated items and zero pickups. `enemy_fight` explicitly reports missing `loot_ground_object_spawn` and `loot_item_pickup`. Retry 8 remains about 1,440 units away at unchanged HP through the existing 3,000-cycle bound; final player Z reaches the lower world bound. Pursuit therefore does not restore the journey. Both record `hub_trigger_mission_menu: refused`, `script_fault="refused trigger"`, `script_refusal_stops=1`, `trigger_action_ok=false`, with unsupported `trigger` and `autoPropSetState` commands. Script-stop behavior is not treated as successful mission continuation. Ordinary level departure also explicitly refuses unrestorable Creature death state; fight selection remains separately labelled diagnostic.

**Root-cause evidence and scope:** death-handler refusal counters include one `potion lookup is outside supplied table` and three `player name format: empty, unterminated or over host bound`. `native/src/item_generation.cpp::generate_consumable` indexes mana/heal potion-name vectors, but `native/src/script_items.cpp::image_tables` never populates those vectors; the omission is present in both wave-33 `66e52972` and wave-34 `91e32bb8`. The formatting callback is likewise unchanged across that comparison. This is concrete evidence of incomplete runtime binding exposed by this composed journey, not evidence authorizing new constants/tables or a wave-34 conflict-only fix. Missing level-load globals were an investigated hypothesis, not the observed refusal. The exact causal contribution of each death refusal, and why the ninth target separates, remain Unknown; no single-cause claim is made for every missing drop.

**Disposition:** discarded all 24 uncommitted pursuit lines with the requested checkout command. No assertion, baseline, fixture, timeout, production code, RNG state or private data was changed. Restored script remains at the inherited committed version; its prior follow-up timeout remains distinct evidence, and a fresh restored-script completion is unverified. The remaining journey gate is **BLOCKED**, not green; resolving upstream runtime bindings/navigation is outside this conflict-only test assignment. Already completed gates (including final CTest 253/253) were not rerun.

Inspected actual rendered `04b-fight-approach.png` and `05-fight-attempt.png`; captures support diagnostic combat presentation, not pickup acceptance or original-client parity. Evidence stays ignored under `work/playable-wave34/playthrough-final/` plus `playthrough-final.out`; the summary, variant JSON and full logs preserve all refusals. Only this tracked report was committed; no push, PR, branch creation, sibling edits or process termination.

## Pickup runtime-binding repair — diagnosis checkpoint (2026-10-27)

Assigned clean start `dcf15739`, branch `codex/playable-integration`. No pursuit edit is present. Clean-tree wrapper is running unchanged in `pickup-clean-repro/`; completion is pending. A separate clean native diagnostic (`--ticks 600 --near-weakest --weak --attack-every 30`, default level/seed) completes and independently reproduces `WarBoar_setState(20): potion lookup is outside supplied table` and a tick formatting refusal; it is diagnostic placement/HP, not normal-route acceptance. Evidence `pickup-clean-native.log`.

Observed source: `image_tables` loads neither potion pointer list, while `generate_consumable` indexes them. Pinned research-loot-drop `77c2f70`, `docs/data-tables.md` section 1 and `docs/spec/items.md` sections 1/7.5 establish addresses `0x402708` / `0x402730`, 4-byte pointers, nine names each followed by null. These can be loaded through the existing runtime ImageReader; no authored game strings are needed.

The formatter callback reads sparse World memory, populated only for player literal ranges. Shared non-player callers include PhProp construction at format address `0x532688` (research-combat-ctors-3 `9ff6ae0`, `docs/spec/gut-chunk.md`; native `combat_phprop_ctors.cpp`) and creature presentation formats. The strict callback was introduced by `c8bc0e00`; `4d6cb8e3` has no callback, and Full `libc_sprintf` returns zero in its absence. Thus the old pickup result did not establish complete formatting or potion binding. The composed journey additionally runs combat-move checks before the inherited fight/retry loop, changing elapsed ticks and shared draw context. No claim of identical loot rolls is warranted. Fix will read requested format addresses through the same executable loader used elsewhere, retaining the existing syntax/length refusals.

Diagnosis checkpoint only: no runtime/test/baseline change yet; causal split for the full clean journey remains pending. No private names, decoded content or private paths are committed.

## Runtime-binding implementation review (2026-10-27)

Reviewed all five interrupted edits before keeping them. Production strings come exclusively from the selected executable through existing ImageReader/Exe byte loaders. Synthetic test strings are authored fixtures, not game names. Verified research-loot-drop `77c2f70`, `docs/data-tables.md` section 1 (two 4-byte, nine-name, null-terminated lists at `0x402708` / `0x402730`) and `docs/spec/items.md` section 7.5. Verified research-combat-ctors-3 `9ff6ae0`, `docs/spec/gut-chunk.md` section 4: PhProp calls formatting at `0x532688`, outside the sparse player literal copies. Existing format syntax, empty/unmapped/unterminated/resource-bound refusals remain. No fallback names or game data added.

Comparison with `4d6cb8e3`: no campaign format callback there; Full libc_sprintf returns zero without one. The later strict callback read absent World bytes for non-player callers. Both tips omit potion lists; changed journey combat-move timing means equal loot RNG context cannot be assumed. Research-pickups `9911a6b`, `docs/pickups.md`, retains manual interaction and real inventory-transfer requirements. No journey, RNG, refusal or baseline is changed.

Clean pre-fix reproduction now completed: headless 80 / rendered 81 steps, both exit 1, pickup 10→10, no drop, zero log errors; four retry kills / five diagnostic placements. This is the restored committed script, distinct from the earlier rejected pursuit candidate. Evidence `pickup-clean-repro/`.

**Pending before full CTest:** fresh build, focused tests, full `ctest -j 3` with isolated timeout reruns, rebuilt GDExtension, strict Godot dev matrix / FULL_TRACE, and clean committed headless/rendered journey. No gate success is claimed at this checkpoint.

- Implementation milestone `51d73589`: fresh Release configure/build PASS (`-j 3`); focused pickups/items/loot/formatter suite **7/7 PASS**, 38.45 s (`pickup-review-{configure,build,focused}.log`). Both universal macOS GDExtension targets rebuilt PASS with parallelism 1 (`pickup-extension-{debug,release}.log`). Full CTest `-j 3` and strict Godot matrix have now started; their results remain pending.

## Repaired clean-tree journey and Godot gates (2026-10-27)

Runtime `51d73589`, clean committed tree `983650fd` (no tracked edits during execution). Unchanged wrapper and journey script, serial headless/rendered variants; evidence `pickup-repaired-journey/` and `pickup-repaired-journey.out`.

| Variant | Steps | Exit | Ending | Pickup | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | player_death refused; saved_level_reloaded=false | **10→11**, produced/walked/picked_up | 0 | 0 |
| Rendered | 81 | 1 | player_death refused; saved_level_reloaded=false | **10→11**, produced/walked/picked_up | 0 | 38 |

Both: one death, one loot call, 11 shared loot draws, one generated consumable, one spawned ground object and one pickup; 26 pickup walking ticks, zero retry kills / zero diagnostic retry placements. Enemy fight passes with empty missing_fight_steps and no fight failure. Neither potion lookup nor player-name-format refusal remains in enemy_fight. The existing initial diagnostic enemy placement remains; this is not normal-route combat acceptance. Disc file counts unchanged **994→994**. Script explicitly keeps rendered_pickup_verified=false (visual item parity is not established); wrapper pickup_verified=true for both. Inspected rendered `05-fight-attempt.png`: presentation shows one loot item and one picked up. No screenshot/private content committed.

Observed root cause: missing runtime potion-pointer binding prevented consumable construction; the composed strict shared formatter read sparse World memory that omitted non-player format strings, interrupting PhProp construction. Both now read the selected executable. Combined repair restores the required pickup without changing combat inputs, timing, RNG, harness, refusals or baselines. No claim that either omission alone explains every historical failed roll. The older pickup branch's absent formatter and different combat timing explain why its success did not cover these composed bindings.

Strict Godot dev matrix **54/54 PASS**, including six graphical cases (`pickup-matrix/results.json`). Full combat **2,645 checks / 0 failures**. FULL_SAVE_LOAD **300/300**, identical presentation hashes. FULL_TRACE **unchanged 36,402 bytes / 9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659**. No baseline changed.

Full CTest `-j 3` remains running and is not yet a PASS. Remaining broader journey blockers retain the existing unsupported script-trigger stop, ordinary departure/checkpoint refusal for dead Creature state, and unsuccessful saved-level reload/death probe. These are separate from the now-restored pickup gate; no full-campaign acceptance is claimed.

## Final native gate and disposition (2026-10-27)

Full `ctest --test-dir build/native --output-on-failure -j 3` **253/253 PASS**, **797.21 s**, exit 0, no timeouts and therefore no isolated timeout rerun required (`pickup-ctest.out`). This run tests the repaired runtime from `51d73589`; subsequent commits contain report updates only. Final diff check PASS; unchanged journey, wrapper and baselines. No push, PR, branch creation, sibling worktree edits, private-data commit or process termination.

Pickup binding assignment is implemented and verified, including required **10→11** in both variants. Full journey still ends refused at **80 headless / 81 rendered** steps, exit 1, saved_level_reloaded=false; existing trigger/departure/death-save boundaries remain explicit blockers to broader campaign acceptance. No missing research was filled with invented contents. No additional runtime changes were made after the verified repair.
