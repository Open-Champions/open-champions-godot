STATUS: MORE — B1H-6 implementation and original-backed comparisons pass; full gates are running.

# Blood1 hostile actors, audit fix round 3

Starting tip: `961a4f7c8b2334940c8156a4ce2bbde87163bceb` on
`codex/port-blood1-hostiles`. Scope: B1H-6 only, from
`port-blood1-hostiles-fix.md`. No scene integration or other AI branches changed.

## Evidence and fix

Pinned original research: `research-blood1` at
`5107fcb1aa7fead20cdeca8693feaabf4cc2c4f7`, `docs/monsters-2.md:5007`
(Kobold settle, event-only hit, targeted turn, completion), `:6120`
(independent pre-state fade turn), `:6129` and `:6134` (direct height
following and grid update in attack states). The existing targeted state-7
refusal remains: its draining/special-hit behavior is not implemented.

Kobold state 6 now turns with a target regardless of the hit event. WarWraith
state 6 dispatches the descriptor/hit only on the event, then turns, follows
target height and calls grid update on every targeted frame, before requesting
the completion transition. The fade-prefix turn remains independent.
`TickInput::target_position` explicitly supplies the target position; missing
WarWraith targeted context refuses before class-specific fade/animation/attack
effects. The generic processDamage/Creature_run prefix still runs first.

Observed by executing the pinned original class bodies with synthetic inputs:
height changes by a signed 1/6 step (including overshooting a nearby target),
does not interpolate or clamp, and remains unchanged at equal heights. Portable
reference arithmetic computes the step; World position and actor height memory
are updated before the grid callback. Grid/turn/hit services are recording
stubs, not evidence of a landed hit or real spatial-grid implementation.

`tools/generate_blood1_targeted_vectors.py` checks the research revision and
original executable hash, executes the original via its existing emulator, and
writes only synthetic input/output fixtures. `native/tests/vectors/blood1_targeted.tsv`
contains 320 frames: seeds 20973/7, four classes, hit/no-hit, running/completed,
fade 98/100, and five actor/target-height pairs (up/down/equal/overshoot/negative).
Both generator and native tests assert full call order, event-only hit flags,
completion transitions, unchanged x/y and updated z visible to the grid service.
The native test also checks missing-position refusal.

## Gates

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: PASS.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/generate_blood1_targeted_vectors.py`
  with the pinned research, read-only original executable, and synthetic TSV
  output arguments: PASS, 320 original frames.
- `ctest --test-dir build/native --output-on-failure -R '^blood1-actors$'`:
  PASS, 1/1, 320 frame comparisons across both seeds plus existing regressions.
- `ctest --test-dir build/native --output-on-failure -j 3`: RUNNING.
- Three existing Godot extension rebuilds, existing 54-case development matrix,
  and scripted/unscripted 111-level sweeps: RUNNING, not yet claimed passed.
- Required FULL_TRACE: 36,402 bytes,
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`;
  fresh result pending.

2026-10-25 coordination: B1H-6-only local implementation checkpoint; no push.
Unrelated pre-existing Godot UID remains untouched. Gate logs stay ignored under
`work/blood1-hostiles/b1h6-*`; no private bytes or decoded private content tracked.
