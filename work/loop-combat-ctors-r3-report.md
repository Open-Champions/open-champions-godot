STATUS: PARTIAL

# Combat constructors and Full fight, implementation round 3

Branch: `codex/combat-ctors` in `worktrees/combat-ctors`. Continued from
`a0d7118`; no rebase, push or PR. Research pins:

- `t3/research-combat-ctors-3` `9ff6ae03cc9228d98bc2f6fb7f8436316ea8b8c1`;
  `docs/spec/skeleton-run.md`, `soul-run.md`, `gut-chunk.md`,
  `combat-death.md`, `weapon-holder.md`, `full-placement.md` §6,
  and their reference models and difftests.
- `t3/research-combat-ctors-4` `6baf4cd17abd7aa1980fb4287ac0ce87ef61ef5e`;
  `phprop-run.md`, `gold-run.md`, `arrow-run.md`,
  `weapon-holder-slots.md`, `attach-matrix.md`, `antwave.md`,
  the prop destructor reference and their tests.

The research itself uses an interpreter and synthetic cases. These ports and
the public tests use authored fixtures; they do not prove play on the user's
disc. No extracted clip lengths, root motion, retail bytes or assets were
committed.

## Implemented

- `Skeleton_run` (including state 8 bow release), the ally-in-line-of-fire
  body and two callees, `Skeleton_onNearbyEvent`, `Skeleton_hasShield`,
  `Soul_run` states 0–0x22, and the executed `Creature_setCharm`.
- `Skeleton_shatter`, holder slot 11's part drop, the GutChunk and PhProp
  constructor chain, `Gold_ctor2`, weapon holder slots 1–24 and element
  setters. The tested constructor draws are one shared seed for each gut
  chunk, bone and gold pile, plus the gut chunk's seven own draws. A Skeleton
  with an element draws once for that element.
- Gold pile run and touch, delayed HUD credit of the full amount to the
  toucher's `+0xe4` after 43 ticks, held Soul attach matrices, AntWave
  constructor/run/draw/destructor, and the prop destructor chain.
- Standalone `PhProp_run` and `GutChunk_run` bodies with rigid-body
  integration, rest replacement and the one shared draw at rest. Focused
  native tests cover flight gravity, life, rest and a GutChunk decal. The
  composed World does **not** dispatch these run slots yet; see below.
- The Skeleton's plain `MissileWeapon_ctor` state-8 arguments now compose
  with the existing native missile flight/touch/destructor. The native
  MissileWeapon body predates this round; the elemental arrow classes still
  need composition.
- The Full fixture now places the Skeleton in slot 5 at (70, 100).
  AntQueen remains in slot 4 at (-110, 110), Soul in slot 6.
  Nearby-event dispatch preserves the base callback's argument shape while
  the Skeleton body reads its executed three inputs. The host callback
  capacity was raised from 128 to 256 for non-Full fixtures to hold the
  expanded service table; it is a host resource bound, not an RTA constant.
- The AntQueen bleed path now reaches the composed `GutChunk_ctor`; the
  spawned chunk reaches an explicit physics-run refusal on the next tick.
  The Full physics-prop and gut-chunk run slots and destructors have their
  pinned table entries. The wave-12 Godot case observes the constructor at
  ready tick 268 and the run boundary at 269.
- The new AntWave, prop and Gold table slots install when their constructors
  execute. Full fixtures without those objects retain their earlier memory
  image; the older `combat-runtime-2-exact` vector passed on both arm64 and
  x86_64 after this fix.
- Direct host GutChunk spawning now registers the constructor callbacks and
  table slots. The older round-3/4 host cases assert the executed constructor
  and explicit physics-run boundary, and the round-5 host case asserts the
  executed Skeleton chase and holder state. All three pass on both native
  architectures.

Milestone commits, local only:
`a5a4c4a`, `de9b88a`, `f4f4663`, `956be35`, `73020a4`,
`dd96f3e`, `c1a3dca`, `444110b`, `4cdea04`, `65538d7`,
`cb8d354`, `fc2004b`, `c9558cd`, `541d74c`.

## Contract and save evidence

- Pinned Python reference unit tests: round 3 **52/52**, round 4
  **110/110**, with `PYTHONDONTWRITEBYTECODE=1`.
- Native `champions-combat-ctors-3-tests`: **202 checks** on arm64 and x86_64,
  including holder, charm, shatter, constructors, Gold, prop destructors,
  attach matrices, AntWave and the focused prop frame cases.
- Native fixture save: **93 checks, 0 failures**. The 300-tick Full fight's
  mid-flight save/load continuation remains byte-exact; presentation world
  `7326016685608146555/385462`, 19 Soul demon checks and 2,169 Holy
  Shield sample points. The report's hash below is Godot's separate
  `FULL_TRACE`, not this native presentation digest.

Research `full-placement.md` corrects the serial-1 probe's first Skeleton
chase to **frame 123 / ready tick 124**, rather than frame 32 (serial 31).
The authored Full fixture starts its slot-5 Skeleton at (70, 100); in that
scenario it first enters state 3 at **frame 282**. These are distinct
placements and timing conditions.

## Full fight distances

The native fixture advanced up to 4,000 ready ticks:

| Scenario | Last ready tick | Stop |
|---|---:|---|
| Idle Full fight | 787 | `party wipe: deathTask` |
| Player presses attack every 12 ticks from tick 95 | 694 | `party wipe: deathTask` |
| Spawned second Skeleton at tick 3 | 818 | `party wipe: deathTask` |
| Potion 1 | 787 | `party wipe: deathTask` |
| Potion 2 | 539 | `party wipe: deathTask` |

## Verification

Both native build directories use Release. The x86_64 target is a Mach-O
x86_64 binary run under Rosetta; the arm64 target is Mach-O arm64.

- arm64 full CTest on `541d74c`: **214/214 passed** in 730.82 seconds
  (`work/combat-ctors-3/ctest-arm64-hostfix.log`).
- x86_64 full CTest on `541d74c`: **214/214 passed** in 1,044.47 seconds
  (`work/combat-ctors-3/ctest-x64-hostfix.log`). The prior `c9558cd`
  aggregate was 210/214: the three now-corrected host cases failed and
  `player-exact` timed out at 240 seconds under concurrent load. In the final
  suite, `player-exact` passed in 154.90 seconds.
- Godot dev matrix on `541d74c`: **45/45 exited 0**
  (`work/combat-ctors-3/godot-matrix-541d74c.log`,
  `work/combat-ctors-3/godot-results.json`).
- Godot Full combat smoke on `541d74c`: **2,619 checks, 0 failures**. Headless and drawn
  300-tick traces match; the mid-fight save/load trace and its 300-draw
  presentation state match byte for byte.
- Godot `FULL_TRACE`: **`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**
  (36,402 bytes, headless). The prior round's value was
  `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`
  over 31,622 bytes. The trace format is 300 lines of ready tick, shared
  seed, player HP and each actor's present/HP pair. The new slot-5 Skeleton
  adds one actor pair per line. Its constructor and equipped holder consume
  their documented shared draws, shifting later seed values and seed-driven
  combat outcomes. The native 300-tick
  fixture observes its chase at frame 282. Gold and the physics-prop rest
  path are not exercised by that scripted trace. AntQueen's slot and
  placement are unchanged. No clip length or root-motion data was added.

## Remaining work and refusals

- `rigidbody_init`, `quat_fromMat3` and `rigidbody_updateDerived` are
  recording services in the composed World. Round-4 research executed those
  bodies during prop construction but did not model or specify their full
  outputs; `phprop-run.md` §11 explicitly lists that boundary. Enabling the
  World prop run on zeroed bodies would change physics and rest timing, so
  `PhProp_run` and `GutChunk_run` remain standalone ports. An executed
  rigid-body initialization contract and terrain sweep service are required
  before they can run in Full.
- Fire, Cold and Explode arrow construction/flight/destruction have not been
  composed into the World. The existing plain missile stick behavior is
  player-owned; the pinned research says a Skeleton-owned plain arrow never
  sticks. The cold and exploding arrow **hit** paths remain refused as
  instructed; `StaticFire_ctor` and Minion-run held-Soul placement remain
  refused. The resting `Prop_run` and `ItemProp_run` also remain refused
  because the pinned prop research does not execute them.
- Contract replay is currently focused rather than exhaustive for the new
  prop frame and AntWave constructor paths. The reference difftests in the
  pinned research passed there, but the native replay does not cover every
  generated reference case. The uncomposed paths are not reported as native
  verified.

No private-disc acceptance result is claimed. Clip lengths and root motion
must be loaded from the user's read-only disc at run time; the checked-in
synthetic Full fixture timings are authored test inputs only.
