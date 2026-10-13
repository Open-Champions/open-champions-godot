# Creature water services: contract blocker

The water service cannot yet be implemented exactly from the published Python
contracts. This evidence check leaves the existing standalone creature body,
dry Session policy, static WORLD refusal, fixtures and vectors unchanged. No
Session water policy or native water module is established by this report.

## Evidence checked

The supplied sibling `../../champions-return-to-arms` is the reconstruction
repository. Its README and ORIGIN link the separate research repository,
[champions-return-to-arms-re](https://github.com/Open-Champions/champions-return-to-arms-re).
The pinned contracts were read from immutable Git objects in an ignored bare
repository under `work/water-research/`; no reconstruction output or game assets
were used.

- `tools/ref_creature.py`, commit
  `a0d89502ce1431375a6fc045b91a73039114e6b9`, SHA-256
  `debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7`.
- `docs/spec/creature-tick.md`, same commit, SHA-256
  `bf87bd76398c2ab0df6102dcf9ffeccb6a384a3389d30796e253e6f73e399d4e`.
- `tools/ref_world.py`, commit
  `6831458ee7ee12bb166d1dce4eb8e4340095bc48`, SHA-256
  `30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8`.
- Research HEAD examined on 2026-10-13:
  `356add27d50e50972b2a671b31e8f4ec93e6778c`. Its `ref_world.py` also has no
  water service. Its `ref_creature.py` still delegates both services.
  `tools/difftest_creature.py::on_water_test` and `on_water_disturb` return
  randomized truth values rather than execute chunk queries or ripple writes.

## Established creature bookkeeping

`ref_creature.Model._water` and creature-tick section 8 specify this order:

1. Creature flag `0x100000` clears bit 61 and bypasses all services.
2. With bit 61 already set, disturb with raw arguments `0x43960000` (300)
   and `0x41400000` (12). It runs when speed `+0xb8` is not below
   `0x3c23d70a`, or when the frame's low four bits are zero. A zero return
   clears bit 61; skipped calls leave it set.
3. Without bit 61, query only when `(frame ^ signed_byte(+0x230)) & 7` is
   zero. Call `water_test(world, position, 0; 0x41400000)`.
4. A nonzero query sets bit 61, advances the object's own `+0x44` RNG once
   without using the draw, formats splash number 1, finds its resource, plays
   the sound at raw volume `0x3f199999`, then disturbs with `0x44bb8000`
   (1500) and `0x41c00000` (24). The shared World RNG is not advanced.

These rules already exist in `native/src/creature_movement.cpp::water_body`.
Their exact callback-vector tests establish caller behavior, not the services.

## Established service notes and missing evidence

At the examined research HEAD, `docs/engine.md` identifies `water_test` at
`0x189438` as testing a water chunk with an optional height result, and
`water_disturb` at `0x189514` as disturbing 32 by 32 grids and returning a
chunk count. Ledger P14 says chunks count even without a ripple grid and the
return is forced to zero while `g_cutSkip` is set; grid allocation is untraced.

`docs/formats/world.md`, “Water chunks (flag 0x80)”, records a 0x90-byte
payload: four signed neighbor indices, a runtime simulation pointer at +8,
and a 128-byte mask at +0x10. Mask meaning is inferred and bit order is
explicitly unknown. It says the queried height is chunk minimum z, which
can differ from the drawn height. Tile size is a measured common case,
not a universal constant.

To author exact services and independent generated vectors, the research
contract must supply:

- Water chunk selection/traversal, ordering, duplicate handling and any
  capacity limits; exact XY/Z boundary and radius comparisons and their
  arithmetic order; mask interpretation or an explicit rule that queries
  ignore it; optional height-write and overlapping-chunk behavior.
- Disturbance chunk selection and return counting, precise cut-skip execution
  order, coordinate-to-cell conversion, affected cell selection, strength
  calculation, numeric widths/overflow, grid layout and ordered writes,
  including absent grids, neighbors and edge cells.
- Explicit supplied runtime state and ownership for those operations, and
  executable pinned Python bodies that produce complete query results and
  disturbance state observations from authored synthetic inputs.

No rectangular approximation, typical tile-size assumption, constant callback
return or authored ripple formula can fill those gaps under the exact-contract
requirement. The conditional Session integration therefore remains blocked as
well. Original rendering, dynamic contacts and Norrath behavior remain separate.

## Validation boundary

The three pinned source/specification hashes above were checked with
`git show <commit>:<path> | shasum -a 256`. Searches of pinned `ref_world.py`,
current reference models, the creature differential callbacks and the linked
research notes establish the missing service contract. No native code changed;
native build, CTest, platform traces and private-file validation were not run
for this documentation-only stop. No new acceptance counts are claimed.
