STATUS: COMPLETE

# Combat constructors and Full fight, round 4 (audit FIX job)

Branch `codex/combat-ctors` in `worktrees/combat-ctors`. Continued from
`9f70852` with no rebase, push or PR. Fix commit: `856a286`. Research pins
read for this job: `t3/research-combat-ctors-4` `6baf4cd`,
`research-combat-ctors-3` `9ff6ae0` and `research-anim`. The audit is
`cursor-runs/audits/combat-ctors-r3.md`.

## Finding map

| Finding | Resolution |
|---|---|
| **C3-1** (MEDIUM) `combat_play.cpp` slot-5 Skeleton at (70, 100) | **Documented as a fixture decision.** It is not a research value, like the AntQueen's (-110, 110). The code comment, `docs/COMBAT_RUNTIME.md` ("Full fixture decisions") and this report all say so. The only traced placement is the round-5 probe's (70, 140) (`full-placement.md` 6.4, `6baf4cd`). y = 140 is off the synthetic level's flat ground (±128), which the fixture's placement check requires. (70, 100) keeps x, puts y on the flat ground, and stands on the far side of the player from the AntQueen and the Soul. First chase there is frame 282. The research's frame 123 belongs to (70, 140) and is not comparable. Placing the Skeleton makes its constructor and holder draws on the shared seed, so it shifts the seed-driven rolls and `FULL_TRACE`. That is a fixture consequence, not a traced event. |
| **C3-2** (MEDIUM) `model_getMeshJoint` answered 0, `resref_get` answered the first word | **Refused unless loaded data supplies it.** Research trace: `model_getMeshJoint` `0x179358` returns the u8 at +6 of the 10-byte mesh record (`model-texture.md`; `symbols/fmt-model.tsv`). `resref_get` `0x17377c` returns the cached data pointer, refreshed when `g_lumpGeneration` changes (`engine.md`; `symbols/engine.tsv`). In `ref_phprop.py`, `gut-chunk.md` §9 and `full-placement.md` §6.2 both are recording call-outs with no modelled result. The record and the clip pointer are loaded disc data, so neither can come from a synthetic fixture. New `Services::mesh_joint(model, mesh)` and `Services::resref(ref)`; a missing function throws `Unsupported`. The stand-ins are removed. The synthetic Full fixture supplies neither. |
| **C3-3** (MEDIUM) `demon_near` skipped slot 0 / unknown getters | **Refused instead of skipped.** `soul-run.md` ("Soul_isDemonNear") and `ref_soul_run.py` `soul_is_demon_near` call `typeid(obj)` on every box object. The skip is gone. A getter with no documented resolution (no `CT_type_id` or `Creature.v0` call-out, not a type-info getter, no class body) throws `A::Error`. `soul_is_demon_near` is now exported so it can be tested alone. The World path is unchanged: it answers through `World::State::is_demon_near`. |
| C3-4 LOW `draw_held_part` 64-bit mask | Not in the instructed scope ("trivial LOW ones" is read as docs/comment-only). Unchanged, presentation no-op. |
| C3-5 LOW `Kind::Effect` bookkeeping | Documented here: World bookkeeping, not an original class. |
| C3-6 LOW gold HUD order / flush / gore option | Documented here: `hud_tick_gold` order against object runs is unpinned, `hud_flush_gold` is never called, and gore option `0x44fdc0` stays 0 (so `gutchunk_ctor` sets bit 1). None is reached in the 300-tick trace. |
| C3-7 LOW no replay of skeleton_run / Soul states | Partly: six new `soul_is_demon_near` checks (below). Replaying `test_ref_skeleton_run.py` and `test_ref_soul_run.py` remains open. |
| C3-8 LOW callback capacity 256 | A host resource bound, not an RTA constant. No change. |

## Refusals counted, and where the Full fight then stops

- **The five scripted Full-fight scenarios reach 0 loaded-data refusals.**
  They stop exactly where round 3 did, on the existing
  `party wipe: deathTask` refusal: idle 787, attacking 694, spawned Skeleton
  818, potion 1 at 787 and potion 2 at 539. The native presentation world is
  also unchanged (`7326016685608146555/385462`).
- **Killing the Skeleton reaches the first refusal.**
  `combat-free-spot-host` kills the slot-5 Skeleton after ready tick 5
  (`World::take_damage`). That gives **1 refusal**, in ready tick 6:
  `model_getMeshJoint: no loaded mesh record service`. This is the
  `Skeleton_shatter` → `PhProp_ctor2` path. A real Full fight therefore stops at
  a Skeleton's death until a loaded mesh service exists.
- `resref_get` (`Gold_ctor2`) is refused by the same mechanism. No Full scenario
  reaches `Gold_ctor2` without loot services, so its refusal is not exercised
  in the World.

## New and changed tests

- `champions-combat-ctors-3-tests`: **208 checks** (202 before). The six
  `demon_near_cases` checks cover a Demon, a Demon with flag 8, a Player, an
  unset slot 0 refusal, an unknown-getter refusal, and `Creature.v0` being
  asked. The authored objects carry getter addresses from `combat-ctors-2.md`
  §3 and are synthetic.
- `combat-free-spot-host`: **412 checks** with the Skeleton-kill refusal.

## Verification

- Native Release, arm64 on `856a286`: **214/214** (760.04 s,
  `work/combat-ctors-4/ctest-arm64-final.log`).
- x86_64 under Rosetta, `build/native-x64` (Mach-O x86_64): **214/214**
  (1,222.41 s, `ctest-x64-final.log`; the machine was heavily loaded).
- `fixture-save`: **93 checks, 0 failures**.
- Godot dev matrix: **45/45 exited 0** (`godot-matrix-r4.log`, `godot-results.json`).
- Full combat smoke: **2,619 checks, 0 failures**. The headless trace equals the
  drawn trace. `FULL_SAVE_LOAD` reports the mid-fight save/load (Holy Strike in
  flight, saved at tick 174) matching byte for byte: **300 presents each, the
  same presentation digest `cb2309ab…d573`, and trace sha256 `9c7922b2…`**.
  Native fixture save: 19 demon checks and 2,169 Holy Shield sample points.
- **FULL_TRACE: `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  36,402 bytes, headless. It is identical to round 3.** Explanation of every
  change: there is none. C3-2 and C3-3 only replace answers on paths the
  300-tick scripted trace never reaches (no shatter, no gold, and the World
  answers the demon test through its own call-out). C3-1 changes only a comment
  and docs. The (70, 100) placement is unchanged, so its draws, its actor pair
  and the seed shifts relative to round 3 are the same. Relative to round 2 the
  trace changed because of that placement; see the round 3 report.

## What remains

- A loaded mesh-record service and resource-reference service. Without them
  `Skeleton_shatter` and `Gold_ctor2` refuse in the World. This needs data
  loaded from the user's read-only disc at run time; none was read or copied.
- The rigid-body services and the remaining round-3 refusals are unchanged.
- No private-disc acceptance result is claimed.
