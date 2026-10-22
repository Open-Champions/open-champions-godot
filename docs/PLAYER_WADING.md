# Player wading

RTA US v2.00 only. Evidence: `t3/research-wading` at `942a6ba`
(`docs/spec/player-wading.md`, `tools/ref_wading.py`, `tools/difftest_wading.py`;
its audit passed), which cites `t3/research-combat-walk`'s `combat-movement.md`.
It supplies the player's `water_disturb` arguments, the water-depth test, the
depth band, the footstep and the finding that wading has no speed effect. Those
were what `codex/water-ripples` refused. Nothing is taken from outside that pin;
every constant in `native/include/champions/player_wading.hpp` carries the
instruction address the research gives.

## What is ported

`champions::simulation::rta::player_wading::step` is `Player_move` 0x1b4020
step (f), in the original's call order:

- In water (`+0xa8` bit 61): `damage_apply` on itself on sixteenth frames of a
  level with word `0x44fe08` bit `0x1000`; then `water_disturb(world,
  (x - 0.0, y, z - 12.0); 300.0, 12.0)` every frame while the stick speed is at
  least 0.01, else on sixteenth frames; the bit clears when that touches no chunk.
- Out of water, on frames with `g_simFrame & 3 == 0`: `water_test(12.0)`. A hit
  takes one shared-seed draw, sets the bit, plays the splash (volume 1.0
  airborne, else 0.4), ripples at 3000 (airborne) or 1000, radius 24, ignores
  that ripple's result, and, airborne, bursts at the chunk's minimum z.
- The depth band is not coded: it is the box each service queries
  (`creature_water::Service`). Entry: feet-to-surface `dz` in [-1, 60], 13 from
  the chunk edge. Staying: `dz` in [-13, 48], 12 from the edge. The deep band
  (48 < dz <= 60) enters, leaves on the next wading ripple and enters again; it
  writes no ring.
- `foley()` / `footstep_flags()` / `cosmetic_draw()` are the `Player_footstep` /
  `sound_playFoley` choice (water variant 1 or 2, else by item flags), drawn from
  the cosmetic seed 0x404460, never the shared seed.
- Wading does **not** change speed, facing, blend or velocity. Nothing here
  touches them (research section 7, 1,000 executed A/B cases per seed).

## Where it runs

- **Composed combat runtime** (`combat_runtime_move.cpp`, `player_water`). The
  `Services` gain `water_disturb` (point, strength, radius -> chunks touched) and
  an optional `water_height`. The flat fixture is a dry world (touches 0). A
  wader whose service is missing refuses instead of reading as dry. The entry
  splash and burst are recorded as `presentation:` events. The `Player_run` state 7
  and 0xb ripples reach the same service (result ignored). Any real level still
  uses the fixture's flat dry ground, so the Full fixture itself never meets water.
- **Real levels in Godot: REFUSED by default.** The diagnostic control walk has
  no `Player_move`, so the step's inputs are not observed: `g_simFrame` (the `& 3`
  and `& 15` gates, research section 3), the stick speed `s` (set to 1.0 by
  Player_move step (e), section 13) and the airborne byte. The research also
  lists the order of the ripple against the water tick as unknown. The scene
  therefore does not step the player's wading; `water.wading_refused` counts the
  ticks it would have. `real_world::Wader` (the same `step` on the level's WORLD
  water image) stays as a library type and behind the explicit `--wade-diagnostic`
  opt-in (`RealWorld.wade_diagnostic`, off by default). The opt-in steps once per
  observation, at the observed tick, ahead of that frame's water tick
  (`water.wading_order_ripple_before_tick` is recorded; an authored diagnostic
  choice, not research), with frame = ready tick, `moving` = the feet moved since
  the last observation, never airborne, skipped catch-up ticks counted in
  `water.wading_skipped`. Counted, not applied: the entry's shared-seed draw and
  the hurting-water damage. A missing `water_disturb` service refuses in
  `Player_move` and in `Player_run`'s ripples alike.

## Tests

- `player-wading-exact`: replays 5,191 cases of `ref_wading.py` (regenerate with
  `tools/generate_player_wading_vectors.py RESEARCH_DIR native/tests/vectors`):
  the 2,652-case band sweep (three feet heights, entry and wading, `dz` and edge
  distance in quarter steps), 2,400 call records (widths 384/120/768, five
  heights, frames, stick, level flag, cut-skip byte: bit, order and arguments of
  the calls, shared seed, ripple grid cells), the seven 32-frame sequences of
  research section 9, and 132 footstep choices. It runs against the real
  `creature_water::Service`. The manifest pins the research commit and the SHA-256
  of the five research files.
- `player-wading-runtime`: whole ticks of a Full player over a synthetic chunk:
  entry, sixteenth-frame ripples, deep band, leaving, the missing-service refusal,
  and the walk in water equal to the walk on land.
- `real-world-adapter`: `Wader` on the fixture tile.

## Not covered

- The footstep sound names and the splash burst are presentation; their strings
  and particle definitions are not in the pin, and the cosmetic seed's start
  value is unknown, so no footstep audio is changed.
- What level flag `0x1000` means, scripts that force the in-water bit, and the
  whole-word stores of `+0xa8` listed in research section 8.
- The order between the player's ripples and the water simulation tick.
