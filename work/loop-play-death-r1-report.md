STATUS: MORE — the existing campaign Player dies, runs the native death clip,
counts down and fades; checkpoint reload and individual respawn remain refused.

# Player death handoff — 2026-10-24 / 2026-10-25

Worktree `play-death`, branch `codex/play-death`, base `252f34a6`. The previous
attempt left eight modified files and three new native files, with no commit.
Its sound timer/task code was retained. Local commits so far: `1d7f3b4`
(native death flow), `cf0a89b` (Godot fade/menu observations and boundaries),
`2d7ba45` (two Godot harness fixes), `719efaa` (private journey death checks
and readable refusal layout). No push, PR, sibling edit or game-data write.

Evidence: research-death-flow `8be79eb`, `docs/spec/player-death-flow.md` and
`tools/ref_death_flow.py`; both requested audits, ending in PASS. The earlier
FIX's one-player Resurrect ward and scheduler execution boundaries are retained.
The research executed 108 all-dead and 56 task cases per seed with service
stubs; no new original-game execution is claimed here.

## Change

- The actual Player damage body, record and World remain authoritative. Lethal
  damage selects the documented model/weapon clip and state 4. HP observations
  now read the record, avoiding the object mirror's one-tick lag at death.
- The audited signed countdowns, gates and task phases use shared World memory.
  The priority-20 task runs after objects and before the frame increment and
  end-of-tick all-dead check. No fade occurs in the scheduling tick.
- Godot consumes menu-close requests and the native fade value. The black fade
  rectangle is rendered, while its time remains native. The body still shows
  its explicitly labelled rest pose. No game-over/continue menu is invented.
- Unknown reload paths refuse once with a specific diagnostic. A dead object
  cannot be healed or moved by the manual save-restoration helper in place.
- A diagnostic lethal hit queues `Player_takeDamage` from a live level enemy;
  it does not force HP, animation or action state. Both private routes use it.

Implementation and remaining contracts: `docs/PLAYER_DEATH_FLOW.md`.
Public fixtures contain authored synthetic data only.

## Gates

Commands ran in this worktree; logs stay under ignored `work/play-death/`.

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release` and
  `cmake --build build/native -j 4`: **PASS**. One unchanged `level-flow` test
  object was reused from the exact base after verifying all 14 project source/
  header dependencies, identical flags/compiler and 830 unchanged toolchain
  dependencies. A compiler sample put the redundant compilation in LLVM SROA;
  only that owned compiler PID and its wait helper were stopped. The current
  runtime libraries and every changed target were built here in Release.
- `cmake --build build/godot-extension -j 4`: PASS, Release/template_debug.
  Two unchanged companion Release libraries were reused from the same clean
  `252f34a6` base after verifying source identity; hashes are recorded locally
  in `work/play-death/companion-artifacts.txt`.
- `ctest --test-dir build/native -j 3 --output-on-failure`: **241/242 passed**
  in **2,900.50 s**; `player-exact` alone hit its **240 s** timeout, with no
  assertion failure reported. The required isolated retry,
  `ctest --test-dir build/native -R '^player-exact$' -j 1 --output-on-failure`,
  also timed out at **240.04 s**. The default CTest timing gate is **not met**.
  Direct execution of the same checker with a 900 s bound **PASSed all 274,304
  exact complete-state cases and transport refusals in 248.48 s**:
  `/opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 tools/test_player_vectors.py --native build/native/champions-player-trace --vectors native/tests/vectors/player.tsv.gz`.
  This does not waive or change the original 240 s CTest property. All contract
  checks completed, but the default CTest timing gate remains unmet.
- `player-death-flow`: **2,828 synthetic checks PASS**, including 24 model/
  weapon selectors on the actual native Player damage body and World task order.
- `python3 work/play-death/run-godot.py`: all 56 commands ran; final aggregate
  is **56/56** after the corrected targeted retries. The new death UI smoke passes
  **11 headless / 13 rendered** checks, including half/full fade
  pixels. Two pre-existing harness failures were fixed and rerun: sandbox
  **124/124**, flow **65/65**, no script errors. Fixture file names now include
  the process ID; the lifecycle art stub supplies the HUD's Dictionary fields.
- `bash work/play-death/run-sweep.sh`: **111 tried / 111 loaded / 111 played**,
  **230 checks, 0 failures, 0 aggregate refusal kinds**; scripts **111 attached /
  76 started**. These are bounded loading/play checks; script-level unsupported
  calls remain counted internally, as in the base sweep.
- `FULL_TRACE`: **unchanged**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, 36,402 bytes.
  `FULL_SAVE_LOAD`: **300/300**, matching trace and presentation hashes.
- `python3 tools/run_loop_playthrough.py --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot --game-folder ../../private-validation/rta-disc --out work/play-death/playthrough`: **25 steps each**, **16 rendered captures**,
  **994 disc files before/after**, both exits **1**. Both verify the death
  sequence and report `blocked_death_reload`, `respawn_completed: false`;
  pre-existing loot/pickup/Gold journey refusals also remain. The new death
  observations agree: 78 actors, 239 native clip steps, zero HP/countdowns,
  fade 128, phase 1, menu closed and record values retained.
  The rendered variant was rerun after fixing clipped refusal text; its 25
  steps and 16 captures agree, and the dead/fading/refused images were inspected.
- `build/native/champions-campaign-fight-probe --game ../../private-validation/rta-disc --level valor --ticks 900 --death-check --attack-every 0`: PASS for the
  bounded death/refusal check: **78 actors, 900 ticks, 239 native clip steps**,
  zero HP, both timers zero, fade 128, phase 1, original dead body retained.
  XP, mana, Gold, all bag bytes and equipment compare unchanged; no values or
  private bytes/hashes are printed. This is not respawn success.

Initial Godot setup lacked its companion libraries and import cache. Those
failed startup runs were stopped using only PIDs started here and retained
separately. Import now waits for completion, and matrix results also reject
script errors even when Godot exits zero.

## Remaining refusals

There is no valid in-memory retail checkpoint in this fight. Reload needs the
complete saved records, level and pose, quests/gates and loader state; populated
world/decal restore and camera application are unsupported. The manual native
save is not substituted for that buffer. The off-act-table same-level loader
also refuses before HP refill. The saved record determines checkpoint rollback;
there is no established fixed death tax.

Individual savepoint/teammate revival, the one-player ward, complete Player/
spawn-protection/light/camera construction, and the allocation-failure route
remain unbound or incompletely documented. Rendered death poses, organic boar
attacks, a continuous original-game death/reload comparison and other platforms
remain unverified; x64 trace comparison and an exported-app run were not done.
Complete original scheduler/script interleaving is not claimed by the World's
bounded task hooks. Invalid checkpoint-buffer behavior and altered world-object/
pickup reset remain Unknown. No synthetic actor or guessed spawn point replaces
these missing services.
