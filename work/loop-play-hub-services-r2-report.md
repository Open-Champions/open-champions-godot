STATUS: COMPLETE — play-hub-services round 2 (fix) (2026-11-09). All five findings of `cursor-runs/audits/play-hub-services-r1.md` (HPS-1..5) are fixed. All gates PASS on the mini: Release build and GDExtensions, focused 4/4, full CTest 260/260, strict Godot 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged. The journeys match round 1 step for step, and the wave 38 deltas are the same ones round 1 reported. The focused `--hub-only` run now executes hub2's PushTrigger and passes `alternate_hub_services` with the stricter conditions.

# play-hub-services round 2 (fix)

Branch `codex/play-hub-services`, from round 1 tip `07eb0eb3`. Commits:
`2c6ef7f7` (HPS-5 citations), `adde4020` (HPS-1 refusals and HPS-2 test),
`e52e59d7` (HPS-3 focused journey), and this report. Research authority is
unchanged: `t3/research-hub-npcs` @ `fbbfe65`, `docs/hub-services.md` (HS) and
`tools/ref_hub_services.py` (RHS).

## HPS-1 — the two K.poll Unknowns now refuse in code

HS:381-383: what several queued `K.poll` copies do together is **Unknown**.
That the warning help box pauses the game is **Inferred** (not executed). Both
cases now throw `RefusedResult` from the native wrapper before the native runs,
which stops the level script. They are counted in
`DialogCounts::hub_warn_refused` and reported as
`hub_service_research().warn_refusals`.

- **Finding the entries, not by name** (`hub_dialog::warn_entries`). HS:51 says
  public `0x54` is the PushTrigger action. Its `callScript` literal is `K.use`
  (HS:32). `K.use`'s `callScript` literal is `K.help`, and `K.help`'s
  `addDeferedCall` literal is `K.poll` (HS:34). Each must sit at its researched
  public offset: hub `0x49c0` / `0x4a80` / `0x4b0c`, hub2 `0x2210` / `0x22d0` /
  `0x235c`. Names are read from the loaded script and never stored. The private
  probe confirms that the chain resolves on both real hubs
  (`warn_entries_resolved` true / true).
- **Second copy.** An `addDeferedCall` whose literal is `K.poll` refuses while
  a copy is pending. Pending means in the back half, or in the front half after
  the entry now running (`Variables::deferred_pending`, read-only). The running
  copy re-queuing itself is not a second copy, so it is admitted. A second
  warning (`K.use` → `K.help`) while one copy is queued therefore refuses.
- **Help box.** A `gamePaused` call made by the running `K.poll` entry
  (`Variables::deferred_running`) refuses while `hud().help_open`, so that run
  is never counted. Research does not settle whether the box pauses:
  `docs/strings.md` §3.3 Observed, but did not execute, the help task's own
  pause flag, and HS:381-382 labels the link to `gamePaused` as Inferred. The
  client's help box writes none of `gamePaused`'s inputs (0x41b4f4, 0x500ae8,
  0x44fcd0). So the case stays refused.
- After either refusal the script stays stopped (`faulted_`), so no later tick
  runs `K.poll` or opens the menu. Only `addDeferedCall` and `gamePaused` in a
  hub / hub2 level with the dialog attached are inspected. `start`'s two
  deferred checks are unaffected (tested).
- `UNKNOWNS` rows `help_box_pause` / `poll_copies` and `docs/HUB_SERVICES.md`
  now describe these refusals in code.

## HPS-2 — the K.poll test is tick-driven and checks what HSF-2 requires

`native/tests/hub_offer_host_tests.hpp`, `table::warn_poll`, runs on both the
hub and hub2 fixtures.

- **Fixture.** The authored chain sits at the researched publics: action `0x54`
  → `K.use` → `K.help` → `K.poll`, found by the production `warn_entries`. A
  `getv` stands in for `K.warn`. `K.help` restarts the count and queues
  `K.poll`. `K.poll` calls `gamePaused` once per run, counts unpaused runs,
  calls `activateMissionMenu` on the 6th, re-queues itself, and never writes
  the entrance (HS:373-374). The fixture records no run counter. Fixture
  publics are now checked for overlap.
- **Ticks counted outside the script.** The test calls `on_ready_tick` itself.
  Its per-run record is `gamePaused`'s executed count. The action queues
  `K.poll` outside a tick, into the back half of the double-buffered queue
  (research-loading-readiness `d6f035fa` `docs/loading-arrival-r2.md:11-43`).
  So tick 1 only promotes it, and every later tick runs exactly one copy and
  leaves exactly one queued. Both are asserted on every tick.
- **Outcomes, as ticks.**
  - Menu on the 6th unpaused run, which is tick 7.
  - Runs 2-4 paused: run 9, tick 10.
  - Scattered pauses at runs 1, 5, 6 and 9: run 10, tick 11.
  - Recall set: the 6th run still calls `activateMissionMenu` (tick 7), and the
    menu stays shut.
- **Entrance.** 0x4f8468 holds a sentinel. With no warning, the same action
  path writes it ("ent") and opens the menu: this control asserts both. With
  the warning, the sentinel is unchanged after 14 ticks and the executed
  `setEntrance` count stays 0.
- **HPS-1 tests.**
  - A second warning before the first tick, or after two, refuses and stops
    the script. Exactly one copy stays queued, and later ticks run no `K.poll`
    and open no menu.
  - A help box opened before run 1, or before run 4, refuses that run. The
    runs before it are counted (0 and 3), and no menu opens even after the box
    closes.
- dialog: **1,126,711 checks, 0 failed**.

## HPS-3 — focused `--hub-only` journey

Hub2's executed `script_run_exit(PushTrigger)` is back. Every pre-existing
assertion that changed in `alternate_hub_services` (`godot/tests/loop_playthrough.gd`):

| Assertion | Wave 38 (`16b66176`) | Round 1 (`f24bca3a`) | Round 2 (now) | Research |
|---|---|---|---|---|
| hub2 `dialog_use` | must refuse | must refuse | must refuse (unchanged) | physical use: vehicle / grid query unavailable |
| hub2 `dialog_talk_to(0)` | must refuse | must succeed, bind the hub2 table, close; dedupe tables match | same, **plus** `fence_refusals == 0`, `start == 1`, `choices_taken >= 2` | HS:3-5, 196-245 (hub2 rows Executed); HS:112-117 (only the five fenced natives, so 0 refusals); HS:201-203 (from H2.N01, MORE then END/SHOP, so at least 2 choices); start 1 is the round 1 baseline |
| hub2 PushTrigger `script_run_exit` | must refuse | **deleted**; only radius / nethostonly compared | must run `ok`, with exactly 1 `setEntrance` and 1 `activateMissionMenu`, the menu open (closed before), and no warn refusal; then the menu is cancelled (CIRCLE 0x20, the call the scene's menu-key branch makes) and must close; record values must still match | HS:373 (no warning: entrance then menu); HS:363-365 (values); mission-unlocks.md:127-232 (cancel) |
| Savepoint | accepted + `request_save` | same | same (unchanged). Used before the PushTrigger, in record order (6 before 7) as in wave 38 and round 1, because once a mission menu has opened its residual open state (mission-unlocks.md:172-176) holds the savepoint's menu gate | — |
| pass condition | loaded, refused, savepoint | adds `alternate_table` | `alternate_table` now includes all of the above | — |

The cancel uses the controller call rather than a key event. The vendor store
that the hub2 SHOP choice opens at dialog end (HS:249-262) is a game menu, and
it takes ESC first. The run records the owner of the input (`game_menus_open`
true). Result:

- `alternate_hub_services` = **bounded**. Entrance writes 1, menu calls 1,
  menu opened and closed, warned false, 0 fence refusals, start 1, 4 choices,
  savepoint accepted.
- All 25 steps match round 1's focused run.
- Exit 1 comes from the existing primary-hub `hub_savepoint` readback refusal,
  as in round 1 and wave 38.

## HPS-4 — probe prints numbers only

`work/hub-services-r2/probe.gd` (ignored) prints integers, booleans, and
dictionary or array sizes only. It prints no strings, dictionary keys, paths
or refusal text (`numbers()` drops strings). Output (`probe.txt`):

- hub: binds, start 1. Dedupe 10/13 match. 3 dialog files, 1 backup, 1 line
  list, stem requested. PushTrigger matches. `warn_entries_resolved` true.
  0 warn refusals.
- hub2: binds the hub2 table, start 1. Dedupe 10/13. 2 files, 1 backup.
  PushTrigger matches. `warn_entries_resolved` true. 0 warn refusals.

## HPS-5 — citations

The corrections in `hub_dialog.hpp` were checked against `fbbfe65`:

- predicate RHS:176-191 (was 200-216), and start RHS:194-200;
- `status` RHS:43-48 (was 42-47) and `offer` RHS:51-52 (was 50-51);
- ACTIONS RHS:123-149 (was 125-151) and NODES RHS:59-120 (was 58-123);
- kinds RHS:21-26, effects RHS:33-37, and K.poll RHS:343-360.

`docs/HUB_SERVICES.md` NODES cite is fixed the same way. The round 1 report is
left as recorded.

## Gates (mini, tree at `e52e59d7`, evidence in ignored `work/hub-services-r2/`)

- Sync: `rsync -rlp --checksum` (no `-t`), and every transferred file is
  touched on the mini. `--delete` is used only on native, gdextension, tools
  and docs, never on `godot/`, and `godot/bin` is excluded.
- Disk: 39-46 GB free at every check (at least 30 required). One build root,
  `build/`, built `-j 4` / `--parallel 4` one configuration at a time and
  deleted after the gates.
- Release native build: PASS, exit 0, fresh, 231 s. GDExtensions: PASS, exit 0,
  368 s. `lipo`: x86_64 arm64 for all five libraries. Import exit 0.
- Focused CTest `^(dialog|hub-services|script-commands|quests)$`: **4/4**.
  The first dialog run had 16 failures, all one off-by-one in the test's tick
  model: tick 1 promotes the queue. The test was corrected, rebuilt and passed
  before the gate.
- Full CTest `-j 3`: **260/260 PASS**, 1,270.57 s.
- Strict Godot matrix (`--captures --timeout 1200`): **54/54 PASS**, the same
  112 log files as round 1. check-log: 105 logs, 0 failing.
- Combat **2,645 checks, 0 failures**. FULL_SAVE_LOAD **300/300**,
  presentation `cb2309ab…d8573` continued = uninterrupted. FULL_TRACE
  **unchanged**: 36,402 bytes, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659` (same as
  round 1 and wave 38).
- Journeys (`tools/run_loop_playthrough.py`, read-only disc 994/994): wrapper
  exit 1 (the retained full-journey refusal, as before).
  - 80 / 81 steps, 0 / 0 log errors, 38 rendered captures.
  - Pickup 10→11 in 52 walk ticks.

### Journey comparison: wave 38 → round 1 → round 2, step by step

`compare3.py`: step names are identical at every index in all three runs, in
both variants.

- Round 2 equals round 1 in every step's result.
- The wave 38 → now deltas are exactly round 1's:
  - headless 24-27, 30-33, 35, 36, 38;
  - rendered the same, plus 38 `mission_menu_render` and 39
    `mission_menu_locked_navigation`.
- Refused rows: 27 / 17 / 17 (headless) and 28 / 17 / 17 (rendered).
- Refusal kinds: 8 / 7 / 7. Round 1 → round 2: 0 gone, 0 new. No warn refusal
  occurs.
- Numeric detail differences from round 1: `enemy_fight.swing_wall_msec`
  (wall time) and rendered `dialog_choice_close.choices_taken` 4 → 6. The
  latter is run-to-run noise from the ENTER cadence against the close fade:
  round 1's own runs on one tree give 5/3, 4/3 and 5/4. The step is `closed`
  in every run.

## Notes

- Commits were made on the MacBook worktree, which only moved forward from
  `07eb0eb3`. The mini copy is not a git repository, as in round 1.
- No push, PR, install, `/tmp` use on the mini, process-name kill, private-disc
  write or mini home path in tracked files. Fixture names (`pa`, `kuse`,
  `khelp`, `kpoll`, `kw`) are synthetic. No disc text.
