STATUS: PARTIAL — audit fixes pass; 111-level sweep regresses against wave-24 base

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

At the original round-5 checkpoint, no private-disc acceptance or
console/emulator execution was claimed. The audit-fix sweep below adds a
private-disc load/play measurement. The
remaining research-unexecuted bodies remain refused. To close verification,
run full Release CTest in `build/native` and `build/native-x64` when host load
is low, then check `player-exact` against its 240-second CTest and 180-second
trace-child limits. The Full World elemental-arrow spawn route and loaded
model hull/path-grid services remain outside this port.

## Audit fixes (2026-10-22)

Audit: `combat-ctors-r5.md` (FIX: one medium, three low). The fix starts from
`5c27a2c` and retains the round-5 research pin above.

- **R5-1:** The Full World `obj_addToAlwaysDraw` service now sets flag `0x2`
  at object `+0xc` after linking it to `g_alwaysDrawList`. Source:
  research-combat-5 `tools/ref_objects.py:add_to_always_draw` and
  `docs/spec/object-system.md:37`. The `path_blockNodes` refusal is retained.
- **R5-2:** The exact rigid-body words were regenerated from
  research-combat-5 `tools/ref_rigidbody.py` at `f33d1a3`: `rb_init` on a
  0x98-byte dirty block with cosmetic seed 12345, mass 2, factor 1, and box
  sizes (2, 3, 4) yielded quaternion words `0x3f7ff25e`, `0x3ca71719`,
  rotation words `0xbf7fc978`, `0xbd270e33`, and seed `0x05391c44`.
  The C++ test now computes the seed from the documented LCG operands. The
  StaticFire `+0x44` golden `0xd1ccf6e9` was reproduced with
  `tools/ref_staticfire.py` using the C++ fixture's synthetic GameObject
  callback: shared seed 0, one shared draw copied in full to `+0x44`, then
  the two constructor own draws. The sibling `test_ref_*.py` tests verify
  formulas and draw counts, not these exact words; the C++ comments now say so.
  The Light `+0xb0` word `0xc07fffff` was also reproduced by
  `test_ref_combat5.py`'s Light fixture through `ref_arrow_hit.py` with a
  0.25-second life; its Python test uses an approximate float comparison.
- **R5-3:** The missile kind 1/2/5/6 light's follow and tag zero values are
  labeled as inference. `combat-round3.md:111-119` pins its colors and stack
  floats; `arrow-hit.md` section 3 pins those zeros only for Explode.
- **R5-4:** `symbols/runtime.tsv:500` pins `StaticFire_typeinfo` getter
  `0x3c7318` and descriptor `0x9083c0`. The World comparison by descriptor
  identity is explicitly labeled an adapter, since the RTTI equality body
  was not executed.

The optional arena-match VIF hull binding (`c1e157d`) was left for a later
round. The current synthetic fixture still has no hull table, and adding
that binding would require a separate model-table and prop-run validation.

### Audit-fix verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: passed (arm64). A final rebuild of the
  comment-only test change passed.
- `ctest --test-dir build/native -j 3 --output-on-failure -R
  '^(combat-ctors-3-reference|combat-runtime-3-host|combat-runtime-3-exact|combat-runtime-4-host|combat-runtime-4-exact|fixture-save)$'`:
  **6/6 passed**. `ctest --test-dir build/native -j 1 --output-on-failure -R
  '^player-exact$'`: **1/1 passed**, 204.24 seconds, after the earlier
  round-5 parallel and isolated timeouts. This is an isolated pass, not a
  clean rerun of the 214-case suite.
- `build/native/champions-fixture-save-tests
  native/tests/fixtures/fx_skill_recipes_synthetic.json`: 93 checks,
  0 failures; Full mid-fight save 422,756 bytes across 448 operations and
  byte-identical continuation.
- `cmake --build build/godot-extension -j 4`: passed.
  `python3 work/combat-ctors-5/run-godot.py`: **45/45** commands exit 0;
  Full combat has 2,619 checks, 0 failures. `FULL_TRACE` remains 36,402
  bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
  `FULL_SAVE_LOAD` matches the uninterrupted trace and the presentation
  digest `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`
  at 300/300 presents.

**Sweep regression relative to base `2e57376`:** Four headless slices with
`--all --levels=0 --ticks=60 --scripts`, the same read-only disc, light and
recipe inputs as wave 24, tried **111**, loaded and played **108**, passed
116 checks with 0 failed checks. All slices exited 0. The three refused
arena levels are identified by sweep indices **69, 70, and 71** (the project
rule keeps proprietary level names out of tracked reports). Each hits
`story_start_ground`'s no-ground refusal; this branch's Godot session does
not select mode A, so it must not call `arena_mode_a_start_ground` from the
records alone. The sweep also records 108 stale `fx.runtime` "loaded level
name unknown" refusals that do not stop level play.
An isolated repeat of indices 69–71 with the same absolute private inputs
tried 3, refused 3 at `level.library-load`, loaded 0 and played 0, with no
failed checks. An initial targeted attempt used relative paths that Godot
resolved under its project directory and failed to open the game folder; it
was stopped by its exact PID and replaced with the absolute-path repeat.

Base `2e57376` **does give 111/111** with the same four-slice driver
arguments and private inputs: the recorded wave-24 `w24on --scripts` raw
summaries show 28+28+28+27 tried, loaded and played, 0 failures and empty
refusal lists. Wave-24 report commit `3850dbb` is a documentation-only child
of `2e57376`, so those recorded results describe that base code. The base
reads the executable's mode-A name table and selects the documented arena
start for those three levels. This branch diverged at `08dbc72` and lacks
that later integration; the regression predates the four audit edits. Per
Malik's direction, this fix does not port mode-A selection. Merge wave must
retain the base's arena route and recheck the sweep after integration.

No new x86_64/Rosetta suite or full 214-case arm64 suite was run for these
four fixes. The complete round-5 prop path remains refused without a loaded
hull table, and `path_blockNodes` still refuses without a path-grid service.
