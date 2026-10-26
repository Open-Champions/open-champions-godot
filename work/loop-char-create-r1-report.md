STATUS: PARTIAL — the audited character creation carousel, stat table, bounded default appearance route, record fields, and disc texture preview are implemented and verified locally. Retail appearance choices, name-grid editing, complete confirm handoff, and 3D preview remain Unknown and are labelled or refused.

2026-10-24 implementation on `codex/play-char-create` at the requested `7f6dbc2` base. Local commit only; no push. The user's disc was read only at run time. No game bytes or captures are tracked, and no files under `~/champions-re` or other worktrees were changed.

## Evidence and changes

- `t3/research-char-create` at `b6dda4e` (audits PASS): the 13 carousel positions are `0,11,10,0..9`; the seven class starting-stat rows, model/class mapping, name-record size, and record layout are pinned there. The rendered retail menu, appearance option bounds, name editing, and complete confirm flow are unverified.
- `t3/research-player-model` at `7f13899` supplies the model body codes. `t3/research-appearance` at `e088796` supports body assets and appearance record fields but does not establish the reachable menu values.
- Native flow now uses the exact carousel order, preserves duplicate model-0 position on cancel, validates the full observed start-stat table, and retains the class-6/model-11 default route. The created record exposes known base/spent values and labels appearance and skill points Unknown. Appearance rows with unknown values refuse selection. The host name entry stays within the record's byte limit and explicitly labels retail editing rules Unknown.
- Godot shows a labelled RGB body texture decoded from the selected disc at run time. It does not claim to reconstruct the retail 3D pose, camera, or appearance variants. No disc bytes are checked in.
- Native and GDScript checks cover carousel order, stat rows, class-6/model-11 selection, the created record, and unknown-value refusal.

## Verification

- arm64 Release native build passed. Relevant serial CTest: `ctest --test-dir build/native -R '^(frontend-exact|menus-exact)$' --output-on-failure -j 1` → **2/2 passed**.
- Godot extension and creature animation extension built locally. Godot dev matrix: `python3 -u work/char-create-r1/run-godot.py` → **54/54 exit 0**. `frontend_smoke` → **128 checks, zero failures, 20 screens**. The existing `scene_play_flow` fixture still logs two `loaded` property errors, and `ui_layout_smoke` logs its expected invalid-arity call while reporting PASS.
- Disc-backed graphical capture: `frontend_capture.gd` → **25/25 images**, with **111 levels detected**, font loaded, and start-stat table accepted. The class-6/model-11 and appearance images were visually inspected; the preview and Unknown labels are readable. Images and logs are ignored under `work/char-create-r1/`.
- Scripted read-only disc sweep: `bash work/char-create-r1/run-slices.sh char-create-r1 --scripts` → four successful slices; **111/111 tried, loaded, and played; 230 checks passed; zero failures; zero script faults or missing entries**. Existing bounded script-command refusals remain in the sweep counts.
- `FULL_TRACE` from the matrix's combined full-combat run: **31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`**, matching the prior branch trace; **2,620 play checks, zero failures**.

## Remaining boundary

The research does not establish retail appearance option bounds and defaults, the complete name-grid key handling, all confirm/cancel transitions and object recalculation, starting gear, or rendered 3D camera/pose. Those parts remain Unknown; the inspected captures establish this host screen's rendering only, not retail pixel parity.
