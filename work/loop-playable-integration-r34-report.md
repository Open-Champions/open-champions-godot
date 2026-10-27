STATUS: MORE — WAVE 34 IN PROGRESS (2026-10-26). Eight audited tips merged in order; gates pending.

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

Pending headless/rendered comparison with wave 33's 67/68 steps. Pickup must show 10→11; script-stop refusals remain explicit.

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
