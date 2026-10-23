STATUS: PARTIAL — PV3-1 archive-identity audit finding is fixed and verified; Full-fixture locomotion cells are exposed. The ordinary game-folder player still has no verified Player_move state or equipment/appearance draw path, so live idle/walk/run and hair/beard/gear parity are not complete.

# Play visuals round 4 — 2026-10-23

Branch `codex/play-visuals`, base `f0213ec`. Evidence: `t3/research-player-anim` `f6dfb13` (`docs/spec/player-animation-selection.md`), `t3/research-appearance` `e088796` (`docs/spec/player-appearance-layers.md`), and the round-3 audit `play-visuals-r3.md` (PV3-1 MEDIUM). No disc bytes or extracted assets were added to tracked files.

## Implementation

- Full synthetic combat observation now publishes the executed player's accelerated speed, blend and walk/run weight raw words from `Player_move`, plus the player object's action state, record model index and weapon animation class. These fields exist only for the Full fixture's present player. Other play policies do not advertise original player locomotion.
- The ordinary game-folder play session still uses `DryCreature` movement (`start_play_session`), not the composed `Player_move` World. Its selected player record has no verified weapon-animation class or action/transition state. The Full fixture's synthetic player is a different owner from that selected record. Mapping its cells to the game-folder character would assert an unverified identity, so automatic original idle/walk/run playback remains refused. Attack, hit and death also remain refused.
- The appearance research specifies the four record bytes, selection resources, item-cache lookup, part attachment, body show mask and hair/beard cover gates. The current new-character path explicitly omits the four appearance bytes; only PS2 import retains them. Neither path carries the 17 equipped item definitions and live cache resolution into the render session, and direct skinning rejects parent-attached parts. Drawing those layers now would invent item choices or transforms. The unresolved native-frame body-mask consumption and asynchronous cache resolution remain **Unknown**.

## Audit fixes

- PV3-1: `read_player_resource` selects and copies a `.vif`, `.anm` or `.tex` entry from one validated, fingerprinted archive snapshot. The caller supplies the selected row, fingerprint and expected entry name; a changed archive or name returns a refusal. `real_player.gd` no longer rereads saved body or clip offsets, and the player skin binder uses the same validated path for body and texture bytes.
- Synthetic archive tests check accepted body/clip/texture copies, a wrong clip name, and changed archives at the same path for each resource. No private bytes enter those fixtures.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4`: passed, arm64 Release.
- `ctest --test-dir build/native --output-on-failure -j 3 -R '^(creature-anim-api|creature-anim-exact|skinning-positions|model-texture-bind|combat-play|play-session-boundary)$'`: 6/6 passed.
- `cmake -S gdextension -B build/godot-extension -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug && cmake --build build/godot-extension -j 4`: passed; rebuilt after the final texture read change.
- Final `archive_smoke.gd`: 291 checks, zero failures. Final `player_model_real.gd` on the read-only owner disc: 24/24 bodies drawn and textured.
- Final rendered `loop_playthrough.gd --visual-only`: passed; explicit class-0 diagnostic idle and walk samples differed at 33,084 corners and 2,008 sampled viewport pixels. The five images are under ignored `work/play-visuals/r4-visual-rendered-final/`; both poses were visually inspected. This is still not live automatic playback.
- `python3 work/play-visuals/r4-run-godot-matrix.py`: final Godot 4.5.2 dev matrix **54/54 exit 0**. The Full fixture smoke includes new checks for the player locomotion fields and the nonzero executed walk cells; **2,622 checks, zero failures**.
- `zsh work/play-visuals/r4-run-sweep-final.sh`: read-only disc, scripts on, four background slices; **111 tried / 111 loaded / 111 played**, 230 checks passed, 111 scripts attached, zero script faults, failures or refusals.
- Final dev Full smoke `FULL_TRACE`: **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, unchanged from round 3. This is a synthetic fixture trace, not retail play parity.

No push or PR. Original player animation integration requires a game-folder Session backed by the executed Player_move/animation state, bound to the selected record. Exact appearance needs record choices, live equipped item/definition/cache data, parent part skinning and draw order, followed by a rendered native-frame comparison for the two Unknown timing points.
