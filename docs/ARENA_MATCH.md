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

Round 5 additionally offers the explicit `RequestServices` path. Its native
`Model::begin` / `start_wave` and World adapters run the established request
stages: preload deduplication, three-wave setup, wave/left/level/intro stores,
nonnegative-life snapshots, per-group shared ring draws and ordered spawn
requests. The required player/resource, spawn, camera and menu services are
fixture or host boundaries, never default-success implementations. The legacy
complete `Services::start_wave` path above remains supported separately.

All round-5 authority is **research-arena-live at `9a0c26a5`**:

| Behavior | Research path and lines |
|---|---|
| Begin preserves current wave; preload then left=3, saved=0 and start(1) | `docs/spec/arena-live-scheduler.md:10-15,30-34`; `tools/difftest_arena_live_scheduler.py:72-83` |
| Signed wave arithmetic, six-byte recipes, modulo/group split, cap and active count | `docs/spec/arena-live-scheduler.md:16-29`; `tools/ref_arena_modes.py:325-365` |
| One ring draw per group, including repeated classes; original raw seed/angle words | `docs/arena-modes.md:125-138`; `tools/ref_arena_modes.py:368-401` |
| Snapshot/intro stores and explicit construction boundaries | `docs/arena-modes.md:97-107,140-146`; `docs/spec/arena-live-scheduler.md:74-82` |
| 180 decrements plus unlock visit, 128-frame scan, pre-increment 480/960 expiry | `docs/spec/arena-live-scheduler.md:35-47`; `tools/difftest_arena_live_scheduler.py:91-138` |
| Level pointer from wrapped next recipe, orientation -1, pending copy and close(0) | `docs/arena-modes.md:77-88`; `tools/ref_arena_modes.py:404-410` |
| Later samples 29,44,149,299,2999,29999,32765 request continuation | `tools/difftest_arena_terminal.py:10-30`; `docs/spec/arena-live-scheduler.md:105-111` |
| Defeat writer 0x306f94, exact full-fade input, restart recipe zero, cancel exit | `docs/spec/arena-live-scheduler.md:113-125`; `tools/difftest_arena_terminal.py:33-79`; `docs/arena-modes.md:222-236` |

The caller supplies recipes and class/level pointers; no shipped names or tables
are included. Preloads deduplicate by class index while spawning retains every
group and ring draw. Begin currently refuses nonaligned or signed-overflowing
three-wave boundaries, missing/invalid tables and required handlers. The
non-null preload factory destruction path remains refused: argument 3 was
observed, but destruction was not executed by this research. Saved-creature
construction also remains refused on the native request path. These are host
refusal bounds, not new game rules. Missing services are preflighted; arbitrary
host callback failures can still leave effects from earlier explicit calls.

The pending-level copy service receives a destination and selected source
pointer, followed by close(0) on continuation. Restart restores active records,
calls shutdown, and requests recipe-zero's level without implying successful
loading. Cancellation clears mode B and sets session exit 1. No terminal-win
branch is added at a recipe wrap. Later samples are independently reconstructed;
continuous progression through 32765 and signed overflow are not proven.

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

`arena-scheduler-runtime` adds 480 synthetic start/preload cases against a new
immutable oracle generated by the unchanged pinned reference; it checks raw
monster levels, all class requests and ring state. It also drives actual World
grid walks and every intro/intermission visit, concrete later-level pointers,
16 defeat-to-input sequences (distinct scores, so unknown ties still refuse),
retry and unsupported-service/data/overflow cases. Existing tests and baselines
are unchanged. This is bounded native function validation, not live acceptance.
