STATUS: PARTIAL. All gates pass. The journeys' ending changed: `player_death` went from `refused` (the death never ran) to `blocked_death_reload`. The full death sequence now runs on the real level, and the reload refuses with a cited reason. A successful death reload is implemented in native code and the client, and covered by synthetic tests. It is not reached in the journeys, because no card checkpoint can be saved on the journey level (Creature state, below).

Branch `codex/play-death-reload-r2`. Base: wave 39 VERIFIED `f9176248`. Commits:

- `34bc55d6`: the native model and the campaign `Fight`.
- `7d234a33`: the controller and the scene.
- `66b2c9fc`: the journey death step.
- This report is a separate commit.

Research: PASS `t3/research-death-reload` `98ea7a6`,
`docs/death-reload-client-path.md`. Audits:
`cursor-runs/audits/research-death-reload-r8.md` and `-r8-fix.md` (PASS,
2 LOW). Pins it builds on:

- `t3/research-save-world` `31a0ca6` (`docs/save-world.md`), here
  "save-world".
- `t3/research-exits-r3` `350fc8b` (`docs/exits-r4-travel.md`, here
  "exits-r4"; `docs/exits-r5-travel.md`, here "exits-r5").

R9-1 is applied: the rank-3 step cites Observed exits-r5:112-117 and
Executed save-world:94-96.

## Where the work ran

All builds, tests, Godot runs and journeys ran on the Mac mini, in
`~/malik-champions/open-champions/worktrees/play-death-reload-r2`.

- The tree was seeded by a local copy of `playable-integration-w40`, excluding
  `build/` and `.godot`. A shallow clone of this branch was laid over it, and
  the 12 untracked files that only w40 had were removed.
- Edits were synced with `rsync --checksum` and the changed files touched.
- One build directory; native builds used `-j 4`.
- Commits were made on the mini. The MacBook only fast-forwarded, after
  checking that its edited files were byte-identical to the mini commit.

## What the client did before (wave 37 r1, wave 39)

- `level_flow::checkpoint` copied the directory into a frozen `Checkpoint`,
  and `party_wipe` restored that copy over the live directory. That contradicts
  the live-buffer contract (client-path:55-63, 137-143).
- The campaign `Fight` death task fed `ps2_save::reload_checkpoint` from a byte
  buffer that normal play never bound. Phase 1 refused; phase 2 and the
  step-5b restart threw unconditionally. The act lookup compared names exactly.
- The client loader (`enable_level_script`) treated a directory miss as fresh,
  even with the gap slot written.
- The journey's death step needed the journey's own card save. That save is
  refused, so the death never ran: `player_death` `refused`,
  `saved_level_reloaded=false`.

## What changed, with citations

Native model, `level_flow` (`34bc55d6`):

- **Live buffer.** `Checkpoint` is now the header only (players, poses,
  quests, gates, level, act, difficulty, camera). It lives in `State::saved`;
  the directory stays live in `State::states`. `party_wipe(State&)` restores the
  header and leaves the directory as later departure stores left it.
  `card_load` replaces header and directory together. Citations:
  client-path:55-63, 115-129, 137-143 (Inferred there, from save-world:285-301);
  save-screen `c8895e2` `docs/save-screen.md:258-279`.
- **`save_game`** (save-world:241-275, Executed).
  - No counted living player (slot below `g_numPlayers`, built, HP above 0):
    nothing happens (:247-252).
  - No act row is a prefix of the name: no build and no store (:255-260).
  - Otherwise the header is built and the current level is stored.
- **Completed game.** No act lookup and no level store, so the level keeps
  its older body (save-world:269-275; client-path:69-71).
- **Step 5 / 5b** (client-path:88-91). The lookup is on the current level.
  - Found: the header with 0xfd, the loaded flag and rank 3.
  - Not found: the current level with 0xfd, HP refilled, nothing else changed.
  - Found but no header ever built or loaded: refused, because the
    uninitialised-buffer behaviour is Unknown (client-path:183).
- **Steps 6, 6a, 7 and 10 on the death load.** These were already in
  `SessionAdapter` and `begin_change`; they are now tested on a death load.
  - The dying level is not stored.
  - The act-change clear is skipped while the act index still updates
    (exits-r4:67-80).
  - At rank 3 the markers and the free-spot search are skipped
    (exits-r4:100-104, exits-r5:124).
  - Z comes from terrain, and only slots passing the HP, peer and medal-slot
    gates are built (exits-r4:107-117).
  - +0x24 is left alone on an act change (exits-r4:118-121).
  - Phase 2 is `death_task_done` (client-path:99).
- **Step 9 stopped-slot rule** (save-world:44-52, Executed; client-path:96).
  `restore_decision` and `require_restorable` decide hit, fresh or gap.
- **Slot-10 gap: refused** (save-world:314-316). The refusal names the key and
  the gap's name, and fires in `begin_change` before the
  `save_levelStateStored` lookup. That lookup already refused the same cases,
  with the generic "past ten entries was not executed".
- **Act change after the checkpoint** (save-world:194, 293-296). An existing
  test pins this client's full directory wipe (`level_flow_tests.cpp:383-385`).
  So the client cannot keep slot 0 as `save_clearBuffer(0)` does. It records the
  dropped key and refuses a death reload into it, or into any key once the
  shift puts the client's slot 9 onto the original's gap. The old act's other
  levels come back fresh, as in the original.
- **Campaign `Fight`.**
  - `bind_death_reload` hands phase 1 (found and not found) and phase 2 to an
    owner loader.
  - The act lookup matches rows by prefix, as `save_findAct` does
    (exits-r4:12-17). Only the comparison with -1 is read.

Client, controller and scene (`7d234a33`):

- A successful card load marks the live buffer with `set_live_checkpoint`
  (save-screen:258-279).
- When the death task finds the level, the controller refuses in three cases:
  - no live checkpoint (client-path:183);
  - the campaign act changed, or an act reset ran, since the checkpoint
    (save-world:194). This client's object directory is not cleared on an act
    change;
  - a written gap slot for the checkpoint level (save-world:44-52, 314-316).
- Otherwise the scene's `run_death_reload` reloads the card image.
  - It uses `restoring_real_level`, so the dying level is not stored
    (client-path:92).
  - It keeps the live object directory instead of the card's copy
    (client-path:120-124).
  - The header supplies the player, quests, pose and HP. The loaded key then
    ends the task.
- Step 5b refuses in the scene with a precise reason: there is no
  current-level reload that keeps the dying record with only HP refilled.
- Scripted level loads now refuse a gap-slot miss with the cited reason,
  instead of loading fresh.

Journey (`66b2c9fc`):

- When the journey's own save was refused, the death step now gives the
  revisited level the character the journey created, turns the level script
  on, starts play and tries a card checkpoint there.
- That save is refused too (below). The death still runs on the scripted
  level, so the death task reaches phase 1.
- The step records the checkpoint attempt, the hit and the fight state.

Not ported, still Unknown: a death fee (client-path:76-78), the real GetReg
values, item fixup with populated bags, per-class load effects, camera
presentation, the alternate-death script (phase 10), and the entry
autosave's camera-mode gate. Without that gate, the client's only checkpoint
source is a card load. No values were invented.

For the research owners:

- Client-path step 5 says the lookup is "on the checkpoint's level". The
  client keeps the round-1 current-level lookup, which matches the 5b wording
  ("request the current level") and the existing tests. The wording should be
  checked.
- save-world:35-39 executes a twelfth key rewriting the gap body. This client
  still refuses a twelfth store (an existing test pins it).

## Gates (mini)

| Gate | Result |
|---|---|
| Native Release build, `-j 4` | PASS. The first build stopped on one compile error in my new test (an ambiguous `objects`, from a namespace). Fixed and rebuilt: exit 0. |
| GDExtensions (template_debug and template_release, level_texture, creature_anim), universal | PASS, exit 0. `ext-lipo.txt`: x86_64 + arm64. |
| Focused CTest | **10/10**: level-flow-exact, level-flow-water-adapter, level-objects, quests, campaign-fight-adopt, campaign-fight-boundaries, player-death-flow, ps2-save, campaign-rewards, campaign-actors. The first focused run failed one check in my new test (stale `built_at` list); fixed and rerun. |
| Full CTest, `-j 3` | **262/262 PASS**, 1,610.5 s. No test timed out, so none was rerun alone. |
| Godot import | Quick editor import exit 0. Then a complete `--import` (needed without `.godot`, as in wave 39): exit 0, 0 error lines. |
| Strict Godot matrix | **54/54 PASS**, 233 s. `check-log`: 105 logs, 0 failing. |
| Combat | `PLAYABLE_PLAY_RESULT` **2,645 checks, 0 failures**, 640 target ticks. |
| FULL_SAVE_LOAD | **300/300** presents. Continued = uninterrupted = `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`. |
| FULL_TRACE | **Unchanged**: 36,402 bytes, sha256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless. |
| Journeys | Headless 80 steps, rendered 81. Exit 1, the retained full-journey refusal as in wave 39. 0 log errors in both, and `check-log` 0 on both logs. Disc 994 files before and after. |

Disk: one check found 29 GB free just after I had started the extension
build. I stopped only the PIDs I had started (the gate script and its cmake
tree), waited inside the turn until 30 GB were free, and rebuilt from the
start. All other builds started with at least 30 GB free.

## Journeys compared step by step with wave 39

Wave 39 results: `work/playable-wave39/playthrough/*/result.json` (copied with
the seed). New results: `work/death-reload-r2/playthrough/`.

- Result sha256: headless `162193bb…1d74`, rendered `624a7de8…a3b3`.
- Every step name and result matches wave 39 at every index, in both
  variants, except the last:

| Variant | Index | Wave 39 | This round |
|---|---|---|---|
| Headless | 79 | `player_death` `refused` | `player_death` `blocked_death_reload` |
| Rendered | 80 | `player_death` `refused` | `player_death` `blocked_death_reload` |

How the ending changes. In wave 39 the step stopped before any death: the
journey save had been refused, so there was nothing to load, and it recorded
only `saved_level_reloaded=false`. Now, in both variants:

1. A checkpoint is attempted. The revisited level is loaded with the level
   script and the created character, and play starts. The card save is
   refused, with the scene's status "Save refused: this level has Creature
   death state that cannot be restored yet."
   - Cause: `Host::save_built_object_fields` refuses a built Creature whose HP
     word +0x228 is not above 0.
   - The level was fresh (alive enemies) and had played 30 ticks, so this is
     not the journey's kill.
   - It is the existing per-class save boundary, which the research leaves
     Unknown. It was not touched.
2. The diagnostic lethal hit is queued. The real death sequence then runs:
   - `death_sequence_verified=true`;
   - HP 0 and action state 4;
   - the native death clip ran for 239 steps;
   - menu closed;
   - wipe countdown and revival countdown both 0;
   - fade 128;
   - Gold, XP and level retained;
   - fight not failed.
3. Phase 1 finds the level in the act table, and the controller refuses the
   reload with: "death reload refused: the live save buffer holds no checkpoint
   (no card load and no save_game build in this session); the original's
   uninitialised-buffer behaviour is Unknown
   (research-death-reload@98ea7a6 docs/death-reload-client-path.md:183)".
   - The result is `blocked_death_reload`, `respawn_completed=false`.
   - The rendered capture `13-player-reload-refused.png` shows the dead player
     and this reason.

Other per-run differences:

- Refused rows: headless 27→26, rendered 28→27. The only change is the last
  step.
- Screenshots (rendered) 38→41: captures 11, 12 and 13 of the death branch
  now run.
- Refusal kinds:
  - Headless stays at 8. Only the count suffix of the hub-action kind changed
    (`count=1` → `count=2`).
  - Rendered went from 6 to 8. The two hub-action kinds that wave 39's
    rendered run lacked are back.
  - Wave 39's report traces these to frame timing (headless 1/2, rendered 0
    at low fps). Wave 38 had all 8. They are not from this change: the hub
    dialog steps are identical.
- `loot_pickup` passed in both: 10→11, dropped, walked, picked up, 52 walk
  ticks, 0 retries. `rendered_pickup_verified` is still false.

Iterations: six earlier journey runs on this branch are kept, ignored, under
`work/death-reload-r2/playthrough-try1..6`.

- try1–2: the checkpoint save was refused; on the unscripted level the death
  never ran.
- try3–5: added diagnostics. The death without a character failed in the
  fight ("player death animation: model/weapon-class clip unavailable").
- try6: identical to the final run.

The final run is the one reported.

## Not done / remaining

- A successful in-journey death reload needs a checkpoint, and every one is
  refused on this level. Possible sources:
  - a card save, refused by the Creature-state boundary;
  - the script's `saveCheckPoint`, which no journey level calls;
  - the entry autosave, whose camera-mode gate is unmodelled.

  The success path (`run_death_reload`, the owner callbacks, phase 2) is
  covered natively by the synthetic `level_flow` death-reload cases. The
  scene-level success path has not been executed against real data.
- The step-5b restart refuses in the scene, with a precise reason.
- The gap-slot policy is refuse.
- The act-change slot-0 survivor refuses, because an existing test pins the
  full wipe.

No push, no PR, no `/tmp` on the mini, nothing installed, no process killed
by name. No disc text and no mini home path in tracked files. Existing
tests and baselines were only added to.
