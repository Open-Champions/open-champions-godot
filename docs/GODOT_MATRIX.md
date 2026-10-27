# Godot development log gate

Use the tracked runner instead of the historical `work/*/run-godot.py` copies.
Those copies were local scratch and checked only exit status. Godot can print a
script error, abort that callback, and still exit zero when a later callback
calls `quit(0)`. A PASS line alone therefore cannot establish success.

Build the three debug-ABI extensions with Release optimization, using the pinned
dependency and commands in [GODOT_BROWSER.md](GODOT_BROWSER.md),
[LEVEL_TEXTURES.md](LEVEL_TEXTURES.md) and
[CREATURE_ANIMATION.md](CREATURE_ANIMATION.md). Then run:

```sh
python3 tools/godot_matrix.py matrix --godot /path/to/Godot
python3 tools/godot_matrix.py matrix --godot /path/to/Godot --case scene_play_flow_smoke
python3 tools/godot_matrix.py matrix --godot /path/to/Godot --captures
```

The public matrix runs the bounded editor import, default scene, explicit
synthetic smoke list and the three isolated creature-animation checks. The Full
play checks use 640 ready ticks. `--captures` adds graphical synthetic capture
drivers and the creature-blend and level-material pixel smokes; it needs an actual
display and renderer. Those two pixel smokes explicitly refuse headless execution
with exit 2 before waiting for draw signals or accessing unavailable images.
A headless run does not validate
exported apps, images, interactions, private game data or another CPU. Logs and
`results.json` go under ignored `work/godot-matrix/` (`--out` changes this).
Each engine's own log also goes there. Each invocation copies the project to
an automatically removed temporary directory (including its import cache).
Each case uses a fresh UUID-named custom Godot user directory, removed on exit
or failure; this includes the generated projects and subprocesses of all three
creature checks. The editor populates the invocation's private import cache for
subsequent cases. Isolation is independent of `--out`; source settings and
application data are untouched. Missing-file negative fixtures start absent. `--project` selects an explicitly staged
project when a test environment needs one; it defaults to this checkout's `godot/`.
The runner continues after failures and returns nonzero if any case failed.

For another command, including private-data smoke/capture scenes, preserve all
its arguments after `--`. Both stdout and stderr are captured in the specified
log. `--expect` requires an exact completion line when the driver supplies one:

```sh
python3 tools/godot_matrix.py run --godot /path/to/Godot \
  --log work/archive.log --expect 'Open Champions browser smoke: PASS' -- \
  --headless --path godot --script res://tests/archive_smoke.gd
python3 tools/godot_matrix.py check-log work/archive.log
```

Success requires normal exit, completion markers when requested, and a clean
log. The shared [checker](../tools/godot_log.py) rejects `SCRIPT ERROR:`,
`ERROR:` (including user errors), parser/compiler/shader error markers, C++
error annotations and assertion/null/bounds API diagnostics. ANSI SGR colour
sequences are removed only for matching. Markers must begin the
line after spaces/tabs and an optional numeric `[mm:ss]` or
`[hh:mm:ss.millis]` timestamp; quoted diagnostics inside warnings, JSON payloads
and paths do not match. No diagnostic category or scene is exempted. The log
itself is preserved. Ordinary warnings and structured native refusals (`{"ok": false,
"error": "unsupported ..."}`) are not engine errors. Warnings remain visible in
the preserved log, including Godot's ObjectDB shutdown warnings; `ERROR:`
resource/RID leak messages fail. A timeout keeps partial
output and fails; the runner never retries a failure to turn it green.

CI editor/browser/session checks, desktop packaging, the isolated
creature-animation runner, mounted-Horsemen driver and private playthrough runner use this same
checker. Existing ignored runners on another checkout must be replaced with
this runner or call `require_clean_log` on their complete stdout/stderr log;
changing this checkout cannot update those local scratch copies.

## Allow-list

**Empty.** No script, engine or API errors have been established as benign.
Environment failures (missing extensions, unwritable data directories, missing
display or unavailable trust-store access) remain failures and must be resolved
or reported as an execution limitation.

Any future entry belongs in `ALLOW_LIST` in `tools/godot_log.py` and must match
one complete diagnostic line. Document the exact message, engine version,
reproduction and evidence that the tested behavior is unaffected, with a reason
and a removal condition here. Do not exempt categories, scene names, native
refusals, leaked objects, missing fixtures or an error solely because exit was
zero. Prefer fixing its GDScript/GDExtension cause.

The regression tests exercise real zero-exit subprocesses with errors on either
stream, PASS-plus-error logs, timeouts, exact completion markers, matrix failure
aggregation and the packaging gates:

```sh
python3 -m unittest discover -s tools -p 'test_godot_matrix.py'
python3 -m unittest discover -s tools -p 'test_desktop_package.py'
```
