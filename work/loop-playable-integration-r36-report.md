STATUS: MORE — WAVE 36 GATES PENDING (2026-10-27). Three audited tips integrated; no playable campaign acceptance claimed.

# Playable integration wave 36 — 2026-10-27

## Scope, evidence and merges

Assigned clean `codex/playable-integration` at wave-35-verified `0d28998e`. Read wave 35/34 reports, repository rules, evidence workflow and all seven requested audits before merging. Full no-ff merges in requested order; no omissions or aborts. Private disc read-only, counts only; raw evidence ignored under `work/playable-wave36/` and `work/private/w36-*`. No push, PR, branch creation, sibling edits or process-name termination.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-progression` | `c12ecc31` | `play-progression-r1.md` | `bc632bdc` | none |
| `codex/play-loading-readiness` | `4dbee736` | `play-loading-readiness-r1.md`, `-r2.md`, `-r2-fix.md` | `08faa5f6` | native Host includes |
| `codex/play-strife-drundar` | `da3c1734` | `play-strife-drundar-r1.md`, `-r2.md`, `-r3-fix.md` | `27fbd8af` | controller, scene, Host and build links |

## Conflict resolutions

- Loading: retained both level-object-state and loading-equipment includes; all incoming timing, publication and refusal checks retained.
- Mission 1: combined save-directory and diagnostic APIs; linked level-state, equipment and Mission 1 source exactly once. Combined complete-command/refusal-reason behavior with Mission 1 refusal stops. Loading deferred reset remains before the Mission 1 start guard. Preserved mission-2 complete-command setup after checked load. Retained both Mission 1 scene labels and existing script-refusal display.
- Attachment retains refused preStart stop before adoption, dialog, start and object parsing; extended that same guard over the existing restored-object-field block so it cannot mutate after a stopped Mission 1 preStart. No automatic diagnostic restoration. All prior pickup, mission-2, options, arena, horsemen and strict-log work retained.

## Non-merge code changes

None at merge checkpoint. Any later source correction will be listed separately.

## Gates and evidence

Native Release build running, parallelism 4. Other requested gates pending.

## Pending full CTest record

Full `ctest --test-dir build/native --output-on-failure -j 3` is **PENDING**, not started at this checkpoint. Result will be committed separately; any timeout will be rerun alone and both results retained.

## Full scripted journey

Pending; compare wave 35 80/81 steps, inventory 10→11. Full progression, unknown arrival command list/resource transformations, continuous Mission 1 execution and successful death reload remain refused/unverified.

## Exact command families

Same wave-35 drivers/method with wave-36 output directories; native Release and universal libraries parallelism 4, full CTest parallelism 3. Strict matrix captures, timeout 1200; fresh full-combat log supplies combat/save-load/trace gates. Private sweeps serial four slices, 60 ticks. Unchanged journey driver serial headless/rendered.

- Native Release configure/build **PASS**, `-j 4`, exit 0 (`configure.log`, `build.log`). All three tips verified ancestors; no unmerged paths; range diff check PASS.

- native-ai: **PASS**, exit 0, 2.67 s. Counts-only output: `No probe summary`. Labelled diagnostic placement/hit; no normal-route or respawn claim. Evidence `native-ai.log`.
