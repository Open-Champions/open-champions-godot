STATUS: IN PROGRESS

Assigned branch `codex/play-death-reload`, base `76c5b35e`.
Research verified at `0e3afdb8ce3cdf79746f535d8a2654db37c9e97b`,
`research-death-reload/docs/death-reload.md:12-62,75-97`; audit PASS.

Full CTest: PENDING. Release build started with -j 4; full suite will use -j 3,
with any timeouts rerun alone. No acceptance gate or fixture changed.
Automatic reload restores the checkpoint buffer, not a card slot or a guessed
spawn. Restart is the separate current-level act-lookup -1 branch. Whole-world
Creature/portal effects and continuous death reload remain unknown and refuse.

Inherited runtime review: b1753453 retained. It uses the actual death Flow and
save loader, compares the whole restored/current record, retains literal != -1
branching and refuses missing camera before player/request mutation. Class
reconstruction is being reviewed separately. Campaign reload now invokes that
loader on shared memory when an owner explicitly supplies a valid checkpoint;
there is no checkpoint synthesized from the dying world. After a request it
stops before another old-world tick because world reconstruction is Unknown.
Full CTest remains PENDING; fresh acceptance results will replace this status.

Review follow-up: DR-1 (missing Prop resource-name copies) and DR-2
(post-request old-world HUD gold credit) were fixed. Prop restores the three
bounded name fields; the shared World now stops immediately after a death
request, before HUD/frame/alive-list effects, and refuses continued ticking.
A real World/Flow/save regression retains pending credit 8 without adding 1 to
the restored record. A speculative clear of nonserialized Creature charm was
removed: nonzero constructor charm without the optional word now refuses.
Focused tests: ps2-save, ps2-class-restore, player-death-flow 3/3 PASS.
Fresh full CTest after these source fixes is PENDING (the earlier suite is
still running and will be recorded separately).

Full CTest (recorded by the orchestrator, 2026-10-28 ~00:10, after the job's provider stopped on its usage limit): `ctest --test-dir build/native --output-on-failure -j 3` on 435e1740 — **258/258 passed, 881.13 s**, no timeouts (log `work/death-reload/ctest-r1.log`). Strict Godot matrix, FULL_SAVE_LOAD, death probe, FULL_TRACE and both journeys for this revision are still PENDING.
