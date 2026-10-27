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

None.

## Gates and evidence

Pending. Live arena match stays refused; native/synthetic success is not campaign acceptance. Wave-34 comparison is 80/81 steps, pickup 10→11; broader trigger/loading/checkpoint/death-reload refusals remain.

- Arena merge: retained both arena-mode and bounded-match documentation, and all campaign/Blood1 libraries alongside arena-match. Live PvP/frame and unbound match services remain refused. Merge SHA recorded next milestone.

- Arena merge commit `14e8f820`.
- Horsemen: retained hud_view and exactly one loaded/catalogue declaration each; combined all runtime link dependencies. Reviewed automatic runtime merges: audited property occurrence/null-list/key refusals, name registration/removal, explicit missing display-name service refusal and opt-in mount initialization coexist with newer integration services and destruction handling. No actor-lifetime algorithm introduced here; shared helper from mission-2 follows next.

- Horsemen merge commit `d553d0fe`.
- Mission-2: retained Chest and mission-2 dispatch and adoption; retained shared seed/starting/Chest probe flags plus counts-only flag, and both pickup and mission-2 resource lumps. Campaign and mission-2 registries delegate reconciliation/registration to the single audited actor_lifetime.hpp helper; mission-2 additionally checks authority identity. No duplicate lifetime algorithm retained.
