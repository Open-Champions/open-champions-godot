STATUS: MORE — implementation in progress; pending full CTest.

Scope: assigned play-progression worktree, codex/play-progression, wave-34
base a68504da. No push or PR. 2026-10-27.

Survey: wave 34 carries the live player record, inventory spans and authored
campaign companion into a replacement script host. XP/stat spending and their
companion are already integrated. Mission completion and availability are
separate; the menu reads runtime executable rows. Save/load uses authored native
companions, not a retail memory-card image. Mission-2 entry alone invokes the
researched act-change path; other carried destinations currently retain the old
act. Nonempty/imported cross-act inventory remains refused.

Evidence: research-quests@7efd3ce docs/quests.md:266-298 establishes prefix/current
act lookup and the guarded reset independently of mission number. This change
extends that existing path to other table-proved cross-act destinations, without
adding a mission order or automatic unlock. Implementation and focused regression
validation pending. Full Release, CTest, Godot matrix, FULL_SAVE_LOAD, FULL_TRACE
and both journey gates pending. Pickup 10→11 and baselines remain required.
