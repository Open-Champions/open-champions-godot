# Bounded RTA arena match mechanics

`champions-arena-match` is portable C++17, using the existing raw32 arithmetic
and shared creature memory. Its contracts are the committed research in
`research-arena` `17810bb` and `research-arena-match`
`d4f58da6377f1011068ba80d3608dbe1e3149370`: `docs/arena-modes.md`,
`docs/spec/arena-match-core.md`, and round 11 of
`docs/spec/arena-match-probe.md`. The round-9, round-10 and round-11 audits
are PASS. The vector manifest pins the specific reference/doc blobs.

The module implements the score block, signed lives and halfword wrap,
player/pet/single-player kill ownership, prediction exclusions, life grants,
overlay snapshot arithmetic, and prompt fade. `Creature_onDeath` now reaches
`arena_addKill` in the Full combat World. Its player death body now calls the
documented mode-B life writer. The existing mode-A mana/skill-orb credit
refusal remains. There is no fixed Goblin gold, experience or item reward
added: the audited assisted deaths call the existing loot generation path,
and do not establish a collected reward.

Caller-supplied recipes produce the documented wave budget, group split and
monster-level arithmetic. No shipped recipe or class-name table is embedded.
Construction and all of its shared-seed draws belong to `start_wave`, an
explicit required service. A recording service observes a request; it does
not create actors or advance the wave fields.

The bounded task covers the 181 intro visits, the 128-frame empty-grid scan,
the pre-increment 480/960 intermission threshold, same-level next-wave and
final-wave level requests, mode-A signed kill limits, mode-B countdown/life/HP
survival and positive-snapshot retry, defeat, and full-fade confirm/cancel.
Preparation follows screen effect 15, preparation service, original grid walk,
saved-object classification, and startWave. In particular `0x612b` is saved
despite its death bit, and alive players are skipped. The caller supplies a
separate array capacity; an existing game field is never used as a host bound.
Confirm validates and copies full `0x4c30`-byte saved records, then calls
shutdown and the restart-level service. Cancel clears mode B and sets exit 1.

These are bounded function contracts, **not a playable arena match**. The
current World still refuses PvP in its frame runner; it has no complete Goblin
constructor/holder, real player-record/equipment setup, wave constructor, or
Godot mode-B session. Its explicit task adapter uses the native grid walker
(off-grid, grid A, grid B) and move-off-grid. Destruction remains an explicit
service: the audited saved-object probes check the target/self but do not pin
the destructor flags, so the adapter does not substitute another task's flags.
The walker follows `ref_objects.py` at the same research pin. Task services for the remaining
operations must be supplied explicitly. Gold purchases, missing UI/resource/
saved-record services, and the unestablished original qsort tie order refuse
with diagnostics. Level services represent a pending-level request; they do
not prove a successful load. The renderer does not show new arena results.
The task requires a zero purchase-candidate count at `0x4f8420`; any nonzero
count refuses before task processing, including candidates without enough gold.

Round 11 executes assisted kills and the first wave handoff in the original
function fixture. It explicitly leaves normal input-only kills, newly spawned
creature ticks, a later completed wave, a displayed result and a live match
unverified. Mode-B empty waves start intermission; three waves request another
level. There is no invented final mode-B victory condition.

`arena-match-host` checks the task branches and service order with authored
memory/grid fixtures, including missing-service refusals. It checks the real
World's base death-to-score callback separately. `arena-match-exact` compares
checked-in synthetic score, death, callback, overlay and recipe results from
the pinned Python references. Neither test claims real arena gameplay.
