STATUS: COMPLETE — the four documented mounted Rider records build; arm64 CTest, Godot dev matrix, both 111-level sweeps and FULL_TRACE pass.

# Native mounted Horsemen, round 3 — 2026-10-22

## Scope and evidence

Merged wave 24 head `2e57376` first (`1afa8fe`). Used `t3/research-horsemen-mount` at `338d459c47d5e8e49dc4d1f393111910b2026f05`, especially `docs/spec/horsemen-mount.md` sections 1–3 and 9.3, for the four Observed 12-byte property lists, MiniBoss initialization, named mount registration and reciprocal handles. The earlier `397c35c` research model remains the pin for the 1,101 synthetic behavior vectors. No retail bytes or extracted assets were added.

The host now builds `RiderDisease` with MiniBoss `horsedisease`, `RiderWar` with `horsewar`, `RiderPestilence` with `horsepestilence`, and `RiderDeath` with `horsedeath`. Each pair consumes two pool blocks and two shared draws, registers both objects, sets the mount's MiniBoss flag and name, links both handles, and completes the rider in state `0x1b` and mount in state 0. A placed `Nightmare` also builds. The factory's undocumented on-foot rows `0x532dc8`–`0x532df8` remain unbuilt, as do unrelated unsupported constructors. `Creature_init initScript` remains a labelled refusal where encountered.

Audit fixes: MH-1 commits the synthetic vectors as gzip and makes CTest reject missing, empty or wrong-count input against the JSON's 1,101 cases. MH-2 labels an exhausted mount pool as an on-foot rider stub without aborting level load, accounting for its one shared draw. MH-3 changes the player scan guard to `>=` and calls its limit a host choice. MH-4 accounts for the one draw made by a placed Nightmare before a later refusal. Synthetic host tests cover all four pairs, the reciprocal handles, MiniBoss and ShowName flags, pool exhaustion and placed-Nightmare refusal.

## Verification

- arm64 Release configure and full native build: passed. Focused CTest `horsemen-mount-contract` and `script-commands`: 2/2 passed; the former replays 1,101 research-model vectors plus two native refusal cases.
- Full arm64 CTest, `ctest --test-dir build/native --output-on-failure -j 3`: 232/232 passed in 1,019.57 seconds after the final source rebuild. An earlier run was stopped after five passing cases to incorporate the final `name` property lookup; the complete rerun is the result reported here.
- Universal macOS debug archive and level-texture GDExtensions: built. The final archive rebuild after the last property lookup edit passed.
- Godot development matrix, 54 commands: 54/54 passed after the final extension rebuild. The Full combat entry passed 2,620 checks, zero failures. `FULL_TRACE` is 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, byte-identical to wave 24.
- Read-only RTA US v2.00 disc, scripts on and off, four 60-tick slices each after the final edit: both loaded and played 111/111 levels, passed 230 checks, had zero failures and empty refusal lists. Scripts-on: 328 built, 15,573 unbuilt, 15,905 registered, 15,573 unmodelled load draws, zero pool refusals, five existing `Creature_init initScript` refusals. Scripts-off: no script-created objects or build refusals. Compared with round 2, four more records build and the four Unknown mount-list refusals disappear.

Commands: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`; `ctest --test-dir build/native -R '^(horsemen-mount-contract|script-commands)$' --output-on-failure`; `ctest --test-dir build/native --output-on-failure -j 3`; `cmake --build build/mac-template_debug --config Release --parallel 4`; `cmake --build build/level-texture-template_debug --config Release --parallel 4`; `python3 work/native-horsemen/run-godot.py`; `bash work/run-slices-nh.sh nhr3-final-on --scripts`; `bash work/run-slices-nh.sh nhr3-final-off`.

The game's rider/mount run-list order, callers for mount states `0x19`/`0x1a`, mounted stun/root/cooldown reachability and charge-picker table capacity remain undocumented. The host schedules rider then mount into successive every-60 buckets; it does not tick the pair in the play scene. Rider draw matrix composition is also unported. The bounded static/synthetic and level-load results do not prove mounted gameplay or presentation. No push or PR.
