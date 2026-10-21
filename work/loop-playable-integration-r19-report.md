STATUS: COMPLETE

Wave 19 of job playable-integration is done. All three branches are merged, in the order given, and none was backed out: `codex/script-commands-2`, `codex/netplay` and `codex/creature-blend`.

Results, all on the first run:
- Real-data sweep: **111/111**, with scripts on and with scripts off.
- CTest: **213/213** on arm64 and on x86_64 (Rosetta).
- Godot: dev matrix **50/50**, export matrix **18/18**.
- **FULL_TRACE is unchanged at `374e40c9…2035`.**

Setup:
- Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch `codex/playable-integration`, start `0f3cafc`.
- Local commits only. Nothing was pushed, and no PR or issue was opened.
- `~/champions-re` was not touched. The disc copy was only read. Its counts and logs stay in the gitignored `work/private/`.
- combat-ctors, dialog, items, script-session, hud-overlay and menus were not merged.

## Merges

| Branch | Tip | Merge | Conflicts |
|---|---|---|---|
| script-commands-2 | 30bc2a6 | 6df4a10 | none |
| netplay | c60759e | 52133a3 | `native/CMakeLists.txt` (both sides additive, both kept) |
| creature-blend | fb4f433 | ee4d03a | `real_data.gd`, `real_data_scene.gd`, `REAL_DATA.md` |

Each merge was checked before its commit:
- **script-commands-2:**
  - Release build, and the script/quests ctests (7/7).
  - The four extension builds and the `script_commands` smoke.
  - The 111-level sweep with scripts on.
  - Both script measure tools. Their output is byte-identical to the branch's own run.
- **netplay:**
  - Release build.
  - `champions-netplay-tests`: 114 checks, with the branch's four FNV digests.
  - `champions-netplay-level-measure` on the disc: 2, 3 and 4 peers, synced, 0 mismatches, save/load proven.
- **creature-blend:**
  - Release build, and the model-texture, skinning and real-data ctests.
  - The extensions, and `creature_blend_smoke` 14/14 in GL Compatibility and in Forward+.
  - `scene_animation_preview` (42) and creature-anim (32).
  - `champions-real-data-measure`.
  - Both sweeps, plus the creature-preview sweep.

## How the audit rules were kept

- **A refused command still stops the script.** script-commands-2 sends every new refusal through the same `refuse()` / `RefusedResult` path. This covers the stub reads, the `select*` forms, and the callee refusals for a live charm target, a missing music volume and an unknown fade level. `scripts.refusal_stops` stays at 39.
- **`setCFlag` / `selectSetCFlag` still refuse on placeholder objects.** Placeholder-object refusals (`stub_refusals`) go from 102 to 205, as expected.
- **One audio mixer per process (AUD-4).** script-commands-2's `Playback` is private to the host, and no merged branch touches `real_audio.gd`. The host also stopped reading the binder's zero `g_musicVolume`, which re-closes AUD-1.
- **The per-run `user://` folders are kept.** No branch touches `playable_wave_smoke*.gd`.
- **AntQueen, Skeleton and Soul stay refused.** `wave12-combat-extensions` shows "constructor is not executed" in the dev run and in the exported run.
- **Netplay kill plane −100000.**
  - `level_flow::LevelRow{}.kill_plane` is written only into the netplay session's own world. The Full play session does not use netplay, so FULL_TRACE does not move.
  - The trace record is 72 bytes, and the save format is v2.
- **Two-pass draws under perf's batching.**
  - Level chunks: perf packs only order-independent batches into atlas pages. `level_texture_stream.cpp` `order_dependent()` returns true for any environment-pass material, so those groups keep their own node, chunk/batch slot and `sorting_offset`.
  - Creature models: two-pass creatures draw through one composite `ShaderMaterial` per mesh state (`real_creature.gd`), and perf does not batch them. The environment-then-base order inside the shader is unchanged.
  - I added a sweep count so this is measured, not only read: `display.environment_groups` 4,797 and `display.environment_in_atlas` **0** over the 111 levels.

## Conflict resolution (creature-blend)

- **`real_data.gd`.** Kept wave 18's `stream_textures`, the level-flag word, and the water-line and additive fields. Added the branch's `_load_environment` (the first `.etex` in `env.lmp`), `creature.set_environment`, and the `environment` result key.
- **`real_data_scene.gd`.** The `creature.two_pass` / `creature.environment` counts sit in the preview block. The wave-18 `--wade` loop follows it. The new environment-group counts sit after the constant-light counts.
- **`REAL_DATA.md`.** Both sections were kept: wave 18, then the creature-blend audit section.

## (1) Real data, 111-level sweep (`work/run-slices-r19.sh`, 60 ticks)

The final-tree runs are `r19bl --scripts` and `r19bln`.

| Run | Tried / loaded / played | Refused | Failed checks | Refusal list |
|---|---|---|---|---|
| `r19bl --scripts` | 111 / 111 / 111 | 0 | 0 | empty |
| `r19bln` (no scripts) | 111 / 111 / 111 | 0 | 0 | empty |

These counts equal wave 18:
- 6,549 ready ticks and 119 checks;
- levels and starts, water, pieces (drawn claimed 14), display and lights;
- scripts started 76, faults 0, refusal stops 39.

Every count that changed, and why:

| Count | Wave 18 → 19 | Cause |
|---|---|---|
| `scripts.stub_refusals` | 102 → 205 | script-commands-2: `setMonsterVal` (63) and `setCreatureParams` (36) are routed now and refuse at the stub check; `charmMonster` adds 4 |
| `refused.setMonsterVal` / `executed` | 75 / 0 → 63 / 12 | 12 calls name no object; the binder runs over an empty list (1 level) |
| `refused.setCreatureParams` / `executed` | 40 / 0 → 36 / 4 | same, 4 calls, 1 level |
| Executed / refused command calls | 4,192 / 669 → 4,208 / 653 | the 16 calls above |
| `scripts.callee_refusals` | new: 0 | script-commands-2 |
| `display.environment_groups` / `environment_in_atlas` | new: 4,797 / 0 | added this wave (perf batching check) |
| `memory.static_peak_bytes` | +0.6 MB summed | creature-blend loads the level's 128 × 128 environment image |
| `time.*`, `audio.frames_pushed` | | run to run |

With scripts off, only the environment counts, peak memory and timing changed.

**Measure tools** (all counts are in REAL_DATA.md, wave 19):
- `champions-script-commands-measure` and `champions-quests-measure` are byte-identical to the branch's own run.
- Against the wave-17 build, script-commands-measure shows the same 102 → 205 routing shift as the sweep, plus the SC-8 stop-rule changes:
  - The wait-queue key now includes the Pawn globals, the quest variables and the post-action name. Counting waits therefore resume, up to the labelled measurement cap.
  - `scripts_stopped_on_refusal` 38 → 39.
  - `getv` 142 → 148 and `callScript` 20 → 38.
  - `gamePaused` 49 → 472 and `addParticleOnObject` 24 → 240.
  - `addDeferedCall` 50 → 230, and `sin1000` / `cos1000` 24 → 240.
  - `getScaledTime` 7 → 52, `countPlayersAlive` 8 → 64 and `getHealthUsed` 4 → 20.
  - `findDistance` 0 → 6 and `closestPlayerDist` 0 → 2.
- The quests tool differs from `w17-quests-measure.txt` only in `getv` 142 → 148.
- `champions-real-data-measure` gives the branch's blend counts: 1,782 meshes drawn with the environment pass, 149 two-pass meshes without the env tag, and 0 / 0 for the two refusal counts.

**Creature-preview sweep** (`--levels=28`):
- 111 previews, 107 textured, 76 two-pass and 107 with the environment image. All equal the branch.
- BLEND-7 shows on 99 runs.
- Against the branch's wave-16-based run, the other changes are:
  - the wave-18 display and water counts;
  - one door fewer captured (`doors_opened` 6 → 5, `checks.passed` 131 → 129).
- The door change fits perf's resident-cell drawing, but I did not isolate it with a wave-18 preview run.

## (2) CTest

| Arch | Result | Time | Slowest |
|---|---|---|---|
| arm64 | **213/213**, first run | 1,101 s | session-facing-exact 194 s, player-exact 174 s, netplay 154 s |
| x86_64 (Rosetta, `build/native-x64`, Mach-O x86_64) | **213/213**, first run | 1,620 s | session-facing-exact 306 s, player-exact 218 s, native-save-indexed 209 s, netplay 205 s |

- The new tests since wave 18 are `script-effects` and `netplay`.
- There were no timeouts.
- There were 0 compiler warnings on either arch. The existing ld duplicate-library note still appears.
- Machine load was about 30–40 from other worktrees.
- `champions-netplay-tests` output is byte-identical between arm64 and x86_64. That includes the four trace digests.

## (3) Godot

- All four universal extension builds pass. They were rebuilt after merges 1 and 3. Netplay links no extension.
- **Dev matrix** `work/playable-wave19/run-godot.py`: **50/50** on the first run.
  - That is wave 18's 48 plus `creature_blend_smoke` in GL Compatibility and in Forward+ (14 each).
  - Every other check count equals wave 18: full combat 2,620, FX 1,876, animation 1,809, play-short 1,341, level 1,643, textures 1,909, audio 2,578, UI 1,377, combat 1,417, wave12 22, creature-anim 32 / 25 / 231.
  - `native_fx_visual` had no refusals.
- **FULL_TRACE** is `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes, unchanged.
- **Export:** `--export-release 'macOS universal'` → `build/exports/wave19/` (`OpenChampions.zip`, `Open Champions.app`, x86_64 + arm64).
  - ZIP SHA-256 begins `9ddf4309`.
  - Archive dylib `83e95d3a…` and level-texture dylib `92410c8e…`. Both are packaged byte-identical.
  - No `.gob`, `.lmp` or `.iso` member is in the zip, and no `rta-disc` or `private-validation` string is in the app.
- **Export matrix** `run-export.py`: **18/18** on the first run.
  - The 36,000-tick 1280×720 Full run passed **108,826 checks with 0 failures** and 63 captures, in 198 s.
  - Exported headless passed 2,620. 960×720 and 800×600 passed 2,746 each.
  - Every FULL_TRACE is `374e40c9…2035`.
- **Exported app on real data** (60 ticks): 12/12, 12/12 with `--scripts`, and 3/3 arena levels (69–71) with `--scripts`. There were 0 failures. Against wave 18 the changes are:
  - creature `two_pass` 10 and `environment` 12;
  - environment groups 351 / 0 in an atlas (arena 88 / 0);
  - stub refusals 3 → 19, from the same routing as the sweep;
  - BLEND-7 (`fx.runtime | material must consume vertex colors`) on 9 of the 12 preview runs.

## Regressions kept / open

1. **The wave-18 refusals are unchanged.**
2. **BLEND-7 stays open.** `native_fx_renderer.gd` refuses the per-surface `ShaderMaterial`s that a mixed-pass creature preview gets. The fix belongs to the FX renderer's owner. Because of it, the exported 12-level runs now list a refusal, where wave 18's list was empty.
3. **These script commands stay refused in play:** the five text setters (no `text_lookup` port), `startMusic` and boss music (no supplied music volume), and `hudFadeScreen` before a reset.
4. **Netplay is an offline co-op simulation.**
   - It runs on the lockstep transport, with no Godot net scene, no sockets and no ghost prediction.
   - After its first swing, a Full combat player in the netplay fixture does not walk again.
   - Walking runs carry no creature. The branch recorded both limits.

## Commits

- 6df4a10, 52133a3, ee4d03a: the three merges. ee4d03a also holds the environment-group sweep count.
- The final commit holds `INTEGRATION.md` (wave-19 section), `REAL_DATA.md` (wave-19 section) and this report.

## Open / unverified

- **The creature-preview door delta (6 → 5)** was not isolated against a wave-18 preview run.
- **Netplay was not checked against the original.** Its proofs are peer-vs-peer, run-vs-run and arch-vs-arch only. There is no original-client interop and no research `ref_combat_full` 2-player digest.
- **No windowed captures** of real two-pass creatures were inspected by eye.
- **No exported-app frame-time run, and no manual play.**
- **Windows and Linux exports** were not rebuilt or run. There was no native Intel run.
