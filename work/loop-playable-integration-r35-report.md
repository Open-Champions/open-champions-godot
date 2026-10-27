STATUS: MORE — WAVE 35 IN PROGRESS (2026-10-27). Gates pending.

# Playable integration wave 35 — 2026-10-27

## Scope, evidence and merges

Assigned clean `codex/playable-integration` at wave-34-verified `a68504da`. Read wave 34 and 33 reports, repository rules and all four PASS audits before merging. Full no-ff merges in requested order; private disc read-only, counts only. Raw evidence ignored under `work/playable-wave35/` and `work/private/w35-*`. No push, PR, branch creation, sibling edits or process-name termination.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-options` | `679ec562` | `native-options-r1-fix2.md` | `b261c31d` | frontend documentation and capture checks |

## Conflict resolutions

- Options: retained both character-creation research pins and options pin; capture retains model-render check and refusal of untraced slider artwork. All other changes merged automatically.

## Non-merge code changes

Post-merge correction: automatic arena overlap duplicated creature_on_death declaration/definition. Removed duplicate declaration and unguarded adopt.cpp definition; retained api.cpp implementation with Full/nonplayer/creature guards and identical model call. Initial Release build failed at duplicate declaration; no checks weakened.

## Gates and evidence

Pending. Live arena match stays refused; native/synthetic success is not campaign acceptance. Wave-34 comparison is 80/81 steps, pickup 10→11; broader trigger/loading/checkpoint/death-reload refusals remain.

- Arena merge: retained both arena-mode and bounded-match documentation, and all campaign/Blood1 libraries alongside arena-match. Live PvP/frame and unbound match services remain refused. Merge SHA recorded next milestone.

- Arena merge commit `14e8f820`.
- Horsemen: retained hud_view and exactly one loaded/catalogue declaration each; combined all runtime link dependencies. Reviewed automatic runtime merges: audited property occurrence/null-list/key refusals, name registration/removal, explicit missing display-name service refusal and opt-in mount initialization coexist with newer integration services and destruction handling. No actor-lifetime algorithm introduced here; shared helper from mission-2 follows next.

- Horsemen merge commit `d553d0fe`.
- Mission-2: retained Chest and mission-2 dispatch and adoption; retained shared seed/starting/Chest probe flags plus counts-only flag, and both pickup and mission-2 resource lumps. Campaign and mission-2 registries delegate reconciliation/registration to the single audited actor_lifetime.hpp helper; mission-2 additionally checks authority identity. No duplicate lifetime algorithm retained.

| `codex/native-arena` | `486d5769` | `native-arena-r4.md` | `14e8f820` | roadmap, simulation libraries |
| `codex/native-horsemen` | `bbaf93df` | `native-horsemen-r7.md` | `d553d0fe` | fixture declarations, runtime links |
| `codex/port-mission2-hostiles` | `8595fcf5` | `port-mission2-hostiles-r2.md` | `ab8a0607` | fight and probe additive overlaps |

- Native Release configure PASS; build retry PASS (`build-final.log`, -j 4), after the recorded duplicate adapter correction. All four audited tips verified ancestors; no unmerged paths and diff check PASS.

## Pending full CTest record

Full `ctest --test-dir build/native --output-on-failure -j 3` is **PENDING**, not started at this commit. Result will be recorded separately; any timeout will be rerun alone and both results retained.

- native-ai: **PASS**, exit 0, 1.88 s. Counts-only output: `No probe summary`. Labelled diagnostic placement/hit; no normal-route or respawn claim. Evidence `native-ai.log`.

- native-death: **PASS**, exit 0, 1.13 s. Counts-only output: `death_check actors=78 ticks=600 failed=0 dead=1 zero_hp=1 wipe_timer=0 slot_timer=0 task=1 phase=1 fade=128 native_clip_started=1 native_clip_steps=239 diagnostic_hit=1 retained_record_fields=1 same_dead_body=1 reload_refused=1`. Labelled diagnostic placement/hit; no normal-route or respawn claim. Evidence `native-death.log`.
