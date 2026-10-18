# Local WORLD level library

`res://scenes/level_library.tscn` is a reusable diagnostic Control panel. The
scene shell instantiates it, calls `configure(load_level, release_controls)`,
and opens it with `show_library()`. `load_level(path)` must be the shell's
existing atomic `load_terrain_file(path) -> bool` callback. The
`release_controls` callback must release held input **and pause the native
scene** before the panel or a file picker opens. For `session_sandbox.gd`,
`set_paused.bind(true)` meets that contract. The component emits
`level_loaded(path)` only after the native callback returns true. The shell
owns resume and may do so after closing the panel or receiving that signal.
The component creates no native controller and does not read WORLD bytes.

The user may add a single local `.WORLD` file, select one directory for a
nonrecursive scan, retry any candidate, or forget its path. A scan examines at
most 512 direct entries and adds at most 128 `.WORLD` names. It skips directory
links and never chooses a folder on its own. History contains at most 128
selected full paths, basenames and local recency numbers in
`user://level-library.json`; it contains no original bytes. Rows with equal
basenames remain distinct by their full local paths. A successful load makes a
row recent, but every later selection asks the native loader to check the
file's **current bytes**. A missing or refused file stays visible. Forget
removes only the history entry. The shell's native error text is shown when
its `status_label` is available.

The panel and folder scan are authored client navigation. They do not recreate
an original game menu or imply campaign order, spawn discovery, or WORLD format
coverage. The existing native loader remains responsible for ground, geometry,
water and moving-piece refusals, starting-floor checks and resource limits.
Successful replacement follows its fresh-session policy; failures and canceled
pickers leave live ground, simulation, health and appearance alone.

`res://tests/level_library_smoke.gd` is a standalone host over the real
`ChampionsSessionController` and the existing synthetic WORLD fixture helpers.
It checks two differently translated accepted meshes, native movement and HP
reset, returning to a recent file, malformed/water/moving/missing/changed
refusals, duplicate basenames, cancel, bounded one-folder discovery, isolated
`user://` persistence and 4:3/16:9 layout. An exported graphical run with
`OPEN_CHAMPIONS_CAPTURE_DIR` set to an ignored `work/` folder writes viewport
captures there. Headless runs check the layout without claiming rendered pixels.

Historical private measurements were 48/116 geometry inputs and 17/116
source-scene inputs accepted for one RTA corpus. This component does not
reverify those counts. Real level selection remains unverified because the
private retail inputs are unavailable on this machine. No Norrath or complete
retail level support is claimed.
