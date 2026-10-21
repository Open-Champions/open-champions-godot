STATUS: COMPLETE — wave 21 integrated, verified and exported locally.

# Playable integration wave 21 — combat walk, constructors, dialog, menus

Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch
`codex/playable-integration`, started clean at `8bdd705` (wave 20). The four
requested sources were merged in order, with a local commit after each focused
verification. No other branch was merged. Nothing was pushed and no PR or issue
was opened. `~/champions-re` was not touched. The owner's disc copy was read
only and supplied counts only; all committed fixtures are authored synthetic
data.

| Source | Merged source | Integration commit | Verification before commit |
|---|---|---|---|
| combat-walk | `07f2479` | `362c9c4` | Release build, `netplay` and `combat-free-spot-host` 2/2. The branch's netplay ancestor was already integrated. |
| combat-ctors round 2 | **`a0d7118` exactly** | `7371245` | Release build; 16 focused CTests; four universal extension builds; Godot Full 2,620 checks, zero failures, expected trace. Includes combat-runtime-6. |
| dialog | `01b6e8a` (`codex/dialog`) | `b1640a7` | Release build; dialog and script/item focused tests; script Godot smoke; native disc census and windowed scene talks; expected trace. |
| menus | **`815a8f3`** | `0548440` | Release build; 8 focused CTests; Godot menu 36/36 and front end 85/85, script and HUD smokes, expected trace. Includes save-import and the same `04d2f1a` music-volume ancestor. |

## Merge resolutions and invariants

- Combat-ctors added its native tests beside the already integrated netplay
  test in `native/CMakeLists.txt`, and both `COMBAT_RUNTIME.md` sections were
  retained. Its researched audio model makes the 0.5 music default a raw
  value, so the script host no longer treats the value as optional. Its old
  no-volume refusal test was updated. The music-volume source is included
  once in each distinct native and extension target; both incoming branches
  share commit `04d2f1a`, with no duplicate implementation.
- Dialog's script host keeps the wave-20 item routing, partial-refusal stop
  rule and offline seed alongside dialog routing and word synchronization.
  The controller keeps the seed pairing and the new dialog task. Its CMake
  target keeps the object factory, items, and dialog. The real-data scene keeps
  both the play HUD and dialog. The dialog host test now expects an item
  condition to execute and open a talk.
- Menus merged without conflict. Its menu smoke writes under a per-process
  `user://` folder. The front end uses the existing scene audio bridge, not
  another mixer. The script-command CTest still covers refused commands
  stopping execution and `setCFlag` refusing placeholder objects. The play
  session's offline `getRand` uses its paired session seed; netplay keeps a
  separate strict simulation. The Full fixture has the AntQueen in slot 4 at
  (-110, 110), the Soul in slot 6, and no Skeleton in slot 5.

## Real-data sweeps and talk counts

The four-slice 60-tick sweeps `w21on` and `w21off` each tried, loaded and
played **111/111 levels**, with **230 passed checks**, zero failed checks and
an empty refusal list. With scripts off, every deterministic count equals
wave 20. With scripts on, every deterministic change belongs to dialog:

| Measure | Wave 20 | Wave 21 | Cause |
|---|---:|---:|---|
| `executed.getv` | 979 | 980 | One new dialog script read. |
| `executed.startDialog`, `levels_executed.startDialog` | 0, 0 | 1, 1 | Dialog answers the request. |
| `refused.startDialog`, `levels_refused.startDialog` | 1, 1 | 0, 0 | The old refusal is gone. |
| Dialog levels attached / with text / with font | 0 / 0 / 0 | 111 / 111 / 111 | Dialog reads the selected folder's text and font. |
| Dialog talkers | 0 | 24 | Dialog registers the talkers. |
| Dialog opens / start messages / scripts run / nodes shown / open at end / voice lines / animation reads | 0 each | 1 each | One script opens a talk during the 60-tick sweep. |
| Dialog ticks open | 0 | 58 | That talk remains open at the sweep cutoff. |

All other newly emitted `dialog.*` diagnostics are zero: choices taken,
client leaves, client mutex releases, deferred shops, ends, missing entries,
open-gate refusals, pending levels, post actions, refused operations, shop
requests, silent lines, stalls, ambiguous/blocked/no-candidate uses and uses.
Combat-walk, combat-ctors and menus change no other deterministic sweep count.

Runtime measurements vary with load. Scripts on: audio frames pushed
2,464,369 → 2,011,377; load time 61,328 → 40,749 ms; play time 74,620 →
51,042 ms; summed process peak 2,187,675,620 → 2,198,041,428 bytes. Scripts
off: frames 2,628,049 → 1,818,193; load 65,535 → 34,705 ms; play 81,278 →
42,350 ms; peak 2,187,577,460 → 2,195,821,332 bytes. Time and frames track
wall-clock load. The larger final extension and added dialog/menu state raise
the memory peak; these measurements cannot assign an exact byte delta to one
branch.

The native counts-only dialog census found 26 talkers, 52 attempted talks,
**48 opened and ended**, zero stalled, and four refused for the documented
missing quoted text key. That is six more openings than dialog round 1's 42;
the six item-condition talks now open. The windowed real-data scene tried 24
talks across 19 levels and **24 opened and closed**, zero failures in 133
checks (round 1: 21 opened, three refused). It drew the dialog box and text
on 992 frames and queued 47 voice lines. No private imagery was saved.

## Godot, export and trace

- Four universal GDExtension builds pass. Godot dev matrix **53/53**: wave
  20's 51 plus menu and front end. Full headless: 2,620 checks, zero failures.
  Wave 20 had 2,621; the researched music default replaces two old refusal
  checks with one value check. Combined audio similarly has 2,578 versus
  2,579. `FULL_TRACE` is
  **`4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**, **31,622 bytes**.
- Exported `build/exports/wave21/OpenChampions.zip` SHA-256 is
  `f73bfbae6869e04181174f6a3a0f0a09cbc660a4ced0448d0caa30bf7a8e63cf`.
  The executable and both extension dylibs have arm64 and x86_64 slices; the
  packaged dylibs match the release builds byte for byte. No `.gob`, `.lmp`
  or `.iso` member or private-validation path string was found.
- Export matrix **21/21**, including menu and front end. The graphical
  1280×720 36,000-tick Full flow passed **108,826 checks**, zero failures,
  with 63 captures and a successful save/load continuation. Wave 20 had
  108,827 checks; the music-default assertion replacement accounts for one
  fewer check. The headless and all three graphical export traces match the
  hash and 31,622-byte length above.
- Exported app, read-only real data, 60 ticks with a creature preview per
  level: **12/12** without scripts, **12/12** with scripts, and **3/3** arena
  levels with scripts. Checks: 26, 26 and 8, all passing. HUD on every level.
  The pre-existing BLEND-7 mixed-pass material refusal appears on nine of
  the 12 preview levels, with no failed level checks.
- Actual synthetic captures were inspected in the dev main menu, boot card,
  inventory panel and exported Full combat. The front end labels its host
  layout and the menu panel labels unsupported controls. No new visible
  rendering failure was found. The temporary capture script was removed.

## Native verification

Both Release builds pass; inspected x86_64 test binaries are Mach-O x86_64.
The first full arm64 pass reached 224/225: `combat-free-spot-host` failed
because the added Full-fight actors changed the shared draw and a Skelight
hit interrupted the isolated `walk.once` swing at tick 11. The initial test
also assumed every Full-fight `findFreeSpot` event had the Skelight's 72/12/0
arguments; the AntQueen legitimately searched with 72/15/0. The host test
now removes opponents only for its researched player swing/walk timing
subcase and still runs the intact Full fixture for idle and walking searches.
It counts the Skelight search signature without rejecting the AntQueen's
event. The corrected `combat-free-spot-host` passes separately on arm64 and
under Rosetta x86_64. The final complete arm64 suite passed **225/225** in
821.38 seconds; Rosetta x86_64 passed **225/225** in 1,254.27 seconds.

## Commands and remaining limits

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 -j 4
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native-x64 --output-on-failure -j 3
work/run-slices-r21.sh w21on --scripts
work/run-slices-r21.sh w21off
python3 work/playable-wave21/run-builds.py
python3 work/playable-wave21/run-godot.py
# Godot 4.5.2 --headless --path godot --export-release 'macOS universal' <absolute wave21 ZIP path>
python3 work/playable-wave21/run-export.py
```

Counts-only logs and metadata are in ignored `work/private/` and
`work/playable-wave21/`. The original-client display and voice synchronization,
real Windows/Linux exports, manual play, and original-client netplay
interoperability remain unverified. Documented refusals such as Skeleton state
3, `GutChunk_ctor` and BLEND-7 remain; no values were invented to hide them.
