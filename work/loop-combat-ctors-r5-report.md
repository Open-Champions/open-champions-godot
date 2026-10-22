STATUS: PARTIAL

# Combat constructors, round 5

Branch `codex/combat-ctors`, starting at `08dbc72`. Research pin:
`t3/research-combat-5` `f33d1a3260411c1ea1b9eefed29fdc47dcb57a34`
(audit PASS); its `rigidbody-init.md`, `arrow-hit.md`, `arrow-run.md`,
`staticfire-ctor.md`, and `minion-place.md` are the contract sources. All public
fixtures are authored and synthetic. No changes to `~/champions-re`, push, or PR.

## Implementation and limits

- Added exact rigid-body init, quaternion normalization/conversion, and derived
  inertia and angular-speed stores. Full World dispatch now enters the already
  ported `PhProp_run` and `GutChunk_run` bodies, and the PhProp constructor
  receives the body output and one cosmetic draw.
- Added the five-slot `Creature_setEffectTimer` and complete `LightEffect_ctor`.
  The World composes the latter for its existing kind 1/2/5/6 arrow light
  path. The Fire, Cold, Poison, and Explode arrow class chains already live in
  the separate native `skills_b` model; they have no Full World spawn route.
- Added `StaticFire_ctor`, its common init and particle push, and the executed
  prefix of `Minion_run` including held-Soul attach-point placement.
- Replayed research reference cases in `combat-ctors-3-reference`: dirty rigid
  body, exact inertia/orientation/cosmetic draw, timer slots and relink, a
  fifteen-frame arrow light, StaticFire fire-ring and neighbour branches, and
  held-Soul order and facing. Both arm64 and Rosetta binaries pass 252 checks.
  The existing `skills-b-exact` arrow-chain vectors also pass on arm64.
- Kept unexecuted StaticFire run/draw/destructor and Minion state dispatch
  refused. Full World StaticFire construction refuses at `path_blockNodes`
  without a loaded path-grid service. The synthetic Full fixture has no loaded
  model hull table. Its AntQueen blood chunk reaches
  `model_getHullPoints returned null` at ready tick 269 in the wave-12 case;
  no synthetic hull is substituted, so settling of a live bone or gold pile is
  not claimed.

## Verification

- Research synthetic Python contracts: `PYTHONDONTWRITEBYTECODE=1 python3 -m
  unittest tools.test_ref_rigidbody tools.test_ref_combat5
  tools.test_ref_arrow_run`: 38 passed.
- Native Release arm64 and `build/native-x64` x86_64 builds: passed. The
  `champions-combat-ctors-3-tests` binary passed 252 checks on each.
- Godot extension Release template_debug build: passed. Godot dev matrix:
  `python3 work/combat-ctors-5/run-godot.py`: 45/45 exit 0; includes rendered
  cases, the Full-combat smoke, and wave-12 combat extensions.
- `build/native/champions-fixture-save-tests
  native/tests/fixtures/fx_skill_recipes_synthetic.json`: 93 checks, 0 failures.
  Mid-fight native save is 422,756 bytes across 448 operations; byte-exact
  restoration and continuation pass. Godot `FULL_SAVE_LOAD`: 300 uninterrupted
  and 300 continued presents, matching presentation digests
  `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`.
  Full-combat smoke: 2,619 checks, 0 failures.
- `FULL_TRACE`: 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
  **Unchanged from round 4.** The scripted 300-tick smoke reaches neither the
  new prop run/hull path nor elemental arrow class chains. The existing kind
  1/2/5/6 light path does not enter this trace.
- Native Full fixture idle: ready tick 787; player attacking every 12 ticks
  from tick 95: ready tick 694. Both stop on the existing
  `party wipe: deathTask` refusal. Spawned Skeleton reaches 818; potion 1
  reaches 787; potion 2 reaches 539.
- Initial full arm64 CTest (`ctest --test-dir build/native -j 3
  --output-on-failure`): 211/214 passed in 1,957.50 seconds. `player-exact`
  timed out at 240.11 seconds during heavy concurrent worktree load. An
  isolated retry also timed out at 240.13 seconds. A direct invocation of
  `tools/test_player_vectors.py` then reached that script's own 180-second
  per-child timeout, with no comparison failure. The other two failures
  were old host assertions catching `combat_runtime::Unsupported`; the
  executed prop run raises `arena_monsters::Unsupported` at the missing
  loaded hull. After catching that exact refusal type, the round-3 and round-4
  host binaries pass 54 and 24 checks on both architectures.
- Final affected CTest subset (`combat-ctors-3-reference`, both combat-runtime
  host tests, `fixture-save`): **4/4 arm64** in 323.39 seconds and **4/4
  x86_64/Rosetta** in 539.00 seconds. The x86_64 binary is Mach-O x86_64
  on an arm64 host. Full x86_64 CTest and a clean final 214-case arm64 CTest
  remain unverified. Host load exceeded 350 while independent worktrees ran
  other suites; running another full fixed-timeout matrix at that load would
  not establish a meaningful pass/fail result.

No private-disc acceptance or console/emulator execution is claimed. The
remaining research-unexecuted bodies remain refused. To close verification,
run full Release CTest in `build/native` and `build/native-x64` when host load
is low, then check `player-exact` against its 240-second CTest and 180-second
trace-child limits. The Full World elemental-arrow spawn route and loaded
model hull/path-grid services remain outside this port.
