STATUS: COMPLETE

# Playable integration wave 20 — script session, items, play HUD

Worktree `~/Projects/open-champions/worktrees/playable-integration`, branch `codex/playable-integration`, started clean at `63c40ab` (wave 19). The three requested finished branches were merged in order, one at a time. No other branch was merged. All commits are local; nothing was pushed and no PR or issue was opened. The owner’s disc copy was read only, with counts-only output in gitignored `work/private/`. `~/champions-re` was not touched.

| Branch | Source tip | Local merge commit | Resolution |
|---|---|---|---|
| `codex/script-session` | `b6b735b` | `3a3b261` | Kept wave 19 callee routing and script-session seed, object factory, player count and stop counts. |
| `codex/items` | `290b951` | `919b6e4` | Kept both host feature sets. Difficulty uses `script_items::DIFFICULTY_IMAGE` once during host construction. |
| `codex/hud-overlay` | `fbffecb` | `ded5aab` | Kept both PS2 import and play HUD in `play_flow.gd`. |

## Verification before each merge commit

- **Script session:** the `script-commands`, `script-effects` and `level-factory` focused CTests passed; the Godot script command smoke passed. The 111-level scripts-on sweep passed 111/111 with zero failed checks and an empty refusal list. A first full native build caught a measurement-loop conflict-resolution error; the code was corrected and its target rebuilt after the next merge.
- **Items:** the merged native targets built in Release. Six focused CTests (`script-items`, `script-commands`, `script-effects`, `level-factory`, `level-objects`, `quests`) and the Godot script smoke passed. The scripts-on sweep passed 111/111 with zero failed checks and an empty refusal list.
- **HUD:** native `ui-layout-exact` and `ui-layout-dynamic-exact`, the authored Godot play HUD smoke, and the Godot script smoke passed. The HUD real-level counts check initially failed because a relative game-folder argument resolved under Godot’s project path. It passed with the absolute owner-selected folder: health bar and status frame drawn, no failed checks. The 111-level scripts-on sweep passed with the HUD on all 111.

## Invariants and conflict resolution

- A used refused answer still stops its script entry via `refuse()` or `RefusedResult`. `setCFlag` and `selectSetCFlag` still refuse on placeholders. The merged item handlers also stop on a refusal part-way through a call. Focused script tests pass.
- The offline play session explicitly uses `SeedPolicy::OfflineSession`; `getRand` answers from its session generator. Unmodelled object-load draws are counted. The host default remains `SeedPolicy::Exact`, and netplay has no script-host binding. The accepted offline policy does not change netplay’s own world or its kill plane.
- `script_items::DIFFICULTY_IMAGE` initializes difficulty 1 in the host. `start_single_player()` adds the one player and mirrors state; it no longer writes difficulty a second time.
- The single audio mixer uses `end_session()` across levels. The host’s playback remains private. The per-process `user://` smoke folders remain. AntQueen, Skeleton and Soul constructors remain refused. Netplay alone writes its kill plane to its own world.

## 111-level real-data sweeps, 60 ticks

Final runs are `r20h2 --scripts` and `r20h2n`, using the same four-slice counts-only recipe as wave 19. Both are **111 tried / 111 loaded / 111 played**, zero failed checks, empty refusal lists. The stable shell script was rerun after a mid-run edit made its wrapper exit with a syntax error, even though all its individual slices had exited 0. The final rerun completed normally with distinct cache folders per tag.

| Measure | Wave 19 | After script session | After items | Final HUD |
|---|---:|---:|---:|---:|
| Executed script calls | 4,208 | 8,472 | 17,128 | 17,128 |
| Refused script calls | 653 | 651 | 695 | 695 |
| `getRand` executed | 0 | 367 | 1,457 | 1,457 |
| Script refusal stops | 39 | 35 | 16 | 16 |
| Objects registered | 17,398 | 15,901 | 15,901 | 15,901 |
| Checks passed | 119 | 119 | 119 | 230 |

**Script session** accounts for the first change: its offline seed gets five levels past old `getRand` refusals, the classified factory builds 324 objects, leaves 15,577 unbuilt, and skips 1,497 records. One player at difficulty 1 makes 8 quest lines and 32/13 mission available/complete bits visible. Scripts then run more polling, deferred and particle work. The 367 `getRand` calls and four fewer refusal stops match the branch’s expected changes; wave 19’s 16 new callee executions explain why the integrated executed total is 8,472 rather than the branch’s wave-17-based 8,456.

**Items** accounts for the second change. Has/count/remove and remove-all calls now execute. Their previous refusals had stopped entries; subsequent scripts reach 1,090 more `getRand` calls, more polling, particle work and other commands. Some later commands still refuse, so total refused calls rise 651 → 695 while refusal *stops* fall 35 → 16. `givePlayerItem` remains refused when the session has no item tables. Placeholder refusals rise 205 → 265 as the newly reached scripts target more unbuilt objects. The count tool found 120 scripts and 5,660 native calls, as the items branch reported.

**HUD** adds one check for each of 111 levels, 111 health bars, 111 status frames, 136 documented-art identities loaded across four slice processes and four font loads. Scripts and all level, display, water, piece, light, start and combat counts remain unchanged. With scripts **off**, wave 19 → wave 20 changes only these HUD keys and the 119 → 230 check total among deterministic counts. Timing, audio frames and process memory are measured runtime values, shown below.

The following table enumerates every changed deterministic numeric key in the scripts-on aggregate. **SS** is script-session’s seed, factory and single-player state; **I** is items’ handlers and the script continuation they enable; **H** is HUD presentation. A dash means the key was not reported at that stage (for command call keys, this is zero calls). Keys absent from this table are unchanged. All values are aggregate counts across the same 111 levels.

| Count key | Wave 19 | Script session | Items | Final HUD | Attribution |
|---|---:|---:|---:|---:|---|
| `campaign.mission_available_bits` | 0 | 32 | 32 | 32 | SS |
| `campaign.mission_complete_bits` | 0 | 13 | 13 | 13 | SS |
| `campaign.quest_lines` | 0 | 8 | 8 | 8 | SS |
| `campaign.vars` | 158 | 160 | 183 | 183 | SS, I |
| `campaign.vars_nonzero` | 56 | 58 | 72 | 72 | SS, I |
| `checks.passed` | 119 | 119 | 119 | 230 | H |
| `executed.addDeferedCall` | 691 | 811 | 1,702 | 1,702 | SS, I |
| `executed.addParticleOnObject` | 472 | 1,608 | 4,644 | 4,644 | SS, I |
| `executed.callScript` | 38 | 41 | 103 | 103 | SS, I |
| `executed.cos1000` | 472 | 1,608 | 2,316 | 2,316 | SS, I |
| `executed.countPlayersAlive` | 167 | 167 | 249 | 249 | I |
| `executed.countQuestItems` | — | — | 33 | 33 | I |
| `executed.findDistance` | 6 | 6 | 9 | 9 | I |
| `executed.gamePaused` | 955 | 1,074 | 2,460 | 2,460 | SS, I |
| `executed.getEvil` | 11 | 11 | 19 | 19 | I |
| `executed.getNumPlayers` | 13 | 14 | 36 | 36 | SS, I |
| `executed.getRand` | — | 367 | 1,457 | 1,457 | SS, I |
| `executed.getScaledTime` | 138 | 138 | 256 | 256 | I |
| `executed.getv` | 490 | 732 | 979 | 979 | SS, I |
| `executed.hideMonster` | 7 | 8 | 8 | 8 | SS |
| `executed.onePlayerHasItem` | — | — | 107 | 107 | I |
| `executed.playerHasItem` | — | — | 91 | 91 | I |
| `executed.postAction` | 32 | 32 | 39 | 39 | I |
| `executed.removeAllItems` | — | — | 1 | 1 | I |
| `executed.removeAllWeapons` | — | — | 1 | 1 | I |
| `executed.removePlayerItem` | — | — | 37 | 37 | I |
| `executed.setSaveInhibit` | 16 | 16 | 18 | 18 | I |
| `executed.setv` | 82 | 84 | 99 | 99 | SS, I |
| `executed.showMonster` | — | 1 | 1 | 1 | SS |
| `executed.sin1000` | 472 | 1,608 | 2,316 | 2,316 | SS, I |
| `executed.startWithSwoopDone` | — | — | 1 | 1 | I |
| `hud.play_art_loaded` | — | — | — | 136 | H |
| `hud.play_art_refused` | — | — | — | 0 | H |
| `hud.play_drawn.health_bar` | — | — | — | 111 | H |
| `hud.play_drawn.status_frame` | — | — | — | 111 | H |
| `hud.play_font_loaded` | — | — | — | 4 | H |
| `hud.play_levels_drawn` | — | — | — | 111 | H |
| `levels_executed.addDeferedCall` | 17 | 19 | 39 | 39 | SS, I |
| `levels_executed.addParticleOnObject` | 1 | 3 | 7 | 7 | SS, I |
| `levels_executed.callScript` | 18 | 19 | 23 | 23 | SS, I |
| `levels_executed.cos1000` | 1 | 3 | 4 | 4 | SS, I |
| `levels_executed.countPlayersAlive` | 5 | 5 | 6 | 6 | I |
| `levels_executed.countQuestItems` | — | — | 12 | 12 | I |
| `levels_executed.findDistance` | 1 | 1 | 4 | 4 | I |
| `levels_executed.gamePaused` | 6 | 8 | 18 | 18 | SS, I |
| `levels_executed.getEvil` | 8 | 8 | 16 | 16 | I |
| `levels_executed.getNumPlayers` | 13 | 14 | 14 | 14 | SS |
| `levels_executed.getRand` | — | 5 | 18 | 18 | SS, I |
| `levels_executed.getScaledTime` | 3 | 3 | 4 | 4 | I |
| `levels_executed.getv` | 40 | 40 | 52 | 52 | I |
| `levels_executed.hideMonster` | 4 | 5 | 5 | 5 | SS |
| `levels_executed.onePlayerHasItem` | — | — | 13 | 13 | I |
| `levels_executed.playerHasItem` | — | — | 16 | 16 | I |
| `levels_executed.postAction` | 24 | 24 | 27 | 27 | I |
| `levels_executed.removeAllItems` | — | — | 1 | 1 | I |
| `levels_executed.removeAllWeapons` | — | — | 1 | 1 | I |
| `levels_executed.removePlayerItem` | — | — | 3 | 3 | I |
| `levels_executed.setSaveInhibit` | 16 | 16 | 18 | 18 | I |
| `levels_executed.setv` | 36 | 37 | 48 | 48 | SS, I |
| `levels_executed.showMonster` | — | 1 | 1 | 1 | SS |
| `levels_executed.sin1000` | 1 | 3 | 4 | 4 | SS, I |
| `levels_executed.startWithSwoopDone` | — | — | 1 | 1 | I |
| `levels_refused.addHelpMessage` | 6 | 6 | 7 | 7 | I |
| `levels_refused.countQuestItems` | 11 | 1 | — | — | SS, I |
| `levels_refused.cutScene` | 13 | 13 | 17 | 17 | I |
| `levels_refused.followPath` | 3 | 4 | 4 | 4 | SS |
| `levels_refused.generateMonster` | 3 | 4 | 4 | 4 | SS |
| `levels_refused.getRand` | 5 | — | — | — | SS |
| `levels_refused.hideMonster` | 26 | 28 | 39 | 39 | SS, I |
| `levels_refused.onePlayerHasItem` | 2 | 13 | — | — | SS, I |
| `levels_refused.playerHasItem` | 15 | 15 | — | — | I |
| `levels_refused.removeAllItems` | 1 | 1 | — | — | I |
| `levels_refused.removeAllWeapons` | 1 | 1 | — | — | I |
| `levels_refused.removePlayerItem` | 3 | 3 | — | — | I |
| `levels_refused.setAlternateDeath` | 7 | 7 | 8 | 8 | I |
| `levels_refused.setParticle` | 12 | 12 | 13 | 13 | I |
| `levels_refused.setWeaponType` | 3 | 3 | 4 | 4 | I |
| `levels_refused.showBust` | 5 | 5 | 7 | 7 | I |
| `levels_refused.showMonster` | 7 | 6 | 14 | 14 | SS, I |
| `levels_refused.startDialog` | — | — | 1 | 1 | I |
| `refused.addHelpMessage` | 6 | 6 | 8 | 8 | I |
| `refused.countQuestItems` | 11 | 1 | — | — | SS, I |
| `refused.cutScene` | 14 | 14 | 18 | 18 | I |
| `refused.followPath` | 13 | 14 | 14 | 14 | SS |
| `refused.generateMonster` | 5 | 6 | 6 | 6 | SS |
| `refused.getRand` | 5 | — | — | — | SS |
| `refused.givePlayerItem` | 1 | 1 | 7 | 7 | I |
| `refused.hideMonster` | 67 | 68 | 114 | 114 | SS, I |
| `refused.onePlayerHasItem` | 2 | 13 | — | — | SS, I |
| `refused.playerHasItem` | 15 | 15 | — | — | I |
| `refused.removeAllItems` | 1 | 1 | — | — | I |
| `refused.removeAllWeapons` | 1 | 1 | — | — | I |
| `refused.removePlayerItem` | 37 | 37 | — | — | I |
| `refused.setAlternateDeath` | 7 | 7 | 8 | 8 | I |
| `refused.setParticle` | 13 | 13 | 14 | 14 | I |
| `refused.setWeaponType` | 3 | 3 | 4 | 4 | I |
| `refused.showBust` | 265 | 265 | 307 | 307 | I |
| `refused.showMonster` | 7 | 6 | 14 | 14 | SS, I |
| `refused.startDialog` | — | — | 1 | 1 | I |
| `scripts.build_refused.Creature_init initScript` | — | 5 | 5 | 5 | SS |
| `scripts.load_draws_unmodelled` | — | 15,577 | 15,577 | 15,577 | SS |
| `scripts.objects_built` | — | 324 | 324 | 324 | SS |
| `scripts.objects_not_created` | — | 1,497 | 1,497 | 1,497 | SS |
| `scripts.objects_pool_refused` | — | 0 | 0 | 0 | SS |
| `scripts.objects_registered` | 17,398 | 15,901 | 15,901 | 15,901 | SS |
| `scripts.objects_unbuilt` | — | 15,577 | 15,577 | 15,577 | SS |
| `scripts.objects_unclassified` | — | 0 | 0 | 0 | SS |
| `scripts.reached_complete` | 28 | 30 | 35 | 35 | SS, I |
| `scripts.reached_wait` | 14 | 15 | 29 | 29 | SS, I |
| `scripts.refusal_stops` | 39 | 35 | 16 | 16 | SS, I |
| `scripts.seed_bound` | — | 111 | 111 | 111 | SS |
| `scripts.seed_paired` | — | 111 | 111 | 111 | SS |
| `scripts.stub_refusals` | 205 | 205 | 265 | 265 | I |

In the scripts-off comparison, runtime-only values also move: `audio.frames_pushed` 1,841,105 → 2,628,049, `time.load_ms` 36,496 → 65,535, `time.play_ms` 44,136 → 81,278, and summed `memory.static_peak_bytes` 2,185,486,420 → 2,187,577,460. The first three depend on wall-clock load; the art and font account for the material memory increase. No other scripts-off key changes.

Runtime observations from the four scripted sweeps, in the same stage order: `audio.frames_pushed` 1,754,897 / 2,717,969 / 2,577,297 / 2,464,369; `time.load_ms` 34,833 / 67,891 / 65,432 / 61,328; `time.play_ms` 41,249 / 84,394 / 79,639 / 74,620; summed `memory.static_peak_bytes` 2,185,574,260 / 2,185,595,396 / 2,185,599,652 / 2,187,675,620. Time and pushed frames vary with machine load. HUD art accounts for the material final-stage memory increase; the earlier small peak differences vary by run.

## Commands and local evidence

The final checks use Release builds and the same 60-tick four-slice recipe as wave 19:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -j 3
cmake -S native -B build/native-x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build/native-x64 -j 4
ctest --test-dir build/native-x64 --output-on-failure -j 3
work/run-slices-r20.sh r20h2 --scripts
work/run-slices-r20.sh r20h2n
python3 work/playable-wave20/run-builds.py
python3 work/playable-wave20/run-godot.py
# Godot 4.5.2: --headless --path godot --export-release 'macOS universal' build/exports/wave20/OpenChampions.zip
python3 work/playable-wave20/run-export.py
```

The script, item and quest measure tools were also run against the explicit read-only disc path; their counts-only logs, CTest logs, matrix logs and export metadata are under ignored `work/private/` and `work/playable-wave20/`. The exported app real-data smoke uses ranges 0–11 and 69–71, 60 ticks and one creature preview per level.

## Final native and Godot verification

- Release native builds pass for arm64 (`build/native`) and Mach-O x86_64 (`build/native-x64`, run under Rosetta). No compiler warnings; the linker emits its existing duplicate-library notes. The four universal GDExtension builds pass.
- Godot dev matrix `work/playable-wave20/run-godot.py`: **51/51** on the first run, wave 19’s 50 plus `play_hud_smoke`. The Full combat headless run passes 2,621 checks, zero failures. Its `FULL_TRACE` is `374e40c991ae15cba5f91ed561edd974726a8d384d5b8e9b38337b89e15b2035`, 21,990 bytes.
- Native CTest arm64 `-j 3`: **215/216** in 1,038.75 s on the full first run. `player-exact` alone hit its 240 s timeout under concurrent load; the same binary passed when rerun alone (`-j 1`) in **148.31 s**. Thus all 216 tests passed, with the first-run timeout reported separately. x86_64 CTest under Rosetta (`build/native-x64`, Mach-O x86_64, `-j 3`): **216/216** on the first run in 1,398.01 s, with zero timeouts or failures.
- Counts-only tools on the owner-selected disc: `champions-script-commands-measure` loaded 111 scripts, started 76, stopped 15 on a used refused answer, registered 15,901 objects (324 built, 15,577 unbuilt, 1,497 records not created), and counted 15,577 unmodelled load draws. `champions-script-items-measure` found 120 scripts and 5,660 native calls. `champions-quests-measure` loaded 111 and started 76. The item measure's first invocation used an unsupported `--game` option; its documented `DIR` form passed.
- A rendered authored-texture HUD was inspected at 1280×720: frame, health and mana bars, and pending overlays visibly draw inside the centered 4:3 canvas. The transient review script was removed. I also inspected exported synthetic UI and paused-combat captures; the app drew their expected states without a new visual error. No private-disc screenshot was saved.

## Exported app and real-data smoke

- `--export-release 'macOS universal'` produced `build/exports/wave20/OpenChampions.zip` and `Open Champions.app`; ZIP SHA-256 is `e9f96e476e886884a18146a41439d5edee7480760536f06133a14b7fe7b91cb5`. The executable and both packaged GDExtensions have arm64 and x86_64 slices. The package audit found no `.gob`, `.lmp` or `.iso` member and no private-validation path string. The extension dylibs in the app are byte-identical to the built release dylibs.
- Export matrix `work/playable-wave20/run-export.py`: **19/19** on the first run, including an exported HUD smoke. Its graphical 36,000-tick Full flow at 1280×720 passes **108,827 checks, zero failures**, with 63 captures (915.49 s under concurrent machine load). Exported headless Full passes 2,621 checks; all four export `FULL_TRACE` records equal `374e40c9…2035` and are 21,990 bytes.
- Exported app on the read-only disc at 60 ticks: **12/12** without scripts, **12/12** with scripts, **3/3** arena levels with scripts; zero failed checks. The health bar and status frame draw on every tested level. Each 12-level run has 26 passed checks, and the arena run has 8. The prior BLEND-7 FX material refusal remains on mixed-pass creature previews but causes no failed level check.

## Remaining limits

- The offline seed policy is accepted for play, with 15,577 object-load draws counted as unmodelled; the original offline clock seed is not claimed. Netplay uses no script host, and the host default is strict `Exact`.
- The session supplies no item tables, so real-data gives still refuse. HUD elements without observed mana, XP, level, gold or other producer state remain refused; the authored synthetic smoke verifies mana and pending overlays.
- BLEND-7 and the wave-18 refusals remain. No private-disc screenshot was saved or original-client raster parity claimed. There was no manual play, Windows or Linux export, native Intel run, or original-client netplay interoperability test.
