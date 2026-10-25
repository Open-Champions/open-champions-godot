STATUS: PARTIAL — native named resource resolution works on the owner's disc, but step 4 still has no constructed combat actor, handled J attack, enemy death or Gold credit.

# Play enemies implementation round 5 — 2026-10-23

Branch `codex/play-enemies`, starting at `8c4ef6c`; Wave 26 head `514b6b3` merged as `57541f1`. This round uses the audited `t3/research-campaign-combat` pin `328b072` and its handoff rounds 2–8. The normal route's initial player class, equipment and shared seed remain distinct from the assisted class-6/model-11 emulator capture.

## Implementation and observed boundary

- Added a portable, session-owned LMP resource manager with owned bytes, name lookup, generation-checked handles and atomic replacement. The selected game folder supplies archives at run time; the names of the two required archives and their 46 entry names are read from the selected executable's definition tables. An unreadable or malformed archive refuses that resource only, leaving script attachment available. No captured return address or asset data is embedded in the client.
- The native factory now resolves each named entry before its class execution gate. The read-only Valor playthrough reached the class-tick/shared-runtime refusal for 78 boar-family and six Minotaur records, with zero named-resource refusals. This establishes the requested resource presence and handle path, not model upload, animation execution or a live actor.
- The ordinary play session still creates a separate `DryCreature` owner. The factory has no shared actor execution for these classes, and the only J attack API belongs to the synthetic Full fixture. The render session has no active equipped item record. The capture's club values belong to a class-6/model-11 assisted player; this client run used class 0/model 0. Assigning those values to that record would be unsupported. The reference models cover bounded constructor fields and selected run/service calls, not the complete target, animation, movement, attack and death decisions needed to make these actors live. The source world record for the observed AutoContainer Gold is also unverified. These specific pieces remain refused.

## Audit fixes (r4)

- PE4-1: step 4 keys enemies by handle. A kill requires that same actor's positive starting HP cross to zero or below, its dead state, and a `death_drop_observed` witness; a missing actor or slot change cannot count as a kill. Unmatched actors are reported separately from matched HP.
- PE4-2: the factory comment now states that round 8 observed direct `runObjects` to `WarBoar_run` dispatch; it does not claim a captured Minotaur dispatch.
- PE4-3: the prior report's verification command now takes `$RTA_DISC` and `$GODOT_BIN`, with no committed absolute private validation path.

## Playthrough and visual inspection

The final-source headless and rendered run each entered `valor` and wrote 24 steps; the rendered run wrote 11 screenshots. Step 4 reported 16 built / 175 unbuilt objects, including 78 unbuilt boar-family and six unbuilt Minotaur records; zero live enemies, J unhandled, Gold 500→500, no AutoContainer source witness, and no death/drop. Both runs exited 1 because the journey is incomplete. I inspected the rendered `05-fight-attempt.png`: it shows the player and terrain, with no visible boar. The read-only disc stayed at 994 files before and after.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`: PASS, Release arm64. `ctest --test-dir build/native --output-on-failure -j 3 -R '^(resource-manager|level-objects|script-commands)$'`: 3/3 PASS.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR=/Users/syndicatemike/Projects/open-champions/worktrees/integration/work/deps/godot-cpp -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug`; `cmake --build build/godot-extension --parallel 4`: PASS. `python3 /Users/syndicatemike/Projects/open-champions/worktrees/playable-integration/work/playable-wave25/run-godot.py`: final-source Godot 4.5.2 dev matrix 54/54 exit 0. Its synthetic `FULL_TRACE` remained 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from round 4; it does not certify campaign combat.
- `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder "$RTA_DISC" --out work/loop-play-enemies-r5/playthrough-final-source`: scoped final-source step-4 outcome above. The variables selected the local Godot binary and owner's read-only disc.
- `bash work/play-enemies/run-sweep.sh`: 111/111 levels tried, loaded and played; 230 checks, zero failures or summary refusal maps. Seven script entries stopped on refused answers; this sweep is level loading and bounded session coverage, not a fight.
- `ctest --test-dir build/native --output-on-failure -j 3 -E '^player-exact$'`: 235/235 PASS in 883.73 seconds. `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'`: 1/1 PASS in 161.06 seconds. Together these cover all 236 registered arm64 cases; the exact player case ran serially.

No push or PR.
