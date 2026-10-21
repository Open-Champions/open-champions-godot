STATUS: DONE, with one conflict left for a decision (AntQueen and Skeleton cannot stay in the default Full fight; see section 1).

# combat-ctors, round 1 (2026-10-21)

Branch `codex/combat-ctors`, worktree `~/Projects/open-champions/worktrees/combat-ctors`, created from `codex/audit-fixes` bffec5f. The tip is `ed062c3`. Commits are local only. Nothing was pushed, and nothing was written in `~/champions-re` or in any research checkout. The private disc was not used.

Evidence: `t3/research-combat-ctors` at e9777eb (`~/Projects/champions-re-wt/research-combat-ctors`):
- `98c081a`: `docs/spec/potion-ctors.md`, `ref_potion_ctors.py`;
- `22408a1`: `docs/spec/music-volume.md`, `ref_music_volume.py`;
- `e9777eb`: `docs/spec/combat-ctors.md`, `ref_combat_ctors.py`.

The generators pin every research blob they read by SHA-256. The vectors come from the pinned reference Python run on authored images, never from native output.

| Commit | What |
|---|---|
| 04d2f1a, merged as af0bb78 | Music volume. Done by a sub-agent on `codex/combat-ctors-music`, which I reviewed and merged. |
| 99adfe6 | AntQueen, Skeleton and Soul ported exactly: the `combat_ctors` library and the `combat-ctors-exact` test. |
| 0ec6644 | The three classes built and run in the combat World. The Soul is back in the Full fight. |
| 1315c3a | Potion endings for kinds 1 and 2: LooseFire with its Fire objects, and poison gas. Test `potion-ctors-exact`. |
| ed062c3 | A mid-fight save test with a Soul and burning fires; the Godot wave-12 smoke; docs. |

## 1. AntQueen, Skeleton, Soul

**What is ported.** `native/src/combat_ctors.cpp` runs on the arena model:
- `AntQueen_ctor`, `Skeleton_ctor` and `Soul_ctor`;
- the set-states 0x237918, 0x2a7768 and 0x2aac38, for every state number;
- the three run functions up to their switch;
- `Soul_getModel`.

The listed corrections are all in:
- AntQueen and Soul store state 0 directly, so +0xf4 = 0 and no entry draw is made.
- The Skeleton constructor calls `setState(0)`.
- Skeleton and Soul return when inactive; AntQueen does not.
- Soul's 0x21 burst is flat.
- On airhub, every Soul request becomes 0x22 before the filter.

**Exactness test.** `combat-ctors-exact` replays 480 reference cases (4,404 operations and 20,976 recorded call-outs). It compares the call names and arguments, every stored byte, and the shared draw count. As a check, I made the Soul burst non-flat; 100 comparisons failed.

**In the World** (`World::spawn_class`, Full composition only):
- **Placement.** The constructors run on the authored placement, as for Gnaw. The placement's active bit is cleared, as `Creature_ctor` leaves it.
- **Tables.** The class tables use only documented slot words; slot sources are in the docs.
- **Run.** Slot 4 runs the executed prologue, then the round-5 switch for states 0 and 1.
- **Creature_setCharm.** I used its Observed body (skills-35-68.md). This puts the Soul on team 0 with a permanent charm.
- **Recorded call-outs.** The anim push calls, `Creature_init`, the event times, and holder slots 1 and 21 are recorded and not executed, as for Gnaw. Slot 21 is presentation per D37.
- **New service.** `model_buildHideMask` is now a required service. The flat fixture answers all ones.

These stay explicit refusals:
- any state past 1;
- `Soul_takeDamage` and `Skeleton_takeDamage`;
- `Skeleton_isEnemy`, and Skeleton slot 12;
- `Skeleton_shatter` and `AntQueen_pickFlyDest`;
- `anim_findTrack`;
- the holder's element slots 2–4, which draw the shared seed, and slot 11;
- the three inline destructors, which are named but whose bodies were not read.

AntQueen slot 2 is the documented plain jump to `Creature_takeDamage`.

**Conflict: AntQueen and Skeleton in the Full fight.** Only the Soul went back into the Full fight: slot 6, at (-60, 100). Slots 4 and 5 stay empty. Measured on the round-5 placement:
- The Skeleton's first target scan, at ready tick 32, calls `Skeleton_isEnemy`.
- The AntQueen sees the player and enters state 3 at tick 33.

Both are refusals you asked me to keep, so either creature would stop the default fight at about tick 32. That would break fixture-save, the idle fight and the Godot full-combat smoke. Putting them back needs one of two decisions:
- research that executes `Skeleton_isEnemy` and the AntQueen switch past state 1; or
- accepting a Full fight that stops at tick 32.

Both creatures can still be built through `spawn_creature`. The wave-12 smoke records the result:
- The Skeleton stops at `Skeleton_isEnemy` at tick 32.
- The AntQueen, hit by the attacking player, bleeds into `GutChunk_ctor` at tick 28. That call has no World handler; this was already true for Gnaw.
- A Skelight placed at (0, 80) attacks the team-0 Soul and stops at `Soul_takeDamage` at tick 30.

## 2. Thrown potions 1 and 2

In `combat_runtime_potions.cpp`:
- `LooseFire_ctor` runs `fx_spawn` id 5. That makes 3 or 4 Fire objects: one shared draw for the count, three per point, and one per `Fire_ctor`.
- `Fire_run` makes no shared draws. It draws from the fire's own generator only and is cosmetic.
- `LooseFire_run` deletes the object at F + 121.
- `LooseFire_onTouch` acts every 20th frame. Descriptor type 0x15, for a player or creature toucher, stays refused and the refusal names its value. Any other toucher gets an empty-descriptor `damage_apply`.
- `poisonGasSpell_ctor` is a plain object with one shared draw.

`potion-ctors-exact` replays 126 reference cases through the World. As a check, I changed the fire life to 99; 84 checks failed.

Still refused:
- `Fire_dtor`, `LooseFire_dtor` and `poisonGasSpell_dtor`. They are named in `runtime.tsv`, but their bodies were not read; `runtime.md` 4.3 gives only the general shape.
- `poisonGasSpell_run`, from combat round 6.

As a result, potion 1 now stops at ready tick 101 at `Fire_dtor`, and potion 2 at tick 2 at the gas run. The deletion request at F + 121 is made, but its destructor refuses.

The round-4 corpus replay now checks the first LooseFire's place and owner, and the gas cloud's words. Its later decals cannot match: the research stubbed the constructor, which draws the shared seed 11 to 18 times.

## 3. Music volume (sub-agent, reviewed)

`music_volume.{hpp,cpp}`:
- `save_init` sets 0.5.
- The options file word +0x08 is copied unclamped at boot and by `game_addCoreTasks`.
- The slider moves in steps of 1/120, clamped to 0..1, behind its repeat timer.
- The commit and the gate are ported. Music plays at exactly 0x3d4ccccc.

The AUD-1 refusal is removed: level and boss music now play. The Godot bridge gets `set_options_file_music`. `music-volume-exact` replays 1,048 records.

## 4. FULL_TRACE and mid-fight saves

**FULL_TRACE** changed from `374e40c9…2035` (21,990 bytes) to `41e063910f5d82df9fa5ed2371f4544de96c9aefebcdc25792327b87e14fee75` (26,856 bytes). The only cause is the Soul in slot 6:
- its placement's GameObject shared draw shifts every later shared draw;
- every trace line gains its actor;
- its runs add own-generator and demon-test activity.

**fixture-save:**
- The 300-tick Full fight, with 19 demon tests and 2,169 Holy Shield sample points, still continues byte for byte after the mid-fight load.
- The new `combat_ctors_extensions` case covers a spawned Soul plus potion 1. It saves at tick 60 with 19 fires burning, loads, and continues byte-identically to the same stop at tick 105, at `Fire_dtor`.

## 5. Verification

- **arm64 Release** (`cmake --build build/native -j 4`): `ctest -j 3` passed 209 of 209 in 850 s. The `fixture-save` binary in that run predated the new save case, so I rebuilt it and reran it alone: passed. Log: `work/combat-ctors/ctest-arm64.log`.
- **x86_64 Release** (`build/native-x64`, `CMAKE_OSX_ARCHITECTURES=x86_64`, binaries checked as x86_64): `ctest -j 3` passed 209 of 209 in 1,183 s, including the new save case. Log: `work/combat-ctors/ctest-x64.log`.
- **Godot.** All three extensions were rebuilt from this branch (template_debug Release), and I ran the same 45-command matrix as audit-fixes.
  - 44 of 45 exited 0. `wave12-combat-extensions` failed on two expectations of the new behaviour. After I updated them, the rerun passed with 28 checks and 0 failures.
  - `combined-full-combat`: 2,619 checks, 0 failures.
  - Logs: `work/combat-ctors/godot-*.log`.

## 6. Open

- The conflict in section 1: AntQueen and Skeleton need research before they can stay in the Full fight.
- The potion destructors and `poisonGasSpell_run` are not ported.
- The composed kind-1 ending (round 4 with the real constructors) has no research run. The research's own section 4 says so. Each body is exact, but the sequence they make together is not compared.
- Fire construction from `fx_spawn` 5 for the arrow kind-1 hit and the Effects observer is unchanged and still refused there.
- Music: nothing applies the session-start copy automatically when a client session starts. How that maps onto the original is not documented.
- A blood hit on any creature still stops at the unhandled `GutChunk_ctor`. This predates the job.
