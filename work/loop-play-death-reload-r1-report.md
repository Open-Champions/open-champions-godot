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
