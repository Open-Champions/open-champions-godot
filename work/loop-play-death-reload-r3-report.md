STATUS: FIX APPLIED. PDR2-1 to PDR2-7 are fixed, and both research-backed pin changes are made. All gates pass. The journeys match round 2 step by step at every index. Part 3 (a real checkpoint in the journey) is **not** unblocked. The census finds that the refusing records on the fresh level are 16 `ScriptGenerator` objects, not dead Creatures, and research does not establish what the original saves for that class. The refusal stays.

Branch `codex/play-death-reload-r2`, round 2 tip `ef91dd0e`. Commits:

- `d58a7e79`: native `level_flow` (PDR2-1, -3, -4, -7 message, Part 2 pins).
- `e4de5e16`: controller, scene, census, journey fields and the new opt-in test (PDR2-2, PDR2-6).
- `d0b50abe`: `docs/PLAYER_DEATH_FLOW.md` (PDR2-5, -7, step 5 note, Part 3 blocker).
- This report is a separate commit.

Audit: `cursor-runs/audits/play-death-reload-r2.md` (FIX, 2 MEDIUM, 5 LOW).
Research pins as in round 2: `t3/research-death-reload` `98ea7a6`
("client-path"), `t3/research-save-world` `31a0ca6` ("save-world"),
`t3/research-saves` `8d43286` ("save-format"), `t3/research-save-screen`
`c8895e2` ("save-screen"), `t3/research-exits-r3` `350fc8b` ("exits-r4").

## Where the work ran

All builds, tests, Godot runs and journeys ran on the Mac mini, in
`~/malik-champions/open-champions/worktrees/play-death-reload-r2`.

- Edits were synced from the MacBook with `rsync -rlp --checksum`, and the
  changed files were touched. Before each commit, a dry-run `rsync --checksum`
  showed that the MacBook and mini files were identical.
- Commits were made on the mini. The MacBook only fast-forwarded. The mini
  worktree is a standalone clone, so the fetch is from that worktree path.
- One build directory (`build/`), native `-j 4`, CTest `-j 3`. `build/` was
  deleted at the end.
- Disk: 44 GB free before the native build and 43 GB before the extension
  build. During CTest it fell to 21 GB. Nothing was built while it was under
  30 GB: the only source change made in that window (a header comment) was
  reverted rather than rebuilt.
- Scratch output is under `work/death-reload-r3/` (ignored). Nothing went to
  `/tmp`, nothing was installed, and no process was killed.

## Part 1: PDR2-1 to PDR2-7

**PDR2-1 (MEDIUM). Completed game outside every act.** In `level_flow.cpp`,
`save_game` now follows the `save_game` / `save_build` split:

1. The living test.
2. Act −1 or `mission_menu_task` goes straight to `save_build`
   (save-world:253-254).
3. Otherwise, with no act row as a prefix, it returns `NoActRow` and builds
   nothing, whatever `game_complete` says (save-world:255-260).
4. In `save_build`, with the game not complete, its own `save_findAct`
   missing gives the new `SaveGame::BuildNoAct`: nothing is written
   (save-format:55-58). `game_complete` skips only that lookup and the level
   store (save-format:58-60; save-world:269-275).

At act −1, a non-hub name has no current-act row and is not stored
(save-format:236-242). A `hub*` name at act −1 refuses, because the store
would read row 0 of no act, and that path was not executed.

Test C, at `level_flow_tests.cpp` `death_reload_r2_cases`, was added on this
branch and is corrected. A completed game in `SYN_NOWHERE` now returns
`NoActRow`, leaves the earlier header (level `aa`, no `SYN_Q`) and adds no
entry. New cases test the bypass separately:

- The mission-menu task with a completed game: built, no store.
- The mission-menu task with the game not complete: `BuildNoAct`, header
  unchanged.
- C2, act −1:
  - `SYN_NOWHERE`: `BuildNoAct`.
  - `aa`: built, not stored.
  - `hub`: refused.

**PDR2-2 (MEDIUM). Card save rebuilds the live header.** In
`real_data_play.gd`, a successful `save_session_file` now calls
`set_live_checkpoint` and sets `_live_checkpoint_path` to the saved file.
That also re-baselines the act check. The original's offline card save
calls `save_build(slot byte, 0)` before writing (save-screen:229, Executed).
So "load A, save to B, die" reloads B.

The same line shows `save_build` runs before the card write. A refused
client save therefore leaves the live header unknown. The scene now calls
the new `invalidate_live_checkpoint(reason)` on every refused real-level
save. The controller's phase-1 owner then refuses with that reason, citing
save-screen:229, instead of reloading an older file.

Test: the new opt-in `godot/tests/death_checkpoint_mark_real.gd` (real game
folder, level script off, synthetic `synthetic_slot` character). On the
mini, all 14 booleans were true, with exit 0 and a clean `check-log`. The
sequence it checks:

1. Save A is marked.
2. Load A is marked.
3. Save B is marked: the path is B and the controller key is the level.
4. A save to a missing directory is refused. The mark is cleared, and the
   reason is recorded.
5. Save C is marked, and the reason is cleared.
6. `run_death_reload` does nothing without a request.

No death runs in that test: an unscripted level has no `Fight` death task.
The refuse branch is exercised by the journeys (below).

**PDR2-3. Split `active`.** `PlayerRecord` gains `object_built` (pointer
+0x43d8) and `object_flag8` (object flag 8). `save_game`'s living test is
`object_built && !object_flag8 && hp > 0` (save-world:247-249). `active`
remains only the placement peer bit (exits-r4:107-117). Test B checks three
cases:

- not built: nothing happens;
- flag 8 set: nothing happens;
- built, with the peer bit clear: the slot still passes the living test.

**PDR2-4. Step 5b bound.** The functions are now `party_wipe(State&, unsigned
players)` and `party_wipe(State&, const optional<Checkpoint>&, unsigned
players)`. The refill covers slots below `players` only (`deathTask`
0x1e76dc-0x1e7710). Test B refills with `players = 1`: slot 1 keeps HP 3
while slot 0 goes to 20. Nothing outside the tests calls `party_wipe`.

**PDR2-5. Wording.** In the doc's "Still Unknown" list, the camera-mode bullet
is replaced. The autosave daemon `save_daemonRun` is not ported, with its
full gate list (`98ea7a6` `docs/audio-save.md:156-162, 341-343`).
`level_flow::save_game` has no production caller (not the controller, not
`saveCheckPoint`). The controller's header therefore comes only from a card
load or a card save.

**PDR2-6. Which "Creature".** A read-only `Host::built_hp_not_above_zero()`
backs the controller method `built_creature_hp_census()`. It returns the
ordinal, class and raw +0x228/+0x22c words of every live built record whose
+0x228 is not a float above 0, plus `stats_ready`. The journey records it on
`in_game_save` and `player_death`. Results, identical in both variants:

- **The death step's fresh checkpoint level** (94 built records,
  `stats_ready` true): 16 records, all class `ScriptGenerator`, ordinals 57-63
  and 65-73, with +0x228 = 0 and +0x22c = 0. No Creature is involved:
  - `build_generator` (script_commands_objects.cpp:286-322) returns
    `Build::Built`, so `adopt_classified` adds these records to
    `built_records_`;
  - `save_built_object_fields` reads every built record with the Creature
    layout, and nothing writes +0x228 in a `ScriptGenerator`.
- **The journey's own `in_game_save`**: the same 16, plus `WarBoarSmall`
  ordinal 17, with +0x228 = `0xbffdfda8` (about −1.98). That is the
  journey's kill.

The scene message is now "a built object's Creature HP word (+0x228) is not
above 0; its save state is unverified". It is matched on the unchanged
native string, so the journey's `in_game_save` classification is unchanged.

**PDR2-7. Gap-slot wording.** The `require_restorable` message, the controller
message, the header comment and the doc now say "the chosen gap-slot policy
(save-world:314-316) is to refuse".

## Part 2: research-backed pin changes

**Act change keeps slot 0** (save-world:194-195, Executed; save-format:294-302).
`begin_change` keeps slot 0 only in the executed restore case. That case is
slot 0 named exactly act 0 row 0's name (`hub`), with the size equal to that
row's, and an owner body that fits in it. The slot keeps its name, size and
body, and restores (new case: `had_saved_state` and `saved_bytes` on return
to `hub`).

Every other case keeps the round 2 behaviour: the whole directory is dropped,
`cleared_slot0` records the key, and a death reload into it refuses. Those
cases are:

- a body larger than the row size;
- a name such as `blood1` whose size the clear cuts;
- a hub sub-area such as `hub_west`, for which only the lookup was checked;
- a slot whose body write failed.

`ChangeOptions::clear_block` (byte 0x4f8f2a) leaves the buffer untouched
while the other act-reset work runs (save-format:296-297). Nothing sets it.

**Twelfth key rewrites the gap** (save-world:35-39, 196, Executed). Once
eleven entries exist, `LevelStates::store` for any name that misses slots
0..9 rewrites the slot-10 body. It uses the gap's offset and kept size, and
keeps the ghost name. No twelfth entry is created. The stale "12th entry
overlaps the data area; that path was not executed" throw is gone. The
`stored()` refusal past ten entries is unchanged (out of scope).

Changed pre-existing assertions. Line numbers are those at the base
`f9176248`; they are the same at `ef91dd0e`.

| file:line (base) | Old | New (line now) | Superseding research |
|---|---|---|---|
| `native/tests/level_flow_tests.cpp:383-385` | `d.act_changed && q == 0 && se.states.entries.empty()` | The same value, `&& se.states.cleared_slot0 == "aa"` (:386-388). Slot 0 here is `aa`, not the executed `hub` case, so it is still dropped. A new block follows (:391-439): the `hub` slot survives and restores; the block byte; a >row-size `hub` and a `hub_west` are dropped and recorded. | save-world:194-195 |
| `native/tests/level_flow_tests.cpp:531-532` | `refuses(store("twelve"))`; `entries.size() == 11` | `store("twelve", …, {0x0c})` is written at offset 10; entries still 11, last `{"eleven", 35000}`; `blobs["eleven"] == {0x0c}`; no `"twelve"` blob (:584-590) | save-world:35-39, 196 |
| `native/tests/level_flow_tests.cpp:1135-1136` | `refuses(store("strife", t[0]))`; `entries.size() == 11` | `store("strife", t[0], …, {0x5e})` is written at the gap offset (sum of slots 0..9); `entries == full_plus`; `blobs["strife"] == {0x5e}` (:1195-1200) | save-world:35-39, 196 |
| `native/tests/level_flow_tests.cpp:549, 557, 561` | `party_wipe(s, cp)`, `party_wipe(s, {})` ×2 | `party_wipe(s, cp, 4)`, `party_wipe(s, {}, 4)` ×2 (:607, 615, 619). The call signature gains the player count; every asserted value is unchanged. | `deathTask` 0x1e76dc-0x1e7710 (audit PDR2-4) |

The third row was not named in the brief. It is the same twelfth-key pin
(a ghost-name store into a full directory), and it failed once the store
stopped throwing, so it is changed for the same research.

Tests added on this branch are corrected without being listed as pins: test
B (living test), test C (PDR2-1) and the second test D case (the `hub` slot
is now kept).

**Step 5 lookup.** The current-level lookup stays. The doc now says
client-path:89's "checkpoint's level" wording is contradicted by the
disassembly. `deathTask` phase 1 passes 0x44fe18 to `save_findAct`
(0x1e76a8-0x1e76b4), and 0x44fe18 is the current base name (`monsters.md:244`;
exits-r4:87).

## Part 3: real checkpoint in the journey (not unblocked)

The question was why a fresh level has a built "Creature" with HP ≤ 0. The
census answers it: the records are 16 `ScriptGenerator` objects, which are
not Creatures. They are built objects whose +0x228 was never written, and the
save reads every built record with the Creature layout.

What research establishes:

- A dead *Creature*: `Creature_saveState` writes HP and, at HP ≤ 0, a
  death-animation handle (save-world:116).
- A Chest: 12 bytes (save-world:115).
- A `ScriptGenerator`: nothing. Whether `world_saveFilter` 0x15d9c8 rebuilds
  it from `objects.ob`, or a save routine writes it, is listed as not done in
  `docs/spec/object-system.md:805-806`. save-world:101-105 names only lights,
  fire, `Line`, `Blocker`, `customVars` and route points.

Porting the dead-Creature record would not unblock this level. Saving
`ScriptGenerator` records would need research that does not exist. So the
refusal at script_commands_objects.cpp:138-139 is unchanged.

Exact blocker: a card save on any level with a built `ScriptGenerator` (and
any built Chest, by the same layout) refuses. The journey's checkpoint level
has 16 of them. FULL_SAVE_LOAD is therefore unaffected by Part 3.

A side note, not changed: the header comment on `save_built_object_fields`
("Only the five currently constructed Creature subclasses participate") is
stale for the same reason. It is left as is, because editing it would have
required a rebuild while disk was under 30 GB.

## Gates (mini)

| Gate | Result |
|---|---|
| Native Release build, `-j 4` | PASS, exit 0, 119 s (after one targeted `champions-level-flow-tests` build for iteration). |
| GDExtensions (template_debug and template_release, level_texture, creature_anim), universal | PASS, exit 0, 710 s. `ext-lipo.txt`: x86_64 + arm64 on all five. |
| Focused CTest | **10/10**: level-flow-exact, level-flow-water-adapter, level-objects, quests, campaign-fight-adopt, campaign-fight-boundaries, player-death-flow, ps2-save, campaign-rewards, campaign-actors. The first run of `level-flow-exact` (iteration build) failed one pin, the third row above; it was fixed before the full build. |
| Full CTest, `-j 3` | **262/262 PASS**, 1,286.9 s. No timeouts. |
| Godot import | Editor import exit 0, 0 error lines. |
| Strict Godot matrix | **54/54 PASS**, 167 s. `check-log`: 105 logs, 0 failing. |
| Combat | `PLAYABLE_PLAY_RESULT` **2,645 checks, 0 failures**, 640 target ticks. |
| FULL_SAVE_LOAD | **300/300**. Continued = uninterrupted = `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, unchanged from round 2. |
| FULL_TRACE | **Unchanged**: 36,402 bytes, sha256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless. |
| Opt-in `death_checkpoint_mark_real.gd` | 14/14 true, exit 0, `check-log` 0. |
| Journeys | Headless 80 steps, rendered 81. Exit 1, the retained full-journey refusal as in round 2. 0 log errors and `check-log` 0 on both. Disc 994 files before and after. |

## Journeys compared step by step with round 2

Round 2: `work/death-reload-r2/playthrough/`. Round 3:
`work/death-reload-r3/playthrough/`. Result sha256: headless
`96f43e1d…e4d7`, rendered `6f7d59a9…27f6`.

- **Every step name and result matches round 2 at every index**, in both
  variants. Headless 80/80, rendered 81/81, 0 differing steps.
- The last step is unchanged: `player_death` `blocked_death_reload`. Phase 1,
  death sequence verified, clip 239 steps, fade 128, Gold/XP/level retained.
- Refused rows are unchanged: headless 26, rendered 27. Screenshots: rendered
  41, as before.
- Refusal kinds are 8 in both variants, as in round 2. Differences:
  - The scene save-refusal kind has the new wording (PDR2-6).
  - Rendered only: the hub-action kind's count suffix went from `count=2` to
    `count=3`. This is the frame-timing variation the round 2 and wave 39
    reports describe; the hub dialog steps are identical.
- The death step's reason is now precise. Round 2 said "the live save buffer
  holds no checkpoint". Round 3 says "the last card save was refused (…), but
  the original's offline card save rebuilds the live save buffer with
  save_build before writing (save-screen.md:229), so the live header … is
  unknown". This is PDR2-2's refuse branch, exercised on real data.
- The new census fields are on `in_game_save` and `player_death` (Part 3).

One earlier round 3 journey run, before the final scene wording, is kept,
ignored, as `work/death-reload-r3/playthrough-try1`. It had the same step
results.

## Not done / remaining

- No successful real-data death reload. The card checkpoint is blocked by
  built `ScriptGenerator` records (Part 3). The autosave daemon and an
  in-session `save_game` caller are not ported.
- Step 5b still refuses in the scene (unchanged).
- The `stored()` refusal past ten entries is unchanged. save-world:42-43
  executes it, so a later round can port it.
- No push, no PR, no `/tmp`, nothing installed, no process killed. No disc
  text, and no mini home path, in tracked files.
