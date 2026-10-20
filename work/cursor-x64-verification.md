STATUS: FAIL

x86_64 CTest **182/183 pass**, 1 fail (`fx-exact`), exit 8, 1649.22 s real. No test timed out.
Python checks **5/6 pass**. `unittest discover` exits 1.
Vector regeneration of the fx and combat-runtime corpora **matches byte for byte** (6/6 file pairs).

Host: Apple M5 Pro, macOS, AppleClang 21.0.0.21000334, CMake 4.4.4, Python 3.12.3.
Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch `codex/playable-integration`, HEAD `ba23958`.
Rosetta is active for these binaries: `file` reports `Mach-O 64-bit executable x86_64`; `arch -x86_64 /usr/sbin/sysctl -n sysctl.proc_translated` prints `1`; `arch -arm64` on `champions-playable-combat-fx-tests` exits 1 with `Bad CPU type in executable`.
No source change. `~/champions-re` was not written. No push.

## Commands

```sh
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 -j 4
ctest --test-dir build/native-x64 --output-on-failure -j 3
ctest --test-dir build/native-x64 -R '^fx-exact$' --output-on-failure -j 1
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 tools/test_windows_pack.py
python3 tools/test_desktop_package.py
python3 tools/test_texture_upscale_cache.py
python3 -m unittest discover -s tools -p 'test_*.py'
PYTHONDONTWRITEBYTECODE=1 python3 -I tools/generate_fx_skills_vectors.py --check
PYTHONDONTWRITEBYTECODE=1 python3 -I tools/generate_fx_exact_vectors.py \
  --research "$HOME/Projects/champions-re-wt/research-fx" \
  --out work/cursor-x64/regen/fx_exact
PYTHONDONTWRITEBYTECODE=1 python3 -I tools/generate_fx_render_vectors.py \
  --research "$HOME/Projects/champions-re-wt/research-fx"
PYTHONDONTWRITEBYTECODE=1 python3 -I tools/generate_fx_recipes_vectors.py \
  --research "$HOME/Projects/champions-re-wt/research-fx"
PYTHONDONTWRITEBYTECODE=1 python3 -I tools/generate_combat_runtime_vectors.py \
  --research "$HOME/Projects/champions-re-wt/research-combat" \
  --out work/cursor-x64/regen/combat_runtime.txt.gz
PYTHONDONTWRITEBYTECODE=1 python3 -I tools/generate_combat_runtime_2_vectors.py \
  --research "$HOME/Projects/champions-re-wt/research-combat" \
  --out work/cursor-x64/regen/combat_runtime_2.txt.gz
```

`fx_render` and `fx_recipes` write their default paths. Those four files were copied aside, regenerated, compared, and copied back. `git status` stayed empty.

Configure wrote `CMAKE_BUILD_TYPE=Release` and `CMAKE_OSX_ARCHITECTURES=x86_64`. Build exit 0 in 491.6 s.

## CTest

`ctest --test-dir build/native-x64 --output-on-failure -j 3` started 2026-10-20T17:49:59Z and finished 2026-10-20T18:17:28Z. **182 passed, 1 failed, exit 8.** Total Test time (real) = 1649.22 sec. The only failure is test 1, `fx-exact` (labels `fx`, `native-fx-2`), 4.85 s in the full run.

No test hit its timeout, so no timeout rerun was required. `player-exact` passed in the aggregate run in 226.94 s (limit 240). `skills-a-exact` 52.66 s, `skills-b-exact` 46.37 s, `skills-c-exact` 79.25 s (limit 1800). `combat-runtime-2-exact` passed in 126.25 s.

The isolated rerun of the failure:

```sh
ctest --test-dir build/native-x64 -R '^fx-exact$' --output-on-failure -j 1
```

Exit 8, 4.75 s, the same 623 mismatches and the same three printed digests.

The existing arm64 binary `build/native/champions-fx-exact-tests` (`Mach-O 64-bit executable arm64`) passes the same test in 3.42 s, exit 0:

```sh
ctest --test-dir build/native -R '^fx-exact$' --output-on-failure -j 1
```

## Failure: fx-exact emitter velocity bits

Minimal reproduction, from this worktree, after the x86_64 Release build above:

```sh
build/native-x64/champions-fx-exact-tests native/tests/vectors
echo exit:$?
```

Observed exit 1 from the test binary (CTest wraps that as exit 8). Stdout/stderr:

```
184 synthetic assertions, 0 failed
FAIL: emitter case 0 digest cd0c06d54541f753 expected dc91a46490c882f0
FAIL: emitter case 1 digest d5222d34221c9c42 expected da7ef84b38f6c267
FAIL: emitter case 3 digest e135baa1d9d5720a expected 901bcb6dce688353
  boot: 1 rows, 0 mismatches
  decalgate: 400 rows, 0 mismatches
  decals: 30 rows, 0 mismatches, 12900 operations
  def: 3000 rows, 0 mismatches
  defreset: 1 rows, 0 mismatches
  emitdef: 1000 rows, 0 mismatches
  emitter: 2000 rows, 623 mismatches, 11105 operations, 12245 spawns
  flipbook: 600 rows, 0 mismatches
  lighteffect: 600 rows, 0 mismatches, 16865 operations
  lightnodes: 150 rows, 0 mismatches, 600 operations
  lightslots: 150 rows, 0 mismatches, 182002 operations
  markers: 300 rows, 0 mismatches
  objlight: 1000 rows, 0 mismatches, 4533 operations
  pool: 42 rows, 0 mismatches, 22198 operations, 4313920 spawns
  props: 400 rows, 0 mismatches, 35493 operations, 48014 spawns
  rz: 3000 rows, 0 mismatches
  spawn: 2000 rows, 0 mismatches
vectors: 14674 rows, 623 mismatches
```

`native/tests/fx_exact_tests.cpp` prints at most three mismatches per property (`bad[prop] < 3`). All 623 mismatches are `emitter`. The other 12,674 rows match, and the 184 synthetic assertions pass.

`replay_emitter` records each spawn as an `O` line of six binary64 patterns: position, then velocity (`Line::d` via `double_bits`). Dump of case 0:

```sh
build/native/champions-fx-exact-tests native/tests/vectors --dump emitter 0
build/native-x64/champions-fx-exact-tests native/tests/vectors --dump emitter 0
```

The `T` lines match (emitter `next`, `on`, RNG seed, spawn count). Two `O` lines differ, and only in a velocity word:

| arm64 bits | x86_64 bits | arm64 value | x86_64 value | integer delta |
|---|---|---|---|---|
| 4616354629288787453 | 4616354629288787454 | 4.146559417072692 | 4.146559417072693 | +1 |
| 4591349800739970752 | 4591349800739970688 | 0.09277828612050154 | 0.09277828612050065 | -64 |

The position words on those same lines match. Emitter case 2 dumps are identical. The committed `fx_exact.tsv` and `fx_exact.json` regenerate byte for byte (below), and the arm64 replay matches those expected digests.

## Python checks

| Command | Result |
|---|---|
| `python3 tools/check_simulation_boundary.py .` | PASS, exit 0. "Simulation has no host floating-point types or presentation dependencies" |
| `python3 tools/test_core_simulation_boundary.py` | PASS, 40 tests, 0.937 s |
| `python3 tools/test_windows_pack.py` | PASS, 2 tests |
| `python3 tools/test_desktop_package.py` | PASS, 26 tests, 0.112 s. Packaging output is mocked |
| `python3 tools/test_texture_upscale_cache.py` | PASS, 8 tests, 1 skipped |
| `python3 -m unittest discover -s tools -p 'test_*.py'` | FAIL, exit 1. 715 tests, 47.714 s, 385 skipped, 1 error |

The discover error is an import failure, so the other 714 tests are the previous wave's 329 executed tests plus 385 skips.

## Failure: unittest cannot import the combat-runtime-2 checker

Minimal reproduction:

```sh
python3 -m unittest discover -s tools -p 'test_*.py'
echo exit:$?
```

Exit 1. The traceback:

```
ERROR: test_combat_runtime_2_vectors (unittest.loader._FailedTest.test_combat_runtime_2_vectors)
ImportError: Failed to import test module: test_combat_runtime_2_vectors
  File ".../tools/test_combat_runtime_2_vectors.py", line 13, in <module>
    exe, path = sys.argv[1:]
ValueError: too many values to unpack (expected 2)
```

`tools/test_combat_runtime_2_vectors.py` unpacks `sys.argv[1:]` at import time. Discovery's argv is `discover -s tools -p test_*.py`, which is not two values. The file is a CLI (`exe` and vector path), and CTest's `combat-runtime-2-exact` passed on this x86_64 build (126.25 s).

## Vector regeneration

Research checkouts were read with `git show` of the pinned commits. Nothing was written under `~/champions-re`.

| Generator | Compared files | Result |
|---|---|---|
| `generate_fx_skills_vectors.py --check` | in-memory vs `fx_skills.tsv`, `fx_skills.json`, synthetic recipe fixture | `fx_skills vectors match`, exit 0, 6 s |
| `generate_fx_exact_vectors.py --out work/cursor-x64/regen/fx_exact` | `fx_exact.tsv`, `fx_exact.json` | `cmp` match, 14,674 rows, 39 s |
| `generate_fx_render_vectors.py` | `fx_render.tsv`, `fx_render.json` | `cmp` match, 8,914 rows, 4 s |
| `generate_fx_recipes_vectors.py` | `fx_recipes.tsv`, `fx_recipes.json` | `cmp` match, 23,800 rows, 38 s |
| `generate_combat_runtime_vectors.py --out work/cursor-x64/regen/combat_runtime.txt.gz` | `combat_runtime.txt.gz`, `combat_runtime.json` | `cmp` match, 585 cases, 12 s |
| `generate_combat_runtime_2_vectors.py --out work/cursor-x64/regen/combat_runtime_2.txt.gz` | `combat_runtime_2.txt.gz`, `combat_runtime_2.json` | `cmp` match, 2,858 cases, 429 s |

`docs/INTEGRATION.md` and the playable job reports list the boundary checker, the named `tools/test_*.py` scripts, and `python3 -m unittest discover -s tools -p 'test_*.py'`. They do not list `generate_*.py` invocations. The historical generators (skills, session, player, and the rest) were left unrun. The discover run's "all five native targets match …" lines are hash checks of the committed corpora.

Logs: `work/cursor-x64/` (`build.log`, `ctest.log`, `x64-fx-exact-alone.log`, `arm64-fx-exact.log`, `python-tooling.log`, `regen-fx-combat.log`).
